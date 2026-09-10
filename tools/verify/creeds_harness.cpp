// ---------------------------------------------------------------------------
// Headless creeds harness (BL-235; no SDL / Lua / ImGui)
// ---------------------------------------------------------------------------
// Exercises docs/lore/CREEDS.md as implemented in src/world/creeds.cpp, over
// the real generated world plus synthetic states.
//
//   C1  DETERMINISM. Creeds are a pure function of (seed, tiles): the same
//       world generated twice yields identical shrine, war, and tongue lines.
//
//   C2  ONE PANTHEON PER CULTURE. Every cradle raises exactly one shrine
//       line, each names a different god (a different tongue coined it), and
//       every creed line carries its consequence (PLANETOLOGY.md §
//       Presentation, non-negotiable).
//
//   C3  FRAGMENTATION FROM CONTACT (BL-852, resolving NR-808). Heavy contact
//       (a high average region_mix_q) pulls fragmentation_q DOWN; zero
//       contact leaves it untouched; and no amount of contact can push it
//       below half the STRUCTURAL value it started from, so creeds alone
//       still cannot manufacture a hegemon by themselves (BL-224).
//
//   C4  GLOBALISATION CLOSES THE STORY. Exactly one common-tongue line, dated
//       1951 (nine years before the epoch), after every shrine and war line.
//
// HONEST SCOPE NOTE: culture regions (BL-218) are not built; the culture unit
// here is the CRADLE, which BL-218's clustering will refine rather than
// replace. A green run means the creeds are self-consistent and
// deterministic — not that the pantheon shapes are calibrated.
//
// The process exits non-zero if any assertion FAILs.

#include "world/components.hpp"
#include "world/creeds.hpp"
#include "world/hard_coded_world.hpp"
#include "harness_params.hpp"
#include "world/history_ladder.hpp"
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

bool contains(const std::string& s, const char* needle)
{
    return s.find(needle) != std::string::npos;
}

history_ladder_state synthetic_ladder(int fragmentation_q, int conquest_q, int cradles)
{
    history_ladder_state h;
    h.fragmentation_q = fragmentation_q;
    h.conquest_cost_q = conquest_q;
    for (int i = 0; i < cradles; ++i)
        h.cradles.push_back(agrarian_cradle{ i * 30, 40, 300, 0, "a reach" });
    return h;
}

} // namespace

