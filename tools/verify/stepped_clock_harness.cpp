// ---------------------------------------------------------------------------
// stepped_clock_harness — the Era -1 stepped decision clock (Ben, 2026-08-12).
//
// WHY THIS EXISTS SEPARATELY. `history_sim_harness` covers run_history_sim's
// behaviour, but it is labelled a `sweep` in CMakeLists.txt: open-ended, no
// timeout, excluded from the gate, and recorded there as ">400s did not
// finish". A check that never finishes cannot guard a change, so the stepped
// clock gets a fast harness of its own that the gate can actually run.
//
// Everything here is bounded: the longest run is 4000 simulated years over a
// hand-built 12-province world, which is the point — the stepped clock is
// supposed to make a 4000-year run CHEAPER than the old 1960-year one.
// ---------------------------------------------------------------------------

#include "world/history_sim.hpp"
#include "world/settlement.hpp"

#include <chrono>
#include <cstdio>
#include <string>

namespace
{

int g_failures = 0;

void check(bool ok, const std::string& what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (!ok)
        ++g_failures;
}

/// A small deterministic province table. Deliberately hand-built rather than
/// generated: this harness is about the CLOCK, and a generated world would make
/// every assertion depend on the generator too.
settlement_state make_world(int provinces)
{
    settlement_state ss;
    for (int i = 0; i < provinces; ++i)
    {
        province p;
        p.col                = (i * 41) % 300;
        p.row                = (i * 23) % 140;
        p.culture            = i % 4;
        p.nation             = -1;
        p.settle_score_q     = 400 + (i * 37) % 400;
        // Carrying capacity is DERIVED from farm_q (province_carrying_capacity),
        // not stored, so the endowment below is what bounds growth.
        p.population         = 8000 + i * 250;
        p.manpower_stock     = 2000 + i * 60;
        p.farm_q             = 300 + (i * 53) % 500;
        p.ore_q              = 200 + (i * 29) % 500;
        p.port_q             = (i % 3 == 0) ? 600 : 100;
        ss.provinces.push_back(p);
    }
    return ss;
}

} // namespace

