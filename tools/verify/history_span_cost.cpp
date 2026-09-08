// ---------------------------------------------------------------------------
// history_span_cost — BL-825. WHAT DOES A 4000-YEAR PRE-HISTORY COST?
// ---------------------------------------------------------------------------
//
// A MEASUREMENT HARNESS. It optimises nothing, changes no sim behaviour, and
// asserts almost nothing — the deliverable is a number. Generation currently
// runs `prehistory_years = 400` (hard_coded_world.hpp) and the design wants
// 4000; nobody had ever run it, so the span question was being argued from
// reading rather than from a clock.
//
// WHAT IT MEASURES
//   1. Wall time of the era at spans of 400 / 1000 / 2000 / 4000 years, on the
//      same world, so the spans are comparable.
//   2. The split inside a 4000-year run — demography, the decision round, the
//      battles inside it, and the heapless Dijkstra `rebuild_reach` — read from
//      `history_sim_last_profile()` (history_sim.hpp), which is report-only and
//      never seen by the sim.
//   3. The step curve: `tick_bands[0].step_years` swept at the 4000-year span.
//      That is the one knob trading fidelity directly for speed.
//   4. Region and polity count, since the cost is a function of both.
//
// HOW IT ISOLATES THE ERA. It builds each world ONCE with the Era -1 fixture
// captured (era_minus_one.hpp) and then re-runs `run_history_sim` from the
// fixture's own arguments, varying only the span or the clock. That is the same
// route history_sweep's `--epoch` path takes, and for the same reason: the
// fixture is what generation actually handed the sim, so a re-run measures
// generation's era rather than a struct default. Row S1 below is the acceptance
// test — the 400-year re-run must reproduce the fixture's battle / conquest /
// founding counts exactly, or nothing else printed here describes the shipped
// sim. It doubles as the determinism check on the profiling instrumentation:
// those counts are generation's, taken before this harness touched anything.
//
// Usage:  history_span_cost [seed_count]        (default 3)
// Build:  node tools/verify/build_harness.js history_span_cost      (/O2 = Release)
//         DEBUG TIMINGS FROM THIS HARNESS ARE MEANINGLESS. Quote the build tree
//         with any figure, always.
// ---------------------------------------------------------------------------

#include "world/era_minus_one.hpp"
#include "world/hard_coded_world.hpp"
#include "world/history_sim.hpp"
#include "world/settlement.hpp"
#include "world/works_roster.hpp"
#include "world/world.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
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

const generation_report::body_entry* kepler_of(const generation_report& r)
{
    for (const generation_report::body_entry& b : r.bodies)
        if (b.is_homeworld) return &b;
    return r.bodies.empty() ? nullptr : &r.bodies.front();
}

/// Milliseconds of wall clock a callable takes.
template <typename F>
double ms_of(F&& f)
{
    const auto t0 = std::chrono::steady_clock::now();
    f();
    const auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

/// Live polities at the end of a run — the count the cost actually scales on,
/// since a dead polity is `continue`d past on the first line of the round.
int alive_polities(const history_sim_state& s)
{
    int n = 0;
    for (const polity& p : s.polities) if (p.alive) ++n;
    return n;
}

struct span_row
{
    int64_t span    = 0;
    double  ms      = 0.0;
    double  us_year = 0.0;
    int64_t battles = 0;
    int64_t rounds  = 0;
    int     powers  = 0;
    int     regions_end = 0; ///< The sim FOUNDS regions, and `rebuild_reach` is O(N^2) in them.
    double  ms_reach    = 0.0;
};

} // namespace

