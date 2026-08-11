// Headless harness for BL-332 (military points + research building): a
// COMPLETED military_base credits its owning corp's military_points every
// tick, a COMPLETED research_institute credits science, both are symmetric
// across player and non-player corps, and neither accumulates while the
// building is still under construction or decommissioned.
// Kept outside src/ so the CMake glob does not pull it into the real build.

#include "world/budget_system.hpp"
#include "world/components.hpp"
#include "world/economy_system.hpp"
#include "world/market_clearing.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <cmath>
#include <cstdio>

static int g_failures = 0;

static void check(bool cond, const char* what)
{
    if (cond) std::printf("  PASS  %s\n", what);
    else      { std::printf("  FAIL  %s\n", what); ++g_failures; }
}

static bool near(float a, float b) { return std::fabs(a - b) < 1e-3f; }

int main()
{
    recipe_registry reg; // default military_capability_params: 1.0 / 1.0 per tick

    // run_construction's "instant safety" (economy_system.cpp) zeroes
    // ticks_remaining outright for any building_type whose economics carry
    // build_duration_ticks <= 0 — the default for an unconfigured registry.
    // Give military_base a real duration so the under-construction case
    // below is actually gated by ticks_remaining, not short-circuited.
    { building_economics e; e.build_duration_ticks = 4.0f;
      reg.set_economics(building_type::military_base, e); }

    world w;
    const entity_id body = w.create_entity();
    w.bodies[body] = body_component{};

    auto make_building = [&](building_type type, int ticks_remaining, bool decommissioned) {
        const entity_id tile = w.create_entity();
        tile_component tc{}; tc.body = body; w.tiles[tile] = tc;
        const entity_id bld = w.create_entity();
        building_component b{};
        b.tile = tile; b.type = type; b.workforce_assigned = 0.0f;
        b.ticks_remaining = ticks_remaining; b.decommissioned = decommissioned;
        w.buildings[bld] = b;
        return bld;
    };

    // Corp A (player, standing in for "the militia's own corp"): one
    // COMPLETED military_base.
    const entity_id corp_a = w.create_entity();
    { corporation_component cc; cc.is_player = true; w.corporations[corp_a] = cc; }
    w.corporations[corp_a].assets.push_back(make_building(building_type::military_base, 0, false));

    // Corp B (NOT player — a rival/background corp, BL-332's Q4: symmetric
    // accumulation): one COMPLETED research_institute.
    const entity_id corp_b = w.create_entity();
    { corporation_component cc; cc.is_player = false; w.corporations[corp_b] = cc; }
    w.corporations[corp_b].assets.push_back(make_building(building_type::research_institute, 0, false));

    // Corp C: a military_base still under construction (ticks_remaining > 0)
    // and a decommissioned research_institute — neither should accumulate.
    const entity_id corp_c = w.create_entity();
    { corporation_component cc; cc.is_player = false; w.corporations[corp_c] = cc; }
    // ticks_remaining set well past this harness's run length, so the base
    // stays under construction for the whole test (run_construction advances
    // it toward 0 each tick, and this must never reach 0 within k_ticks).
    w.corporations[corp_c].assets.push_back(make_building(building_type::military_base, 1000, false));
    w.corporations[corp_c].assets.push_back(make_building(building_type::research_institute, 0, true));

    constexpr int k_ticks = 5;
    for (int t = 0; t < k_ticks; ++t)
    {
        economy_report rep = run_economy_step(w, reg);
        auto flows = clear_markets(w, reg, rep);
        apply_budget(w, reg, flows, rep.workforce_contention);
    }

    std::printf("Military-capability harness (BL-332), %d ticks\n", k_ticks);

    check(near(w.corporations[corp_a].military_points, 5.0f),
          "R1 a completed military_base credits military_points at the authored per-tick rate");
    check(near(w.corporations[corp_a].science, 0.0f),
          "R1 that corp's science stays at zero (no research_institute)");

    check(near(w.corporations[corp_b].science, 5.0f),
          "R2 a completed research_institute credits science, on a NON-PLAYER corp (Q4 symmetry)");
    check(near(w.corporations[corp_b].military_points, 0.0f),
          "R2 that corp's military_points stays at zero (no military_base)");

    check(near(w.corporations[corp_c].military_points, 0.0f),
          "R3 a base still under construction (ticks_remaining > 0) accumulates nothing");
    check(near(w.corporations[corp_c].science, 0.0f),
          "R3 a decommissioned research_institute accumulates nothing");

    if (g_failures == 0) std::printf("\nALL PASS  (0 failures)\n");
    else                 std::printf("\nFAILURES (%d failures)\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
