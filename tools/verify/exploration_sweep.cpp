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
// Usage:  exploration_sweep [seed_count] [--seeds a,b,c] [--out path] [--w_want_q=N]
//         (default 8 seeds, 0..N-1)
//
//   --seeds a,b,c  BL-1026: measure exactly these seeds, in this order, instead
//                  of 0..N-1. The seed library's list reaches 46, and a 48-seed
//                  run to reach it would overwrite the checked-in 16-seed table.
//   --out path     BL-1026: write the JSON table here instead of the default
//                  name. The library's fingerprints are read from
//                  `--seeds <library> --out seed_library_sweep.json`
//                  (`node tools/session/seed_library.js --seed-list` prints it).
//   --through Y    BL-1027: run the Exploration call on to year Y instead of 1660
//                  (world_params::exploration_stop_year; epoch_year stays 0, so
//                  this is the shipped world continued, never the superseded
//                  two-span arc). Every reading then describes the close at Y,
//                  field names ending _1660 included. Without --out, a run past
//                  1660 writes exploration_sweep.through.json, never the table.
//   --cost         BL-1027: time two UNTRACED re-runs per seed from the fixture,
//                  to 1660 and to --through, with the sim's profile split, and
//                  report the 1660 -> Y half as their difference.
//   --industry-open Y  BL-1038 TUNING ONLY: the traced re-run (and its half-A
//                  prefix) runs with history_sim_params::industry_tree_enabled
//                  on from year Y. Prints, per seed and pooled, the Industry
//                  nodes held, fork sides taken, rim holders, the share of
//                  living polities that never passed the seam fuel gate, and
//                  the urban mass the rate reads at 1660 and at the close.
//                  Pair with --through 1960. Writes the tuning table.
//
// BL-1018 adds the ALARM SPREAD: the raw visible capability every near-home
// treaty read saw over the traced run, its quantiles, and the alarm those
// reads come to through the reference in effect -- printed per seed and
// pooled, written as `spread.alarm_spread` and per seed. (The trace-only half
// of that item; its re-scaled `visible_capability_reference` is not taken.)
// BL-1019 adds FIRST CONTACT: per seed, the first-contact count over the
// traced run (crossings and inheritance), the geography at 1200 it had to work
// with, the unmet candidate's contest (what beat it on the rounds it cleared),
// and pooled what each contact class is scored on; written per seed as
// `first_contacts`. (Trace-only half; its census label changes are not taken.)
// BL-1028 adds the WEAKNESS COUNTERS: first contacts by kind, met-in-span
// pairs holding a treaty, near-home alarm reads at the ceiling, displacement,
// the contact-class campaign funnel, the unmet contest and cross-landmass
// trade volume -- per seed, for half A (1200 -> 1660) and, when --through runs
// past 1660, half B (1660 -> --through). Half B needs ONE extra traced re-run
// per seed, stopped at 1660; its method is stated above the section. Printed
// as "--- weakness counters (BL-1028) ---", written per seed as `weakness`
// (`half_a`, `half_b`) and pooled as `spread.weakness`.
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
#include "scripting/lua_state.hpp"

#include <algorithm>
#include <chrono>
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

// BL-1016 -- READING 3 CLASSIFIES BY RANK, ON ONE SCALE. The two raw leans are
// not commensurable: `consolidator_lean_q` is a PRODUCT (dominion x (1 - sea
// legs)), so it is at most its smaller factor, while `expansion_lean_q` is a
// MEAN (sea legs and zeal), so it is at least its smaller input. Comparing
// them raw answers a question about arithmetic -- a middling culture (sea
// 300, zeal 500, dominion 500) reads 350 against 400 and "is" expansionist
// with nothing seafaring about it. The sim never compares them raw: BL-955
// reads each lean only as a per-mille rank among the round's living, cultured
// polities (`exploration_lean_ranks`). The sweep now does the same, off that
// very function, and a realm is a consolidator when its consolidator rank
// sits in the TOP THIRD by that lean, an expansionist likewise by expansion
// rank. Each test is independent, so a realm may be both or neither.
//
// 667, not 666: the rank counts polities leaning STRICTLY lower (ties share
// the lowest rank), so >= 667 per mille means more than two thirds of the
// seed's other living polities lean lower than this one.
constexpr int kLeanTopThirdRankQ = 667;

bool reads_consolidator(int cons_rank_q) { return cons_rank_q >= kLeanTopThirdRankQ; }
bool reads_expansionist(int expn_rank_q) { return expn_rank_q >= kLeanTopThirdRankQ; }

/// "C" consolidator, "E" expansionist, "B" both, "-" neither (or no culture).
const char* lean_tag_of(int cons_rank_q, int expn_rank_q)
{
    const bool c = reads_consolidator(cons_rank_q), e = reads_expansionist(expn_rank_q);
    return c && e ? "B" : c ? "C" : e ? "E" : "-";
}

/// Nearest-rank-below percentile over an ASCENDING vector: index (n-1)*pct/100.
/// Integer, so the printed face is a property of the values alone.
int percentile_of(const std::vector<int>& sorted, int pct)
{
    if (sorted.empty()) return -1;
    const std::size_t idx = (sorted.size() - 1) * static_cast<std::size_t>(pct) / 100u;
    return sorted[idx];
}

// ---------------------------------------------------------------------------
// BL-1028 -- ONE HALF OF THE WEAKNESS COUNTERS.
// ---------------------------------------------------------------------------
// Two kinds of field, and the difference is the whole method:
//   CUMULATIVE -- counted over a traced run from 1200 (a trace counter, or a
//     count over an append-only trace vector). Half A reads them off a traced
//     run stopped at 1660; half B is (run to --through) minus (run to 1660).
//   AT STOP -- the state a run closes on (standing pairs, treaties, the
//     final round's trade flows, the alarm each near pair reads). Read at
//     each run's own stop: half A at 1660, half B at --through. Never
//     subtracted.
struct weakness_half
{
    int64_t from_year = 0, to_year = 0;

    // --- CUMULATIVE ----------------------------------------------------------
    /// New contact pairs raised (`contacts_raised_trace`): [0] a campaign
    /// crossing onto the other's ground, [1] inherited from a conquered polity.
    int64_t contacts_raised[2] = {0, 0};
    /// Near-home treaty reads (`near_capability_trace` entries) and how many
    /// read the alarm's ceiling through the run's reference (raw * 1000 /
    /// reference >= 1000, the same clamp `visible_capability_q` applies).
    int64_t alarm_reads = 0, alarm_reads_ceiling = 0;
    /// Traced battles by pair class (`battle_trace`): neighbour = the pair was
    /// in contact at 1200 (`pre_exploration_contacts`), frontier otherwise;
    /// ambiguous = attacker and defender both id 0 (excluded, as readings 1-2).
    int64_t neighbour_battles = 0, frontier_battles = 0, ambiguous_battles = 0;
    /// `campaign_class_trace` [class][gate]: class 0 near (met before 1200),
    /// 1 met in span, 2 unmet; gates examined, treaty-blocked, water-illegal,
    /// reach-denied, cleared (season-grain), chosen (round-grain).
    int64_t funnel[3][6] = {};
    /// `unmet_contest_trace` [0] rounds an unmet candidate cleared, [1] won by
    /// it, [2] lost to a near campaign, [3] lost to a met-in-span campaign,
    /// [4] lost to another verb; the margin sum over the lost rounds; and
    /// `unmet_lost_to_verb_trace` by `sim_verb`.
    int64_t unmet_contest[5] = {0, 0, 0, 0, 0};
    int64_t unmet_margin_sum = 0;
    int64_t unmet_lost_verb[8] = {};

    // --- AT STOP --------------------------------------------------------------
    /// Pairs (canonical row) first met at or after 1200, those holding a
    /// non-aggression clause (the existing `met_pairs_bound`'s own test), and
    /// those holding a clause of ANY treaty kind.
    int64_t met_pairs = 0, met_pairs_bound = 0, met_pairs_any_treaty = 0;
    /// Pairs met before 1200: how many, how many either side reads a positive
    /// `deterrence_alarm_q` of, how many read the ceiling, and the largest
    /// read (the existing `near_alarm_max_q`'s own test).
    int64_t near_pairs = 0, near_pairs_alarmed = 0, near_pairs_ceiling = 0;
    int     near_alarm_max_q = 0;
    /// The final decision round's trade flows (reading 11's own test).
    int64_t flow_volume = 0, cross_landmass_volume = 0, unknown_landmass_volume = 0;
};

/// Half B's cumulative fields as (to --through) minus (to 1660); its at-stop
/// fields are the --through run's own.
weakness_half weakness_minus(const weakness_half& through, const weakness_half& a)
{
    weakness_half b = through;
    for (int k = 0; k < 2; ++k) b.contacts_raised[k] -= a.contacts_raised[k];
    b.alarm_reads         -= a.alarm_reads;
    b.alarm_reads_ceiling -= a.alarm_reads_ceiling;
    b.neighbour_battles   -= a.neighbour_battles;
    b.frontier_battles    -= a.frontier_battles;
    b.ambiguous_battles   -= a.ambiguous_battles;
    for (int k = 0; k < 3; ++k)
        for (int g = 0; g < 6; ++g) b.funnel[k][g] -= a.funnel[k][g];
    for (int g = 0; g < 5; ++g) b.unmet_contest[g] -= a.unmet_contest[g];
    b.unmet_margin_sum -= a.unmet_margin_sum;
    for (int g = 0; g < 8; ++g) b.unmet_lost_verb[g] -= a.unmet_lost_verb[g];
    return b;
}

/// Every CUMULATIVE field non-negative: what a (through - 1660) difference
/// must satisfy if the shorter run is a prefix of the longer one.
bool weakness_cumulative_nonnegative(const weakness_half& h)
{
    bool ok = h.contacts_raised[0] >= 0 && h.contacts_raised[1] >= 0
           && h.alarm_reads >= 0 && h.alarm_reads_ceiling >= 0
           && h.neighbour_battles >= 0 && h.frontier_battles >= 0 && h.ambiguous_battles >= 0
           && h.unmet_margin_sum >= 0;
    for (int k = 0; k < 3; ++k)
        for (int g = 0; g < 6; ++g) ok = ok && h.funnel[k][g] >= 0;
    for (int g = 0; g < 5; ++g) ok = ok && h.unmet_contest[g] >= 0;
    for (int g = 0; g < 8; ++g) ok = ok && h.unmet_lost_verb[g] >= 0;
    return ok;
}

/// Pooled sum of two halves' counts (maximum for the max read).
void weakness_accumulate(weakness_half& into, const weakness_half& h)
{
    into.from_year = h.from_year; into.to_year = h.to_year;
    for (int k = 0; k < 2; ++k) into.contacts_raised[k] += h.contacts_raised[k];
    into.alarm_reads         += h.alarm_reads;
    into.alarm_reads_ceiling += h.alarm_reads_ceiling;
    into.neighbour_battles   += h.neighbour_battles;
    into.frontier_battles    += h.frontier_battles;
    into.ambiguous_battles   += h.ambiguous_battles;
    for (int k = 0; k < 3; ++k)
        for (int g = 0; g < 6; ++g) into.funnel[k][g] += h.funnel[k][g];
    for (int g = 0; g < 5; ++g) into.unmet_contest[g] += h.unmet_contest[g];
    into.unmet_margin_sum += h.unmet_margin_sum;
    for (int g = 0; g < 8; ++g) into.unmet_lost_verb[g] += h.unmet_lost_verb[g];
    into.met_pairs            += h.met_pairs;
    into.met_pairs_bound      += h.met_pairs_bound;
    into.met_pairs_any_treaty += h.met_pairs_any_treaty;
    into.near_pairs           += h.near_pairs;
    into.near_pairs_alarmed   += h.near_pairs_alarmed;
    into.near_pairs_ceiling   += h.near_pairs_ceiling;
    into.near_alarm_max_q      = std::max(into.near_alarm_max_q, h.near_alarm_max_q);
    into.flow_volume             += h.flow_volume;
    into.cross_landmass_volume   += h.cross_landmass_volume;
    into.unknown_landmass_volume += h.unknown_landmass_volume;
}

