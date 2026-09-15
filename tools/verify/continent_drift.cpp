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

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
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

// --- The drift digest (BL-964) ------------------------------------------
// FNV-1a over the whole drift record of the reference seed, in the same style
// world_determinism's deep_digest uses. What it folds, in order: the present
// plate set; the present surface (height_bias, convergent, divergent — the
// three consumer-facing rasters CONTINENTS.md § Outputs and contracts names,
// in raster order); then for EVERY epoch on the clock, 0..continent_drift_epochs,
// the wound-back plate set and the epoch's plate_id raster. One constant pins
// all of it: a change to the Voronoi tie-break, the drift table, the boundary
// classification, the epoch length, the depth, or the winding arithmetic moves
// the digest, and nothing else in this harness would necessarily notice.

constexpr uint64_t fnv_basis = 14695981039346656037ull;
constexpr uint64_t fnv_prime = 1099511628211ull;

void fold_bytes(uint64_t& h, const void* p, std::size_t n)
{
    const auto* b = static_cast<const unsigned char*>(p);
    for (std::size_t i = 0; i < n; ++i)
    {
        h ^= static_cast<uint64_t>(b[i]);
        h *= fnv_prime;
    }
}

void fold_i32(uint64_t& h, int32_t v) { fold_bytes(h, &v, sizeof v); }
void fold_i64(uint64_t& h, int64_t v) { fold_bytes(h, &v, sizeof v); }
void fold_f32(uint64_t& h, float v)   { fold_bytes(h, &v, sizeof v); }
void fold_u8 (uint64_t& h, uint8_t v) { fold_bytes(h, &v, sizeof v); }

void fold_plates(uint64_t& h, const std::vector<tectonic_plate>& plates)
{
    fold_i32(h, static_cast<int32_t>(plates.size()));
    for (const tectonic_plate& p : plates)
    {
        fold_f32(h, p.seed_col);
        fold_f32(h, p.seed_row);
        fold_f32(h, p.drift_col);
        fold_f32(h, p.drift_row);
        fold_u8 (h, p.oceanic ? 1 : 0);
    }
}

uint64_t drift_digest(const continent_state& cs, int gw, int gh)
{
    uint64_t h = fnv_basis;

    fold_plates(h, cs.plates);
    fold_i32(h, static_cast<int32_t>(cs.height_bias.size()));
    for (float v : cs.height_bias) fold_f32(h, v);
    fold_i32(h, static_cast<int32_t>(cs.convergent.size()));
    for (uint8_t v : cs.convergent) fold_u8(h, v);
    fold_i32(h, static_cast<int32_t>(cs.divergent.size()));
    for (uint8_t v : cs.divergent) fold_u8(h, v);

    for (int e = 0; e <= continent_drift_epochs; ++e)
    {
        const continent_snapshot s = continent_snapshot_at(cs, e, gw, gh);
        fold_i32(h, s.epochs_back);
        fold_i64(h, s.years_before_present);
        fold_plates(h, s.plates);
        fold_i32(h, static_cast<int32_t>(s.plate_id.size()));
        for (int v : s.plate_id) fold_i32(h, v);
    }
    return h;
}

