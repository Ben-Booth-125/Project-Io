// ---------------------------------------------------------------------------
// history_conquest_gap — WHY the Era −1 sim fights and never conquers (BL-384)
// ---------------------------------------------------------------------------
// REPORT-ONLY, AND DELIBERATELY. `history_sim_harness`'s B384a/B384b already
// ASSERT that a region changes hands by war, and they are red. This harness is
// the other half: it does not re-assert the failure, it explains it.
//
// BL-384's design is explicit that the mechanism must be CONFIRMED rather than
// assumed — "instrument one run to log, per battle, the scorer's estimated
// p_win against the realised outcome. If the estimate is systematically
// optimistic by roughly the terrain factor, the mechanism is established... If
// it is not, the cause is elsewhere and tuning combat constants would be tuning
// the wrong thing." This is that instrument.
//
// THE HYPOTHESIS, stated so the numbers can refute it. The scorer estimates the
// odds as attacker levy x supply x cohesion against the defender's RAW
// manpower_stock. `resolve_battle` is then handed a defender carrying TWO terms
// the scorer never saw: `terrain_defence(...)`, and `work_defence_mod` as
// readiness. So the scorer should pick fights on odds the resolver does not
// honour, and the error should be one-directional.
//
// A SECOND CANDIDATE MECHANISM, which the trace can separate from the first and
// which reading alone cannot: the scorer estimates the attacker's strength from
// the MAXIMUM-manpower holding, while execution levies from the NEAREST one.
// Those are usually different regions. That is optimism too, but it has nothing
// to do with terrain, and the fix for it is different.
//
// AND A THIRD, which is why the trace is taken at the conquest bar rather than
// at the resolver: "267 battles, 0 conquests" is consistent with the attacker
// never WINNING, and equally consistent with it winning often and never
// clearing `transfer_decisiveness_q`. Those are different bugs. Any diagnosis
// that cannot tell them apart is a guess.
//
// Headless: world/* logic only, no SDL and no Lua.
// ---------------------------------------------------------------------------

#include "world/hard_coded_world.hpp"
#include "world/history_sim.hpp"
#include "world/sim_terrain_build.hpp"
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

