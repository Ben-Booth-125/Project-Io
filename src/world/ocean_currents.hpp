#pragma once

// ---------------------------------------------------------------------------
// Ocean currents — a field generated from the planet (BL-1120).
//
// docs/generation/EXPLORATION.md § Currents are a force, not a picture: "each
// ocean region's current follows its latitude band's prevailing wind (easterly
// in the tropics, westerly in the mid-latitudes, its sense set by the body's
// rotation) and is turned along the coasts the continents present, so a basin
// circulates; a leg run with its current costs less and against it more, by
// one weight measured on the curated seeds."
//
// THE MODEL IS A STREAM FUNCTION, because a stream function is the one
// construction that makes "a basin circulates" true by arithmetic rather than
// by tuning. Per sea tile,
//
//     psi = psi0(latitude) x min(d, R) / R
//
// where psi0 is the band profile of the prevailing wind and d the tile's
// distance to the nearest ground. The current is the curl of psi: east =
// d psi / d(south), north = d psi / d(east). Three consequences fall out of
// the construction, none of them a rule of their own:
//
//   * In open water far from any coast psi = psi0, so the current is the
//     band's wind: westward in the tropics, eastward in the mid-latitudes,
//     westward again under the polar easterlies, calm at the equator (the
//     doldrums), at 30 degrees (the horse latitudes) and at 60.
//   * psi is ZERO on every coast, so the coast is a streamline: water never
//     flows into the land, and the wind's push that would have is turned
//     along the shore instead.
//   * psi0 peaks at 30 degrees and falls to zero on the coasts either side of
//     a basin, so between two continents psi is a hill (a valley in the
//     southern hemisphere) and the water runs round it — poleward up the
//     basin's western shore, eastward across the mid-latitudes, equatorward
//     down its eastern shore and westward home along the trades. Clockwise in
//     the north, counter-clockwise in the south, for a prograde spin. The
//     discrete curl of a scalar field has zero divergence, so no current is
//     ever created or destroyed inside a basin: it can only circulate.
//
// THE BODY'S ROTATION SETS THE SENSE. The data model records no spin for a
// body (PLANETOLOGY.md generates none, and nothing downstream reads one), so
// the sense is an explicit input: +1 prograde, the ordinary case, and -1
// retrograde, which reverses every wind and so every current.
//
// THE GRAIN IS AN OCEAN REGION: a square cell of `ocean_current_cell_tiles`
// tiles on a side, whose current is the mean of its sea tiles'. Seven is the
// sea province spacing (`k_sea_province_seed_spacing`, PROVINCES.md § Three
// domains), so a cell is the size of the open-ocean province the campaign
// draws, but the lattice is fixed rather than read off the province
// partition, because the partition is built after the history spans that
// price legs with the field, and a lattice has no seeding order to depend on.
// The stream function itself is resolved per tile (the coast taper is a tile
// quantity); only the answer is read per region, which is what one arrow on
// a map is.
//
// PURE AND INTEGER. The field is a function of the substrate raster, the grid
// and the sense — no seed, no float, no container order — so every caller
// that asks of the same ground gets the same field on every machine.
// ---------------------------------------------------------------------------

#include "world/components.hpp"

#include <cstdint>
#include <vector>

/// The ocean region: a square lattice cell this many tiles on a side
/// (the sea province spacing — see the header comment).
inline constexpr int ocean_current_cell_tiles = 7;

/// The coastal boundary layer, in tiles: how far off a coast the current
/// takes to reach the band's open-water strength. The same seven, so a
/// basin's boundary current occupies the ocean region against its shore.
inline constexpr int ocean_current_coast_taper_tiles = 7;

/// The field over one body. Vectors are per mille of a full current: a
/// boundary current at the band's strongest reads 1000, the band's open
/// water peaks near half that.
struct ocean_current_field
{
    int gw = 0;
    int gh = 0;
    int cell = ocean_current_cell_tiles;
    int cells_w = 0; ///< ceil(gw / cell)
    int cells_h = 0; ///< ceil(gh / cell)
    int rotation_sense = 1;

