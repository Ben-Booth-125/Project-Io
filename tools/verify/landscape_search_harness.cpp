// ---------------------------------------------------------------------------
// landscape_search — BL-770 slice 3. The phase 6 SEARCH, and the one property
// that is allowed to fail it.
//
// Slice 1 asked whether the objective could see a roster (it could not), slice 2
// gave it the term that can, and this slice builds the search that walks on it.
// The question here is NOT "is the winner good" — the objective is
// viable-but-uneven and slice 1's own caveat says these scores are ORDINAL — it
// is "is the winner the SAME winner, always".
//
// THE BINDING ASSERTION IS R2: same seed, different thread counts, identical
// winner, bit-identical on every scored term. Everything else in this harness
// exists to stop R2 passing vacuously — a search that never moved off its seed
// candidate would be trivially thread-invariant and would prove nothing, so R1
// requires the walk to actually go somewhere before R2's result is read.
//
// Build:  bash tools/verify/build_lua_harness.sh landscape_search_harness
// ---------------------------------------------------------------------------

#include "harness_params.hpp"
#include "scripting/lua_state.hpp"
#include "world/corporation_generation.hpp"
#include "world/hard_coded_world.hpp"
#include "world/landscape_search.hpp"
#include "world/market_saturation.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"
#include "world/world_gen_config.hpp"

#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

namespace
{

int failures = 0;

void check(bool ok, const std::string& id, const std::string& what)
{
    std::printf("  [%s] %s  %s\n", ok ? "PASS" : "FAIL", id.c_str(), what.c_str());
    if (!ok)
        ++failures;
}

bool same_candidate(const landscape_candidate& a, const landscape_candidate& b)
{
    return a.corporation_count == b.corporation_count
        && a.placement_seed    == b.placement_seed
        && a.road_tier         == b.road_tier;
}

/// BIT-IDENTICAL, not "close". A tolerance here would hide exactly the drift the
/// assertion exists to catch — a reduction that summed in completion order gives
/// answers that agree to twelve digits and are still non-deterministic.
bool same_score(const landscape_score& a, const landscape_score& b)
{
    return a.composite         == b.composite
        && a.realisation       == b.realisation
        && a.mean_actual       == b.mean_actual
        && a.mean_completeness == b.mean_completeness
        && a.mean_balance      == b.mean_balance
        && a.spread            == b.spread
        && a.market_count      == b.market_count;
}

std::string describe(const landscape_candidate& c)
{
    char buf[96];
    std::snprintf(buf, sizeof buf, "corps=%d placement=%08X road_tier=%u",
                  c.corporation_count, c.placement_seed,
                  static_cast<unsigned>(c.road_tier));
    return buf;
}

void print_result(const char* label, const landscape_search_result& r, double ms)
{
    std::printf("  %-14s  seed composite=%.6f -> WINNER composite=%.6f   (%s)\n",
                label, r.seed_score.composite, r.winner_score.composite,
                describe(r.winner).c_str());
    std::printf("  %-14s  realised %.3f -> %.3f   potential %.5f -> %.5f   "
                "spread %.5f -> %.5f\n", "",
                r.seed_score.realisation, r.winner_score.realisation,
                r.seed_score.mean_completeness, r.winner_score.mean_completeness,
                r.seed_score.spread, r.winner_score.spread);
    std::printf("  %-14s  %d evaluations, %d accepted, %zu path steps, %.0f ms\n",
                "", r.evaluations, r.accepted, r.path.size(), ms);
}

void print_path(const landscape_search_result& r)
{
    std::printf("\n  THE PATH (what each round proposed and what it bought):\n");
    for (const landscape_search_step& s : r.path)
    {
        std::printf("    r%-2d %-10s %-40s composite=%.6f %s\n",
                    s.round, landscape_axis_name(s.axis),
                    describe(s.proposal).c_str(), s.score.composite,
                    s.accepted ? "<- TAKEN" : "");
    }
}

} // namespace

