// ---------------------------------------------------------------------------
// exploration_sweep — BL-937. Run the Exploration span (1200 -> 1660 CE) over
// a seed spread and report the ELEVEN readings EXPLORATION.md sec "What the
// phase is judged on" names, all taken at 1660, all over the spread, NEVER
// per world (the same distributional discipline history_sweep applies to the
// Empire handoff).
//
// WHY A SIBLING HARNESS RATHER THAN A NEW BLOCK IN history_sweep.cpp. That
// file is already ~3,800 lines and its own subject is the Empire span's
// closure contract (BL-275/BL-907). This item's subject is a different span
// with a different judgement (EXPLORATION.md, not CIVILISATION.md), so a
// sibling keeps each harness readable against the one doc it measures,
// exactly as history_sweep itself argues for one report per closure.
//
// THIS HARNESS REPORTS; IT DOES NOT GATE, on the same instruction
// history_sweep's own banner states: EXPLORATION.md is explicit that "a
// reading is a requirement, not a target" — a seed that refuses one is a
// legitimate world, a spread that refuses one is a phase that did not do its
// job, and that is Ben's judgement to make off the printed table, not this
// harness's to assert into a PASS/FAIL line.
//
// WHAT EACH READING IS READ OFF. Every one of the ten readings is computed
// off real sim output. Readings 1-2 (displacement, conflict-persists) and
// 3-7 read a traced, disposable re-run of the Exploration span (battle
// traces, treaties, subjects, fleets, and the 1660 regions/cultures the
// re-run leaves behind); readings 8-9 read the treasury/corridor state of
// that re-run and of generation's own untraced run; reading 10 reads the
// 1200 CE handoff state directly. Where a reading has no data on a spread it
// says so in its own printed line (NOT MEASURED), never by a missing row.
//
// ONE GENERATION PER SEED (BL-952). `make_hard_coded_world` is the dominant
// cost of this harness, so it is called exactly once per seed, in the main
// loop, and everything any reading needs is captured onto `exploration_row`
// there. The report sections below read `rows` only; none regenerates a
// world, and each generation stops at the Exploration close (BL-958), so
// the `[sweep] seed N generated` line count equals the seed count.
//
// Usage:  exploration_sweep [seed_count] [--w_want_q=N]   (default 8)
//
// Writes: exploration_sweep.json in the working directory (BL-971) — one row
// per seed over all eleven readings plus the spread face, checked in at the
// repo root from a 16-seed run at generation's own constants. A tuning run
// (any override flag) writes exploration_sweep.tuning.json instead. Every
// HELD seed on readings 1-2 also carries a named cause (BL-999): printed in
// its own section and written per seed as `held_cause` with the facts it
// was read off (`held_cause_facts`, written for every ran seed).
//
//   --w_want_q=N  BL-953 TUNING ONLY: re-runs the traced span with the want
//                 lean at N instead of generation's own value. Readings 1, 2,
//                 4-7 then describe the overridden run; the traced-vs-untraced
//                 structural check is skipped (it compares against a run made
//                 with different params by construction).
//   --set NAME=N  BL-949/BL-950 TUNING ONLY: same footing as --w_want_q, for
//                 post_road_treasury_cost, deterrence_alarm_weight_q,
//                 treaty_far_penalty_q, visible_capability_reference. Reading 9
//                 then reads the traced run too.
// ---------------------------------------------------------------------------

#include "world/era_minus_one.hpp"
#include "world/hard_coded_world.hpp"
#include "world/history_sim.hpp"
#include "world/sim_terrain_build.hpp"
#include "world/settlement.hpp"
#include "world/works_roster.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace
{

int g_failures = 0;

void check(bool ok, const char* label)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", label);
    if (!ok) ++g_failures;
}

const generation_report::body_entry* kepler_of(const generation_report& r)
{
    for (const generation_report::body_entry& b : r.bodies)
        if (b.is_homeworld) return &b;
    return r.bodies.empty() ? nullptr : &r.bodies.front();
}

// NO WORKS FIXTURE HERE, DELIBERATELY. `make_hard_coded_world` is called
// below with no registry (works = nullptr), on the same footing as
// history_sweep.cpp's own generation-deriving path — see that file's S1b
// comment: "generation is called here with works = nullptr... the APP passes
// its real registry, so the shipped game scores FIVE verbs where every
// harness scores four." A traced re-run of the Exploration span must use
// EXACTLY what generation's own run used (`era_minus_one_fixture::works`,
// captured below), not a harness-invented registry — the first draft of this
// file passed one and it broke the acceptance check for that reason.

/// Does `contacts` carry a directed pair naming both @p a and @p b, in either
/// direction? `contact` is recorded in BOTH directions when a pair first
/// meets (history_sim.hpp's own comment on the struct), so one direction is
/// technically enough — checked both ways here rather than trusted, because
/// asserting an invariant a different item guarantees is how this file avoids
/// silently depending on it.
bool contact_exists(const std::vector<contact>& contacts, int a, int b)
{
    for (const contact& c : contacts)
        if ((c.from == a && c.to == b) || (c.from == b && c.to == a)) return true;
    return false;
}

// ---------------------------------------------------------------------------
// ONE SEED'S READING.
// ---------------------------------------------------------------------------
struct exploration_row
{
    uint32_t seed = 0;
    bool     ok   = false; ///< False if the span did not run at all for this seed.

    // --- The Empires round's own close, for reading 2's baseline -----------
    int64_t empire_battles    = 0;
    int64_t empire_years      = 0;

    // --- The Exploration span, as generation's own (untraced) run produced -
    int64_t expl_battles      = 0;
    int64_t expl_conquests    = 0;
    int64_t expl_foundings    = 0;
    int64_t expl_years        = 0;

    // --- Readings 1-2, off the traced re-run --------------------------------
    int64_t neighbour_wars    = 0; ///< Battles between pairs already in contact by 1200.
    int64_t frontier_skirmishes = 0; ///< Battles between pairs that first met DURING the span.
    int64_t ambiguous_battles = 0; ///< defender id 0 — unowned ground or polity 0, indistinguishable (see note below).

    bool traced_matches_untraced = false; ///< The acceptance check: re-run reproduces battles/conquests/foundings bit for bit.

    /// BL-932 DIAGNOSTIC (not yet reading 8's formal row — that is BL-940's,
    /// once throughput exists too): (capital treasury, corridors touching
    /// held ground) at the traced run's own close, one pair per living
    /// polity. A cheap proxy for "the inherited corridor network" ahead of
    /// BL-940 giving throughput its own real quantity.
    std::vector<std::pair<int64_t, int64_t>> polity_treasury_corridor;

    // --- Reading 4: treaty depth --------------------------------------------
    int64_t treaties_formed_total  = 0;
    int64_t treaties_broken_total  = 0;
    int64_t treaty_blocked_campaigns = 0;
    std::vector<int64_t> treaty_years_left; ///< one per standing non-aggression pair at 1660.

    // --- Reading 5: colonial asymmetry --------------------------------------
    int64_t subjects_alive       = 0; ///< living polities with `overlord >= 0`.
    int64_t overlords_alive      = 0; ///< distinct living polities holding >= 1 subject.
    int64_t alive_polities       = 0;
    int64_t max_subject_distance = 0; ///< Chebyshev, capital to overlord's capital.
    int64_t subjections_formed   = 0;
    int64_t subjections_freed    = 0;
    int64_t tribute_remitted     = 0;

    // --- Reading 6: subject friction (operationalized -- see the printed
    // note beside readings 1-2's own operationalization disclosure). A
    // subject's contact graph is read as a proxy for its want graph (no live
    // want table is maintained during the span; contact is the want table's
    // own precondition -- CIVILISATION.md sec The directed want: "a want
    // requires contact"): friction is counted where a subject has met a
    // THIRD polity its overlord has never met.
    int64_t subjects_with_friction = 0;

    /// BL-953: subjects whose TOP WANT (argmax good of `polity_good_want_q`,
    /// off the LIVE preference derived from the traced run's own 1660 state)
    /// differs from their overlord's -- the want graph itself rather than the
    /// contact-graph proxy above. "No want at all" counts as its own value.
    int64_t subjects_with_divergent_top_want = 0;

    /// BL-953: (culture, good) preference entries derived LIVE from the traced
    /// run's 1660 close, beside reading 10's 1200 CE derivation.
    int64_t live_pref_entries_1660 = 0;

    // --- Reading 7: fleets ---------------------------------------------------
    int64_t navy_holders          = 0; ///< living polities with navy_stock > 0 at 1660.
    int64_t treasury_spent_on_navies = 0;
    int64_t treasury_spent_on_ports  = 0;
    int64_t treasury_spent_on_standing_armies = 0;
    /// BL-955: navies at 1660 split by the holder's EXPANSION RANK at 1660
    /// (`exploration_lean_ranks`): top half = rank >= 500.
    int64_t navy_holders_expn_top    = 0;
    int64_t navy_holders_expn_bottom = 0;
    /// BL-955: stock steps bought over the span, by kind (one choice per round).
    int64_t port_steps = 0, navy_steps = 0, army_steps = 0;
    /// BL-955: polities whose navy decayed from a standing fleet to zero at
    /// least once during the span.
    int64_t navies_lapsed = 0;
    /// BL-955: per living polity at 1660, its paid standing army (summed over
    /// held ground) per held region, and its mean garrison_target per held
    /// region -- the scale the saturation reference is read against.
    std::vector<int64_t> standing_per_region;
    std::vector<int64_t> garrison_per_region;
    int64_t standing_holders = 0; ///< living polities with any paid standing heads at 1660.
    /// BL-972: the BILL replaces the scorer caps. Polity-rounds on which the
    /// army/navy bill went short, treasury paid in upkeep over the span, and
    /// the levy drawn from / returned to the manpower pools.
    int64_t army_unpaid_rounds = 0;
    int64_t navy_unpaid_rounds = 0;
    int64_t spent_army_upkeep  = 0;
    int64_t spent_navy_upkeep  = 0;
    int64_t levy_raised        = 0;
    int64_t levy_returned      = 0;
    int64_t standing_invariant_violations = 0; ///< must be 0 (BL-955 F3).

    // --- Reading 3: both strategies pay -------------------------------------
    /// One realm in a seed's top 3, with every quantity either ranking reads.
    struct strength_entry
    {
        int     id            = -1;
        int64_t treasury      = 0; ///< capital region's `treasury` at 1660.
        int     mean_supply_q = 0; ///< mean `network_supply_q` over held regions.
        int64_t regions       = 0; ///< held region count at 1660.
        int     cons_q        = 0; ///< consolidator_lean_q of its culture.
        int     expn_q        = 0; ///< expansion_lean_q of its culture.
    };
    bool strength_measured = false; ///< >= 2 living polities at the traced re-run's close.
    /// PRIMARY ranking (BL-951): capital treasury, tie-break mean supply.
    std::vector<strength_entry> top_by_treasury;
    bool top_has_consolidator = false;
    bool top_has_expansionist = false;
    /// COMPARISON ranking: held region count (the pre-BL-951 metric).
    std::vector<strength_entry> top_by_regions;
    bool regions_top_has_consolidator = false;
    bool regions_top_has_expansionist = false;

    // --- Reading 9: throughput, off generation's own (untraced) run ---------
    std::vector<int32_t> corridor_uses;
    int64_t post_roads_built        = 0;
    int64_t treasury_spent_on_roads = 0;
    /// BL-949: corridors at each carried rung (`history_corridor::tier`, 0-3)
    /// at the close, and inherited corridors whose 1200 traffic alone already
    /// reaches `road_tier3_uses` (a third rung NOT bought).
    int64_t tier_count[4] = {0, 0, 0, 0};
    int64_t inherited_traffic_tier3 = 0;

    // --- Reading 10: preference, off the 1200 CE handoff state --------------
    std::vector<int16_t> preference_weights;

    // --- Reading 11: trade (BL-954) -----------------------------------------
    int64_t flow_count            = 0;
    int64_t flow_volume           = 0;
    int64_t cross_landmass_volume = 0; ///< seller and buyer capitals on different landmasses.
    int64_t unknown_landmass_volume = 0; ///< a capital tile that reads as water: landmass undefined.
    int64_t flows_without_clause  = 0; ///< must be 0: every flow stands on a trade_access clause.
    int64_t trade_bound_pairs     = 0; ///< distinct pairs holding trade_access at 1660.
    std::vector<int64_t> pair_volume; ///< one per trade_access-bound pair, zero-flow pairs included.

    // --- BL-999: the held-seed cause census, off the traced re-run ---------
    // Every quantity here is captured for EVERY ran seed (so a displaced
    // seed's numbers sit beside a held one's for comparison) and read only
    // when readings 1-2 call the seed HELD.
    /// `campaign_class_trace` copied whole: [class][gate], class 0 = met
    /// before the span (a neighbour), 1 = met during it, 2 = unmet; gate 0 =
    /// examined, 1 = treaty-blocked, 2 = water-illegal, 3 = reach-denied,
    /// 4 = cleared the score threshold, 5 = chosen. Candidate-grain, summed
    /// over the whole span.
    int64_t campaign_class[3][6] = {};
    /// Pairs at 1660 by contact class, and how many of each hold a
    /// non-aggression clause at the close.
    int64_t near_pairs = 0, near_bound = 0, far_pairs = 0, far_bound = 0;
    /// Of the near pairs, those where EITHER side reads a positive
    /// `deterrence_alarm_q` at the close (a snapshot, not a span integral —
    /// the sim keeps no cumulative alarm counter), and the largest such read.
    int64_t near_pairs_alarmed = 0;
    int     near_alarm_max_q   = 0;
    /// Polities holding the exploration tree's rim node at the close: alive,
    /// and ever (the mask is never cleared, so a dead holder still reads).
    int64_t rim_holders_alive = 0, rim_holders_ever = 0;
    /// The directed want table derived at the close: entries, those whose
    /// holder sits on ANOTHER landmass from the wanter's capital (the doc's
    /// own "outward, across water"), and those whose pair met during the span.
    int64_t wants_total = 0, wants_outward = 0, wants_frontier = 0;
};

/// LANDMASS IDENTITY for reading 11. `region::domain` only says land /
/// coastal water / open ocean -- it carries no mass id, and `pass_one_output`
/// says landmass identity is derived by a consumer. Derived here as the
/// 4-connected components of non-water tiles over the sim's own terrain
/// raster, columns wrapping (the map is a cylinder, history_sim.cpp's own
/// distance helper). -1 on water.
std::vector<int32_t> label_landmasses(const std::vector<terrain_substrate>& sub, int gw, int gh)
{
    std::vector<int32_t> mass(sub.size(), -1);
    if (gw <= 0 || gh <= 0 || sub.size() < static_cast<std::size_t>(gw) * static_cast<std::size_t>(gh))
        return mass;
    int32_t next = 0;
    std::vector<int> stack;
    for (int start = 0; start < gw * gh; ++start)
    {
        if (mass[static_cast<std::size_t>(start)] >= 0 || is_water(sub[static_cast<std::size_t>(start)]))
            continue;
        mass[static_cast<std::size_t>(start)] = next;
        stack.push_back(start);
        while (!stack.empty())
        {
            const int idx = stack.back(); stack.pop_back();
            const int c = idx % gw, r = idx / gw;
            const int nb[4][2] = { {(c + 1) % gw, r}, {(c + gw - 1) % gw, r}, {c, r + 1}, {c, r - 1} };
            for (const auto& n : nb)
            {
                if (n[1] < 0 || n[1] >= gh) continue;
                const int ni = n[1] * gw + n[0];
                if (mass[static_cast<std::size_t>(ni)] >= 0 || is_water(sub[static_cast<std::size_t>(ni)]))
                    continue;
                mass[static_cast<std::size_t>(ni)] = next;
                stack.push_back(ni);
            }
        }
        ++next;
    }
    return mass;
}

