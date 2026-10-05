// Headless harness for population-static-mvp (BL-047) and workforce-pool-step2
// (BL-042). Verifies without SDL / Lua / ImGui:
//   R3: generate_population_centres seeds centres on Kepler
//   R4: market demand for agricultural_produce is non-zero after the tick pair
//       run_economy_step + clear_markets (the BL-190 population injection lives
//       in clear_markets, after its demand reset)
//   R5: agglomeration -- larger population scale drives more workforce supply
//       (verified via workforce_contention scalar on identical buildings)
// workforce-pool-step2:
//   R1: workforce pool supply derives from population centres; higher scale body
//       supplies more labour (lower workforce_contention at the same demand)
//
// Build (from repo root, after sourcing vcvars64):
//   cl /nologo /std:c++20 /EHsc /I src tools\verify\population_mvp.cpp ^
//      src\world\world.cpp src\world\tile_generation.cpp src\world\nation_generation.cpp ^
//      src\world\corporation_generation.cpp src\world\placement_rules.cpp ^
//      src\world\population_generation.cpp src\world\hard_coded_world.cpp ^
//      src\world\orbital_system.cpp src\world\economy_system.cpp ^
//      src\world\market_clearing.cpp src\world\budget_system.cpp ^
//      /Fo:build_gen\verify\population_mvp\ /Fe:build_gen\verify\population_mvp.exe
// Run:
//   .\build_gen\verify\population_mvp.exe
//
// Note: recipe_registry.cpp is NOT linked -- it depends on Lua. The registry
// is used inline-only (set_thresholds / set_economics / add_recipe are in header).

#include "world/budget_system.hpp"
#include "world/components.hpp"
#include "world/economy_system.hpp"
#include "world/hard_coded_world.hpp"
#include "harness_params.hpp"
#include "world/market_clearing.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <memory>
#include <utility>
#include <vector>

static int g_failures = 0;
static int g_passes   = 0;

static void check(bool cond, const char* what, float got = 0.0f, float want = 0.0f)
{
    if (cond) {
        std::printf("  PASS  %s\n", what);
        ++g_passes;
    } else {
        std::printf("  FAIL  %s   (got %.4f, want %.4f)\n", what, got, want);
        ++g_failures;
    }
}

static std::size_t ri(resource_type r)
{
    return static_cast<std::size_t>(r);
}

