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
// Sprint 57, the follow-up measurement (why the upkeep goes unmet): the
// network upkeep bill at the start, and per tick for coal/timber/stone what
// the trade shelves hold going IN (the upkeep pass draws before the tick's
// output lands at the clear, so tick 1 sees the settle's empty shelves),
// what the world dug (deposit reserves, before - after), what network upkeep
// drew, the building upkeep wanted on those shelves, and the actors' dial
// throughput beside the timber and stone sites' mean workforce target.
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
#include "world/logistics.hpp"      // count_road_edges_by_nation
#include "world/network_upkeep.hpp" // network_purchase, network_upkeep_params
#include "world/economy_system.hpp" // agency_event
#include <array>
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

    // Sprint 57 (trade dies on the folded world): the NETWORK UPKEEP BILL at the
    // start — every nation's road edges and hubs at the authored rates, the
    // stone and timber the logistics_maintenance line would buy each tick were
    // money and shelves unlimited (network_upkeep.cpp). Set beside the stone and
    // timber the world digs (below), it is the sink that empties the shelves.
    {
        long edges[3] = {0, 0, 0};
        int  hubs = 0;
        for (const auto& [nid, tiers] : count_road_edges_by_nation(w))
            for (std::size_t i = 0; i < 3; ++i) edges[i] += tiers[i];
        for (const auto& [bid, bc] : w.buildings)
            if ((bc.type == building_type::port || bc.type == building_type::inland_logistics_hub)
                && bc.ticks_remaining <= 0 && !bc.decommissioned && w.tile_to_nation.count(bc.tile))
                ++hubs;
        const network_upkeep_params& np = reg.network_upkeep();
        double stone = hubs * static_cast<double>(np.stone_per_hub);
        double timber = hubs * static_cast<double>(np.timber_per_hub);
        for (std::size_t i = 0; i < 3; ++i)
        {
            stone  += edges[i] * static_cast<double>(np.stone_per_level[i]);
            timber += edges[i] * static_cast<double>(np.timber_per_level[i]);
        }
        std::printf("network upkeep bill: edges %ld/%ld/%ld (tier 1/2/3), %d hubs -> stone %.1f, timber %.1f a tick\n",
                    edges[0], edges[1], edges[2], hubs, stone, timber);
    }

    // The trade buildings' shelves, fixed for the run.
    std::set<entity_id> trade_mkts;
    for (const auto& [bid, b] : w.buildings)
        if (is_trade_building(b.type))
            if (const entity_id mid = market_for_tile(w, b.tile); mid != null_entity) trade_mkts.insert(mid);
    const resource_type watched[] = {resource_type::coal, resource_type::timber, resource_type::stone};

    for (int t = 1; t <= live; ++t)
    {
        const int day = t * 90;
        w.current_day_tick = day;
        // What the shelves hold going INTO the tick — what the upkeep pass sees
        // (it draws before this tick's output lands at the clear). And every
        // deposit's remaining reserve, so the tick's DIG per good is exact.
        std::array<double, 3> shelf_in{};
        for (const entity_id mid : trade_mkts)
            for (std::size_t g = 0; g < 3; ++g)
                shelf_in[g] += w.markets.at(mid).inventory[static_cast<std::size_t>(watched[g])];
        std::array<double, 3> reserve_in{};
        for (const auto& [tid, tc] : w.tiles)
            for (std::size_t g = 0; g < 3; ++g)
                reserve_in[g] += tc.resource_remaining[static_cast<std::size_t>(watched[g])];
        const settle_tick_result r = run_settle_tick(w, reg, k_app_validation_ticks + (t - 1), day, /*spectating=*/true);
        const trade_tick& tt = r.trades;
        std::array<double, 3> reserve_out{};
        for (const auto& [tid, tc] : w.tiles)
            for (std::size_t g = 0; g < 3; ++g)
                reserve_out[g] += tc.resource_remaining[static_cast<std::size_t>(watched[g])];
        // Network upkeep DRAWN this tick (settle_network_purchases), all shelves
        // and the trade buildings' shelves; building upkeep WANTED on the trade
        // shelves (every building there, the trade buildings included).
        std::array<double, 3> net_all{}, net_trade{}, upk_trade{};
        for (const network_purchase& np : r.report.network_purchases)
            for (std::size_t g = 0; g < 3; ++g)
                if (np.resource == watched[g] && np.completed)
                {
                    net_all[g] += np.drawn;
                    if (trade_mkts.count(np.market)) net_trade[g] += np.drawn;
                }
        for (const auto& [key, row] : r.report.upkeep_wants)
            if (trade_mkts.count(key.second))
                for (std::size_t g = 0; g < 3; ++g)
                    upk_trade[g] += row[static_cast<std::size_t>(watched[g])];
        for (std::size_t g = 0; g < 3; ++g)
            std::printf("    %-7s shelves in %.1f | dug (world) %.1f | network upkeep drawn %.1f (%.1f on these shelves) | upkeep wanted here %.1f\n",
                        resource_names::name_of(watched[g]).c_str(), shelf_in[g], reserve_in[g] - reserve_out[g],
                        net_all[g], net_trade[g], upk_trade[g]);
        // The actors' dial throughput (AI_OPPONENT.md § 11): who was evaluated,
        // how many workforce dials moved, and where the timber and stone sites'
        // dials and supply factors stand.
        {
            int dials = 0, builds = 0;
            for (const agency_event& e : r.report.agency_events)
            {
                if (e.what == agency_event::kind::workforce_set) ++dials;
                if (e.what == agency_event::kind::built) ++builds;
            }
            double tgt = 0.0, sup = 0.0; int sites = 0;
            for (const auto& [bid, b] : w.buildings)
                if (b.type == building_type::extraction_site && b.ticks_remaining <= 0
                    && (b.target_resource == resource_type::timber || b.target_resource == resource_type::stone))
                { ++sites; tgt += b.workforce_target; sup += b.supply_factor_permille; }
            std::printf("    actors evaluated %zu, workforce dials moved %d, builds %d | timber+stone sites %d: mean workforce_target %.1f, mean supply factor %.0f\n",
                        r.report.corps_evaluated.size(), dials, builds, sites,
                        sites ? tgt / sites : 0.0, sites ? sup / sites : 0.0);
        }
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
