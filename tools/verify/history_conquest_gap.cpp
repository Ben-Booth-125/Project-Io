// ---------------------------------------------------------------------------
// history_conquest_gap — WHY the Era −1 sim fights and never conquers (BL-384),
// measured on THE ERA GENERATION ACTUALLY RUNS (BL-462).
// ---------------------------------------------------------------------------
// READ THIS FIRST: FOR ITS FIRST THREE SPRINTS THIS HARNESS MEASURED A SIM THE
// GAME DOES NOT RUN. `history_sim_params`'s struct default is a 4000 BCE -> 0 CE
// span on a six-band clock (136 decision rounds). Generation overrides it to 400
// years on one 4-year band (100 rounds) — and to five more things besides — at a
// call site no other caller could see. Every number this file printed before
// 2026-08-23 described a different world, including the findings two sprints
// were steered by. BL-462 is that defect; this file is now its acceptance test.
//
// THE SIX DIVERGENCES, all six now closed (world/era_minus_one.hpp carries the
// long form):
//   1. SPAN   4000 years -> 400.
//   2. CLOCK  six bands (136 rounds) -> one 4-year band (100 rounds).
//   3. SEED   the bare world seed -> `params.seed ^ 0x415C1E17u`.
//   4. CREEDS a null pointer, which flattens every polity's aggression to a
//             neutral 500 -> the real per-culture aggressions. This one deleted
//             the single input that makes polities differ in readiness to fight,
//             inside a harness whose whole subject is why they do not.
//   5. WORKS  a null registry, which gates `build_work` OUT of the verb contest
//             -> the pointer generation was handed. See THE ONE REMAINING GAP.
//   6. STATE  the report's POST-sim settlement (the sim mutates in place) -> the
//             settlement as the sim received it. Re-running on the report's copy
//             starts the era from its own ending.
//
// HOW THEY ARE CLOSED, and why not by copying generation's settings in here:
// copying is the defect deferred — two places constructing one invocation is how
// all six arrived. `make_hard_coded_world` now fills an `era_minus_one_fixture`
// with exactly what it passed `run_history_sim`, and this harness hands those
// values straight back. It constructs NOTHING of its own, so there is nothing
// left to drift.
//
// A1 IS THE ROW THAT MAKES THE REST MEAN ANYTHING. For every swept seed the
// re-run's battles/conquests/foundings must equal `generation_report`'s
// prehistory_* fields from the same generation, bit for bit. Green: this is
// generation's era. Red: it is not, and no other number below is evidence of
// anything.
//
// ------------------------- THE ONE REMAINING GAP ---------------------------
// THE WORKS TABLE IS AUTHORED IN LUA (`scripts/works.lua`) AND A HEADLESS
// HARNESS CANNOT LOAD IT. So this harness passes null, generation-under-harness
// therefore also passes null, and both sides agree — A1 stays exact — but what
// is measured is the era with `build_work` GATED OUT: a FOUR-verb contest where
// the shipped game runs a FIVE-verb one (`history_sim.cpp`, `works != nullptr
// && works->size() > 0`). Verb competition is precisely what this harness is
// used to measure, so this is the largest thing it still cannot see, and it is
// printed as a banner on every run rather than left in a comment.
//
// It was NOT closed by transcribing works.lua into C++. That would put the
// roster in two places and make the harness stale the next time the table moves
// — BL-462's own failure mode, rebuilt one layer down. The real fixes are a
// Lua-free authoring path production also uses, or a `--verify` (Lua-capable)
// check; both are calls above this file's pay grade and are in the review queue.
// `world_gen.lua` is the same shape and the same size of gap: a headless harness
// measures the default `world_gen_config`.
//
// BL-384's ORIGINAL QUESTION, unchanged. The scorer estimates the odds as
// attacker levy x supply x cohesion against the defender's RAW manpower_stock.
// `resolve_battle` is then handed a defender carrying TWO terms the scorer never
// saw: `terrain_defence(...)`, and `work_defence_mod` as readiness. So the
// scorer should pick fights on odds the resolver does not honour, and the error
// should be one-directional. A second candidate: the scorer estimates attacker
// strength from the MAXIMUM-manpower holding while execution levies from the
// NEAREST. A third is why the trace is taken at the conquest bar rather than at
// the resolver — "battles, 0 conquests" is consistent with the attacker never
// WINNING and equally with it winning often and never clearing
// `transfer_decisiveness_q`, and any diagnosis that cannot tell those apart is a
// guess.
//
// Headless: world/* logic only, no SDL and no Lua.
// ---------------------------------------------------------------------------

#include "world/era_minus_one.hpp"
#include "world/hard_coded_world.hpp"
#include "world/history_sim.hpp"
#include "world/settlement.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace
{

int g_failures = 0;
int g_checks   = 0;

void check(bool ok, const char* what)
{
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what);
    ++g_checks;
    if (!ok) ++g_failures;
}