// ---------------------------------------------------------------------------
// R3 + R4: population centres on Kepler; demand non-zero after econ step
// ---------------------------------------------------------------------------
static void test_population_on_kepler()
{
    std::printf("--- R3: population centres on Kepler ---\n");

    world w = make_hard_coded_world(no_prehistory());
    const entity_id kepler = w.home_body;

    // Count centres whose tile belongs to Kepler.
    int kepler_centres = 0;
    for (const auto& kv : w.population_centres)
    {
        const entity_id cid = kv.first;
        const auto tile_it  = w.population_centre_tile.find(cid);
        if (tile_it == w.population_centre_tile.end())
            continue;
        const auto tc_it = w.tiles.find(tile_it->second);
        if (tc_it != w.tiles.end() && tc_it->second.body == kepler)
            ++kepler_centres;
    }

    std::printf("  Kepler population centres: %d\n", kepler_centres);
    check(kepler_centres >= 20, "Kepler has >= 20 population centres (target 20-40)");

    // At least one Town+ (scale >= 2) from the weighted draw.
    int has_town_plus = 0;
    for (const auto& kv : w.population_centres)
        if (kv.second.scale >= 2) ++has_town_plus;
    check(has_town_plus > 0, "at least one centre scale >= 2 (Town+)");

    // -------------------------------------------------------------------------
    // R4: market demand for agricultural_produce is non-zero after the full tick
    // pair. The population injection lives in clear_markets (after its demand
    // reset — BL-190 ordering fix), so the econ step alone no longer shows it.
    // -------------------------------------------------------------------------
    std::printf("--- R4: agricultural demand non-zero after econ step + clearing ---\n");

    // The comment here used to read "empty -- population injection runs
    // unconditionally", and that premise died with BL-368. inject_population_demand
    // now reads reg.population_demand_basket() (BL-640: the era-resolved fold
    // of the shared tranche and the band's own), which on a default-
    // constructed registry is all zeroes — so this assertion has been measuring an
    // empty basket rather than an absent population, and failing for a reason that
    // had nothing to do with what it tests.
    //
    // Authoring the basket here rather than loading scripts/economy.lua is
    // deliberate and forced: recipe_registry.cpp is not linked into this harness
    // (it pulls sol2/Lua — see the note at the top of this file), so the Lua route
    // is unavailable. The weight below mirrors economy.lua's agricultural_produce
    // row so the check stays anchored to the shipped value rather than an invented
    // one.
    recipe_registry reg;
    {
        population_demand_params pd; // defaults for elasticity/scale, as economy.lua has them
        pd.demand_basket[ri(resource_type::agricultural_produce)] = 0.20f; // economy.lua's row
        reg.set_population_demand(pd);
    }
    const economy_report report = run_economy_step(w, reg);
    clear_markets(w, reg, report);

    // DIAGNOSTIC, added 2026-08-12. This loop used to `break` at the FIRST
    // Kepler market in map order and assert on that one alone, which was only
    // ever safe while the body carried a single market. Print every market's
    // figure and assert on the TOTAL: the requirement is that population demand
    // reaches the market layer, not that it reaches whichever market happens to
    // be iterated first.
    bool  found_demand = false;
    float total_demand = 0.0f;
    int   kepler_markets = 0;
    for (const auto& kv : w.markets)
    {
        const market_component& mc = kv.second;
        if (mc.body != kepler)
            continue;
        const float demand = mc.demand[ri(resource_type::agricultural_produce)];
        std::printf("  Kepler market %d agricultural_produce demand: %.1f\n",
                    kepler_markets, demand);
        ++kepler_markets;
        total_demand += demand;
        found_demand = true;
    }
    {
        std::printf("  Kepler markets: %d, total agricultural_produce demand: %.1f\n",
                    kepler_markets, total_demand);
        check(total_demand > 0.0f,
              "Kepler market.demand[agricultural_produce] > 0 after econ step + clearing");
    }
    if (!found_demand)
    {
        std::printf("  FAIL  No market found for Kepler body\n");
        ++g_failures;
    }
}

