// ---------------------------------------------------------------------------
// trade_pass_probe — diagnostic (reported, no assertions)
// ---------------------------------------------------------------------------
// The trade pass's own counters, tick by tick, on the SHIPPED start (the app
// mirror), through the settle and a seated live window: how many owners made
// trade points, how many points, how many auto/manual shipments went out, and
// how many the Logistic Point cap refused. Plus, at the start, every trade
// building's owner and whether it makes points. Written when sprint 57 found
// haulage_measure reading zero convoys after the fold.
//
// Build: cmd //c tools\verify\build_lua_harness.bat trade_pass_probe
// Run:   build_gen/verify/trade_pass_probe.exe [seed] [live_ticks]   (default 0, 12)

#include "harness_params.hpp"
#include "scripting/lua_state.hpp"
#include "world/campaign_settle.hpp"
#include "world/trade.hpp"
#include "world/world.hpp"
#include "world/market_clearing.hpp"
#include "world/resource_names.hpp"
#include <set>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <vector>

int main(int argc, char** argv)
{
    const std::uint32_t seed = argc > 1 ? static_cast<std::uint32_t>(std::strtoul(argv[1], nullptr, 10)) : 0u;
    const int live = argc > 2 ? std::atoi(argv[2]) : 12;
    lua_state lua;
    world_params params = arc_params(world_arc::shipped);
    params.seed = seed;
    app_start_world out;
    build_app_start_world(lua, params, out);
    world& w = out.w;
    const recipe_registry& reg = out.reg;

    // The trade buildings at the start, by owner.
    std::map<entity_id, int> mp_by_owner, port_by_owner;
    int mp = 0, ports = 0, mp_idle = 0;
    for (const auto& [bid, b] : w.buildings)
    {
        if (!is_trade_building(b.type)) continue;
        const building_owner o = building_owner_of(w, bid);
        if (b.type == building_type::planetary_marketplace) { ++mp; ++mp_by_owner[o.id]; if (b.ticks_remaining > 0 || b.state_idle || b.decommissioned) ++mp_idle; }
        else { ++ports; ++port_by_owner[o.id]; }
    }
    std::printf("start: %d Marketplaces (%d under construction / idle / decommissioned), %d ports; owners with a Marketplace %zu\n",
                mp, mp_idle, ports, mp_by_owner.size());

    for (int t = 1; t <= live; ++t)
    {
        const int day = t * 90;
        w.current_day_tick = day;
        const settle_tick_result r = run_settle_tick(w, reg, k_app_validation_ticks + (t - 1), day, /*spectating=*/true);
        const trade_tick& tt = r.trades;
        int idle = 0, unmet = 0, n = 0;
        for (const auto& [bid, b] : w.buildings)
        {
            if (!is_trade_building(b.type)) continue;
            ++n;
            if (b.state_idle) ++idle;
            if (r.report.upkeep_unmet.count(bid)) ++unmet;
        }
        std::printf("  trade buildings %d: state_idle %d, upkeep unmet %d\n", n, idle, unmet);
        // The upkeep goods on the trade buildings' own shelves, and their price
        // against base (the reservation ceiling is a multiple of base).
        std::set<entity_id> mkts;
        for (const auto& [bid, b] : w.buildings)
            if (is_trade_building(b.type))
                if (const entity_id mid = market_for_tile(w, b.tile); mid != null_entity) mkts.insert(mid);
        for (const resource_type g : {resource_type::coal, resource_type::timber, resource_type::stone})
        {
            const std::size_t gi = static_cast<std::size_t>(g);
            float inv = 0.0f, pr = 0.0f, base = 0.0f; int empty = 0;
            for (const entity_id mid : mkts)
            {
                const market_component& m = w.markets.at(mid);
                inv += m.inventory[gi]; pr += m.price[gi]; base += m.base_price[gi];
                if (m.inventory[gi] <= 0.0f) ++empty;
            }
            std::printf("    %-7s on %zu shelves: stock %.1f (%d empty), mean price/base %.2f\n",
                        resource_names::name_of(g).c_str(), mkts.size(), static_cast<double>(inv), empty,
                        base > 0.0f ? static_cast<double>(pr / base) : 0.0);
        }
        std::printf("live tick %2d: traders %d, points %.1f, spent %.1f, auto %d, manual %d, refused (no LP) %d, units %.1f, convoys in flight %zu\n",
                    t, tt.corps_trading, static_cast<double>(tt.points_made), static_cast<double>(tt.points_spent),
                    tt.auto_shipments, tt.manual_shipments, tt.refused_no_lp, static_cast<double>(tt.units_shipped),
                    w.convoys.size());
    }
    return 0;
}
