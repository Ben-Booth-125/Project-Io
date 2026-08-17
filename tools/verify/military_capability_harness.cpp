// Headless harness for BL-332 (research building) and BL-455 (its reader): a
// COMPLETED research_institute credits its owning corp's science every tick,
// symmetrically across player and non-player corps, and accumulates nothing
// while under construction or decommissioned — and that accumulator is now
// MEASURABLE by condition_subject::science, so a tech gate or law can require
// a research level.
//
// BL-455 (2026-08-17) deleted corporation_component::military_points and every
// assertion about it. It was credited per tick by a completed military_base and
// read by nothing in src/. The rows are gone rather than commented out: a
// harness asserting a deleted field's behaviour is the "green check over a
// number nothing reads" this item existed to remove.
// Kept outside src/ so the CMake glob does not pull it into the real build.

#include "world/budget_system.hpp"
#include "world/condition_set.hpp"
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

    // BL-455 (2026-08-17): every military_points row is GONE with the field.
    // R1's surviving half is the one that still says something — a corp holding
    // only a military_base accumulates NOTHING, which after the deletion is a
    // statement about the base rather than about a second accumulator.
    check(near(w.corporations[corp_a].science, 0.0f),
          "R1 a corp with only a military_base accumulates no science");

    check(near(w.corporations[corp_b].science, 5.0f),
          "R2 a completed research_institute credits science, on a NON-PLAYER corp (Q4 symmetry)");

    check(near(w.corporations[corp_c].science, 0.0f),
          "R3 a decommissioned research_institute accumulates nothing");

    // --- BL-455: science now has a READER, which is the whole point of keeping
    // it. condition_subject::science measures the accumulator, so a tech gate or
    // a law can require a research level. Without these rows the deletion half
    // of this item would be verified and the wiring half would not.
    {
        condition c;
        c.subject    = condition_subject::science;
        c.comparator = condition_comparator::at_least;

        c.operand = 5.0f;
        check(evaluate_condition(c, w, corp_b),
              "R4 a gate requiring 5 research is MET by the corp that earned 5");
        check(!evaluate_condition(c, w, corp_a),
              "R4 the same gate is NOT met by a corp with no research institute");

        c.operand = 5.5f;
        check(!evaluate_condition(c, w, corp_b),
              "R4 the gate is not met one step above what was earned");

        // Continuous, not integral: 5 units of research is 5.0, and a gate at
        // 4.5 must pass rather than round. Asserted because the classification
        // is a hand-maintained switch and getting it wrong is invisible.
        c.operand = 4.5f;
        check(evaluate_condition(c, w, corp_b),
              "R5 science compares as a float, not rounded to a count");
        check(!condition_subject_is_integral(condition_subject::science),
              "R5 science is classified continuous");
    }

    if (g_failures == 0) std::printf("\nALL PASS  (0 failures)\n");
    else                 std::printf("\nFAILURES (%d failures)\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
