#pragma once

#include "planetology.hpp"

#include <cstdint>
#include <vector>

// ---------------------------------------------------------------------------
// Continents/Drift — the first slice of BL-210 (the oral-history pivot)
//
// A deterministic sibling pass that runs AFTER Planetology's S3 Engine (which
// already decides mobile_lid and the tectonic energy budget) and BEFORE the
// six-pass tile pipeline. It answers "where did the land end up, and why?" by
// simulating a small number of drifting plates rather than reading noise.
//
// Consequence, not simulation-as-dice (Ben, 2026-07-28): plate count, drift
// direction, and speed are all DERIVED from Planetology's already-computed
// theta/mobile_lid — nothing here is an independent random branch. The output
// is a coarse per-tile height bias (added into Pass 1's heightmap, same shape
// as an ordinary sibling-pass contribution) plus dated history_event lines
// appended to the body's biography, tagged chain_stage::engine — the textual
// half of "graphical (hexes) + textual (fully recorded)" that every
// oral-history stage must carry (Ben, 2026-07-28).
//
// Design authority: docs/generation/CONTINENTS.md; full architecture in
// backlog.json BL-210.
// ---------------------------------------------------------------------------

/// One drifting plate. Position and drift are on the tile grid's own axes so
/// the Voronoi assignment below needs no separate coordinate system.
struct tectonic_plate
{
    float seed_col   = 0.0f;
    float seed_row   = 0.0f;
    float drift_col  = 0.0f; ///< Per-epoch drift, grid columns (wraps).
    float drift_row  = 0.0f; ///< Per-epoch drift, grid rows.
    bool  oceanic     = false; ///< Oceanic plates bias the height field down.
};

/// What the continents pass computed for one body.
struct continent_state
{
    std::vector<tectonic_plate> plates;

    /// [row*gw+col] index into `plates` — which plate owns each tile, from the
    /// Voronoi assignment. Always sized gw*gh; all-zero on a stagnant-lid body
    /// (one plate owns the whole surface). Retained so the Continent lens can
    /// draw the plates the height bias was derived FROM, rather than inferring
    /// landmasses back out of the finished terrain — the boundary that raised a
    /// mountain range is the thing worth showing, and it is invisible once the
    /// bias has been folded into Pass 1's heightmap. See LENSES.md § Continent.
    std::vector<int> plate_id;

    /// [row*gw+col] height contribution, roughly [-1, 1]. Zero everywhere on a
    /// stagnant-lid body (one plate, no boundaries, no bias). Added into Pass
    /// 1's heightmap before normalisation — see generate_body_tiles.
    std::vector<float> height_bias;

    /// [row*gw+col], 1 where the tile touches a CLASSIFIED CONVERGENT boundary
    /// — the pairs that earned the +0.12 uplift above. Empty on a stagnant-lid
    /// body (no boundaries at all).
    ///
    /// Exposed as data because the classification used to exist only as prose:
    /// run_continents knew which boundaries collided, said so in a history line,
    /// folded 0.12 into the heightmap and then forgot. Pass 5 could not seed
    /// mountains where mountains actually form, so it fell back to "high and
    /// rocky", which puts ranges in blobs on existing high ground rather than in
    /// chains along the boundary that raised them. See TILE_GENERATION.md
    /// § Pass 5 and CONTINENTS.md § Outputs and contracts.
    std::vector<uint8_t> convergent;

    /// [row*gw+col], 1 where the tile touches a CLASSIFIED DIVERGENT boundary
    /// — the pairs that earned the -0.08 subsidence above. Empty on a
    /// stagnant-lid body (no boundaries at all), exactly like `convergent`, and
    /// written in the same loop from the same `sign` test.
    ///
    /// NOT the complement of `convergent`, and not disjoint from it. Most tiles
    /// are in neither mask. A tile is walked against TWO neighbours (right and
    /// down) and marked per neighbour, so one sitting where a closing pair meets
    /// an opening pair carries both marks — rare (a plate junction), real, and
    /// already true of `height_bias`, which accumulates +0.12 and -0.08 on that
    /// same tile. What IS exclusive is the per-boundary classification: one
    /// `sign` decides one pair, once.
    ///
    /// The sibling of `convergent`, and exposed for the same reason: the pass
    /// classified every boundary BOTH ways, said so in a history line, folded
    /// -0.08 into the heightmap and then recorded only half of what it knew.
    /// A rift is a legible read on a body — a rift valley, crust thinned and
    /// pulled apart, and the floor of the basin the sea eventually finds —
    /// and none of that survives in the finished heightmap, where -0.08 is
    /// indistinguishable from ground that was simply low to begin with. A
    /// consumer wanting rift landforms, thinned-crust deposits, or where the
    /// basins are has to be TOLD which tiles the rift ran through; it cannot
    /// recover them from height. See CONTINENTS.md § Outputs and contracts.
    std::vector<uint8_t> divergent;