/// In reach, per `campaign_class_trace`'s grain: examined less the three gates.
int64_t funnel_in_reach(const int64_t* c) { return c[0] - c[1] - c[2] - c[3]; }

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

    // --- BL-1038: the Industry tree, when --industry-open ran ----------------
    /// Read off the traced re-run's close (the switch on from the open year).
    /// `industry_forks[f][s]`: fork f in node-table order, s = 0 first side
    /// held, 1 second side held, 2 neither.
    bool                 industry_ran        = false;
    int                  industry_alive      = 0;
    int                  industry_invested   = 0; ///< living polities holding at least one node
    int                  industry_rim        = 0; ///< living polities holding IN-SP-3m
    /// The fuel gate, split (fix round). "Had an Industry round" is read as
    /// `industry_investing >= 0 || industry_mask != 0`: the first round always
    /// targets the ungated root, after which a polity is investing or holds a
    /// node, and a polity minted mid-run starts at the defaults — so the
    /// reading is exact without a sim field of its own.
    int                  industry_had_round   = 0; ///< living polities with at least one Industry round
    int                  industry_fuel_never  = 0; ///< ...of those, never passed the seam gate
    int                  industry_no_round    = 0; ///< living polities with no Industry round at all
    std::vector<int>     industry_nodes;          ///< nodes held, one per living polity
    int                  industry_forks[3][3] = {};
    std::vector<int64_t> urban_mass_open;         ///< top-k urban mass per living polity at the open year (--through > 1660 only)
    std::vector<int64_t> urban_mass_close;        ///< the same at the traced close

    // --- BL-1027: the span's cost, when --cost ran ---------------------------
    /// One untraced re-run from the fixture, stopped at a given year: its wall
    /// clock, the sim's own profile split, and what the world looked like at
    /// the stop. Every counter is cumulative from 1200.
    struct span_cost
    {
        int64_t wall_ms = 0;
        int64_t ns_demography = 0, ns_decisions = 0, ns_battles = 0, ns_reach = 0;
        int64_t decision_rounds = 0, reach_rebuilds = 0;
        int64_t battles = 0, conquests = 0, foundings = 0;
        int64_t regions = 0, alive = 0, rim_holders = 0;
        double  mean_exploration_nodes = 0.0; ///< Exploration-tree nodes held, per living polity.
    };
    bool      cost_ran        = false;
    bool      cost_reproduces = false; ///< The untraced run to --through reproduces generation's counts.
    int64_t   regions_1200    = 0;
    int64_t   alive_1200      = 0;
    span_cost to_1660;
    span_cost to_through;

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
        int     cons_q        = 0; ///< consolidator_lean_q of its culture (-1: no culture).
        int     expn_q        = 0; ///< expansion_lean_q of its culture (-1: no culture).
        /// BL-1016: the per-mille RANK of each lean among the seed's living,
        /// cultured polities at 1660 (`exploration_lean_ranks`), -1 with no
        /// culture. The classification reads THESE, never the raw leans.
        int     cons_rank_q   = -1;
        int     expn_rank_q   = -1;
    };
    /// BL-1016: one living, cultured polity at the traced re-run's 1660 close
    /// -- the whole population reading 3 classifies against, not the top 3.
    struct lean_point
    {
        int cons_q = 0, expn_q = 0;           ///< the raw leans (0-1000)
        int cons_rank_q = 0, expn_rank_q = 0; ///< their per-mille ranks
        int sea_q = 0, dominion_q = 0, zeal_q = 0; ///< the three inputs, each 0-1000
    };
    std::vector<lean_point> lean_population;
    int64_t living_uncultured = 0; ///< living polities with no culture: unranked, never classified.
    bool strength_measured = false; ///< >= 2 living polities at the traced re-run's close.
    /// PRIMARY ranking (BL-951): capital treasury, tie-break mean supply.
    std::vector<strength_entry> top_by_treasury;
    bool top_has_consolidator = false;
    bool top_has_expansionist = false;
    bool top_both_distinct    = false; ///< a consolidator AND a DIFFERENT expansionist realm (BL-1016).
    /// COMPARISON ranking: held region count (the pre-BL-951 metric).
    std::vector<strength_entry> top_by_regions;
    bool regions_top_has_consolidator = false;
    bool regions_top_has_expansionist = false;
    bool regions_top_both_distinct    = false;

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
    /// BL-1018: of those near pairs, how many read the alarm's ceiling (1000)
    /// at the close.
    int64_t near_pairs_saturated = 0;
    /// BL-1018: the traced run's `visible_capability_reference`, and the RAW
    /// visible capability of the counterpart in every near-home treaty read
    /// over the whole traced run (`near_capability_trace`), sorted ascending.
    int64_t capability_reference = 0;
    std::vector<int64_t> near_capability;

    // --- BL-1019: the first crossing, off the traced re-run -----------------
    /// New contact pairs raised over the traced run: [0] by a campaign
    /// crossing, [1] inherited from a conquered polity (`contacts_raised_trace`).
    int64_t contacts_raised[2] = {0, 0};
    /// `class_score_trace` copied whole (see its field comment).
    int64_t class_score[3][13] = {};
    /// `unmet_contest_trace` and its margin sum, and `unmet_lost_to_verb_trace`, copied whole.
    int64_t unmet_contest[5] = {0, 0, 0, 0, 0};
    int64_t unmet_lost_verb[8] = {};
    int64_t unmet_margin_sum = 0;
    /// THE GEOGRAPHY AT 1200, off the handoff state: living polities, their
    /// pairs, the pairs never met, and of those the ones that TOUCH -- some
    /// region of each within `neighbour_radius` of the other, which is the
    /// only way a campaign candidate between them can exist -- split by
    /// whether their capitals share a landmass.
    int64_t polities_1200 = 0, pairs_1200 = 0, unmet_pairs_1200 = 0;
    int64_t unmet_touching_1200 = 0, unmet_touching_cross_mass_1200 = 0;
    int64_t landmasses_with_polity_1200 = 0;

    // --- BL-1028: the weakness counters, per half ---------------------------
    weakness_half weak_a;              ///< 1200 -> min(1660, --through)
    weakness_half weak_b;              ///< 1660 -> --through; meaningful only when weak_have_b
    bool          weak_have_b = false;
    /// The traced run stopped at 1660 is a prefix of the traced run to
    /// --through (battle traces, alarm reads, every cumulative counter) --
    /// what licenses the subtraction. True when no half B was taken.
    bool          weak_prefix_ok = true;
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

/// BL-1038: the Industry rate's input for every LIVING polity, read the way
/// the sim reads it (`industry_urban_mass` over the regions it holds, held =
/// `region::nation`), sorted ascending. What the rate earns from is what the
/// sizing rule is tuned against, so the sweep prints it beside the outcome.
std::vector<int64_t> living_urban_masses(const std::vector<region>& regions,
                                         const std::vector<polity>& polities)
{
    std::vector<std::vector<int>> held(polities.size());
    for (std::size_t i = 0; i < regions.size(); ++i)
    {
        const int n = regions[i].nation;
        if (n >= 0 && static_cast<std::size_t>(n) < held.size()) held[static_cast<std::size_t>(n)].push_back(static_cast<int>(i));
    }
    std::vector<int64_t> out;
    for (std::size_t p = 0; p < polities.size(); ++p)
        if (polities[p].alive) out.push_back(industry_urban_mass(regions, held[p]));
    std::sort(out.begin(), out.end());
    return out;
}

/// The value at percentile @p pct (0-100) of an ascending vector, or -1 empty.
template <typename T>
long long pct_of_sorted(const std::vector<T>& v, int pct)
{
    if (v.empty()) return -1;
    return static_cast<long long>(v[(v.size() - 1) * static_cast<std::size_t>(pct) / 100]);
}

} // namespace


// ---------------------------------------------------------------------------
// The shipped generation inputs (BL-1007)
// ---------------------------------------------------------------------------
// GENERATION TAKES TWO THINGS FROM THE DATA LAYER, and until this item the
// sweeps handed it neither. `app::begin_new_game` loads scripts/world_gen.lua
// into a `world_gen_config`, and `app::ensure_works_loaded` loads
// scripts/works.lua into a `works_registry`; both are passed to
// `make_hard_coded_world`. The sweeps passed `world_gen_config{}` — the C++
// struct defaults, which world_gen_config.hpp's own header warns price 10 of 47
// resources where the script authors 42 — and a null works pointer, which makes
// the `build_work` verb a dead branch (history_sim.cpp: "works == nullptr ...
// break"), so no polity ever raised a work in a swept world.
//
// The cost of that was measured on 2026-09-16: seed 0's Empires span fought
// 6,479 battles in the sweep and 9,928 in the game. Every reading was
// self-consistent and described a world nobody plays. These helpers are the one
// place either sweep reads the data layer, and they mirror the app's own order.
struct shipped_inputs
{
    lua_state        lua;
    world_gen_config cfg{};
    works_registry   works;
};

/// Load the data layer exactly as the app does, and say so on the face: a
/// reading that cannot name the world it measured is the defect this closes.
inline void load_shipped_inputs(shipped_inputs& in)
{
    in.lua.load("scripts/recipes.lua");
    in.lua.load("scripts/economy.lua");
    in.lua.load("scripts/world_gen.lua");
    in.cfg.load_from_lua(in.lua);
    in.lua.load("scripts/works.lua");
    in.works.load_from_lua(in.lua);
    std::printf("generation inputs: scripts/world_gen.lua + scripts/works.lua "
                "(%zu works rows) — the shipped configuration (BL-1007)\n",
                in.works.size());
}

