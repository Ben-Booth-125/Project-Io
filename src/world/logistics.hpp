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

// ---------------------------------------------------------------------------
// Logistics reach (BL-323 S2 — the placement-side "breadth must cost something")
// ---------------------------------------------------------------------------
// Until this, placement had no distance rule of any kind: a corp could site a
// building arbitrarily far from any road, hub, city or port at no cost and with
// no refusal. That makes optimal siting "the richest tile anywhere" — a lookup
// rather than a decision, and the first thing an AI on the corp-command seam
// finds. Reach is the constraint that gives siting a trade-off.
//
// Deliberately the SAME cost function convoys already pay (landform weight x
// road discount), so the rule reads as "you must be able to supply it", not as
// a second, invented distance metric.

/// True iff @p tile carries something that anchors supply: a city (population
/// centre), or a BUILT, ACTIVE port or inland logistics hub (the completion
/// contract matches the convoy discount — an unbuilt or decommissioned node
/// anchors nothing). These are exactly BL-148/149's logistics nodes — the
/// places a convoy can already start cheaply — so reach inherits that
/// vocabulary rather than introducing a rival one.
bool is_supply_anchor(const world& w, entity_id tile);

/// True iff @p body carries any anchor at all: a city, or a port/hub building
/// in ANY state (under construction and decommissioned included — existence,
/// not activity). The first-anchor bootstrap reads this: on a body where it is
/// false, an anchor-type placement skips the reach rule, and committing that
/// first anchor immediately makes it true again so the exemption cannot be
/// used twice while the first hub is still building.
bool body_has_supply_anchor(const world& w, entity_id body);

/// Weighted cost from every tile of @p body to its NEAREST supply anchor, raster
/// indexed (grid_y*grid_width + grid_x). Infinity where no anchor is reachable
/// (an unanchored body, or an island cut off from one).
///
/// One multi-source Dijkstra over the body, cached on `world.body_reach_cost`
/// and invalidated wherever `astar_cost_cache` is. Deterministic: seeded from
/// the anchor set in raster order, never in tiles-map iteration order.
const std::vector<float>& body_reach_field(world& w, entity_id body);

/// Reach cost for one tile, read-only. Returns -1 when the field has not been
/// built for that body yet, so a caller can tell "not computed" from "computed
/// and unreachable" (which returns infinity). The const half of the pair above:
/// UI surfaces hold a `const world&` and must not trigger the Dijkstra.
float tile_reach_cost(const world& w, entity_id tile);

/// Clear the path-cost and reach-field caches together. Call after ANY event
/// that can change traversal cost or the anchor set: a building placed,
/// demolished, completed, or decommissioned/resumed, or a road laid (Ben's
/// 2026-08-08 ruling: the simple every-event rule over per-type cleverness —
/// each of these is rare against the per-frame reads the caches serve). The
/// caches rebuild lazily on next read, so an over-clear costs one Dijkstra,
/// never a wrong answer; a MISSED clear is a reach field that lies.
inline void invalidate_logistics_caches(world& w)
{
    w.astar_cost_cache.clear();
    w.body_reach_cost.clear();
}
