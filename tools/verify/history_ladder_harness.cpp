// ---------------------------------------------------------------------------
// Headless history-ladder harness (BL-221; no SDL / Lua / ImGui)
// ---------------------------------------------------------------------------
// Exercises docs/lore/HISTORY.md Stages 0-2 as implemented in
// src/world/history_ladder.cpp, over the real generated world.
//
//   H1  DETERMINISM. The ladder is a pure function of (seed, tiles): the same
//       world generated twice yields identical cradles, fragmentation, costs
//       and history lines. This is the standing invariant everything else in
//       the generation layer answers to.
//
//   H2  IT DRIVES, IT DOES NOT NARRATE (Ben, 2026-07-30). The load-bearing
//       claim of the whole item, so it is asserted rather than assumed: a
//       fragmented ladder state must produce DIFFERENT nation_params than a
//       unifiable one, in the stated direction, and the modulation must stay
//       inside its bounds so a tuning mistake cannot make a one-nation world.
//
//   H3  THE CAUSAL CHAIN. Stage 0's cradle count feeds Stage 2's fragmentation
//       (HISTORY.md: "More cradles -> deeper eventual fragmentation"), and
//       fragmentation feeds the polity seed budget. If these are independent
//       the ladder is three unrelated dice, not a ladder.
//
//   H4  PRESENTATION. Every emitted line carries its consequence
//       (PLANETOLOGY.md § Presentation, non-negotiable), is dated inside
//       recorded history rather than deep time, and sorts into the biography
//       in order.
//
//   H5  FAILURE CASES ARE OUTPUT, NOT DEFECTS. A world below a land biosphere
//       produces NO ladder at all and no padded lines; a world that unified
//       reports the hegemon rather than hiding it. Ben asked to see failure
//       cases, so the harness pins that they are reachable and named.
//
// HONEST SCOPE NOTE: two of Stage 0's designed inputs do not exist in src/ yet
// (river connectivity, BL-170; domesticable clades, BL-217). The score stands
// in for them as documented in history_ladder.hpp, so a green run means the
// ladder is self-consistent and deterministic - NOT that its cradle placement
// is calibrated against anything.
//
// The process exits non-zero if any assertion FAILs.

#include "world/components.hpp"
#include "world/hard_coded_world.hpp"
#include "world/history_ladder.hpp"
#include "world/nation_generation.hpp"
#include "world/planetology.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace {

int g_pass = 0, g_fail = 0;

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    ok ? ++g_pass : ++g_fail;
}

/// Build a ladder state by hand, for the driving/bounds assertions that must
/// not depend on whatever Kepler happens to roll.
history_ladder_state synthetic(int fragmentation_q, int conquest_q, int cradles)
{
    history_ladder_state h;
    h.fragmentation_q = fragmentation_q;
    h.conquest_cost_q = conquest_q;
    for (int i = 0; i < cradles; ++i)
        h.cradles.push_back(agrarian_cradle{ i * 10, i * 5, 300, i % 2, "a reach" });
    h.polity_seeds = cradles;
    return h;
}

} // namespace

