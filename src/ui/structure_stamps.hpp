#pragma once

#include "world/entity.hpp"

#include <cstdint>
#include <vector>

// ---------------------------------------------------------------------------
// Structure stamps (BL-1241, structures baked) — the installation pass of the
// ground bake and the STAMP SEAM it draws through. docs/ui/RENDERING.md
// § Installations is the authority.
//
// What stands on a tile is baked into the ground as painterly structures, not
// drawn over it as a glyph: one structure per STACK (placement_rules::
// stack_members — (type, target)), up to three, the dominant (lowest-id) stack
// largest and in front; a population centre as a settlement whose footprint and
// height step with its scale; a razed centre as a ruin; a site under
// construction as scaffolding.
//
// THE SEAM. A stamp is keyed by WHAT IT DEPICTS (`stamp_key`): a building's type
// plus its family (extraction target family / the active recipe's group), a
// settlement's scale, a ruin, scaffolding. The bake asks the seam for a key
// (`bake_params::stamps`, a `stamp_sheet`): an authored raster registered for
// the key is blitted; every key without one draws its PROCEDURAL form. Today no
// sheet ships, so every key is procedural — and nothing procedural is thrown
// away when art arrives, because the art replaces a key, not the pass.
//
// The pass is static art (no owner colour, no running-state cue): the snapshot
// below carries nothing tick-rate, so the chunk hash it feeds moves on a
// construction event, never on a tick. Pure and SDL/ImGui/Lua-free like the
// rest of the bake, so tools/verify/ground_bake_check can exercise it.
// ---------------------------------------------------------------------------

struct world;
class recipe_registry;

namespace ui::ground {

struct geometry;
struct bake_params;
struct bake_source;

/// What a stamp depicts — the seam's first key field.
enum class stamp_subject : std::uint8_t
{
    none       = 0,
    building   = 1, ///< A standing installation: type + family.
    settlement = 2, ///< A population centre: scale.
    ruin       = 3, ///< A razed population centre.
    scaffold   = 4, ///< A site under construction (type + family kept: the footprint it will become).
};

/// An extraction site's form family, keyed on its target resource — the
/// mine head-frame, the quarry pit, the well housing… (the retired glyph's
/// per-target split, regrouped by what the works LOOK like).
enum class extraction_family : std::uint8_t
{
    mine = 0,   ///< Ores and coal: head-frame, winding house, spoil heap.
    quarry,     ///< Stone, sand, clay, silica, regolith: a terraced open pit.
    oil_well,   ///< Petroleum: a derrick and tank farm.
    water_well, ///< Water: a well housing and a water tower.
    timber,     ///< Timber: log stacks and a saw shed.
    farm,       ///< Produce, fibre: fields, barn, silo.
    plantation, ///< Tobacco, spices, coffee: bush rows and a drying shed.
    peat,       ///< Peat: cut trenches and stacked turves.
    trapping,   ///< Hides, furs: a lodge and drying racks.
    count
};

/// A processing facility's form family — its active recipe's `group`
/// (recipes.lua, BL-434), so *what is made here* reads before a hover.
enum class processing_family : std::uint8_t
{
    general = 0,
    metal_foundry,
    fuel_production,
    construction_materials,
    construction,
    artisan_goods,
    food_processing,
    chemical_works,
    refinery,
    power_generation,
    electronics,
    advanced_fabrication,
    welfare_goods,
    count
};

/// THE SEAM'S KEY: what one structure stamp depicts. Two installations with the
/// same key draw the same form (hash-varied in detail by their grid position).
struct stamp_key
{
    stamp_subject subject = stamp_subject::none;
    std::uint8_t  type    = 0; ///< building_type (building / scaffold).
    std::uint8_t  family  = 0; ///< extraction_family or processing_family by type; 0 otherwise.
    std::uint8_t  scale   = 0; ///< Settlement scale 1-5; 0 otherwise.