/// Median of a copy — the distributions here are skewed, so a mean would be
/// the wrong summary and would hide exactly the tail that matters.
int median_of(std::vector<int> v)
{
    if (v.empty()) return 0;
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

const char* verb_name(sim_verb v)
{
    switch (v)
    {
    case sim_verb::none:        return "none";
    case sim_verb::settle:      return "Settle";
    case sim_verb::campaign:    return "Campaign";
    case sim_verb::invest:      return "Invest";
    case sim_verb::consolidate: return "Consolidate";
    case sim_verb::build_work:  return "BuildWork";
    }
    return "?";
}

constexpr int verb_slots = 6; ///< sim_verb none..build_work.

/// ORDER-OF-MAGNITUDE BUCKETS, NOT A MEAN — the T2 requirement in one function.
/// A margin of 3 and a margin of 3000 are different bugs (a weighting nudge vs
/// BL-318 incommensurability, two scores authored on different scales), and a
/// mean of the two is 1501, which is neither. The bucket edges are decades
/// because that is the axis the two bugs separate on.
const char* margin_bucket_label(int i)
{
    static const char* labels[] = {"1-3", "4-10", "11-30", "31-100",
                                   "101-300", "301-1000", "1001+"};
    return labels[i];
}
constexpr int margin_buckets = 7;

int margin_bucket_of(int m)
{
    if (m <=    3) return 0;
    if (m <=   10) return 1;
    if (m <=   30) return 2;
    if (m <=  100) return 3;
    if (m <=  300) return 4;
    if (m <= 1000) return 5;
    return 6;
}

/// Quantile of an ALREADY-SORTED vector, nearest-rank.
int quantile_of(const std::vector<int>& sorted, double q)
{
    if (sorted.empty()) return 0;
    const std::size_t i = static_cast<std::size_t>(q * (sorted.size() - 1) + 0.5);
    return sorted[std::min(i, sorted.size() - 1)];
}

/// The three readings the fork admits. Total by construction — every seed lands
/// in exactly one, which is what R1 asserts.
enum class fork_reading
{
    unclassified = 0, ///< Only ever a bug in this harness; asserted against.
    never_cleared,    ///< Campaign never clears its own threshold.
    cleared_and_lost, ///< In the running every round, wins none of them.
    cleared_and_won,  ///< Wins at least one round.
};

const char* reading_name(fork_reading r)
{
    switch (r)
    {
    case fork_reading::unclassified:     return "UNCLASSIFIED (harness bug)";
    case fork_reading::never_cleared:    return "NEVER CLEARS the threshold";
    case fork_reading::cleared_and_lost: return "CLEARS and LOSES every round";
    case fork_reading::cleared_and_won:  return "clears and wins at least once";
    }
    return "?";
}

/// One seed's whole funnel, kept so the assertions run over the SEEDS rather
/// than over the battles. That distinction is the point: a silent world produces
/// no battles at all, so any check written as a loop over `battle_trace`s is
/// silently vacuous on exactly the worlds that need explaining.
struct seed_row
{
    int     seed = 0;
    int64_t battles = 0, conquests = 0, foundings = 0;
    /// What the SAME generation wrote into `generation_report`. A1 compares the
    /// three above against these; they are two independent readings of one run
    /// only if the fixture is faithful.
    int64_t rep_battles = 0, rep_conquests = 0, rep_foundings = 0;
    int64_t contacts = 0, scored = 0, chosen = 0;
    int64_t cleared = 0, cleared_rounds = 0, cleared_lost = 0;
    /// The span and clock this seed's era actually ran on, carried so the output
    /// NAMES the configuration it measured (BL-462's work item 3).
    int64_t start_year = 0, stop_year = 0;
    int     bands = 0, first_band_step = 0;
    fork_reading reading = fork_reading::unclassified;
    /// Losing margins (winner_score - campaign_score), split by winning verb.
    std::vector<int> margins[verb_slots];
    /// THE LEVELS THE MARGIN HAS TO BE READ AGAINST. A margin of 280 is a
    /// weighting nudge if Campaign was scoring 3000 and an order-of-magnitude
    /// mismatch if it was scoring 45 — the same number, two different bugs, and
    /// the margin alone cannot tell them apart. So the two scores are carried
    /// beside it, not just their difference.
    std::vector<int> camp_scores[verb_slots];
    std::vector<int> win_scores[verb_slots];
    /// The control: what a Campaign score looks like on a round Campaign WON.
    /// Empty on a silent world by definition, which is itself the finding.
    std::vector<int> won_camp_scores;
};

/// The pinned table (R3).
///
/// THE POINT OF PINNING IS NOT REGRESSION IN GENERAL — it is that a MEASUREMENT
/// which no longer reproduces the world under study is measuring a different
/// one. That is the failure these numbers exist to catch, and it is the failure
/// that actually happened here.
///
/// --------------------------- RE-PINNED 2026-08-23 (BL-462) -----------------
/// EVERY NUMBER IN THIS TABLE MOVED, AND THE MOVEMENT IS THE ITEM LANDING, NOT
/// A REGRESSION. The old values measured `history_sim_params`'s struct default —
/// a 4000-year six-band run with null creeds, the bare world seed and the
/// report's post-sim settlement. They are kept here so the change is legible and
/// so nobody re-blesses back to them by accident:
///
///     seed | battles conquests foundings |  contacts    scored  chosen
///        0 |       0         0       786 |  4972710   9945420       0
///        1 |     236       210       643 |  6189696  12379392     241
///        2 |     435       435       543 |  2620190   5240380     435
///        3 |     163       163       383 |  3062571   6125142     163
///        4 |       0         0       762 |  4089264   8178528       0
///        5 |     164       164       628 |  3796047   7592094     165
///        6 |      44        43       872 |  5441888  10883776      44
///        7 |     184       184       662 |  1578177   3156354     185
///
/// The two headline claims those numbers carried — "2 of 8 worlds fight nothing"
/// and, at n=32, "17 of 32" — were claims about that sim, not about the game's.
/// Read Q0 on a current run for the rate in the era that ships.
///
/// PINNED SEEDS ARE DELIBERATELY DECOUPLED FROM THE SWEEP WIDTH, and must stay
/// so. They were one number until 2026-08-23, when `pinned[r.seed]` was indexed
/// by a seed the sweep could be widened past — so raising `seeds` read out of
/// bounds, and the only safe sweep was the one the pinned table happened to
/// match. That coupling is the mechanical reason a headline claim rested on n=8:
/// the instrument could not be widened to test its own generalisation.
struct pinned_row { int64_t battles, conquests, foundings, contacts, scored, chosen; };
constexpr int pinned_seeds = 8;
constexpr pinned_row pinned[pinned_seeds] = {
    {    8,    8, 1180,   568923,   1137846,    8},
    {  153,   52,  934,   878164,   1756328,  178},
    {  241,  235,  875,   377756,    755512,  241},
    {   62,   54,  754,   668514,   1337028,   62},
    {    0,    0, 1188,   561564,   1123128,    0},
    {   70,   61, 1007,   677921,   1355842,   88},
    {  127,   29, 1011,   664241,   1328482,  127},
    {   60,   50, 1066,   134236,    268472,   60},
};

} // namespace