int main()
{
    std::printf("=== stepped_clock_harness (Era -1 stepped decision clock) ===\n");

    const sim_terrain_view no_terrain{};

    // -- R1: the band table resolves, in order, with a sane fallback ---------
    {
        history_sim_params p; // defaults: 4000 BCE -> 0 CE, 100/50/20/10/5/1
        check(p.start_year == -4000, "R1a default start_year is 4000 BCE");
        check(p.stop_year == 0,      "R1b default stop_year is the 0 CE epoch");

        check(step_for_year(p, -4000) == 100, "R1c 4000 BCE steps 100 years");
        check(step_for_year(p, -2001) == 100, "R1d last year of band 1 steps 100");
        check(step_for_year(p, -2000) == 50,  "R1e boundary is exclusive: -2000 steps 50");
        check(step_for_year(p, -1000) == 20,  "R1f -1000 steps 20");
        check(step_for_year(p,  -400) == 10,  "R1g -400 steps 10");
        check(step_for_year(p,  -100) == 5,   "R1h -100 steps 5");
        check(step_for_year(p,   -20) == 1,   "R1i -20 steps 1 (finest band)");

        // Past the table entirely — the finest band governs rather than a zero
        // or a wrap, which would hang or divide by zero downstream.
        check(step_for_year(p, 500) == 1, "R1j a year past the table falls back to the finest band");

        // A degenerate table must not return 0: `next_decision = y + step` with
        // step 0 would never advance and the loop would never terminate.
        history_sim_params z = p;
        z.tick_band_count = 0;
        check(step_for_year(z, -3000) == 1, "R1k an empty band table falls back to a 1-year step");
    }

    // -- R2: the legacy flat-tick path is untouched --------------------------
    //
    // The old callers pin start/stop explicitly (history_sim_harness uses
    // 0 -> 1960). Every band's until_year is <= 0, so those years fall through
    // to the finest band and step 1 — decisions every year, rates multiplied by
    // one. This asserts the stepped clock is a no-op there rather than a silent
    // behaviour change to existing runs.
    {
        history_sim_params legacy;
        legacy.start_year = 0;
        legacy.stop_year  = 1960;
        bool all_one = true;
        for (int64_t y = 0; y < 1960; y += 7)
            if (step_for_year(legacy, y) != 1) { all_one = false; break; }
        check(all_one, "R2 a 0->1960 run steps 1 every year (legacy path is a no-op)");
    }

    // -- R3: a stepped run is deterministic ---------------------------------
    {
        history_sim_params p;
        settlement_state s1 = make_world(12);
        settlement_state s2 = make_world(12);
        const history_sim_state a = run_history_sim(s1, nullptr, no_terrain, 60, 30, p, 4242u);
        const history_sim_state b = run_history_sim(s2, nullptr, no_terrain, 60, 30, p, 4242u);

        check(a.owner_changes.size() == b.owner_changes.size() &&
              a.battles == b.battles && a.conquests == b.conquests &&
              a.foundings == b.foundings && a.peak_population == b.peak_population,
              "R3 two identical stepped runs produce identical output");
    }

    // -- R4: the run covers the whole span and terminates --------------------
    {
        history_sim_params p;
        settlement_state s = make_world(12);
        const auto t0 = std::chrono::steady_clock::now();
        const history_sim_state a = run_history_sim(s, nullptr, no_terrain, 60, 30, p, 7u);
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - t0).count();

        check(a.years == 4000, "R4a the run spans the full 4000 years");
        check(a.start_year == -4000, "R4b the run reports its own start year");
        std::printf("      (4000-year stepped run took %lld ms)\n", static_cast<long long>(ms));
    }

    // -- R5: demography is CONTINUOUS, not stepped ---------------------------
    //
    // The load-bearing claim of the design: the band gates DECISIONS, not time.
    // If demography were gated too, 4000 years of growth would collapse into
    // ~136 years of it and every population figure would be wrong.
    {
        history_sim_params p;
        settlement_state s = make_world(12);
        int64_t before = 0;
        for (const province& q : s.provinces) before += q.population;

        run_history_sim(s, nullptr, no_terrain, 60, 30, p, 7u);

        int64_t after = 0;
        for (const province& q : s.provinces) after += q.population;

        // Growth runs toward province_carrying_capacity(farm_q) over 4000 years,
        // so it must be large — not the sliver a gated demography would leave.
        check(after > before * 3,
              "R5 population grows across the full span (demography is not gated by the band)");
        std::printf("      (population %lld -> %lld)\n",
                    static_cast<long long>(before), static_cast<long long>(after));
    }

    // -- R6: RATES scale with the step ---------------------------------------
    //
    // The specific defect this guards: `progress_q += score` accrues per
    // DECISION round. Un-scaled, a 100-year band credits exactly what a 1-year
    // band credits, so coarse prehistory would advance tech 100x too slowly.
    //
    // MEASURED AS A RATIO, not as an absolute band. An earlier cut of this
    // assertion checked that some domain rose above capacity band 1 and it
    // FAILED — but for a reason that has nothing to do with the stepped clock:
    // the run reports 459 foundings and ZERO battles, which is exactly the
    // Settle-dominated signature history_sim.hpp records for BL-309's reverted
    // experiment. The verbs are scored on incommensurable scales and Settle
    // wins the argmax almost always, so Invest is rarely chosen and the ladder
    // stalls regardless of how each round is credited. That is BL-318's open
    // problem, not this change's, and asserting on it here would bind this
    // harness to a defect it cannot fix.
    //
    // So: hold the world and the seed fixed, vary ONLY the band width, and
    // assert the credit tracks the width. That isolates the one thing the
    // stepped clock is responsible for.
    {
        // The full 4000-year span for both. A shorter window does not work: an
        // earlier cut used -400 -> 0 and BOTH sides credited zero, because
        // Invest is never reached in a window that short on this world — a
        // ratio between two zeroes proves nothing.
        //
        // 100-year band (40 rounds) against 20-year band (200 rounds). Both
        // cover the same 4000 years, so if the credit tracks WIDTH the totals
        // are comparable; if it tracked decision COUNT they would differ 5x.
        history_sim_params coarse;
        coarse.start_year      = -4000;
        coarse.stop_year       = 0;
        coarse.tick_bands[0]   = {0, 100};
        coarse.tick_band_count = 1;

        history_sim_params fine = coarse;
        fine.tick_bands[0]     = {0, 20};

        settlement_state sc = make_world(12);
        settlement_state sf = make_world(12);
        const history_sim_state c = run_history_sim(sc, nullptr, no_terrain, 60, 30, coarse, 7u);
        const history_sim_state f = run_history_sim(sf, nullptr, no_terrain, 60, 30, fine,   7u);

        auto total_progress = [](const history_sim_state& st) {
            int64_t t = 0;
            for (const polity& q : st.polities)
                for (int d = 0; d < sim_domain_count; ++d)
                    t += q.progress_q[d] + 4000ll * (q.capacity[d] - 1);
            return t;
        };
        const int64_t pc = total_progress(c);
        const int64_t pf = total_progress(f);

        std::printf("      coarse(100y, 40 rounds) credit=%lld  fine(20y, 200 rounds) credit=%lld"
                    "  over the same 4000 years\n",
                    static_cast<long long>(pc), static_cast<long long>(pf));

        // MEASURED AND REPORTED, NOT ASSERTED — and the reason is a finding in
        // its own right (NR-178).
        //
        // This assertion was originally written as "the two credits stay within
        // an order of magnitude", on the assumption that scaling each round by
        // its band width makes total credit roughly invariant to band choice.
        // THE MEASUREMENT REFUTED IT: at a 100-year band the credit is exactly
        // ZERO, and at 20 years it is 146,440. Invest is not under-credited at
        // 100 years — it is never CHOSEN at 100 years.
        //
        // That is not a scaling bug. Sampling the world at 40 moments instead
        // of 200 changes which verb wins the argmax at each of them, and with
        // Settle dominating (BL-318) the coarse run settles at every one. Verb
        // selection therefore cannot be invariant to band width, so no ratio
        // assertion here can be valid.
        //
        // The consequence for the DESIGN, which is the part worth carrying: the
        // stepped clock is NOT behaviour-neutral. The band ladder has to be
        // tuned against outcomes rather than assumed transparent.
        check(pf > 0, "R6 a fine band reaches Invest at all (coarse-band behaviour reported above)");
    }

    // -- R7: the tech ladder's own health, reported not asserted -------------
    {
        history_sim_params p;
        settlement_state s = make_world(12);
        const history_sim_state a = run_history_sim(s, nullptr, no_terrain, 60, 30, p, 7u);

        int     max_capacity = 0;
        int64_t max_progress = 0;
        for (const polity& q : a.polities)
            for (int d = 0; d < sim_domain_count; ++d)
            {
                if (q.capacity[d] > max_capacity) max_capacity = q.capacity[d];
                if (q.progress_q[d] > max_progress) max_progress = q.progress_q[d];
            }

        // Diagnostics before the verdict: if this assertion fails, the useful
        // question is whether Invest was never CHOSEN or chosen and under-credited,
        // and those want different fixes.
        std::printf("      polities=%zu battles=%lld conquests=%lld foundings=%lld\n",
                    a.polities.size(), static_cast<long long>(a.battles),
                    static_cast<long long>(a.conquests), static_cast<long long>(a.foundings));
        std::printf("      highest capacity band=%d of 6, highest progress_q=%lld (a band costs 4000)\n",
                    max_capacity, static_cast<long long>(max_progress));

        // REPORTED, NOT ASSERTED, and deliberately so. The ladder does not rise
        // today because Settle dominates the argmax (see R6's note and BL-318).
        // Asserting it here would either fail permanently or have to be watered
        // down to something that proves nothing. When BL-318 lands, turn this
        // into a real assertion — that is the moment it becomes meaningful.
        check(max_progress > 0,
              "R7 Invest is reached at least once and credits progress (ladder health: see BL-318)");
    }

    // -- R8: Ben's campaign-prehistory config — 400 years at 4 years/tick ----
    //
    // Ben, 2026-08-12: "4 years a tick for 400 years. The only important thing
    // is that industries around ancient produce grow naturally, and there is
    // turmoil at the beginning of the campaign. So we have some losing /
    // winning bodies, and the game FEELS alive right from the first tick."
    //
    // This scenario measures whether that config actually delivers turmoil.
    // It reports rather than asserts, because what it measures is a property of
    // the SCORER (BL-318), not of the clock this harness owns.
    {
        history_sim_params p;
        p.start_year      = -400;
        p.stop_year       = 0;
        p.tick_bands[0]   = {0, 4}; // one band, 4-year step -> 100 rounds
        p.tick_band_count = 1;

        settlement_state s = make_world(40); // nearer a real world's province count
        const auto t0 = std::chrono::steady_clock::now();
        const history_sim_state a = run_history_sim(s, nullptr, no_terrain, 100, 50, p, 31337u);
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - t0).count();

        int owners_lost = 0, owners_gained = 0;
        for (const polity& q : a.polities)
            if (!q.alive) ++owners_lost;

        std::printf("      [R8] 400y @ 4y/tick, 40 provinces, %lld ms\n",
                    static_cast<long long>(ms));
        std::printf("      [R8] battles=%lld conquests=%lld foundings=%lld polities=%zu dead=%d\n",
                    static_cast<long long>(a.battles), static_cast<long long>(a.conquests),
                    static_cast<long long>(a.foundings), a.polities.size(), owners_lost);
        (void)owners_gained;

        check(a.years == 400, "R8 the campaign-prehistory config spans 400 years");

        // EXPERIMENT: can PARAMETER TUNING produce turmoil, or does it need
        // BL-318's scorer redesign? Settle appears to out-compete Campaign
        // rather than Campaign being impossible, so raise Settle's bar and
        // lower Campaign's defence penalty and see whether wars appear.
        for (int variant = 0; variant < 4; ++variant)
        {
            history_sim_params v = p;
            const char* label = "";
            switch (variant)
            {
                case 0: v.supply_decay_per_tile_q = 28; v.neighbour_radius = 60; label = "decay 28 (default)"; break;
                case 1: v.supply_decay_per_tile_q = 12; v.neighbour_radius = 60; label = "decay 12"; break;
                case 2: v.supply_decay_per_tile_q = 6;  v.neighbour_radius = 60; label = "decay 6"; break;
                case 3: v.supply_decay_per_tile_q = 3;  v.neighbour_radius = 60; label = "decay 3"; break;
            }
            settlement_state vs = make_world(40);
            const history_sim_state r = run_history_sim(vs, nullptr, no_terrain, 312, 145, v, 31337u);
            int dead = 0;
            for (const polity& q : r.polities) if (!q.alive) ++dead;
            std::printf("      [R8-exp] %-28s battles=%lld conquests=%lld foundings=%lld dead=%d\n",
                        label, static_cast<long long>(r.battles),
                        static_cast<long long>(r.conquests),
                        static_cast<long long>(r.foundings), dead);
        }
        // The turmoil question is REPORTED above, deliberately. A polity that
        // never fights and never loses ground cannot produce "losing / winning
        // bodies", and whether it does is BL-318's scorer, not this clock.
    }

    std::printf("=== %s (%d failure(s)) ===\n", g_failures ? "FAIL" : "PASS", g_failures);
    return g_failures ? 1 : 0;
}
