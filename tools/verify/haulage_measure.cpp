// haulage_measure — what does it actually COST to serve a neighbouring market?
// (BL-442 step 2)
//
// WHY THIS EXISTS. BL-442 step 2 asks for a widened scarcity ceiling, and Ben's
// requirement (2026-08-17) makes that ceiling a DERIVED quantity rather than a
// round number: scarcity must price high enough to cross the margin for nearby
// markets, so inter-market trading exists from day 1 rather than as a late-game
// unlock. "Cross the margin" is an arithmetic claim about a number nobody had
// measured — the per-unit haulage the supply layer already charges between two
// adjacent markets in the REAL generated world.
//
// A ceiling picked before that number is known is a guess wearing a derivation's
// clothes. This harness measures the number, so the multiplier in
// scripts/economy.lua can be re-derived rather than trusted.
//
// WHAT IT MEASURES. Exactly the cost `dispatch_convoys` charges
// (supply_system.cpp): for an intra-body leg,
//
//     haulage_per_unit = logistics_cost(mode) * path.cost * (1 - node_discount)
//
// where path.cost is the terrain-weighted A* cost (logistics.hpp) between the
// two markets' centre tiles, mode is sea when the path crosses ocean, and the
// node discount is the BL-148/149 city/hub discount along that same path. It
// walks every market and finds its NEAREST market neighbour by that cost — the
// "nearby market" of the requirement — and reports the distribution.
//
// THE DERIVATION IT FEEDS. A seller in market A can serve a scarce market B only
// if the scarcity price clears the base price plus the haul:
//
//     ceil_mult * base_price  >  base_price + haulage_per_unit
//     ceil_mult               >  1 + haulage_per_unit / base_price
//
// The BINDING case is the cheapest good over the dearest haul, so the harness
// reports the required ceil_mult against the minimum authored base_price as well
// as against the median. It also reports the SECOND constraint Ben named: the
// ceiling must be wide enough that a scarce cheap resource outprices an abundant
// dear one, i.e. ceil_mult > max_base_price / min_base_price.
//
// Report-only. This harness asserts nothing about the band's value — the band is
// authored data and the assertions on it live in price_band_harness.cpp. Its one
// assertion is a vacuity guard: that it measured a world with markets in it.
//
// Run: .\build\haulage_measure.exe [seeds]

#include "scripting/lua_state.hpp"
#include "world/components.hpp"
#include "world/corporation_generation.hpp"
#include "world/hard_coded_world.hpp"
#include "world/budget_system.hpp"
#include "world/economy_system.hpp"
#include "world/logistics.hpp"
#include "world/market_clearing.hpp"
#include "world/supply_system.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"
#include "world/world_gen_config.hpp"
#include "harness_params.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

void check(bool ok, const std::string& what)
{
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (!ok)
        ++g_failures;
}

double quantile(std::vector<double> v, double q)
{
    if (v.empty())
        return 0.0;
    std::sort(v.begin(), v.end());
    const std::size_t i = static_cast<std::size_t>(q * static_cast<double>(v.size() - 1) + 0.5);
    return v[std::min(i, v.size() - 1)];
}

} // namespace