int main(int argc, char** argv)
{
    std::printf("=== history_conquest_gap (BL-384 / BL-462) — the era GENERATION runs ===\n\n");
    std::printf("  !! WORKS ARE OFF. scripts/works.lua is Lua-authored and a headless harness\n"
                "  !! cannot load it, so `build_work` is gated OUT of the verb contest here\n"
                "  !! (history_sim.cpp: works != nullptr && works->size() > 0). The shipped\n"
                "  !! game scores FIVE verbs; this run scores FOUR. Both sides of A1 pass the\n"
                "  !! same null registry, so A1 stays exact — but any statement below about\n"
                "  !! what beats Campaign is a statement about a four-verb contest.\n"
                "  !! world_gen_config is the default here for the same reason.\n\n");

    // SWEEP WIDTH, independent of `pinned_seeds`. Override from the command line
    // to falsify a claim drawn from the default sweep — which is exactly what
    // n=32 did to this file's headline on 2026-08-23.
    int seeds = pinned_seeds;
    if (argc > 1)
    {
        const int want = std::atoi(argv[1]);
        if (want > 0 && want <= 256) seeds = want;
    }
    std::vector<battle_trace> all;

    // Determinism guard on the instrument itself: a traced run and an untraced
    // run must agree in every other output. An instrument that perturbs what it
    // measures is worse than no instrument, and this is asserted rather than
    // merely asserted about.
    bool trace_is_inert = true;
    /// The trace counters must be GATED, not merely harmless. An untraced run
    /// has to leave every one of them at zero and both trace vectors empty —
    /// otherwise the sim pays for the measurement on every generated world, and
    /// the inertness check above would still be green because it only compares
    /// the OTHER outputs.
    bool counters_are_gated = true;
    /// THE ANTI-VACUITY GUARD. Checking "the untraced run left the counters at
    /// zero" on a seed whose TRACED run also leaves them at zero proves nothing.
    /// So the gating pair runs on the first seed whose traced run actually moves
    /// a counter, and that seed is asserted to exist.
    int  gating_seed = -1;
    int  peaceful_worlds = 0;
    int  worlds = 0;
    /// A1's accumulator. Counted rather than short-circuited so the run reports
    /// HOW MANY seeds reproduce, not just whether all of them do.
    int  a1_matched = 0;
    bool a1_ok = true;
    bool all_eras_ran = true;
    bool works_seen = false; ///< True if any fixture came back with a registry.
    std::vector<seed_row> rows;

    for (int i = 0; i < seeds; ++i)
    {
        world_params wp;
        wp.seed = static_cast<uint32_t>(i);
        generation_report     rep;
        era_minus_one_fixture fx;
        // THE FIXTURE IS THE WHOLE POINT: this call is the ONLY construction of
        // the era's arguments anywhere in this file.
        const world w = make_hard_coded_world(wp, &rep, world_gen_config{},
                                              /*progress=*/nullptr, /*works=*/nullptr, &fx);
        (void)w;

        if (!fx.ran)
        {
            all_eras_ran = false;
            std::printf("  seed %d: THE ERA DID NOT RUN — the fixture's gate is false. Every\n"
                        "          number for this seed would be a struct default.\n", i);
            continue;
        }
        if (fx.works != nullptr) works_seen = true;

        // ---- The re-run, from the fixture and nothing else -----------------
        // `run_history_sim` mutates its settlement in place, so every run below
        // starts from a fresh copy of the captured PRE-sim state.
        settlement_state   ss_on = fx.settlement;
        history_sim_params p_on  = fx.params;
        p_on.trace_battles = true;
        const history_sim_state on =
            run_history_sim(ss_on, &fx.creeds, fx.terrain.view(), fx.gw, fx.gh,
                            p_on, fx.seed, /*year_progress=*/nullptr, fx.works);

        // The inertness/gating pair runs on ONE seed rather than doubling every
        // run: the property is structural (no trace field feeds a decision or a
        // draw), so one world exercises it, and a second full sim per seed costs
        // minutes for a claim the first already settles.
        if (i == 0 || (gating_seed < 0 && on.campaign_cleared > 0))
        {
            settlement_state   ss_off = fx.settlement;
            history_sim_params p_off  = fx.params; // trace_battles stays false.
            const history_sim_state off =
                run_history_sim(ss_off, &fx.creeds, fx.terrain.view(), fx.gw, fx.gh,
                                p_off, fx.seed, /*year_progress=*/nullptr, fx.works);

            if (off.battles != on.battles || off.conquests != on.conquests
                || off.foundings != on.foundings || off.winter_campaigns != on.winter_campaigns
                || off.owner_changes.size() != on.owner_changes.size())
                trace_is_inert = false;

            if (off.campaign_cleared != 0 || off.campaign_cleared_rounds != 0
                || off.campaign_cleared_lost != 0 || !off.verb_contests.empty()
                || off.campaign_contacts != 0 || off.campaign_scored != 0
                || off.campaign_chosen != 0 || !off.battle_traces.empty())
                counters_are_gated = false;

            if (gating_seed < 0 && on.campaign_cleared > 0) gating_seed = i;
        }

        if (on.battles == 0) ++peaceful_worlds;
        ++worlds;

        seed_row row;
        row.seed          = i;
        row.battles       = on.battles;
        row.conquests     = on.conquests;
        row.foundings     = on.foundings;
        row.rep_battles   = rep.prehistory_battles;
        row.rep_conquests = rep.prehistory_conquests;
        row.rep_foundings = rep.prehistory_foundings;
        row.contacts       = on.campaign_contacts;
        row.scored         = on.campaign_scored;
        row.chosen         = on.campaign_chosen;
        row.cleared        = on.campaign_cleared;
        row.cleared_rounds = on.campaign_cleared_rounds;
        row.cleared_lost   = on.campaign_cleared_lost;
        row.start_year      = fx.params.start_year;
        row.stop_year       = fx.params.stop_year;
        row.bands           = fx.params.tick_band_count;
        row.first_band_step = fx.params.tick_bands[0].step_years;

        const bool matched = on.battles   == rep.prehistory_battles
                          && on.conquests == rep.prehistory_conquests
                          && on.foundings == rep.prehistory_foundings;
        if (matched) ++a1_matched; else a1_ok = false;

        if (on.campaign_cleared == 0)      row.reading = fork_reading::never_cleared;
        else if (on.campaign_chosen == 0)  row.reading = fork_reading::cleared_and_lost;
        else                               row.reading = fork_reading::cleared_and_won;

        for (const verb_contest_trace& vc : on.verb_contests)
        {
            if (vc.winner == sim_verb::campaign)
            {
                row.won_camp_scores.push_back(vc.campaign_score);
                continue;
            }
            const int slot = static_cast<int>(vc.winner);
            if (slot < 0 || slot >= verb_slots) continue;
            row.margins[slot].push_back(vc.winner_score - vc.campaign_score);
            row.camp_scores[slot].push_back(vc.campaign_score);
            row.win_scores[slot].push_back(vc.winner_score);
        }
        rows.push_back(std::move(row));

        // The funnel plus A1's pair, printed for EVERY seed including the silent
        // ones — a world with no battles has no traces, so the per-battle record
        // is mute about exactly the case that needs explaining.
        std::printf("  seed %d: %4lld battles  %4lld conquests  %4lld foundings"
                    "  | contacts %7lld  scored %8lld  chosen %5lld  | report %lld/%lld/%lld %s\n",
                    i, static_cast<long long>(on.battles),
                    static_cast<long long>(on.conquests),
                    static_cast<long long>(on.foundings),
                    static_cast<long long>(on.campaign_contacts),
                    static_cast<long long>(on.campaign_scored),
                    static_cast<long long>(on.campaign_chosen),
                    static_cast<long long>(rep.prehistory_battles),
                    static_cast<long long>(rep.prehistory_conquests),
                    static_cast<long long>(rep.prehistory_foundings),
                    matched ? "==" : "<-- MISMATCH");
        std::fflush(stdout);

        all.insert(all.end(), on.battle_traces.begin(), on.battle_traces.end());
    }

    // ======================================================================
    // A1 — THE ACCEPTANCE TEST (BL-462). Everything else is downstream of it.
    // ======================================================================
    std::printf("\n  === A1. IS THIS GENERATION'S ERA? ===\n");
    if (!rows.empty())
        std::printf("    configuration measured: %lld -> %lld (%lld years), %d band(s),"
                    " first step %d years, creeds ON, works %s\n",
                    static_cast<long long>(rows.front().start_year),
                    static_cast<long long>(rows.front().stop_year),
                    static_cast<long long>(rows.front().stop_year - rows.front().start_year),
                    rows.front().bands, rows.front().first_band_step,
                    works_seen ? "ON" : "OFF (see the banner)");
    std::printf("    seeds whose re-run reproduces generation_report exactly: %d / %d\n",
                a1_matched, static_cast<int>(rows.size()));
    check(a1_ok && all_eras_ran && !rows.empty() && a1_matched == static_cast<int>(rows.size()),
          "A1 the re-run IS generation's own Era -1: battles/conquests/foundings equal "
          "generation_report::prehistory_* on EVERY swept seed, and the era ran on every one");

    // ======================================================================
    // T1/T2 — VERB COMPETITION.
    // ======================================================================
    //
    // WHY THIS IS THE FORK. BL-384 in its entirety is about battles that
    // happened; a world that fights NO WAR IN AN ENTIRE ERA makes every
    // per-battle number an empty set. `campaign_chosen == 0` with
    // `campaign_scored` large narrows it to "the scorer looks at a war it could
    // start and picks something else", and then stops: a candidate DISCARDED
    // BELOW ITS THRESHOLD and one that CLEARED AND LOST THE ARGMAX are the same
    // zero. They are different defects and the fix for one is inert against the
    // other. T1 separates them; T2 sizes the second.
    std::printf("\n  === T1. THE FORK: does Campaign CLEAR its threshold and lose, or never clear? ===\n");
    std::printf("    cleared        = CANDIDATES scoring >= campaign_threshold_q (a polity\n"
                "                     examines many targets x 2 seasons in one round).\n");
    std::printf("    rounds/won/lost= ROUND grain. Every round Campaign was in the running\n"
                "                     either won the argmax or lost it; there is no third case.\n\n");
    std::printf("    seed | battles | scored cand | cleared cand | rounds |  won |  lost | reading\n");
    std::printf("    -----+---------+-------------+--------------+--------+------+-------+--------\n");
    for (const seed_row& r : rows)
        std::printf("    %4d | %7lld | %11lld | %12lld | %6lld | %4lld | %5lld | %s\n",
                    r.seed, static_cast<long long>(r.battles),
                    static_cast<long long>(r.scored),
                    static_cast<long long>(r.cleared),
                    static_cast<long long>(r.cleared_rounds),
                    static_cast<long long>(r.chosen),
                    static_cast<long long>(r.cleared_lost),
                    reading_name(r.reading));

    // The silent worlds, called out on their own, because they are the subject.
    std::printf("\n    The worlds that fight nothing:\n");
    int silent_seen = 0, silent_never_cleared = 0, silent_cleared_lost = 0;
    for (const seed_row& r : rows)
    {
        if (r.battles != 0) continue;
        ++silent_seen;
        if (r.reading == fork_reading::never_cleared)    ++silent_never_cleared;
        if (r.reading == fork_reading::cleared_and_lost) ++silent_cleared_lost;
        std::printf("      seed %d: %lld candidates scored, %lld cleared the threshold,"
                    " %lld rounds lost to another verb -> %s\n",
                    r.seed, static_cast<long long>(r.scored),
                    static_cast<long long>(r.cleared),
                    static_cast<long long>(r.cleared_lost),
                    reading_name(r.reading));
    }
    if (silent_seen == 0)
        std::printf("      (none in this sweep — in the era generation runs, every world here\n"
                    "       fights. The silent-world readings below do not apply.)\n");
    else if (silent_never_cleared == silent_seen)
        std::printf("      READING: the campaign score NEVER clears its own threshold on a silent\n"
                    "               world. Verb competition is not the subject — nothing was ever\n"
                    "               in the running for another verb to beat.\n");
    else if (silent_cleared_lost == silent_seen)
        std::printf("      READING: Campaign is in the running on every silent world and loses\n"
                    "               every argmax. The subject is VERB COMPETITION, and T2 names\n"
                    "               what beats it and by how much.\n");
    else
        std::printf("      READING: the silent worlds are silent for DIFFERENT reasons — %d never\n"
                    "               clear, %d clear and lose. One fix will not serve both.\n",
                    silent_never_cleared, silent_cleared_lost);

    // ---- T2. Who beats Campaign, and by how much -------------------------
    std::printf("\n  === T2. WHEN CAMPAIGN CLEARS AND LOSES: the winning verb and the margin ===\n");
    std::printf("    margin = winning verb's score - best cleared Campaign score, in the shared\n"
                "    currency (BL-318). A margin of 3 is a weighting nudge; a margin of 3000 is\n"
                "    incommensurability — two scores authored on different scales. This file has\n"
                "    been bitten by that twice (w_cult flat 150; w_dist flat on a tripled map),\n"
                "    so the DISTRIBUTION is printed and no mean is taken anywhere.\n");

    int64_t total_lost_rounds = 0;
    for (const seed_row& r : rows) total_lost_rounds += r.cleared_lost;

    // WHICH VERBS ACTUALLY COMPETED, swept — the question `build_work` makes
    // load-bearing. A verb that never appears here either never wins or is not
    // in the contest at all, and those are different facts.
    {
        int64_t wins_by_verb[verb_slots] = {0};
        for (const seed_row& r : rows)
            for (int v = 0; v < verb_slots; ++v)
                wins_by_verb[v] += static_cast<int64_t>(r.margins[v].size());
        int64_t camp_wins = 0;
        for (const seed_row& r : rows) camp_wins += static_cast<int64_t>(r.won_camp_scores.size());
        std::printf("\n    Rounds won, swept, among rounds where Campaign was in the running:\n");
        std::printf("      %-12s %lld\n", verb_name(sim_verb::campaign),
                    static_cast<long long>(camp_wins));
        for (int v = 0; v < verb_slots; ++v)
        {
            if (v == static_cast<int>(sim_verb::campaign)) continue;
            std::printf("      %-12s %lld%s\n", verb_name(static_cast<sim_verb>(v)),
                        static_cast<long long>(wins_by_verb[v]),
                        (v == static_cast<int>(sim_verb::build_work) && !works_seen)
                            ? "   <- GATED OUT: no works registry (see the banner)" : "");
        }
    }

    if (total_lost_rounds == 0)
    {
        std::printf("\n    No round anywhere in the sweep had Campaign clear and lose.\n"
                    "    T2 does not apply: the fork's answer is on the other branch.\n");
    }
    else
    {
        for (const seed_row& r : rows)
        {
            if (r.cleared_lost == 0) continue;
            std::printf("\n    seed %d — %lld rounds cleared and lost:\n",
                        r.seed, static_cast<long long>(r.cleared_lost));
            std::printf("      %-11s | %7s | %-28s | %-17s | %-17s\n",
                        "winner", "rounds", "margin (min p25 med p75 p90 max)",
                        "Campaign scored", "winner scored");
            for (int v = 0; v < verb_slots; ++v)
            {
                if (r.margins[v].empty()) continue;
                std::vector<int> m = r.margins[v];
                std::vector<int> c = r.camp_scores[v];
                std::vector<int> w = r.win_scores[v];
                std::sort(m.begin(), m.end());
                std::sort(c.begin(), c.end());
                std::sort(w.begin(), w.end());
                std::printf("      %-11s | %7zu | %4d %4d %4d %4d %4d %5d | %4d %4d %4d   | %4d %4d %4d\n",
                            verb_name(static_cast<sim_verb>(v)), m.size(),
                            m.front(), quantile_of(m, 0.25), quantile_of(m, 0.50),
                            quantile_of(m, 0.75), quantile_of(m, 0.90), m.back(),
                            c.front(), quantile_of(c, 0.50), c.back(),
                            w.front(), quantile_of(w, 0.50), w.back());
            }
            // THE CONTROL, printed even when empty — "Campaign never scored high
            // enough to win a round here" is a finding, and a table that simply
            // omitted the line would read as an oversight instead.
            if (r.won_camp_scores.empty())
                std::printf("      control: Campaign won NO round on this seed, so there is no\n"
                            "               winning Campaign score to compare the losing ones to.\n");
            else
            {
                std::vector<int> wc = r.won_camp_scores;
                std::sort(wc.begin(), wc.end());
                std::printf("      control: on the %zu rounds Campaign DID win, its score ran"
                            " %d / %d / %d (min/med/max).\n",
                            wc.size(), wc.front(), quantile_of(wc, 0.50), wc.back());
            }
            // The histogram, because the quantiles above are still summary and
            // the two bugs separate on the decade.
            for (int v = 0; v < verb_slots; ++v)
            {
                if (r.margins[v].empty()) continue;
                int hist[margin_buckets] = {0};
                for (int m : r.margins[v]) ++hist[margin_bucket_of(m)];
                std::printf("      %-11s margin histogram:", verb_name(static_cast<sim_verb>(v)));
                for (int b = 0; b < margin_buckets; ++b)
                    std::printf("  %s:%d", margin_bucket_label(b), hist[b]);
                std::printf("\n");
            }
        }
    }

    // THE SHARPEST READ, AND THE ONE A TABLE OF QUANTILES HIDES: not how far
    // apart the medians are, but whether the two distributions OVERLAP AT ALL.
    // A Campaign score that could beat the winner on its best round and simply
    // usually does not is a weighting problem. A Campaign CEILING that sits
    // below the winner's FLOOR is a level problem — no round of that world was
    // ever winnable for Campaign, and no tie-break, salt or lucky target could
    // have changed it.
    if (total_lost_rounds > 0)
    {
        std::printf("\n    Ceiling against floor — do the two score distributions ever overlap?\n");
        std::printf("      Both columns are taken over LOST rounds only. See the note in the\n"
                    "      source for why a won round must not raise the ceiling.\n");
        std::printf("      seed | Campaign best cleared | winning verb's worst win | overlap?\n");
        for (const seed_row& r : rows)
        {
            if (r.cleared_lost == 0) continue;
            int camp_max = -1, win_min = -1;
            for (int v = 0; v < verb_slots; ++v)
                for (std::size_t j = 0; j < r.camp_scores[v].size(); ++j)
                {
                    if (r.camp_scores[v][j] > camp_max) camp_max = r.camp_scores[v][j];
                    if (win_min < 0 || r.win_scores[v][j] < win_min) win_min = r.win_scores[v][j];
                }
            // DELIBERATELY NOT FOLDING IN `r.won_camp_scores`. A round Campaign
            // WON contributes a score that by construction beat everything else
            // that round, so including it raises the ceiling on exactly the
            // seeds where Campaign wins — which are exactly the seeds this
            // column then reports as "overlaps". That conditions the statistic
            // on the outcome it is being used to explain, and a statistic that
            // cannot come out the other way is not evidence.
            // The won-round scores are reported above as their own control.
            std::printf("      %4d | %21d | %24d | %s%s\n",
                        r.seed, camp_max, win_min,
                        camp_max >= win_min ? "overlaps" : "DISJOINT — Campaign's best < the winner's worst",
                        r.battles == 0 ? "   <- silent world" : "");
        }
    }

    // ---- The rows --------------------------------------------------------
    //
    // Written over SEEDS, never over battles. A loop over `battle_trace`s
    // asserts nothing on a silent seed, which is precisely where the defect is.
    std::printf("\n  --- requirement group era-minus-one-fixture ---\n");

    // R1. Every seed classified, and the round accounting closes on each one.
    bool r1_total = true, r1_identity = true, r1_grain = true;
    for (const seed_row& r : rows)
    {
        if (r.reading == fork_reading::unclassified) r1_total = false;
        if (r.cleared_rounds != r.chosen + r.cleared_lost) r1_identity = false;
        // Candidate grain must dominate round grain, and clearing is a subset
        // of scoring. Both would break if a counter were incremented in the
        // wrong loop — the most likely way to get a plausible wrong answer.
        if (r.cleared < r.cleared_rounds || r.cleared > r.scored) r1_grain = false;
    }
    check(static_cast<int>(rows.size()) == seeds && r1_total && r1_identity && r1_grain,
          "R1 the fork is ANSWERED for every seed: all classified, and on each one "
          "cleared_rounds == chosen + lost with cleared_rounds <= cleared <= scored");

    // R2. The counters, not the battle set, carry the classification — and the
    // binding is written so an empty battle set cannot make it green.
    //
    // The universal clause is the one-directional implication the code
    // guarantees: `++out.battles` sits inside the Campaign case, AFTER
    // `++out.campaign_chosen`, so a seed that fought must also have chosen. It
    // fires on every seed in the sweep whether or not any world is silent —
    // which is what keeps this row alive in a sweep with no silent worlds, where
    // the silent-specific clause below has nothing to check.
    bool r2_ok = true;
    for (const seed_row& r : rows)
    {
        if (r.scored <= 0) r2_ok = false;                     // the funnel ran at all
        if (r.battles > 0 && r.chosen <= 0) r2_ok = false;    // fought => chose
        if (r.battles == 0)
        {
            // A silent seed must be classified from COUNTERS: candidates were
            // examined, none was chosen, and exactly one fork branch fired.
            const bool never = (r.cleared == 0);
            const bool lost  = (r.cleared > 0 && r.cleared_lost == r.cleared_rounds
                                && r.cleared_rounds > 0);
            if (r.chosen != 0 || !(never != lost)) r2_ok = false;
        }
    }
    std::printf("     R2 checked %d seeds, of which %d are silent.\n",
                static_cast<int>(rows.size()), silent_seen);
    check(r2_ok && !rows.empty(),
          "R2 the classification comes from COUNTERS, not from a battle set: every seed "
          "scored candidates, every seed that fought also chose, and every zero-battle "
          "seed fired exactly one fork branch with chosen == 0");

    // R3. The measured world is still the world under study. Pins only the seeds
    // the table holds rows for, so widening the sweep adds coverage without
    // weakening the regression — and reports how many it actually checked, so a
    // sweep that pinned nothing cannot read as a pass.
    int  r3_checked = 0;
    bool r3_ok = static_cast<int>(rows.size()) == seeds;
    for (const seed_row& r : rows)
    {
        if (r.seed < 0 || r.seed >= pinned_seeds) continue; // beyond the pinned set
        ++r3_checked;
        const pinned_row& p = pinned[r.seed];
        if (r.battles != p.battles || r.conquests != p.conquests || r.foundings != p.foundings
            || r.contacts != p.contacts || r.scored != p.scored || r.chosen != p.chosen)
        {
            r3_ok = false;
            std::printf("        seed %d MOVED: battles %lld (pinned %lld), conquests %lld (%lld), "
                        "foundings %lld (%lld), contacts %lld (%lld), scored %lld (%lld), chosen %lld (%lld)\n",
                        r.seed,
                        static_cast<long long>(r.battles),   static_cast<long long>(p.battles),
                        static_cast<long long>(r.conquests), static_cast<long long>(p.conquests),
                        static_cast<long long>(r.foundings), static_cast<long long>(p.foundings),
                        static_cast<long long>(r.contacts),  static_cast<long long>(p.contacts),
                        static_cast<long long>(r.scored),    static_cast<long long>(p.scored),
                        static_cast<long long>(r.chosen),    static_cast<long long>(p.chosen));
        }
    }
    std::printf("     R3 pinned %d of %d swept seeds against the regression table.\n",
                r3_checked, seeds);
    // The table in C++ form, so a legitimate re-pin is a copy rather than a
    // re-typing — the step where a re-pin usually acquires its typo.
    std::printf("     R3 current values, in pinned[] form:\n");
    for (const seed_row& r : rows)
    {
        if (r.seed < 0 || r.seed >= pinned_seeds) continue;
        std::printf("        {%5lld, %4lld, %4lld, %8lld, %9lld, %4lld},\n",
                    static_cast<long long>(r.battles), static_cast<long long>(r.conquests),
                    static_cast<long long>(r.foundings), static_cast<long long>(r.contacts),
                    static_cast<long long>(r.scored), static_cast<long long>(r.chosen));
    }
    check(r3_ok && r3_checked == pinned_seeds,
          "R3 the sweep reproduces the pinned table EXACTLY on every pinned seed, and all "
          "pinned seeds were actually reached");

    // R4. The instrument is inert AND gated.
    check(trace_is_inert && counters_are_gated && gating_seed >= 0,
          "R4 the instrument is INERT and GATED: traced == untraced on every non-trace output, "
          "and an untraced run leaves every trace counter empty (checked on a seed that "
          "actually moves them)");
    std::printf("        (gating pair ran on seed %d — the first whose traced run moves a counter)\n",
                gating_seed);

    // THE HEADLINE: how many worlds fight NO WAR AT ALL, in the era that ships.
    std::printf("\n  --- Q0. How many worlds fight at all, and where the silent ones stop ---\n");
    std::printf("    contacts = own region beside a FOREIGN-OWNED neighbour (an unowned\n"
                "    neighbour is not contact — it is somewhere to Settle). scored = reached\n"
                "    the score comparison. chosen = Campaign won the verb choice that round.\n");
    std::printf("    worlds with ZERO battles: %d / %d (%.0f%%)\n",
                peaceful_worlds, worlds,
                worlds > 0 ? 100.0 * peaceful_worlds / worlds : 0.0);

    if (all.empty())
    {
        std::printf("\n  No battles across %d worlds — the gap is UPSTREAM of the resolver:\n"
                    "  the scorer never selects Campaign at all. Nothing below applies.\n", seeds);
        std::printf("\n%d checks, %d failures\n%s\n", g_checks, g_failures,
                    g_failures == 0 ? "ALL PASS (0 failures)" : "FAILURES");
        return g_failures == 0 ? 0 : 1;
    }

    // ---- Q1. Does the attacker win, and does winning move a border? -------
    int won = 0, took = 0;
    for (const battle_trace& b : all) { if (b.attacker_won) ++won; if (b.conquered) ++took; }

    std::printf("\n  --- Q1. WHERE the run stops: at the fight, or at the bar? ---\n");
    std::printf("    battles traced      %zu\n", all.size());
    std::printf("    attacker victories  %d  (%.1f%%)\n", won,
                100.0 * won / static_cast<double>(all.size()));
    std::printf("    of those, conquests %d  (%.1f%% of victories)\n", took,
                won > 0 ? 100.0 * took / static_cast<double>(won) : 0.0);
    if (won == 0)
        std::printf("    READING: the attacker NEVER wins. The conquest bar is irrelevant;\n"
                    "             the gap is entirely in the matchup.\n");
    else if (took == 0)
        std::printf("    READING: the attacker wins %d times and takes nothing. The gap is at\n"
                    "             the TRANSFER BAR, not in the fight — tuning combat constants\n"
                    "             would be tuning the wrong thing.\n", won);
    else
        std::printf("    READING: both halves fire. The gap is a RATE, not a dead branch.\n");

    // ---- Q2. Is the scorer's estimate systematically optimistic? ----------
    std::printf("\n  --- Q2. The scorer's estimate against the realised outcome ---\n");
    std::printf("    est. p_win  |  battles  |  actually won  |  error\n");
    struct bucket { int n = 0, w = 0; int64_t est = 0; };
    bucket buckets[5];
    for (const battle_trace& b : all)
    {
        const int idx = std::min(4, std::max(0, b.p_win_q / 200));
        buckets[idx].n++;
        buckets[idx].est += b.p_win_q;
        if (b.attacker_won) buckets[idx].w++;
    }
    for (int i = 0; i < 5; ++i)
    {
        if (buckets[i].n == 0) continue;
        const double est  = buckets[i].est / 1000.0 / buckets[i].n;
        const double real = buckets[i].w / static_cast<double>(buckets[i].n);
        std::printf("    %.1f–%.1f     |  %7d  |  %12.1f%% |  %+.1f pp\n",
                    i * 0.2, (i + 1) * 0.2, buckets[i].n, 100.0 * real,
                    100.0 * (real - est));
    }

    // ---- Q3. The two terms the scorer cannot see --------------------------
    std::vector<int> terr_v, works_v;
    int with_terrain = 0, with_works = 0;
    for (const battle_trace& b : all)
    {
        terr_v.push_back(b.terrain_defence_q);
        works_v.push_back(b.works_defence_q);
        if (b.terrain_defence_q != 0) ++with_terrain;
        if (b.works_defence_q   != 0) ++with_works;
    }
    std::printf("\n  --- Q3. The defender's invisible terms ---\n");
    std::printf("    terrain_defence   median %4d   non-zero in %d/%zu battles\n",
                median_of(terr_v), with_terrain, all.size());
    std::printf("    works_defence     median %4d   non-zero in %d/%zu battles%s\n",
                median_of(works_v), with_works, all.size(),
                works_seen ? "" : "   <- necessarily zero: no works registry");
    std::printf("    Neither appears in the scorer's p_win. If Q2's error tracks the\n"
                "    magnitude here, BL-384's hypothesis is established; if the medians\n"
                "    are ~0, the hypothesis is REFUTED and the cause is elsewhere.\n");

    // ---- Q4. The hub the scorer used vs the hub that actually levied ------
    int hub_mismatch = 0;
    for (const battle_trace& b : all)
        if (b.scored_hub != b.exec_hub) ++hub_mismatch;
    std::printf("\n  --- Q4. The second candidate mechanism: scored hub vs levying hub ---\n");
    std::printf("    scored one hub, levied from another: %d/%zu (%.1f%%)\n",
                hub_mismatch, all.size(), 100.0 * hub_mismatch / static_cast<double>(all.size()));
    std::printf("    The scorer estimates attacker strength from the MAXIMUM-manpower\n"
                "    holding; execution levies from the NEAREST. Where these differ the\n"
                "    estimate is optimistic for a reason that has nothing to do with terrain.\n");

    // ---- Q5. How far short does decisiveness fall? ------------------------
    std::vector<int> shortfall;
    int cleared = 0;
    for (const battle_trace& b : all)
    {
        if (!b.attacker_won) continue;
        if (b.decisiveness >= b.transfer_needed) ++cleared;
        else shortfall.push_back(b.transfer_needed - b.decisiveness);
    }
    std::printf("\n  --- Q5. The transfer bar, for victories only ---\n");
    if (won == 0)
        std::printf("    No victories to measure.\n");
    else
    {
        std::printf("    victories clearing the bar: %d/%d\n", cleared, won);
        if (!shortfall.empty())
        {
            std::sort(shortfall.begin(), shortfall.end());
            std::printf("    shortfall when it misses: median %d, best near-miss %d, worst %d\n",
                        median_of(shortfall), shortfall.front(), shortfall.back());
            std::printf("    A small median shortfall means the bar is nearly reachable and the\n"
                        "    fix is proportionate. A large one means the two sides of this\n"
                        "    comparison are on different scales — BL-318's incommensurability.\n");
        }
    }

    std::printf("\n%d checks, %d failures\n%s\n", g_checks, g_failures,
                g_failures == 0 ? "ALL PASS (0 failures)" : "FAILURES");
    return g_failures == 0 ? 0 : 1;
}