const generation_report::body_entry* kepler_of(const generation_report& rep)
{
    for (const auto& b : rep.bodies)
        if (b.settlement.regions.size() > 0) return &b;
    return nullptr;
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
/// than over the battles. That distinction is the point: seeds 0 and 4 produce
/// no battles at all, so any check written as a loop over `battle_trace`s is
/// silently vacuous on exactly the two worlds this sprint exists to explain.
struct seed_row
{
    int     seed = 0;
    int64_t battles = 0, conquests = 0, foundings = 0;
    int64_t contacts = 0, scored = 0, chosen = 0;
    int64_t cleared = 0, cleared_rounds = 0, cleared_lost = 0;
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

/// The pinned table (R3). These are the counts the pre-instrumentation build
/// produced on 2026-08-23, taken from a run of this harness against HEAD before
/// the T1/T2 counters were added.
///
/// THE POINT OF PINNING THEM IS NOT REGRESSION IN GENERAL — it is that a
/// MEASUREMENT which no longer reproduces the defect is measuring a different
/// world. Seeds 0 and 4 at zero battles are the defect; if instrumentation
/// moved them, every number this harness prints would describe a sim that no
/// longer has the problem the sprint was scoped around.
struct pinned_row { int64_t battles, conquests, foundings, contacts, scored, chosen; };
/// The PINNED REGRESSION SET is deliberately separate from the SWEEP WIDTH
/// below, and they must stay separate. They were one number until 2026-08-23,
/// when `pinned[r.seed]` was indexed by a seed the sweep could be widened past —
/// so raising `seeds` read out of bounds, and the only safe sweep was the one the
/// pinned table happened to be the same size as. That coupling is the mechanical
/// reason the sprint's headline claim rested on n=8 with two silent worlds in it:
/// the instrument could not be widened to test its own generalisation.
/// R3 now pins only the seeds it holds rows for and says how many it checked.
constexpr int pinned_seeds = 8;
constexpr pinned_row pinned[pinned_seeds] = {
    {   0,    0, 786, 4972710,  9945420,   0},
    { 236,  210, 643, 6189696, 12379392, 241},
    { 435,  435, 543, 2620190,  5240380, 435},
    { 163,  163, 383, 3062571,  6125142, 163},
    {   0,    0, 762, 4089264,  8178528,   0},
    { 164,  164, 628, 3796047,  7592094, 165},
    {  44,   43, 872, 5441888, 10883776,  44},
    { 184,  184, 662, 1578177,  3156354, 185},
};

// THE CONSTANTS, NEVER THE NUMBERS. The first cut of this harness hardcoded
// 312x145 — the homeworld's size before BL-424 took it to 70% area — against a
// real grid of 261x121. `build_sim_terrain` fills an oversized grid with its
// defaults (sedimentary / grass / plains), so every region anchor outside the
// real bounds read as flat grassland and `region_distance`'s cylinder wrapped at
// the wrong width. The measurement was of a world that does not exist.
//
// It is worth naming the tell: terrain_defence came back non-zero in only 20% of
// battles, which read as a finding about the sim and was actually a finding about
// the harness. A number that surprises you is a number to check the fixture for.
constexpr int kgw = home_grid_width;
constexpr int kgh = home_grid_height;

} // namespace

int main(int argc, char** argv)
{
    std::printf("=== history_conquest_gap (BL-384) — report only, asserts nothing about the gap ===\n\n");

    // SWEEP WIDTH, independent of `pinned_seeds`. Override from the command line
    // to falsify a claim drawn from the default sweep — which is exactly what
    // n=32 did to this sprint's headline on 2026-08-23.
    int seeds = pinned_seeds;
    if (argc > 1)
    {
        const int want = std::atoi(argv[1]);
        if (want > 0 && want <= 256) seeds = want;
    }
    std::vector<battle_trace> all;

    // Determinism guard on the instrument itself: a traced run and an untraced
    // run must agree in every other output. An instrument that perturbs what it
    // measures is worse than no instrument, and this is the one thing here that
    // IS asserted.
    bool trace_is_inert = true;
    /// R4's second half: the new T1/T2 counters must be GATED, not merely
    /// harmless. An untraced run has to leave every one of them at zero and
    /// `verb_contests` empty — otherwise the sim is paying for the measurement
    /// on every generated world, and the inertness check above would still be
    /// green because it only compares the OTHER outputs.
    bool counters_are_gated = true;
    /// THE ANTI-VACUITY GUARD ON R4. Checking "the untraced run left the
    /// counters at zero" on a seed whose TRACED run also leaves them at zero
    /// proves nothing — and seed 0, where the existing inertness pair runs, is
    /// exactly a candidate for that. So the gating pair is run on the first
    /// seed whose traced run actually moves a counter, and the seed it ran on
    /// is asserted to exist.
    int  gating_seed = -1;
    int  peaceful_worlds = 0;
    int  worlds = 0;
    std::vector<seed_row> rows;

    for (int i = 0; i < seeds; ++i)
    {
        world_params wp;
        wp.seed = static_cast<uint32_t>(i);
        generation_report rep;
        const world w = make_hard_coded_world(wp, &rep);

        const generation_report::body_entry* k = kepler_of(rep);
        if (k == nullptr) continue;

        const sim_terrain_arrays terr = build_sim_terrain(w, k->id, kgw, kgh);

        // The inertness check runs on ONE seed rather than doubling every run:
        // the property is structural (no trace field feeds a decision or a
        // draw), so one world exercises it, and a second full sim per seed
        // costs minutes for a claim the first already settles.
        if (i == 0)
        {
            settlement_state ss_off = k->settlement;
            history_sim_params p_off;
            const history_sim_state off =
                run_history_sim(ss_off, nullptr, terr.view(), kgw, kgh, p_off, wp.seed);

            settlement_state ss_chk = k->settlement;
            history_sim_params p_chk;
            p_chk.trace_battles = true;
            const history_sim_state chk =
                run_history_sim(ss_chk, nullptr, terr.view(), kgw, kgh, p_chk, wp.seed);

            if (off.battles != chk.battles || off.conquests != chk.conquests
                || off.foundings != chk.foundings || off.winter_campaigns != chk.winter_campaigns
                || off.owner_changes.size() != chk.owner_changes.size())
                trace_is_inert = false;

            if (off.campaign_cleared != 0 || off.campaign_cleared_rounds != 0
                || off.campaign_cleared_lost != 0 || !off.verb_contests.empty())
                counters_are_gated = false;
        }

        settlement_state ss_on = k->settlement;
        history_sim_params p_on;
        p_on.trace_battles = true;
        const history_sim_state on =
            run_history_sim(ss_on, nullptr, terr.view(), kgw, kgh, p_on, wp.seed);

        if (on.battles == 0) ++peaceful_worlds;
        ++worlds;

        if (gating_seed < 0 && on.campaign_cleared > 0)
        {
            settlement_state ss_g = k->settlement;
            history_sim_params p_g; // trace_battles stays false.
            const history_sim_state g =
                run_history_sim(ss_g, nullptr, terr.view(), kgw, kgh, p_g, wp.seed);
            if (g.campaign_cleared != 0 || g.campaign_cleared_rounds != 0
                || g.campaign_cleared_lost != 0 || !g.verb_contests.empty())
                counters_are_gated = false;
            if (g.battles != on.battles || g.conquests != on.conquests
                || g.foundings != on.foundings || g.winter_campaigns != on.winter_campaigns
                || g.owner_changes.size() != on.owner_changes.size())
                trace_is_inert = false;
            gating_seed = i;
        }

        seed_row row;
        row.seed      = i;
        row.battles   = on.battles;
        row.conquests = on.conquests;
        row.foundings = on.foundings;
        row.contacts  = on.campaign_contacts;
        row.scored    = on.campaign_scored;
        row.chosen    = on.campaign_chosen;
        row.cleared        = on.campaign_cleared;
        row.cleared_rounds = on.campaign_cleared_rounds;
        row.cleared_lost   = on.campaign_cleared_lost;

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

        // The funnel, printed for EVERY seed including the silent ones — a world
        // with no battles has no traces, so the per-battle record is mute about
        // exactly the case that needs explaining.
        std::printf("  seed %d: %4lld battles  %4lld conquests  %4lld foundings"
                    "  | contacts %7lld  scored %8lld  chosen %5lld\n",
                    i, static_cast<long long>(on.battles),
                    static_cast<long long>(on.conquests),
                    static_cast<long long>(on.foundings),
                    static_cast<long long>(on.campaign_contacts),
                    static_cast<long long>(on.campaign_scored),
                    static_cast<long long>(on.campaign_chosen));
        std::fflush(stdout);

        all.insert(all.end(), on.battle_traces.begin(), on.battle_traces.end());
    }

    // ======================================================================
    // T1/T2 — VERB COMPETITION. Sprint 28 lane A.
    // ======================================================================
    //
    // WHY THIS IS THE WHOLE SPRINT'S FORK. Everything above (and BL-384 in its
    // entirety) is about battles that happened. Two worlds in eight fight NO
    // WAR IN AN ENTIRE ERA — not few, none — and for those worlds every
    // per-battle number here is measuring an empty set. `campaign_chosen == 0`
    // with `campaign_scored` in the millions narrows it to "the scorer looks at
    // a war it could start ten million times and picks something else", and
    // then stops: a candidate DISCARDED BELOW ITS THRESHOLD and one that
    // CLEARED AND LOST THE ARGMAX are the same zero.
    //
    // They are different defects. Never-cleared is a defect in the campaign
    // score itself; cleared-and-lost is a defect in the comparison between
    // verbs — and the fix for one would be inert against the other. That is
    // what T1 separates and what T2 then sizes.
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
        std::printf("      (none in this sweep — the defect this sprint was scoped around\n"
                    "       is NOT REPRODUCING, and every number below describes a different world.)\n");
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
            //
            // Measured both ways, 2026-08-23: excluding them changes ONE cell in
            // eight (seed 2, 411 -> 390) and no verdict. So the conditioning was
            // harmless here — but it was harmless by luck, and the honest form
            // is the one that would still have been readable if it had not been.
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
    // asserts nothing on seeds 0 and 4, which is precisely where the defect is.
    std::printf("\n  --- requirement group verb-competition-measurement ---\n");

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
          "R1 the fork is ANSWERED for every seed: all 8 classified, and on each one "
          "cleared_rounds == chosen + lost with cleared_rounds <= cleared <= scored");

    // R2. The silent seeds are measured, not skipped — and the assertion is
    // written so an empty battle set cannot make it green.
    bool r2_ok = silent_seen > 0;
    for (const seed_row& r : rows)
    {
        if (r.battles != 0) continue;
        // A silent seed must be classified from COUNTERS: candidates were
        // examined, none was chosen, and exactly one branch of the fork fired.
        const bool never  = (r.cleared == 0);
        const bool lost   = (r.cleared > 0 && r.cleared_lost == r.cleared_rounds && r.cleared_rounds > 0);
        if (r.scored == 0 || r.chosen != 0 || !(never != lost)) r2_ok = false;
    }
    check(r2_ok, "R2 every ZERO-BATTLE seed is classified from counters, not from an empty "
                 "battle set: scored > 0, chosen == 0, and exactly one fork branch fired");

    // R3. The measured world is still the defective world. Pins only the seeds
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
    check(r3_ok && r3_checked == pinned_seeds,
          "R3 the instrumented sweep reproduces the pinned table EXACTLY — seeds 0 and 4 "
          "still at zero battles, every pinned seed's 6 counts unmoved, and all 8 pinned "
          "seeds were actually reached");

    // R4. The instrument is inert AND gated.
    check(trace_is_inert && counters_are_gated && gating_seed >= 0,
          "R4 the instrument is INERT and GATED: traced == untraced on every non-trace output, "
          "and an untraced run leaves all four new counters empty (checked on a seed that "
          "actually moves them)");
    std::printf("        (gating pair ran on seed %d — the first whose traced run moves a counter)\n",
                gating_seed);

    // THE HEADLINE, and it is not the one BL-384 expected: how many worlds
    // fight NO WAR AT ALL. A sim that conquers heavily on most seeds and is
    // perfectly peaceful on others has a different defect from one that never
    // conquers, and the item was written against a single seed.
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
    // The comparison BL-384 names. Bucketed by the estimate, because a mean
    // error over all battles would hide a bias that only bites at the margin —
    // and the margin is where a scorer's errors actually cost you.
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
    std::printf("    works_defence     median %4d   non-zero in %d/%zu battles\n",
                median_of(works_v), with_works, all.size());
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