    std::uint32_t packed() const
    {
        return static_cast<std::uint32_t>(subject)
             | (static_cast<std::uint32_t>(type)   << 8)
             | (static_cast<std::uint32_t>(family) << 16)
             | (static_cast<std::uint32_t>(scale)  << 24);
    }
    bool operator==(const stamp_key& o) const { return packed() == o.packed(); }
};

/// What one tile stands, snapshotted at source-prepare time. Up to three stack
/// structures in DOMINANCE order (index 0 = the stack holding the tile's
/// lowest-id building) plus at most one settlement / ruin.
struct tile_installation
{
    static constexpr int k_max_stacks = 3;
    stamp_key    stacks[k_max_stacks];
    std::uint8_t n_stacks = 0;
    stamp_key    settlement; ///< subject none / settlement / ruin.
};

/// The installation half of the bake source: sparse, raster-indexed.
struct installation_snapshot
{
    std::vector<std::int32_t>      of_tile; ///< Per raster tile: index into `list`, or -1.
    std::vector<tile_installation> list;
};

// --- The authored-raster side of the seam ---------------------------------

/// One authored stamp: an RGBA8 (ABGR u32) sprite, anchored at its ground
/// contact point, sized in canonical units (hex circumradius = 1). Drawn
/// before the grade like the procedural forms; its alpha is coverage.
struct authored_stamp
{
    stamp_key                  key;
    int                        w = 0, h = 0;
    std::vector<std::uint32_t> px;
    float anchor_x = 0.5f, anchor_y = 1.0f; ///< Ground contact, fraction of w/h.
    float width_canonical = 0.6f;           ///< Drawn width at slot size 1.
};

/// A set of authored stamps, immutable once handed to the bake (it travels by
/// pointer in bake_params to the worker). Empty or absent = all procedural.
struct stamp_sheet
{
    std::vector<authored_stamp> stamps;
    const authored_stamp* find(const stamp_key& k) const
    {
        for (const authored_stamp& s : stamps)
            if (s.key == k)
                return &s;
        return nullptr;
    }
};

// --- The pass --------------------------------------------------------------

/// Snapshot the installations standing on @p body's REVEALED tiles into
/// @p s.inst (call after the tile pass has set s.cls: a masked tile records
/// nothing, so no structure leaks through the survey mask). Reads buildings
/// (type, target, recipe group via @p reg, under-construction), and population
/// centres (scale, razed) — nothing tick-rate. @p reg may be null: processing
/// facilities then draw the general form.
void extract_installations(const world& w, entity_id body, const recipe_registry* reg,
                           bake_source& s);

/// Stamp every structure that can reach the window into @p out — pads, SE
/// shadows, then the structures back-to-front, NW-lit, before the grade.
/// Honours @p tag exactly as the tree stamps do (never paints lock fill or the
/// transparent margin). Deterministic and wrap-exact: hashes key on WRAPPED
/// grid coordinates, positions on unwrapped centres.
void stamp_installations(const bake_source& src, const geometry& g, const bake_params& p,
                         int px0, int py0, int pw, int ph, std::uint32_t* out,
                         const std::uint8_t* tag);

/// The installation fields the pass reads for every tile whose structures can
/// reach the window, folded into one value for region_hash.
std::uint64_t installation_hash(const bake_source& src, const geometry& g,
                                int px0, int py0, int pw, int ph);

/// How far one tile's structures reach from its centre, canonical units (BL-1246,
/// the partial re-bake): pads, the SE shadow, the east lean and the standing
/// height at @p g's angle. The window walk of the pass and of the hash is this
/// extent inverted, so a pixel outside it cannot be touched by the tile.
struct structure_extent { double left = 0, right = 0, up = 0, down = 0; };
structure_extent installation_extent(const geometry& g);

/// A grid cell, column UNWRAPPED (it may lie past either edge of the grid; the
/// tile is column mod gw) so its centre sits beside the window it was found for.
struct grid_cell { int c = 0, r = 0; };

/// BL-1246, the partial re-bake: append to @p out every tile that can reach the
/// window (the pass's own walk) whose installation differs between @p a and
/// @p b. Returns the count appended, or -1 when the sources' grids differ.
int changed_installation_tiles(const bake_source& a, const bake_source& b, const geometry& g,
                               int px0, int py0, int pw, int ph, std::vector<grid_cell>& out);

/// BL-1246, the partial re-bake: the pixel box [x0, x1) x [y0, y1) (absolute
/// bake pixels, x beside unwrapped column @p c) of every pixel the pass can
/// touch for tile (@p c, @p r) — its parts rasterised exactly as the pass
/// draws them, in every mode, writing nothing. False when the tile stands
/// nothing that draws. Never smaller than what the pass paints for it.
bool installation_tile_bounds(const bake_source& src, const geometry& g, const bake_params& p,
                              int c, int r, int& x0, int& y0, int& x1, int& y1);

/// Radius (canonical units, from the tile centre) inside which the tree stamp
/// leaves raster tile @p i clear — the ground an installation stands on.
float installation_clear_radius(const bake_source& src, std::size_t i);

/// The depicted-subject name of a key (verify / log use).
const char* stamp_name(const stamp_key& k);

} // namespace ui::ground