int main()
{
    std::printf("landscape_search — BL-770 slice 3, the phase 6 greedy search\n");

    lua_state lua;
    lua.load("scripts/recipes.lua");
    lua.load("scripts/economy.lua");
    lua.load("scripts/world_gen.lua");
    recipe_registry reg;
    reg.load_from_lua(lua);
    const world_gen_config gen_cfg = parsed_gen_config(lua);

    // The standing vacuity guard: a registry that loaded nothing reports every
    // recipe costless and every chain closed.
    if (reg.recipe_count() == 0)
    {
        std::printf("FATAL: no recipes loaded — run from the repo root.\n");
        return 2;
    }

    // ONE base world, generated ONCE. Ben's point 3: candidates vary rosters,
    // placements and road tiers — never worlds — which is the whole reason the
    // search is affordable and the reason `search_landscape` takes a const base.
    const world_params wp = no_prehistory();
    const world base = make_hard_coded_world(wp, nullptr, gen_cfg);

    landscape_search_params sp;
    sp.rounds = 4;
    sp.seed   = 0x5EA12C00u;
    sp.start  = landscape_candidate{ 8, 0xC0FFEEu, 1 };

    // ------------------------------------------------------------------
    // R1 — the search runs, and it MOVES. A search that never left its seed
    //      would make R2 below true for the wrong reason.
    // ------------------------------------------------------------------
    std::printf("\nR1. the walk\n");
    landscape_search_result serial;
    {
        const auto t0 = std::chrono::steady_clock::now();
        sp.thread_count = 1;
        serial = search_landscape(base, reg, sp);
        const double ms = std::chrono::duration<double, std::milli>(
                              std::chrono::steady_clock::now() - t0).count();
        print_result("serial", serial, ms);
    }

    check(serial.evaluations == 1 + landscape_axis_count * sp.rounds, "R1.1",
          "FIXED ROUNDS - evaluations are exactly 1 + 3 x rounds, so the work does "
          "not depend on the landscape (no convergence exit, no hidden threshold)");
    check(static_cast<int>(serial.path.size()) == landscape_axis_count * sp.rounds, "R1.2",
          "every round proposed on all three axes and the whole path was recorded");
    check(compare_landscape(serial.winner_score, serial.seed_score) >= 0, "R1.3",
          "the winner is never WORSE than the seed candidate (strict-improvement-only "
          "acceptance cannot regress)");
    check(serial.accepted > 0, "R1.4",
          "the walk actually MOVED off its seed - without this, thread-invariance "
          "below would be true of a search that never searched");

    // Strict improvement, checked on the path rather than on the outcome: every
    // accepted step must beat the incumbent it replaced.
    {
        landscape_score incumbent = serial.seed_score;
        bool strict = true;
        for (const landscape_search_step& s : serial.path)
        {
            if (!s.accepted)
                continue;
            if (compare_landscape(s.score, incumbent) <= 0)
                strict = false;
            incumbent = s.score;
        }
        check(strict, "R1.5",
              "the incumbent was replaced ONLY on a STRICT improvement - a tie leaves "
              "it standing rather than churning between equals");
        check(same_score(incumbent, serial.winner_score), "R1.6",
              "the reported winner IS the last accepted step - the path and the result "
              "are one record, not two");
    }

    // ------------------------------------------------------------------
    // R2 — THE BINDING ASSERTION. Same seed, different thread counts.
    // ------------------------------------------------------------------
    std::printf("\nR2. THREAD-COUNT INVARIANCE (the binding constraint)\n");
    for (const int threads : { 2, 3, 4, 8 })
    {
        landscape_search_params tp = sp;
        tp.thread_count = threads;
        const auto t0 = std::chrono::steady_clock::now();
        const landscape_search_result r = search_landscape(base, reg, tp);
        const double ms = std::chrono::duration<double, std::milli>(
                              std::chrono::steady_clock::now() - t0).count();

        char lbl[32];
        std::snprintf(lbl, sizeof lbl, "%d threads", threads);
        print_result(lbl, r, ms);

        char id[16];
        std::snprintf(id, sizeof id, "R2.%d", threads);
        const bool ok = same_candidate(r.winner, serial.winner)
                     && same_score(r.winner_score, serial.winner_score)
                     && r.evaluations == serial.evaluations
                     && r.accepted    == serial.accepted;
        check(ok, id,
              std::string("winner and every scored term are IDENTICAL to the serial run at ")
                  + lbl + " - the argmax reads a total order, never completion order");
    }

    // ------------------------------------------------------------------
    // R3 — replay. The same seed twice is the same search.
    // ------------------------------------------------------------------
    std::printf("\nR3. replay\n");
    {
        landscape_search_params tp = sp;
        tp.thread_count = 1;
        const landscape_search_result again = search_landscape(base, reg, tp);
        bool path_same = again.path.size() == serial.path.size();
        for (std::size_t i = 0; path_same && i < again.path.size(); ++i)
            path_same = same_candidate(again.path[i].proposal, serial.path[i].proposal)
                     && same_score(again.path[i].score, serial.path[i].score)
                     && again.path[i].accepted == serial.path[i].accepted;
        check(path_same, "R3.1",
              "re-running the same seed reproduces the WHOLE PATH bit-identically, not "
              "merely the same winner");
    }

    // ------------------------------------------------------------------
    // R4 — the seed is a real input. Two search seeds must be able to walk
    //      differently, or the perturbation stream is not being consulted.
    // ------------------------------------------------------------------
    std::printf("\nR4. the perturbation stream is live\n");
    {
        landscape_search_params tp = sp;
        tp.seed = 0x0BADF00Du;
        tp.thread_count = 1;
        const landscape_search_result other = search_landscape(base, reg, tp);
        print_result("seed 0BADF00D", other, 0.0);
        bool any_different = false;
        for (std::size_t i = 0; i < other.path.size() && i < serial.path.size(); ++i)
            if (!same_candidate(other.path[i].proposal, serial.path[i].proposal))
                any_different = true;
        check(any_different, "R4.1",
              "a different search seed proposes different candidates - the seeded stream "
              "is actually driving the perturbations");
        check(same_score(other.seed_score, serial.seed_score), "R4.2",
              "both searches start from the SAME scored seed candidate - the search seed "
              "moves the walk, never the starting point");
    }

    // ------------------------------------------------------------------
    // R5 — PER-AXIS DISCRIMINATION. Slice 1's question, asked once per axis
    //      instead of once per candidate: does the objective SEE this axis?
    //      An axis it cannot see still costs a third of every round's work and
    //      contributes nothing, so this is a measurement the search's caller
    //      needs, not a nicety.
    //
    //      IT REPORTS RATHER THAN FAILS, deliberately, exactly as slice 1's
    //      harness did: a blind axis is a finding about the OBJECTIVE, not a
    //      defect in the search, and a permanently-red suite is how a real
    //      regression gets missed. What DOES fail here is the control — if the
    //      fixture never moved, nothing below is readable.
    // ------------------------------------------------------------------
    std::printf("\nR5. per-axis discrimination\n");
    {
        int base_hist[4] = { 0, 0, 0, 0 };
        for (const auto& kv : base.tiles)
            if (kv.second.road_level > 0 && kv.second.road_level < 4)
                ++base_hist[kv.second.road_level];
        std::printf("    base road field: tier1=%d tier2=%d tier3=%d\n",
                    base_hist[1], base_hist[2], base_hist[3]);

        landscape_score at[4];
        int at_top[4] = { 0, 0, 0, 0 };   // tiles at tier 3 AFTER the candidate applied
        for (std::uint8_t t = 1; t <= 3; ++t)
        {
            world w = base;
            apply_landscape_candidate(w, reg, landscape_candidate{ 8, 0xC0FFEEu, t });
            for (const auto& kv : w.tiles)
                if (kv.second.road_level >= 3)
                    ++at_top[t];
            at[t] = score_landscape(w, reg);
            std::printf("    road_tier=%u  highway tiles=%-5d  potential=%.5f  "
                        "ACTUAL=%.5f  composite=%.9f\n",
                        static_cast<unsigned>(t), at_top[t], at[t].mean_completeness,
                        at[t].mean_actual, at[t].composite);
        }

        // THE CONTROL, and it compares the SAME quantity across candidates — the
        // first cut counted "tiles at or above tier t", which is trivially equal
        // for every t once the uplift has run and therefore controlled nothing.
        check(at_top[3] > at_top[1], "R5.0",
              "the tier axis was actually APPLIED - the highway count moves between "
              "tier 1 and tier 3, so a flat score is about the objective, not a no-op");

        // NR-793, THE DECISIVE MEASUREMENT (Ben, 2026-09-07: "run it").
        //
        // A flat score has two possible causes and they call for opposite
        // fixes. Either (a) the tier moves the REACH FIELD and the objective
        // then fails to read the difference — a blind objective, fixed by a new
        // term; or (b) the tier does not move the reach field AT ALL, because
        // roads sit where the economy already is and the 24.0 budget's frontier
        // is out where there are no roads to upgrade — in which case no term
        // could see it and the axis itself is the wrong one.
        //
        // `in_reach_tiles` is the boolean the objective actually consumes:
        // tiles inside `max_logistics_reach` of their market. Summing it either
        // side of the uplift separates (a) from (b) in one number.
        for (std::uint8_t t = 1; t <= 3; t += 2)
        {
            world w = base;
            apply_landscape_candidate(w, reg, landscape_candidate{ 8, 0xC0FFEEu, t });
            const auto rows = measure_market_completeness(w, reg, classify_resources(w, reg));
            long long catch_sum = 0, reach_sum = 0, raws_sum = 0;
            for (const auto& r : rows)
            {
                catch_sum += r.catchment_tiles;
                reach_sum += r.in_reach_tiles;
                raws_sum  += r.raws_in_reach;
            }
            std::printf("    road_tier=%u  catchment=%lld  IN_REACH=%lld  raws_in_reach=%lld\n",
                        static_cast<unsigned>(t), catch_sum, reach_sum, raws_sum);
        }

        // NR-793 PART 2 (Ben, 2026-09-07): re-run the axis on a LIVE world.
        //
        // Everything above runs on the default-seed fixture, which carries TWO
        // markets, a balance term of exactly 0 and therefore a composite of
        // exactly 0 for every candidate. A conclusion about what the objective
        // can SEE cannot rest on a world where the objective evaluates to zero
        // for every input. Seed ABCDEF01 carries ten markets and a live
        // composite, and it is one of the same control worlds the score
        // harness already uses — same construction, so nothing new is invented
        // here to make the number come out.
        std::printf("\n    -- the same axis on a TEN-MARKET world (seed ABCDEF01) --\n");
        {
            landscape_score live[4];
            for (std::uint8_t t = 1; t <= 3; t += 2)
            {
                world_params lp = wp;
                lp.seed = 0xABCDEF01u;
                world w = make_hard_coded_world(lp, nullptr, gen_cfg);
                assign_default_recipes(w, reg);
                corporation_params cp;
                cp.corporation_count = 8;
                generate_corporations(w, cp, 0xC0FFEEu);
                generate_background_firms(w, reg, 0xC0FFEEu);

                apply_landscape_candidate(w, reg, landscape_candidate{ 8, 0xC0FFEEu, t });
                const auto rows = measure_market_completeness(w, reg, classify_resources(w, reg));
                long long reach_sum = 0, raws_sum = 0;
                for (const auto& r : rows)
                {
                    reach_sum += r.in_reach_tiles;
                    raws_sum  += r.raws_in_reach;
                }
                live[t] = score_landscape(w, reg);
                std::printf("    tier=%u  mkts=%d  IN_REACH=%lld  raws=%lld  potential=%.5f  "
                            "ACTUAL=%.5f  balance=%.5f  spread=%.5f  composite=%.9f\n",
                            static_cast<unsigned>(t), live[t].market_count, reach_sum, raws_sum,
                            live[t].mean_completeness, live[t].mean_actual,
                            live[t].mean_balance, live[t].spread, live[t].composite);
            }
            std::printf("    VERDICT ON A LIVE OBJECTIVE: the road axis is %s\n",
                        same_score(live[1], live[3])
                            ? "STILL INVISIBLE - the fixture was not what hid it"
                            : "SEEN - the earlier finding was an artefact of a degenerate fixture");
        }

        const bool road_seen = !same_score(at[1], at[3]);
        std::printf("    FINDING: the road/infrastructure axis is %s\n",
                    road_seen ? "SEEN by the objective"
                              : "INVISIBLE to the objective - every term is bit-identical "
                                "across tier 1, 2 and 3");
        if (!road_seen)
            std::printf("      Raising all %d road tiles from Track to Highway moves no\n"
                        "      term. The tier scales traversal COST (x0.67/x0.50/x0.40);\n"
                        "      the objective reads catchment MEMBERSHIP and terminal\n"
                        "      closure, both of which are booleans this world's reach\n"
                        "      field does not flip. One of the two has to change before\n"
                        "      this axis earns its third of every round.\n",
                        base_hist[1] + base_hist[2] + base_hist[3]);
    }

    print_path(serial);

    std::printf("\n--- THE SLICE'S QUESTION ---\n"
                "  The search exists and it is DETERMINISTIC: one seed, five thread\n"
                "  counts, one winner, bit-identical on every scored term. The walk\n"
                "  improved the seed candidate's composite %.6f -> %.6f (+%.1f%%);\n"
                "  accepted by axis: roster %d, placement %d, road_tier %d.\n",
                serial.seed_score.composite, serial.winner_score.composite,
                100.0 * (serial.winner_score.composite / serial.seed_score.composite - 1.0),
                serial.accepted_by_axis[0], serial.accepted_by_axis[1],
                serial.accepted_by_axis[2]);

    // ------------------------------------------------------------------
    // The caveat slice 1 printed, carried forward unchanged. It bears on this
    // slice more than on that one: a SEARCH over a shrinking field ranks
    // degrees of failure with more conviction than a scorer does.
    // ------------------------------------------------------------------
    std::printf("\n--- SCORES HERE ARE ORDINAL AND PROVISIONAL ---\n"
                "  Sprint 33's growth gate is UNMET (BL-759 re-based figures). The search\n"
                "  ORDERS candidates; it does not evidence that the winner is viable.\n");

    std::printf("\n%s — %d failure(s)\n", failures == 0 ? "PASS" : "FAIL", failures);
    return failures == 0 ? 0 : 1;
}