    /// Per tile, raster order: 1 where the tile is sea (`is_sea`: ocean or
    /// coast — a lake is not part of the ocean and carries no current).
    std::vector<uint8_t> sea;

    /// Per ocean region, row-major (cy * cells_w + cx): the region's current,
    /// east-positive and north-positive, per mille. Zero where the region
    /// holds no sea.
    std::vector<int16_t> east_q;
    std::vector<int16_t> north_q;
    /// How many sea tiles the region holds (0 = a land region).
    std::vector<uint16_t> sea_tiles;

    bool empty() const { return cells_w <= 0 || cells_h <= 0; }
    /// The ocean region a tile falls in.
    int region_of(int col, int row) const
    {
        return (row / cell) * cells_w + (col / cell);
    }
};

/// Build the field for one body from its substrate raster (`gw * gh`, raster
/// order), with the body's rotation sense (+1 prograde, -1 retrograde). An
/// empty field is returned for a malformed raster or a sense other than +/-1.
ocean_current_field build_ocean_currents(const std::vector<terrain_substrate>& substrate,
                                         int gw, int gh, int rotation_sense = 1);

/// How far the current along a leg from (a_col, a_row) to (b_col, b_row)
/// runs WITH the leg, per mille in [-1000, 1000]: the mean, over the leg's
/// straight line sampled one tile per step (columns wrap the short way), of
/// each sampled tile's ocean-region current dotted with the leg's direction.
/// Land samples count zero, so a leg that is half ground feels half its sea's
/// current. EXACTLY ANTISYMMETRIC: the line is always walked from the same
/// canonical end, so the return leg reads the negation of the outbound, bit
/// for bit — the property that makes a current a direction rather than a toll.
int ocean_current_alignment_q(const ocean_current_field& f,
                              int a_col, int a_row, int b_col, int b_row);

/// A sea leg's cost against still water, per mille: `1000 - weight x align / 1000`,
/// so with the current a leg costs down to `1000 - weight` and against it up to
/// `1000 + weight`. ONE weight prices both directions (the item's "by one
/// measured weight"). `weight_q` must lie in [0, 999]; the caller rejects any
/// other value before asking.
inline int ocean_current_leg_cost_q(int weight_q, int align_q)
{
    return 1000 - (weight_q * align_q) / 1000;
}

/// True iff @p weight_q is a legal current weight: 0 (still water) to 999
/// (a leg with a full current nearly free, never free — the cost stays >= 1).
inline bool ocean_current_weight_valid(int weight_q)
{
    return weight_q >= 0 && weight_q < 1000;
}

/// FNV-1a over the field's regions, for determinism rows and reports.
uint64_t ocean_current_digest(const ocean_current_field& f);

/// WHICH LANDMASS EACH TILE STANDS ON (BL-1140): the 4-connected components
/// of everything that is NOT SEA (`is_sea`: ocean and coast), columns
/// wrapping, rows not, numbered in raster order of their first tile; -1 on
/// sea. Four-way because every traversal reader walks the four cardinal
/// steps (logistics.cpp): two realms are on one landmass exactly when a
/// traveller can walk between them without crossing the sea, so ground that
/// touches only at a corner is two landmasses. A lake is part of the land it
/// lies in. (`exploration_sweep`'s outward-want census also reads 4-connected
/// ground but treats lakes as water, so a lake can split its landmasses where
/// this does not.) Pure: the substrate raster alone decides it.
std::vector<int32_t> landmass_labels(const std::vector<terrain_substrate>& substrate, int gw, int gh);

/// The landmass a place stands on, off @p labels (`landmass_labels`). A
/// place on the sea -- a seat on the shoreline ring, owned via the shore --
/// stands on the landmass it borders: the label most of the ground within
/// two tiles carries, ties to the lower label, a count over a fixed window
/// so no walk order reaches it. -1 out of range or with no ground that near.
int32_t landmass_at(const std::vector<int32_t>& labels, int gw, int gh, int col, int row);

