// Headless harness for the BL-190 population-demand ordering fix (2026-07-31)
// and the BL-368 basket generalisation (2026-08-11) built on top of it.
//
// The wrinkle: run_economy_step used to write the population agricultural_produce
// demand stub directly into market demand, but clear_markets zero-resets demand
// before accumulating its own — so the population signal was erased the same tick
// and never reached price resolution. The fix moves the injection into
// clear_markets (inject_population_demand, market_clearing.cpp), after the reset,
// alongside the BL-078 substrate injection. BL-368 then generalised the single
// flat-1.0-agricultural_produce stub into a real, price-elastic, multi-resource
// basket (population_demand_params, economy.population_demand in Lua) — this
// harness configures demand_elasticity = 0 throughout so the elastic factor is
// always exactly 1 (pow(x, 0) == 1), reproducing the OLD flat behaviour
// deterministically without depending on price drift between ticks; the elastic
// shape itself is exercised by R4 below.
//
// Verifies without SDL / Lua / ImGui:
//   R1 (unit): inject_population_demand adds scale x basket[r] units of demand
//       to a centre's catchment market, for a hand-configured single-resource
//       basket.
//   R2 (integration): after run_economy_step + clear_markets on the hard-coded
//       world with a hand-configured flat agricultural_produce basket, the
//       summed demand across Kepler's markets equals the summed scale of
//       Kepler's population centres — the signal survives into the cleared
//       market state that price resolution reads.
//   R3 (ordering contract): demand pre-seeded before clear_markets is erased by
//       the reset, while the population injection still lands — stale demand
//       does not leak, the population pull does.
//   R4 (BL-368 elasticity + multi-resource): a basket with TWO resources and
//       nonzero elasticity — a price above base suppresses demand below the
//       flat scale x weight figure, a price below base raises it above; an
//       untradeable resource (base_price 0) is skipped regardless of its
//       basket weight.
//
// Build + run via the verifier-headless skill (build_gen\verify\). Source set
// matches population_mvp.cpp (world, generation, economy, market_clearing,
// budget; recipe_registry.cpp NOT linked — Lua-dependent, header-inline only,
// so every population_demand_params here is hand-set via set_population_demand).

#include "world/components.hpp"
#include "world/economy_system.hpp"
#include "world/hard_coded_world.hpp"
#include "harness_params.hpp"
#include "world/market_clearing.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <cmath>
#include <cstdio>

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

static std::size_t agri()
{
    return static_cast<std::size_t>(resource_type::agricultural_produce);
}

// ---------------------------------------------------------------------------
// R1: unit — inject_population_demand routes scale units to the catchment market.
// ---------------------------------------------------------------------------
static void test_unit_injection()
{
    std::printf("--- R1: inject_population_demand unit behaviour ---\n");

    world w;

    const entity_id body = w.create_entity();
    {
        body_component bc{};
        bc.name        = "UnitBody";
        bc.grid_width  = 10;
        bc.grid_height = 4;
        w.bodies[body] = bc;
    }

    const entity_id mkt = w.create_entity();
    {
        market_component mc{};
        mc.body = body;
        mc.base_price[agri()] = 1.0f; // must be > 0 to carry a basket weight at all
        w.markets[mkt] = mc;
    }

    const entity_id tile = w.create_entity();
    {
        tile_component tc{};
        tc.body   = body;
        tc.grid_x = 0;
        tc.grid_y = 0;
        w.tiles[tile] = tc;
    }

    {
        const entity_id pop = w.create_entity();
        population_centre_component pcc{};
        pcc.scale        = 3;
        pcc.population   = 30;
        pcc.habitability = 1.0f;
        w.population_centres[pop]     = pcc;
        w.population_centre_tile[pop] = tile;
    }

    recipe_registry reg;
    {
        population_demand_params pd;
        pd.demand_basket[agri()] = 1.0f;
        pd.demand_elasticity     = 0.0f; // pow(x, 0) == 1 -> deterministic flat behaviour
        reg.set_population_demand(pd);
    }
    inject_population_demand(w, reg);

    const float demand = w.markets.at(mkt).demand[agri()];
    check(std::fabs(demand - 3.0f) < 1e-4f,
          "scale-3 centre adds 3.0 agricultural_produce demand to its market",
          demand, 3.0f);
}

// ---------------------------------------------------------------------------
// Shared: expected population demand routed to a body's markets, mirroring the
// injector's routing (tile → market_for_tile catchment).
// ---------------------------------------------------------------------------
static float expected_body_demand(const world& w, entity_id body)
{
    float sum = 0.0f;
    for (const auto& [cid, pcc] : w.population_centres)
    {
        const auto tile_it = w.population_centre_tile.find(cid);
        if (tile_it == w.population_centre_tile.end())
            continue;
        const entity_id mid = market_for_tile(w, tile_it->second);
        if (mid == null_entity)
            continue;
        if (w.markets.at(mid).body == body)
            sum += static_cast<float>(pcc.scale);
    }
    return sum;
}

static float cleared_body_demand(const world& w, entity_id body)
{
    float sum = 0.0f;
    for (const auto& [mid, mc] : w.markets)
        if (mc.body == body)
            sum += mc.demand[agri()];
    return sum;
}