// ---------------------------------------------------------------------------
// R5 + workforce-pool-step2 R1: larger population scale -> more labour supply.
//
// Sets up a synthetic world with two bodies; corp owns extraction sites on both.
// body_low  has one pop centre scale 1 (labour_by_scale[1] = 1.0)
// body_high has one pop centre scale 4 (labour_by_scale[4] = 30.0)
//
// Pass 1: 1 building per body. body_low supply (1.0) barely covers demand (1).
//         body_high is clearly uncontended (30.0 >> 1).
// Pass 2: +5 buildings on body_low (demand 6 > supply 1). Contention drops on
//         body_low; body_high stays near 1.0.
// ---------------------------------------------------------------------------
static void test_agglomeration_and_pool()
{
    std::printf("--- R5 + workforce-pool-step2 R1: agglomeration workforce supply ---\n");

    world w;

    // One corp owns all buildings.
    const entity_id corp = w.create_entity();
    w.corporations[corp] = corporation_component{};

    // body_low: pop scale 1, labour_by_scale[1] = 1.0.
    const entity_id body_low = w.create_entity();
    {
        body_component bc{};
        bc.name        = "Low";
        bc.grid_width  = 10;
        bc.grid_height = 4;
        w.bodies[body_low] = bc;
    }
    {
        const entity_id mkt = w.create_entity();
        market_component mc{};
        mc.body = body_low;
        w.markets[mkt] = mc;
    }
    // Tile for body_low.
    const entity_id tile_low = w.create_entity();
    {
        tile_component tc{};
        tc.body  = body_low;
        tc.grid_x = 0;
        tc.grid_y = 0;
        w.tiles[tile_low] = tc;
    }
    // Pop centre scale 1 on tile_low.
    {
        const entity_id pop = w.create_entity();
        population_centre_component pcc{};
        pcc.scale        = 1;
        pcc.population   = 10;
        pcc.habitability = 1.0f;
        w.population_centres[pop]      = pcc;
        w.population_centre_tile[pop]  = tile_low;
    }

    // body_high: pop scale 4, labour_by_scale[4] = 30.0.
    const entity_id body_high = w.create_entity();
    {
        body_component bc{};
        bc.name        = "High";
        bc.grid_width  = 10;
        bc.grid_height = 4;
        w.bodies[body_high] = bc;
    }
    {
        const entity_id mkt = w.create_entity();
        market_component mc{};
        mc.body = body_high;
        w.markets[mkt] = mc;
    }
    const entity_id tile_high = w.create_entity();
    {
        tile_component tc{};
        tc.body  = body_high;
        tc.grid_x = 0;
        tc.grid_y = 0;
        w.tiles[tile_high] = tc;
    }
    {
        const entity_id pop = w.create_entity();
        population_centre_component pcc{};
        pcc.scale        = 4;
        pcc.population   = 1000;
        pcc.habitability = 1.0f;
        w.population_centres[pop]      = pcc;
        w.population_centre_tile[pop]  = tile_high;
    }

    // Helper: add an extraction site on a given tile for the shared corp.
    // workforce_assigned = 1.0 so the economy system counts 1 unit of labour
    // demand for this building (as if it was at full staffing at construction).
    auto add_extractor = [&](entity_id tile_id)
    {
        building_component bld{};
        bld.tile              = tile_id;
        bld.type              = building_type::extraction_site;
        bld.target_resource   = resource_type::iron_ore;
        bld.workforce_target  = 100;
        bld.workforce_assigned = 1.0f; // demand counts only non-zero assigned workers
        const entity_id bid   = w.create_entity();
        w.buildings[bid] = bld;
        w.corporations[corp].assets.push_back(bid);
    };

    // One extractor on each body.
    add_extractor(tile_low);
    add_extractor(tile_high);

    recipe_registry reg;
    building_economics ex{};
    ex.base_rate  = 10.0f;
    ex.maintenance = 2.0f;
    ex.base_wage  = 5.0f;
    reg.set_economics(building_type::extraction_site, ex);
    reg.set_thresholds(1.0f, 0.2f);

    // Pass 1.
    economy_report r1 = run_economy_step(w, reg);
    const auto key_low  = std::make_pair(corp, body_low);
    const auto key_high = std::make_pair(corp, body_high);
    const auto i1l = r1.workforce_contention.find(key_low);
    const auto i1h = r1.workforce_contention.find(key_high);
    const float c1l = (i1l != r1.workforce_contention.end()) ? i1l->second : 1.0f;
    const float c1h = (i1h != r1.workforce_contention.end()) ? i1h->second : 1.0f;
    std::printf("  Pass 1: scale-1=%.4f  scale-4=%.4f\n", c1l, c1h);
    check(std::fabs(c1l - 1.0f) < 1e-3f,
          "scale-1 body starts uncontended with 1 building (supply 1.0 == demand 1)");
    check(std::fabs(c1h - 1.0f) < 1e-3f,
          "scale-4 body is uncontended with 1 building (supply 30 >> demand 1)");

    // Pass 2: +5 extractors on body_low (demand 6 > supply 1.0).
    for (int i = 0; i < 5; ++i)
        add_extractor(tile_low);

    economy_report r2 = run_economy_step(w, reg);
    const auto i2l = r2.workforce_contention.find(key_low);
    const auto i2h = r2.workforce_contention.find(key_high);
    const float c2l = (i2l != r2.workforce_contention.end()) ? i2l->second : 1.0f;
    const float c2h = (i2h != r2.workforce_contention.end()) ? i2h->second : 1.0f;
    std::printf("  Pass 2: scale-1=%.4f  scale-4=%.4f\n", c2l, c2h);

    check(c2l < 1.0f,
          "scale-1 body becomes contended (6 buildings vs supply 1.0)");
    check(c2h >= 0.99f,
          "scale-4 body stays near-uncontended (supply 30 >> demand 6)");
    check(c2l < c2h,
          "scale-1 body worse contention than scale-4 under same demand");
}

