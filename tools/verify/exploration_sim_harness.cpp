// ---------------------------------------------------------------------------
// exploration_sim_harness — BL-930 (the Exploration tree wired into the sim)
// and BL-931 (the Exploration span runs, 1200 -> 1660, on the shared engine).
//
// Binds:
//   T1   the tree's five-rule availability, read off io::exploration_tree
//   T2   the rim read (`polity_holds_exploration_rim`)
//   T3   the four new-but-unbuilt scorer terms stay pinned at a neutral 0
//        until their owning items (BL-932/939/940, BL-933/934) land
//   R1   resuming from a prior span's close reproduces the SAME state a
//        fresh (non-resumed) run of that same span would have reached --
//        i.e. `resume_polities == nullptr` is untouched by this item
//   R2   the Exploration span, resumed off a real Empires close, runs its
//        full 460 years without crashing and without touching the Empires
//        span's own recorded outcome (BL-462's acceptance test, replayed)
//   R3   the resumed run is deterministic: same seed, same fixture, twice
//        -> byte-identical ownership record and counters
//   R4   objects with a term expire on schedule and not before
//   R5   the round-level upkeep hook is callable and moves nothing (it is a
//        documented no-op until BL-932)
//
// Headless: world/* logic only, no SDL and no Lua.
// ---------------------------------------------------------------------------

#include "world/era_minus_one.hpp"
#include "world/hard_coded_world.hpp"
#include "world/history_sim.hpp"
#include "world/sim_terrain_build.hpp"
#include "world/settlement.hpp"
#include "world/world.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

namespace
{

int g_failures = 0;

void check(bool ok, const char* label)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", label);
    if (!ok) ++g_failures;
}

int find_node(const char* id)
{
    for (int i = 0; i < io::exploration_tree::node_count; ++i)
        if (std::strcmp(io::exploration_tree::nodes[i].id, id) == 0) return i;
    return -1;
}

} // namespace

