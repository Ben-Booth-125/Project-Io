#pragma once

#include "entity.hpp"

#include <cstdint>
#include <iosfwd>
#include <unordered_map>
#include <vector>

// ---------------------------------------------------------------------------
// Province partition (BL-466) — the 3-5 tile locality cell, made real
// ---------------------------------------------------------------------------
// A deterministic, seeded partition of every body's LAND tiles into small,
// contiguous, purely spatial cells. Ocean is excluded outright: water is a
// movement mode, not a locality, and a sea battle is not this object's job.
//
// The province is deliberately NOT the region: no name, no owner, no culture,
// no economy. It exists because BL-467 (battle state) needs an engagement
// envelope with a stable identity, and folds the province id into the battle's
// seed stream.
//
// THE ID ORDER IS THE CONTRACT. Downstream code walks provinces in ascending
// `province::id` and gets an order that does not depend on container internals,
// tile-map iteration order, or the order bodies were created in. See the id
// layout below — sorted id order is body-major, then block raster order, then
// component index, which is also a sensible spatial walk.
//
// THE PARTITION IS PART OF WORLD GENERATION AND VERSIONS WITH IT. It is never
// patched in place: a change to the algorithm re-rolls every battle in every
// world. Authority: docs/GLOSSARY.md (the spatial vocabulary) and BL-466.
// ---------------------------------------------------------------------------

struct world;

/// One province — a contiguous run of land tiles on a single body.
struct province
{
    /// Stable derived identity. Layout, high bits first:
    ///   bits 31..20 (12) — the body's RANK in ascending entity-id order over
    ///                      `world::bodies`. Makes the id globally unique while
    ///                      keeping ascending-id iteration body-major.
    ///   bits 19..3  (17) — the base 2x2 block's raster index within the body
    ///                      (block_row * block_cols + block_col).
    ///   bits  2..0  (3)  — the connected-component index within that block,
    ///                      assigned smallest-member-tile-id first (a 2x2 block
    ///                      bisected by ocean splits; a block yields at most 4).
    /// Ids are DERIVED, never allocated, so they are stable under recompute and
    /// carry gaps wherever a block held no land or a fragment was merged away.
    uint32_t id = 0;

    /// The body every tile in this province sits on. A province never spans
    /// bodies, and never spans water.
    entity_id body = null_entity;

    /// Member land tiles, ASCENDING entity id. Non-empty for every province in
    /// a built partition.
    std::vector<entity_id> tiles;
};

/// The whole world's partition — every body's provinces in one ascending-id run.
struct province_partition
{
    /// The seed `build_province_partition` was called with. Stored so the
    /// partition can be recomputed and checked against itself (the replay
    /// guard) without the caller having to remember the world descriptor.
    uint32_t seed = 0;

    /// Every province, ASCENDING `province::id`. This is the iteration contract.
    std::vector<province> provinces;

    /// Land tile -> owning province id. Derived from `provinces` (rebuilt on
    /// read, never serialised separately) — every land tile appears exactly once.
    std::unordered_map<entity_id, uint32_t> tile_province;

    /// Owning province id for @p tile, or 0 when the tile is ocean, off-body, or
    /// otherwise unpartitioned. Province id 0 is unreachable as a real id (block
    /// 0, component 0, body rank 0 would require a land tile there AND is still a
    /// valid id) — so callers wanting certainty should check `tile_province`
    /// directly. This accessor is the convenience path.
    uint32_t province_of(entity_id tile) const
    {
        const auto it = tile_province.find(tile);
        return (it != tile_province.end()) ? it->second : 0u;
    }

    /// The province with id @p id, or nullptr. Binary search over the ascending
    /// `provinces` vector.
    const province* find(uint32_t id) const;
};