// ---------------------------------------------------------------------------
// R2 + R3: the demand survives clear_markets' reset; pre-seeded stale demand
// does not.
// ---------------------------------------------------------------------------
static void test_ordering_survives_clearing()
{
    std::printf("--- R2: population demand survives into the cleared state ---\n");

    world w = make_hard_coded_world(no_prehistory());
    const entity_id kepler = w.home_body;

    // No recipes, no substrate basket -> the only agricultural_produce demand
    // after clearing is the population injection. Flat basket weight 1.0,
    // elasticity 0 (pow(x,0)==1) reproduces the pre-BL-368 flat-scale figure
    // deterministically, independent of the price drift between ticks.
    recipe_registry reg;
    {
        population_demand_params pd;
        pd.demand_basket[agri()] = 1.0f;
        pd.demand_elasticity     = 0.0f;
        reg.set_population_demand(pd);
    }

    const float expected = expected_body_demand(w, kepler);
    std::printf("  Kepler expected population demand: %.1f\n", expected);
    check(expected > 0.0f, "Kepler has population centres routed to markets",
          expected, 1.0f);

    const economy_report report = run_economy_step(w, reg);
    clear_markets(w, reg, report);

    const float got = cleared_body_demand(w, kepler);
    std::printf("  Kepler cleared agricultural_produce demand: %.1f\n", got);
    check(std::fabs(got - expected) < 1e-3f,
          "cleared demand equals summed centre scale (survived the reset)",
          got, expected);

    std::printf("--- R3: stale pre-clearing demand is reset, injection lands ---\n");

    // Pre-seed garbage demand the way the old run_economy_step stub did — it
    // must be erased by the reset while the injection still arrives.
    for (auto& [mid, mc] : w.markets)
        if (mc.body == kepler)
            mc.demand[agri()] += 999.0f;

    const economy_report report2 = run_economy_step(w, reg);
    clear_markets(w, reg, report2);

    const float got2 = cleared_body_demand(w, kepler);
    std::printf("  Kepler cleared agricultural_produce demand: %.1f\n", got2);
    check(std::fabs(got2 - expected) < 1e-3f,
          "pre-seeded 999/market erased; population injection intact",
          got2, expected);
}

// ---------------------------------------------------------------------------
// R4 (BL-368): elasticity + multi-resource basket. Price above base suppresses
// demand below the flat scale x weight figure; price below base raises it;
// an untradeable resource (base_price 0) is skipped regardless of weight.
// ---------------------------------------------------------------------------
static void test_elastic_multi_resource_basket()
{
    std::printf("--- R4: BL-368 elasticity + multi-resource basket ---\n");

    world w;
    const entity_id body = w.create_entity();
    { body_component bc{}; bc.grid_width = 4; bc.grid_height = 4; w.bodies[body] = bc; }

    const std::size_t water = static_cast<std::size_t>(resource_type::water);
    const std::size_t untradeable = static_cast<std::size_t>(resource_type::steel);

    const entity_id mkt = w.create_entity();
    {
        market_component mc{};
        mc.body = body;
        mc.base_price[agri()] = 4.0f;
        mc.price[agri()]      = 8.0f;  // 2x base -> demand suppressed
        mc.base_price[water]  = 2.0f;
        mc.price[water]       = 1.0f;  // 0.5x base -> demand raised
        // steel (untradeable here): base_price left 0 -> must be skipped
        // even though the basket below weights it.
        w.markets[mkt] = mc;
    }

    const entity_id tile = w.create_entity();
    { tile_component tc{}; tc.body = body; w.tiles[tile] = tc; }

    {
        const entity_id pop = w.create_entity();
        population_centre_component pcc{};
        pcc.scale = 10;
        w.population_centres[pop]     = pcc;
        w.population_centre_tile[pop] = tile;
    }

    recipe_registry reg;
    {
        population_demand_params pd;
        pd.demand_basket[agri()]       = 1.0f;
        pd.demand_basket[water]        = 1.0f;
        pd.demand_basket[untradeable]  = 5.0f; // large weight, but base_price is 0
        pd.demand_elasticity = 1.0f;           // linear in (base/price) for a legible check
        pd.elasticity_min    = 0.0f;
        pd.elasticity_max    = 10.0f;
        reg.set_population_demand(pd);
    }

    inject_population_demand(w, reg);

    const market_component& mc = w.markets.at(mkt);
    const float flat = 10.0f; // scale x weight(1.0) x demand_scale(1.0), no elasticity

    check(mc.demand[agri()] < flat,
          "price 2x base suppresses agricultural_produce demand below the flat figure",
          mc.demand[agri()], flat);
    check(std::fabs(mc.demand[agri()] - flat * (4.0f / 8.0f)) < 1e-3f,
          "agricultural_produce demand matches scale x weight x (base/price)^elasticity exactly",
          mc.demand[agri()], flat * (4.0f / 8.0f));
    check(mc.demand[water] > flat,
          "price 0.5x base raises water demand above the flat figure",
          mc.demand[water], flat);
    check(mc.demand[untradeable] == 0.0f,
          "an untradeable resource (base_price 0) is skipped despite a nonzero basket weight",
          mc.demand[untradeable], 0.0f);
}

// ---------------------------------------------------------------------------
int main()
{
    test_unit_injection();
    test_ordering_survives_clearing();
    test_elastic_multi_resource_basket();

    if (g_failures == 0)
        std::printf("\nALL PASS (%d assertions)\n", g_passes);
    else
        std::printf("\n%d FAILURE(s) / %d assertions\n", g_failures, g_passes + g_failures);

    return g_failures > 0 ? 1 : 0;
}