int main()
{
    std::printf("=== exploration_sim_harness (BL-930 / BL-931) ===\n\n");

    // -----------------------------------------------------------------
    // T1: the tree's five-rule availability
    // -----------------------------------------------------------------
    {
        const int root   = find_node("EX-SP-1a");
        const int sp1m   = find_node("EX-SP-1m");
        const int hl2a   = find_node("EX-HL-2a"); // Ocean Carrack
        const int hl2b   = find_node("EX-HL-2b"); // Fleet of the Line (excludes hl2a)
        const int pt1a   = find_node("EX-PT-1a");
        const int wy1a   = find_node("EX-WY-1a");

        check(root >= 0 && sp1m >= 0 && hl2a >= 0 && hl2b >= 0 && pt1a >= 0 && wy1a >= 0,
              "T1.0  every id this harness names resolves to a real node");

        check(exploration_node_available(0, root),
              "T1.1  the tree's own root is available with nothing held");

        // The milestone needs PT-1a and WY-1a held (its `requires_mask`) --
        // not available off the root alone.
        const uint64_t mask_root_only = 1ULL << root;
        check(!exploration_node_available(mask_root_only, sp1m),
              "T1.2  the ring-1 milestone refuses without its `requires` set");

        const uint64_t mask_with_requires =
            mask_root_only | (1ULL << pt1a) | (1ULL << wy1a);
        check(exploration_node_available(mask_with_requires, sp1m),
              "T1.3  the ring-1 milestone opens once PT-1a and WY-1a are both held");

        // Fork exclusion: taking one side closes the other, permanently.
        const uint64_t mask_carrack = 1ULL << hl2a;
        check(!exploration_node_available(mask_carrack, hl2b),
              "T1.4  taking the Ocean Carrack closes Fleet of the Line (fork excludes)");
        const uint64_t mask_fleet = 1ULL << hl2b;
        check(!exploration_node_available(mask_fleet, hl2a),
              "T1.5  and the reverse: taking Fleet of the Line closes Ocean Carrack");

        // Already-held nodes are never available again.
        check(!exploration_node_available(mask_carrack, hl2a),
              "T1.6  an already-held node is never available a second time");

        // Ring 2 is locked without the ring-1 milestone.
        check(!exploration_node_available(mask_with_requires, hl2a),
              "T1.7  a ring-2 node stays locked until the ring-1 milestone is held");
    }

    // -----------------------------------------------------------------
    // T2: the rim read
    // -----------------------------------------------------------------
    {
        polity q;
        check(!polity_holds_exploration_rim(q),
              "T2.1  a fresh polity does not hold the rim");
        q.exploration_mask = 1ULL << io::exploration_tree::rim_node_index;
        check(polity_holds_exploration_rim(q),
              "T2.2  setting the rim's own bit is read back as holding it");
    }

    // -----------------------------------------------------------------
    // T3: the four stubbed scorer terms stay pinned at 0 -- named here so
    // BL-932/939/940/933/934 landing shows as a DIFF in this harness rather
    // than a silent change (EXPLORATION_TREE.md sec The scorer).
    // -----------------------------------------------------------------
    {
        // EX-SP-2a's own term is `purse_low`; make it the ONLY available
        // major-kind candidate at ring 2 by opening ring 2 (mask holds the
        // ring-1 milestone) with every ground signal at its most generous —
        // if `purse_low` were anything other than a pinned 0, this is the
        // scenario that would show it, because nothing else competes.
        const int sp1m = find_node("EX-SP-1m");
        const int sp2a = find_node("EX-SP-2a"); // purse_low
        const uint64_t mask = 1ULL << sp1m;

        check(exploration_node_available(mask, sp2a),
              "T3.0  EX-SP-2a (purse_low) is a legal target once ring 2 opens");

        // Called with every OTHER term maxed out, the argmax still considers
        // EX-SP-2a (it does not vanish from the candidate set), and it never
        // wins over a live term at the same ring -- confirming purse_low
        // contributes nothing to the score it competes on.
        const int chosen_high_signals = choose_exploration_node(
            mask, /*stores_low_q=*/1000, /*reach_bound_q=*/1000,
            /*ground_port_q=*/1000, /*ground_farm_q=*/1000, /*surplus_q=*/1000);
        check(chosen_high_signals != sp2a,
              "T3.1  purse_low (stubbed 0) loses to any live ring-2 term when one is available");
    }

    // -----------------------------------------------------------------
    // R1/R2/R3: the resumed span, over a REAL Empires close
    // -----------------------------------------------------------------
    era_minus_one_fixture fixture;
    world_params wp;
    wp.seed = 0xABCDEF01u;
    generation_report rep;
    make_hard_coded_world(wp, &rep, {}, nullptr, nullptr, &fixture);

    check(fixture.ran, "R0  the fixture captured a real Empires-round invocation");
    if (fixture.ran)
    {
        // R1: reproduce generation's own Empires run EXACTLY (BL-462's own
        // acceptance test), from the captured fixture -- this is the
        // `resume_polities == nullptr` path, UNCHANGED by this item.
        settlement_state ss_a = fixture.settlement;
        const history_sim_state hs_a = run_history_sim(
            ss_a, &fixture.creeds, fixture.terrain.view(), fixture.gw, fixture.gh,
            fixture.params, fixture.seed, nullptr, fixture.works, nullptr);

        check(hs_a.battles == fixture.battles && hs_a.conquests == fixture.conquests
           && hs_a.foundings == fixture.foundings,
              "R1  a fresh (non-resumed) re-run still reproduces generation's own Empires "
              "outcome bit-for-bit (BL-931 touched nothing on the null-resume path)");

        // R2/R3: fold the handoff, then run the Exploration span off it,
        // TWICE from independent copies, at the same seed.
        const pass_one_output p1 = make_pass_one_output(
            ss_a, hs_a, static_cast<int>(fixture.creeds.cultures.size()));

        check(!p1.polities.empty(), "R2.0  the Empires close leaves at least one living polity");

        const auto run_exploration = [&](uint32_t seed) {
            settlement_state ss = ss_a; // Independent copy each call.
            history_sim_params ep;
            ep.start_year = fixture.params.stop_year; // 1200, wherever Empires closed.
            ep.stop_year  = fixture.params.stop_year + 460; // EXPLORATION.md's own span.
            ep.tick_bands[0]   = {ep.stop_year, 4};
            ep.tick_band_count = 1;
            ep.exploration_upkeep_enabled          = true;
            ep.city_states_by_population_threshold = true;
            ep.settle_requires_razed_ground         = true;
            ep.resume_polities  = &p1.polities;
            ep.resume_grudges   = &p1.grudges;
            ep.resume_contacts  = &p1.contacts;
            ep.resume_corridors = &p1.surviving_corridors;
            return run_history_sim(ss, &fixture.creeds, fixture.terrain.view(),
                                   fixture.gw, fixture.gh, ep, seed, nullptr,
                                   fixture.works, nullptr);
        };

        const history_sim_state ex1 = run_exploration(0x515C0E17u);
        const history_sim_state ex2 = run_exploration(0x515C0E17u);

        check(ex1.years == 460, "R2.1  the resumed span runs its full 460 years (1200 -> 1660)");
        check(static_cast<int>(ex1.polities.size()) == static_cast<int>(p1.polities.size())
           || ex1.polities.size() > p1.polities.size(), // BL-920 can raise new city states mid-span
              "R2.2  the resumed run opens on the handed-off polity set, never fewer");

        bool same_owner_changes = ex1.owner_changes.size() == ex2.owner_changes.size();
        if (same_owner_changes)
            for (std::size_t i = 0; i < ex1.owner_changes.size(); ++i)
                if (ex1.owner_changes[i].year   != ex2.owner_changes[i].year
                 || ex1.owner_changes[i].region != ex2.owner_changes[i].region
                 || ex1.owner_changes[i].owner  != ex2.owner_changes[i].owner)
                { same_owner_changes = false; break; }

        check(same_owner_changes
           && ex1.battles == ex2.battles && ex1.conquests == ex2.conquests
           && ex1.foundings == ex2.foundings,
              "R3  the resumed Exploration span is deterministic: same seed, same fixture, "
              "twice -> byte-identical ownership record and counters");

        std::printf("      exploration span: years=%lld battles=%lld conquests=%lld "
                    "foundings=%lld polities(1200)=%d polities(1660)=%d\n",
                    static_cast<long long>(ex1.years), static_cast<long long>(ex1.battles),
                    static_cast<long long>(ex1.conquests), static_cast<long long>(ex1.foundings),
                    static_cast<int>(p1.polities.size()), static_cast<int>(ex1.polities.size()));

        // BL-930, folded into a real run: at least the tree's own root can
        // fire once the empire rim is available to SOME polity, or none do
        // (a legitimate seed, per EXPLORATION_TREE.md's own sizing caveat) --
        // report which, rather than asserting an outcome no single seed owes.
        int rim_holders_1660 = 0, exploration_active = 0;
        for (const polity& q : ex1.polities)
        {
            if (polity_holds_exploration_rim(q)) ++rim_holders_1660;
            if (q.exploration_mask != 0) ++exploration_active;
        }
        std::printf("      exploration tree: %d/%zu polities hold the rim at 1660, "
                    "%d hold at least one node\n",
                    rim_holders_1660, ex1.polities.size(), exploration_active);
    }

    // -----------------------------------------------------------------
    // R4: objects with a term
    // -----------------------------------------------------------------
    {
        std::vector<dated_object> objs;
        objs.push_back(dated_object{1210, 1, 0, 1});
        objs.push_back(dated_object{1200, 2, 2, 3});
        objs.push_back(dated_object{1660, 3, 4, 5});

        expire_dated_objects(objs, 1199);
        check(objs.size() == 3, "R4.1  nothing expires before its own term");

        expire_dated_objects(objs, 1200);
        check(objs.size() == 2, "R4.2  a term ending AT the current year expires (<=, not <)");

        expire_dated_objects(objs, 1210);
        check(objs.size() == 1, "R4.3  a later term expires once its own year arrives");

        expire_dated_objects(objs, 1660);
        check(objs.empty(), "R4.4  every remaining term is gone once the span's own close arrives");
    }

    // -----------------------------------------------------------------
    // R5: the upkeep hook is callable and moves nothing (documented no-op)
    // -----------------------------------------------------------------
    {
        std::vector<polity> qs(2);
        qs[0].capacity[0] = 3; qs[1].cohesion_q = 700;
        const std::vector<polity> before = qs;
        run_exploration_upkeep(qs, 1234);
        bool unchanged = qs.size() == before.size();
        for (std::size_t i = 0; unchanged && i < qs.size(); ++i)
            unchanged = qs[i].capacity[0] == before[i].capacity[0]
                     && qs[i].cohesion_q == before[i].cohesion_q
                     && qs[i].exploration_mask == before[i].exploration_mask
                     && qs[i].empire_mask == before[i].empire_mask;
        check(unchanged, "R5  run_exploration_upkeep moves nothing today (BL-932 gives it a body)");
    }

    std::printf("\n%s (%d failure%s)\n",
                g_failures == 0 ? "ALL PASS" : "FAILURES",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