int main(int argc, char** argv)
{
    shipped_inputs shipped;
    load_shipped_inputs(shipped);

    int seed_count = 8;
    std::vector<uint32_t> seed_list; // BL-1026: --seeds; empty means 0..seed_count-1.
    std::string out_path;            // BL-1026: --out; empty means the default name.
    int64_t through_year = 1660;     // BL-1027: --through; the Exploration call's stop.
    bool    run_cost     = false;    // BL-1027: --cost.
    int64_t industry_open = 0;       // BL-1038: --industry-open; 0 = the switch stays off.
    bool want_override = false;
    int  want_override_q = 0;
    std::vector<std::pair<std::string, long long>> param_sets;
    for (int a = 1; a < argc; ++a)
    {
        if (std::strcmp(argv[a], "--seeds") == 0 && a + 1 < argc)
        {
            const std::string list = argv[++a];
            std::size_t at = 0;
            while (at <= list.size())
            {
                const std::size_t comma = list.find(',', at);
                const std::string tok = list.substr(at, comma == std::string::npos ? std::string::npos : comma - at);
                if (tok.empty() || tok.find_first_not_of("0123456789") != std::string::npos)
                { std::printf("--seeds: '%s' is not a seed number\n", tok.c_str()); std::exit(2); }
                seed_list.push_back(static_cast<uint32_t>(std::strtoul(tok.c_str(), nullptr, 10)));
                if (comma == std::string::npos) break;
                at = comma + 1;
            }
            continue;
        }
        if (std::strcmp(argv[a], "--out") == 0 && a + 1 < argc)
        {
            out_path = argv[++a];
            continue;
        }
        if (std::strcmp(argv[a], "--through") == 0 && a + 1 < argc)
        {
            through_year = std::atoll(argv[++a]);
            if (through_year <= 1200) { std::printf("--through must be after 1200\n"); std::exit(2); }
            continue;
        }
        if (std::strcmp(argv[a], "--cost") == 0)
        {
            run_cost = true;
            continue;
        }
        if (std::strcmp(argv[a], "--industry-open") == 0 && a + 1 < argc)
        {
            industry_open = std::atoll(argv[++a]);
            if (industry_open <= 0) { std::printf("--industry-open needs a calendar year\n"); std::exit(2); }
            continue;
        }
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
    if (seed_list.empty())
        for (int i = 0; i < seed_count; ++i) seed_list.push_back(static_cast<uint32_t>(i));
    seed_count = static_cast<int>(seed_list.size());
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
    if (industry_open > 0)
    {
        // A tuning override like --set: the traced re-run (and the half-A
        // re-run built from it) differ from generation's own run by
        // construction, so the structural traced-vs-untraced check is waived
        // and the table goes to exploration_sweep.tuning.json.
        if (!want_override) want_override_q = -1;
        want_override = true;
        std::printf("NOTE: --industry-open %lld turns the Industry tree ON in the traced re-run from %lld\n"
                    "      (history_sim_params::industry_tree_enabled, BL-1038; tuning only -- every shipped\n"
                    "      path keeps it off). The Industry readings describe the traced close.\n",
                    static_cast<long long>(industry_open), static_cast<long long>(industry_open));
    }

    std::printf("=== exploration sweep (BL-937) - %d seeds, 1200 -> %lld CE ===\n\n", seed_count,
                static_cast<long long>(through_year));
    if (through_year != 1660)
        std::printf("NOTE: --through %lld runs the SHIPPED world (epoch 0) with Exploration's own call continued\n"
                    "      past 1660. It runs Exploration's forces only, with the Empires-round overrides off\n"
                    "      (era_minus_one.cpp exploration_sim_params). Every reading below describes the close\n"
                    "      at %lld, including those whose names say 1660.\n\n",
                    static_cast<long long>(through_year), static_cast<long long>(through_year));

    std::vector<exploration_row> rows;
    rows.reserve(static_cast<std::size_t>(seed_count));

    for (const uint32_t seed : seed_list)
    {
        const int i = static_cast<int>(seed); // the printed seed, not a loop index
        world_params wp;
        wp.seed = seed;
        wp.exploration_sim_enabled = true; // BL-937: the whole point of this sweep.
        wp.exploration_stop_year   = through_year; // BL-1027: 1660 unless --through.

        generation_report     rep;
        era_minus_one_fixture fx;
        // BL-958: STOP AT THE EXPLORATION CLOSE. Every reading reads the era
        // fixture, which is complete before this stop; nations, roads, firms and
        // markets (~90% of a whole world) are never read here.
        world_gen_config gen_cfg = shipped.cfg; // BL-1007: the shipped data layer
        gen_cfg.stop_after_exploration = true;
        std::fprintf(stderr, "[sweep] seed %d generated to the Exploration close\n", i);
        const world w = make_hard_coded_world(wp, &rep, gen_cfg,
                                              /*progress=*/nullptr, &shipped.works, &fx);
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
        if (industry_open > 0)
        {
            ep2.industry_tree_enabled = true;          // BL-1038, tuning only
            ep2.industry_open_year    = industry_open;
        }

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

        // --- BL-1038: THE INDUSTRY TREE at the traced close ------------------
        // Per living polity: nodes held, fork sides taken, the rim, and whether
        // it EVER passed the seam gate on an Industry-investing round
        // (`polity::industry_fuel_seen`). Forks are found off the generated
        // table's `excludes`, never by id.
        if (industry_open > 0)
        {
            int fork_a[3] = {-1, -1, -1}, fork_b[3] = {-1, -1, -1}, nf = 0;
            for (int n = 0; n < io::industry_tree::node_count && nf < 3; ++n)
            {
                const int x = io::industry_tree::nodes[n].excludes;
                if (x > n) { fork_a[nf] = n; fork_b[nf] = x; ++nf; }
            }
            row.industry_ran = true;
            for (const polity& q : traced.polities)
            {
                if (!q.alive) continue;
                ++row.industry_alive;
                int held_n = 0;
                for (uint64_t m = q.industry_mask; m != 0; m &= m - 1) ++held_n;
                row.industry_nodes.push_back(held_n);
                if (held_n > 0) ++row.industry_invested;
                if (polity_holds_industry_rim(q)) ++row.industry_rim;
                // Had an Industry round? See `industry_had_round`'s comment.
                if (q.industry_investing >= 0 || q.industry_mask != 0)
                {
                    ++row.industry_had_round;
                    if (!q.industry_fuel_seen) ++row.industry_fuel_never;
                }
                else ++row.industry_no_round;
                for (int f = 0; f < nf; ++f)
                {
                    const bool a = (q.industry_mask >> fork_a[f]) & 1ULL;
                    const bool b = (q.industry_mask >> fork_b[f]) & 1ULL;
                    ++row.industry_forks[f][a ? 0 : b ? 1 : 2];
                }
            }
            std::sort(row.industry_nodes.begin(), row.industry_nodes.end());
            row.urban_mass_close = living_urban_masses(ss_copy.regions, traced.polities);
            std::printf("  industry seed %d: alive %d invested %d | nodes held min/p25/med/p75/max %lld/%lld/%lld/%lld/%lld"
                        " | rim %d | had a round %d, of which never passed fuel %d | no round %d | forks",
                        i, row.industry_alive, row.industry_invested,
                        pct_of_sorted(row.industry_nodes, 0), pct_of_sorted(row.industry_nodes, 25),
                        pct_of_sorted(row.industry_nodes, 50), pct_of_sorted(row.industry_nodes, 75),
                        pct_of_sorted(row.industry_nodes, 100), row.industry_rim,
                        row.industry_had_round, row.industry_fuel_never, row.industry_no_round);
            for (int f = 0; f < nf; ++f)
                std::printf(" %s %d / %s %d / neither %d%s", io::industry_tree::nodes[fork_a[f]].id,
                            row.industry_forks[f][0], io::industry_tree::nodes[fork_b[f]].id,
                            row.industry_forks[f][1], row.industry_forks[f][2], f + 1 < nf ? " ;" : "");
            std::printf(" | urban mass at close med/max %lld/%lld\n",
                        pct_of_sorted(row.urban_mass_close, 50), pct_of_sorted(row.urban_mass_close, 100));
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
                if (m >= 1000) ++row.near_pairs_saturated; // BL-1018
                row.near_alarm_max_q = std::max(row.near_alarm_max_q, m);
            }
            // BL-1018: the raw capability behind every near-home read.
            row.capability_reference = ep2.visible_capability_reference;
            row.near_capability      = traced.near_capability_trace;
            std::sort(row.near_capability.begin(), row.near_capability.end());

            // BL-1019: the first crossing's funnel, and the geography it
            // had to work with at 1200.
            row.contacts_raised[0] = traced.contacts_raised_trace[0];
            row.contacts_raised[1] = traced.contacts_raised_trace[1];
            for (int k = 0; k < 3; ++k)
                for (int g = 0; g < 13; ++g) row.class_score[k][g] = traced.class_score_trace[k][g];
            for (int g = 0; g < 5; ++g) row.unmet_contest[g] = traced.unmet_contest_trace[g];
            for (int g = 0; g < 8; ++g) row.unmet_lost_verb[g] = traced.unmet_lost_to_verb_trace[g];
            row.unmet_margin_sum = traced.unmet_contest_margin_sum;
            {
                const std::vector<polity>& P = fx.pre_exploration_polities;
                const std::vector<region>& R = fx.pre_exploration_settlement.regions;
                const std::vector<int32_t> mass = label_landmasses(fx.terrain.substrate, fx.gw, fx.gh);
                const auto mass_of = [&](const polity& q) -> int32_t {
                    if (q.capital < 0 || static_cast<std::size_t>(q.capital) >= R.size()) return -1;
                    const region& rg = R[static_cast<std::size_t>(q.capital)];
                    if (rg.col < 0 || rg.row < 0 || rg.col >= fx.gw || rg.row >= fx.gh) return -1;
                    return mass[static_cast<std::size_t>(rg.row * fx.gw + rg.col)];
                };
                const std::size_t np = P.size();
                const auto alive_id = [&](int id) {
                    return id >= 0 && static_cast<std::size_t>(id) < np && P[static_cast<std::size_t>(id)].alive;
                };
                // Touching pairs: any two differently-held regions of living
                // polities within the radius, as sorted unique (lo, hi) keys.
                std::vector<std::pair<int, int>> touching;
                for (std::size_t i = 0; i < R.size(); ++i)
                {
                    if (!alive_id(R[i].nation)) continue;
                    for (std::size_t j = i + 1; j < R.size(); ++j)
                    {
                        if (R[j].nation == R[i].nation || !alive_id(R[j].nation)) continue;
                        if (region_distance(R[i], R[j], fx.gw) > ep2.neighbour_radius) continue;
                        touching.push_back({std::min(R[i].nation, R[j].nation), std::max(R[i].nation, R[j].nation)});
                    }
                }
                std::sort(touching.begin(), touching.end());
                touching.erase(std::unique(touching.begin(), touching.end()), touching.end());
                std::vector<int32_t> masses;
                for (std::size_t a = 0; a < np; ++a)
                {
                    if (!P[a].alive) continue;
                    ++row.polities_1200;
                    if (mass_of(P[a]) >= 0) masses.push_back(mass_of(P[a]));
                    for (std::size_t b = a + 1; b < np; ++b)
                    {
                        if (!P[b].alive) continue;
                        ++row.pairs_1200;
                        if (contact_exists(fx.pre_exploration_contacts, static_cast<int>(a), static_cast<int>(b)))
                            continue;
                        ++row.unmet_pairs_1200;
                        if (!std::binary_search(touching.begin(), touching.end(),
                                                std::pair<int, int>{static_cast<int>(a), static_cast<int>(b)}))
                            continue;
                        ++row.unmet_touching_1200;
                        if (mass_of(P[a]) != mass_of(P[b])) ++row.unmet_touching_cross_mass_1200;
                    }
                }
                std::sort(masses.begin(), masses.end());
                masses.erase(std::unique(masses.begin(), masses.end()), masses.end());
                row.landmasses_with_polity_1200 = static_cast<int64_t>(masses.size());
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

            // BL-1016: the ranks the classification reads -- the sim's own
            // function over the same 1660 close, so the sweep's scale is the
            // scale BL-955's spend scorer reads.
            std::vector<int> expn_rank, cons_rank;
            exploration_lean_ranks(traced.polities, &cs_copy, expn_rank, cons_rank);

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
                    e.cons_rank_q = cons_rank[p];
                    e.expn_rank_q = expn_rank[p];

                    exploration_row::lean_point lp;
                    lp.cons_q = e.cons_q;           lp.expn_q = e.expn_q;
                    lp.cons_rank_q = e.cons_rank_q; lp.expn_rank_q = e.expn_rank_q;
                    lp.sea_q = std::clamp(cu.sea_legs_q, 0, 1000);
                    if (cu.pantheon.size() >= 2)
                    {
                        lp.dominion_q = std::clamp(cu.pantheon[1].dominion, 0, 10) * 100;
                        lp.zeal_q     = std::clamp(cu.pantheon[1].zeal, 0, 10) * 100;
                    }
                    row.lean_population.push_back(lp);
                }
                else
                {
                    // no culture: neither lean, never ranked, never classified
                    e.cons_q = e.expn_q = -1;
                    ++row.living_uncultured;
                }
                entries.push_back(e);
            }

            if (entries.size() >= 2)
            {
                const std::size_t top_n = std::min<std::size_t>(3, entries.size());
                auto take_top = [&](std::vector<exploration_row::strength_entry> v,
                                    bool by_treasury,
                                    std::vector<exploration_row::strength_entry>& out,
                                    bool& has_cons, bool& has_expn, bool& both_distinct) {
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
                        if (reads_consolidator(v[k].cons_rank_q)) has_cons = true;
                        if (reads_expansionist(v[k].expn_rank_q)) has_expn = true;
                    }
                    // Both strategies in DIFFERENT realms: one realm reading as
                    // both is not two ways to be strong.
                    for (std::size_t a = 0; a < top_n; ++a)
                        for (std::size_t b = 0; b < top_n; ++b)
                            if (a != b && reads_consolidator(v[a].cons_rank_q)
                                       && reads_expansionist(v[b].expn_rank_q))
                                both_distinct = true;
                };
                take_top(entries, true,  row.top_by_treasury,
                         row.top_has_consolidator, row.top_has_expansionist, row.top_both_distinct);
                take_top(entries, false, row.top_by_regions,
                         row.regions_top_has_consolidator, row.regions_top_has_expansionist,
                         row.regions_top_both_distinct);
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

        // --- BL-1028: THE WEAKNESS COUNTERS, PER HALF --------------------------
        // Half A is 1200 -> 1660, half B 1660 -> --through. At --through 1660
        // (or earlier) the traced re-run above IS half A and no half B exists.
        // Past 1660, ONE more traced re-run is built exactly as the one above
        // (the same `ep2` -- resume pointers, overrides, seed, fx.works, the
        // band's step) but stopped at 1660; half A reads it, and half B's
        // cumulative fields are the longer run's less the shorter's. The sim's
        // year loop reads `stop_year` only as its bound, so the shorter run is
        // a prefix of the longer one; that is CHECKED per seed, not assumed.
        {
            constexpr int64_t kHalfBoundary = 1660;
            const std::vector<int32_t> mass_k = label_landmasses(fx.terrain.substrate, fx.gw, fx.gh);
            const int64_t reference = std::max<int64_t>(1, ep2.visible_capability_reference);

            const auto read_cumulative = [&](const history_sim_state& st, weakness_half& h) {
                h.contacts_raised[0] = st.contacts_raised_trace[0];
                h.contacts_raised[1] = st.contacts_raised_trace[1];
                h.alarm_reads = static_cast<int64_t>(st.near_capability_trace.size());
                for (int64_t c : st.near_capability_trace)
                    if (std::max<int64_t>(0, c) * 1000 / reference >= 1000) ++h.alarm_reads_ceiling;
                for (const battle_trace& bt : st.battle_traces)
                {
                    if (bt.defender == 0 && bt.attacker == 0) { ++h.ambiguous_battles; continue; }
                    if (contact_exists(fx.pre_exploration_contacts, bt.attacker, bt.defender)) ++h.neighbour_battles;
                    else                                                                      ++h.frontier_battles;
                }
                for (int k = 0; k < 3; ++k)
                    for (int g = 0; g < 6; ++g) h.funnel[k][g] = st.campaign_class_trace[k][g];
                for (int g = 0; g < 5; ++g) h.unmet_contest[g] = st.unmet_contest_trace[g];
                h.unmet_margin_sum = st.unmet_contest_margin_sum;
                for (int g = 0; g < 8; ++g) h.unmet_lost_verb[g] = st.unmet_lost_to_verb_trace[g];
            };
            const auto read_at_stop = [&](const history_sim_state& st, const std::vector<region>& regs,
                                          weakness_half& h, int64_t stop_year) {
                for (const contact& c : st.contacts)
                {
                    if (c.from >= c.to) continue;
                    if (c.first.year >= ep2.start_year)
                    {
                        bool non_aggression = false, any = false;
                        for (const dated_object& o : st.dated_objects)
                        {
                            if (!((o.a == c.from && o.b == c.to) || (o.a == c.to && o.b == c.from))) continue;
                            if (o.kind < 0 || o.kind >= treaty_clause_count) continue;
                            // Standing AT the stop: a clause is gone once a round reaches its
                            // expires_year, and a run's last round falls before its stop, so
                            // expire here as the handoff fold does (cold review, BL-1028).
                            if (o.expires_year <= stop_year) continue;
                            any = true;
                            if (o.kind == static_cast<int32_t>(treaty_clause::non_aggression)) non_aggression = true;
                        }
                        ++h.met_pairs;
                        if (non_aggression) ++h.met_pairs_bound;
                        if (any)            ++h.met_pairs_any_treaty;
                    }
                    else
                    {
                        const int m = std::max(deterrence_alarm_q(regs, st, ep2, c.from, c.to),
                                               deterrence_alarm_q(regs, st, ep2, c.to, c.from));
                        ++h.near_pairs;
                        if (m > 0)     ++h.near_pairs_alarmed;
                        if (m >= 1000) ++h.near_pairs_ceiling;
                        h.near_alarm_max_q = std::max(h.near_alarm_max_q, m);
                    }
                }
                const auto mass_of_capital = [&](int pid) -> int32_t {
                    if (pid < 0 || static_cast<std::size_t>(pid) >= st.polities.size()) return -1;
                    const int cap = st.polities[static_cast<std::size_t>(pid)].capital;
                    if (cap < 0 || static_cast<std::size_t>(cap) >= regs.size()) return -1;
                    const region& rg = regs[static_cast<std::size_t>(cap)];
                    if (rg.col < 0 || rg.row < 0 || rg.col >= fx.gw || rg.row >= fx.gh) return -1;
                    return mass_k[static_cast<std::size_t>(rg.row * fx.gw + rg.col)];
                };
                for (const trade_flow& f : st.trade_flows)
                {
                    h.flow_volume += f.volume_q;
                    const int32_t ms = mass_of_capital(f.seller), mb = mass_of_capital(f.buyer);
                    if (ms < 0 || mb < 0) h.unknown_landmass_volume += f.volume_q;
                    else if (ms != mb)    h.cross_landmass_volume   += f.volume_q;
                }
            };

            weakness_half to_through;
            read_cumulative(traced, to_through);
            read_at_stop(traced, ss_copy.regions, to_through, through_year);

            if (through_year > kHalfBoundary)
            {
                history_sim_params ep_a = ep2;
                ep_a.stop_year       = kHalfBoundary;
                ep_a.tick_bands[0]   = {kHalfBoundary, ep2.tick_bands[0].step_years};
                ep_a.tick_band_count = 1;
                settlement_state ss_a = fx.pre_exploration_settlement;
                creed_state      cs_a = fx.pre_exploration_creeds;
                const history_sim_state traced_a = run_history_sim(
                    ss_a, &cs_a, fx.terrain.view(), fx.gw, fx.gh, ep_a,
                    fx.exploration_seed, /*year_progress=*/nullptr, fx.works, /*tap=*/nullptr);

                // BL-1038: what the Industry rate reads at 1660, off the same
                // prefix run (the switch cannot have fired before its open
                // year, so this is the rate's opening input).
                if (industry_open > 0)
                {
                    row.urban_mass_open = living_urban_masses(ss_a.regions, traced_a.polities);
                    std::printf("  industry seed %d: urban mass (top-%d) at 1660 over %zu living: p25/med/p75/max %lld/%lld/%lld/%lld\n",
                                i, industry_research_top_k, row.urban_mass_open.size(),
                                pct_of_sorted(row.urban_mass_open, 25), pct_of_sorted(row.urban_mass_open, 50),
                                pct_of_sorted(row.urban_mass_open, 75), pct_of_sorted(row.urban_mass_open, 100));
                }

                weakness_half a;
                read_cumulative(traced_a, a);
                read_at_stop(traced_a, ss_a.regions, a, kHalfBoundary);
                a.from_year = ep2.start_year; a.to_year = kHalfBoundary;
                weakness_half b = weakness_minus(to_through, a);
                b.from_year = kHalfBoundary;  b.to_year = through_year;

                // THE PREFIX CHECK: every battle the short run traced is the
                // long run's battle at the same index (year, pair, ground,
                // outcome), all dated before the boundary, and the long run's
                // remaining battles all dated at or after it; the alarm reads
                // agree entry for entry; and no cumulative difference is
                // negative.
                bool prefix = traced_a.battle_traces.size() <= traced.battle_traces.size()
                           && traced_a.near_capability_trace.size() <= traced.near_capability_trace.size();
                for (std::size_t k = 0; prefix && k < traced_a.battle_traces.size(); ++k)
                {
                    const battle_trace& s = traced_a.battle_traces[k];
                    const battle_trace& l = traced.battle_traces[k];
                    prefix = s.year == l.year && s.attacker == l.attacker && s.defender == l.defender
                          && s.region == l.region && s.attacker_won == l.attacker_won
                          && s.year < kHalfBoundary;
                }
                for (std::size_t k = traced_a.battle_traces.size(); prefix && k < traced.battle_traces.size(); ++k)
                    prefix = traced.battle_traces[k].year >= kHalfBoundary;
                if (prefix)
                    prefix = std::equal(traced_a.near_capability_trace.begin(), traced_a.near_capability_trace.end(),
                                        traced.near_capability_trace.begin());
                prefix = prefix && weakness_cumulative_nonnegative(b);

                row.weak_a = a;
                row.weak_b = b;
                row.weak_have_b    = true;
                row.weak_prefix_ok = prefix;
            }
            else
            {
                to_through.from_year = ep2.start_year; to_through.to_year = through_year;
                row.weak_a = to_through;
            }
        }

        // --- BL-1027: THE SPAN'S COST ----------------------------------------
        // Two UNTRACED re-runs from the fixture, one stopped at 1660 and one at
        // --through, so the later half is the difference of two clocks on the
        // same world. Untraced because generation never pays for tracing, and
        // built exactly as the traced re-run above is, minus the trace.
        if (run_cost)
        {
            const auto run_to = [&](int64_t stop) {
                history_sim_params hp = fx.exploration_params;
                hp.stop_year        = stop;
                hp.tick_bands[0]    = {stop, fx.exploration_params.tick_bands[0].step_years};
                hp.tick_band_count  = 1;
                hp.trace_battles    = false;
                hp.resume_polities  = &fx.pre_exploration_polities;
                hp.resume_grudges   = &fx.pre_exploration_grudges;
                hp.resume_contacts  = &fx.pre_exploration_contacts;
                hp.resume_corridors = &fx.pre_exploration_corridors;
                settlement_state ss = fx.pre_exploration_settlement;
                creed_state      cs = fx.pre_exploration_creeds;
                const auto t0 = std::chrono::steady_clock::now();
                const history_sim_state st = run_history_sim(
                    ss, &cs, fx.terrain.view(), fx.gw, fx.gh, hp,
                    fx.exploration_seed, /*year_progress=*/nullptr, fx.works, /*tap=*/nullptr);
                const auto t1 = std::chrono::steady_clock::now();

                exploration_row::span_cost c;
                c.wall_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
                const history_sim_profile& pr = history_sim_last_profile();
                c.ns_demography   = pr.ns_demography;
                c.ns_decisions    = pr.ns_decisions;
                c.ns_battles      = pr.ns_battles;
                c.ns_reach        = pr.ns_reach;
                c.decision_rounds = pr.decision_rounds;
                c.reach_rebuilds  = pr.reach_rebuilds;
                c.battles   = st.battles;
                c.conquests = st.conquests;
                c.foundings = st.foundings;
                c.regions   = static_cast<int64_t>(ss.regions.size());
                int64_t nodes = 0;
                for (const polity& q : st.polities)
                {
                    if (!q.alive) continue;
                    ++c.alive;
                    for (uint64_t m = q.exploration_mask; m != 0; m &= m - 1) ++nodes;
                    if (polity_holds_exploration_rim(q)) ++c.rim_holders;
                }
                c.mean_exploration_nodes = c.alive > 0 ? static_cast<double>(nodes) / static_cast<double>(c.alive) : 0.0;
                return c;
            };
            row.cost_ran   = true;
            row.to_1660    = run_to(1660);
            row.to_through = run_to(through_year);
            row.cost_reproduces = row.to_through.battles   == fx.exploration_state.battles
                               && row.to_through.conquests == fx.exploration_state.conquests
                               && row.to_through.foundings == fx.exploration_state.foundings;
            row.regions_1200 = static_cast<int64_t>(fx.pre_exploration_settlement.regions.size());
            for (const polity& q : fx.pre_exploration_polities) if (q.alive) ++row.alive_1200;
        }

        row.ok = true;
        rows.push_back(row);
    }

    // -----------------------------------------------------------------------
    // BL-1038 — THE INDUSTRY TREE, per seed and pooled. REPORTED, not gated:
    // the sizing rule (TREES.md sec Sizes: the leading polity finishes just
    // before the phase ends, the median reaches halfway) is the reading the
    // rate's first-cut constants are judged against, and that is a call.
    // -----------------------------------------------------------------------
    if (industry_open > 0)
    {
        std::printf("\n--- the Industry tree (BL-1038), switch on from %lld, read at %lld ---\n",
                    static_cast<long long>(industry_open), static_cast<long long>(through_year));
        std::printf("  'had round' = at least one Industry round (investing or holding); 'no fuel' counts only those;"
                    " 'no round' never invested after the open year.\n");
        std::printf("  seed | alive | invested | nodes min/p25/med/p75/max | rim | had round | no fuel (of had) | no round |"
                    " Fuel coke/charcoal/- | Labour cleared/smallholder/- | Works arsenal/private/- | urban@1660 med/max |"
                    " urban@close med/max\n");
        std::vector<int> pooled_nodes;
        int p_alive = 0, p_inv = 0, p_rim = 0, p_never = 0, p_had = 0, p_noround = 0, p_forks[3][3] = {};
        std::vector<int> seed_max, seed_med;
        for (const exploration_row& r : rows)
        {
            if (!r.industry_ran) continue;
            std::printf("  %4u | %5d | %8d | %3lld/%3lld/%3lld/%3lld/%3lld | %3d | %9d | %3d (%3.0f%%) | %8d | %3d/%3d/%3d | %3d/%3d/%3d | %3d/%3d/%3d | %lld/%lld | %lld/%lld\n",
                        r.seed, r.industry_alive, r.industry_invested,
                        pct_of_sorted(r.industry_nodes, 0), pct_of_sorted(r.industry_nodes, 25),
                        pct_of_sorted(r.industry_nodes, 50), pct_of_sorted(r.industry_nodes, 75),
                        pct_of_sorted(r.industry_nodes, 100), r.industry_rim, r.industry_had_round,
                        r.industry_fuel_never,
                        r.industry_had_round > 0 ? 100.0 * r.industry_fuel_never / r.industry_had_round : 0.0,
                        r.industry_no_round,
                        r.industry_forks[0][0], r.industry_forks[0][1], r.industry_forks[0][2],
                        r.industry_forks[1][0], r.industry_forks[1][1], r.industry_forks[1][2],
                        r.industry_forks[2][0], r.industry_forks[2][1], r.industry_forks[2][2],
                        pct_of_sorted(r.urban_mass_open, 50), pct_of_sorted(r.urban_mass_open, 100),
                        pct_of_sorted(r.urban_mass_close, 50), pct_of_sorted(r.urban_mass_close, 100));
            pooled_nodes.insert(pooled_nodes.end(), r.industry_nodes.begin(), r.industry_nodes.end());
            p_alive += r.industry_alive; p_inv += r.industry_invested; p_rim += r.industry_rim;
            p_never += r.industry_fuel_never; p_had += r.industry_had_round; p_noround += r.industry_no_round;
            for (int f = 0; f < 3; ++f) for (int s = 0; s < 3; ++s) p_forks[f][s] += r.industry_forks[f][s];
            if (!r.industry_nodes.empty())
            {
                seed_max.push_back(r.industry_nodes.back());
                seed_med.push_back(static_cast<int>(pct_of_sorted(r.industry_nodes, 50)));
            }
        }
        std::sort(pooled_nodes.begin(), pooled_nodes.end());
        std::sort(seed_max.begin(), seed_max.end());
        std::sort(seed_med.begin(), seed_med.end());
        std::printf("  POOLED: alive %d, invested %d, nodes held min/p25/med/p75/max %lld/%lld/%lld/%lld/%lld of %d,"
                    " rim %d; had an Industry round %d, of which never passed fuel %d (%.0f%%); never had a round %d"
                    " (%.0f%% of alive)\n",
                    p_alive, p_inv, pct_of_sorted(pooled_nodes, 0), pct_of_sorted(pooled_nodes, 25),
                    pct_of_sorted(pooled_nodes, 50), pct_of_sorted(pooled_nodes, 75),
                    pct_of_sorted(pooled_nodes, 100), io::industry_tree::node_count, p_rim, p_had, p_never,
                    p_had > 0 ? 100.0 * p_never / p_had : 0.0, p_noround,
                    p_alive > 0 ? 100.0 * p_noround / p_alive : 0.0);
        std::printf("  POOLED forks: Fuel coke %d / charcoal %d / neither %d; Labour cleared %d / smallholder %d /"
                    " neither %d; Works arsenal %d / private %d / neither %d\n",
                    p_forks[0][0], p_forks[0][1], p_forks[0][2], p_forks[1][0], p_forks[1][1], p_forks[1][2],
                    p_forks[2][0], p_forks[2][1], p_forks[2][2]);
        std::printf("  SIZING (per seed): leader's nodes med %lld (range %lld-%lld); median polity's nodes med %lld"
                    " (range %lld-%lld). The rule asks the leader to finish (%d, one side per fork) and the"
                    " median to reach halfway.\n",
                    pct_of_sorted(seed_max, 50), pct_of_sorted(seed_max, 0), pct_of_sorted(seed_max, 100),
                    pct_of_sorted(seed_med, 50), pct_of_sorted(seed_med, 0), pct_of_sorted(seed_med, 100),
                    io::industry_tree::node_count - 3);
    }

    // -----------------------------------------------------------------------
    // BL-1027 — THE SPAN'S COST, per seed. Wall clock on whatever build ran
    // this; the build type is printed because a Debug figure is 12-80x a
    // Release one and means nothing quoted alone.
    // -----------------------------------------------------------------------
    if (run_cost)
    {
#ifdef NDEBUG
        const char* build_kind = "Release (NDEBUG)";
#else
        const char* build_kind = "DEBUG - these timings are meaningless";
#endif
        const long long T = static_cast<long long>(through_year);
        std::printf("\n--- span cost (BL-1027), %s, two untraced re-runs per seed ---\n", build_kind);
        std::printf("  Half A = 1200 -> 1660; half B = 1660 -> %lld, the difference of the two runs.\n", T);
        std::printf("  seed | regions 1200/1660/%lld | alive 1200/1660/%lld | A ms (rounds, rebuilds, reach%%) | B ms (rounds, rebuilds, reach%%)"
                    " | battles/century A B | conquests/century A B | expl nodes held A B | rim holders A B\n", T, T);
        const double years_a = 460.0, years_b = static_cast<double>(through_year - 1660);
        std::vector<int64_t> ms_a, ms_b;
        for (const exploration_row& r : rows)
        {
            if (!r.cost_ran) continue;
            const exploration_row::span_cost& a = r.to_1660;
            const exploration_row::span_cost& t = r.to_through;
            const int64_t b_ms = t.wall_ms - a.wall_ms;
            const auto pct = [](int64_t part, int64_t whole) { return whole > 0 ? 100.0 * part / whole : 0.0; };
            const double b_battles = years_b > 0 ? 100.0 * (t.battles - a.battles) / years_b : 0.0;
            const double b_conq    = years_b > 0 ? 100.0 * (t.conquests - a.conquests) / years_b : 0.0;
            std::printf("  %4u | %lld/%lld/%lld | %lld/%lld/%lld | %lld (%lld, %lld, %.0f%%) | %lld (%lld, %lld, %.0f%%)"
                        " | %.1f %.1f | %.2f %.2f | %.1f %.1f | %lld %lld%s\n",
                        r.seed,
                        (long long)r.regions_1200, (long long)a.regions, (long long)t.regions,
                        (long long)r.alive_1200, (long long)a.alive, (long long)t.alive,
                        (long long)a.wall_ms, (long long)a.decision_rounds, (long long)a.reach_rebuilds,
                        pct(a.ns_reach, a.ns_demography + a.ns_decisions),
                        (long long)b_ms, (long long)(t.decision_rounds - a.decision_rounds),
                        (long long)(t.reach_rebuilds - a.reach_rebuilds),
                        pct(t.ns_reach - a.ns_reach, (t.ns_demography + t.ns_decisions) - (a.ns_demography + a.ns_decisions)),
                        100.0 * a.battles / years_a, b_battles, 100.0 * a.conquests / years_a, b_conq,
                        a.mean_exploration_nodes, t.mean_exploration_nodes,
                        (long long)a.rim_holders, (long long)t.rim_holders,
                        r.cost_reproduces ? "" : "  [DOES NOT REPRODUCE GENERATION]");
            ms_a.push_back(a.wall_ms);
            ms_b.push_back(b_ms);
        }
        const auto summary = [](const char* label, std::vector<int64_t> v) {
            if (v.empty()) return;
            std::sort(v.begin(), v.end());
            int64_t sum = 0;
            for (int64_t x : v) sum += x;
            std::printf("  %s: median %lld ms, min %lld, max %lld, total %lld ms over %zu seeds\n", label,
                        (long long)v[v.size() / 2], (long long)v.front(), (long long)v.back(), (long long)sum, v.size());
        };
        summary("half A (1200 -> 1660)", ms_a);
        summary("half B (1660 -> through)", ms_b);
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
    if (run_cost)
    {
        bool all_cost_reproduce = true;
        for (const exploration_row& r : rows)
            if (r.ok && r.cost_ran && !r.cost_reproduces) all_cost_reproduce = false;
        check(all_cost_reproduce,
            "BL-1027: the untraced cost run to --through reproduces generation's own counts, every ran seed");
    }
    if (through_year > 1660)
    {
        bool all_prefix = true;
        for (const exploration_row& r : rows)
            if (r.ok && r.weak_have_b && !r.weak_prefix_ok) all_prefix = false;
        check(all_prefix,
            "BL-1028: the traced run stopped at 1660 is a prefix of the traced run to --through "
            "(battle traces, alarm reads, cumulative counters), every ran seed");
    }

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
        // BL-1018 alarm spread: pooled over every near-home treaty read.
        int64_t a_reads = 0, a_reference = 0;
        int64_t a_cap[8] = {0, 0, 0, 0, 0, 0, 0, 0};  ///< p10 p25 p50 p75 p90 p95 p99 max
        int     a_alarm[5] = {0, 0, 0, 0, 0};         ///< p10 p25 p50 p75 p90
        double  a_saturated_share = 0.0, a_zero_share = 0.0;
        int64_t a_hist[12] = {};                      ///< [0], (0,100), [100,200) .. [900,1000), [1000]
        int64_t a_seed_p90_median = 0;
        int64_t a_close_saturated = 0, a_close_pairs = 0;
        // BL-1028 weakness counters, pooled per half: the summed half, the
        // median of per-seed displacement ratios over seeds with any
        // neighbour battle, and how many seeds that median is taken over.
        weakness_half w_pooled[2];
        bool    w_have_median[2] = {false, false};
        double  w_median_ratio[2] = {0.0, 0.0};
        int     w_seeds_with_ratio[2] = {0, 0};
        int     w_seeds[2] = {0, 0};
        bool    w_have_b = false;
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
                r3_reg_cons = 0, r3_reg_expn = 0, r3_reg_both = 0,
                r3_both_distinct = 0, r3_reg_both_distinct = 0; // BL-1016
        // BL-1016: the pooled lean population. pct[] is min, p10, p25, median,
        // p75, p90, max; -1 throughout when nothing was measured.
        int64_t r3_pop = 0, r3_uncultured = 0, r3_raw_cons_gt_expn = 0,
                r3_class_c = 0, r3_class_e = 0, r3_class_b = 0, r3_class_none = 0;
        int     r3_cons_pct[7] = {-1, -1, -1, -1, -1, -1, -1};
        int     r3_expn_pct[7] = {-1, -1, -1, -1, -1, -1, -1};
        int     r3_sea_pct[7]  = {-1, -1, -1, -1, -1, -1, -1};
        int     r3_dom_pct[7]  = {-1, -1, -1, -1, -1, -1, -1};
        int     r3_zeal_pct[7] = {-1, -1, -1, -1, -1, -1, -1};
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

    // -----------------------------------------------------------------------
    // THE ALARM SPREAD (BL-1018, Ben's ruling 2026-09-16, NR-880). Deterrence
    // acts through near-home Alarm = min(1000, capability * 1000 / reference),
    // and a reference the capabilities outgrow reads 1000 on every pair -- a
    // flat constant, not a discriminator. So this section reads the RAW
    // capability every near-home treaty read saw (both sides, every decision
    // round, formation and break alike -- `near_capability_trace`), the
    // quantiles the reference would be derived from, and the alarm those reads
    // come to through the reference in effect. Pooled over the spread, since
    // the reference is one constant for every world; each seed's own line sits
    // beside it so one heavy seed cannot hide. The close snapshot (pairs at the
    // close reading the ceiling) is printed as the cross-check. Whole traced
    // run (1200 -> --through); the per-half split is BL-1028's section.
    // Report only.
    // -----------------------------------------------------------------------
    {
        const auto at_pct = [](const std::vector<int64_t>& v, int pct) -> int64_t {
            if (v.empty()) return 0;
            return v[(v.size() - 1) * static_cast<std::size_t>(pct) / 100];
        };
        const auto alarm_of = [](int64_t cap, int64_t ref) -> int {
            return static_cast<int>(std::min<int64_t>(1000, std::max<int64_t>(0, cap) * 1000
                                                             / std::max<int64_t>(1, ref)));
        };
        std::printf("\n--- alarm spread over near pairs (BL-1018) -- every near-home treaty read, "
                    "both sides, every decision round ---\n");
        std::printf("%-6s %9s %9s | %9s %9s %9s %9s | %6s %6s %6s %7s %7s | %s\n",
                    "seed", "reads", "reference", "cap.p50", "cap.p90", "cap.p99", "cap.max",
                    "al.p10", "al.p50", "al.p90", "al=1000", "al=0", "close: near pairs at 1000");
        std::vector<int64_t> pooled;
        std::vector<int64_t> seed_p90;
        int64_t reference = 0;
        for (const exploration_row& r : rows)
        {
            if (!r.ok) continue;
            reference = r.capability_reference; // one value per run: every row's traced params agree
            face.a_close_saturated += r.near_pairs_saturated;
            face.a_close_pairs     += r.near_pairs;
            const std::vector<int64_t>& v = r.near_capability;
            int64_t sat = 0, zero = 0;
            for (int64_t c : v)
            {
                const int a = alarm_of(c, r.capability_reference);
                if (a >= 1000) ++sat;
                if (a == 0) ++zero;
            }
            if (!v.empty()) seed_p90.push_back(at_pct(v, 90));
            const double n = static_cast<double>(std::max<std::size_t>(1, v.size()));
            std::printf("%-6u %9zu %9lld | %9lld %9lld %9lld %9lld | %6d %6d %6d %6.1f%% %6.1f%% | %lld/%lld\n",
                        r.seed, v.size(), static_cast<long long>(r.capability_reference),
                        static_cast<long long>(at_pct(v, 50)), static_cast<long long>(at_pct(v, 90)),
                        static_cast<long long>(at_pct(v, 99)), static_cast<long long>(v.empty() ? 0 : v.back()),
                        alarm_of(at_pct(v, 10), r.capability_reference),
                        alarm_of(at_pct(v, 50), r.capability_reference),
                        alarm_of(at_pct(v, 90), r.capability_reference),
                        100.0 * static_cast<double>(sat) / n, 100.0 * static_cast<double>(zero) / n,
                        static_cast<long long>(r.near_pairs_saturated), static_cast<long long>(r.near_pairs));
            pooled.insert(pooled.end(), v.begin(), v.end());
        }
        std::sort(pooled.begin(), pooled.end());
        std::sort(seed_p90.begin(), seed_p90.end());
        face.a_reads = static_cast<int64_t>(pooled.size());
        face.a_reference = reference;
        const int cap_pcts[7] = {10, 25, 50, 75, 90, 95, 99};
        for (int k = 0; k < 7; ++k) face.a_cap[k] = at_pct(pooled, cap_pcts[k]);
        face.a_cap[7] = pooled.empty() ? 0 : pooled.back();
        const int alarm_pcts[5] = {10, 25, 50, 75, 90};
        for (int k = 0; k < 5; ++k) face.a_alarm[k] = alarm_of(at_pct(pooled, alarm_pcts[k]), reference);
        int64_t sat = 0, zero = 0;
        for (int64_t c : pooled)
        {
            const int a = alarm_of(c, reference);
            if (a >= 1000) ++sat;
            if (a == 0) ++zero;
            const int bucket = a <= 0 ? 0 : (a >= 1000 ? 11 : (a < 100 ? 1 : 1 + a / 100));
            ++face.a_hist[bucket];
        }
        const double n = static_cast<double>(std::max<std::size_t>(1, pooled.size()));
        face.a_saturated_share = static_cast<double>(sat) / n;
        face.a_zero_share      = static_cast<double>(zero) / n;
        face.a_seed_p90_median = seed_p90.empty() ? 0 : seed_p90[seed_p90.size() / 2];

        if (pooled.empty())
            std::printf("  NOT MEASURED: no near-home treaty read on this spread.\n");
        else
        {
            std::printf("  POOLED over %lld reads, reference %lld:\n", static_cast<long long>(face.a_reads),
                        static_cast<long long>(reference));
            std::printf("    raw capability  p10 %lld  p25 %lld  p50 %lld  p75 %lld  p90 %lld  p95 %lld  p99 %lld  max %lld"
                        "   (median of per-seed p90: %lld)\n",
                        static_cast<long long>(face.a_cap[0]), static_cast<long long>(face.a_cap[1]),
                        static_cast<long long>(face.a_cap[2]), static_cast<long long>(face.a_cap[3]),
                        static_cast<long long>(face.a_cap[4]), static_cast<long long>(face.a_cap[5]),
                        static_cast<long long>(face.a_cap[6]), static_cast<long long>(face.a_cap[7]),
                        static_cast<long long>(face.a_seed_p90_median));
            std::printf("    alarm           p10 %d  p25 %d  p50 %d  p75 %d  p90 %d;  reading 1000: %.1f%%  reading 0: %.1f%%\n",
                        face.a_alarm[0], face.a_alarm[1], face.a_alarm[2], face.a_alarm[3], face.a_alarm[4],
                        100.0 * face.a_saturated_share, 100.0 * face.a_zero_share);
            std::printf("    alarm histogram  0:%.1f%%  1-99:%.1f%%", 100.0 * face.a_hist[0] / n, 100.0 * face.a_hist[1] / n);
            for (int b = 2; b <= 10; ++b)
                std::printf("  %d-%d:%.1f%%", (b - 1) * 100, (b - 1) * 100 + 99, 100.0 * face.a_hist[b] / n);
            std::printf("  1000:%.1f%%\n", 100.0 * face.a_hist[11] / n);
            std::printf("    close snapshot: %lld of %lld near pairs read the ceiling (%.1f%%)\n",
                        static_cast<long long>(face.a_close_saturated), static_cast<long long>(face.a_close_pairs),
                        face.a_close_pairs > 0 ? 100.0 * face.a_close_saturated / face.a_close_pairs : 0.0);
        }
    }

    // -----------------------------------------------------------------------
    // THE FIRST CROSSING (BL-1019, Ben's ruling 2026-09-16, NR-880). A pair
    // meets only when a campaign crosses onto the other's ground or a
    // conqueror inherits what its victim knew, so the frontier the
    // displacement reading needs is made by first crossings. Per seed: the
    // geography at 1200 (unmet pairs of living polities, and the ones that
    // TOUCH -- the only unmet pairs a campaign can ever reach), the
    // first-contact count over the traced run (new pairs raised, by kind), the
    // met-in-span pairs still standing at the close, and the unmet candidate's
    // funnel with what beat it on the rounds it cleared. Then, pooled, what
    // each contact class is scored on -- a first crossing set beside a known
    // neighbour. Whole traced run (1200 -> --through). Report only.
    // -----------------------------------------------------------------------
    {
        std::printf("\n--- first contact (BL-1019) -- geography at 1200, contacts raised over the traced run, "
                    "and the unmet candidate's contest ---\n");
        std::printf("%-6s %6s %7s %7s %7s %7s | %8s %8s %8s | %8s %7s %6s | %7s %6s %6s %6s %6s %7s | %7s %7s %7s %7s\n",
                    "seed", "pol.", "unmet", "touch", "t.xmass", "masses",
                    "1st.camp", "1st.inh", "met@cls",
                    "u.reach", "u.clear", "u.chos",
                    "u.rnds", "won", "l.near", "l.met", "l.verb", "margin",
                    "n.value", "u.value", "n.supp", "u.supp");
        for (const exploration_row& r : rows)
        {
            if (!r.ok) continue;
            const int64_t* u = r.campaign_class[2];
            const int64_t lost = r.unmet_contest[2] + r.unmet_contest[3] + r.unmet_contest[4];
            const auto mean_of = [&](int k, int g) {
                return r.class_score[k][0] > 0
                    ? static_cast<double>(r.class_score[k][g]) / static_cast<double>(r.class_score[k][0]) : 0.0;
            };
            std::printf("%-6u %6lld %7lld %7lld %7lld %7lld | %8lld %8lld %8lld | %8lld %7lld %6lld | %7lld %6lld %6lld %6lld %6lld %7.0f"
                        " | %7.1f %7.1f %7.1f %7.1f\n",
                        r.seed, static_cast<long long>(r.polities_1200), static_cast<long long>(r.unmet_pairs_1200),
                        static_cast<long long>(r.unmet_touching_1200),
                        static_cast<long long>(r.unmet_touching_cross_mass_1200),
                        static_cast<long long>(r.landmasses_with_polity_1200),
                        static_cast<long long>(r.contacts_raised[0]), static_cast<long long>(r.contacts_raised[1]),
                        static_cast<long long>(r.far_pairs),
                        static_cast<long long>(u[0] - u[1] - u[2] - u[3]), static_cast<long long>(u[4]),
                        static_cast<long long>(u[5]),
                        static_cast<long long>(r.unmet_contest[0]), static_cast<long long>(r.unmet_contest[1]),
                        static_cast<long long>(r.unmet_contest[2]), static_cast<long long>(r.unmet_contest[3]),
                        static_cast<long long>(r.unmet_contest[4]),
                        lost > 0 ? static_cast<double>(r.unmet_margin_sum) / static_cast<double>(lost) : 0.0,
                        mean_of(0, 4), mean_of(2, 4), mean_of(0, 3), mean_of(2, 3));
        }
        std::printf("  columns: pol. living polities at 1200; unmet/touch/t.xmass unmet pairs, of them touching within the "
                    "neighbour radius, of those with capitals on different landmasses; masses landmasses holding a capital; "
                    "1st.camp/1st.inh new contact pairs raised over the traced run by a crossing / by inheritance; met@cls "
                    "met-in-span pairs at the close; u.* the unmet class (in reach, season scores clearing, "
                    "rounds chosen); u.rnds rounds an unmet candidate cleared, won by it / lost to a near campaign / to a "
                    "met-in-span campaign / to another verb, and the mean winning margin over the lost rounds; n./u.value "
                    "and n./u.supp the mean value and supply of in-reach near-class / unmet-class candidates.\n");

        int64_t pooled[3][13] = {};
        int64_t lost_verb[8] = {};
        for (const exploration_row& r : rows)
            if (r.ok)
            {
                for (int k = 0; k < 3; ++k)
                    for (int g = 0; g < 13; ++g) pooled[k][g] += r.class_score[k][g];
                for (int g = 0; g < 8; ++g) lost_verb[g] += r.unmet_lost_verb[g];
            }
        std::printf("  POOLED, rounds an unmet candidate cleared and another VERB won, by verb: "
                    "settle %lld, invest %lld, consolidate %lld, build %lld, upgrade-supply %lld, organise %lld\n",
                    static_cast<long long>(lost_verb[static_cast<int>(sim_verb::settle)]),
                    static_cast<long long>(lost_verb[static_cast<int>(sim_verb::invest)]),
                    static_cast<long long>(lost_verb[static_cast<int>(sim_verb::consolidate)]),
                    static_cast<long long>(lost_verb[static_cast<int>(sim_verb::build_work)]),
                    static_cast<long long>(lost_verb[static_cast<int>(sim_verb::upgrade_supply)]),
                    static_cast<long long>(lost_verb[static_cast<int>(sim_verb::organise)]));
        const char* names[3] = {"near (met before 1200)", "met in span", "unmet (first crossing)"};
        std::printf("  POOLED, what an in-reach candidate is scored on, by contact class (means):\n");
        std::printf("    %-24s %9s %7s %7s %7s %7s %7s %7s %7s %7s %7s %7s %7s %7s %8s\n", "class", "scored",
                    "city", "ground", "prize", "p_win", "supply", "costed", "cult", "value", "def", "dist",
                    "sea-leg", "ally", "clear/scr");
        for (int k = 0; k < 3; ++k)
        {
            const double n = static_cast<double>(std::max<int64_t>(1, pooled[k][0]));
            int64_t cleared = 0;
            for (const exploration_row& r : rows) if (r.ok) cleared += r.campaign_class[k][4];
            std::printf("    %-24s %9lld %7.1f %7.1f %7.1f %7.1f %7.1f %7.1f %7.1f %7.1f %7.1f %7.1f %6.1f%% %6.1f%% %7.1f%%\n",
                        names[k], static_cast<long long>(pooled[k][0]),
                        pooled[k][8] / n, pooled[k][9] / n, pooled[k][1] / n, pooled[k][2] / n, pooled[k][3] / n,
                        pooled[k][10] / n, pooled[k][11] / n, pooled[k][4] / n, pooled[k][5] / n, pooled[k][6] / n,
                        100.0 * pooled[k][7] / n, 100.0 * pooled[k][12] / n,
                        100.0 * static_cast<double>(cleared) / (2.0 * n));
        }
        std::printf("    (city = campaign_prize_q; ground = farm/ore/port at the un-jittered weights; prize = the ground's "
                    "worth before odds; costed = after odds, distance and supply cost; cult = share foreignness leaves; "
                    "value = after every lean, before the defender and season terms; ally = discounted by a mutual-defence "
                    "ally; clear/scr = season scores clearing the threshold per season score)\n");
    }

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
    //
    // BL-1016 -- WHAT A REALM "IS" IS READ BY RANK. A consolidator is a realm
    // whose consolidator lean ranks in the top third of its world's living,
    // cultured polities; an expansionist likewise (see `kLeanTopThirdRankQ`
    // for why the raw leans cannot be compared). The whole population behind
    // the label is printed first, so the reader sees what "top third" meant on
    // each seed, and the verdict reads a consolidator and an expansionist in
    // DIFFERENT realms.
    // -----------------------------------------------------------------------
    std::printf("\n--- reading 3: both strategies pay (consolidator/expansionist, by creed) ---\n");
    std::printf("  metric: top-3 realms per seed ranked by CAPITAL TREASURY at 1660, tie-break mean "
                "held-region network_supply_q, then polity id (region-count ranking shown for comparison)\n");
    std::printf("  classifier (BL-1016): each lean as a per-mille RANK among the seed's living, cultured "
                "polities at 1660 (exploration_lean_ranks -- the scale BL-955's spend scorer reads); a realm "
                "is a consolidator when its consolidator rank >= %d (the top third by that lean), an "
                "expansionist likewise by expansion rank, so a realm may be both or neither. The raw leans "
                "are never compared: one is a product, the other a mean.\n", kLeanTopThirdRankQ);
    {
        // --- BL-1016: the population behind the label ----------------------
        std::vector<int> all_cons, all_expn, all_sea, all_dom, all_zeal;
        int64_t uncultured = 0, raw_cons_gt = 0, class_c = 0, class_e = 0, class_b = 0, class_none = 0;
        for (const exploration_row& r : rows)
        {
            if (!r.ok || !r.strength_measured) continue;
            uncultured += r.living_uncultured;
            for (const exploration_row::lean_point& lp : r.lean_population)
            {
                all_cons.push_back(lp.cons_q); all_expn.push_back(lp.expn_q);
                all_sea.push_back(lp.sea_q);   all_dom.push_back(lp.dominion_q);
                all_zeal.push_back(lp.zeal_q);
                if (lp.cons_q > lp.expn_q) ++raw_cons_gt;
                const bool c = reads_consolidator(lp.cons_rank_q), e = reads_expansionist(lp.expn_rank_q);
                if (c && e) ++class_b; else if (c) ++class_c; else if (e) ++class_e; else ++class_none;
            }
        }
        const int pcts[7] = {0, 10, 25, 50, 75, 90, 100};
        const auto fill_pct = [&](std::vector<int> v, int out[7]) {
            std::sort(v.begin(), v.end());
            for (int k = 0; k < 7; ++k) out[k] = percentile_of(v, pcts[k]);
        };
        fill_pct(all_cons, face.r3_cons_pct); fill_pct(all_expn, face.r3_expn_pct);
        fill_pct(all_sea, face.r3_sea_pct);   fill_pct(all_dom, face.r3_dom_pct);
        fill_pct(all_zeal, face.r3_zeal_pct);
        face.r3_pop = static_cast<int64_t>(all_cons.size()); face.r3_uncultured = uncultured;
        face.r3_raw_cons_gt_expn = raw_cons_gt;
        face.r3_class_c = class_c; face.r3_class_e = class_e; face.r3_class_b = class_b;
        face.r3_class_none = class_none;

        std::printf("  lean population, pooled: %lld living cultured polities at 1660 (%lld living with no "
                    "culture, unranked)\n", static_cast<long long>(face.r3_pop), static_cast<long long>(uncultured));
        const auto print_pct = [&](const char* label, const int v[7]) {
            std::printf("    %-22s min=%4d p10=%4d p25=%4d med=%4d p75=%4d p90=%4d max=%4d\n",
                        label, v[0], v[1], v[2], v[3], v[4], v[5], v[6]);
        };
        print_pct("consolidator_lean_q", face.r3_cons_pct);
        print_pct("expansion_lean_q",    face.r3_expn_pct);
        print_pct("  input sea_legs_q",  face.r3_sea_pct);
        print_pct("  input dominion x100", face.r3_dom_pct);
        print_pct("  input zeal x100",   face.r3_zeal_pct);
        std::printf("    raw consolidator lean > raw expansion lean (the retired comparison): %lld of %lld\n",
                    static_cast<long long>(raw_cons_gt), static_cast<long long>(face.r3_pop));
        std::printf("    by rank: consolidator only=%lld  expansionist only=%lld  both=%lld  neither=%lld\n",
                    static_cast<long long>(class_c), static_cast<long long>(class_e),
                    static_cast<long long>(class_b), static_cast<long long>(class_none));

        // Per seed: the raw lean a realm needed to read as top third.
        const auto floor_of = [](const std::vector<exploration_row::lean_point>& pop, bool cons) {
            int best = -1;
            for (const exploration_row::lean_point& lp : pop)
            {
                const bool in = cons ? reads_consolidator(lp.cons_rank_q) : reads_expansionist(lp.expn_rank_q);
                const int  v  = cons ? lp.cons_q : lp.expn_q;
                if (in && (best < 0 || v < best)) best = v;
            }
            return best;
        };
        for (const exploration_row& r : rows)
        {
            if (!r.ok || !r.strength_measured) continue;
            std::vector<int> c, e;
            for (const exploration_row::lean_point& lp : r.lean_population)
            { c.push_back(lp.cons_q); e.push_back(lp.expn_q); }
            std::sort(c.begin(), c.end()); std::sort(e.begin(), e.end());
            std::printf("    seed %u: n=%zu  cons p25/med/p75=%d/%d/%d top-third floor=%d  "
                        "expn p25/med/p75=%d/%d/%d top-third floor=%d\n",
                        r.seed, r.lean_population.size(),
                        percentile_of(c, 25), percentile_of(c, 50), percentile_of(c, 75),
                        floor_of(r.lean_population, true),
                        percentile_of(e, 25), percentile_of(e, 50), percentile_of(e, 75),
                        floor_of(r.lean_population, false));
        }

        auto print_top = [&](const char* label, const std::vector<exploration_row::strength_entry>& v) {
            std::printf("    %-9s", label);
            for (const auto& e : v)
                std::printf("  [p%d %s trs=%lld sup=%d reg=%lld cons=%d(r%d) expn=%d(r%d)]", e.id,
                            lean_tag_of(e.cons_rank_q, e.expn_rank_q),
                            static_cast<long long>(e.treasury), e.mean_supply_q,
                            static_cast<long long>(e.regions), e.cons_q, e.cons_rank_q,
                            e.expn_q, e.expn_rank_q);
            std::printf("\n");
        };

        int64_t seeds_measured = 0, seeds_with_consolidator_top = 0,
                seeds_with_expansionist_top = 0, seeds_with_both = 0, seeds_both_distinct = 0;
        int64_t reg_cons = 0, reg_expn = 0, reg_both = 0, reg_both_distinct = 0;
        for (const exploration_row& r : rows)
        {
            if (!r.ok || !r.strength_measured) continue;
            ++seeds_measured;
            if (r.top_has_consolidator) ++seeds_with_consolidator_top;
            if (r.top_has_expansionist) ++seeds_with_expansionist_top;
            if (r.top_has_consolidator && r.top_has_expansionist) ++seeds_with_both;
            if (r.top_both_distinct) ++seeds_both_distinct;
            if (r.regions_top_has_consolidator) ++reg_cons;
            if (r.regions_top_has_expansionist) ++reg_expn;
            if (r.regions_top_has_consolidator && r.regions_top_has_expansionist) ++reg_both;
            if (r.regions_top_both_distinct) ++reg_both_distinct;

            std::printf("  seed %u (C = consolidator by rank, E = expansionist, B = both, - = neither; "
                        "(rN) = per-mille rank):\n", r.seed);
            print_top("treasury:", r.top_by_treasury);
            print_top("regions:",  r.top_by_regions);
        }
        face.r3_measured = seeds_measured; face.r3_cons = seeds_with_consolidator_top;
        face.r3_expn = seeds_with_expansionist_top; face.r3_both = seeds_with_both;
        face.r3_reg_cons = reg_cons; face.r3_reg_expn = reg_expn; face.r3_reg_both = reg_both;
        face.r3_both_distinct = seeds_both_distinct; face.r3_reg_both_distinct = reg_both_distinct;
        std::printf("  seeds measured=%lld  top-3-by-TREASURY include a consolidator=%lld  include an "
                    "expansionist=%lld  BOTH present=%lld  BOTH in different realms=%lld\n",
                    static_cast<long long>(seeds_measured), static_cast<long long>(seeds_with_consolidator_top),
                    static_cast<long long>(seeds_with_expansionist_top), static_cast<long long>(seeds_with_both),
                    static_cast<long long>(seeds_both_distinct));
        std::printf("  (comparison) top-3-by-REGIONS include a consolidator=%lld  include an expansionist=%lld  "
                    "BOTH present=%lld  BOTH in different realms=%lld\n",
                    static_cast<long long>(reg_cons), static_cast<long long>(reg_expn),
                    static_cast<long long>(reg_both), static_cast<long long>(reg_both_distinct));
        // Two questions, answered separately (BL-1016): do the creed axes
        // separate the POPULATION at all, and do both strategies stand among
        // one world's strongest realms. Only the first failing is the case
        // EXPLORATION.md sends upstream.
        std::printf("  %s\n", class_c > 0 && class_e > 0
            ? "the creed axes separate the population: some realms read consolidator and not "
              "expansionist, others the reverse."
            : "the creed axes do NOT separate the population -- report to Ben (EXPLORATION.md sec Two "
              "ways to be strong: if they do not separate them, the fix is upstream, not a flag here).");
        std::printf("  %s\n", seeds_both_distinct > 0
            ? "at least one seed's strongest realms include a consolidator and a different "
              "expansionist realm, each traceable to its own creed."
            : "no seed on this spread shows a consolidator and a different expansionist among its "
              "strongest realms -- report to Ben rather than forcing the mapping.");
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
    // THE WEAKNESS COUNTERS (BL-1028), PER HALF. Half A = 1200 -> 1660 (or to
    // --through if that is earlier), half B = 1660 -> --through, printed only
    // when --through runs past 1660. HOW EACH READING'S HALVES ARE DERIVED:
    //
    //   first contacts (crossing / inherited) -- CUMULATIVE,
    //     `contacts_raised_trace`. A: the traced run stopped at 1660. B: the
    //     traced run to --through less the run to 1660.
    //   met pairs, bound, any treaty -- AT STOP. Canonical contact rows first
    //     met at or after 1200 (the engine's start_year on this path), and of
    //     them those holding non-aggression (`met_pairs_bound`'s own test) and
    //     those holding any clause. A: at the 1660 stop. B: at the --through
    //     stop -- so B counts every pair met since 1200 that stands then, not
    //     only pairs met after 1660.
    //   near-home alarm reads at the ceiling -- CUMULATIVE, over
    //     `near_capability_trace` (near home = contact before 1200): an entry
    //     is at the ceiling when raw * 1000 / visible_capability_reference
    //     >= 1000. A: the 1660 run's entries. B: the --through run's count less
    //     the 1660 run's (the entries past the 1660 run's length, which the
    //     prefix check proves are the same reads). Beside it, AT STOP: near
    //     pairs, those alarmed, those at the ceiling, and `near_alarm_max_q`.
    //   displacement -- CUMULATIVE, over `battle_trace`: neighbour = the pair
    //     was in contact at 1200, frontier otherwise (readings 1-2's test,
    //     unchanged in both halves). A: the 1660 run's traces. B: the
    //     difference, equal to the --through run's traces dated >= 1660 (the
    //     prefix check verifies the dates). Ratio = frontier / neighbour;
    //     pooled = summed frontier / summed neighbour; median over seeds with
    //     any neighbour battle in that half. No SILENT verdict is taken here.
    //   campaign funnel (near / met in span / unmet: examined, in reach,
    //     cleared, chosen) -- CUMULATIVE, `campaign_class_trace`, by
    //     difference. In reach = examined less treaty-blocked, water-illegal
    //     and reach-denied. Grains as that trace's own comment: cleared is
    //     season-grain, chosen round-grain.
    //   unmet contest (rounds cleared, won, lost to another verb) --
    //     CUMULATIVE, `unmet_contest_trace`, by difference.
    //   trade volume crossing landmasses -- AT STOP, the final decision
    //     round's `trade_flows` (reading 11's test). A: at 1660. B: at --through.
    //
    // The prefix the subtraction relies on is a structural check above.
    // Report only; read per seed, never the median alone.
    // -----------------------------------------------------------------------
    {
        const bool have_b = through_year > 1660;
        face.w_have_b = have_b;
        std::printf("\n--- weakness counters (BL-1028) ---\n");
        if (have_b)
            std::printf("  half A = 1200 -> 1660 (a traced re-run stopped at 1660); half B = 1660 -> %lld "
                        "(cumulative: the run to %lld less the run to 1660; at-stop: read at %lld)\n",
                        static_cast<long long>(through_year), static_cast<long long>(through_year),
                        static_cast<long long>(through_year));
        else
            std::printf("  half A = 1200 -> %lld only (--through does not pass 1660, so there is no half B)\n",
                        static_cast<long long>(through_year));
        std::printf("  %-4s %-4s | %6s %6s | %5s %5s %5s | %8s %7s | %5s %5s %5s %5s | %5s %5s %6s | "
                    "%-23s | %-23s | %-23s | %6s %5s %6s | %9s %9s %6s\n",
                    "seed", "half", "1st.x", "1st.in", "met", "bound", "treat",
                    "al.reads", "al@1000", "near", "alrmd", "@1000", "max", "nb", "fr", "ratio",
                    "near ex/rch/clr/ch", "met ex/rch/clr/ch", "unmet ex/rch/clr/ch",
                    "u.clr", "u.won", "u.verb", "volume", "x-mass", "x.shr");

        const auto pct_of = [](int64_t num, int64_t den) {
            return den > 0 ? 100.0 * static_cast<double>(num) / static_cast<double>(den) : 0.0;
        };
        const auto funnel_str = [](const int64_t* c) {
            char buf[64];
            std::snprintf(buf, sizeof buf, "%lld/%lld/%lld/%lld", static_cast<long long>(c[0]),
                          static_cast<long long>(funnel_in_reach(c)), static_cast<long long>(c[4]),
                          static_cast<long long>(c[5]));
            return std::string(buf);
        };
        const auto print_half = [&](const char* seed_label, const char* half_label, const weakness_half& h) {
            char ratio[16];
            if (h.neighbour_battles > 0)
                std::snprintf(ratio, sizeof ratio, "%.2f",
                              static_cast<double>(h.frontier_battles) / static_cast<double>(h.neighbour_battles));
            else
                std::snprintf(ratio, sizeof ratio, "no-nb");
            std::printf("  %-4s %-4s | %6lld %6lld | %5lld %5lld %5lld | %8lld %6.1f%% | %5lld %5lld %5lld %5d | "
                        "%5lld %5lld %6s | %-23s | %-23s | %-23s | %6lld %5lld %5.1f%% | %9lld %9lld %5.1f%%\n",
                        seed_label, half_label,
                        static_cast<long long>(h.contacts_raised[0]), static_cast<long long>(h.contacts_raised[1]),
                        static_cast<long long>(h.met_pairs), static_cast<long long>(h.met_pairs_bound),
                        static_cast<long long>(h.met_pairs_any_treaty),
                        static_cast<long long>(h.alarm_reads), pct_of(h.alarm_reads_ceiling, h.alarm_reads),
                        static_cast<long long>(h.near_pairs), static_cast<long long>(h.near_pairs_alarmed),
                        static_cast<long long>(h.near_pairs_ceiling), h.near_alarm_max_q,
                        static_cast<long long>(h.neighbour_battles), static_cast<long long>(h.frontier_battles), ratio,
                        funnel_str(h.funnel[0]).c_str(), funnel_str(h.funnel[1]).c_str(), funnel_str(h.funnel[2]).c_str(),
                        static_cast<long long>(h.unmet_contest[0]), static_cast<long long>(h.unmet_contest[1]),
                        pct_of(h.unmet_contest[4], h.unmet_contest[0]),
                        static_cast<long long>(h.flow_volume), static_cast<long long>(h.cross_landmass_volume),
                        pct_of(h.cross_landmass_volume, h.flow_volume));
        };

        std::vector<double> ratios[2];
        for (const exploration_row& r : rows)
        {
            if (!r.ok) continue;
            char seed_label[16];
            std::snprintf(seed_label, sizeof seed_label, "%u", r.seed);
            print_half(seed_label, "A", r.weak_a);
            weakness_accumulate(face.w_pooled[0], r.weak_a);
            ++face.w_seeds[0];
            if (r.weak_a.neighbour_battles > 0)
                ratios[0].push_back(static_cast<double>(r.weak_a.frontier_battles)
                                    / static_cast<double>(r.weak_a.neighbour_battles));
            if (have_b && r.weak_have_b)
            {
                print_half(seed_label, r.weak_prefix_ok ? "B" : "B!", r.weak_b);
                weakness_accumulate(face.w_pooled[1], r.weak_b);
                ++face.w_seeds[1];
                if (r.weak_b.neighbour_battles > 0)
                    ratios[1].push_back(static_cast<double>(r.weak_b.frontier_battles)
                                        / static_cast<double>(r.weak_b.neighbour_battles));
            }
        }
        for (int hh = 0; hh < (have_b ? 2 : 1); ++hh)
        {
            std::sort(ratios[hh].begin(), ratios[hh].end());
            face.w_seeds_with_ratio[hh] = static_cast<int>(ratios[hh].size());
            face.w_have_median[hh] = !ratios[hh].empty();
            face.w_median_ratio[hh] = ratios[hh].empty() ? 0.0 : ratios[hh][ratios[hh].size() / 2];
        }
        for (int hh = 0; hh < (have_b ? 2 : 1); ++hh)
            print_half("POOL", hh == 0 ? "A" : "B", face.w_pooled[hh]);

        std::printf("  columns: 1st.x/1st.in new contact pairs raised in the half by a crossing / by inheritance; "
                    "met/bound/treat pairs first met since 1200 standing at the half's stop, those holding "
                    "non-aggression (met_pairs_bound), those holding any treaty clause; al.reads/al@1000 near-home "
                    "treaty reads in the half and the share at the alarm ceiling; near/alrmd/@1000/max near pairs at "
                    "the stop, alarmed, at the ceiling, and near_alarm_max_q; nb/fr/ratio neighbour and frontier "
                    "battles in the half and frontier/neighbour; ex/rch/clr/ch the campaign funnel by contact class "
                    "(examined, in reach, cleared [season-grain], chosen); u.clr/u.won/u.verb rounds an unmet "
                    "candidate cleared, won by it, and the share another verb won; volume/x-mass/x.shr trade volume "
                    "at the stop, crossing landmasses, and its share. A half marked B! failed the prefix check.\n");
        for (int hh = 0; hh < (have_b ? 2 : 1); ++hh)
        {
            const weakness_half& p = face.w_pooled[hh];
            std::printf("  POOLED half %c (%lld -> %lld, %d seeds): displacement pooled %s, median %s over %d seeds "
                        "with a neighbour battle; alarm reads at the ceiling %.1f%% (max near alarm at the stop %d); "
                        "met pairs bound %lld of %lld (any treaty %lld); first contacts %lld crossing + %lld inherited; "
                        "unmet rounds cleared %lld, won %lld, lost to another verb %.1f%%; cross-landmass trade "
                        "%.1f%% of volume\n",
                        hh == 0 ? 'A' : 'B', static_cast<long long>(p.from_year), static_cast<long long>(p.to_year),
                        face.w_seeds[hh],
                        p.neighbour_battles > 0
                            ? std::to_string(static_cast<double>(p.frontier_battles)
                                             / static_cast<double>(p.neighbour_battles)).substr(0, 5).c_str()
                            : "undefined",
                        face.w_have_median[hh] ? std::to_string(face.w_median_ratio[hh]).substr(0, 5).c_str() : "undefined",
                        face.w_seeds_with_ratio[hh],
                        pct_of(p.alarm_reads_ceiling, p.alarm_reads), p.near_alarm_max_q,
                        static_cast<long long>(p.met_pairs_bound), static_cast<long long>(p.met_pairs),
                        static_cast<long long>(p.met_pairs_any_treaty),
                        static_cast<long long>(p.contacts_raised[0]), static_cast<long long>(p.contacts_raised[1]),
                        static_cast<long long>(p.unmet_contest[0]), static_cast<long long>(p.unmet_contest[1]),
                        pct_of(p.unmet_contest[4], p.unmet_contest[0]),
                        pct_of(p.cross_landmass_volume, p.flow_volume));
        }
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
        const char* json_path = !out_path.empty() ? out_path.c_str()
                              : want_override ? "exploration_sweep.tuning.json"
                              : through_year != 1660 ? "exploration_sweep.through.json"
                              : "exploration_sweep.json";
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
                                    "\"regions\": %lld, \"cons_q\": %d, \"expn_q\": %d, "
                                    "\"cons_rank_q\": %d, \"expn_rank_q\": %d}",
                                 k ? ", " : "", e.id,
                                 lean_tag_of(e.cons_rank_q, e.expn_rank_q),
                                 static_cast<long long>(e.treasury), e.mean_supply_q,
                                 static_cast<long long>(e.regions), e.cons_q, e.expn_q,
                                 e.cons_rank_q, e.expn_rank_q);
                }
                std::fprintf(f, "]");
            };
            // BL-1028: one half of the weakness counters as a JSON object.
            // `median` is written only for the pooled face (have_median set).
            const auto put_weak = [&](const weakness_half& h, bool pooled, bool have_median, double median,
                                      int seeds_with_ratio, int seeds, const char* tail) {
                const auto put_funnel = [&](const char* key, const int64_t* c, const char* ftail) {
                    std::fprintf(f, "\"%s\": {\"examined\": %lld, \"treaty_blocked\": %lld, \"water_illegal\": %lld, "
                                    "\"reach_denied\": %lld, \"in_reach\": %lld, \"cleared\": %lld, \"chosen\": %lld}%s",
                                 key, static_cast<long long>(c[0]), static_cast<long long>(c[1]),
                                 static_cast<long long>(c[2]), static_cast<long long>(c[3]),
                                 static_cast<long long>(funnel_in_reach(c)), static_cast<long long>(c[4]),
                                 static_cast<long long>(c[5]), ftail);
                };
                std::fprintf(f, "{\"from_year\": %lld, \"to_year\": %lld, ",
                             static_cast<long long>(h.from_year), static_cast<long long>(h.to_year));
                if (pooled) std::fprintf(f, "\"seeds\": %d, ", seeds);
                std::fprintf(f, "\"first_contacts_crossing\": %lld, \"first_contacts_inherited\": %lld, "
                                "\"met_pairs_at_stop\": %lld, \"met_pairs_bound_at_stop\": %lld, "
                                "\"met_pairs_any_treaty_at_stop\": %lld, "
                                "\"near_alarm_reads\": %lld, \"near_alarm_reads_at_ceiling\": %lld, ",
                             static_cast<long long>(h.contacts_raised[0]), static_cast<long long>(h.contacts_raised[1]),
                             static_cast<long long>(h.met_pairs), static_cast<long long>(h.met_pairs_bound),
                             static_cast<long long>(h.met_pairs_any_treaty),
                             static_cast<long long>(h.alarm_reads), static_cast<long long>(h.alarm_reads_ceiling));
                put_d("near_alarm_ceiling_share", h.alarm_reads > 0,
                      h.alarm_reads > 0 ? static_cast<double>(h.alarm_reads_ceiling) / static_cast<double>(h.alarm_reads) : 0.0,
                      ", ");
                std::fprintf(f, "\"near_pairs_at_stop\": %lld, \"near_pairs_alarmed_at_stop\": %lld, "
                                "\"near_pairs_at_ceiling_at_stop\": %lld, \"near_alarm_max_q_at_stop\": %d, "
                                "\"neighbour_battles\": %lld, \"frontier_battles\": %lld, \"ambiguous_battles\": %lld, ",
                             static_cast<long long>(h.near_pairs), static_cast<long long>(h.near_pairs_alarmed),
                             static_cast<long long>(h.near_pairs_ceiling), h.near_alarm_max_q,
                             static_cast<long long>(h.neighbour_battles), static_cast<long long>(h.frontier_battles),
                             static_cast<long long>(h.ambiguous_battles));
                put_d(pooled ? "displacement_pooled" : "displacement_ratio", h.neighbour_battles > 0,
                      h.neighbour_battles > 0
                          ? static_cast<double>(h.frontier_battles) / static_cast<double>(h.neighbour_battles) : 0.0,
                      ", ");
                if (pooled)
                {
                    put_d("displacement_median", have_median, median, ", ");
                    std::fprintf(f, "\"seeds_with_ratio\": %d, ", seeds_with_ratio);
                }
                std::fprintf(f, "\"funnel\": {");
                put_funnel("near", h.funnel[0], ", ");
                put_funnel("met", h.funnel[1], ", ");
                put_funnel("unmet", h.funnel[2], "}, ");
                std::fprintf(f, "\"unmet_contest\": {\"cleared_rounds\": %lld, \"won\": %lld, \"lost_to_near\": %lld, "
                                "\"lost_to_met\": %lld, \"lost_to_verb\": %lld, \"lost_margin_sum\": %lld, "
                                "\"lost_to_verb_by_verb\": {\"settle\": %lld, \"invest\": %lld, \"consolidate\": %lld, "
                                "\"build\": %lld, \"upgrade_supply\": %lld, \"organise\": %lld}}, ",
                             static_cast<long long>(h.unmet_contest[0]), static_cast<long long>(h.unmet_contest[1]),
                             static_cast<long long>(h.unmet_contest[2]), static_cast<long long>(h.unmet_contest[3]),
                             static_cast<long long>(h.unmet_contest[4]), static_cast<long long>(h.unmet_margin_sum),
                             static_cast<long long>(h.unmet_lost_verb[static_cast<int>(sim_verb::settle)]),
                             static_cast<long long>(h.unmet_lost_verb[static_cast<int>(sim_verb::invest)]),
                             static_cast<long long>(h.unmet_lost_verb[static_cast<int>(sim_verb::consolidate)]),
                             static_cast<long long>(h.unmet_lost_verb[static_cast<int>(sim_verb::build_work)]),
                             static_cast<long long>(h.unmet_lost_verb[static_cast<int>(sim_verb::upgrade_supply)]),
                             static_cast<long long>(h.unmet_lost_verb[static_cast<int>(sim_verb::organise)]));
                std::fprintf(f, "\"flow_volume_at_stop\": %lld, \"cross_landmass_volume_at_stop\": %lld, "
                                "\"unknown_landmass_volume_at_stop\": %lld}%s",
                             static_cast<long long>(h.flow_volume), static_cast<long long>(h.cross_landmass_volume),
                             static_cast<long long>(h.unknown_landmass_volume), tail);
            };

            std::fprintf(f, "{\n \"_note\": \"BL-937/BL-971 exploration sweep, 1200 -> 1660 CE. Reported, not gated "
                            "- see the harness header. One row per seed over the eleven readings of "
                            "EXPLORATION.md sec What the phase is judged on; 'spread' carries the face the "
                            "console prints. Displacement is read pooled (the verdict), volume-weighted and "
                            "as a median; a seed under silent_floor_battles traced battles is SILENT and "
                            "carries no verdict.\",\n");
            std::fprintf(f, " \"seed_count\": %d,\n \"through_year\": %lld,\n \"deterrence_alarm_weight_q\": %d,\n \"overrides\": [",
                         seed_count, static_cast<long long>(through_year), history_sim_params{}.deterrence_alarm_weight_q);
            {
                bool first = true;
                if (want_override && want_override_q >= 0)
                { std::fprintf(f, "\"w_want_q=%d\"", want_override_q); first = false; }
                for (const auto& kv : param_sets)
                { std::fprintf(f, "%s\"%s=%lld\"", first ? "" : ", ", kv.first.c_str(), kv.second); first = false; }
                if (industry_open > 0) // BL-1038
                { std::fprintf(f, "%s\"industry_open=%lld\"", first ? "" : ", ", static_cast<long long>(industry_open)); first = false; }
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
            // BL-1018: the alarm spread over every near-home treaty read (whole traced run).
            std::fprintf(f, "  \"alarm_spread\": {\"reference\": %lld, \"reads\": %lld, "
                            "\"capability_p10\": %lld, \"capability_p25\": %lld, \"capability_p50\": %lld, "
                            "\"capability_p75\": %lld, \"capability_p90\": %lld, \"capability_p95\": %lld, "
                            "\"capability_p99\": %lld, \"capability_max\": %lld, \"seed_p90_median\": %lld, "
                            "\"alarm_p10\": %d, \"alarm_p25\": %d, \"alarm_p50\": %d, \"alarm_p75\": %d, \"alarm_p90\": %d, "
                            "\"saturated_share\": %.4f, \"zero_share\": %.4f, \"histogram\": [",
                         static_cast<long long>(face.a_reference), static_cast<long long>(face.a_reads),
                         static_cast<long long>(face.a_cap[0]), static_cast<long long>(face.a_cap[1]),
                         static_cast<long long>(face.a_cap[2]), static_cast<long long>(face.a_cap[3]),
                         static_cast<long long>(face.a_cap[4]), static_cast<long long>(face.a_cap[5]),
                         static_cast<long long>(face.a_cap[6]), static_cast<long long>(face.a_cap[7]),
                         static_cast<long long>(face.a_seed_p90_median),
                         face.a_alarm[0], face.a_alarm[1], face.a_alarm[2], face.a_alarm[3], face.a_alarm[4],
                         face.a_saturated_share, face.a_zero_share);
            for (int b = 0; b < 12; ++b)
                std::fprintf(f, "%s%lld", b ? ", " : "", static_cast<long long>(face.a_hist[b]));
            std::fprintf(f, "], \"close_near_pairs\": %lld, \"close_near_pairs_saturated\": %lld},\n",
                         static_cast<long long>(face.a_close_pairs), static_cast<long long>(face.a_close_saturated));
            std::fprintf(f, "  \"strategies\": {\"classifier\": \"per-mille lean rank among living cultured polities, top third\", "
                            "\"top_third_rank_q\": %d, \"seeds_measured\": %lld, \"treasury_top_has_consolidator\": %lld, "
                            "\"treasury_top_has_expansionist\": %lld, \"treasury_top_has_both\": %lld, "
                            "\"treasury_top_has_both_distinct\": %lld, "
                            "\"regions_top_has_consolidator\": %lld, \"regions_top_has_expansionist\": %lld, "
                            "\"regions_top_has_both\": %lld, \"regions_top_has_both_distinct\": %lld, ",
                         kLeanTopThirdRankQ,
                         static_cast<long long>(face.r3_measured), static_cast<long long>(face.r3_cons),
                         static_cast<long long>(face.r3_expn), static_cast<long long>(face.r3_both),
                         static_cast<long long>(face.r3_both_distinct),
                         static_cast<long long>(face.r3_reg_cons), static_cast<long long>(face.r3_reg_expn),
                         static_cast<long long>(face.r3_reg_both), static_cast<long long>(face.r3_reg_both_distinct));
            {
                const auto put_pct = [&](const char* key, const int v[7], const char* tail) {
                    std::fprintf(f, "\"%s\": [%d, %d, %d, %d, %d, %d, %d]%s",
                                 key, v[0], v[1], v[2], v[3], v[4], v[5], v[6], tail);
                };
                std::fprintf(f, "\"lean_population\": {\"_pct\": \"min, p10, p25, median, p75, p90, max\", "
                                "\"polities\": %lld, \"uncultured\": %lld, \"raw_cons_gt_expn\": %lld, "
                                "\"consolidator_only\": %lld, \"expansionist_only\": %lld, \"both\": %lld, "
                                "\"neither\": %lld, ",
                             static_cast<long long>(face.r3_pop), static_cast<long long>(face.r3_uncultured),
                             static_cast<long long>(face.r3_raw_cons_gt_expn),
                             static_cast<long long>(face.r3_class_c), static_cast<long long>(face.r3_class_e),
                             static_cast<long long>(face.r3_class_b), static_cast<long long>(face.r3_class_none));
                put_pct("consolidator_lean_q", face.r3_cons_pct, ", ");
                put_pct("expansion_lean_q", face.r3_expn_pct, ", ");
                put_pct("sea_legs_q", face.r3_sea_pct, ", ");
                put_pct("dominion_x100", face.r3_dom_pct, ", ");
                put_pct("zeal_x100", face.r3_zeal_pct, "}},\n");
            }
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
            put_d("top_decile_share", face.r11_volume > 0 && face.r11_pairs > 0, face.r11_top_decile_share, "},\n");
            // BL-1028: the weakness counters pooled per half.
            std::fprintf(f, "  \"weakness\": {\"half_a\": ");
            put_weak(face.w_pooled[0], true, face.w_have_median[0], face.w_median_ratio[0],
                     face.w_seeds_with_ratio[0], face.w_seeds[0], face.w_have_b ? ",\n   \"half_b\": " : "}\n");
            if (face.w_have_b)
                put_weak(face.w_pooled[1], true, face.w_have_median[1], face.w_median_ratio[1],
                         face.w_seeds_with_ratio[1], face.w_seeds[1], "}\n");
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
                // BL-1019: this seed's first contact over the traced run, the
                // geography at 1200, and the unmet candidate's contest.
                std::fprintf(f, "   \"first_contacts\": {\"crossings\": %lld, \"inherited\": %lld, \"met_pairs_1660\": %lld, "
                                "\"polities_1200\": %lld, \"pairs_1200\": %lld, \"unmet_pairs_1200\": %lld, "
                                "\"unmet_touching_1200\": %lld, \"unmet_touching_cross_mass_1200\": %lld, "
                                "\"landmasses_with_polity_1200\": %lld, \"unmet_cleared_rounds\": %lld, "
                                "\"unmet_won\": %lld, \"unmet_lost_to_near\": %lld, \"unmet_lost_to_met\": %lld, "
                                "\"unmet_lost_to_verb\": %lld, \"unmet_lost_margin_sum\": %lld},\n",
                             static_cast<long long>(r.contacts_raised[0]), static_cast<long long>(r.contacts_raised[1]),
                             static_cast<long long>(r.far_pairs), static_cast<long long>(r.polities_1200),
                             static_cast<long long>(r.pairs_1200), static_cast<long long>(r.unmet_pairs_1200),
                             static_cast<long long>(r.unmet_touching_1200),
                             static_cast<long long>(r.unmet_touching_cross_mass_1200),
                             static_cast<long long>(r.landmasses_with_polity_1200),
                             static_cast<long long>(r.unmet_contest[0]), static_cast<long long>(r.unmet_contest[1]),
                             static_cast<long long>(r.unmet_contest[2]), static_cast<long long>(r.unmet_contest[3]),
                             static_cast<long long>(r.unmet_contest[4]), static_cast<long long>(r.unmet_margin_sum));
                // BL-1018: this seed's own alarm read (whole traced run).
                {
                    const std::vector<int64_t>& v = r.near_capability;
                    const auto pct = [&](int p) -> long long {
                        return v.empty() ? 0 : static_cast<long long>(v[(v.size() - 1) * static_cast<std::size_t>(p) / 100]);
                    };
                    int64_t sat = 0;
                    for (int64_t c : v)
                        if (std::max<int64_t>(0, c) * 1000 / std::max<int64_t>(1, r.capability_reference) >= 1000) ++sat;
                    std::fprintf(f, "   \"alarm_reads\": %zu, \"capability_p50\": %lld, \"capability_p90\": %lld, "
                                    "\"alarm_saturated_share\": %.4f, \"near_pairs_saturated_1660\": %lld,\n",
                                 v.size(), pct(50), pct(90),
                                 v.empty() ? 0.0 : static_cast<double>(sat) / static_cast<double>(v.size()),
                                 static_cast<long long>(r.near_pairs_saturated));
                }
                // reading 3
                std::fprintf(f, "   \"strength_measured\": %s, \"treasury_top_has_consolidator\": %s, "
                                "\"treasury_top_has_expansionist\": %s, \"treasury_top_both_distinct\": %s, "
                                "\"regions_top_has_consolidator\": %s, "
                                "\"regions_top_has_expansionist\": %s, \"regions_top_both_distinct\": %s, "
                                "\"lean_polities\": %zu, \"lean_uncultured\": %lld, \"top_by_treasury\": ",
                             r.strength_measured ? "true" : "false",
                             r.top_has_consolidator ? "true" : "false", r.top_has_expansionist ? "true" : "false",
                             r.top_both_distinct ? "true" : "false",
                             r.regions_top_has_consolidator ? "true" : "false",
                             r.regions_top_has_expansionist ? "true" : "false",
                             r.regions_top_both_distinct ? "true" : "false",
                             r.lean_population.size(), static_cast<long long>(r.living_uncultured));
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
                                    "\"trade_access_pairs\": %lld, \"pairs_with_flow\": %lld",
                                 static_cast<long long>(r.flow_count), static_cast<long long>(r.flow_volume),
                                 static_cast<long long>(r.cross_landmass_volume),
                                 static_cast<long long>(r.unknown_landmass_volume),
                                 static_cast<long long>(r.flows_without_clause),
                                 static_cast<long long>(r.trade_bound_pairs), static_cast<long long>(with_flow));
                }
                // BL-1027: the span's cost, cumulative from 1200 at each stop.
                if (r.cost_ran)
                {
                    const auto put_cost = [&](const char* key, const exploration_row::span_cost& c, const char* tail) {
                        std::fprintf(f, "\"%s\": {\"wall_ms\": %lld, \"ns_demography\": %lld, \"ns_decisions\": %lld, "
                                        "\"ns_battles\": %lld, \"ns_reach\": %lld, \"decision_rounds\": %lld, "
                                        "\"reach_rebuilds\": %lld, \"battles\": %lld, \"conquests\": %lld, "
                                        "\"foundings\": %lld, \"regions\": %lld, \"alive\": %lld, "
                                        "\"rim_holders\": %lld, \"mean_exploration_nodes\": %.3f}%s",
                                     key, (long long)c.wall_ms, (long long)c.ns_demography, (long long)c.ns_decisions,
                                     (long long)c.ns_battles, (long long)c.ns_reach, (long long)c.decision_rounds,
                                     (long long)c.reach_rebuilds, (long long)c.battles, (long long)c.conquests,
                                     (long long)c.foundings, (long long)c.regions, (long long)c.alive,
                                     (long long)c.rim_holders, c.mean_exploration_nodes, tail);
                    };
                    std::fprintf(f, ",\n   \"cost\": {\"reproduces_generation\": %s, \"regions_1200\": %lld, \"alive_1200\": %lld,\n    ",
                                 r.cost_reproduces ? "true" : "false",
                                 (long long)r.regions_1200, (long long)r.alive_1200);
                    put_cost("to_1660", r.to_1660, ",\n    ");
                    put_cost("to_through", r.to_through, "}");
                }
                // BL-1028: the weakness counters, per half (method: the
                // console section's comment).
                std::fprintf(f, ",\n   \"weakness\": {\"half_a\": ");
                put_weak(r.weak_a, false, false, 0.0, 0, 0, r.weak_have_b ? ",\n    \"half_b\": " : "}");
                if (r.weak_have_b)
                {
                    put_weak(r.weak_b, false, false, 0.0, 0, 0, ", ");
                    std::fprintf(f, "\"prefix_check_ok\": %s}", r.weak_prefix_ok ? "true" : "false");
                }
                std::fprintf(f, "}%s\n", sep);
            }
            std::fprintf(f, " ]\n}\n");
            std::fclose(f);
            std::printf("\nWrote %s (%d rows)\n", json_path, static_cast<int>(rows.size()));
        }
    }

    std::printf("\n%d failure(s) in structural checks.\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