int main()
{
    std::printf("=== creeds (BL-235) ===\n\n");

    // --- C3 fragmentation from contact ---------------------------------------
    // Asserted on synthetic ladders first, so the claim holds for any settled
    // map, not just the one Kepler happens to grow.
    {
        // Heavy contact everywhere: every region's nearest neighbour is a
        // different people (region_mix_q == 1000 throughout). The average is
        // 1000, which would zero fragmentation outright if unbounded - the
        // floor is the point of this case.
        history_ladder_state hl = synthetic_ladder(800, 0, 2);
        record_cultural_contact(hl, std::vector<int>(6, 1000));
        check(hl.fragmentation_q < 800,
              "C3 heavy contact pulls fragmentation down");
        check(hl.fragmentation_q == 400,
              "C3 contact is floored at half the STRUCTURAL fragmentation (BL-224)");

        // Zero contact - every region's nearest neighbour is its own people -
        // leaves the ladder exactly where Stage 3 (terrain + cradle count)
        // set it. No roll, no war: this must be exact, not merely close.
        history_ladder_state hl2 = synthetic_ladder(800, 0, 2);
        record_cultural_contact(hl2, std::vector<int>(6, 0));
        check(hl2.fragmentation_q == 800,
              "C3 zero contact leaves fragmentation untouched");

        // Partial contact lands between the two: average mix 300 over a
        // structural 800 subtracts exactly 300, well clear of the floor.
        history_ladder_state hl3 = synthetic_ladder(800, 0, 2);
        record_cultural_contact(hl3, std::vector<int>{ 300, 300, 300, 300 });
        check(hl3.fragmentation_q == 500,
              "C3 partial contact subtracts its own average, not a fixed step");

        // No settled regions at all - nothing to read contact off - is a
        // no-op, not a divide-by-zero.
        history_ladder_state hl4 = synthetic_ladder(800, 0, 2);
        record_cultural_contact(hl4, {});
        check(hl4.fragmentation_q == 800,
              "C3 an unsettled map leaves fragmentation untouched");
    }

    // --- Generate the real world twice --------------------------------------
    world_params wp;
    wp.seed = 0xB235u;
    // The ladder this harness reads runs to 1960 (industrialisation, ruptures,
    // the common tongue). The DEFAULT epoch became 0 CE with the ancient refocus
    // (NR-177), which stops the settlement pass before any of it happens - so ask
    // for the era under test rather than inheriting whatever the campaign wants.
    wp.epoch_year = 1960;
    generation_report r1, r2;
    const world w1 = make_hard_coded_world(no_prehistory(wp), &r1);
    const world w2 = make_hard_coded_world(no_prehistory(wp), &r2);

    const generation_report::body_entry* k1 = nullptr;
    const generation_report::body_entry* k2 = nullptr;
    // The home body is identified by its flag, never by its (generated) name - BL-257.
    for (const auto& b : r1.bodies) if (b.is_homeworld) k1 = &b;
    for (const auto& b : r2.bodies) if (b.is_homeworld) k2 = &b;

    check(k1 && k2, "C1 Kepler reports exist in both runs");
    if (!k1 || !k2) { std::printf("\n%d passed, %d FAILED\n", g_pass, g_fail); return 1; }

    // --- C1 determinism ------------------------------------------------------
    {
        bool same = k1->state.history.size() == k2->state.history.size();
        for (std::size_t i = 0; same && i < k1->state.history.size(); ++i)
            same = k1->state.history[i].event == k2->state.history[i].event
                && k1->state.history[i].years_before_epoch == k2->state.history[i].years_before_epoch;
        check(same, "C1 two runs of the same seed write the identical biography");
    }

    // --- C2 one pantheon per culture ----------------------------------------
    {
        std::vector<const history_event*> shrines;
        std::vector<const history_event*> tongue;
        for (const history_event& e : k1->state.history)
        {
            if (contains(e.event, "raise the first shrine")) shrines.push_back(&e);
            if (contains(e.event, "common tongue"))          tongue.push_back(&e);
        }

        check(!shrines.empty(), "C2 a world with cradles raises shrines");

        // Each shrine names a different god - each tongue coined its own.
        std::vector<std::string> gods;
        for (const history_event* e : shrines)
        {
            const std::size_t at = e->event.find("shrine of ");
            const std::size_t to = e->event.find(",", at);
            if (at != std::string::npos && to != std::string::npos)
                gods.push_back(e->event.substr(at + 10, to - at - 10));
        }
        std::sort(gods.begin(), gods.end());
        check(std::adjacent_find(gods.begin(), gods.end()) == gods.end(),
              "C2 every culture's chief god carries a distinct native name");

        bool consequences = true;
        for (const history_event& e : k1->state.history)
            if (contains(e.event, "shrine") || contains(e.event, "war-bands") ||
                contains(e.event, "common tongue"))
                consequences = consequences && !e.consequence.empty()
                            && e.consequence.rfind("->", 0) == 0;
        check(consequences, "C2 every creed line carries its consequence");

        // --- C4 globalisation ------------------------------------------------
        check(tongue.size() == 1, "C4 exactly one common-tongue line");
        if (tongue.size() == 1)
        {
            check(tongue[0]->years_before_epoch == 9,
                  "C4 the common tongue is dated 1951 - nine years before the epoch");
            bool latest = true;
            for (const history_event* e : shrines)
                latest = latest && e->years_before_epoch > tongue[0]->years_before_epoch;
            check(latest, "C4 globalisation postdates every shrine");
        }
    }

    // The seed's actual creed record, for the human reading the run — the
    // sweep readout is these lines' only window (BL-219's argument).
    std::printf("\n--- seed 0xB235's creed record ---\n");
    for (const history_event& e : k1->state.history)
        if (contains(e.event, "shrine") || contains(e.event, "war-bands") ||
            contains(e.event, "common tongue"))
            std::printf("  %s  %s\n      %s\n",
                        format_history_date(e.years_before_epoch).c_str(),
                        e.event.c_str(), e.consequence.c_str());

    std::printf("\n%d passed, %d FAILED\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