    /// Dated lines for the body's biography (history_event::stage ==
    /// chain_stage::engine). Appended to planetology_state::history by the
    /// caller, not stored twice.
    std::vector<history_event> history;
};

/// Run the plate-drift pass for one body.
///
/// Pure deterministic function of (@p pl, @p gw, @p gh, @p seed) — no global
/// state, no std::-distribution entropy, no transcendentals (drift directions
/// are a fixed 8-way compass table, not sin/cos), per PLANETOLOGY.md's
/// portability rules.
///
/// @param pl   The body's already-run Planetology state (reads mobile_lid,
///             theta, mass — writes nothing back).
/// @param gw   Tile grid width (columns; wraps).
/// @param gh   Tile grid height (rows).
/// @param seed Per-body seed, already folded with the campaign seed (same
///             convention as run_planetology and generate_body_tiles).
continent_state run_continents(const planetology_state& pl, int gw, int gh, uint32_t seed);

// ===========================================================================
// BL-763 — the drift TIME AXIS
// ===========================================================================
//
// `tectonic_plate::drift_col`/`drift_row` were documented "per-epoch" and NO
// EPOCH LENGTH WAS DEFINED ANYWHERE. The drift vector existed and nothing
// integrated it, so the pass produced an endpoint and no history: "where was
// this ground at age T" was not a question the layer could answer. That is the
// first of the three gaps behind the Life phase (BL-765); the other two —
// ground that actually rides its plate, and a per-tile past climate — are
// BL-764's and are NOT addressed here.
//
// WHY A DERIVED SNAPSHOT RATHER THAN A STORED SEQUENCE. The item asked for
// `run_continents` to return an ordered sequence. It does not, deliberately:
// `continent_state` is on the SAVE ENVELOPE seam (src/core/save_game.cpp), and
// twenty `plate_id` rasters at 31,581 tiles each is ~2.5 MB of save per body
// for data that is a pure function of five floats per plate. So the sequence is
// DERIVABLE rather than stored — the epoch length and depth are defined
// constants, and `continent_snapshot_at` reconstructs any epoch on demand. The
// ordered sequence is `continent_snapshot_at(cs, e, ...)` for e in [0, depth],
// and it costs nothing until someone asks for it.

/// Years per drift epoch. One grid column on the home body is roughly 153 km
/// (a ~40,000 km circumference over 261 columns), and Earth-like plate motion
/// covers that in about 3-6 My — so 5 My per epoch puts `drift_col`'s clamped
/// 0.15-1.2 columns per epoch at a plausible rate rather than an arbitrary one.
/// It is a STATED constant, which is the whole point: the previous value was
/// undefined and therefore unfalsifiable.
inline constexpr int64_t continent_epoch_years = 5'000'000;

/// How far back the drift record is meaningful, in epochs. 20 epochs is 100 My
/// — deep enough to reach a carboniferous-analogue coal window, shallow enough
/// that extrapolating a single linear drift vector is not a fiction. Past this
/// the plates' straight-line motion stops being a defensible reconstruction.
inline constexpr int continent_drift_epochs = 20;

/// The plate configuration at one past epoch. Derived, never stored.
struct continent_snapshot
{
    /// How far back this snapshot sits. 0 is the present.
    int     epochs_back         = 0;
    int64_t years_before_present = 0;

    /// The plates with their seeds wound BACK along their drift vectors. Every
    /// other field (drift, oceanic) is carried through unchanged — drift is a
    /// constant of the plate, not a per-epoch state.
    std::vector<tectonic_plate> plates;

    /// [row*gw+col] index into `plates`, the Voronoi assignment at that epoch.
    /// At `epochs_back == 0` this is bit-identical to `continent_state::plate_id`
    /// by construction — same seeds, same comparison, same tie-break.
    std::vector<int> plate_id;
};

/// The plate configuration @p epochs_back drift epochs before the present.
///
/// PURE, and it consumes NO RANDOMNESS. That is what makes it safe to add: the
/// plate stream in `run_continents` draws position, direction and oceanic flag
/// for every plate from one shared `rng` in a fixed order, so a single new draw
/// anywhere in that loop would shift every subsequent plate and change the
/// world. This function touches none of it — it winds existing seeds back and
/// re-runs the Voronoi.
///
/// It also does NOT re-run the boundary classification or the rift-basin
/// search. The basin search is the expensive half of the pass (O(total) per
/// candidate pair with a 25-sample inlandness probe per corridor tile) and it
/// describes the PRESENT surface; re-running it per epoch would cost more than
/// the snapshot and mean less.
///
/// A stagnant-lid body (one plate) returns its single plate and an all-zero
/// assignment at every epoch, which is correct: nothing drifted.
continent_snapshot continent_snapshot_at(const continent_state& cs, int epochs_back,
                                        int gw, int gh);