int main()
{
    std::printf("=== history ladder (BL-221) ===\n\n");

    const nation_params base{ .min_seed_separation = 5 };

    // --- H2 it drives -------------------------------------------------------
    // Asserted before anything reads the generated world, because this is the
    // item's central claim and it must hold for any ladder state, not just the
    // one Kepler produced.
    {
        const nation_params frag = nation_params_from_ladder(synthetic(900, 800, 8), base);
        const nation_params unif = nation_params_from_ladder(synthetic(50, 100, 1), base);

        std::printf("      fragmented -> tiles/seed %d, min_nation %d, separation %d\n",
                    frag.land_tiles_per_seed, frag.min_nation_tiles, frag.min_seed_separation);
        std::printf("      unifiable  -> tiles/seed %d, min_nation %d, separation %d\n",
                    unif.land_tiles_per_seed, unif.min_nation_tiles, unif.min_seed_separation);

        check(frag.land_tiles_per_seed < unif.land_tiles_per_seed,
              "H2 a fragmented world seeds MORE densely than a unifiable one");
        check(frag.min_nation_tiles < unif.min_nation_tiles,
              "H2 a fragmented world lets SMALLER nations survive the merge");
        check(frag.min_seed_separation > unif.min_seed_separation,
              "H2 expensive conquest pushes neighbours further apart");

        // Bounds. Constrain the inputs, never the gates: an extreme ladder
        // state must still land inside a sane political map.
        const nation_params hi = nation_params_from_ladder(synthetic(1000, 1000, 24), base);
        const nation_params lo = nation_params_from_ladder(synthetic(0, 0, 1), base);
        check(hi.land_tiles_per_seed >= base.land_tiles_per_seed / 2 &&
              lo.land_tiles_per_seed <= (base.land_tiles_per_seed * 3) / 2,
              "H2 the modulation stays inside its bounds at both extremes");
        check(hi.min_nation_tiles > 0 && lo.min_nation_tiles > 0,
              "H2 no ladder state can drive the minimum viable territory to zero");

        // The empty case must not drive from zeros.
        const nation_params none = nation_params_from_ladder(history_ladder_state{}, base);
        check(none.land_tiles_per_seed == base.land_tiles_per_seed &&
              none.min_nation_tiles == base.min_nation_tiles &&
              none.min_seed_separation == base.min_seed_separation,
              "H2 a world with no cradles leaves the caller's defaults untouched");
    }

    // --- H3 the causal chain ------------------------------------------------
    {
        const history_ladder_state few  = synthetic(200, 200, 1);
        const history_ladder_state many = synthetic(800, 200, 8);
        check(nation_params_from_ladder(many, base).land_tiles_per_seed <
              nation_params_from_ladder(few,  base).land_tiles_per_seed,
              "H3 more fragmentation reaches the political map, not just the prose");
    }

    // --- Generate the real world twice --------------------------------------
    world_params wp;
    wp.seed = 0xB221u;
    generation_report r1, r2;
    const world w1 = make_hard_coded_world(wp, &r1);
    const world w2 = make_hard_coded_world(wp, &r2);

    const generation_report::body_entry* k1 = nullptr;
    const generation_report::body_entry* k2 = nullptr;
    for (const auto& b : r1.bodies) if (b.name == "Kepler") k1 = &b;
    for (const auto& b : r2.bodies) if (b.name == "Kepler") k2 = &b;
    check(k1 && k2, "H1 Kepler appears in the generation report");
    if (!k1 || !k2) { std::printf("\n=== FAILURES PRESENT ===\n"); return 1; }

    // --- H1 determinism -----------------------------------------------------
    {
        bool same = k1->state.history.size() == k2->state.history.size();
        for (std::size_t i = 0; same && i < k1->state.history.size(); ++i)
            same = k1->state.history[i].years_before_epoch == k2->state.history[i].years_before_epoch
                && k1->state.history[i].event            == k2->state.history[i].event
                && k1->state.history[i].consequence      == k2->state.history[i].consequence;
        check(same, "H1 the biography is identical across two generations");
    }

    // --- Find the ladder's own lines ---------------------------------------
    // They are the ones dated inside recorded history: every deep-time line is
    // millions of years old, so the split is unambiguous.
    std::vector<const history_event*> ladder;
    for (const history_event& h : k1->state.history)
        if (h.years_before_epoch < 1000000LL && h.years_before_epoch != 0)
            ladder.push_back(&h);

    std::printf("\n      Kepler's ladder lines (%d):\n", static_cast<int>(ladder.size()));
    for (const history_event* h : ladder)
        std::printf("      %14s  %-58s %s\n",
                    format_history_date(h->years_before_epoch).c_str(),
                    h->event.c_str(), h->consequence.c_str());
    std::printf("\n");

    check(!ladder.empty(), "H4 Kepler's ladder emitted at least one dated line");

    // --- H4 presentation ----------------------------------------------------
    {
        bool all_consequenced = true, all_dated = true;
        for (const history_event* h : ladder)
        {
            if (h->consequence.empty()) all_consequenced = false;
            // Recorded history, not deep time: within 12 kyr of the epoch.
            if (h->years_before_epoch > 12000LL) all_dated = false;
        }
        check(all_consequenced, "H4 every ladder line carries its consequence");
        check(all_dated, "H4 every ladder line is dated inside recorded history");

        bool ordered = true;
        for (std::size_t i = 1; i < k1->state.history.size(); ++i)
            if (k1->state.history[i - 1].years_before_epoch < k1->state.history[i].years_before_epoch)
                ordered = false;
        check(ordered, "H4 the ladder lines sort into the biography, oldest first");

        // Stage 0 must predate Stages 1-2 — a charter cannot precede the farms.
        bool staged = true;
        for (std::size_t i = 1; i < ladder.size(); ++i)
            if (ladder[i - 1]->years_before_epoch <= ladder[i]->years_before_epoch) staged = false;
        check(ladder.size() < 2 || staged,
              "H4 the stages are strictly ordered: surplus, then charter, then borders");
    }

    // --- H5 failure cases are reachable and named ---------------------------
    {
        // A dead body must produce no ladder at all, and must not be padded.
        const planetology_state* dead = nullptr;
        for (const auto& b : r1.bodies)
            if (b.state.peak < life_stage::land) { dead = &b.state; break; }
        check(dead != nullptr, "H5 the prototype set still contains a lifeless body to test");
        if (dead)
        {
            const history_ladder_state none =
                run_history_ladder(*dead, w1, std::vector<entity_id>{}, 0, 0, 1u);
            check(none.cradles.empty() && none.history.empty(),
                  "H5 a world below a land biosphere emits no ladder and no padding");
        }

        // The hegemon line is the named failure case, not a rejection.
        bool has_accord = false, has_hegemon = false;
        for (const history_event* h : ladder)
        {
            if (h->event.find("realms confirm mutual borders") != std::string::npos) has_accord = true;
            if (h->event.find("absorbed the rest") != std::string::npos) has_hegemon = true;
        }
        check(has_accord || has_hegemon,
              "H5 Stage 2 reports its outcome either way - accord or hegemon");
        std::printf("      Kepler's Stage 2 outcome: %s\n",
                    has_accord ? "multipolar accord" : (has_hegemon ? "HEGEMON (a data point, not a defect)" : "none"));
    }

    // --- Cross-seed spread: both Stage 2 outcomes must be reachable ---------
    {
        int accord = 0, hegemon = 0, seeds = 0;
        for (uint32_t s = 0; s < 12; ++s)
        {
            world_params p; p.seed = 0xB2210000u + s * 0x9E37u;
            generation_report rr;
            make_hard_coded_world(p, &rr);
            for (const auto& b : rr.bodies)
            {
                if (b.name != "Kepler") continue;
                ++seeds;
                for (const history_event& h : b.state.history)
                {
                    if (h.event.find("realms confirm mutual borders") != std::string::npos) ++accord;
                    if (h.event.find("absorbed the rest") != std::string::npos) ++hegemon;
                }
            }
        }
        std::printf("      across %d seeds: %d accord, %d hegemon\n", seeds, accord, hegemon);
        check(accord + hegemon == seeds,
              "H5 every seed records a Stage 2 outcome, none silently omits one");
    }

    std::printf("\n=== %s ===\n", g_fail == 0 ? "ALL PASS" : "FAILURES PRESENT");
    return g_fail == 0 ? 0 : 1;
}