/// The blessed drift digest for seed 0xC0117E57 on the 261x121 home grid,
/// theta 0.85, mobile lid — 21 epochs at 5 My. Blessed ONCE, 2026-09-15, from
/// `node tools/verify/build_harness.js continent_drift --run` (BL-964). A
/// world-independent curated golden: it moves only when the drift record's
/// content legitimately changes, and then the delta is reported and re-blessed
/// with authorisation, never quietly.
constexpr uint64_t k_drift_digest = 0x481BDCA5300AF14Full;

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

    // --- C6: the cost, against a CEILING (BL-964) ---------------------------
    // R4's constraint is that the expensive half of the pass — the rift-basin
    // search — is NOT re-run per epoch. This measures what the whole 21-epoch
    // snapshot set costs against one full run_continents, which is the
    // observable form of it. Best-of-3 on each side so a scheduler hiccup does
    // not decide the row; a RATIO rather than a millisecond figure so the
    // ceiling holds on any machine.
    //
    // Measured 2026-09-15 (release harness build): 21 snapshots 4.8 ms, one full
    // pass 3.1 ms — a ratio of ~1.5. The ceiling is 4x the full pass, which is
    // generous by design: a snapshot that re-ran the basin search would cost
    // ~21x, so 4x separates "not re-running it" from "re-running it" with room
    // and does not turn timer noise into a red row. The full pass is floored at
    // 0.5 ms before scaling so a spuriously fast timer read cannot fail it.
    auto best_of_3 = [](auto&& fn) {
        double best = 1e9;
        for (int k = 0; k < 3; ++k)
        {
            const auto a = std::chrono::steady_clock::now();
            fn();
            const auto b = std::chrono::steady_clock::now();
            best = std::min(best, std::chrono::duration<double, std::milli>(b - a).count());
        }
        return best;
    };
    const double snaps_ms = best_of_3([&] {
        for (int e = 0; e <= continent_drift_epochs; ++e)
            (void)continent_snapshot_at(cs, e, gw, gh);
    });
    const double full_ms = best_of_3([&] { (void)run_continents(pl, gw, gh, 0xC0117E57u); });
    constexpr double k_cost_ceiling_ratio = 4.0;
    const double ceiling_ms = k_cost_ceiling_ratio * std::max(full_ms, 0.5);
    std::printf("\n     %d snapshots %.1f ms (%.2f ms each) | one full run_continents %.1f ms"
                " | ceiling %.1f ms (4x)\n",
                continent_drift_epochs + 1, snaps_ms,
                snaps_ms / (continent_drift_epochs + 1), full_ms, ceiling_ms);
    check(snaps_ms <= ceiling_ms,
          "C6   the whole snapshot set costs under 4x one full pass (measured ~1.5x, 2026-09-15) — "
          "the basin search is not re-run per epoch");

    // --- C7: the drift record is PINNED (BL-964) -----------------------------
    // Every row above checks a property of the record; none pins its content.
    // One digest over the reference seed's whole 21-epoch snapshot set does.
    const uint64_t digest = drift_digest(cs, gw, gh);
    std::printf("     drift digest %016llX (blessed %016llX)\n",
                static_cast<unsigned long long>(digest),
                static_cast<unsigned long long>(k_drift_digest));
    check(digest == k_drift_digest,
          "C7   the reference seed's 21-epoch drift record matches the blessed digest "
          "(2026-09-15) — plates, height_bias, convergent, divergent, every epoch's plate_id");

    // =======================================================================
    // BL-764 slice 1 — TILES RIDE PLATES
    // =======================================================================
    //
    // BL-763 gave drift a clock. These rows check the frame of reference it was
    // missing: a tile as a MATERIAL POINT on its plate, able to answer where it
    // was and what climate it sat in at a past epoch.
    //
    // THE LOAD-BEARING ROW IS P1, for the same reason C1 is above. If epoch 0
    // does not reproduce the present exactly — position, band and moisture — then
    // every deeper epoch is fiction AND the 0 CE world moves the moment BL-765
    // reads it.
    std::printf("\n--- BL-764: tiles ride plates ---\n\n");

    constexpr temperature_class k_temp = temperature_class::temperate;

    // A deterministic stand-in moisture field. The real one comes from
    // generation_record::moisture; what matters here is that the SAME field is
    // sampled at the past position, so a hash-shaped field with structure is
    // both sufficient and stricter than a smooth one.
    std::vector<float> moist(static_cast<std::size_t>(gw) * gh, 0.0f);
    for (std::size_t i = 0; i < moist.size(); ++i)
    {
        uint32_t h = static_cast<uint32_t>(i) * 2654435761u;
        h ^= h >> 15;
        moist[i] = static_cast<float>(h & 0xFFFFu) / 65535.0f;
    }

    // --- P1: epoch 0 IS the present, on every axis --------------------------
    bool p1_pos = true, p1_band = true, p1_moist = true, p1_plate = true;
    for (int row = 0; row < gh; ++row)
        for (int col = 0; col < gw; ++col)
        {
            const paleo_tile_state s =
                paleo_tile_at(cs, gw, gh, col, row, 0, k_temp, &moist);
            const std::size_t i = static_cast<std::size_t>(col + row * gw);
            if (s.col != static_cast<float>(col) || s.row != static_cast<float>(row)) p1_pos = false;
            if (s.band != band_for_row(row, gh, k_temp)) p1_band = false;
            if (s.moisture != moist[i] || !s.moisture_known) p1_moist = false;
            if (s.plate != cs.plate_id[i]) p1_plate = false;
        }
    check(p1_pos,   "P1   epoch 0 puts every tile exactly where it is today");
    check(p1_band,  "P1b  epoch 0 reproduces Pass 3's own band, tile for tile");
    check(p1_moist, "P1c  epoch 0 samples the tile's own moisture cell");
    check(p1_plate, "P1d  a tile rides the plate the present assignment gives it");

    // --- P2: the ground moves WITH its plate, not under it ------------------
    // The consistency that makes the two reconstructions one model: a tile's
    // offset from its plate's seed is a CONSTANT of the tile. If it were not,
    // ground would slide across plates as the epoch deepened, which is exactly
    // the Voronoi-reshuffle defect this slice exists to remove.
    bool rigid = true;
    for (int e = 0; e <= continent_drift_epochs && rigid; ++e)
    {
        const continent_snapshot snap = continent_snapshot_at(cs, e, gw, gh);
        for (int row = 0; row < gh && rigid; row += 7)
            for (int col = 0; col < gw && rigid; col += 11)
            {
                const paleo_tile_state s =
                    paleo_tile_at(cs, gw, gh, col, row, e, k_temp, &moist);
                const tectonic_plate& now  = cs.plates[static_cast<std::size_t>(s.plate)];
                const tectonic_plate& then = snap.plates[static_cast<std::size_t>(s.plate)];

                // Row offset is exact; column offset is compared as a wrapped
                // distance, since both the seed and the ground wrap.
                const float dr_now  = static_cast<float>(row) - now.seed_row;
                const float dr_then = s.row - then.seed_row;
                float dc_now  = std::fabs(static_cast<float>(col) - now.seed_col);
                float dc_then = std::fabs(s.col - then.seed_col);
                if (dc_now  > gw * 0.5f) dc_now  = gw - dc_now;
                if (dc_then > gw * 0.5f) dc_then = gw - dc_then;

                if (std::fabs(dr_now - dr_then) > 0.01f) rigid = false;
                if (std::fabs(dc_now - dc_then) > 0.01f) rigid = false;
            }
    }
    check(rigid, "P2   a tile's offset from its plate's seed is constant — it RIDES the plate");

    // --- P3: paleo-latitude actually moves ----------------------------------
    int band_moved = 0, pos_moved = 0;
    for (int row = 0; row < gh; ++row)
        for (int col = 0; col < gw; ++col)
        {
            const paleo_tile_state s =
                paleo_tile_at(cs, gw, gh, col, row, continent_drift_epochs, k_temp, &moist);
            if (s.band != band_for_row(row, gh, k_temp)) ++band_moved;
            if (s.row != static_cast<float>(row) || s.col != static_cast<float>(col)) ++pos_moved;
        }
    const long long cells = static_cast<long long>(gw) * gh;
    std::printf("     at %d epochs back (%lld My): %lld%% of tiles moved, %lld%% changed climate band\n",
                continent_drift_epochs,
                static_cast<long long>(continent_epoch_years) * continent_drift_epochs / 1'000'000,
                static_cast<long long>(pos_moved) * 100 / cells,
                static_cast<long long>(band_moved) * 100 / cells);
    check(pos_moved > 0,  "P3   a deep epoch moves the ground — the tile is not its raster row");
    check(band_moved > 0, "P3b  and moves some of it into a DIFFERENT climate band — "
                          "palaeo-latitude exists");

    // --- P4: latitude folds over the pole, it does not run off the scale ----
    bool in_range = true;
    int  off_grid = 0;
    for (int e = 0; e <= continent_drift_epochs; ++e)
        for (int row = 0; row < gh; row += 3)
            for (int col = 0; col < gw; col += 13)
            {
                const paleo_tile_state s =
                    paleo_tile_at(cs, gw, gh, col, row, e, k_temp, &moist);
                if (!(s.latitude >= 0.0f && s.latitude <= 1.0f)) in_range = false;
                if (!s.on_grid) ++off_grid;
            }
    check(in_range, "P4   palaeo-latitude stays in [0,1] at every epoch — ground past a pole "
                    "folds back");
    std::printf("     %d sampled (tile, epoch) pairs sat off the grid — reported as on_grid=false,\n"
                "     never as a silently clamped moisture sample.\n", off_grid);

    // --- P5: deterministic, and consumes nothing ----------------------------
    bool repeatable = true;
    for (int row = 0; row < gh && repeatable; row += 5)
        for (int col = 0; col < gw && repeatable; col += 9)
        {
            const paleo_tile_state a = paleo_tile_at(cs, gw, gh, col, row, 13, k_temp, &moist);
            const paleo_tile_state b = paleo_tile_at(cs, gw, gh, col, row, 13, k_temp, &moist);
            repeatable = a.col == b.col && a.row == b.row && a.band == b.band
                      && a.latitude == b.latitude && a.moisture == b.moisture
                      && a.plate == b.plate;
        }
    check(repeatable, "P5   two reconstructions of one (tile, epoch) agree");

    // --- P6: a stagnant lid never moved -------------------------------------
    bool stationary = true;
    for (int row = 0; row < gh && stationary; row += 5)
        for (int col = 0; col < gw && stationary; col += 9)
        {
            const paleo_tile_state s =
                paleo_tile_at(sl, gw, gh, col, row, continent_drift_epochs, k_temp, &moist);
            stationary = s.col == static_cast<float>(col)
                      && s.row == static_cast<float>(row)
                      && s.band == band_for_row(row, gh, k_temp);
        }
    check(stationary, "P6   a stagnant lid's ground sat where it sits, at every epoch");

    std::printf("\n%s (%d failure%s)\n", g_failures == 0 ? "ALL PASS" : "FAILURES",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