int main(int argc, char** argv)
{
    const int seed_count = argc > 1 ? std::atoi(argv[1]) : 3;

    std::printf("\n=== history_span_cost — BL-825, what a 4000-year pre-history costs ===\n");
    std::printf("Seeds: %d.  Every era run below is re-run FROM GENERATION'S OWN FIXTURE\n"
                "(era_minus_one.hpp), so the span is the only thing that varies.\n"
                "TIMINGS ARE BUILD-TREE SPECIFIC. Quote the tree with the number.\n\n",
                seed_count);

    const int64_t spans[] = {400, 1000, 2000, 4000};
    const int     steps[] = {1, 2, 4, 8, 16};

    for (int s = 0; s < seed_count; ++s)
    {
        world_params wp;
        wp.seed = static_cast<uint32_t>(s);
        // The era must RUN for the fixture to be captured, and it runs at
        // generation's shipped 400. The spans below are re-runs, not this.
        wp.prehistory_years = 400;

        generation_report     rep;
        era_minus_one_fixture fx;

        const double ms_world = ms_of([&] {
            const world w = make_hard_coded_world(wp, &rep, world_gen_config{},
                                                  /*progress=*/nullptr, /*works=*/nullptr, &fx);
            (void)w;
        });

        const generation_report::body_entry* k = kepler_of(rep);
        if (k == nullptr || !fx.ran)
        {
            std::printf("seed %u: THE ERA DID NOT RUN (fixture gate false) — skipped.\n", wp.seed);
            continue;
        }

        std::printf("---------------------------------------------------------------\n");
        std::printf("seed %u   regions %d   world build %.0f ms   generation's era "
                    "(%lld yr, step %d) %lld ms\n",
                    wp.seed, static_cast<int>(fx.settlement.regions.size()),
                    ms_world, static_cast<long long>(fx.years),
                    fx.params.tick_bands[0].step_years,
                    static_cast<long long>(fx.ms_era));

        // --- S1: the re-run IS generation's era ---------------------------
        {
            settlement_state  ss  = fx.settlement;   // the PRE-sim state
            history_sim_state sim = run_history_sim(ss, &fx.creeds, fx.terrain.view(),
                                                    fx.gw, fx.gh, fx.params, fx.seed,
                                                    nullptr, fx.works);
            const bool same = sim.battles == fx.battles
                           && sim.conquests == fx.conquests
                           && sim.foundings == fx.foundings;
            std::string lbl = "seed " + std::to_string(wp.seed)
                            + "  S1: re-run reproduces generation's era exactly "
                              "(battles/conquests/foundings) — also the determinism "
                              "check on the BL-825 profiling instrumentation";
            check(same, lbl.c_str());
            if (!same)
                std::printf("      got %lld/%lld/%lld, generation had %lld/%lld/%lld\n",
                            static_cast<long long>(sim.battles),
                            static_cast<long long>(sim.conquests),
                            static_cast<long long>(sim.foundings),
                            static_cast<long long>(fx.battles),
                            static_cast<long long>(fx.conquests),
                            static_cast<long long>(fx.foundings));
        }

        // --- (1) The span table -------------------------------------------
        std::vector<span_row> rows;
        for (int64_t span : spans)
        {
            history_sim_params hp = fx.params;
            hp.start_year = hp.stop_year - span;

            settlement_state  ss = fx.settlement;
            history_sim_state sim;
            span_row row;
            row.span = span;
            row.ms = ms_of([&] {
                sim = run_history_sim(ss, &fx.creeds, fx.terrain.view(), fx.gw, fx.gh,
                                      hp, fx.seed, nullptr, fx.works);
            });
            const history_sim_profile prof = history_sim_last_profile();
            row.us_year = (row.ms * 1000.0) / static_cast<double>(span);
            row.battles = sim.battles;
            row.rounds  = prof.decision_rounds;
            row.powers  = alive_polities(sim);
            row.regions_end = static_cast<int>(ss.regions.size());
            row.ms_reach    = prof.ns_reach / 1e6;
            rows.push_back(row);

            if (span == 4000)
            {
                // --- (2) The split, at the span the item is about ----------
                const double tot = row.ms;
                const double dem = prof.ns_demography / 1e6;
                const double dec = prof.ns_decisions  / 1e6;
                const double bat = prof.ns_battles    / 1e6;
                const double rch = prof.ns_reach      / 1e6;
                std::printf("  split @4000yr:  total %.0f ms  |  demography %.0f ms (%.0f%%)"
                            "  decisions %.0f ms (%.0f%%)  [of which battles %.0f ms (%.0f%%),"
                            "  reach/Dijkstra %.0f ms (%.0f%%)]  other %.0f ms\n",
                            tot, dem, 100.0 * dem / tot, dec, 100.0 * dec / tot,
                            bat, 100.0 * bat / tot, rch, 100.0 * rch / tot,
                            tot - dem - dec);
                std::printf("  counts @4000yr: decision rounds %lld   reach rebuilds %lld"
                            "   battles %lld   powers alive %d\n",
                            static_cast<long long>(prof.decision_rounds),
                            static_cast<long long>(prof.reach_rebuilds),
                            static_cast<long long>(sim.battles), row.powers);
            }
        }

        std::printf("  span(yr)     ms    us/yr   rounds  battles  powers  regions_end"
                    "  reach ms  ms/rebuild\n");
        for (const span_row& r : rows)
            std::printf("  %6lld  %7.0f  %7.1f   %6lld  %7lld  %6d  %11d  %8.0f  %10.3f\n",
                        static_cast<long long>(r.span), r.ms, r.us_year,
                        static_cast<long long>(r.rounds),
                        static_cast<long long>(r.battles), r.powers, r.regions_end,
                        r.ms_reach,
                        r.rounds > 0 ? r.ms_reach / static_cast<double>(r.rounds * r.powers)
                                     : 0.0);

        // Linearity, stated rather than left to the reader: per-year cost at
        // 4000 against per-year cost at 400. A ratio near 1.0 is flat.
        if (rows.size() == 4 && rows[0].us_year > 0.0)
            std::printf("  per-year cost 4000yr / 400yr = %.2fx  (1.0 = linear in years)\n",
                        rows[3].us_year / rows[0].us_year);

        // --- (3) The step curve at 4000 years ------------------------------
        std::printf("  step_years curve @ 4000-year span:\n");
        std::printf("   step     ms   rounds  battles  powers\n");
        for (int st : steps)
        {
            history_sim_params hp = fx.params;
            hp.start_year = hp.stop_year - 4000;
            for (int b = 0; b < hp.tick_band_count; ++b) hp.tick_bands[b].step_years = st;

            settlement_state  ss = fx.settlement;
            history_sim_state sim;
            const double ms = ms_of([&] {
                sim = run_history_sim(ss, &fx.creeds, fx.terrain.view(), fx.gw, fx.gh,
                                      hp, fx.seed, nullptr, fx.works);
            });
            const history_sim_profile prof = history_sim_last_profile();
            std::printf("   %4d  %7.0f   %6lld  %7lld  %6d\n", st, ms,
                        static_cast<long long>(prof.decision_rounds),
                        static_cast<long long>(sim.battles), alive_polities(sim));
        }
        std::printf("\n");
    }

    std::printf("=== history_span_cost: %d failure(s) ===\n", g_failures);
    std::printf("REPORTS, DOES NOT GATE — the only assertion is S1 (the re-run is\n"
                "generation's own era). No timing here is ever asserted: a wall clock\n"
                "differs every run and on every machine.\n");
    return g_failures == 0 ? 0 : 1;
}