// ---------------------------------------------------------------------------
// population-dynamic R2: body habitability < 0.6 applies a reduced workforce
// multiplier. Uses a synthetic single-body world with habitability=0.3.
// With one building (demand 1) and pop supply 1.0, contention should be 1.0
// normally, but the hab scalar reduces it to 0.5 + (0.3/0.6)*0.5 = 0.75.
// ---------------------------------------------------------------------------
static void test_habitability_scalar()
{
    std::printf("--- population-dynamic R2: habitability scalar ---\n");

    world w;

    const entity_id corp = w.create_entity();
    w.corporations[corp] = corporation_component{};

    const entity_id body = w.create_entity();
    {
        body_component bc{};
        bc.name        = "LowHab";
        bc.grid_width  = 10;
        bc.grid_height = 4;
        w.bodies[body] = bc;
    }

    {
        const entity_id mkt = w.create_entity();
        market_component mc{};
        mc.body = body;
        w.markets[mkt] = mc;
    }

    const entity_id tile = w.create_entity();
    {
        tile_component tc{};
        tc.body  = body;
        tc.grid_x = 0;
        tc.grid_y = 0;
        w.tiles[tile] = tc;
    }

    // Pop centre with habitability 0.3 (below the 0.6 threshold).
    {
        const entity_id pop = w.create_entity();
        population_centre_component pcc{};
        pcc.scale        = 2;   // supply = labour_by_scale[2] = 3.0
        pcc.population   = 50;
        pcc.habitability = 0.3f; // triggers hab_scalar = 0.5 + (0.3/0.6)*0.5 = 0.75
        w.population_centres[pop]     = pcc;
        w.population_centre_tile[pop] = tile;
    }

    // One extraction site (demand = 1.0 workforce_assigned).
    {
        building_component bld{};
        bld.tile               = tile;
        bld.type               = building_type::extraction_site;
        bld.target_resource    = resource_type::iron_ore;
        bld.workforce_target   = 100;
        bld.workforce_assigned = 1.0f;
        const entity_id bid    = w.create_entity();
        w.buildings[bid] = bld;
        w.corporations[corp].assets.push_back(bid);
    }

    recipe_registry reg;
    building_economics ex{};
    ex.base_rate  = 10.0f;
    ex.base_wage  = 5.0f;
    reg.set_economics(building_type::extraction_site, ex);
    reg.set_thresholds(1.0f, 0.2f);

    const economy_report rpt = run_economy_step(w, reg);

    // body_habitability should reflect the pop centre's hab (0.3).
    const auto hit = rpt.body_habitability.find(body);
    const float hab = (hit != rpt.body_habitability.end()) ? hit->second : 1.0f;
    std::printf("  body_habitability=%.4f\n", hab);
    check(hab < 0.5f, "body with habitability=0.3 centre reports hab < 0.5");

    // Contention for this corp/body: supply = 3.0, demand = 1.0 -> raw = 1.0,
    // then hab_scalar = 0.5 + (0.3/0.6)*0.5 = 0.75 applied -> contention < 1.0.
    const auto key = std::make_pair(corp, body);
    const auto cit = rpt.workforce_contention.find(key);
    const float contention = (cit != rpt.workforce_contention.end()) ? cit->second : 1.0f;
    std::printf("  contention=%.4f (expected ~0.75)\n", contention);
    check(contention < 1.0f, "low-habitability body has contention < 1.0 (hab scalar applies)");
    check(contention > 0.5f, "low-habitability body contention > 0.5 (not fully throttled at 0.3)");
}