// ---------------------------------------------------------------------------
// THE SEA'S OWN WALK (BL-1098's lane walker, shared since BL-1152). A Dijkstra
// over the SEA tiles (`is_sea`: ocean and coast; a lake is not the ocean) on
// FOUR CARDINAL STEPS, columns wrapping and rows not -- the grid every
// traversal reader walks. Each step costs 1000 x the leg cost
// (`ocean_current_leg_cost_q`) of the step's direction against the ENTERED
// tile's ocean-region current at @p weight_q, at least 1 -- so still water is
// 1000 a tile. The frontier is ordered on the pair (cost, raster index), which
// is unique, so every tie resolves the same way on every machine. @p sea is
// one byte per tile, raster order, 1 on sea; @p currents null or @p weight_q 0
// is still water.
// ---------------------------------------------------------------------------

/// The nearest sea tile to (@p col, @p row) within @p radius (Chebyshev,
/// columns wrapping), ring by ring, ties to the lower raster index; -1 if none.
int nearest_sea_tile(const std::vector<std::uint8_t>& sea, int gw, int gh, int col, int row, int radius);

/// The walk from raster index @p from to @p to, both sea tiles: the path in
/// walk order, both ends included; empty when no water joins them.
std::vector<int> sea_walk(const std::vector<std::uint8_t>& sea, int gw, int gh,
                          const ocean_current_field* currents, int weight_q, int from, int to);

/// The cost of the cheapest walk from any of @p sources to every tile, by the
/// same steps (a multi-source Dijkstra); INT64_MAX where no water reaches.
/// Sources off the sea, or out of range, are skipped.
std::vector<int64_t> sea_cost_field(const std::vector<std::uint8_t>& sea, int gw, int gh,
                                    const ocean_current_field* currents, int weight_q,
                                    const std::vector<int>& sources);

/// BL-1152 -- every sea tile within @p radius of (@p col, @p row), in the order
/// `nearest_sea_tile` ranks them: ring by ring outward (Chebyshev, columns
/// wrapping), inside a ring by raster index. Its first entry is
/// `nearest_sea_tile`'s answer.
std::vector<int> sea_tiles_by_ring(const std::vector<std::uint8_t>& sea, int gw, int gh, int col, int row,
                                   int radius);

/// BL-1152 -- which body of water each tile is in, by the walk's own four
/// cardinal steps (columns wrapping, rows not): a label per tile, -1 off the
/// sea, labels numbered in raster order of each body's first tile. Two sea
/// tiles have a walk between them exactly when their labels match.
std::vector<int> sea_components(const std::vector<std::uint8_t>& sea, int gw, int gh);

/// BL-1152 -- `sea_cost_field` from one source, with the walk's predecessor
/// per tile, so a walk to any tile reads off it (`sea_path_on`) without a
/// second search. The path to a tile is the one `sea_walk` returns: a settled
/// tile's predecessor never changes after it settles.
struct sea_field
{
    std::vector<int64_t> dist;
    std::vector<int>     prev;
};
sea_field sea_field_from(const std::vector<std::uint8_t>& sea, int gw, int gh,
                         const ocean_current_field* currents, int weight_q, int source);
/// The walk from @p f's source to @p to, in walk order; empty if unreached.
std::vector<int> sea_path_on(const sea_field& f, int to);

/// BL-1152 -- A FLEET'S POWER AT SEA, @p cost from its port (a
/// `sea_cost_field` reading): `navy x 1024`, halved every @p halving_tiles
/// still-water tiles (x 1000 cost units), linear within a halving -- so a
/// fleet reaches less far against the current, which prices the distance.
/// 0 with no fleet, out of reach (INT64_MAX), or with @p halving_tiles <= 0.
/// Integer throughout; the scale (1024) is only resolution.
int64_t fleet_power_at(int64_t navy, int64_t cost, int64_t halving_tiles);
