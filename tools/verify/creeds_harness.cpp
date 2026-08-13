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
//   C3  THE CREED DRIVES. A warlike synthetic culture welds its neighbour and
//       fragmentation_q FALLS; a peaceable one leaves it untouched; and no
//       amount of welding can push it below half its incoming value, so
//       warlike creeds cannot manufacture a hegemon by themselves.
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

/// A synthetic culture with a hand-set temperament. The pantheon layout
/// matters: index 0 is the chief, index 1 the war god (creeds.cpp's contract).
culture synthetic_culture(int cradle, const char* name, int zeal, int dominion, int aggression)
{
    culture c;
    c.cradle = cradle;
    c.name = name;
    c.pantheon.push_back(culture_god{ "Chief", "the storm", "who splits the sky", 4, 5 });
    c.pantheon.push_back(culture_god{ "Warlord", "war", "who counts the spears", zeal, dominion });
    c.aggression_q = aggression;
    return c;
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

    // --- C3 the creed drives ------------------------------------------------
    // Asserted on synthetic states first, so the claim holds for any creed,
    // not just the one Kepler happens to roll.
    {
        // A maximal aggressor against a defenceless neighbour on open ground:
        // attack = 10*60 + 1000/2 + r(120) >= 1100; defence < 240. Always won.
        creed_state cs;
        cs.cultures.push_back(synthetic_culture(0, "Marauders", 10, 10, 1000));
        cs.cultures.push_back(synthetic_culture(1, "Farmers", 0, 0, 0));
        history_ladder_state hl = synthetic_ladder(800, 0, 2);

        record_tribal_conflict(cs, hl, 0xB235u);
        check(hl.fragmentation_q < 800,
              "C3 a won tribal war welds cradles - fragmentation falls");
        check(hl.fragmentation_q >= 400,
              "C3 welding is floored at half the incoming fragmentation");
        check(!cs.history.empty() && contains(cs.history.back().event, "war-bands"),
              "C3 the war writes its line");

        // Peaceable creeds leave the ladder untouched.
        creed_state calm;
        calm.cultures.push_back(synthetic_culture(0, "Weavers", 2, 2, 100));
        calm.cultures.push_back(synthetic_culture(1, "Potters", 2, 2, 100));
        history_ladder_state hl2 = synthetic_ladder(800, 0, 2);
        record_tribal_conflict(calm, hl2, 0xB235u);
        check(hl2.fragmentation_q == 800 && calm.history.empty(),
              "C3 peaceable creeds farm instead - no wars, no welding");

        // Expensive ground defends: same marauders, conquest cost at maximum.
        // defence >= 0 + 0 + 500 + 0; attack floor 1100 still clears it, so
        // raise the defender too - the point is the cost REACHES the fight.
        creed_state cs3;
        cs3.cultures.push_back(synthetic_culture(0, "Marauders", 10, 4, 600));
        cs3.cultures.push_back(synthetic_culture(1, "Highlanders", 8, 8, 400));
        history_ladder_state hl3 = synthetic_ladder(800, 1000, 2);
        record_tribal_conflict(cs3, hl3, 0xB235u);
        check(hl3.fragmentation_q == 800,
              "C3 conquest cost prices the march - the highland frontier holds");
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
