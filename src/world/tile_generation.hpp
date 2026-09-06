#pragma once

#include "world.hpp"

#include <array>
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

/// The Continents/Drift result (src/world/continents.hpp). FORWARD-DECLARED
/// rather than included: continents.hpp includes planetology.hpp, which includes
/// THIS header, so including it back would close a cycle. Pass 6's Life phase
/// only ever holds a pointer to it.
struct continent_state;

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

// ---------------------------------------------------------------------------
// Latitude bands (Pass 3) — PROMOTED OUT OF tile_generation.cpp BY BL-764
// ---------------------------------------------------------------------------
//
// This enum and the two functions below lived in tile_generation.cpp's anonymous
// namespace, which was the whole reason a tile's latitude was UNASKABLE from
// anywhere else: `band_for_row(row, gh, temp)` is the only definition of what a
// latitude means in this generator, and it was reachable from exactly one
// translation unit. BL-764 needs it from continents.cpp, where a tile is asked
// what its climate WAS.
//
// The promotion is linkage only. The values, the boundaries and the arithmetic
// are untouched, so Pass 3 assigns exactly the bands it assigned before.

/// The five climate belts Pass 3 assigns, widest-to-narrowest by temperature class.
enum class lat_band : uint8_t { polar, subpolar, temperate, subtropical, tropical };

/// Latitude band for a distance from the equator.
///
/// @param d    |distance from the equator| in [0, 1] — 0 at the equator, 1 at a
///             pole. Values outside that range are the caller's to fold first
///             (see `paleo_tile_at`, which folds a past row back over the pole).
/// @param temp The body's thermal class, which shifts every boundary.
///
/// SPLIT OUT OF band_for_row RATHER THAN DUPLICATED. The boundary table is the
/// generator's definition of a climate belt; a second copy of it for paleo use
/// is a copy that drifts, which is the defect this split exists to prevent.
lat_band band_for_distance(double d, temperature_class temp);

/// Latitude band for a grid row. `band_for_distance` with d derived from the row.
lat_band band_for_row(int row, int gh, temperature_class temp);

/// Solar-level constants describing what kind of world a body is. Derived per
/// body by the Planetology pass (run_planetology's returned state carries the
/// profile); the generation passes read these and branch on nothing else. A
/// future layer could derive them from orbital mechanics (see
/// docs/generation/TILE_GENERATION.md § Deferred).
struct body_profile
{
    temperature_class   temperature    = temperature_class::temperate;
    atmosphere_class    atmosphere     = atmosphere_class::moderate;
    hydrological_state  hydrology      = hydrological_state::none;
    geological_activity geology        = geological_activity::none;
    float               water_fraction = 0.0f; ///< Target ocean coverage; used only when hydrology == liquid.
    composition_bias    bias           = composition_bias::standard;
};

