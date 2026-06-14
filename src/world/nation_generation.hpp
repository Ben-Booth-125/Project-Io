#pragma once

#include "world.hpp"

#include <cstdint>
#include <vector>

// ---------------------------------------------------------------------------
// Procedural nation generation
//
// Deterministic, five-pass political-layer generation for a single body.
// Runs over an already-generated tile map; reads tile compositions from `w`
// and writes nation_component entries plus tile ownership back into `w`.
// See docs/generation/NATION_GENERATION.md for the design authority and
// per-pass rules.
// ---------------------------------------------------------------------------

/// Tunable parameters for a single nation generation run.
struct nation_params
{
    /// Number of nations to place on the body. Clamped internally to the number
    /// of available land tiles, so it is safe to set higher than the land area.
    int nation_count = 10;

    /// Minimum grid-distance (Chebyshev, column-wrapped) between any two nation
    /// seeds placed in Pass 1. Raise to spread nations further apart.
    int min_seed_separation = 6;
};

/// Generate nations over the tile map of one body and register all results in @p w.
///
/// Runs the five-pass nation generation pipeline (seed placement, territory
/// expansion, resource profiling, political character, naming) described in
/// docs/generation/NATION_GENERATION.md. The grid wraps horizontally (columns)
/// and does not wrap vertically (rows), matching the tile layout produced by
/// generate_body_tiles.
///
/// Deterministic in @p seed: the same seed, tile map, and params always produce
/// the same political map. Does not use std::random_device or global state.
///
/// @param w        World holding the tile components; receives the new nation
///                 entities and tile_to_nation ownership entries.
/// @param body_id  Body whose tiles are being given political ownership.
/// @param tile_ids Raster-order tile IDs from generate_body_tiles (index = row * gw + col).
/// @param gw       Grid width (columns).
/// @param gh       Grid height (rows).
/// @param params   Tunable generation parameters.
/// @param seed     Per-run RNG seed for independent, reproducible results.
/// @return         Nation entity IDs in seed-placement order (one per nation created).
std::vector<entity_id> generate_nations(
    world& w,
    entity_id body_id,
    const std::vector<entity_id>& tile_ids,
    int gw, int gh,
    const nation_params& params,
    uint32_t seed);
