// ---------------------------------------------------------------------------
// continent_drift — BL-763. The drift TIME AXIS.
//
// `tectonic_plate::drift_col`/`drift_row` were documented "per-epoch" while no
// epoch length was defined anywhere, so the drift vector existed and nothing
// integrated it. This harness checks the axis that closes that: an epoch length
// and depth that are stated constants, and `continent_snapshot_at` reconstructing
// any past configuration as a pure function of the plates.
//
// THE LOAD-BEARING ROW IS C1. Everything the Life phase (BL-765) will hang off
// this depends on epoch 0 reproducing `continent_state::plate_id` EXACTLY — if
// the reconstruction disagrees with the present at zero offset, every deeper
// epoch is fiction and the 0 CE world would move the moment anything read it.
//
// Lua-free and world-free by construction: it hand-builds the two
// `planetology_state` fields `run_continents` actually reads, so it runs in
// milliseconds rather than building a world.
// ---------------------------------------------------------------------------

#include "world/continents.hpp"
#include "world/planetology.hpp"

#include <chrono>
#include <cstdio>
#include <vector>

namespace
{

int g_failures = 0;

void check(bool ok, const char* label)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", label);
    if (!ok) ++g_failures;
}

planetology_state mobile_body(float theta)
{
    planetology_state pl;
    pl.mobile_lid = true;
    pl.theta      = theta;
    return pl;
}

} // namespace

int main()
{
    constexpr int gw = 261, gh = 121; // the home body's grid (hard_coded_world.hpp)

    std::printf("=== continent_drift (BL-763) — epoch %lld y, depth %d epochs (%lld My) ===\n\n",
                static_cast<long long>(continent_epoch_years),
                continent_drift_epochs,
                static_cast<long long>(continent_epoch_years)
                    * continent_drift_epochs / 1'000'000);

    const planetology_state pl = mobile_body(0.85f);
    const continent_state   cs = run_continents(pl, gw, gh, 0xC0117E57u);

    std::printf("     plates %d\n\n", static_cast<int>(cs.plates.size()));

    // --- C1: epoch 0 IS the present ----------------------------------------
    const continent_snapshot now = continent_snapshot_at(cs, 0, gw, gh);
    check(now.plate_id == cs.plate_id,
          "C1   epoch 0 reproduces continent_state::plate_id bit-identically");
    check(now.epochs_back == 0 && now.years_before_present == 0,
          "C1b  epoch 0 carries a zero age");

    // --- C2: the axis actually moves ground --------------------------------
    const continent_snapshot deep = continent_snapshot_at(cs, continent_drift_epochs, gw, gh);
    int moved = 0;
    for (std::size_t i = 0; i < deep.plate_id.size(); ++i)
        if (deep.plate_id[i] != cs.plate_id[i]) ++moved;
    const int pct = static_cast<int>((static_cast<long long>(moved) * 100)
                                     / static_cast<long long>(cs.plate_id.size()));
    std::printf("     at %d epochs back, %d%% of tiles sit on a different plate\n",
                continent_drift_epochs, pct);
    check(moved > 0, "C2   a deep epoch differs from the present — drift is integrated, not ignored");
    check(deep.years_before_present
              == static_cast<int64_t>(continent_drift_epochs) * continent_epoch_years,
          "C2b  the age is epochs x the stated epoch length");

    // --- C3: monotone, and deterministic -----------------------------------
    const continent_snapshot again = continent_snapshot_at(cs, continent_drift_epochs, gw, gh);
    check(again.plate_id == deep.plate_id, "C3   two reconstructions of one epoch agree");

    int prev_moved = 0;
    bool monotone  = true;
    for (int e = 0; e <= continent_drift_epochs; ++e)
    {
        const continent_snapshot s = continent_snapshot_at(cs, e, gw, gh);
        int m = 0;
        for (std::size_t i = 0; i < s.plate_id.size(); ++i)
            if (s.plate_id[i] != cs.plate_id[i]) ++m;
        if (m < prev_moved - static_cast<int>(s.plate_id.size()) / 50) monotone = false;
        prev_moved = m;
    }
    check(monotone, "C3b  divergence from the present does not COLLAPSE as the epoch deepens");

    // --- C4: no randomness was consumed ------------------------------------
    // Structural, and the reason it is safe to add this at all: the plate stream
    // draws position, direction and oceanic flag for every plate in one fixed
    // order, so any new draw would shift every later plate. A second identical
    // run_continents proves the stream is where it was.
    const continent_state cs2 = run_continents(pl, gw, gh, 0xC0117E57u);
    bool plates_same = cs2.plates.size() == cs.plates.size();
    for (std::size_t i = 0; plates_same && i < cs.plates.size(); ++i)
        plates_same = cs2.plates[i].seed_col == cs.plates[i].seed_col
                   && cs2.plates[i].seed_row == cs.plates[i].seed_row
                   && cs2.plates[i].drift_col == cs.plates[i].drift_col
                   && cs2.plates[i].drift_row == cs.plates[i].drift_row
                   && cs2.plates[i].oceanic == cs.plates[i].oceanic;
    check(plates_same && cs2.plate_id == cs.plate_id,
          "C4   run_continents is unchanged — the snapshot consumes no randomness");

    // --- C5: a stagnant lid has no history to reconstruct -------------------
    planetology_state stag;
    stag.mobile_lid = false;
    stag.theta      = 0.4f;
    const continent_state    sl   = run_continents(stag, gw, gh, 0xC0117E57u);
    const continent_snapshot sl_d = continent_snapshot_at(sl, continent_drift_epochs, gw, gh);
    bool all_zero = sl_d.plate_id.size() == static_cast<std::size_t>(gw) * gh;
    for (int v : sl_d.plate_id) if (v != 0) { all_zero = false; break; }
    check(sl.plates.size() == 1 && all_zero,
          "C5   a stagnant lid owns everything at every epoch — nothing drifted");

    // --- C6: the cost, REPORTED ---------------------------------------------
    // R4's constraint is that the expensive half of the pass — the rift-basin
    // search — is NOT re-run per epoch. This measures what a snapshot actually
    // costs against a full run_continents, which is the observable form of it.
    const auto t0 = std::chrono::steady_clock::now();
    for (int e = 0; e <= continent_drift_epochs; ++e)
        (void)continent_snapshot_at(cs, e, gw, gh);
    const auto t1 = std::chrono::steady_clock::now();
    (void)run_continents(pl, gw, gh, 0xC0117E57u);
    const auto t2 = std::chrono::steady_clock::now();

    const double snaps_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    const double full_ms  = std::chrono::duration<double, std::milli>(t2 - t1).count();
    std::printf("\n     %d snapshots %.1f ms (%.2f ms each) | one full run_continents %.1f ms\n",
                continent_drift_epochs + 1, snaps_ms,
                snaps_ms / (continent_drift_epochs + 1), full_ms);
    std::printf("     REPORTED, not asserted: a snapshot must be cheaper than the full pass,\n"
                "     which is what shows the basin search is not being re-run.\n");

    std::printf("\n%s (%d failure%s)\n", g_failures == 0 ? "ALL PASS" : "FAILURES",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