// ---------------------------------------------------------------------------
// population-dynamic R3: sustained ticks with habitability >= 0.5 AND met
// ratio >= 0.5 cause a scale-1 centre at the scale-2 headcount rung to promote
// (BL-616: promotion is population-driven — rung + sustained window).
// Sets food supply equal to demand each tick to satisfy the growth condition.
// ---------------------------------------------------------------------------
static void test_population_growth()
{
    std::printf("--- population-dynamic R3: population growth level-up ---\n");

    world w;

    const entity_id body = w.create_entity();
    {
        body_component bc{};
        bc.name = "GrowBody";
        bc.grid_width  = 10;
        bc.grid_height = 4;
        w.bodies[body] = bc;
    }

    const entity_id tile = w.create_entity();
    {
        tile_component tc{};
        tc.body  = body;
        tc.grid_x = 0;
        tc.grid_y = 0;
        w.tiles[tile] = tc;
    }

    // Market: food supply set equal to demand each iteration.
    const entity_id mkt = w.create_entity();
    {
        market_component mc{};
        mc.body = body;
        w.markets[mkt] = mc;
    }

    // Pop centre scale 1, good habitability. BL-616 (centre promotion and
    // decline) made promotion POPULATION-driven: a centre promotes when its
    // headcount has held above the NEXT tier's rung (k_population_for_scale)
    // for a sustained window, so this fixture seeds the headcount AT the
    // scale-2 rung (50) and the window (50 qualifying ticks) is what the run
    // proves — the old fixed 200-tick accumulator threshold is gone.
    const entity_id pop = w.create_entity();
    {
        population_centre_component pcc{};
        pcc.scale        = 1;
        pcc.population   = 50;
        pcc.habitability = 0.9f;
        w.population_centres[pop]     = pcc;
        w.population_centre_tile[pop] = tile;
    }

    recipe_registry reg;

    // Run 210 ticks. Before each tick, satisfy food supply = demand to keep the
    // met-supply gate satisfied. (With this empty registry the demand basket is
    // all-zero, so the gate is trivially met regardless — the supply dance is
    // belt-and-braces against a future non-empty basket.) The population demand
    // itself is injected by clear_markets (BL-190 ordering fix), not the econ
    // step. BL-616: promotion fires once the rung-level headcount has held
    // through the 50-tick sustained window, so 210 ticks is ample.
    for (int t = 0; t < 210; ++t)
    {
        // Set food supply to match last tick's demand (or enough to be >= 50%).
        market_component& mc = w.markets.at(mkt);
        const std::size_t food_idx = static_cast<std::size_t>(resource_type::agricultural_produce);
        mc.supply[food_idx] = mc.demand[food_idx] > 0.0f ? mc.demand[food_idx] : 1.0f;

        const economy_report rep = run_economy_step(w, reg);
        clear_markets(w, reg, rep);
    }

    const population_centre_component& pcc_after = w.population_centres.at(pop);
    std::printf("  scale after 210 ticks: %d (growth_accumulator=%d)\n",
                pcc_after.scale, pcc_after.growth_accumulator);
    check(pcc_after.scale >= 2, "pop centre at the scale-2 rung promotes after the sustained window (BL-616)");
}

