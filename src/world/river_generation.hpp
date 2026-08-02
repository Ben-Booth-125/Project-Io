#pragma once

#include "components.hpp"
#include "world.hpp"

#include <cstdint>
#include <vector>

// ---------------------------------------------------------------------------
// Rivers & freshwater generation (BL-170)
// ---------------------------------------------------------------------------
// A river is a directed EDGE crossing one of a tile's 6 hex sides, never a
// tile-occupying feature (a lake would be tile-occupying — out of scope; see
// docs/generation/TILE_GENERATION.md). Traced as a deterministic downhill walk
// over the Pass-1 heightmap (src/world/tile_generation.cpp) from high ground
// toward ocean/basin.
//
// Pipeline-shape convention (BL-051): this is a SIBLING pass invoked after
// generate_body_tiles returns — it needs Pass 2's ocean placement and the
// full Pass 5 landform set for source-tile candidacy — not spliced into the
// six-pass core.
// ---------------------------------------------------------------------------

/// Hex side ordering used by river tracing and the intra-body A* discount:
/// 0=E, 1=NE, 2=NW, 3=W, 4=SW, 5=SE (odd-r offset). Mirrors the neighbour-walk
/// shape used by placement_rules.cpp's is_coastal, independently tabulated here
/// (river_generation.cpp) so this pass stays self-contained rather than reaching
/// into placement_rules.
///
/// @param dc, dr    Column/row offset from a tile to its candidate neighbour.
/// @param odd_row   Whether the source tile's grid_y is odd (odd-r offset rule).
/// @return          Hex side index 0-5, or -1 if (dc, dr) is not one of the six
///                  neighbour offsets for that row parity.
int hex_side_for_offset(int dc, int dr, bool odd_row);

/// True if any of the tile's 6 sides carries a river — the water-adjacency read
/// BL-166/168-style farming predicates want. A secondary consequence of the same
/// bitmask; wiring it into farming logic is a separate task.
bool tile_borders_river(const tile_component& tc);

/// Per-edge logistics discount multiplier for the side of @p from that a river
/// crosses, as traced by generate_rivers. Returns 1.0 (no discount) if that side
/// carries no river. Downstream travel (leaving @p from via the direction the
/// river flows) is cheaper than upstream. Stacks MULTIPLICATIVELY with
/// road_traversal_multiplier in the A* edge-cost calculation (logistics.cpp).
///
/// @param from  The tile being left (the "from" node of the A* edge).
/// @param side  Hex side index (0-5, hex_side_for_offset's ordering) the edge crosses.
float river_edge_discount(const tile_component& from, int side);

/// Deterministic downhill river trace over a body's already-generated tiles.
///
/// Reads @p height (the Pass-1 heightmap, same array captured by
/// generate_body_tiles's optional generation_record) plus each tile's
/// composition (ocean already placed by Pass 2) and landform (source
/// candidacy from Pass 5), and writes tile_component::river_edges /
/// river_downstream on the tiles in @p tile_ids.
///
/// Pure function of (@p height, @p tile_ids' composition/landform, @p seed) —
/// no clock/rand leaks, no unordered-map iteration-order dependence: source
/// selection sorts candidates by (height, raster index) before drawing, and
/// each trace step is a strictly-decreasing walk over @p height (guaranteeing
/// termination and ruling out cycles by construction).
///
/// @param w        World holding the tile components (mutated in place).
/// @param tile_ids Raster-order tile IDs from generate_body_tiles (index = row*gw+col).
/// @param gw, gh   Grid dimensions.
/// @param height   Pass-1 heightmap, raster order, same as generation_record::height.
/// @param seed     Per-body seed distinct from the six-pass offsets (XOR a river-specific prime).
void generate_rivers(world& w, const std::vector<entity_id>& tile_ids,
                     int gw, int gh, const std::vector<float>& height, uint32_t seed);