/// Forward declaration — the Planetology pass (BL-167) runs BEFORE this pipeline
/// and hands it the body's derived history. Declared rather than included because
/// planetology.hpp includes THIS header for body_profile; the definition is pulled
/// in by tile_generation.cpp.
struct planetology_state;

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

    /// [row*gw+col] the value Pass 2 actually COMPARED against `ocean_threshold` —
    /// the heightmap after the equatorial down-bias. Captured rather than left to the
    /// reader to recompute: `height` alone cannot answer "did this tile clear the
    /// threshold", so a breadcrumb built from it would have to re-derive the bias
    /// constant, and a second copy of that constant is a copy that drifts. Empty on a
    /// body that runs no ocean pass (hydrology != liquid), where the question has no
    /// meaning. Pure capture — Pass 2's own arithmetic is untouched.
    std::vector<float>   ocean_score;

    float ocean_threshold = 0.0f;  ///< Latitude-biased height percentile used for ocean (Pass 2).
    int   ocean_tiles     = 0;     ///< Tiles assigned the ocean composition (Pass 2).

    // -----------------------------------------------------------------------
    // BL-762 — WHICH PHASE PLACED WHAT
    // -----------------------------------------------------------------------
    //
    // Pass 6 draws every deposit in one traversal, but it no longer writes them
    // into one array: `generate_deposits` dispatches each `put` on
    // `resource_origin_of`, so the BODY phase's output (ores, aggregates, ice)
    // and the LIFE phase's output (coal, petroleum, peat, timber, crops, hides)
    // are separate destinations decided by the origin table rather than by which
    // line happens to write them.
    //
    // These two arrays are that split, summed over the body's tiles, and they
    // exist so the claim is CHECKABLE rather than merely asserted in a comment:
    // every biological resource must be zero in `body_phase_placed`, and every
    // geological one zero in `life_phase_placed`. See tools/verify/deposit_origin.
    //
    // RAW MAGNITUDES, before the three post-multiplies (abundance scalar,
    // planetology endowment, ore fields). What a phase PLACED is a question about
    // the draw, not about what survived the endowment — a body whose biosphere
    // never reached land still had the life phase run and write zero-endowment
    // coal, and reading these arrays post-endowment would hide that.
    //
    // Endemic trade goods (the C -> D addition) are counted in
    // `life_phase_placed` — biosphere output by construction, and the endemic
    // set is empty on any world that never reached a land biosphere.
    //
    // AT THE RAW MAGNITUDE, LIKE EVERYTHING ELSE HERE. The first cut recorded
    // the endemic contribution at `amount * deposit_scalar` — the value written
    // into the tile — while the two accumulations above record the pre-multiply
    // draw. On `abundance_level::sparse` or `lean`, where `deposit_scalar != 1`,
    // that made `life_phase_placed[coal]` a sum of two different units and the
    // total meaningless. Both halves are raw placement now, which is what makes
    // this array comparable to itself across abundance levels.
    std::array<double, resource_count> body_phase_placed{};
    std::array<double, resource_count> life_phase_placed{};
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
/// @param deposit_scalar Resource-abundance multiplier on generated deposits (BL-114).
///                1.0 = the earth-like ceiling; leaner worlds pass a value below 1
///                (GENERATION_STRATEGY.md § The resource ceiling). Applied as a pure
///                post-multiply on the deposit array — it consumes no RNG, so a scalar
///                of 1.0 reproduces the unscaled surface bit-for-bit.
/// @param pl      Optional Planetology result for this body (BL-167). When non-null it
///                does two things: its per-resource `endowment` multiplies the generated
///                deposits (a pure post-multiply alongside @p deposit_scalar, drawing no
///                RNG), and a `life_stage` below `land` masks the biotic compositions out
///                of Pass 4 — so a world whose biosphere never left the water LOOKS dead
///                on the canvas rather than merely reading poorer in a ledger.
///                Passing nullptr reproduces the pre-BL-167 surface bit-for-bit.
/// @param record  Optional out-param; when non-null, receives the per-pass intermediates.
/// @param continent_bias Optional per-tile height contribution from the Continents/Drift
///                sibling pass (BL-210; src/world/continents.hpp), sized gw*gh. Added into
///                Pass 1's heightmap before normalisation, so plate-boundary uplift/rift
///                shapes the same terrain a pure-noise heightmap would otherwise produce.
///                A null pointer reproduces the pre-BL-210 surface bit-for-bit.
/// @param convergent Optional per-tile mask (continent_state::convergent), sized gw*gh,
///                1 where the tile sits on a classified CONVERGENT plate boundary. Pass 5
///                seeds mountain clusters here first, so ranges follow the collision that
///                raised them instead of pooling on whatever ground is already high. A
///                null pointer falls back to the height/composition rule and reproduces
///                the earlier surface bit-for-bit.
/// @param continents Optional full Continents/Drift result (BL-765). The LIFE phase of
///                Pass 6 asks each tile where it SAT when its fossils formed, through
///                `paleo_tile_at` — coal at the land-burial epoch, petroleum at the
///                marine-anoxic one. A null pointer (or a stagnant lid, or a body with
///                no plates) leaves the ground stationary at every epoch, so the palaeo
///                answer collapses to the present and the fossil rules read today's
///                climate. That is a correct fallback, not a degraded one: a world with
///                no drift history has no palaeo-geography to read.
/// @return        Tile entity IDs in raster order (index = row * gw + col).
std::vector<entity_id> generate_body_tiles(
    world& w,
    entity_id body_id,
    int gw, int gh,
    const body_profile& profile,
    uint32_t seed,
    float deposit_scalar = 1.0f,
    const planetology_state* pl = nullptr,
    generation_record* record = nullptr,
    const std::vector<float>* continent_bias = nullptr,
    const std::vector<uint8_t>* convergent = nullptr,
    const continent_state* continents = nullptr);

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
