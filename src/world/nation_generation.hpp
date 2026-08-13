#pragma once

#include "tongue.hpp"
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
///
/// There is deliberately **no nation-count knob**: the number of nations is a
/// *consequence* of the body's geography, not a target. Seeds scale with the
/// habitable land area (`land_tiles_per_seed`) and the merge pass absorbs every
/// nation that fails to clear a minimum viable territory (`min_nation_tiles`).
/// A bigger, drier, or less fragmented homeworld therefore ends up with a
/// different political map than a small, ocean-heavy one.
struct nation_params
{
    /// **Seed density** — one Pass-1 seed per this many habitable (non-ocean)
    /// tiles on the body. The pre-merge seed count is `land_tiles / this`
    /// (at least one on any body with land), so the seed budget grows and
    /// shrinks with the landmass. Lower = more, tighter-packed seeds. A seed is
    /// only a *candidate* core: most are absorbed by the merge pass below, so
    /// this is deliberately far denser than the expected surviving count.
    int land_tiles_per_seed = 80;

    /// Minimum grid-distance (Chebyshev, column-wrapped) between any two nation
    /// seeds placed in Pass 1. Raise to spread nations further apart. Acts as a
    /// hard ceiling on how many seeds actually fit, whatever the density asks for.
    int min_seed_separation = 6;

    /// **Minimum viable territory**, in tiles. Pass 2c absorbs any nation holding
    /// fewer tiles than this into its largest adjacent neighbour — smallest first
    /// — and stops once every survivor clears the floor. The final nation count
    /// falls out of that, rather than being counted down to a target. 0 disables
    /// the merge pass entirely (every placed seed keeps its Voronoi cell).
    ///
    /// Set equal to `land_tiles_per_seed` by default, which reads as: a nation
    /// must end up holding at least the land its own seed was budgeted. The
    /// resulting count is ladder-derived: the history ladder (BL-221) modulates
    /// seeding, so the default Kepler seed lands well above the pre-ladder figure
    /// (43 nations at the current default seed). Halve the habitable area and
    /// the count still roughly halves with it.
    int min_nation_tiles = 80;

    /// **Explicit seed tiles** (raster indices, `row * gw + col`) — when this is
    /// non-empty Pass 1's random placement is SKIPPED and these are used verbatim,
    /// in order, as the nation cores.
    ///
    /// This is the whole of BL-218's "seeding changes, expansion does not": the
    /// settlement pass (`src/world/settlement.{hpp,cpp}`) places provinces on
    /// ground people would actually have settled and hands the anchors over here,
    /// so the BFS/growth machinery BL-053 tuned is reused untouched and only its
    /// starting points become historical. Empty (the default) preserves the
    /// pre-BL-218 random placement bit-for-bit for every body without a
    /// settlement pass.
    ///
    /// Entries outside the grid, on ocean, or duplicated are dropped by Pass 1;
    /// `min_seed_separation` is NOT re-applied to them (the settlement pass owns
    /// its own separation rule and re-filtering here would silently discard
    /// history).
    std::vector<int> seed_tiles;

    /// Parallel to `seed_tiles`: the tongue of the culture that settled each
    /// anchor (BL-290). Pass 5 names a nation in the speech of the province it
    /// grew from, so a realm, its gods and its cities share one sound system as
    /// a consequence of the generation chain rather than by coincidence. Short,
    /// empty, or unusable entries fall back to a tongue rolled in Pass 5.
    std::vector<tongue> seed_tongues;
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
/// @param progress Optional progress sink (BL-305). When non-null, Pass 2
///                 publishes each tile as it is claimed and Pass 2c republishes
///                 the merged map, so a loading screen can show the carve
///                 happening. A pure TAP: write-only, consumes no randomness,
///                 changes no branch — passing null (the default, and every
///                 headless caller) produces a byte-identical political map.
///                 Defined in world/hard_coded_world.hpp.
/// @return         Nation entity IDs in seed-placement order (one per nation created).
std::vector<entity_id> generate_nations(
    world& w,
    entity_id body_id,
    const std::vector<entity_id>& tile_ids,
    int gw, int gh,
    const nation_params& params,
    uint32_t seed,
    struct generation_progress* progress = nullptr);
