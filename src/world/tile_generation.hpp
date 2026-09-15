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
/// — "Generation Ledger"). It is handed back only when a non-null record is
/// passed to generate_body_tiles; the common path receives nothing. (Since
/// BL-965 the record is also the seam between the generator's two halves, so
/// one is always built internally; a null caller pays the seam's fill and
/// nothing more.)
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

    // -----------------------------------------------------------------------
    // BL-965 — THE BODY/LIFE SEAM
    // -----------------------------------------------------------------------
    //
    // `generate_body_tiles` is two callable halves: `generate_body_surface`
    // (Passes 1-5 and the BODY phase of Pass 6) and `generate_life_deposits_over`
    // (the LIFE phase of Pass 6 and the ore-field pre-pass), and this record is
    // what passes between them. The two arrays below are the only things the
    // Life half cannot read off the tiles it is handed or recompute from the
    // seed: the raw geological deposit per tile, and the endemic amounts the
    // Body half drew. Both are written by the Body half and READ ONLY by the
    // Life half, which is what lets the Life half run over the same record
    // more than once.
    //
    // WHY THE ENDEMIC DRAW SITS ON THIS SIDE. An endemic good is biosphere
    // output and is accounted in `life_phase_placed` — but its amount is drawn
    // on `tile_rng`, BETWEEN the body deposit block and the environment jitter,
    // and that stream cannot be re-cut without moving hazard and habitability
    // on every tile of every world. So the Body half draws it (the stream stays
    // exactly where it was) and the Life half places it (the accounting stays
    // with the placement). Only draws that clear the `> 1` gate are kept, in
    // raster order and then endemic order — the order the writes were made.

    /// One endemic amount the Body half drew and the Life half places.
    struct endemic_draw
    {
        int           idx;    ///< Raster index of the tile it was drawn for.
        resource_type good;   ///< The endemic good.
        float         amount; ///< Raw amount, before `deposit_scalar`.
    };

    /// [row*gw+col] the BODY phase's raw deposit per tile — `tile_deposits::geological`
    /// as drawn, before every post-multiply. Zero on water.
    std::vector<std::array<float, resource_count>> body_deposits;

    /// The endemic draws, raster order then endemic order (see above).
    std::vector<endemic_draw> endemic_draws;
};

/// The BODY half of `generate_body_tiles` (BL-965): Passes 1-5 (with 4b-4e) and
/// the Body phase of Pass 6. Creates the tile entities with their three axes,
/// water kind, height, hazard and habitability, and leaves both deposit arrays
/// value-initialised for the Life half to write. Fills @p record — the seam —
/// entirely, including `body_deposits` and `endemic_draws`.
///
/// Parameters as for `generate_body_tiles`, minus the three the Body half does
/// not consume (`deposit_scalar`, `continents`; the record is required here
/// rather than optional, since it IS the seam).
///
/// @return Tile entity IDs in raster order (index = row * gw + col).
std::vector<entity_id> generate_body_surface(
    world& w,
    entity_id body_id,
    int gw, int gh,
    const body_profile& profile,
    uint32_t seed,
    const planetology_state* pl,
    generation_record& record,
    const std::vector<float>* continent_bias = nullptr,
    const std::vector<uint8_t>* convergent = nullptr);

/// The LIFE half of `generate_body_tiles` (BL-965): the palaeo pre-pass, the
/// ore-field pre-pass and the Life phase of Pass 6, run over a tile set the Body
/// half built and the record it filled. Writes each tile's `resource_deposit`
/// and `resource_remaining` and the record's `life_phase_placed`, and mutates
/// nothing else — so it may be called AGAIN over the same tiles and record, and
/// the result is the same as a fresh `generate_body_tiles` with the same
/// arguments. That re-entry is the point: a Life-phase rule can be re-tuned
/// over a cached Body half without paying for the passes above it.
///
/// @param tile_ids The raster-order ids `generate_body_surface` returned.
/// @param record   The record `generate_body_surface` filled. Read for the
///                 seam; only `life_phase_placed` is written.
/// @param seed     The SAME seed the Body half was given — the Life phase's
///                 per-tile stream and the rarity profile are derived from it.
/// Remaining parameters as for `generate_body_tiles`. A record the Body half did
/// not fill (sizes disagree with `gw*gh`) writes nothing.
void generate_life_deposits_over(
    world& w,
    const std::vector<entity_id>& tile_ids,
    generation_record& record,
    const body_profile& profile,
    uint32_t seed,
    float deposit_scalar = 1.0f,
    const planetology_state* pl = nullptr,
    const std::vector<uint8_t>* convergent = nullptr,
    const continent_state* continents = nullptr);

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