// ---------------------------------------------------------------------------
// BL-1163 (play villages decline): the growth gate is judged PER CENTRE, at the
// centre's OWN market, over the HOUSEHOLD basket it bids there
// (population_met_ratio). This replaces BL-357's body-wide aggregate, which it
// deliberately reverses: one market's shortage must not shrink a centre that
// clears elsewhere, and another market's plenty must not grow one that starves.
// Two markets on one body, each anchored on its own tile; the centre stands on
// A's anchor, so it clears at A. Market state is set by hand each tick (no
// clear_markets — the growth step reads the household bid and fill as the
// previous clearing left them; BL-1196 made the fill a real draw, so the
// registers are `household_bid` / `household_fill`, not supply / demand).
//   Run 1: A met (10/10), B starved (0/10)          -> grows (B is not its market)
//   Run 2: A starved (0/10), B met (10/10)          -> does NOT grow (the old
//          aggregate read 10/20 = 0.5 and grew it)
//   Run 3: A's household good met, A's STEEL 0/10   -> grows (steel is a firm's
//          want, not the household's — out of the basket)
// ---------------------------------------------------------------------------
static void test_multi_market_growth_aggregate()
{
    std::printf("--- BL-1163: growth gate judged at the centre's own market, household basket ---\n");

    world w;

    const entity_id body = w.create_entity();
    {
        body_component bc{};
        bc.name        = "TwoMkt";
        bc.grid_width  = 10;
        bc.grid_height = 4;
        w.bodies[body] = bc;
    }

    auto make_tile = [&](int x, int y) {
        const entity_id t = w.create_entity();
        tile_component tc{};
        tc.body   = body;
        tc.grid_x = x;
        tc.grid_y = y;
        w.tiles[t] = tc;
        return t;
    };
    const entity_id tile   = make_tile(0, 0); // A's anchor; the centre stands here
    const entity_id tile_b = make_tile(9, 3); // B's anchor, far side of the body

    const std::size_t food  = ri(resource_type::agricultural_produce);
    const std::size_t steel = ri(resource_type::steel);

    auto make_market = [&](entity_id anchor) {
        const entity_id m = w.create_entity();
        market_component mc{};
        mc.body        = body;
        mc.centre_tile = anchor;
        mc.base_price[food]  = 1.0f; // priced, so the household bids it
        mc.base_price[steel] = 1.0f;
        w.markets[m] = mc;
        return m;
    };
    const entity_id mkt_a = make_market(tile);
    const entity_id mkt_b = make_market(tile_b);
    check(market_for_tile(w, tile) == mkt_a, "fixture: the centre's tile clears at market A");

    const entity_id pop = w.create_entity();
    {
        population_centre_component pcc{};
        pcc.scale        = 1;
        pcc.population   = 50; // at the scale-2 rung — BL-616 promotion is population-driven (see R3 above)
        pcc.habitability = 0.9f;
        w.population_centres[pop]     = pcc;
        w.population_centre_tile[pop] = tile;
    }

    // The household basket: one good. No growth-side list exists any more.
    recipe_registry reg;
    {
        population_demand_params pd;
        pd.demand_basket[food] = 1.0f;
        reg.set_population_demand(pd);
    }

    auto run = [&](float a_food, float b_food, float a_steel) {
        population_centre_component& pcc = w.population_centres.at(pop);
        pcc.scale              = 1;
        pcc.population         = 50;
        pcc.growth_accumulator = 0;
        for (int t = 0; t < 210; ++t)
        {
            market_component& a = w.markets.at(mkt_a);
            market_component& b = w.markets.at(mkt_b);
            a.household_bid[food]  = 10.0f; a.household_fill[food]  = a_food;
            b.household_bid[food]  = 10.0f; b.household_fill[food]  = b_food;
            // Steel: a firm's want on the ordinary registers. Never a household
            // bid, so it never reaches the gate (run 3).
            a.demand[steel] = 10.0f; a.supply[steel] = a_steel;
            run_economy_step(w, reg);
        }
        return w.population_centres.at(pop).scale;
    };

    const int s1 = run(10.0f, 0.0f, 10.0f);
    std::printf("  run 1 (A met, B starved): scale %d, met at A %.2f\n", s1,
                population_met_ratio(w, reg, mkt_a));
    check(s1 >= 2, "centre grows when ITS market fills the household bid (B's shortage is not its)");

    const int s2 = run(0.0f, 10.0f, 10.0f);
    std::printf("  run 2 (A starved, B met): scale %d, met at A %.2f\n", s2,
                population_met_ratio(w, reg, mkt_a));
    check(s2 == 1, "centre does not grow when its own market starves, whatever B holds");

    const int s3 = run(10.0f, 0.0f, 0.0f);
    std::printf("  run 3 (A household met, A steel 0/10): scale %d, met at A %.2f\n", s3,
                population_met_ratio(w, reg, mkt_a));
    check(s3 >= 2, "a firm's want (steel) unmet does not hold a centre back");
}