/// Century-scaled rate, avoiding a divide-by-zero span.
double per_century(int64_t count, int64_t years)
{
    return years > 0 ? (static_cast<double>(count) * 100.0) / static_cast<double>(years) : 0.0;
}

} // namespace

int main(int argc, char** argv)
{
    int seed_count = 8;
    bool want_override = false;
    int  want_override_q = 0;
    std::vector<std::pair<std::string, long long>> param_sets;
    for (int a = 1; a < argc; ++a)
    {
        if (std::strcmp(argv[a], "--set") == 0 && a + 1 < argc)
        {
            const std::string kv = argv[++a];
            const std::size_t eq = kv.find('=');
            if (eq != std::string::npos)
                param_sets.push_back({kv.substr(0, eq), std::atoll(kv.c_str() + eq + 1)});
            continue;
        }
        if (std::strncmp(argv[a], "--w_want_q=", 11) == 0)
        {
            want_override   = true;
            want_override_q = std::atoi(argv[a] + 11);
            continue;
        }
        const int n = std::atoi(argv[a]);
        if (n > 0) seed_count = n;
    }
    const auto apply_sets = [&](history_sim_params& hp) {
        for (const auto& kv : param_sets)
        {
            if      (kv.first == "post_road_treasury_cost")      hp.post_road_treasury_cost = kv.second;
            else if (kv.first == "deterrence_alarm_weight_q")    hp.deterrence_alarm_weight_q = static_cast<int>(kv.second);
            else if (kv.first == "treaty_far_penalty_q")         hp.treaty_far_penalty_q = static_cast<int>(kv.second);
            else if (kv.first == "visible_capability_reference") hp.visible_capability_reference = kv.second;
            // BL-972: the bill and the levy bound, for tuning runs.
            else if (kv.first == "standing_army_upkeep_per_1000_heads_year_q") hp.standing_army_upkeep_per_1000_heads_year_q = kv.second;
            else if (kv.first == "navy_upkeep_per_1000_units_year_q")          hp.navy_upkeep_per_1000_units_year_q = kv.second;
            else if (kv.first == "standing_army_levy_per_mille_q")             hp.standing_army_levy_per_mille_q = static_cast<int>(kv.second);
            else { std::printf("unknown --set %s\n", kv.first.c_str()); std::exit(2); }
        }
    };
    for (const auto& kv : param_sets)
        std::printf("NOTE: --set %s=%lld overrides the traced re-run (tuning only).\n",
                    kv.first.c_str(), kv.second);
    if (!param_sets.empty()) { want_override = true; want_override_q = -1; }
    if (want_override && want_override_q >= 0)
        std::printf("NOTE: --w_want_q=%d overrides the traced re-run's want lean (tuning only).\n",
                    want_override_q);

    std::printf("=== exploration sweep (BL-937) - %d seeds, 1200 -> 1660 CE ===\n\n", seed_count);

    std::vector<exploration_row> rows;
    rows.reserve(static_cast<std::size_t>(seed_count));

    for (int i = 0; i < seed_count; ++i)
    {
        world_params wp;
        wp.seed = static_cast<uint32_t>(i);
        wp.exploration_sim_enabled = true; // BL-937: the whole point of this sweep.

        generation_report     rep;
        era_minus_one_fixture fx;
        // BL-958: STOP AT THE EXPLORATION CLOSE. Every reading reads the era
        // fixture, which is complete before this stop; nations, roads, firms and
        // markets (~90% of a whole world) are never read here.
        world_gen_config gen_cfg;
        gen_cfg.stop_after_exploration = true;
        std::fprintf(stderr, "[sweep] seed %d generated to the Exploration close\n", i);
        const world w = make_hard_coded_world(wp, &rep, gen_cfg,
                                              /*progress=*/nullptr, /*works=*/nullptr, &fx);
        (void)w;

        exploration_row row;
        row.seed = wp.seed;

        if (!fx.ran)
        {
            std::printf("  seed %d: THE EMPIRES ROUND DID NOT RUN — skipped.\n", i);
            rows.push_back(row);
            continue;
        }
        row.empire_battles = fx.battles;
        row.empire_years   = fx.years;

        if (!fx.exploration_ran)
        {
            // Real and worth seeing: BL-931's own gate is
            // `!kepler_pass_one.polities.empty()`, so a seed whose Empires
            // round eliminated every polity before 1200 (a hegemon or a
            // total collapse) legitimately produces no Exploration span at
            // all. EXPLORATION.md's own discipline: a seed that refuses a
            // reading is a legitimate world.
            std::printf("  seed %d: EMPIRES ROUND RAN, EXPLORATION SPAN DID NOT (no surviving polity at 1200).\n", i);
            rows.push_back(row);
            continue;
        }

        row.expl_battles   = fx.exploration_state.battles;
        row.expl_conquests = fx.exploration_state.conquests;
        row.expl_foundings = fx.exploration_state.foundings;
        row.expl_years     = fx.exploration_state.years;

        // --- Reading 9 capture: generation's own (untraced) run -------------
        // `fx.exploration_state.supply_corridors`, finalised at that run's own
        // close, and the two road-ladder counters BL-940 added.
        for (const history_corridor& c : fx.exploration_state.supply_corridors)
        {
            row.corridor_uses.push_back(c.uses);
            ++row.tier_count[c.tier < 4 ? c.tier : 3];
        }
        {
            const int t3 = history_sim_params{}.road_tier3_uses;
            for (const history_corridor& c : fx.pre_exploration_corridors)
                if (c.uses >= t3) ++row.inherited_traffic_tier3;
        }
        row.post_roads_built        = fx.exploration_state.post_roads_built;
        row.treasury_spent_on_roads = fx.exploration_state.treasury_spent_on_roads;

        // --- Reading 10 capture: the PRE-EXPLORATION handoff state ---------
        // `derive_culture_preference` is pure and needs no traced re-run of
        // its own — it takes the same regions/contacts/polities `derive_wants`
        // already reads at 1200 CE. Taken BEFORE the traced re-run below so it
        // reads the fixture exactly as generation left it.
        {
            const auto prefs = derive_culture_preference(
                fx.pre_exploration_settlement.regions, fx.pre_exploration_contacts,
                fx.pre_exploration_polities, static_cast<int>(fx.pre_exploration_creeds.cultures.size()));
            for (const auto& p : prefs) row.preference_weights.push_back(p.weight_q);
        }

        // --- THE TRACED RE-RUN (readings 1-2 need per-battle attacker/ -----
        // defender pairs, which only `battle_trace` carries, and tracing is
        // deliberately never turned on in generation's own run — see the
        // field comment on `era_minus_one_fixture::pre_exploration_settlement`
        // for why this is a second, disposable run rather than a flag flipped
        // on the real one.
        history_sim_params ep2 = fx.exploration_params;
        ep2.trace_battles = true;
        ep2.resume_polities  = &fx.pre_exploration_polities;
        ep2.resume_grudges   = &fx.pre_exploration_grudges;
        ep2.resume_contacts  = &fx.pre_exploration_contacts;
        ep2.resume_corridors = &fx.pre_exploration_corridors;
        if (want_override && want_override_q >= 0) ep2.w_want_q = want_override_q;
        apply_sets(ep2);

        settlement_state ss_copy = fx.pre_exploration_settlement;
        creed_state       cs_copy = fx.pre_exploration_creeds;
        // `fx.works` — WHATEVER GENERATION'S OWN RUN ACTUALLY USED (nullptr in
        // this harness, since `make_hard_coded_world` is called below with no
        // registry — the same known divergence history_sweep.cpp's own S1b
        // comment names: "the APP passes its real registry, so the shipped
        // game scores FIVE verbs where every harness scores four"). Passing
        // this harness's own `works_fixture()` here instead of `fx.works`
        // made the FIRST run of this harness fail its own acceptance check —
        // Build became a scoreable verb in the re-run and not in the
        // original, which is exactly the "second construction drifts" defect
        // era_minus_one.hpp exists to close. Fixed by reading the capture.
        const history_sim_state traced = run_history_sim(
            ss_copy, &cs_copy, fx.terrain.view(), fx.gw, fx.gh, ep2,
            fx.exploration_seed, /*year_progress=*/nullptr, fx.works, /*tap=*/nullptr);

        if (!param_sets.empty())
        {
            row.corridor_uses.clear();
            for (int t = 0; t < 4; ++t) row.tier_count[t] = 0;
            for (const history_corridor& c : traced.supply_corridors)
            {
                row.corridor_uses.push_back(c.uses);
                ++row.tier_count[c.tier < 4 ? c.tier : 3];
            }
            row.post_roads_built        = traced.post_roads_built;
            row.treasury_spent_on_roads = traced.treasury_spent_on_roads;
        }

        // BL-949: does the road spend track the polities that built? Capital
        // treasury at the close for builders, for EX-WY-1a holders that never
        // built, and for every living polity; plus the builders' share of the
        // EX-WY-1a holders.
        {
            int node = -1;
            for (int n = 0; n < io::exploration_tree::node_count; ++n)
                if (std::strcmp(io::exploration_tree::nodes[n].id, "EX-WY-1a") == 0) { node = n; break; }
            std::vector<int64_t> t_build, t_hold, t_all;
            for (const polity& q : traced.polities)
            {
                if (!q.alive || q.capital < 0
                 || static_cast<std::size_t>(q.capital) >= ss_copy.regions.size()) continue;
                const int64_t t = ss_copy.regions[static_cast<std::size_t>(q.capital)].treasury;
                t_all.push_back(t);
                const bool built = static_cast<std::size_t>(q.id) < traced.post_roads_by_polity.size()
                                && traced.post_roads_by_polity[static_cast<std::size_t>(q.id)] > 0;
                const bool holds = node >= 0 && (q.exploration_mask & (1ULL << node));
                if (built) t_build.push_back(t);
                else if (holds) t_hold.push_back(t);
            }
            const auto med = [](std::vector<int64_t> v) -> long long {
                if (v.empty()) return -1;
                std::sort(v.begin(), v.end());
                return static_cast<long long>(v[v.size() / 2]);
            };
            int64_t builders_total = 0;
            for (int32_t n : traced.post_roads_by_polity) if (n > 0) ++builders_total;
            std::printf("  roads seed %d: built=%lld builders=%lld (alive at close %zu) EX-WY-1a holders not building=%zu"
                        "  median capital treasury: builders=%lld holders-not-building=%lld all=%lld\n",
                        i, static_cast<long long>(traced.post_roads_built),
                        static_cast<long long>(builders_total), t_build.size(), t_hold.size(),
                        med(t_build), med(t_hold), med(t_all));
        }

        // BL-950 DIAGNOSTIC: why is frontier war absent on a seed? Candidate
        // gates by the target owner's contact class, and the pairs bound at close.
        {
            const auto& ct = traced.campaign_class_trace;
            int64_t far_met = 0, far_bound = 0, near_pairs = 0, near_bound = 0;
            for (const contact& c : traced.contacts)
            {
                if (c.from >= c.to) continue;
                bool bound = false;
                for (const dated_object& o : traced.dated_objects)
                    if (o.kind == static_cast<int32_t>(treaty_clause::non_aggression)
                     && ((o.a == c.from && o.b == c.to) || (o.a == c.to && o.b == c.from)))
                    { bound = true; break; }
                if (c.first.year >= ep2.start_year) { ++far_met; if (bound) ++far_bound; }
                else                                 { ++near_pairs; if (bound) ++near_bound; }
            }
            int64_t navies = 0, ports = 0;
            for (const polity& q : traced.polities) if (q.alive && q.navy_stock > 0) ++navies;
            for (const region& r : ss_copy.regions) if (r.port_stock_q > 0) ++ports;

            // BL-999: the same counters onto the row, plus the rim, the alarm
            // read and the want table, so the held-seed census below reads
            // `rows` like every other section.
            for (int k = 0; k < 3; ++k)
                for (int g = 0; g < 6; ++g) row.campaign_class[k][g] = ct[k][g];
            row.near_pairs = near_pairs; row.near_bound = near_bound;
            row.far_pairs  = far_met;    row.far_bound  = far_bound;
            for (const contact& c : traced.contacts)
            {
                if (c.from >= c.to || c.first.year >= ep2.start_year) continue;
                const int aa = deterrence_alarm_q(ss_copy.regions, traced, ep2, c.from, c.to);
                const int ab = deterrence_alarm_q(ss_copy.regions, traced, ep2, c.to, c.from);
                const int m  = std::max(aa, ab);
                if (m > 0) ++row.near_pairs_alarmed;
                row.near_alarm_max_q = std::max(row.near_alarm_max_q, m);
            }
            for (const polity& q : traced.polities)
            {
                if (!polity_holds_exploration_rim(q)) continue;
                ++row.rim_holders_ever;
                if (q.alive) ++row.rim_holders_alive;
            }
            {
                const std::vector<int32_t> mass_w =
                    label_landmasses(fx.terrain.substrate, fx.gw, fx.gh);
                const auto mass_of = [&](int pid) -> int32_t {
                    if (pid < 0 || static_cast<std::size_t>(pid) >= traced.polities.size()) return -1;
                    const int cap = traced.polities[static_cast<std::size_t>(pid)].capital;
                    if (cap < 0 || static_cast<std::size_t>(cap) >= ss_copy.regions.size()) return -1;
                    const region& rg = ss_copy.regions[static_cast<std::size_t>(cap)];
                    if (rg.col < 0 || rg.row < 0 || rg.col >= fx.gw || rg.row >= fx.gh) return -1;
                    return mass_w[static_cast<std::size_t>(rg.row * fx.gw + rg.col)];
                };
                const std::vector<want> wants =
                    derive_wants(ss_copy.regions, traced.contacts, traced.polities);
                row.wants_total = static_cast<int64_t>(wants.size());
                for (const want& w : wants)
                {
                    const int32_t mf = mass_of(w.from), mt = mass_of(w.to);
                    if (mf >= 0 && mt >= 0 && mf != mt) ++row.wants_outward;
                    if (contact_first_year(traced, w.from, w.to) >= ep2.start_year) ++row.wants_frontier;
                }
            }
            std::printf("  diag seed %d: cand[near ex=%lld blk=%lld wet=%lld rch=%lld clr=%lld ch=%lld]"
                        " [met-in-span ex=%lld blk=%lld wet=%lld rch=%lld clr=%lld ch=%lld]"
                        " [unmet ex=%lld blk=%lld wet=%lld rch=%lld clr=%lld ch=%lld]"
                        " pairs near=%lld(bound %lld) met-in-span=%lld(bound %lld) navies=%lld ports=%lld\n",
                        i, (long long)ct[0][0], (long long)ct[0][1], (long long)ct[0][2], (long long)ct[0][3],
                        (long long)ct[0][4], (long long)ct[0][5],
                        (long long)ct[1][0], (long long)ct[1][1], (long long)ct[1][2], (long long)ct[1][3],
                        (long long)ct[1][4], (long long)ct[1][5],
                        (long long)ct[2][0], (long long)ct[2][1], (long long)ct[2][2], (long long)ct[2][3],
                        (long long)ct[2][4], (long long)ct[2][5],
                        (long long)near_pairs, (long long)near_bound, (long long)far_met, (long long)far_bound,
                        (long long)navies, (long long)ports);
        }

        row.traced_matches_untraced = want_override ||
            (traced.battles == fx.exploration_state.battles &&
             traced.conquests == fx.exploration_state.conquests &&
             traced.foundings == fx.exploration_state.foundings);

        for (const battle_trace& bt : traced.battle_traces)
        {
            if (bt.defender == 0 && bt.attacker == 0)
            {
                // Both id 0 is not a real Campaign battle this harness can
                // classify (see the note on `contact_exists`'s caller below)
                // — count it separately rather than guess.
                ++row.ambiguous_battles;
                continue;
            }
            const bool already_known =
                contact_exists(fx.pre_exploration_contacts, bt.attacker, bt.defender);
            if (already_known) ++row.neighbour_wars;
            else               ++row.frontier_skirmishes;
        }

        // BL-932 DIAGNOSTIC — read straight off the traced re-run above,
        // which already mutated `ss_copy` in place and is disposable.
        for (const polity& q : traced.polities)
        {
            if (!q.alive || q.capital < 0
             || static_cast<std::size_t>(q.capital) >= ss_copy.regions.size())
                continue;
            int64_t touch = 0;
            for (const history_corridor& c : traced.supply_corridors)
            {
                const bool a_held = static_cast<std::size_t>(c.a) < ss_copy.regions.size()
                                  && ss_copy.regions[c.a].nation == q.id;
                const bool b_held = static_cast<std::size_t>(c.b) < ss_copy.regions.size()
                                  && ss_copy.regions[c.b].nation == q.id;
                if (a_held || b_held) ++touch;
            }
            row.polity_treasury_corridor.push_back(
                {ss_copy.regions[static_cast<std::size_t>(q.capital)].treasury, touch});
        }

        // --- BL-933/934/935 -- readings 4, 5, 6, 7, off the traced re-run ---
        row.treaties_formed_total     = traced.treaties_formed;
        row.treaties_broken_total     = traced.treaties_broken;
        row.treaty_blocked_campaigns  = traced.treaty_blocked_campaigns;
        row.subjections_formed        = traced.subjections_formed;
        row.subjections_freed         = traced.subjections_freed;
        row.tribute_remitted          = traced.tribute_remitted;
        row.treasury_spent_on_navies          = traced.treasury_spent_on_navies;
        row.treasury_spent_on_ports           = traced.treasury_spent_on_ports;
        row.treasury_spent_on_standing_armies = traced.treasury_spent_on_standing_armies;

        {
            std::vector<std::pair<uint16_t, uint16_t>> active_pairs;
            for (const dated_object& o : traced.dated_objects)
                if (o.kind == static_cast<int32_t>(treaty_clause::non_aggression))
                    active_pairs.push_back({o.a, o.b});
            for (const auto& pr : active_pairs)
            {
                for (const dated_object& o : traced.dated_objects)
                    if (o.kind == static_cast<int32_t>(treaty_clause::non_aggression)
                     && o.a == pr.first && o.b == pr.second)
                    {
                        row.treaty_years_left.push_back(o.expires_year - (ep2.start_year + traced.years));
                        break;
                    }
            }
        }

        // BL-953: the live preference at the traced run's own close, derived
        // once, for reading 6's want-divergence count and reading 10's live line.
        const std::vector<culture_good_preference> live_prefs = derive_culture_preference(
            ss_copy.regions, traced.contacts, traced.polities,
            static_cast<int>(cs_copy.cultures.size()));
        row.live_pref_entries_1660 = static_cast<int64_t>(live_prefs.size());
        const auto top_want = [&](int polity_id) {
            const region_class goods[4] = { region_class::farm, region_class::ore,
                                            region_class::energy, region_class::port };
            int best = -1, best_q = 0;
            for (int g = 0; g < 4; ++g)
            {
                const int w = polity_good_want_q(ss_copy.regions, traced.polities, live_prefs,
                                                 polity_id, goods[g]);
                if (w > best_q) { best_q = w; best = g; } // ties keep the lower good index
            }
            return best;
        };

        for (const polity& q : traced.polities)
        {
            if (!q.alive) continue;
            ++row.alive_polities;
            if (q.overlord >= 0 && static_cast<std::size_t>(q.overlord) < traced.polities.size()
             && traced.polities[static_cast<std::size_t>(q.overlord)].alive)
            {
                ++row.subjects_alive;
                if (q.capital >= 0
                 && traced.polities[static_cast<std::size_t>(q.overlord)].capital >= 0
                 && static_cast<std::size_t>(q.capital) < ss_copy.regions.size()
                 && static_cast<std::size_t>(traced.polities[static_cast<std::size_t>(q.overlord)].capital)
                        < ss_copy.regions.size())
                {
                    const region& sc = ss_copy.regions[static_cast<std::size_t>(q.capital)];
                    const region& oc = ss_copy.regions[static_cast<std::size_t>(
                        traced.polities[static_cast<std::size_t>(q.overlord)].capital)];
                    int dc = std::abs(sc.col - oc.col), dr = std::abs(sc.row - oc.row);
                    row.max_subject_distance = std::max<int64_t>(row.max_subject_distance,
                                                                   std::max(dc, dr));
                }
                // Reading 6: does this subject know a THIRD polity its
                // overlord has never met?
                bool friction = false;
                for (const contact& c : traced.contacts)
                {
                    if (c.from != q.id) continue;
                    if (c.to == q.overlord) continue;
                    if (!contact_exists(traced.contacts, q.overlord, c.to)) { friction = true; break; }
                }
                if (friction) ++row.subjects_with_friction;
                if (top_want(q.id) != top_want(q.overlord)) ++row.subjects_with_divergent_top_want;
            }
        }
        {
            std::vector<bool> is_overlord(traced.polities.size(), false);
            for (const polity& q : traced.polities)
                if (q.alive && q.overlord >= 0 && static_cast<std::size_t>(q.overlord) < is_overlord.size())
                    is_overlord[static_cast<std::size_t>(q.overlord)] = true;
            for (bool v : is_overlord) if (v) ++row.overlords_alive;
        }
        {
            std::vector<int> expn_rank, cons_rank;
            exploration_lean_ranks(traced.polities, &cs_copy, expn_rank, cons_rank);
            for (std::size_t p = 0; p < traced.polities.size(); ++p)
            {
                const polity& q = traced.polities[p];
                if (!q.alive || q.navy_stock <= 0) continue;
                ++row.navy_holders;
                if (expn_rank[p] >= 500) ++row.navy_holders_expn_top;
                else                     ++row.navy_holders_expn_bottom;
            }
            row.port_steps = traced.port_steps_bought;
            row.navy_steps = traced.navy_steps_bought;
            row.army_steps = traced.army_steps_bought;
            for (uint8_t v : traced.navy_lapsed) if (v) ++row.navies_lapsed;

            const std::size_t np = traced.polities.size();
            std::vector<int64_t> held(np, 0), standing(np, 0), garrison(np, 0);
            for (const region& r : ss_copy.regions)
            {
                if (r.nation < 0 || static_cast<std::size_t>(r.nation) >= np) continue;
                const std::size_t n = static_cast<std::size_t>(r.nation);
                ++held[n];
                standing[n] += standing_army_heads(r);
                garrison[n] += garrison_target(r, ep2.garrison_fraction_q);
            }
            for (std::size_t p = 0; p < np; ++p)
            {
                const polity& q = traced.polities[p];
                if (!q.alive || held[p] <= 0) continue;
                row.standing_per_region.push_back(standing[p] / held[p]);
                row.garrison_per_region.push_back(garrison[p] / held[p]);
                if (standing[p] > 0) ++row.standing_holders;
            }
            row.standing_invariant_violations = traced.standing_army_invariant_violations;
            row.army_unpaid_rounds = traced.army_upkeep_unpaid_rounds;
            row.navy_unpaid_rounds = traced.navy_upkeep_unpaid_rounds;
            row.spent_army_upkeep  = traced.treasury_spent_on_army_upkeep;
            row.spent_navy_upkeep  = traced.treasury_spent_on_navy_upkeep;
            row.levy_raised        = traced.levy_heads_raised;
            row.levy_returned      = traced.levy_heads_returned;
        }

        // --- Reading 3 capture, off the traced re-run's 1660 close ---------
        // (`ss_copy`/`cs_copy` as the re-run left them).
        //
        // BL-951 — WHICH REALMS ARE "STRONGEST". Held region count favours
        // expansion by construction: a consolidator holds the same ground
        // worked harder, so a count of ground cannot see it. The PRIMARY
        // ranking is therefore the capital's `treasury` (EXPLORATION.md: the
        // treasury sits at the capital seat and is fed by what the polity
        // already holds and moves), tie-broken by mean `network_supply_q`
        // over held ground (how well the network feeds what it holds), then
        // by id. Region count is kept as a printed COMPARISON ranking.
        {
            const std::size_t np = traced.polities.size();
            std::vector<int64_t> region_count(np, 0), supply_sum(np, 0);
            for (const region& r : ss_copy.regions)
                if (r.nation >= 0 && static_cast<std::size_t>(r.nation) < np)
                {
                    ++region_count[static_cast<std::size_t>(r.nation)];
                    supply_sum[static_cast<std::size_t>(r.nation)] += r.network_supply_q;
                }

            std::vector<exploration_row::strength_entry> entries;
            for (std::size_t p = 0; p < np; ++p)
            {
                const polity& q = traced.polities[p];
                if (!q.alive) continue;
                exploration_row::strength_entry e;
                e.id      = static_cast<int>(p);
                e.regions = region_count[p];
                e.mean_supply_q = region_count[p] > 0
                    ? static_cast<int>(supply_sum[p] / region_count[p]) : 0;
                if (q.capital >= 0 && static_cast<std::size_t>(q.capital) < ss_copy.regions.size())
                    e.treasury = ss_copy.regions[static_cast<std::size_t>(q.capital)].treasury;
                if (q.culture >= 0 && static_cast<std::size_t>(q.culture) < cs_copy.cultures.size())
                {
                    const culture& cu = cs_copy.cultures[static_cast<std::size_t>(q.culture)];
                    e.cons_q = consolidator_lean_q(cu);
                    e.expn_q = expansion_lean_q(cu);
                }
                else
                {
                    e.cons_q = e.expn_q = -1; // no culture: neither lean, excluded below
                }
                entries.push_back(e);
            }

            if (entries.size() >= 2)
            {
                const std::size_t top_n = std::min<std::size_t>(3, entries.size());
                auto take_top = [&](std::vector<exploration_row::strength_entry> v,
                                    bool by_treasury,
                                    std::vector<exploration_row::strength_entry>& out,
                                    bool& has_cons, bool& has_expn) {
                    std::sort(v.begin(), v.end(), [&](const auto& a, const auto& b) {
                        if (by_treasury)
                        {
                            if (a.treasury != b.treasury) return a.treasury > b.treasury;
                            if (a.mean_supply_q != b.mean_supply_q) return a.mean_supply_q > b.mean_supply_q;
                        }
                        else if (a.regions != b.regions) return a.regions > b.regions;
                        return a.id < b.id; // explicit tie-break
                    });
                    for (std::size_t k = 0; k < top_n; ++k)
                    {
                        out.push_back(v[k]);
                        if (v[k].cons_q > v[k].expn_q)      has_cons = true;
                        else if (v[k].expn_q > v[k].cons_q) has_expn = true;
                    }
                };
                take_top(entries, true,  row.top_by_treasury,
                         row.top_has_consolidator, row.top_has_expansionist);
                take_top(entries, false, row.top_by_regions,
                         row.regions_top_has_consolidator, row.regions_top_has_expansionist);
                row.strength_measured = true;
            }
        }

        // --- BL-954 -- reading 11, off the traced re-run's final round ----
        {
            const std::vector<int32_t> mass =
                label_landmasses(fx.terrain.substrate, fx.gw, fx.gh);
            const auto mass_of_capital = [&](int pid) -> int32_t {
                if (pid < 0 || static_cast<std::size_t>(pid) >= traced.polities.size()) return -1;
                const int cap = traced.polities[static_cast<std::size_t>(pid)].capital;
                if (cap < 0 || static_cast<std::size_t>(cap) >= ss_copy.regions.size()) return -1;
                const region& rg = ss_copy.regions[static_cast<std::size_t>(cap)];
                if (rg.col < 0 || rg.row < 0 || rg.col >= fx.gw || rg.row >= fx.gh) return -1;
                return mass[static_cast<std::size_t>(rg.row * fx.gw + rg.col)];
            };

            std::vector<std::pair<uint16_t, uint16_t>> bound;
            for (const dated_object& o : traced.dated_objects)
                if (o.kind == static_cast<int32_t>(treaty_clause::trade_access))
                    bound.push_back({static_cast<uint16_t>(std::min(o.a, o.b)),
                                     static_cast<uint16_t>(std::max(o.a, o.b))});
            std::sort(bound.begin(), bound.end());
            bound.erase(std::unique(bound.begin(), bound.end()), bound.end());
            row.trade_bound_pairs = static_cast<int64_t>(bound.size());
            row.pair_volume.assign(bound.size(), 0);

            for (const trade_flow& f : traced.trade_flows)
            {
                ++row.flow_count;
                row.flow_volume += f.volume_q;
                const std::pair<uint16_t, uint16_t> key{std::min(f.seller, f.buyer),
                                                        std::max(f.seller, f.buyer)};
                const auto it = std::lower_bound(bound.begin(), bound.end(), key);
                if (it == bound.end() || *it != key) ++row.flows_without_clause;
                else row.pair_volume[static_cast<std::size_t>(it - bound.begin())] += f.volume_q;
                const int32_t ms = mass_of_capital(f.seller), mb = mass_of_capital(f.buyer);
                if (ms < 0 || mb < 0)  row.unknown_landmass_volume += f.volume_q;
                else if (ms != mb)     row.cross_landmass_volume   += f.volume_q;
            }
        }

        row.ok = true;
        rows.push_back(row);
    }

    // -----------------------------------------------------------------------
    // STRUCTURAL CHECKS
    // -----------------------------------------------------------------------
    std::printf("\n--- structural ---\n");
    int ran_count = 0;
    for (const exploration_row& r : rows) if (r.ok) ++ran_count;
    check(ran_count > 0, "at least one seed ran the Exploration span");

    bool all_traces_match = true;
    for (const exploration_row& r : rows)
        if (r.ok && !r.traced_matches_untraced) all_traces_match = false;
    check(all_traces_match,
        "traced re-run reproduces generation's own (untraced) battles/conquests/foundings, every ran seed");

    // -----------------------------------------------------------------------
    // THE SPREAD FACE (BL-971). Every aggregate a section prints is ALSO
    // assigned here, and the JSON writer at the end prints this struct and
    // `rows` — so the checked-in artefact carries exactly the numbers the
    // console did, never a second computation of them.
    // -----------------------------------------------------------------------
    struct spread_face
    {
        // readings 1-2
        bool    have_median = false, have_pooled = false, have_weighted = false;
        double  median = 0.0, pooled = 0.0, weighted = 0.0;
        int64_t sum_nb = 0, sum_fr = 0, weighted_battles = 0, no_nb_frontier_volume = 0;
        int64_t silent_floor = 0;
        int     seeds_ran = 0, seeds_silent = 0, seeds_displaced = 0, seeds_displaced_no_nb = 0, seeds_held = 0;
        double  med_empire_rate = 0.0, med_expl_rate = 0.0;
        int     ambiguous_total = 0;
        // BL-999 held-seed cause census: NO FRONTIER, NO EXPLORER, TREATY-CALM,
        // DETERRENCE-INERT, UNCLASSIFIED.
        int     held_cause[5] = {0, 0, 0, 0, 0};
        // reading 8
        int64_t r8_polities = 0, r8_treasury_min = 0, r8_treasury_max = 0;
        double  r8_treasury_mean = 0.0, r8_corr = 0.0;
        // reading 9
        int64_t r9_corridors = 0; int32_t r9_uses_min = 0, r9_uses_median = 0, r9_uses_max = 0;
        int64_t r9_post_roads = 0, r9_spent_roads = 0, r9_tier[4] = {0, 0, 0, 0}, r9_inherited_t3 = 0;
        int     r9_seeds_with_post_road = 0;
        // reading 4
        int64_t r4_formed = 0, r4_broken = 0, r4_blocked = 0, r4_standing = 0;
        int64_t r4_years_left_min = 0, r4_years_left_median = 0, r4_years_left_max = 0;
        // reading 5
        int64_t r5_subjects = 0, r5_overlords = 0, r5_alive = 0, r5_formed = 0, r5_freed = 0,
                r5_tribute = 0, r5_max_distance = 0;
        // reading 6
        int64_t r6_friction = 0, r6_divergent = 0;
        // reading 7
        int64_t r7_navy_holders = 0, r7_spent_ports = 0, r7_spent_navies = 0, r7_spent_armies = 0,
                r7_navy_top = 0, r7_navy_bottom = 0, r7_port_steps = 0, r7_navy_steps = 0,
                r7_army_steps = 0, r7_lapsed = 0, r7_standing_holders = 0,
                r7_army_unpaid = 0, r7_navy_unpaid = 0, r7_army_upkeep = 0, r7_navy_upkeep = 0,
                r7_levy_raised = 0, r7_levy_returned = 0, // BL-972
                r7_violations = 0;
        // reading 3
        int64_t r3_measured = 0, r3_cons = 0, r3_expn = 0, r3_both = 0,
                r3_reg_cons = 0, r3_reg_expn = 0, r3_reg_both = 0;
        // reading 10
        int64_t r10_live_entries = 0, r10_entries = 0, r10_seeds_with_spread = 0;
        int     r10_w_min = 0, r10_w_median = 0, r10_w_max = 0;
        // reading 11
        int64_t r11_flows = 0, r11_volume = 0, r11_cross = 0, r11_unknown = 0, r11_orphan = 0,
                r11_pairs = 0, r11_zero_pairs = 0;
        double  r11_top_decile_share = 0.0;
    };
    spread_face face;

    // -----------------------------------------------------------------------
    // READINGS 1-2 — DISPLACEMENT AND CONFLICT-PERSISTENCE, THE ONE TEST.
    // -----------------------------------------------------------------------
    //
    // OPERATIONALIZATION, STATED EXPLICITLY (this is a judgement call, not a
    // literal reading off EXPLORATION.md, and it is flagged in NEEDS_REVIEW
    // as such): a battle's pair is a LONG-CONTACTED NEIGHBOUR if
    // `pre_exploration_contacts` (the directed contact table exactly as the
    // Empires round left it at 1200) already names the pair; it is a
    // NEWLY-CONTACTED, frontier pair otherwise — i.e. the two polities met
    // for the first time at some point during 1200-1660. The RATIO compares
    // the two resulting rates (per century) against EACH OTHER within the
    // span, rather than each independently against the Empires round's own
    // rate — the doc's "falls relative to" reads most naturally as a
    // within-span comparison, and the trap it exists to catch (a world that
    // "simply stopped fighting") is caught regardless of which comparison is
    // taken, because reading 2 (conflict persists, compared against the
    // Empires round explicitly) is read ALONGSIDE it, exactly as the doc
    // insists.
    //
    // THREE READINGS OF THE ONE RATIO (BL-971). A median of per-seed ratios
    // lets a seed that barely fought (three frontier battles against one
    // neighbour war reads as 3.0) count exactly as much as one that fought
    // a hundred times, and drops every zero-neighbour seed from the count
    // entirely — so the claim can be carried by silence, which is the doc's
    // own named failure. So the ratio is read three ways, on one face:
    //
    //   median   — over seeds with any neighbour war, as before (kept for
    //              continuity with the prose numbers recorded before this
    //              item; NOT the verdict line);
    //   pooled   — sum(frontier) / sum(neighbour) over every ran seed: one
    //              ratio over the spread's whole conflict volume, so a silent
    //              seed contributes exactly its handful of battles and no
    //              more, and a zero-neighbour seed's frontier battles land in
    //              the numerator rather than vanishing;
    //   weighted — each defined per-seed ratio weighted by that seed's own
    //              traced battle count (neighbour + frontier). Zero-neighbour
    //              seeds have no finite ratio and are excluded here; their
    //              frontier volume is printed beside it so the exclusion is
    //              visible, and the pooled reading already carries them.
    //
    // SILENCE IS NAMED, NOT FOLDED IN. A seed whose traced battle count over
    // the whole span sits below `kSilentFloorBattles` is reported SILENT: its
    // ratio is printed but it carries no verdict either way. A zero-neighbour
    // seed counts as displacement only when its frontier count alone clears
    // the same floor. The floor is chosen off the 16-seed table at the
    // authorised constants (deterrence_alarm_weight_q = 525): traced totals
    // there, sorted, run 4, 15, 32, 34, 35, 60, ... — seeds 14 and 9 sit
    // under 5 battles/century across 460 years (the two NR-867 named) and
    // every other seed is at 32+ (7/century or more). 20 sits in that gap:
    // ~4.3/century, above both silent seeds and well below the next. It is
    // a report threshold, not a sim constant — moving it changes what is
    // called silent, never what happened.
    constexpr int64_t kSilentFloorBattles = 20;

    struct displacement_seed
    {
        uint32_t    seed    = 0;
        int64_t     nb      = 0;
        int64_t     fr      = 0;
        int64_t     total   = 0;     ///< nb + fr, the seed's traced conflict volume.
        double      ratio   = -1.0;  ///< fr/nb; -1 when nb == 0.
        const char* verdict = "";    ///< SILENT | displaced | displaced(no-nb) | held
        const char* cause   = nullptr; ///< BL-999: set on held seeds only (see the census below).
    };
    std::vector<displacement_seed> disp;

    std::printf("\n--- readings 1-2 (read together) ---\n");
    std::printf("%-6s %10s %10s %8s %8s %8s %10s | %6s %6s %10s %10s %8s  %s\n",
                "seed", "emp.battl", "emp.yrs", "emp/cent",
                "expl.bat", "expl.yrs", "expl/cent",
                "nb", "front", "neigh/c", "front/c", "ratio", "verdict");
    std::vector<double> ratios;
    std::vector<double> empire_rates, expl_rates;
    int64_t sum_nb = 0, sum_fr = 0;
    double  weighted_num = 0.0;
    int64_t weighted_den = 0;
    int64_t no_nb_frontier_volume = 0; ///< frontier battles the weighted reading cannot see.
    int seeds_ran = 0, seeds_silent = 0, seeds_displaced = 0, seeds_displaced_no_nb = 0, seeds_held = 0;
    for (const exploration_row& r : rows)
    {
        if (!r.ok) continue;
        ++seeds_ran;
        const double empire_rate = per_century(r.empire_battles, r.empire_years);
        const double expl_rate   = per_century(r.expl_battles, r.expl_years);
        const double neigh_rate  = per_century(r.neighbour_wars, r.expl_years);
        const double front_rate  = per_century(r.frontier_skirmishes, r.expl_years);
        const double ratio       = neigh_rate > 0.0 ? front_rate / neigh_rate : -1.0; // -1 => no neighbour wars at all

        displacement_seed d;
        d.seed  = r.seed;
        d.nb    = r.neighbour_wars;
        d.fr    = r.frontier_skirmishes;
        d.total = d.nb + d.fr;
        d.ratio = ratio;
        if (d.total < kSilentFloorBattles)      { d.verdict = "SILENT";            ++seeds_silent; }
        else if (d.nb == 0)                     { d.verdict = "displaced(no-nb)";  ++seeds_displaced_no_nb; }
        else if (ratio > 1.0)                   { d.verdict = "displaced";         ++seeds_displaced; }
        else                                    { d.verdict = "held";              ++seeds_held; }
        disp.push_back(d);

        empire_rates.push_back(empire_rate);
        expl_rates.push_back(expl_rate);
        sum_nb += d.nb;
        sum_fr += d.fr;
        if (ratio >= 0.0)
        {
            ratios.push_back(ratio);
            weighted_num += ratio * static_cast<double>(d.total);
            weighted_den += d.total;
        }
        else
        {
            no_nb_frontier_volume += d.fr;
        }

        std::printf("%-6u %10lld %10lld %8.2f %8lld %8lld %10.2f | %6lld %6lld %10.2f %10.2f %8s  %s\n",
                    r.seed,
                    static_cast<long long>(r.empire_battles), static_cast<long long>(r.empire_years), empire_rate,
                    static_cast<long long>(r.expl_battles), static_cast<long long>(r.expl_years), expl_rate,
                    static_cast<long long>(d.nb), static_cast<long long>(d.fr),
                    neigh_rate, front_rate,
                    ratio >= 0.0 ? (std::to_string(ratio).substr(0, 6)).c_str() : "no-nb",
                    d.verdict);
    }

    // The three readings, each nullable on the face when its denominator is
    // empty (undefined is not zero — the pre-BL-971 text already insisted).
    bool   have_median = false, have_pooled = false, have_weighted = false;
    double median_ratio = 0.0, pooled_ratio = 0.0, weighted_ratio = 0.0;
    if (!ratios.empty())
    {
        std::sort(ratios.begin(), ratios.end());
        median_ratio = ratios[ratios.size() / 2];
        have_median  = true;
    }
    if (sum_nb > 0)
    {
        pooled_ratio = static_cast<double>(sum_fr) / static_cast<double>(sum_nb);
        have_pooled  = true;
    }
    if (weighted_den > 0)
    {
        weighted_ratio = weighted_num / static_cast<double>(weighted_den);
        have_weighted  = true;
    }

    std::printf("\n  DISPLACEMENT (frontier / neighbour), three readings over %d ran seeds:\n", seeds_ran);
    if (have_pooled)
        std::printf("    pooled   sum(frontier)=%lld / sum(neighbour)=%lld = %.2f   <- the verdict line\n",
                    static_cast<long long>(sum_fr), static_cast<long long>(sum_nb), pooled_ratio);
    else
        std::printf("    pooled   UNDEFINED: no seed fought a single neighbour war (frontier total %lld)\n",
                    static_cast<long long>(sum_fr));
    if (have_weighted)
        std::printf("    weighted by each seed's traced battles, over %zu seeds with a defined ratio "
                    "(%lld battles): %.2f   (%lld frontier battles on zero-neighbour seeds not visible here)\n",
                    ratios.size(), static_cast<long long>(weighted_den), weighted_ratio,
                    static_cast<long long>(no_nb_frontier_volume));
    else
        std::printf("    weighted UNDEFINED: no seed carries a finite ratio\n");
    if (have_median)
        std::printf("    median   across %zu seeds with any neighbour war: %.2f   (kept beside; NOT the verdict)\n",
                    ratios.size(), median_ratio);
    else
        std::printf("    median   UNDEFINED: no seed fought a single neighbour war\n");
    std::printf("  seeds: displaced=%d displaced(no-nb, frontier >= %lld)=%d held=%d SILENT(< %lld traced battles)=%d",
                seeds_displaced, static_cast<long long>(kSilentFloorBattles), seeds_displaced_no_nb,
                seeds_held, static_cast<long long>(kSilentFloorBattles), seeds_silent);
    if (seeds_silent > 0)
    {
        std::printf("  silent:");
        for (const displacement_seed& d : disp)
            if (std::strcmp(d.verdict, "SILENT") == 0) std::printf(" %u", d.seed);
    }
    std::printf("\n");
    if (have_pooled)
        std::printf("  %s\n", pooled_ratio > 1.0
            ? "pooled > 1: frontier-skirmish volume exceeds neighbour-war volume — displacement, not mere calming."
            : "pooled <= 1: neighbour-war volume still leads — no displacement measured on this spread.");
    else
        std::printf("  the ratio is undefined, not zero. Report this spread's raw counts to Ben rather "
                    "than reading a ratio into an empty denominator.\n");
    if (have_pooled && have_weighted && ((pooled_ratio > 1.0) != (weighted_ratio > 1.0)))
        std::printf("  NOTE: pooled and weighted readings DISAGREE on the 1.0 line — a few heavy seeds "
                    "carry the pooled figure; read the per-seed verdicts.\n");

    face.have_median = have_median; face.have_pooled = have_pooled; face.have_weighted = have_weighted;
    face.median = median_ratio; face.pooled = pooled_ratio; face.weighted = weighted_ratio;
    face.sum_nb = sum_nb; face.sum_fr = sum_fr; face.weighted_battles = weighted_den;
    face.no_nb_frontier_volume = no_nb_frontier_volume;
    face.silent_floor = kSilentFloorBattles;
    face.seeds_ran = seeds_ran; face.seeds_silent = seeds_silent; face.seeds_displaced = seeds_displaced;
    face.seeds_displaced_no_nb = seeds_displaced_no_nb; face.seeds_held = seeds_held;

    // -----------------------------------------------------------------------
    // THE HELD-SEED CAUSE CENSUS (BL-999, Ben's ruling 2026-09-15, NR-873):
    // every seed readings 1-2 call HELD carries a NAMED CAUSE before any
    // constant moves. Report only; nothing here asserts.
    //
    // WHAT EACH COLUMN IS. Deterrence in this sim acts through ONE channel:
    // near-home Alarm raises `treaty_value_q`, a bound pair holds a
    // non-aggression clause, and a campaign against a bound owner is
    // treaty-blocked at the funnel's first gate. So "refused by deterrence"
    // is read as NEAR-class candidates (owner met before the span) that the
    // clause blocked, against those allowed past it. The FRONTIER is two
    // classes, printed apart because the sim makes them differently: a
    // contact is raised ONLY by a campaign crossing onto the other's ground
    // (`raise_contact`'s two call sites) or inherited from a conquest, so the
    // MET-IN-SPAN class exists only after an UNMET candidate was chosen once
    // -- the first crossing. "In reach" is what cleared the treaty, water and
    // reach gates and was actually scored. The rim is the exploration tree's
    // rim node. The want table is derived at the close; its outward share is
    // the doc's own "outward, across water" -- holder on another landmass
    // from the wanter's capital. The alarm read is a 1660 snapshot: the sim
    // keeps no cumulative alarm counter. GRAINS DIFFER along the funnel, as
    // `campaign_class_trace`'s own comment says: examined / blocked / wet /
    // reach-denied are candidate-grain, "cleared" is SEASON-grain (up to two
    // scores per candidate, so it can exceed "in reach"), "chosen" is
    // round-grain (one per round Campaign won, by the winner's class).
    //
    // THE LADDER, in the item's order, with its report thresholds
    // (thresholds on the READING, like kSilentFloorBattles -- moving them
    // changes what a seed is called, never what happened):
    //   NO FRONTIER       the span raised fewer than kFrontierPairsFloor new
    //                     contact pairs -- nothing was met to displace to;
    //   NO EXPLORER       no polity ever held the rim;
    //   TREATY-CALM       neighbours bound (>= kBoundShareCalm of near
    //                     candidates treaty-blocked) and fewer than
    //                     kFrontierReachFloor met-in-span candidates in reach;
    //   DETERRENCE-INERT  met-in-span candidates in reach at or over the floor
    //                     and neighbours NOT bound -- Alarm did not quiet them.
    // A seed with a reachable frontier AND bound neighbours that is still
    // held fits none of the four -- its remaining unbound near pairs carry
    // the war -- and is printed UNCLASSIFIED rather than forced into one.
    constexpr int64_t kFrontierPairsFloor = 2;    ///< met-in-span pairs at 1660.
    constexpr int64_t kFrontierReachFloor = 100;  ///< met-in-span candidates scored over the span.
    constexpr double  kBoundShareCalm     = 0.5;  ///< near candidates treaty-blocked / examined.

    struct held_cause_census { int no_frontier = 0, no_explorer = 0, treaty_calm = 0,
                                   deterrence_inert = 0, unclassified = 0; } census;
    std::printf("\n--- held-seed causes (BL-999) -- report thresholds: NO FRONTIER under %lld met-in-span pairs, "
                "frontier reach floor %lld scored met-in-span candidates, neighbours bound at >= %.0f%% blocked ---\n",
                static_cast<long long>(kFrontierPairsFloor), static_cast<long long>(kFrontierReachFloor),
                kBoundShareCalm * 100.0);
    for (displacement_seed& d : disp)
    {
        if (std::strcmp(d.verdict, "held") != 0) continue;
        const exploration_row* rp = nullptr;
        for (const exploration_row& r : rows) if (r.ok && r.seed == d.seed) { rp = &r; break; }
        if (!rp) continue;
        const exploration_row& r = *rp;
        const int64_t* near_c  = r.campaign_class[0];
        const int64_t* met_c   = r.campaign_class[1];
        const int64_t* unmet_c = r.campaign_class[2];
        const auto in_reach = [](const int64_t* c) { return c[0] - c[1] - c[2] - c[3]; };
        const int64_t near_allowed  = near_c[0] - near_c[1];
        const int64_t near_in_reach = in_reach(near_c);
        const int64_t met_in_reach  = in_reach(met_c);
        const int64_t unmet_in_reach = in_reach(unmet_c);
        const auto pct = [](int64_t num, int64_t den) {
            return den > 0 ? 100.0 * static_cast<double>(num) / static_cast<double>(den) : 0.0;
        };
        const double blocked_share  = pct(near_c[1], near_c[0]) / 100.0;
        const bool neighbours_bound   = blocked_share >= kBoundShareCalm;
        const bool frontier_reachable = met_in_reach >= kFrontierReachFloor;

        if      (r.far_pairs < kFrontierPairsFloor)          { d.cause = "NO FRONTIER";      ++census.no_frontier; }
        else if (r.rim_holders_ever == 0)                   { d.cause = "NO EXPLORER";      ++census.no_explorer; }
        else if (!frontier_reachable && neighbours_bound)   { d.cause = "TREATY-CALM";      ++census.treaty_calm; }
        else if (!frontier_reachable)                       { d.cause = "NO FRONTIER";      ++census.no_frontier; }
        else if (!neighbours_bound)                         { d.cause = "DETERRENCE-INERT"; ++census.deterrence_inert; }
        else                                                { d.cause = "UNCLASSIFIED";     ++census.unclassified; }

        std::printf("  seed %-3u nb=%lld fr=%lld  %s%s\n",
                    d.seed, static_cast<long long>(d.nb), static_cast<long long>(d.fr), d.cause,
                    std::strcmp(d.cause, "UNCLASSIFIED") == 0
                        ? " (frontier reachable AND neighbours bound; the unbound near pairs carry the war)" : "");
        std::printf("    neighbour campaigns: examined %lld, refused by deterrence (treaty-blocked) %lld (%.1f%%), "
                    "allowed %lld -> in reach %lld -> cleared %lld -> chosen %lld\n",
                    static_cast<long long>(near_c[0]), static_cast<long long>(near_c[1]), pct(near_c[1], near_c[0]),
                    static_cast<long long>(near_allowed), static_cast<long long>(near_in_reach),
                    static_cast<long long>(near_c[4]), static_cast<long long>(near_c[5]));
        std::printf("    pairs at 1660: near %lld, bound %lld (%.1f%%), unbound %lld; met-in-span %lld, bound %lld (%.1f%%); "
                    "near pairs reading alarm > 0: %lld/%lld (max alarm %d)\n",
                    static_cast<long long>(r.near_pairs), static_cast<long long>(r.near_bound),
                    pct(r.near_bound, r.near_pairs), static_cast<long long>(r.near_pairs - r.near_bound),
                    static_cast<long long>(r.far_pairs), static_cast<long long>(r.far_bound),
                    pct(r.far_bound, r.far_pairs),
                    static_cast<long long>(r.near_pairs_alarmed), static_cast<long long>(r.near_pairs),
                    r.near_alarm_max_q);
        std::printf("    explorer rim: %lld polities ever held it (%lld alive at 1660)\n",
                    static_cast<long long>(r.rim_holders_ever), static_cast<long long>(r.rim_holders_alive));
        std::printf("    frontier, met-in-span: examined %lld, treaty-blocked %lld, water-illegal %lld, reach-denied %lld, "
                    "IN REACH %lld, cleared %lld, chosen %lld\n",
                    static_cast<long long>(met_c[0]), static_cast<long long>(met_c[1]), static_cast<long long>(met_c[2]),
                    static_cast<long long>(met_c[3]), static_cast<long long>(met_in_reach),
                    static_cast<long long>(met_c[4]), static_cast<long long>(met_c[5]));
        std::printf("    frontier, unmet (the first crossing): examined %lld, water-illegal %lld, reach-denied %lld, "
                    "IN REACH %lld, cleared %lld, chosen %lld\n",
                    static_cast<long long>(unmet_c[0]), static_cast<long long>(unmet_c[2]),
                    static_cast<long long>(unmet_c[3]), static_cast<long long>(unmet_in_reach),
                    static_cast<long long>(unmet_c[4]), static_cast<long long>(unmet_c[5]));
        std::printf("    want table at 1660: %lld wants, outward (across water) %lld (%.1f%%), "
                    "pointing at a met-in-span polity %lld (%.1f%%)\n",
                    static_cast<long long>(r.wants_total), static_cast<long long>(r.wants_outward),
                    pct(r.wants_outward, r.wants_total),
                    static_cast<long long>(r.wants_frontier), pct(r.wants_frontier, r.wants_total));
    }
    if (seeds_held == 0)
        std::printf("  no held seed on this spread.\n");
    else
        std::printf("  census: NO FRONTIER=%d NO EXPLORER=%d TREATY-CALM=%d DETERRENCE-INERT=%d UNCLASSIFIED=%d\n",
                    census.no_frontier, census.no_explorer, census.treaty_calm,
                    census.deterrence_inert, census.unclassified);
    face.held_cause[0] = census.no_frontier;      face.held_cause[1] = census.no_explorer;
    face.held_cause[2] = census.treaty_calm;      face.held_cause[3] = census.deterrence_inert;
    face.held_cause[4] = census.unclassified;

    double med_empire = 0.0, med_expl = 0.0;
    if (!empire_rates.empty())
    {
        std::sort(empire_rates.begin(), empire_rates.end());
        std::sort(expl_rates.begin(), expl_rates.end());
        med_empire = empire_rates[empire_rates.size() / 2];
        med_expl   = expl_rates[expl_rates.size() / 2];
        face.med_empire_rate = med_empire; face.med_expl_rate = med_expl;
        std::printf("\n  CONFLICT PERSISTS: median Empires battle rate %.2f/century vs median "
                    "Exploration battle rate %.2f/century.\n", med_empire, med_expl);
        std::printf("  %s\n",
            (med_expl < med_empire && med_expl > 0.05)
                ? "Below Empires' and not near zero — the handoff's own bar."
                : (med_expl <= 0.05
                    ? "NEAR ZERO — a frozen map. This is the named failure mode."
                    : "NOT below Empires' rate — conflict did not fall at all."));
    }

    int ambiguous_total = 0;
    for (const exploration_row& r : rows) ambiguous_total += static_cast<int>(r.ambiguous_battles);
    face.ambiguous_total = ambiguous_total;
    if (ambiguous_total > 0)
        std::printf("\n  NOTE: %d battle(s) traced with BOTH attacker and defender id 0 across the "
                    "spread, excluded from both counts above (battle_trace's own comment: defender "
                    "0 is the sentinel for \"unowned\", indistinguishable from a real polity id 0 — "
                    "a pre-existing ambiguity in the struct, not introduced here).\n", ambiguous_total);

    // -----------------------------------------------------------------------
    // READING 8 — TREASURY SPREAD VS. INHERITED CORRIDOR TOUCH (BL-932/940).
    // -----------------------------------------------------------------------
    {
        std::vector<std::pair<int64_t, int64_t>> pairs; // (treasury, corridor_touch)
        for (const exploration_row& r : rows)
            for (const auto& tc : r.polity_treasury_corridor) pairs.push_back(tc);

        std::printf("\n--- reading 8: treasury vs. inherited corridor touch ---\n");
        if (pairs.size() < 2)
        {
            std::printf("  fewer than two living polities across the spread — no spread to read.\n");
        }
        else
        {
            int64_t min_t = pairs[0].first, max_t = pairs[0].first, sum_t = 0;
            for (const auto& p : pairs) { min_t = std::min(min_t, p.first);
                                          max_t = std::max(max_t, p.first); sum_t += p.first; }
            const double mean_t = static_cast<double>(sum_t) / static_cast<double>(pairs.size());

            // Pearson correlation, integer inputs, double accumulation --
            // a report figure, not a decision input, so float is fine here.
            double mean_c = 0.0;
            for (const auto& p : pairs) mean_c += static_cast<double>(p.second);
            mean_c /= static_cast<double>(pairs.size());
            double cov = 0.0, var_t = 0.0, var_c = 0.0;
            for (const auto& p : pairs)
            {
                const double dt = static_cast<double>(p.first)  - mean_t;
                const double dc = static_cast<double>(p.second) - mean_c;
                cov += dt * dc; var_t += dt * dt; var_c += dc * dc;
            }
            const double corr = (var_t > 0.0 && var_c > 0.0) ? cov / std::sqrt(var_t * var_c) : 0.0;
            face.r8_polities = static_cast<int64_t>(pairs.size());
            face.r8_treasury_min = min_t; face.r8_treasury_max = max_t; face.r8_treasury_mean = mean_t;
            face.r8_corr = corr;

            std::printf("  %zu living polities across the spread: treasury min=%lld max=%lld mean=%.1f\n",
                        pairs.size(), static_cast<long long>(min_t), static_cast<long long>(max_t), mean_t);
            std::printf("  correlation(treasury, corridor-touch) = %.3f\n", corr);
            std::printf("  %s\n", corr > 0.2
                ? "positive — treasury tracks the inherited/grown corridor network, as the done-when criterion asks."
                : "not clearly positive on this spread — report to Ben rather than re-tuning the formula silently.");
        }
    }

    // -----------------------------------------------------------------------
    // READING 9 — THROUGHPUT: corridors carrying materially different
    // volumes, with the road ladder visible in the difference (BL-940).
    // Read off each seed's own (untraced, real) exploration run, captured
    // onto the row in the main loop.
    // -----------------------------------------------------------------------
    {
        std::vector<int32_t> uses;
        int64_t total_post_roads_built = 0, total_treasury_spent = 0;
        int seeds_with_post_road = 0;
        int64_t tiers[4] = {0, 0, 0, 0};
        int64_t inherited_t3 = 0;
        for (const exploration_row& r : rows)
        {
            if (!r.ok) continue;
            for (int t = 0; t < 4; ++t) tiers[t] += r.tier_count[t];
            inherited_t3 += r.inherited_traffic_tier3;
            uses.insert(uses.end(), r.corridor_uses.begin(), r.corridor_uses.end());
            if (r.post_roads_built > 0) ++seeds_with_post_road;
            total_post_roads_built += r.post_roads_built;
            total_treasury_spent   += r.treasury_spent_on_roads;
        }

        std::printf("\n--- reading 9: corridor throughput and the road ladder's third rung ---\n");
        if (uses.empty())
        {
            std::printf("  no seed in this spread produced a single corridor — no spread to read.\n");
        }
        else
        {
            std::sort(uses.begin(), uses.end());
            const int32_t min_u = uses.front(), max_u = uses.back();
            const int32_t med_u = uses[uses.size() / 2];
            face.r9_corridors = static_cast<int64_t>(uses.size());
            face.r9_uses_min = min_u; face.r9_uses_median = med_u; face.r9_uses_max = max_u;
            face.r9_post_roads = total_post_roads_built; face.r9_spent_roads = total_treasury_spent;
            face.r9_seeds_with_post_road = seeds_with_post_road;
            for (int t = 0; t < 4; ++t) face.r9_tier[t] = tiers[t];
            face.r9_inherited_t3 = inherited_t3;
            std::printf("  %zu corridors across the spread: uses min=%d median=%d max=%d\n",
                        uses.size(), min_u, med_u, max_u);
            std::printf("  %s\n", (max_u > min_u)
                ? "corridors carry materially different volumes on this spread."
                : "every corridor carries the same volume — no spread measured.");
            std::printf("  post_roads_built total=%lld (in %d/%d seeds) treasury_spent_on_roads total=%lld\n",
                        static_cast<long long>(total_post_roads_built), seeds_with_post_road, seed_count,
                        static_cast<long long>(total_treasury_spent));
            std::printf("  carried rung at 1660 (history_corridor::tier): none=%lld track=%lld road=%lld post-road=%lld;"
                        " inherited 1200 corridors whose traffic alone reaches road_tier3_uses=%lld\n",
                        static_cast<long long>(tiers[0]), static_cast<long long>(tiers[1]),
                        static_cast<long long>(tiers[2]), static_cast<long long>(tiers[3]),
                        static_cast<long long>(inherited_t3));
            std::printf("  per seed post_roads_built:");
            for (const exploration_row& r : rows)
                if (r.ok) std::printf(" %lld", static_cast<long long>(r.post_roads_built));
            std::printf("\n");
            std::printf("  %s\n", total_post_roads_built > 0
                ? "the road ladder's third rung fired at least once on this spread."
                : "the third rung never fired on this spread — report to Ben rather than "
                  "re-tuning post_road_treasury_cost silently.");
        }
    }

    // -----------------------------------------------------------------------
    // READING 4 — TREATY DEPTH (BL-933).
    // -----------------------------------------------------------------------
    std::printf("\n--- reading 4: treaty depth ---\n");
    {
        int64_t formed = 0, broken = 0, blocked_campaigns = 0;
        std::vector<int64_t> years_left;
        for (const exploration_row& r : rows)
        {
            if (!r.ok) continue;
            formed += r.treaties_formed_total;
            broken += r.treaties_broken_total;
            blocked_campaigns += r.treaty_blocked_campaigns;
            for (int64_t yl : r.treaty_years_left) years_left.push_back(yl);
        }
        std::printf("  treaties formed=%lld broken=%lld  non-aggression-blocked campaigns=%lld  "
                    "(across %d seeds)\n",
                    static_cast<long long>(formed), static_cast<long long>(broken),
                    static_cast<long long>(blocked_campaigns), seed_count);
        face.r4_formed = formed; face.r4_broken = broken; face.r4_blocked = blocked_campaigns;
        face.r4_standing = static_cast<int64_t>(years_left.size());
        if (!years_left.empty())
        {
            std::vector<int64_t> yl = years_left;
            std::sort(yl.begin(), yl.end());
            face.r4_years_left_min = yl.front(); face.r4_years_left_median = yl[yl.size() / 2];
            face.r4_years_left_max = yl.back();
        }
        if (years_left.empty())
        {
            std::printf("  no treaty stood at 1660 on this spread — NOT MEASURED (or the "
                        "formation threshold sat too high; see treaty_formation_threshold_q).\n");
        }
        else
        {
            std::sort(years_left.begin(), years_left.end());
            std::printf("  %zu standing at 1660, years-left min=%lld median=%lld max=%lld\n",
                        years_left.size(), static_cast<long long>(years_left.front()),
                        static_cast<long long>(years_left[years_left.size() / 2]),
                        static_cast<long long>(years_left.back()));
            std::printf("  %s\n", years_left.size() > 0 && broken > 0
                ? "treaties both stand AND break on this spread — depth without a frozen map."
                : "treaties stand but none broke on this spread — report to Ben, do not re-tune silently.");
        }
    }

    // -----------------------------------------------------------------------
    // READING 5 — COLONIAL ASYMMETRY (BL-934).
    // -----------------------------------------------------------------------
    std::printf("\n--- reading 5: colonial asymmetry ---\n");
    {
        int64_t subjects = 0, overlords = 0, alive = 0, formed = 0, freed = 0, tribute = 0;
        int64_t max_dist = 0;
        for (const exploration_row& r : rows)
        {
            if (!r.ok) continue;
            subjects += r.subjects_alive; overlords += r.overlords_alive; alive += r.alive_polities;
            formed += r.subjections_formed; freed += r.subjections_freed; tribute += r.tribute_remitted;
            max_dist = std::max(max_dist, r.max_subject_distance);
        }
        face.r5_subjects = subjects; face.r5_overlords = overlords; face.r5_alive = alive;
        face.r5_formed = formed; face.r5_freed = freed; face.r5_tribute = tribute;
        face.r5_max_distance = max_dist;
        std::printf("  subjects=%lld overlords=%lld alive-polities=%lld (formed=%lld freed=%lld) "
                    "tribute remitted total=%lld  max subject-overlord distance=%lld\n",
                    static_cast<long long>(subjects), static_cast<long long>(overlords),
                    static_cast<long long>(alive), static_cast<long long>(formed),
                    static_cast<long long>(freed), static_cast<long long>(tribute),
                    static_cast<long long>(max_dist));
        std::printf("  %s\n", (subjects > 0 && subjects < alive)
            ? "some polities hold subjects, most do not — asymmetry measured."
            : "no asymmetry measured on this spread — report to Ben (subjection_reach_q / "
              "subjection_treasury_margin_q may sit wrong for this seed spread).");
    }

    // -----------------------------------------------------------------------
    // READING 6 — SUBJECT FRICTION (BL-934), OPERATIONALIZED.
    // -----------------------------------------------------------------------
    //
    // No live want table is maintained during the span (§ file header); a
    // subject's CONTACT graph diverging from its overlord's is read as the
    // proxy, since contact is the want table's own precondition
    // (CIVILISATION.md sec The directed want: "a want requires contact").
    // Flagged exactly as readings 1-2's own operationalization is.
    std::printf("\n--- reading 6: subject friction (operationalized as contact-graph divergence) ---\n");
    {
        int64_t friction = 0, divergent = 0, subjects = 0;
        for (const exploration_row& r : rows)
        {
            if (!r.ok) continue;
            friction  += r.subjects_with_friction;
            divergent += r.subjects_with_divergent_top_want;
            subjects  += r.subjects_alive;
        }
        face.r6_friction = friction; face.r6_divergent = divergent;
        std::printf("  subjects whose contact graph names a polity their overlord never met: %lld\n",
                    static_cast<long long>(friction));
        std::printf("  subjects whose top want (argmax good, live 1660 preference) differs from "
                    "their overlord's: %lld of %lld\n",
                    static_cast<long long>(divergent), static_cast<long long>(subjects));
        std::printf("  %s\n", friction > 0
            ? "at least one subject's own graph points somewhere its overlord's does not."
            : "no friction measured on this spread — NOT MEASURED, do not read as a failure "
              "without more seeds (this reading depends on subjects existing at all).");
    }

    // -----------------------------------------------------------------------
    // READING 7 — FLEETS (BL-935).
    // -----------------------------------------------------------------------
    std::printf("\n--- reading 7: fleets ---\n");
    {
        int64_t navy_holders = 0, spent_navies = 0, spent_ports = 0, spent_armies = 0;
        int64_t top = 0, bottom = 0, port_steps = 0, navy_steps = 0, army_steps = 0, lapsed = 0;
        for (const exploration_row& r : rows)
        {
            if (!r.ok) continue;
            navy_holders += r.navy_holders;
            spent_navies += r.treasury_spent_on_navies;
            spent_ports  += r.treasury_spent_on_ports;
            spent_armies += r.treasury_spent_on_standing_armies;
            top          += r.navy_holders_expn_top;
            bottom       += r.navy_holders_expn_bottom;
            port_steps   += r.port_steps;
            navy_steps   += r.navy_steps;
            army_steps   += r.army_steps;
            lapsed       += r.navies_lapsed;
        }
        face.r7_navy_holders = navy_holders; face.r7_spent_ports = spent_ports;
        face.r7_spent_navies = spent_navies; face.r7_spent_armies = spent_armies;
        face.r7_navy_top = top; face.r7_navy_bottom = bottom;
        face.r7_port_steps = port_steps; face.r7_navy_steps = navy_steps; face.r7_army_steps = army_steps;
        face.r7_lapsed = lapsed;
        std::printf("  polities holding a navy at 1660: %lld  treasury spent -- ports=%lld "
                    "navies=%lld standing armies=%lld\n",
                    static_cast<long long>(navy_holders), static_cast<long long>(spent_ports),
                    static_cast<long long>(spent_navies), static_cast<long long>(spent_armies));
        // BL-955 -- who holds the fleets, what was bought, and what was let go.
        std::printf("  navies at 1660 by holder's expansion rank (1660): top half (>=500)=%lld  "
                    "bottom half=%lld\n", static_cast<long long>(top), static_cast<long long>(bottom));
        std::printf("  stock steps bought over the span: port=%lld navy=%lld standing army=%lld\n",
                    static_cast<long long>(port_steps), static_cast<long long>(navy_steps),
                    static_cast<long long>(army_steps));
        std::printf("  polities that held a navy and let it decay to zero at least once: %lld\n",
                    static_cast<long long>(lapsed));
        {
            std::vector<int64_t> st, ga;
            int64_t holders = 0, living = 0, violations = 0;
            int64_t army_unpaid = 0, navy_unpaid = 0, army_upkeep = 0, navy_upkeep = 0,
                    levy_raised = 0, levy_returned = 0; // BL-972
            for (const exploration_row& r : rows)
            {
                if (!r.ok) continue;
                st.insert(st.end(), r.standing_per_region.begin(), r.standing_per_region.end());
                ga.insert(ga.end(), r.garrison_per_region.begin(), r.garrison_per_region.end());
                holders    += r.standing_holders;
                violations += r.standing_invariant_violations;
                army_unpaid += r.army_unpaid_rounds; navy_unpaid += r.navy_unpaid_rounds;
                army_upkeep += r.spent_army_upkeep;  navy_upkeep += r.spent_navy_upkeep;
                levy_raised += r.levy_raised;        levy_returned += r.levy_returned;
            }
            living = static_cast<int64_t>(st.size());
            face.r7_standing_holders = holders; face.r7_violations = violations;
            face.r7_army_unpaid = army_unpaid; face.r7_navy_unpaid = navy_unpaid;
            face.r7_army_upkeep = army_upkeep; face.r7_navy_upkeep = navy_upkeep;
            face.r7_levy_raised = levy_raised; face.r7_levy_returned = levy_returned;
            const auto pct = [](std::vector<int64_t> v, int p) -> long long {
                if (v.empty()) return 0;
                std::sort(v.begin(), v.end());
                return static_cast<long long>(v[(v.size() - 1) * static_cast<std::size_t>(p) / 100]);
            };
            std::printf("  paid standing army at 1660, heads per held region over %lld living polities: "
                        "p50=%lld p75=%lld p90=%lld max=%lld  (holders=%lld)\n",
                        static_cast<long long>(living), pct(st, 50), pct(st, 75), pct(st, 90),
                        pct(st, 100), static_cast<long long>(holders));
            // BL-972: the bill is the observable that replaced the cap.
            std::printf("  upkeep (BL-972): army bill paid=%lld navy bill paid=%lld  polity-rounds short: "
                        "army=%lld navy=%lld  levy raised=%lld returned=%lld heads\n",
                        static_cast<long long>(army_upkeep), static_cast<long long>(navy_upkeep),
                        static_cast<long long>(army_unpaid), static_cast<long long>(navy_unpaid),
                        static_cast<long long>(levy_raised), static_cast<long long>(levy_returned));
            std::printf("  paid standing army invariant violations (every round + close): %lld\n",
                        static_cast<long long>(violations));
            std::printf("  garrison_target per held region: p50=%lld p75=%lld p90=%lld max=%lld\n",
                        pct(ga, 50), pct(ga, 75), pct(ga, 90), pct(ga, 100));
        }
        std::printf("  %s\n", spent_navies > 0
            ? "at least one polity funded a navy on this spread (decay itself is confirmed by "
              "exploration_sim_harness R5.8/R5.9, not by this aggregate sweep)."
            : "no polity ever funded a navy on this spread — report to Ben rather than "
              "re-tuning navy_build_cost_q/navy_min_port_stock_q silently.");
    }

    // -----------------------------------------------------------------------
    // READING 3 — BOTH STRATEGIES PAY (BL-942). Consolidators AND
    // expansionists both among a seed's strongest realms, each traceable to
    // its creed's recorded deeds (`zeal`/`dominion`/`sea_legs_q`).
    // "Strongest" is ranked by CAPITAL TREASURY at 1660, tie-broken by mean
    // held-region `network_supply_q` (BL-951 — see the capture's comment for
    // why region count cannot see a consolidator). The region-count ranking
    // is printed beside it for comparison; the verdict line reads the
    // treasury ranking.
    // -----------------------------------------------------------------------
    std::printf("\n--- reading 3: both strategies pay (consolidator/expansionist, by creed) ---\n");
    std::printf("  metric: top-3 realms per seed ranked by CAPITAL TREASURY at 1660, tie-break mean "
                "held-region network_supply_q, then polity id (region-count ranking shown for comparison)\n");
    {
        auto lean_tag = [](const exploration_row::strength_entry& e) {
            return e.cons_q > e.expn_q ? "C" : (e.expn_q > e.cons_q ? "E" : "-");
        };
        auto print_top = [&](const char* label, const std::vector<exploration_row::strength_entry>& v) {
            std::printf("    %-9s", label);
            for (const auto& e : v)
                std::printf("  [p%d %s trs=%lld sup=%d reg=%lld cons=%d expn=%d]", e.id, lean_tag(e),
                            static_cast<long long>(e.treasury), e.mean_supply_q,
                            static_cast<long long>(e.regions), e.cons_q, e.expn_q);
            std::printf("\n");
        };

        int64_t seeds_measured = 0, seeds_with_consolidator_top = 0,
                seeds_with_expansionist_top = 0, seeds_with_both = 0;
        int64_t reg_cons = 0, reg_expn = 0, reg_both = 0;
        for (const exploration_row& r : rows)
        {
            if (!r.ok || !r.strength_measured) continue;
            ++seeds_measured;
            if (r.top_has_consolidator) ++seeds_with_consolidator_top;
            if (r.top_has_expansionist) ++seeds_with_expansionist_top;
            if (r.top_has_consolidator && r.top_has_expansionist) ++seeds_with_both;
            if (r.regions_top_has_consolidator) ++reg_cons;
            if (r.regions_top_has_expansionist) ++reg_expn;
            if (r.regions_top_has_consolidator && r.regions_top_has_expansionist) ++reg_both;

            std::printf("  seed %u (C = consolidator-leaning creed, E = expansionist, - = neither):\n", r.seed);
            print_top("treasury:", r.top_by_treasury);
            print_top("regions:",  r.top_by_regions);
        }
        face.r3_measured = seeds_measured; face.r3_cons = seeds_with_consolidator_top;
        face.r3_expn = seeds_with_expansionist_top; face.r3_both = seeds_with_both;
        face.r3_reg_cons = reg_cons; face.r3_reg_expn = reg_expn; face.r3_reg_both = reg_both;
        std::printf("  seeds measured=%lld  top-3-by-TREASURY include a consolidator-leaning "
                    "creed=%lld  include an expansionist-leaning creed=%lld  BOTH present=%lld\n",
                    static_cast<long long>(seeds_measured), static_cast<long long>(seeds_with_consolidator_top),
                    static_cast<long long>(seeds_with_expansionist_top), static_cast<long long>(seeds_with_both));
        std::printf("  (comparison) top-3-by-REGIONS include a consolidator-leaning creed=%lld  "
                    "include an expansionist-leaning creed=%lld  BOTH present=%lld\n",
                    static_cast<long long>(reg_cons), static_cast<long long>(reg_expn),
                    static_cast<long long>(reg_both));
        std::printf("  %s\n", seeds_with_both > 0
            ? "at least one seed's strongest realms include both a consolidator and an "
              "expansionist, each traceable to its own creed."
            : "no seed on this spread shows both strategies among its strongest realms -- "
              "report to Ben rather than forcing the mapping (EXPLORATION.md sec Two ways to be "
              "strong: if the creed axes do not separate them, the fix is upstream, not a flag here).");
    }

    // -----------------------------------------------------------------------
    // READING 10 — PREFERENCE (BL-936). Goods wanted differently by
    // different cultures, with the difference traceable to route. Read off
    // the PRE-EXPLORATION handoff state, captured onto the row in the main
    // loop (`derive_culture_preference` over the 1200 CE fixture).
    // -----------------------------------------------------------------------
    std::printf("\n--- reading 10: preference (goods wanted differently by culture, by route) ---\n");
    {
        int64_t live_entries = 0;
        for (const exploration_row& r : rows) if (r.ok) live_entries += r.live_pref_entries_1660;
        face.r10_live_entries = live_entries;
        std::printf("  LIVE at 1660 (the preference the span's scorer reads, off the traced run): "
                    "total (culture, good) entries=%lld\n", static_cast<long long>(live_entries));
    }
    {
        int64_t total_entries = 0, seeds_with_spread = 0, seeds_measured = 0;
        std::vector<int16_t> weights;
        for (const exploration_row& r : rows)
        {
            if (!r.ok) continue;
            ++seeds_measured;

            total_entries += static_cast<int64_t>(r.preference_weights.size());
            const std::vector<int16_t>& seed_weights = r.preference_weights;
            if (!seed_weights.empty())
            {
                const auto mm = std::minmax_element(seed_weights.begin(), seed_weights.end());
                if (*mm.second > *mm.first) ++seeds_with_spread;
            }
            weights.insert(weights.end(), seed_weights.begin(), seed_weights.end());
        }
        face.r10_entries = total_entries; face.r10_seeds_with_spread = seeds_with_spread;
        if (!weights.empty())
        {
            std::vector<int16_t> ws = weights;
            std::sort(ws.begin(), ws.end());
            face.r10_w_min = ws.front(); face.r10_w_median = ws[ws.size() / 2]; face.r10_w_max = ws.back();
        }
        std::printf("  seeds measured=%lld  total (culture, good) preference entries=%lld  "
                    "seeds with a non-uniform weight spread=%lld\n",
                    static_cast<long long>(seeds_measured), static_cast<long long>(total_entries),
                    static_cast<long long>(seeds_with_spread));
        if (weights.empty())
        {
            std::printf("  no (culture, good) preference entry derived on this spread -- NOT "
                        "MEASURED (no contact-exposed absence existed at 1200 CE on any seed).\n");
        }
        else
        {
            std::sort(weights.begin(), weights.end());
            std::printf("  weight_q across the spread: min=%d median=%d max=%d\n",
                        weights.front(), weights[weights.size() / 2], weights.back());
            std::printf("  %s\n", (weights.back() > weights.front())
                ? "goods are weighted differently by different cultures on this spread."
                : "every derived preference weighs the same -- no spread measured.");
        }
    }

    // -----------------------------------------------------------------------
    // READING 11 — TRADE (BL-954). "Flow crosses between polities on
    // different landmasses, unevenly — some pairs carry most of it, most
    // carry none — and every flow stands on a trade-access clause." Read off
    // the traced re-run's `trade_flows`, which hold the span's FINAL decision
    // round (flows are rebuilt each round, never accumulated).
    // -----------------------------------------------------------------------
    std::printf("\n--- reading 11: trade (flows at the final decision round) ---\n");
    {
        std::printf("%-6s %8s %10s %10s %10s %10s %12s\n",
                    "seed", "flows", "volume", "cross-mass", "unk-mass", "tr-pairs", "pairs-w/flow");
        int64_t flows = 0, volume = 0, cross = 0, unknown = 0, orphan = 0;
        std::vector<int64_t> pooled_pairs;
        for (const exploration_row& r : rows)
        {
            if (!r.ok) continue;
            int64_t with_flow = 0;
            for (int64_t v : r.pair_volume) if (v > 0) ++with_flow;
            std::printf("%-6u %8lld %10lld %10lld %10lld %10lld %12lld\n", r.seed,
                        static_cast<long long>(r.flow_count), static_cast<long long>(r.flow_volume),
                        static_cast<long long>(r.cross_landmass_volume),
                        static_cast<long long>(r.unknown_landmass_volume),
                        static_cast<long long>(r.trade_bound_pairs), static_cast<long long>(with_flow));
            flows += r.flow_count; volume += r.flow_volume; cross += r.cross_landmass_volume;
            unknown += r.unknown_landmass_volume; orphan += r.flows_without_clause;
            pooled_pairs.insert(pooled_pairs.end(), r.pair_volume.begin(), r.pair_volume.end());
        }
        face.r11_flows = flows; face.r11_volume = volume; face.r11_cross = cross;
        face.r11_unknown = unknown; face.r11_orphan = orphan;
        face.r11_pairs = static_cast<int64_t>(pooled_pairs.size());
        std::printf("  total: flows=%lld volume=%lld  cross-landmass share=%.3f  "
                    "(capital-on-water, landmass undefined: %.3f)\n",
                    static_cast<long long>(flows), static_cast<long long>(volume),
                    volume > 0 ? static_cast<double>(cross) / static_cast<double>(volume) : 0.0,
                    volume > 0 ? static_cast<double>(unknown) / static_cast<double>(volume) : 0.0);

        if (!pooled_pairs.empty() && volume > 0)
        {
            std::sort(pooled_pairs.begin(), pooled_pairs.end(), std::greater<int64_t>());
            const std::size_t decile = std::max<std::size_t>(1, (pooled_pairs.size() + 9) / 10);
            int64_t top = 0, zero = 0;
            for (std::size_t k = 0; k < decile; ++k) top += pooled_pairs[k];
            for (int64_t v : pooled_pairs) if (v == 0) ++zero;
            face.r11_top_decile_share = static_cast<double>(top) / static_cast<double>(volume);
            face.r11_zero_pairs = zero;
            std::printf("  %zu trade-access pairs pooled: top-decile (%zu pairs) share of volume=%.3f  "
                        "pairs carrying none=%lld (%.3f)\n",
                        pooled_pairs.size(), decile,
                        static_cast<double>(top) / static_cast<double>(volume),
                        static_cast<long long>(zero),
                        static_cast<double>(zero) / static_cast<double>(pooled_pairs.size()));
        }
        else
        {
            std::printf("  no trade volume on this spread -- NOT MEASURED; report to Ben rather than "
                        "re-tuning a weight silently.\n");
        }
        std::printf("  flows whose pair holds no trade_access clause at 1660: %lld\n",
                    static_cast<long long>(orphan));
        check(orphan == 0, "reading 11: every flow stands on a trade_access clause");
    }

    // -----------------------------------------------------------------------
    // THE CHECKED-IN TABLE (BL-971). exploration_sweep.json at the working
    // directory (the repo root when run as documented), one row per seed
    // over all eleven readings plus the spread face above — the same spirit
    // as history_sweep.json, so the numbers survive as data rather than as
    // prose in a devlog. A tuning run (--w_want_q / --set) writes
    // exploration_sweep.tuning.json instead, so the committed artefact
    // always describes generation's own constants.
    // -----------------------------------------------------------------------
    {
        const char* json_path = want_override ? "exploration_sweep.tuning.json" : "exploration_sweep.json";
        FILE* f = std::fopen(json_path, "w");
        if (!f)
        {
            std::printf("\nCould not write %s\n", json_path);
        }
        else
        {
            // Small helpers: nullable doubles, and min/median/max over a
            // sorted copy of an integer vector (empty -> nulls).
            const auto put_d = [&](const char* key, bool have, double v, const char* tail) {
                if (have) std::fprintf(f, "\"%s\": %.4f%s", key, v, tail);
                else      std::fprintf(f, "\"%s\": null%s", key, tail);
            };
            const auto put_mmm = [&](const char* prefix, std::vector<int64_t> v, const char* tail) {
                if (v.empty())
                {
                    std::fprintf(f, "\"%s_min\": null, \"%s_median\": null, \"%s_max\": null%s",
                                 prefix, prefix, prefix, tail);
                    return;
                }
                std::sort(v.begin(), v.end());
                std::fprintf(f, "\"%s_min\": %lld, \"%s_median\": %lld, \"%s_max\": %lld%s",
                             prefix, static_cast<long long>(v.front()),
                             prefix, static_cast<long long>(v[v.size() / 2]),
                             prefix, static_cast<long long>(v.back()), tail);
            };
            const auto top3 = [&](const std::vector<exploration_row::strength_entry>& v) {
                std::fprintf(f, "[");
                for (std::size_t k = 0; k < v.size(); ++k)
                {
                    const auto& e = v[k];
                    std::fprintf(f, "%s{\"id\": %d, \"lean\": \"%s\", \"treasury\": %lld, \"supply_q\": %d, "
                                    "\"regions\": %lld, \"cons_q\": %d, \"expn_q\": %d}",
                                 k ? ", " : "", e.id,
                                 e.cons_q > e.expn_q ? "C" : (e.expn_q > e.cons_q ? "E" : "-"),
                                 static_cast<long long>(e.treasury), e.mean_supply_q,
                                 static_cast<long long>(e.regions), e.cons_q, e.expn_q);
                }
                std::fprintf(f, "]");
            };

            std::fprintf(f, "{\n \"_note\": \"BL-937/BL-971 exploration sweep, 1200 -> 1660 CE. Reported, not gated "
                            "- see the harness header. One row per seed over the eleven readings of "
                            "EXPLORATION.md sec What the phase is judged on; 'spread' carries the face the "
                            "console prints. Displacement is read pooled (the verdict), volume-weighted and "
                            "as a median; a seed under silent_floor_battles traced battles is SILENT and "
                            "carries no verdict.\",\n");
            std::fprintf(f, " \"seed_count\": %d,\n \"deterrence_alarm_weight_q\": %d,\n \"overrides\": [",
                         seed_count, history_sim_params{}.deterrence_alarm_weight_q);
            {
                bool first = true;
                if (want_override && want_override_q >= 0)
                { std::fprintf(f, "\"w_want_q=%d\"", want_override_q); first = false; }
                for (const auto& kv : param_sets)
                { std::fprintf(f, "%s\"%s=%lld\"", first ? "" : ", ", kv.first.c_str(), kv.second); first = false; }
            }
            std::fprintf(f, "],\n \"structural_failures\": %d,\n", g_failures);

            // --- the spread face -------------------------------------------
            std::fprintf(f, " \"spread\": {\n  \"displacement\": {");
            put_d("pooled", face.have_pooled, face.pooled, ", ");
            put_d("weighted", face.have_weighted, face.weighted, ", ");
            put_d("median", face.have_median, face.median, ", ");
            std::fprintf(f, "\"sum_frontier\": %lld, \"sum_neighbour\": %lld, \"weighted_battles\": %lld, "
                            "\"no_nb_frontier_volume\": %lld, \"silent_floor_battles\": %lld, "
                            "\"seeds_ran\": %d, \"seeds_displaced\": %d, \"seeds_displaced_no_nb\": %d, "
                            "\"seeds_held\": %d, \"seeds_silent\": %d, \"silent_seeds\": [",
                         static_cast<long long>(face.sum_fr), static_cast<long long>(face.sum_nb),
                         static_cast<long long>(face.weighted_battles),
                         static_cast<long long>(face.no_nb_frontier_volume),
                         static_cast<long long>(face.silent_floor),
                         face.seeds_ran, face.seeds_displaced, face.seeds_displaced_no_nb,
                         face.seeds_held, face.seeds_silent);
            {
                bool first = true;
                for (const displacement_seed& d : disp)
                    if (std::strcmp(d.verdict, "SILENT") == 0)
                    { std::fprintf(f, "%s%u", first ? "" : ", ", d.seed); first = false; }
            }
            std::fprintf(f, "], \"ambiguous_battles\": %d, \"held_cause_census\": {\"NO FRONTIER\": %d, "
                            "\"NO EXPLORER\": %d, \"TREATY-CALM\": %d, \"DETERRENCE-INERT\": %d, "
                            "\"UNCLASSIFIED\": %d}},\n",
                         face.ambiguous_total, face.held_cause[0], face.held_cause[1],
                         face.held_cause[2], face.held_cause[3], face.held_cause[4]);
            std::fprintf(f, "  \"conflict_persists\": {\"median_empire_rate_per_century\": %.4f, "
                            "\"median_exploration_rate_per_century\": %.4f},\n",
                         face.med_empire_rate, face.med_expl_rate);
            std::fprintf(f, "  \"strategies\": {\"seeds_measured\": %lld, \"treasury_top_has_consolidator\": %lld, "
                            "\"treasury_top_has_expansionist\": %lld, \"treasury_top_has_both\": %lld, "
                            "\"regions_top_has_consolidator\": %lld, \"regions_top_has_expansionist\": %lld, "
                            "\"regions_top_has_both\": %lld},\n",
                         static_cast<long long>(face.r3_measured), static_cast<long long>(face.r3_cons),
                         static_cast<long long>(face.r3_expn), static_cast<long long>(face.r3_both),
                         static_cast<long long>(face.r3_reg_cons), static_cast<long long>(face.r3_reg_expn),
                         static_cast<long long>(face.r3_reg_both));
            std::fprintf(f, "  \"treaty_depth\": {\"formed\": %lld, \"broken\": %lld, \"blocked_campaigns\": %lld, "
                            "\"standing_1660\": %lld, ",
                         static_cast<long long>(face.r4_formed), static_cast<long long>(face.r4_broken),
                         static_cast<long long>(face.r4_blocked), static_cast<long long>(face.r4_standing));
            if (face.r4_standing > 0)
                std::fprintf(f, "\"years_left_min\": %lld, \"years_left_median\": %lld, \"years_left_max\": %lld},\n",
                             static_cast<long long>(face.r4_years_left_min),
                             static_cast<long long>(face.r4_years_left_median),
                             static_cast<long long>(face.r4_years_left_max));
            else
                std::fprintf(f, "\"years_left_min\": null, \"years_left_median\": null, \"years_left_max\": null},\n");
            std::fprintf(f, "  \"colonial_asymmetry\": {\"subjects\": %lld, \"overlords\": %lld, \"alive_polities\": %lld, "
                            "\"subjections_formed\": %lld, \"subjections_freed\": %lld, \"tribute_remitted\": %lld, "
                            "\"max_subject_distance\": %lld},\n",
                         static_cast<long long>(face.r5_subjects), static_cast<long long>(face.r5_overlords),
                         static_cast<long long>(face.r5_alive), static_cast<long long>(face.r5_formed),
                         static_cast<long long>(face.r5_freed), static_cast<long long>(face.r5_tribute),
                         static_cast<long long>(face.r5_max_distance));
            std::fprintf(f, "  \"subject_friction\": {\"contact_graph_friction\": %lld, \"divergent_top_want\": %lld},\n",
                         static_cast<long long>(face.r6_friction), static_cast<long long>(face.r6_divergent));
            std::fprintf(f, "  \"fleets\": {\"navy_holders\": %lld, \"spent_ports\": %lld, \"spent_navies\": %lld, "
                            "\"spent_standing_armies\": %lld, \"navy_holders_expn_top\": %lld, "
                            "\"navy_holders_expn_bottom\": %lld, \"port_steps\": %lld, \"navy_steps\": %lld, "
                            "\"army_steps\": %lld, \"navies_lapsed\": %lld, \"standing_holders\": %lld, "
                            "\"army_unpaid_rounds\": %lld, \"navy_unpaid_rounds\": %lld, "
                            "\"spent_army_upkeep\": %lld, \"spent_navy_upkeep\": %lld, "
                            "\"levy_raised\": %lld, \"levy_returned\": %lld, "
                            "\"standing_invariant_violations\": %lld},\n",
                         static_cast<long long>(face.r7_navy_holders), static_cast<long long>(face.r7_spent_ports),
                         static_cast<long long>(face.r7_spent_navies), static_cast<long long>(face.r7_spent_armies),
                         static_cast<long long>(face.r7_navy_top), static_cast<long long>(face.r7_navy_bottom),
                         static_cast<long long>(face.r7_port_steps), static_cast<long long>(face.r7_navy_steps),
                         static_cast<long long>(face.r7_army_steps), static_cast<long long>(face.r7_lapsed),
                         static_cast<long long>(face.r7_standing_holders),
                         static_cast<long long>(face.r7_army_unpaid), static_cast<long long>(face.r7_navy_unpaid),
                         static_cast<long long>(face.r7_army_upkeep), static_cast<long long>(face.r7_navy_upkeep),
                         static_cast<long long>(face.r7_levy_raised), static_cast<long long>(face.r7_levy_returned),
                         static_cast<long long>(face.r7_violations));
            std::fprintf(f, "  \"treasury_spread\": {\"polities\": %lld, \"treasury_min\": %lld, \"treasury_max\": %lld, "
                            "\"treasury_mean\": %.4f, \"corr_treasury_corridor_touch\": %.4f},\n",
                         static_cast<long long>(face.r8_polities), static_cast<long long>(face.r8_treasury_min),
                         static_cast<long long>(face.r8_treasury_max), face.r8_treasury_mean, face.r8_corr);
            std::fprintf(f, "  \"throughput\": {\"corridors\": %lld, \"uses_min\": %d, \"uses_median\": %d, \"uses_max\": %d, "
                            "\"post_roads_built\": %lld, \"seeds_with_post_road\": %d, \"treasury_spent_on_roads\": %lld, "
                            "\"tier_none\": %lld, \"tier_track\": %lld, \"tier_road\": %lld, \"tier_post_road\": %lld, "
                            "\"inherited_traffic_tier3\": %lld},\n",
                         static_cast<long long>(face.r9_corridors), face.r9_uses_min, face.r9_uses_median,
                         face.r9_uses_max, static_cast<long long>(face.r9_post_roads), face.r9_seeds_with_post_road,
                         static_cast<long long>(face.r9_spent_roads),
                         static_cast<long long>(face.r9_tier[0]), static_cast<long long>(face.r9_tier[1]),
                         static_cast<long long>(face.r9_tier[2]), static_cast<long long>(face.r9_tier[3]),
                         static_cast<long long>(face.r9_inherited_t3));
            std::fprintf(f, "  \"preference\": {\"live_entries_1660\": %lld, \"entries_1200\": %lld, "
                            "\"seeds_with_spread\": %lld, \"weight_min\": %d, \"weight_median\": %d, \"weight_max\": %d},\n",
                         static_cast<long long>(face.r10_live_entries), static_cast<long long>(face.r10_entries),
                         static_cast<long long>(face.r10_seeds_with_spread),
                         face.r10_w_min, face.r10_w_median, face.r10_w_max);
            std::fprintf(f, "  \"trade\": {\"flows\": %lld, \"volume\": %lld, \"cross_landmass_volume\": %lld, "
                            "\"unknown_landmass_volume\": %lld, \"flows_without_clause\": %lld, "
                            "\"trade_access_pairs\": %lld, \"pairs_carrying_none\": %lld, ",
                         static_cast<long long>(face.r11_flows), static_cast<long long>(face.r11_volume),
                         static_cast<long long>(face.r11_cross), static_cast<long long>(face.r11_unknown),
                         static_cast<long long>(face.r11_orphan), static_cast<long long>(face.r11_pairs),
                         static_cast<long long>(face.r11_zero_pairs));
            put_d("cross_landmass_share", face.r11_volume > 0,
                  face.r11_volume > 0 ? static_cast<double>(face.r11_cross) / static_cast<double>(face.r11_volume) : 0.0, ", ");
            put_d("top_decile_share", face.r11_volume > 0 && face.r11_pairs > 0, face.r11_top_decile_share, "}\n");
            std::fprintf(f, " },\n");

            // --- one row per seed -------------------------------------------
            std::fprintf(f, " \"worlds\": [\n");
            for (std::size_t i = 0; i < rows.size(); ++i)
            {
                const exploration_row& r = rows[i];
                const char* sep = (i + 1 < rows.size()) ? "," : "";
                if (!r.ok)
                {
                    std::fprintf(f, "  {\"seed\": %u, \"ok\": false}%s\n", r.seed, sep);
                    continue;
                }
                const displacement_seed* d = nullptr;
                for (const displacement_seed& x : disp) if (x.seed == r.seed) { d = &x; break; }

                std::fprintf(f, "  {\"seed\": %u, \"ok\": true, \"traced_matches_untraced\": %s,\n", r.seed,
                             r.traced_matches_untraced ? "true" : "false");
                // readings 1-2
                std::fprintf(f, "   \"empire_battles\": %lld, \"empire_years\": %lld, \"expl_battles\": %lld, "
                                "\"expl_conquests\": %lld, \"expl_foundings\": %lld, \"expl_years\": %lld, "
                                "\"neighbour_wars\": %lld, \"frontier_skirmishes\": %lld, \"ambiguous_battles\": %lld, ",
                             static_cast<long long>(r.empire_battles), static_cast<long long>(r.empire_years),
                             static_cast<long long>(r.expl_battles), static_cast<long long>(r.expl_conquests),
                             static_cast<long long>(r.expl_foundings), static_cast<long long>(r.expl_years),
                             static_cast<long long>(r.neighbour_wars), static_cast<long long>(r.frontier_skirmishes),
                             static_cast<long long>(r.ambiguous_battles));
                put_d("empire_rate_per_century", true, per_century(r.empire_battles, r.empire_years), ", ");
                put_d("expl_rate_per_century", true, per_century(r.expl_battles, r.expl_years), ", ");
                put_d("displacement_ratio", d && d->ratio >= 0.0, d ? d->ratio : 0.0, ", ");
                std::fprintf(f, "\"displacement_verdict\": \"%s\",\n", d ? d->verdict : "");
                // BL-999: the named cause (held seeds only; null otherwise) and
                // the facts it was read off, for every ran seed. The three
                // campaign classes are written apart (near / met-in-span /
                // unmet) because the census reads them apart.
                if (d && d->cause) std::fprintf(f, "   \"held_cause\": \"%s\", ", d->cause);
                else               std::fprintf(f, "   \"held_cause\": null, ");
                {
                    const auto put_class = [&](const char* prefix, const int64_t* c, const char* tail) {
                        std::fprintf(f, "\"%s_examined\": %lld, \"%s_treaty_blocked\": %lld, \"%s_water_illegal\": %lld, "
                                        "\"%s_reach_denied\": %lld, \"%s_in_reach\": %lld, \"%s_cleared\": %lld, "
                                        "\"%s_chosen\": %lld%s",
                                     prefix, static_cast<long long>(c[0]), prefix, static_cast<long long>(c[1]),
                                     prefix, static_cast<long long>(c[2]), prefix, static_cast<long long>(c[3]),
                                     prefix, static_cast<long long>(c[0] - c[1] - c[2] - c[3]),
                                     prefix, static_cast<long long>(c[4]), prefix, static_cast<long long>(c[5]), tail);
                    };
                    std::fprintf(f, "\"held_cause_facts\": {");
                    put_class("near",  r.campaign_class[0], ", ");
                    put_class("met",   r.campaign_class[1], ", ");
                    put_class("unmet", r.campaign_class[2], ", ");
                    std::fprintf(f, "\"near_pairs\": %lld, \"near_pairs_bound\": %lld, \"met_pairs\": %lld, "
                                    "\"met_pairs_bound\": %lld, \"near_pairs_alarmed\": %lld, \"near_alarm_max_q\": %d, "
                                    "\"rim_holders_ever\": %lld, \"rim_holders_alive\": %lld, \"wants\": %lld, "
                                    "\"wants_outward\": %lld, \"wants_frontier\": %lld},\n",
                                 static_cast<long long>(r.near_pairs), static_cast<long long>(r.near_bound),
                                 static_cast<long long>(r.far_pairs), static_cast<long long>(r.far_bound),
                                 static_cast<long long>(r.near_pairs_alarmed), r.near_alarm_max_q,
                                 static_cast<long long>(r.rim_holders_ever), static_cast<long long>(r.rim_holders_alive),
                                 static_cast<long long>(r.wants_total), static_cast<long long>(r.wants_outward),
                                 static_cast<long long>(r.wants_frontier));
                }
                // reading 3
                std::fprintf(f, "   \"strength_measured\": %s, \"treasury_top_has_consolidator\": %s, "
                                "\"treasury_top_has_expansionist\": %s, \"regions_top_has_consolidator\": %s, "
                                "\"regions_top_has_expansionist\": %s, \"top_by_treasury\": ",
                             r.strength_measured ? "true" : "false",
                             r.top_has_consolidator ? "true" : "false", r.top_has_expansionist ? "true" : "false",
                             r.regions_top_has_consolidator ? "true" : "false",
                             r.regions_top_has_expansionist ? "true" : "false");
                top3(r.top_by_treasury);
                std::fprintf(f, ", \"top_by_regions\": ");
                top3(r.top_by_regions);
                std::fprintf(f, ",\n");
                // reading 4
                std::fprintf(f, "   \"treaties_formed\": %lld, \"treaties_broken\": %lld, \"treaty_blocked_campaigns\": %lld, "
                                "\"treaties_standing_1660\": %zu, ",
                             static_cast<long long>(r.treaties_formed_total), static_cast<long long>(r.treaties_broken_total),
                             static_cast<long long>(r.treaty_blocked_campaigns), r.treaty_years_left.size());
                put_mmm("treaty_years_left", r.treaty_years_left, ",\n");
                // reading 5-6
                std::fprintf(f, "   \"subjects_alive\": %lld, \"overlords_alive\": %lld, \"alive_polities\": %lld, "
                                "\"max_subject_distance\": %lld, \"subjections_formed\": %lld, \"subjections_freed\": %lld, "
                                "\"tribute_remitted\": %lld, \"subjects_with_friction\": %lld, "
                                "\"subjects_with_divergent_top_want\": %lld, \"live_pref_entries_1660\": %lld,\n",
                             static_cast<long long>(r.subjects_alive), static_cast<long long>(r.overlords_alive),
                             static_cast<long long>(r.alive_polities), static_cast<long long>(r.max_subject_distance),
                             static_cast<long long>(r.subjections_formed), static_cast<long long>(r.subjections_freed),
                             static_cast<long long>(r.tribute_remitted), static_cast<long long>(r.subjects_with_friction),
                             static_cast<long long>(r.subjects_with_divergent_top_want),
                             static_cast<long long>(r.live_pref_entries_1660));
                // reading 7
                std::fprintf(f, "   \"navy_holders\": %lld, \"navy_holders_expn_top\": %lld, \"navy_holders_expn_bottom\": %lld, "
                                "\"spent_navies\": %lld, \"spent_ports\": %lld, \"spent_standing_armies\": %lld, "
                                "\"port_steps\": %lld, \"navy_steps\": %lld, \"army_steps\": %lld, \"navies_lapsed\": %lld, "
                                "\"standing_holders\": %lld, \"army_unpaid_rounds\": %lld, \"navy_unpaid_rounds\": %lld, "
                                "\"spent_army_upkeep\": %lld, \"spent_navy_upkeep\": %lld, "
                                "\"levy_raised\": %lld, \"levy_returned\": %lld, \"standing_invariant_violations\": %lld, ",
                             static_cast<long long>(r.navy_holders), static_cast<long long>(r.navy_holders_expn_top),
                             static_cast<long long>(r.navy_holders_expn_bottom),
                             static_cast<long long>(r.treasury_spent_on_navies), static_cast<long long>(r.treasury_spent_on_ports),
                             static_cast<long long>(r.treasury_spent_on_standing_armies),
                             static_cast<long long>(r.port_steps), static_cast<long long>(r.navy_steps),
                             static_cast<long long>(r.army_steps), static_cast<long long>(r.navies_lapsed),
                             static_cast<long long>(r.standing_holders),
                             static_cast<long long>(r.army_unpaid_rounds), static_cast<long long>(r.navy_unpaid_rounds),
                             static_cast<long long>(r.spent_army_upkeep), static_cast<long long>(r.spent_navy_upkeep),
                             static_cast<long long>(r.levy_raised), static_cast<long long>(r.levy_returned),
                             static_cast<long long>(r.standing_invariant_violations));
                put_mmm("standing_per_region", r.standing_per_region, ", ");
                put_mmm("garrison_per_region", r.garrison_per_region, ",\n");
                // reading 8 (per seed: living polities, treasury spread, and the
                // seed's own correlation where >= 2 polities exist)
                {
                    std::vector<int64_t> tr;
                    for (const auto& tc : r.polity_treasury_corridor) tr.push_back(tc.first);
                    std::fprintf(f, "   \"living_polities\": %zu, ", r.polity_treasury_corridor.size());
                    put_mmm("treasury", tr, ", ");
                    double corr = 0.0; bool have_corr = false;
                    if (r.polity_treasury_corridor.size() >= 2)
                    {
                        const double n = static_cast<double>(r.polity_treasury_corridor.size());
                        double mt = 0.0, mc = 0.0;
                        for (const auto& p : r.polity_treasury_corridor)
                        { mt += static_cast<double>(p.first); mc += static_cast<double>(p.second); }
                        mt /= n; mc /= n;
                        double cov = 0.0, vt = 0.0, vc = 0.0;
                        for (const auto& p : r.polity_treasury_corridor)
                        {
                            const double dt = static_cast<double>(p.first) - mt;
                            const double dc = static_cast<double>(p.second) - mc;
                            cov += dt * dc; vt += dt * dt; vc += dc * dc;
                        }
                        have_corr = vt > 0.0 && vc > 0.0;
                        corr = have_corr ? cov / std::sqrt(vt * vc) : 0.0;
                    }
                    put_d("corr_treasury_corridor_touch", have_corr, corr, ",\n");
                }
                // reading 9
                {
                    std::vector<int64_t> uses(r.corridor_uses.begin(), r.corridor_uses.end());
                    std::fprintf(f, "   \"corridors\": %zu, ", r.corridor_uses.size());
                    put_mmm("uses", uses, ", ");
                    std::fprintf(f, "\"post_roads_built\": %lld, \"treasury_spent_on_roads\": %lld, "
                                    "\"tier_none\": %lld, \"tier_track\": %lld, \"tier_road\": %lld, "
                                    "\"tier_post_road\": %lld, \"inherited_traffic_tier3\": %lld,\n",
                                 static_cast<long long>(r.post_roads_built), static_cast<long long>(r.treasury_spent_on_roads),
                                 static_cast<long long>(r.tier_count[0]), static_cast<long long>(r.tier_count[1]),
                                 static_cast<long long>(r.tier_count[2]), static_cast<long long>(r.tier_count[3]),
                                 static_cast<long long>(r.inherited_traffic_tier3));
                }
                // reading 10
                {
                    std::vector<int64_t> w(r.preference_weights.begin(), r.preference_weights.end());
                    std::fprintf(f, "   \"preference_entries_1200\": %zu, ", r.preference_weights.size());
                    put_mmm("preference_weight", w, ",\n");
                }
                // reading 11
                {
                    int64_t with_flow = 0;
                    for (int64_t v : r.pair_volume) if (v > 0) ++with_flow;
                    std::fprintf(f, "   \"flows\": %lld, \"flow_volume\": %lld, \"cross_landmass_volume\": %lld, "
                                    "\"unknown_landmass_volume\": %lld, \"flows_without_clause\": %lld, "
                                    "\"trade_access_pairs\": %lld, \"pairs_with_flow\": %lld}%s\n",
                                 static_cast<long long>(r.flow_count), static_cast<long long>(r.flow_volume),
                                 static_cast<long long>(r.cross_landmass_volume),
                                 static_cast<long long>(r.unknown_landmass_volume),
                                 static_cast<long long>(r.flows_without_clause),
                                 static_cast<long long>(r.trade_bound_pairs), static_cast<long long>(with_flow), sep);
                }
            }
            std::fprintf(f, " ]\n}\n");
            std::fclose(f);
            std::printf("\nWrote %s (%d rows)\n", json_path, static_cast<int>(rows.size()));
        }
    }

    std::printf("\n%d failure(s) in structural checks.\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
