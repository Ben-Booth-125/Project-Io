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
// Design authority: docs/generation/GENERATION_STRATEGY.md § The oral-history
// pivot; full architecture in backlog.json BL-210.
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