/// Build @p w's province partition from its tiles, REPLACING `w.provinces`.
///
/// Pure in everything but its inputs: the result is a function of (@p seed, each
/// body's grid dimensions, its land mask, and its tiles' composition / landform /
/// river edges) alone. NO RNG STREAM IS CONSUMED — every draw is a stateless fold
/// from @p seed, the campaign_battle identity idiom, so the partition can never
/// perturb another generation pass's draws no matter where it is called.
///
/// Three passes (BL-466's settled algorithm):
///   1. Base blocks. A per-body origin offset (dx, dy in {0,1}) folded from
///      (seed, body), then 2x2 offset-coordinate blocks over the land tiles.
///      An interior block is 4 tiles — inside the 3-5 band. Each block is then
///      split into hex-connected components, so no province spans a strait.
///   2. Terrain-seam jitter. One sweep over boundary tiles in ascending tile id,
///      moving a tile across a border when it sits across a seam (a river edge
///      or a composition/landform change) from its OWN province and shares an
///      unseamed edge with the neighbouring one. Borders drift onto rivers and
///      terrain edges, so a battle envelope's boundary means something. Clamped:
///      both provinces stay 3-5, the mover may not be a cut vertex of its
///      source, and each province is resized at most once per sweep.
///   3. Coastal repair. A one-tile province merges into its lowest-id
///      hex-adjacent province (Ben's ruling); a true islet with no neighbour
///      stands alone. Post-merge coastal sizes may exceed 5 — the coast bends
///      the band, and that is recorded rather than hidden.
///
/// Columns wrap east-west (the cylinder every other body-grid consumer uses);
/// rows do not.
///
/// @param w    World whose tiles are read and whose `provinces` is replaced.
/// @param seed Partition seed. Callers derive this from the world seed with
///             their own XOR offset so the stream stays uncorrelated.
void build_province_partition(world& w, uint32_t seed);

// ---------------------------------------------------------------------------
// Serialisation — the trailing section of the world's flat-binary stream
// ---------------------------------------------------------------------------
// The history log (history_log.{hpp,cpp}) is the project's flat-binary seam.
// The province section is APPENDED AFTER its last record and nowhere else:
// no existing record moves, so a stream written before BL-466 is a byte-exact
// PREFIX of one written after it, and reads back cleanly (the reader treats a
// clean end-of-stream as "no province section" rather than as corruption).
// tools/verify/province_partition_harness.cpp asserts all three properties.
// ---------------------------------------------------------------------------

/// Leading magic identifying a province section: the bytes 'I','O','P','V'.
inline constexpr uint32_t province_section_magic =
    (uint32_t('I')) | (uint32_t('O') << 8) | (uint32_t('P') << 16) | (uint32_t('V') << 24);

/// Format version. Bump on any layout change to the section; the reader rejects
/// a mismatch rather than reinterpreting differently-shaped records.
inline constexpr uint32_t province_section_version = 1;

/// Sanity ceiling on the declared province count — guards an eager reserve
/// against a corrupt or malicious length prefix.
inline constexpr uint32_t province_section_max_provinces = 1u << 24;

/// Sanity ceiling on one province's declared tile count. Real provinces are
/// 1-5 tiles (coastal merges aside); this is orders of magnitude clear.
inline constexpr uint32_t province_section_max_tiles = 1u << 16;

/// Append @p p to @p out as: magic, version, seed, province count, then per
/// province (id, body, tile count, tiles). Binary, native byte order — the
/// project's settled flat-binary convention. `tile_province` is NOT written;
/// it is derived, and the reader rebuilds it.
///
/// @param p   Partition to write.
/// @param out Binary output stream, positioned at the append point.
void write_province_section(const province_partition& p, std::ostream& out);

/// Read a section written by write_province_section into @p out.
///
/// A CLEAN END OF STREAM IS SUCCESS: a stream written before this item simply
/// ends after its last history record, and must still load. In that case @p out
/// is left empty and the function returns true. Anything else that is not a
/// well-formed section — a wrong magic, a mismatched version, a truncated
/// record, a count past the ceilings above, ids out of ascending order — is a
/// rejection (returns false), leaving @p out unspecified for the caller to
/// discard.
///
/// @param out Partition filled on success (`tile_province` rebuilt from the
///            written provinces).
/// @param in  Binary input stream, positioned at the section start.
/// @return    True on a well-formed section OR a clean absent one; false on a
///            malformed section.
bool read_province_section(province_partition& out, std::istream& in);
