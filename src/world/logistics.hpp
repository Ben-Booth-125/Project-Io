#pragma once

#include "components.hpp"
#include "world.hpp"

#include <cstdint>
#include <vector>

// ---------------------------------------------------------------------------
// Intra-body logistics pathfinding (BL-077 — planetary logistics core)
// ---------------------------------------------------------------------------
// Terrain-weighted A* over a body's tile grid: the shortest weighted path
// between two tiles, each tile weighted by its landform build-cost (TILES.md)
// and discounted by road_level, respecting the east-west cylinder wrap. Ocean
// tiles are crossable at a higher "sea-leg" cost, so a path exists on any
// connected body; whether the cheapest path touches ocean selects sea vs land
// mode. Results cache on `world` (body_tile_index + astar_cost_cache), so the
// per-Tick dispatch loop pays the search once per fixed endpoint pair.
//
// Grid topology matches nation_generation.cpp: 4-cardinal neighbours, column
// wrap, raster index grid_y*grid_width + grid_x; the search reuses that file's
// priority-queue Dijkstra shape.

/// Canonical per-tile logistics traversal cost by landform (TILES.md multipliers:
/// plains 1.0, highland 1.25, mountain 2.0, canyon 1.5, valley 1.1, crater 1.3,
/// rift 1.6). Deliberately distinct from nation_generation's private expansion_cost
/// (a different concept — territory growth — left untouched to preserve world-gen
/// determinism).
float landform_logistics_cost(terrain_landform lf);

/// Traversal-cost multiplier for a tile's road tier (BL-077). Tier 0 = 1.0 (no road);
/// higher tiers reduce the cost so A* prefers roaded corridors. 0 everywhere in the
/// core (roads arrive with BL-146); wired here so the follow-on needs no A* change.
float road_traversal_multiplier(std::uint8_t road_level);

/// Traversal-cost multiplier for river adjacency (BL-170). Stacks *multiplicatively*
/// with road_traversal_multiplier (river-adjacent tiles benefit alongside any road
/// discount, not instead of it). tile_traversal_cost is a single-tile (node) weight —
/// it is not edge-aware, so this is an undirected flat discount, not a directional
/// upstream/downstream one; directionality would need tile_traversal_cost reworked to
/// take both endpoints (a larger A*-cost-model change, out of scope here).
float river_traversal_multiplier(std::uint8_t river_edges);

/// Per-body raster index (grid_y*grid_width + grid_x -> tile entity, null_entity for an
/// absent cell), built and cached in world.body_tile_index on first use. Deterministic —
/// a pure function of the body's tiles, independent of tiles-map iteration order. Returns
/// an empty vector for an unknown body.
const std::vector<entity_id>& body_tile_grid(world& w, entity_id body);

/// Terrain-weighted A* between two tiles on the same body, with cylinder wrap and the
/// road discount; caches on world.astar_cost_cache under a canonicalised endpoint key
/// (edge cost is the average of the two tiles' costs, so the path is symmetric). Returns
/// {reachable=false} if either tile is unknown or not on `body`.
logistics_path intra_body_path(world& w, entity_id body, entity_id src_tile, entity_id dst_tile);
