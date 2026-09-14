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
//   R6   BL-956: the Exploration handoff (`exploration_output`) passes its
//        validator on a real world, fails it when corrupted, and is what
//        world setup seeds sentiment and stamps roads from
//
// Headless: world/* logic only, no SDL and no Lua.
// ---------------------------------------------------------------------------

#include "world/era_minus_one.hpp"
#include "world/hard_coded_world.hpp"
#include "world/history_sim.hpp"
#include "world/sim_terrain_build.hpp"
#include "world/settlement.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
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

        // Called with every OTHER term maxed out and purse_low itself EMPTY
        // (treasury full, so purse_low_q == 0), the argmax still considers
        // EX-SP-2a (it does not vanish from the candidate set), and it never
        // wins over a live term at the same ring -- confirming an EMPTY
        // purse_low loses fairly rather than by being wired wrong.
        const int chosen_high_signals = choose_exploration_node(
            mask, /*stores_low_q=*/1000, /*reach_bound_q=*/1000,
            /*ground_port_q=*/1000, /*ground_farm_q=*/1000, /*surplus_q=*/1000,
            /*purse_low_q=*/0, /*wants_unmet_q=*/0, /*throughput_bound_q=*/0);
        check(chosen_high_signals != sp2a,
              "T3.1  purse_low at 0 (full treasury) loses to any live ring-2 term when one is "
              "available");

        // BL-932: purse_low IS live now -- an EMPTY treasury (purse_low_q at
        // its own max) DOES win EX-SP-2a when nothing else competes at all.
        const int chosen_empty_purse = choose_exploration_node(
            mask, /*stores_low_q=*/0, /*reach_bound_q=*/0,
            /*ground_port_q=*/0, /*ground_farm_q=*/0, /*surplus_q=*/0,
            /*purse_low_q=*/1000, /*wants_unmet_q=*/0, /*throughput_bound_q=*/0);
        check(chosen_empty_purse == sp2a,
              "T3.2  purse_low at 1000 (an empty treasury) wins EX-SP-2a when nothing else "
              "competes -- BL-932 wired it live");
    }

    // -----------------------------------------------------------------
    // T4: BL-939 -- wants_unmet, live at EX-GD-2a (Quayside Market)
    // -----------------------------------------------------------------
    {
        const int gd1a = find_node("EX-GD-1a");
        const int gd2a = find_node("EX-GD-2a"); // wants_unmet
        const int sp1m = find_node("EX-SP-1m");
        const uint64_t mask = (1ULL << sp1m) | (1ULL << gd1a);

        check(exploration_node_available(mask, gd2a),
              "T4.0  EX-GD-2a (wants_unmet) is a legal target once its ring-1 neighbour is held");

        const int chosen_no_want = choose_exploration_node(
            mask, /*stores_low_q=*/0, /*reach_bound_q=*/0,
            /*ground_port_q=*/0, /*ground_farm_q=*/0, /*surplus_q=*/0,
            /*purse_low_q=*/0, /*wants_unmet_q=*/0, /*throughput_bound_q=*/0);
        check(chosen_no_want != gd2a,
              "T4.1  wants_unmet at 0 (every want already met) does not single out EX-GD-2a");

        const int chosen_high_want = choose_exploration_node(
            mask, /*stores_low_q=*/0, /*reach_bound_q=*/0,
            /*ground_port_q=*/0, /*ground_farm_q=*/0, /*surplus_q=*/0,
            /*purse_low_q=*/0, /*wants_unmet_q=*/1000, /*throughput_bound_q=*/0);
        check(chosen_high_want == gd2a,
              "T4.2  wants_unmet at 1000 wins EX-GD-2a when nothing else competes -- BL-939 "
              "wired it live");
    }

    // -----------------------------------------------------------------
    // T4b: BL-940 -- throughput_bound, live at EX-WY-2a
    // -----------------------------------------------------------------
    {
        const int wy1a = find_node("EX-WY-1a");
        const int wy2a = find_node("EX-WY-2a"); // throughput_bound
        const int sp1m = find_node("EX-SP-1m");
        const uint64_t mask = (1ULL << sp1m) | (1ULL << wy1a);

        check(exploration_node_available(mask, wy2a),
              "T4b.0  EX-WY-2a (throughput_bound) is a legal target once Post Roads is held");

        const int chosen_unbound = choose_exploration_node(
            mask, /*stores_low_q=*/0, /*reach_bound_q=*/0,
            /*ground_port_q=*/0, /*ground_farm_q=*/0, /*surplus_q=*/0,
            /*purse_low_q=*/0, /*wants_unmet_q=*/0, /*throughput_bound_q=*/0);
        check(chosen_unbound != wy2a,
              "T4b.1  throughput_bound at 0 (a free-flowing network) does not single out EX-WY-2a");

        const int chosen_bound = choose_exploration_node(
            mask, /*stores_low_q=*/0, /*reach_bound_q=*/0,
            /*ground_port_q=*/0, /*ground_farm_q=*/0, /*surplus_q=*/0,
            /*purse_low_q=*/0, /*wants_unmet_q=*/0, /*throughput_bound_q=*/1000);
        check(chosen_bound == wy2a,
              "T4b.2  throughput_bound at 1000 wins EX-WY-2a when nothing else competes -- "
              "BL-940 wired it live");
    }

    // -----------------------------------------------------------------
    // T5: BL-939 -- the scarcity signal itself (refresh + the contact gate)
    // -----------------------------------------------------------------
    {
        std::vector<region> regions(2);
        regions[0].nation = 0; regions[0].has_market = true;
        regions[0].dominant = region_class::none; // wants everything, dominates nothing
        regions[1].nation = 1; regions[1].dominant = region_class::farm; // no market

        std::vector<polity> qs(2);
        qs[0].id = 0; qs[0].alive = true;
        qs[1].id = 1; qs[1].alive = true;

        refresh_market_scarcity(regions, qs);
        check(regions[0].scarcity_q[scarcity_good_index(region_class::farm)] > 0,
              "T5.1  a market on ground its own polity holds nowhere carries a live scarcity signal");
        check(regions[1].scarcity_q[0] == 0 && regions[1].scarcity_q[1] == 0
           && regions[1].scarcity_q[2] == 0 && regions[1].scarcity_q[3] == 0,
              "T5.2  a region with no market carries no scarcity signal at all");

        history_sim_state s;
        check(market_scarcity_q(regions, s, /*viewer=*/0, /*market_region=*/0, region_class::farm)
                  == regions[0].scarcity_q[scarcity_good_index(region_class::farm)],
              "T5.3  a polity reads its OWN market's signal unconditionally");
        check(market_scarcity_q(regions, s, /*viewer=*/1, /*market_region=*/0, region_class::farm) == 0,
              "T5.4  a polity that has never met the market's holder reads 0 (the omniscience guard)");

        s.contacts.push_back(contact{1, 0}); // viewer 1 reading holder 0's market
        std::sort(s.contacts.begin(), s.contacts.end(),
                  [](const contact& a, const contact& b) {
                      return a.from != b.from ? a.from < b.from : a.to < b.to;
                  });
        check(market_scarcity_q(regions, s, /*viewer=*/1, /*market_region=*/0, region_class::farm)
                  == regions[0].scarcity_q[scarcity_good_index(region_class::farm)],
              "T5.5  once contact is recorded, a foreign viewer reads the same signal the holder does");
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

        // BL-940 -- REPORTED, not gated: whether any polity in THIS seed ever
        // held EX-WY-1a and had a treasury to spend is a fact about the seed,
        // same discipline `unsustained_attrition_events` and the naval
        // counters already hold to elsewhere in this codebase.
        std::printf("      exploration road ladder: post_roads_built=%lld  "
                    "treasury_spent_on_roads=%lld\n",
                    static_cast<long long>(ex1.post_roads_built),
                    static_cast<long long>(ex1.treasury_spent_on_roads));

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
    // R6: BL-956 -- the Exploration handoff is a validated value, and
    // world setup reads 1660 grudges and corridors from it
    // -----------------------------------------------------------------
    check(fixture.exploration_ran,
          "R6.0  default params run the Exploration span during generation");
    if (fixture.exploration_ran)
    {
        const exploration_output& eo = fixture.exploration_handoff;
        std::string why;
        const bool valid = exploration_output_valid(eo, &why);
        if (!valid) std::printf("      exploration_output_valid: %s\n", why.c_str());
        check(valid, "R6.1  the real generated world's exploration_output passes its validator");
        check(eo.start_year == fixture.exploration_params.start_year
           && eo.stop_year == fixture.exploration_params.stop_year,
              "R6.2  the handoff's span is the span generation actually ran");

        // R6.3-R6.5: the validator is not a rubber stamp -- a deliberately
        // corrupted copy of the same value fails it.
        if (!eo.polities.empty())
        {
            exploration_output bad = eo;
            bad.polities[0].overlord = static_cast<int32_t>(bad.polities.size()) + 7;
            bad.polities[0].subject_kind = 1;
            check(!exploration_output_valid(bad, nullptr),
                  "R6.3  an out-of-range overlord fails the validator");

            exploration_output self_lord = eo;
            self_lord.polities[0].overlord = 0;
            self_lord.polities[0].subject_kind = 1;
            check(!exploration_output_valid(self_lord, nullptr),
                  "R6.4  a polity that is its own overlord fails the validator");
        }
        if (!eo.holdings.empty() && !eo.holdings[0].regions.empty())
        {
            exploration_output bad = eo;
            const int r = bad.holdings[0].regions[0];
            bad.regions[static_cast<std::size_t>(r)].nation = bad.holdings[0].polity == 0 ? 1 : 0;
            check(!exploration_output_valid(bad, nullptr),
                  "R6.5  a holding that disagrees with region ownership fails the validator");
        }

        // R6.6/R6.7: world setup consumed the handoff's own tables.
        bool grudges_equal = fixture.setup_grudges.size() == eo.grudges.size();
        for (std::size_t i = 0; grudges_equal && i < eo.grudges.size(); ++i)
            grudges_equal = fixture.setup_grudges[i].from == eo.grudges[i].from
                         && fixture.setup_grudges[i].to == eo.grudges[i].to
                         && fixture.setup_grudges[i].score == eo.grudges[i].score;
        check(grudges_equal,
              "R6.6  sentiment was seeded from the 1660 handoff's grudge table");

        bool corridors_equal = fixture.setup_corridors.size() == eo.surviving_corridors.size();
        for (std::size_t i = 0; corridors_equal && i < eo.surviving_corridors.size(); ++i)
            corridors_equal = fixture.setup_corridors[i].a == eo.surviving_corridors[i].a
                           && fixture.setup_corridors[i].b == eo.surviving_corridors[i].b
                           && fixture.setup_corridors[i].uses == eo.surviving_corridors[i].uses;
        check(corridors_equal,
              "R6.7  roads were stamped from the 1660 handoff's surviving network");

        // REPORTED, not gated: how far 1660 moved from 1200 on this seed.
        const auto pair_in = [](const std::vector<grudge>& v, int f, int t) {
            for (const grudge& g : v) if (g.from == f && g.to == t) return true;
            return false;
        };
        int g_only_1200 = 0, g_only_1660 = 0;
        for (const grudge& g : fixture.pre_exploration_grudges)
            if (!pair_in(eo.grudges, g.from, g.to)) ++g_only_1200;
        for (const grudge& g : eo.grudges)
            if (!pair_in(fixture.pre_exploration_grudges, g.from, g.to)) ++g_only_1660;
        const auto corr_in = [](const std::vector<history_corridor>& v, int a, int b) {
            for (const history_corridor& c : v) if (c.a == a && c.b == b) return true;
            return false;
        };
        int c_only_1200 = 0, c_only_1660 = 0;
        for (const history_corridor& c : fixture.pre_exploration_corridors)
            if (!corr_in(eo.surviving_corridors, c.a, c.b)) ++c_only_1200;
        for (const history_corridor& c : eo.surviving_corridors)
            if (!corr_in(fixture.pre_exploration_corridors, c.a, c.b)) ++c_only_1660;
        std::printf("      handoff 1200 -> 1660: grudges %zu -> %zu (%d dropped, %d new); "
                    "corridors %zu -> %zu (%d dropped, %d new)\n",
                    fixture.pre_exploration_grudges.size(), eo.grudges.size(),
                    g_only_1200, g_only_1660,
                    fixture.pre_exploration_corridors.size(), eo.surviving_corridors.size(),
                    c_only_1200, c_only_1660);
        std::printf("      world setup: grudge_sentiment_rows=%lld dropped=%lld "
                    "stamped_corridors=%lld\n",
                    static_cast<long long>(rep.grudge_sentiment_rows),
                    static_cast<long long>(rep.grudge_sentiment_dropped),
                    static_cast<long long>(rep.prehistory_corridors));
        int subjects = 0;
        for (const polity& q : eo.polities) if (q.alive && q.overlord >= 0) ++subjects;
        std::printf("      handoff 1660: regions=%zu polities=%zu holdings=%zu subjects=%d "
                    "standing_dated_objects=%zu contacts=%zu wants=%zu culture_prefs=%zu\n",
                    eo.regions.size(), eo.polities.size(), eo.holdings.size(), subjects,
                    eo.dated_objects.size(), eo.contacts.size(), eo.wants.size(),
                    eo.culture_preference.size());
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
    // R5: the upkeep hook is callable and EARNS (BL-932 gave it a body)
    // -----------------------------------------------------------------
    {
        std::vector<region> regions(2);
        regions[0].nation = 0; regions[0].farm_q = 1000; regions[0].ore_q = 1000;
        regions[0].energy_q = 1000; regions[0].port_q = 1000;
        regions[0].has_market = true;
        regions[1].nation = 1; // a poor, unconnected polity's capital: no endowment set

        // BL-954: ids set explicitly. With the default id (-1) no region read
        // as held, so the endowment term earned nothing and R5.1 passed only
        // on the flat market income BL-954 removed -- it now tests endowment.
        std::vector<polity> qs(2);
        qs[0].id = 0; qs[0].capital = 0; qs[0].capacity[0] = 3;
        qs[1].id = 1; qs[1].capital = 1; qs[1].cohesion_q = 700;
        history_sim_params ep2;
        ep2.start_year = 1200; // consolidation year: not this call's `year`

        run_exploration_upkeep(regions, qs, /*corridors=*/{}, ep2, /*year=*/1234, /*step_years=*/4);

        check(regions[0].treasury > regions[1].treasury,
              "R5.1  a rich, market-carrying capital earns more treasury than a bare one");
        check(regions[1].treasury == 0,
              "R5.2  a capital with no endowment, no market and no corridor touch earns nothing");
        check(qs[0].capacity[0] == 3 && qs[1].cohesion_q == 700,
              "R5.3  upkeep does not disturb fields it has no business touching");

        // R5.4: the ONE-TIME consolidation, at year == start_year only.
        // BL-935's PAY/INVEST now spends from this same treasury the very
        // same call, so this test isolates EARN's own consolidation act by
        // switching every BL-935 cost off -- R5.5/R5.6 below cover spend.
        history_sim_params ep_consolidate_only = ep2;
        ep_consolidate_only.port_build_cost_q = 0;
        ep_consolidate_only.navy_build_cost_q = 0;
        ep_consolidate_only.standing_army_build_cost_q = 0;
        regions[0].material_stock = 500;
        const int64_t before_treasury = regions[0].treasury;
        run_exploration_upkeep(regions, qs, {}, ep_consolidate_only, /*year=*/1200, /*step_years=*/4);
        check(regions[0].treasury >= before_treasury + 500 && regions[0].material_stock == 0,
              "R5.4  consolidation folds material_stock into treasury once, at the span's own "
              "start year, and empties the seat's material_stock");
    }

    // -----------------------------------------------------------------
    // R5b: BL-935 -- ports, navies and standing armies, PAY then INVEST
    // -----------------------------------------------------------------
    {
        std::vector<region> regions(1);
        regions[0].nation = 0;
        regions[0].port_q = 1000; // the endowment WINDOW -- never spent itself
        regions[0].port_stock_q = 500; // already partly built: clears `navy_min_port_stock_q`
                                       // and still leaves room for R5.5's own build step to fire
        regions[0].treasury = 100000;
        std::vector<polity> qs(1);
        qs[0].capital = 0;

        history_sim_params ep;
        ep.start_year = 9999; // never this call's `year` -- no consolidation noise

        exploration_upkeep_spend spend;
        run_exploration_upkeep(regions, qs, /*corridors=*/{}, ep, /*year=*/1234,
                               /*step_years=*/1, &spend);
        check(regions[0].port_stock_q > 0 && spend.ports == ep.port_build_cost_q,
              "R5.5  a funded port raises `port_stock_q` and spends the treasury doing it");
        check(qs[0].navy_stock > 0 && spend.navies == ep.navy_build_cost_q,
              "R5.6  a funded, port-staged navy grows and spends the treasury doing it");
        check(regions[0].army_stock > 0 && spend.standing_armies == ep.standing_army_build_cost_q,
              "R5.7  a funded standing army adds to `army_stock` and spends the treasury doing it");

        // R5.8: underfunded, a built port silts and the navy still decays --
        // a fleet is a running cost, never a one-time purchase.
        regions[0].treasury = 0;
        const int port_before = regions[0].port_stock_q;
        const int64_t navy_before = qs[0].navy_stock;
        run_exploration_upkeep(regions, qs, /*corridors=*/{}, ep, /*year=*/1235,
                               /*step_years=*/1);
        check(regions[0].port_stock_q < port_before,
              "R5.8  an underfunded port silts toward nothing");
        check(qs[0].navy_stock < navy_before,
              "R5.9  a navy decays every round regardless of funding");
    }

    // -----------------------------------------------------------------
    // R6: BL-954 -- trade is a want met by throughput
    // -----------------------------------------------------------------
    {
        const int farm = scarcity_good_index(region_class::farm);

        // Polity 0 (the BUYER): one market capital holding nothing, so it
        // wants farm at the "held nowhere" level. Polity 1 (the SELLER): a
        // market capital dominant in nothing plus one farm region --
        // holding_q for farm is 500, and for every other good 0. A supply corridor joins buyer region 0 to seller
        // region 2; its line is the weaker side's reach, 600.
        std::vector<region> base_regions(3);
        base_regions[0].nation = 0; base_regions[0].has_market = true;
        base_regions[0].dominant = region_class::none; base_regions[0].network_supply_q = 1000;
        base_regions[1].nation = 1; base_regions[1].has_market = true;
        base_regions[1].dominant = region_class::none;
        base_regions[2].nation = 1; base_regions[2].dominant = region_class::farm;
        base_regions[2].network_supply_q = 600;

        std::vector<polity> base_qs(2);
        base_qs[0].id = 0; base_qs[0].alive = true; base_qs[0].capital = 0;
        base_qs[1].id = 1; base_qs[1].alive = true; base_qs[1].capital = 1;

        const std::vector<history_corridor> corridors = { history_corridor{0, 2, 5} };

        history_sim_params ep;
        ep.start_year = 9999; // no consolidation
        ep.port_build_cost_q = 0; ep.navy_build_cost_q = 0; ep.standing_army_build_cost_q = 0;

        const std::vector<dated_object> no_trade = {
            dated_object{2000, static_cast<int32_t>(treaty_clause::non_aggression), 0, 1} };
        const std::vector<dated_object> with_trade = {
            dated_object{2000, static_cast<int32_t>(treaty_clause::non_aggression), 0, 1},
            dated_object{2000, static_cast<int32_t>(treaty_clause::trade_access), 0, 1} };

        // R6.1 -- no flow without the clause, even with want, holder and line.
        {
            std::vector<region> regions = base_regions;
            std::vector<polity> qs = base_qs;
            std::vector<trade_flow> flows = { trade_flow{9, 9, 0, 1} }; // must be overwritten
            run_exploration_upkeep(regions, qs, corridors, ep, 1234, 4, nullptr, &no_trade, &flows);
            check(flows.empty(),
                  "R6.1  no flow forms without a trade_access clause (non-aggression alone opens none)");
        }

        // R6.2-R6.6 -- the clause opens exactly the one flow the ground supports.
        std::vector<region> regions = base_regions;
        std::vector<polity> qs = base_qs;
        std::vector<trade_flow> flows;
        run_exploration_upkeep(regions, qs, corridors, ep, 1234, 4, nullptr, &with_trade, &flows);
        const bool one_flow = flows.size() == 1 && flows[0].seller == 1 && flows[0].buyer == 0
                           && flows[0].good == farm;
        check(one_flow, "R6.2  a trade_access clause opens the seller->buyer farm flow, and only it");
        const int raw_want = regions[0].scarcity_raw_q[farm];
        check(one_flow && raw_want == 700 && flows[0].volume_q == 500,
              "R6.3  volume is bounded by the seller's holding (min of want 700, holding 500, line 600)");
        check(one_flow && regions[0].scarcity_q[farm] == raw_want - flows[0].volume_q
           && regions[0].scarcity_raw_q[farm] == 700,
              "R6.4  a met want relieves the buyer's signal (raw kept, unmet = raw - inbound)");

        {
            std::vector<region> r_none = base_regions;
            std::vector<polity> q_none = base_qs;
            run_exploration_upkeep(r_none, q_none, corridors, ep, 1234, 4, nullptr, &no_trade, nullptr);
            const int64_t expected = (500LL * ep.treasury_trade_income_q * 4) / 1000;
            check(expected > 0
               && regions[0].treasury - r_none[0].treasury == expected
               && regions[1].treasury - r_none[1].treasury == expected,
                  "R6.5  the flow credits BOTH capitals, volume x treasury_trade_income_q x step / 1000");
        }

        // Want, holding and line each bound the volume on their own.
        {
            std::vector<region> rw = regions;
            rw[0].scarcity_raw_q[farm] = 120;
            const trade_context ctx = build_trade_context(rw, qs, corridors);
            check(trade_flow_volume_q(ctx, rw, qs, 1, 0, farm) == 120,
                  "R6.6  volume is bounded by the buyer's raw want");
        }
        {
            std::vector<region> rl = regions;
            rl[2].network_supply_q = 80;
            const trade_context ctx = build_trade_context(rl, qs, corridors);
            check(trade_flow_volume_q(ctx, rl, qs, 1, 0, farm) == 80,
                  "R6.7  volume is bounded by the land line (the weaker side's reach to the border)");
        }
        {
            // No corridor: the line is the sea, and only the SELLER's navy carries it.
            std::vector<region> rs = regions;
            std::vector<polity> qn = qs;
            rs[0].port_stock_q = 800; rs[1].port_stock_q = 300;
            const trade_context ctx = build_trade_context(rs, qn, /*corridors=*/{});
            check(trade_flow_volume_q(ctx, rs, qn, 1, 0, farm) == 0,
                  "R6.8  no corridor and no seller navy -> no line, no volume");
            qn[0].navy_stock = 500; // the BUYER's navy does not carry the seller's goods
            check(trade_flow_volume_q(ctx, rs, qn, 1, 0, farm) == 0,
                  "R6.9  a buyer's navy alone opens no sea line");
            qn[1].navy_stock = 500;
            check(trade_flow_volume_q(ctx, rs, qn, 1, 0, farm) == 300,
                  "R6.10 the seller's navy opens the sea line, bounded by the smaller built port");
        }

        // No flat market income remains: a market capital with no endowment,
        // no corridor and no trade earns nothing at all.
        {
            std::vector<region> rm(1);
            rm[0].nation = 0; rm[0].has_market = true;
            std::vector<polity> qm(1);
            qm[0].id = 0; qm[0].alive = true; qm[0].capital = 0;
            run_exploration_upkeep(rm, qm, {}, ep, 1234, 4);
            check(rm[0].treasury == 0,
                  "R6.11 a market capital with no endowment, corridor or trade earns nothing (no flat market income)");
        }

        // A pair with trade to open is worth more to bind than the same pair without.
        {
            const trade_context ctx = build_trade_context(regions, qs, corridors);
            const int trade_value = pair_trade_value_q(ctx, regions, qs, 0, 1);
            check(trade_value == 500,
                  "R6.12 pair_trade_value_q reads the flow the clause WOULD open, before any binding");
            for (const bool near : {true, false})
            {
                const int without = treaty_value_q(ep, 0, 0, 0, /*aggression=*/400, /*alarm=*/0, near, 0);
                const int with_tr = treaty_value_q(ep, 0, 0, 0, /*aggression=*/400, /*alarm=*/0, near,
                                                    trade_value);
                check(with_tr > without,
                      near ? "R6.13 trade to open raises treaty value near home"
                           : "R6.14 trade to open raises treaty value far from home too");
            }
        }
    }

    std::printf("\n%s (%d failure%s)\n",
                g_failures == 0 ? "ALL PASS" : "FAILURES",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