// ---------------------------------------------------------------------------
// BL-1196 (households consume): MULTI-TICK. The households a market serves draw
// their bid off its shelf at the end of every clear, and the next tick's growth
// gate reads what they received (household_fill / household_bid). A one-tick
// row cannot see a shelf drain, so every row here runs the real tick pair
// (run_economy_step + clear_markets) for 60 ticks.
//
// The fixture pins the bid: elasticity exponent 0 (factor 1 at any price) and
// demand_scale 10, so a scale-1 centre bids exactly 10 food a tick. Population
// 20 sits between the scale-1 and scale-2 rungs (10 / 50), so no promotion or
// demotion changes the bid mid-run; a growth or shed step is max(1, 20/25) = 1.
// No corporation exists, so nothing is ever LISTED — supply stays 0 every tick.
// Under the pro-rata stand-in (supply / demand) such a centre read met 0 and
// could never grow beside a full shelf; under the draw it eats the shelf.
//
//   Row D1 (draw down): shelf 300. Every clear takes min(10, shelf) — the shelf
//          falls by exactly the fill, to 0 after 30 clears, 300 drawn in all.
//          While it lasts the centre grows; once it is bare the centre sheds.
//   Row D2 (threshold): the shelf is restocked by hand before every tick (a
//          stand-in seller) with 0.6 x the bid: met 0.6 -> grows. With 0.4 x
//          the bid: met 0.4 -> sheds. The draw, not the stock, decides.
//   Row D3 (pooled, pro rata): two centres (scale 1 and 2 -> bids 10 and 20)
//          share one market restocked with 15 a tick: the pooled bid is 30,
//          the fill 15, and both centres read met 0.5.
// ---------------------------------------------------------------------------
static void test_household_draw_multi_tick()
{
    std::printf("--- BL-1196: households draw their basket off the shelf; growth reads the fill ---\n");

    const std::size_t food = ri(resource_type::agricultural_produce);

    struct fixture
    {
        world w;
        entity_id mkt = null_entity;
        std::vector<entity_id> pops;
        recipe_registry reg;
    };
    auto make = [&](std::initializer_list<int> scales) {
        auto f = std::make_unique<fixture>();
        world& w = f->w;
        const entity_id body = w.create_entity();
        {
            body_component bc{};
            bc.name        = "Shelf";
            bc.grid_width  = 4;
            bc.grid_height = 4;
            w.bodies[body] = bc;
        }
        const entity_id tile = w.create_entity();
        {
            tile_component tc{};
            tc.body = body;
            w.tiles[tile] = tc;
        }
        f->mkt = w.create_entity();
        {
            market_component mc{};
            mc.body            = body;
            mc.base_price[food] = 1.0f;
            w.markets[f->mkt]  = mc;
        }
        for (const int sc : scales)
        {
            const entity_id pop = w.create_entity();
            population_centre_component pcc{};
            pcc.scale        = sc;
            pcc.population   = (sc == 1) ? 20 : 120; // each between its rungs
            pcc.habitability = 0.9f;
            w.population_centres[pop]     = pcc;
            w.population_centre_tile[pop] = tile;
            f->pops.push_back(pop);
        }
        population_demand_params pd;
        pd.demand_basket[food] = 1.0f;
        pd.demand_elasticity   = 0.0f;  // factor 1 at every price
        pd.demand_scale        = 10.0f; // 10 a tick per scale point
        f->reg.set_population_demand(pd);
        return f;
    };
    auto tick = [](fixture& f) {
        const economy_report rep = run_economy_step(f.w, f.reg);
        clear_markets(f.w, f.reg, rep);
    };

    // ---- D1: a 300-unit shelf drawn down over 60 clears -------------------
    {
        auto f = make({ 1 });
        f->w.markets.at(f->mkt).inventory[food] = 300.0f;
        float drawn = 0.0f, max_err = 0.0f, max_supply = 0.0f;
        int pop_at_empty = -1, tick_empty = -1;
        for (int t = 1; t <= 60; ++t)
        {
            const float before = f->w.markets.at(f->mkt).inventory[food];
            tick(*f);
            const market_component& mc = f->w.markets.at(f->mkt);
            const float fill = mc.household_fill[food];
            drawn += fill;
            max_err = std::max(max_err, std::fabs((before - fill) - mc.inventory[food]));
            max_err = std::max(max_err, std::fabs(fill - std::min(mc.household_bid[food], before)));
            max_supply = std::max(max_supply, mc.supply[food]);
            if (tick_empty < 0 && mc.inventory[food] <= 0.0f)
            {
                tick_empty   = t;
                // The growth pass of the NEXT tick still reads this full fill,
                // so the fed streak ends one tick later; read pop here anyway.
                pop_at_empty = f->w.population_centres.at(f->pops[0]).population;
            }
        }
        const market_component& mc = f->w.markets.at(f->mkt);
        const int pop_end = f->w.population_centres.at(f->pops[0]).population;
        std::printf("  D1 bid %.2f/tick, nothing listed (max supply %.2f); shelf bare after clear %d, %.2f drawn in all\n",
                    mc.household_bid[food], max_supply, tick_empty, drawn);
        std::printf("     population 20 -> %d at the empty shelf -> %d at tick 60 (met now %.2f)\n",
                    pop_at_empty, pop_end, population_met_ratio(f->w, f->reg, f->mkt));
        check(std::fabs(mc.household_bid[food] - 10.0f) < 1e-4f, "D1 the pinned bid is 10 a tick",
              mc.household_bid[food], 10.0f);
        check(max_err < 1e-3f, "D1 every clear: fill = min(bid, shelf) and the shelf falls by exactly the fill",
              max_err, 0.0f);
        check(tick_empty == 30, "D1 the 300 shelf is bare after exactly 30 clears", static_cast<float>(tick_empty), 30.0f);
        check(std::fabs(drawn - 300.0f) < 1e-2f, "D1 households drew the whole shelf and nothing more", drawn, 300.0f);
        check(max_supply == 0.0f, "D1 nothing was listed: the fill came off the shelf, not off listings");
        check(pop_at_empty > 20, "D1 the centre GREW while its draw met the bid (supply was 0 the whole time)",
              static_cast<float>(pop_at_empty), 21.0f);
        check(pop_end < pop_at_empty, "D1 the centre SHED once the shelf was bare",
              static_cast<float>(pop_end), static_cast<float>(pop_at_empty));
    }

    // ---- D2: restocked at 0.6 / 0.4 of the bid -----------------------------
    auto restocked = [&](float share, float& met_out) {
        auto f = make({ 1 });
        for (int t = 1; t <= 60; ++t)
        {
            f->w.markets.at(f->mkt).inventory[food] += share * 10.0f; // the stand-in seller
            tick(*f);
        }
        met_out = population_met_ratio(f->w, f->reg, f->mkt);
        return f->w.population_centres.at(f->pops[0]).population;
    };
    {
        float met_hi = 0.0f, met_lo = 0.0f;
        const int pop_hi = restocked(0.6f, met_hi);
        const int pop_lo = restocked(0.4f, met_lo);
        std::printf("  D2 restocked 0.6 x bid: met %.2f, population 20 -> %d; 0.4 x bid: met %.2f, 20 -> %d\n",
                    met_hi, pop_hi, met_lo, pop_lo);
        check(std::fabs(met_hi - 0.6f) < 1e-4f && pop_hi > 20, "D2 a draw meeting 0.6 of the bid grows the centre",
              static_cast<float>(pop_hi), 21.0f);
        check(std::fabs(met_lo - 0.4f) < 1e-4f && pop_lo < 20, "D2 a draw meeting 0.4 of the bid sheds it",
              static_cast<float>(pop_lo), 19.0f);
    }

    // ---- D3: two centres, one market, pooled pro rata ----------------------
    {
        auto f = make({ 1, 2 });
        for (int t = 1; t <= 5; ++t)
        {
            f->w.markets.at(f->mkt).inventory[food] += 15.0f;
            tick(*f);
        }
        const market_component& mc = f->w.markets.at(f->mkt);
        const float met = population_met_ratio(f->w, f->reg, f->mkt);
        std::printf("  D3 pooled bid %.2f, fill %.2f, shelf %.2f, met %.2f (both centres read it)\n",
                    mc.household_bid[food], mc.household_fill[food], mc.inventory[food], met);
        check(std::fabs(mc.household_bid[food] - 30.0f) < 1e-3f && std::fabs(mc.household_fill[food] - 15.0f) < 1e-3f
                  && mc.inventory[food] < 1e-3f,
              "D3 two centres' bids pool (10 + 20) and draw the 15 restocked", mc.household_fill[food], 15.0f);
        check(std::fabs(met - 0.5f) < 1e-4f, "D3 every centre on the market reads the same share, 0.5", met, 0.5f);
    }
}

// ---------------------------------------------------------------------------
int main()
{
    test_population_on_kepler();
    test_agglomeration_and_pool();
    test_habitability_scalar();
    test_population_growth();
    test_multi_market_growth_aggregate();
    test_household_draw_multi_tick();

    if (g_failures == 0)
        std::printf("\nALL PASS (%d assertions)\n", g_passes);
    else
        std::printf("\n%d FAILURE(s) / %d assertions\n", g_failures, g_passes + g_failures);

    return g_failures > 0 ? 1 : 0;
}
