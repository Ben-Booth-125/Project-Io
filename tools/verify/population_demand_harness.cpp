// Headless harness for the BL-190 population-demand ordering fix (2026-07-31).
//
// The wrinkle: run_economy_step used to write the population agricultural_produce
// demand stub directly into market demand, but clear_markets zero-resets demand
// before accumulating its own — so the population signal was erased the same tick
// and never reached price resolution. The fix moves the injection into
// clear_markets (inject_population_demand, market_clearing.cpp), after the reset,
// alongside the BL-078 substrate injection.
//
// Verifies without SDL / Lua / ImGui:
//   R1 (unit): inject_population_demand adds scale units of agricultural_produce
//       demand to a centre's catchment market.
//   R2 (integration): after run_economy_step + clear_markets on the hard-coded
//       world with an empty registry (no substrate basket, no production), the
//       summed agricultural_produce demand across Kepler's markets equals the
//       summed scale of Kepler's population centres — the signal survives into
//       the cleared market state that price resolution reads.
//   R3 (ordering contract): demand pre-seeded before clear_markets is erased by
//       the reset, while the population injection still lands — stale demand
//       does not leak, the population pull does.
//
// Build + run via the verifier-headless skill (build_gen\verify\). Source set
// matches population_mvp.cpp (world, generation, economy, market_clearing,
// budget; recipe_registry.cpp NOT linked — Lua-dependent, header-inline only).

#include "world/components.hpp"
#include "world/economy_system.hpp"
#include "world/hard_coded_world.hpp"
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

    inject_population_demand(w);

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

    world w = make_hard_coded_world();
    const entity_id kepler = w.home_body;

    // Empty registry: no substrate basket, no recipes → the only agricultural
    // demand after clearing is the population injection.
    recipe_registry reg;

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
int main()
{
    test_unit_injection();
    test_ordering_survives_clearing();

    if (g_failures == 0)
        std::printf("\nALL PASS (%d assertions)\n", g_passes);
    else
        std::printf("\n%d FAILURE(s) / %d assertions\n", g_failures, g_passes + g_failures);

    return g_failures > 0 ? 1 : 0;
}
