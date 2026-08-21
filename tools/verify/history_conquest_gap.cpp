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
#include <vector>

namespace
{

int g_failures = 0;

void check(bool ok, const char* what)
{
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what);
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

int main()
{
    std::printf("=== history_conquest_gap (BL-384) — report only, asserts nothing about the gap ===\n\n");

    constexpr int seeds = 8;
    std::vector<battle_trace> all;

    // Determinism guard on the instrument itself: a traced run and an untraced
    // run must agree in every other output. An instrument that perturbs what it
    // measures is worse than no instrument, and this is the one thing here that
    // IS asserted.
    bool trace_is_inert = true;
    int  peaceful_worlds = 0;
    int  worlds = 0;

    struct seed_row { int seed; campaign_funnel funnel; };
    std::vector<seed_row> seed_rows;

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
            p_off.start_year = -400; p_off.stop_year = 0;
            p_off.tick_bands[0] = {0, 4}; p_off.tick_band_count = 1;
            const history_sim_state off =
                run_history_sim(ss_off, nullptr, terr.view(), kgw, kgh, p_off, wp.seed);

            settlement_state ss_chk = k->settlement;
            history_sim_params p_chk;
            p_chk.start_year = -400; p_chk.stop_year = 0;
            p_chk.tick_bands[0] = {0, 4}; p_chk.tick_band_count = 1;
            p_chk.trace_battles = true;
            const history_sim_state chk =
                run_history_sim(ss_chk, nullptr, terr.view(), kgw, kgh, p_chk, wp.seed);

            if (off.battles != chk.battles || off.conquests != chk.conquests
                || off.foundings != chk.foundings || off.winter_campaigns != chk.winter_campaigns
                || off.owner_changes.size() != chk.owner_changes.size())
                trace_is_inert = false;
        }

        settlement_state ss_on = k->settlement;
        history_sim_params p_on;
        p_on.trace_battles = true;

        // PRODUCTION PARAMETERS, NOT THE STRUCT DEFAULTS — and this is the whole
        // reason the harness was measuring a world the game does not generate.
        //
        // `history_sim_params`'s defaults are a 4000-year run (-4000 -> 0) on the
        // six-band ladder, 136 decision rounds. `make_hard_coded_world` runs
        // something else entirely: `prehistory_years = 400`, so -400 -> 0, on ONE
        // band with step 4 — 100 rounds over a tenth of the span. Every history
        // harness in the repo, this one included until now, took the defaults and
        // therefore measured a sim production never runs.
        //
        // Mirrored from hard_coded_world.cpp rather than shared, because a
        // constant restated in a harness is a known cost while a harness reaching
        // into generation's private setup is a worse one. If that block changes,
        // this must change with it — the same contract sea_leg_census carries for
        // its rate constants.
        p_on.start_year      = -400;
        p_on.stop_year       = 0;
        p_on.tick_bands[0]   = {0, 4};
        p_on.tick_band_count = 1;
        const history_sim_state on =
            run_history_sim(ss_on, nullptr, terr.view(), kgw, kgh, p_on, wp.seed);

        if (on.battles == 0) ++peaceful_worlds;
        ++worlds;

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

        seed_rows.push_back({i, on.funnel});
        all.insert(all.end(), on.battle_traces.begin(), on.battle_traces.end());
    }

    check(trace_is_inert, "T1 tracing is INERT — a traced run matches an untraced one in every other output");
    check(!all.empty(),   "T2 the instrument caught something (a run with zero battles would explain nothing)");

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

    // ---- T1/T2. THE FORK: never cleared, or cleared and beaten? -----------
    // Sprint 28's gate. Everything downstream is conditional on this, and the
    // two cases need different fixes.
    std::printf("\n  --- T1/T2. Inside the choice: why Campaign is not taken ---\n");
    {
        const char* verb_name[6] = {"none", "settle", "campaign", "invest",
                                    "consolidate", "build_work"};
        for (const seed_row& r : seed_rows)
        {
            const campaign_funnel& f = r.funnel;
            std::printf("    seed %d: cleared %8lld  (lost %8lld)   below-threshold %8lld\n",
                        r.seed,
                        static_cast<long long>(f.rounds_cleared),
                        static_cast<long long>(f.rounds_lost),
                        static_cast<long long>(f.rounds_below));
            if (f.rounds_lost > 0)
            {
                std::printf("             beaten by:");
                for (int v = 0; v < 6; ++v)
                    if (f.lost_to[v] > 0)
                        std::printf("  %s x%lld", verb_name[v],
                                    static_cast<long long>(f.lost_to[v]));
                std::printf("\n             margin  mean %lld  min %d  max %d\n",
                            static_cast<long long>(f.margin_sum / f.rounds_lost),
                            f.margin_min, f.margin_max);
            }
            if (f.rounds_below > 0)
                std::printf("             shortfall to threshold  mean %lld  min %d  max %d\n",
                            static_cast<long long>(f.below_shortfall_sum / f.rounds_below),
                            f.below_shortfall_min, f.below_shortfall_max);
            std::printf("             settle chosen %8lld  of which FAILED (no free cell) %8lld\n",
                        static_cast<long long>(f.settle_chosen),
                        static_cast<long long>(f.settle_failed));
        }
        std::printf("\n    HOW TO READ IT. `cleared` means Campaign met campaign_threshold_q and\n"
                    "    entered the > comparison; `lost` means another verb then outscored it.\n"
                    "    A silent world with cleared=0 is a THRESHOLD problem and the shortfall\n"
                    "    says how far off. A silent world with cleared>0 and lost=cleared is a\n"
                    "    WEIGHTING problem, and the margin decides which kind: small is a nudge,\n"
                    "    enormous is two scores on different scales (BL-318), which this file has\n"
                    "    been bitten by twice.\n\n"
                    "    The settle line is the SPECIFIC form of that suspicion. Campaign\n"
                    "    discounts itself by p_win_q; Settle does not ask whether an empty cell\n"
                    "    exists at all, so a FAILED settle is a round where the sim preferred an\n"
                    "    impossible action to a possible one.\n");
    }

    if (all.empty())
    {
        std::printf("\n  No battles across %d worlds. The per-battle sections below cannot\n"
                    "  apply — but the T1/T2 fork above is exactly the reading for this case.\n", seeds);
        std::printf("\n%d checks, %d failures\n%s\n", 2, g_failures,
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

    std::printf("\n2 checks, %d failures\n%s\n", g_failures,
                g_failures == 0 ? "ALL PASS (0 failures)" : "FAILURES");
    return g_failures == 0 ? 0 : 1;
}