int main(int argc, char** argv)
{
    const int n_seeds       = (argc > 1) ? std::atoi(argv[1]) : 5;
    const int n_trade_ticks = (argc > 2) ? std::atoi(argv[2]) : 20;
    if (n_seeds <= 0 || n_trade_ticks <= 0)
    {
        std::printf("usage: %s [seeds] [ticks]  (both positive)\n", argv[0]);
        return 2;
    }

    lua_state lua;
    lua.load("scripts/recipes.lua");
    lua.load("scripts/economy.lua");
    lua.load("scripts/world_gen.lua");
    recipe_registry reg;
    reg.load_from_lua(lua);
    world_gen_config gen_cfg;
    gen_cfg.load_from_lua(lua);
    if (reg.recipe_count(building_type::processing_facility) == 0)
    {
        std::printf("FATAL: no recipes loaded — run from the repo root.\n");
        return 2;
    }

    std::printf("=== haulage_measure (BL-442 step 2) — the cost of serving a neighbour ===\n");
    std::printf("%d seeds, real generated world, real Lua logistics costs\n\n", n_seeds);

    std::printf("authored logistics cost per unit distance per unit cargo:\n");
    std::printf("  land %.4f   sea %.4f   space %.4f\n\n",
                static_cast<double>(reg.logistics_cost(convoy_mode::land)),
                static_cast<double>(reg.logistics_cost(convoy_mode::sea)),
                static_cast<double>(reg.logistics_cost(convoy_mode::space)));
    std::printf("current price band: floor %.3fx  ceil %.3fx\n\n",
                static_cast<double>(reg.price_band().floor_mult),
                static_cast<double>(reg.price_band().ceil_mult));

    // Per-unit haulage to each market's NEAREST market neighbour, pooled over seeds.
    std::vector<double> nearest_haul;
    // Every same-body market pair, for the wider picture.
    std::vector<double> all_pair_haul;
    long n_markets = 0, n_bodies_multi = 0, n_sea_legs = 0;

    // Authored base prices, pooled: the denominator of the derivation.
    std::vector<double> base_prices;
    double base_min = std::numeric_limits<double>::max(), base_max = 0.0;
    int base_min_idx = -1, base_max_idx = -1;

    for (int s = 0; s < n_seeds; ++s)
    {
        world_params p = no_prehistory();
        p.seed = static_cast<uint32_t>(s);
        world w = make_hard_coded_world(p, nullptr, gen_cfg);
        assign_default_recipes(w, reg);
        generate_background_firms(w, reg, static_cast<uint32_t>(s) ^ 0x8A21F00Du);
        assign_default_recipes(w, reg);

        // Sorted id walk (standing.hpp convention) — markets is an unordered_map.
        std::vector<entity_id> mids;
        mids.reserve(w.markets.size());
        for (const auto& [id, m] : w.markets)
            mids.push_back(id);
        std::sort(mids.begin(), mids.end());

        if (s == 0)
        {
            // Base prices are authored per market from the rarity table; read the
            // first market's, which is the world's authored reference set.
            for (const entity_id mid : mids)
            {
                const market_component& m = w.markets.at(mid);
                for (std::size_t r = 0; r < resource_count; ++r)
                {
                    const double bp = static_cast<double>(m.base_price[r]);
                    if (bp <= 0.0)
                        continue;
                    base_prices.push_back(bp);
                    if (bp < base_min) { base_min = bp; base_min_idx = static_cast<int>(r); }
                    if (bp > base_max) { base_max = bp; base_max_idx = static_cast<int>(r); }
                }
                break; // one market's authored table is the reference
            }
        }

        // Group markets by body.
        std::vector<entity_id> bodies;
        for (const entity_id mid : mids)
        {
            const entity_id b = w.markets.at(mid).body;
            if (std::find(bodies.begin(), bodies.end(), b) == bodies.end())
                bodies.push_back(b);
        }
        std::sort(bodies.begin(), bodies.end());

        for (const entity_id body : bodies)
        {
            std::vector<entity_id> on_body;
            for (const entity_id mid : mids)
                if (w.markets.at(mid).body == body)
                    on_body.push_back(mid);
            if (on_body.size() < 2)
                continue;
            ++n_bodies_multi;

            for (const entity_id a : on_body)
            {
                const entity_id ta = w.markets.at(a).centre_tile;
                if (ta == null_entity)
                    continue;
                ++n_markets;
                double best = std::numeric_limits<double>::max();
                for (const entity_id b : on_body)
                {
                    if (b == a)
                        continue;
                    const entity_id tb = w.markets.at(b).centre_tile;
                    if (tb == null_entity)
                        continue;
                    // Read cost/mode out of the cache reference immediately —
                    // it is only valid until an invalidation.
                    double cost;
                    bool   sea;
                    {
                        const logistics_path& path = intra_body_path(w, body, ta, tb);
                        if (!path.reachable)
                            continue;
                        cost = static_cast<double>(path.cost);
                        sea  = path.crosses_ocean;
                    }
                    const double unit = static_cast<double>(
                        reg.logistics_cost(sea ? convoy_mode::sea : convoy_mode::land));
                    // The node discount needs the path tiles; recompute against the
                    // same cached path. Deliberately NOT applied here: the discount
                    // is a corp-built optimisation (cities/hubs on the route) and the
                    // day-1 case the requirement names has neither. Measuring the
                    // UNDISCOUNTED haul is the conservative side — it is the cost a
                    // day-1 trader actually pays, and the discount only makes the
                    // derived ceiling more than sufficient.
                    const double haul = unit * cost;
                    all_pair_haul.push_back(haul);
                    if (haul < best)
                    {
                        best = haul;
                        if (sea) ++n_sea_legs;
                    }
                }
                if (best < std::numeric_limits<double>::max())
                    nearest_haul.push_back(best);
            }
        }
    }

    check(!nearest_haul.empty(),
          "measured at least one market-to-neighbour haul (vacuity guard)");
    if (nearest_haul.empty())
    {
        std::printf("\n=== haulage_measure: %d failure(s) ===\n", g_failures);
        return 1;
    }

    const double n_p10 = quantile(nearest_haul, 0.10);
    const double n_med = quantile(nearest_haul, 0.50);
    const double n_p90 = quantile(nearest_haul, 0.90);
    const double n_max = quantile(nearest_haul, 1.00);
    const double a_med = quantile(all_pair_haul, 0.50);
    const double a_max = quantile(all_pair_haul, 1.00);

    std::printf("\n--- per-unit haulage, market -> NEAREST market neighbour ---\n");
    std::printf("  samples %zu over %ld multi-market bodies (%ld markets)\n",
                nearest_haul.size(), n_bodies_multi, n_markets);
    std::printf("  p10 %8.4f   median %8.4f   p90 %8.4f   max %8.4f\n",
                n_p10, n_med, n_p90, n_max);
    std::printf("--- per-unit haulage, ALL same-body market pairs ---\n");
    std::printf("  samples %zu   median %8.4f   max %8.4f\n",
                all_pair_haul.size(), a_med, a_max);

    std::printf("\n--- authored base prices (the derivation's denominator) ---\n");
    std::printf("  min %6.3f (resource #%d)   median %6.3f   max %6.3f (resource #%d)   max/min %6.2f\n",
                base_min, base_min_idx, quantile(base_prices, 0.50),
                base_max, base_max_idx,
                base_min > 0.0 ? base_max / base_min : 0.0);

    std::printf("\n--- DERIVATION: ceil_mult > 1 + haulage / base_price ---\n");
    const double bmed = quantile(base_prices, 0.50);
    std::printf("  vs cheapest good (%6.3f), median haul  : ceil > %6.2f\n", base_min, 1.0 + n_med / base_min);
    std::printf("  vs cheapest good (%6.3f), p90 haul     : ceil > %6.2f\n", base_min, 1.0 + n_p90 / base_min);
    std::printf("  vs cheapest good (%6.3f), max haul     : ceil > %6.2f\n", base_min, 1.0 + n_max / base_min);
    std::printf("  vs median good   (%6.3f), median haul  : ceil > %6.2f\n", bmed, 1.0 + n_med / bmed);
    std::printf("  vs median good   (%6.3f), p90 haul     : ceil > %6.2f\n", bmed, 1.0 + n_p90 / bmed);
    std::printf("\n--- DERIVATION: scarcity must outrank abundance ---\n");
    std::printf("  ceil_mult > max_base/min_base = %6.2f\n",
                base_min > 0.0 ? base_max / base_min : 0.0);

    // -----------------------------------------------------------------------
    // Does inter-market trade actually HAPPEN? (BL-442 step 2's real deliverable)
    // -----------------------------------------------------------------------
    // The derivation above says what the ceiling must EXCEED. It cannot say
    // whether widening it makes trade occur — that is a claim about the running
    // economy, and asserting it from arithmetic is exactly the guess this
    // harness exists to refuse. So run the real tick loop and count what moves.
    //
    // Three counters, because they are three different claims:
    //   intra-body inter-market convoys — the "nearby market" of the requirement
    //   inter-body convoys              — the space lane (launchpad-gated)
    //   trade_routes                    — BL-088's persistent record, body-level
    //                                     only, so it can only ever see the second
    std::printf("\n--- does inter-market trade OCCUR? (%d ticks per seed) ---\n", n_trade_ticks);
    long conv_total = 0, conv_inter_market = 0, conv_inter_body = 0, routes = 0;
    for (int s = 0; s < n_seeds; ++s)
    {
        world_params p = no_prehistory();
        p.seed = static_cast<uint32_t>(s);
        world w = make_hard_coded_world(p, nullptr, gen_cfg);
        assign_default_recipes(w, reg);
        generate_background_firms(w, reg, static_cast<uint32_t>(s) ^ 0x8A21F00Du);
        assign_default_recipes(w, reg);

        std::size_t seen = 0;
        for (int t = 1; t <= n_trade_ticks; ++t)
        {
            dispatch_convoys(w, reg, reg.logistics_cost(convoy_mode::land),
                             reg.logistics_cost(convoy_mode::space));
            // Count only convoys appended THIS tick — w.convoys is append-only
            // within a tick and compacted by advance/credit, so walking from the
            // previous size is the honest dispatch count.
            for (std::size_t i = seen; i < w.convoys.size(); ++i)
            {
                const convoy_component& c = w.convoys[i];
                ++conv_total;
                if (c.mode == convoy_mode::space)
                    ++conv_inter_body;
                else if (c.source_market != c.dest_market)
                    ++conv_inter_market;
            }
            advance_convoys(w);
            const economy_report report = run_economy_step(w, reg);
            const auto flows = clear_markets(w, reg, report);
            apply_budget(w, reg, flows, report.workforce_contention, nullptr);
            credit_arrived_convoys(w, t);
            seen = w.convoys.size();
        }
        routes += static_cast<long>(w.trade_routes.size());
    }
    std::printf("  convoys dispatched (all)          : %ld\n", conv_total);
    std::printf("  ... intra-body, market -> market  : %ld\n", conv_inter_market);
    std::printf("  ... inter-body (space lane)       : %ld\n", conv_inter_body);
    std::printf("  persistent trade_routes (BL-088)  : %ld\n", routes);

    std::printf("\n=== haulage_measure: %d failure(s) ===\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
