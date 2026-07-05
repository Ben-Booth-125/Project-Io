#pragma once

#include "world.hpp"

#include <cstdint>
#include <vector>

// ---------------------------------------------------------------------------
// Procedural tile generation
//
// Deterministic, six-pass surface generation for a single body. Body character
// comes entirely from the solar parameters fed in (a body_profile), not from
// body-specific code paths. See docs/generation/TILE_GENERATION.md for the
// design authority and per-pass rules.
// ---------------------------------------------------------------------------

/// Broad thermal class of a body. Shifts the latitude band widths in Pass 3:
/// a scorching body has no polar band; a frozen body is all polar.
enum class temperature_class : uint8_t { scorching, hot, temperate, cold, frozen };

/// Atmospheric density class. `none` or `thin` gates out organic compositions
/// (grassland, forest, wetland) and routes a body to the airless tables.
enum class atmosphere_class : uint8_t { none, thin, moderate, thick };

/// Surface water state. `liquid` runs the ocean pass; `polar_frozen` skips it and
/// ices the polar rows; `none` produces no water at all.
enum class hydrological_state : uint8_t { none, polar_frozen, liquid };

/// Geological activity level. Scales volcanic composition probability and the
/// number of mountain-range and rift-zone cluster seeds.
enum class geological_activity : uint8_t { none, low, moderate, high };

/// Override for bodies whose surface composition is dominated by a single type.
enum class composition_bias : uint8_t { standard, metallic };

/// Solar-level constants describing what kind of world a body is. Authored by
/// hand per body in make_hard_coded_world(); the generation passes read these and
/// branch on nothing else. A future layer could derive them from orbital
/// mechanics (see docs/generation/TILE_GENERATION.md § Deferred).
struct body_profile
{
    temperature_class   temperature    = temperature_class::temperate;
    atmosphere_class    atmosphere     = atmosphere_class::moderate;
    hydrological_state  hydrology      = hydrological_state::none;
    geological_activity geology        = geological_activity::none;
    float               water_fraction = 0.0f; ///< Target ocean coverage; used only when hydrology == liquid.
    composition_bias    bias           = composition_bias::standard;
};

/// Optional capture of a body's per-pass intermediate state, for analysis.
///
/// Generation is deterministic, so this is reproducible on demand rather than
/// authoritative; the struct exists as the seam a future generation Ledger will
/// read to explain *why* a tile turned out as it did (see docs/generation/GENERATION_LEDGER.md
/// — "Generation Ledger"). It is filled only when a non-null record is passed to
/// generate_body_tiles, so the common path pays nothing.
struct generation_record
{
    int gw = 0; ///< Grid width the record was captured at.
    int gh = 0; ///< Grid height the record was captured at.

    std::vector<float>   height;   ///< [row*gw+col] normalised heightmap (Pass 1).
    std::vector<float>   moisture; ///< [row*gw+col] normalised moisture (Pass 3).
    std::vector<uint8_t> band;     ///< [row*gw+col] latitude band index (Pass 3).

    float ocean_threshold = 0.0f;  ///< Latitude-biased height percentile used for ocean (Pass 2).
    int   ocean_tiles     = 0;     ///< Tiles assigned the ocean composition (Pass 2).
};

/// Generate the full hex tile grid for one body and attach the tiles to @p w.
///
/// Deterministic in @p seed: the same seed and profile always produce the same
/// surface across every run. Runs the six-pass pipeline (heightmap, ocean,
/// latitude/moisture, composition, landform clusters, deposits) described in
/// docs/generation/TILE_GENERATION.md.
///
/// @param w       World to create tile entities in.
/// @param body_id Body the tiles belong to.
/// @param gw      Grid width (columns); the surface wraps horizontally.
/// @param gh      Grid height (rows); pole-to-pole, with the polar caps truncated.
/// @param profile Solar parameters driving composition, ocean, and landforms.
/// @param seed    Per-body RNG seed for independent, reproducible results.
/// @param record  Optional out-param; when non-null, receives the per-pass intermediates.
/// @return        Tile entity IDs in raster order (index = row * gw + col).
std::vector<entity_id> generate_body_tiles(
    world& w,
    entity_id body_id,
    int gw, int gh,
    const body_profile& profile,
    uint32_t seed,
    generation_record* record = nullptr);

/// Scan raster order and return the first @p n land (non-ocean) tile IDs. Used to
/// pick building attachment points after a body's tiles are generated.
///
/// @param tile_ids Raster-order tile IDs from generate_body_tiles.
/// @param w        World holding the tile components.
/// @param gw       Grid width.
/// @param gh       Grid height.
/// @param n        Maximum number of tiles to return.
/// @return         Up to @p n non-ocean tile IDs in raster order.
std::vector<entity_id> first_land_tiles(const std::vector<entity_id>& tile_ids,
                                        const world& w, int gw, int gh, int n);
