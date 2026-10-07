#pragma once

#include "components.hpp"
#include "world.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <unordered_map>
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

/// A tile's traversal-cost WEIGHT — ocean or landform cost, discounted by
/// road_level. The per-node weight A*, body_reach_field's Dijkstra, and (BL-470)
/// a marching unit all share; an edge/hop cost is the mean of its two nodes.
/// Exposed (promoted out of logistics.cpp's anonymous namespace) so run_unit_march
/// (economy_system.cpp) spends march points against the SAME cost function
/// rather than a second invented model.
float tile_traversal_cost(const tile_component& tc);

/// Per-body raster index (grid_y*grid_width + grid_x -> tile entity, null_entity for an
/// absent cell), built and cached in world.body_tile_index on first use. Deterministic —
/// a pure function of the body's tiles, independent of tiles-map iteration order. Returns
/// an empty vector for an unknown body.
const std::vector<entity_id>& body_tile_grid(world& w, entity_id body);

/// Terrain-weighted least-cost path between two tiles on the same body, with cylinder
/// wrap, the road discount and the river discount. Returns {reachable=false} if either
/// tile is unknown or not on `body`.
///
/// DIRECTED, AND A PURE FUNCTION OF THE ORDERED PAIR (BL-1126, path cost reads the
/// cache; LOGISTICS.md § 1). A river discounts an edge one way only, so the cost is
/// always the travel cost @p src_tile -> @p dst_tile, read from @p dst_tile's flood
/// field (built if absent) and never from @p src_tile's, and cached on
/// world.astar_cost_cache under the ORDERED key (body, src, dst). The answer is the
/// same warm or cold, before or after a load. `tiles` stays stored lo -> hi (a
/// reader wanting travel order flips it when src is the higher id).
///
/// Returns a reference into the cache (BL-362: a cache hit used to copy the whole tile
/// vector). Valid until invalidate_logistics_caches clears the map — read or copy it
/// before any call that can invalidate; never mutate it through a cast.
const logistics_path& intra_body_path(world& w, entity_id body, entity_id src_tile,
                                      entity_id dst_tile);

/// Every cell of @p body whose travel cost TO @p anchor_tile is at most
/// @p max_cost, as (raster index, cost) pairs in settle order (cost
/// ascending). The same directed edge weights `intra_body_path`'s flood
/// relaxes (landform x road x river, a water cell at the sea weight, cylinder
/// wrap), so with @p land_only false a cell's cost here equals
/// intra_body_path(cell, anchor).cost wherever both are within reach. That is
/// a PATH cost, not a convoy's bill: a convoy also needs a port at both ends to
/// cross water and is billed per mode. @p land_only true never enters a water
/// cell (the anchor itself is always settled). A bounded Dijkstra, NOT cached
/// and touching no cache but the raster index (`body_tile_grid`). BL-1125: the
/// gravity fold's reach.
std::vector<std::pair<int, float>> bounded_cost_to_tile(world& w, entity_id body,
                                                        entity_id anchor_tile, float max_cost,
                                                        bool land_only);

// ---------------------------------------------------------------------------
// A route's LEGS (BL-1186, goods cross markets — SUPPLY.md § Logistical cost)
// ---------------------------------------------------------------------------
// "Mode is a property of the leg, not of the whole route": a route that crosses
// water is land -> port -> sea -> port -> land, each leg priced at its own mode.
// These are the leg-confined paths that route is built from; supply_system.cpp's
// router chooses the ports.

/// The domain one leg of a route may cross.
enum class leg_domain : std::uint8_t
{
    land = 1, ///< never enters a water cell
    sea  = 2, ///< SEA only (is_sea: never a lake), between two endpoints that may be land (the ports)
};

/// The least-cost path @p src_tile -> @p dst_tile confined to @p domain: one LEG.
/// The same directed edge weights intra_body_path relaxes (landform x road x river,
/// water at the sea weight, cylinder wrap), so a land leg costs exactly what the
/// unconfined path costs over the same tiles.
///   land: never enters water; a water endpoint is unreachable. src == dst on land
///         is a reachable zero-cost leg (an origin standing on its port).
///   sea:  leaves @p dst_tile's flood (the destination PORT) by SEA only (a lake is no lane), crosses
///         water, and may END on a land cell touching that water (the origin port)
///         but never continues overland — port -> water -> port and nothing else.
///         Never zero-length: one port is no crossing. The port <-> water hop is
///         made across any of the six hex sides (placement's `is_coastal` reads
///         six, the flood four), so every placeable Port is a reachable sea end;
///         water-to-water stays cardinal, priced exactly as intra_body_path.
/// Directed and cached like intra_body_path: the destination's leg flood on
/// world.leg_flood_fields, the pair on world.leg_path_cache under the ORDERED key,
/// both cleared by invalidate_logistics_caches and on load. `tiles` is stored lo -> hi
/// (intra_body_path's orientation rule). A reference into the cache.
const logistics_path& intra_body_leg_path(world& w, entity_id body, entity_id src_tile,
                                          entity_id dst_tile, leg_domain domain);

/// BL-1186: the tiles of @p body carrying a BUILT, ACTIVE Port (the sea leg's gate,
/// SUPPLY.md § Infrastructure gates), ascending and unique. Cached on
/// world.body_port_tiles; cleared with the logistics caches.
const std::vector<entity_id>& body_active_port_tiles(world& w, entity_id body);

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

/// BL-1230 (power crosses markets) — LOGISTICS.md § 3a, "The province is the
/// grid's cell". A province with a road tile in it is WIRED; wired provinces
/// whose roads join form ONE grid. Roads join where two roaded land tiles are
/// 4-cardinal neighbours (east-west wrapped — the road raster's own adjacency),
/// or across a strait: a run of at most kMaxCrossingTiles non-ocean water tiles
/// between them, the same crossing the road-only walk admits. The province is
/// the cell, so two road pieces in one province are one grid even where their
/// tiles do not touch.
///
/// Returns wired province id -> grid id, the grid id being the LOWEST wired
/// province id in it, so the answer is independent of every visit order.
/// Built lazily and cached on `world.power_grid_of_province`; cleared by
/// invalidate_logistics_caches. Deterministic.
const std::map<std::uint32_t, std::uint32_t>& province_power_grid(world& w);

/// The grid @p tile's province is on, or 0 when the province is dark (no road)
/// or the tile is unpartitioned. Builds the cache on first use.
std::uint32_t tile_power_grid(world& w, entity_id tile);

/// Clear the path-cost and reach-field caches together. Call after any event
/// that can change traversal cost or the anchor set. The caches rebuild lazily
/// on next read, so an over-clear costs one Dijkstra, never a wrong answer; a
/// MISSED clear is a reach field that lies.
///
/// NARROWED 2026-08-12. Ben's 2026-08-08 ruling chose the simple every-event
/// rule over per-type cleverness because "each of these is rare against the
/// per-frame reads". The 0 CE world broke that premise: with the corp AI
/// building/idling every tick and hundreds of generated sites completing
/// through the warm start, the caches were cleared essentially every econ tick
/// and the rebuilds (per-pair Dijkstras over 45,240 tiles, plus the reach
/// field) became the tick — the AppHangB1 stall. The sim-rate call sites now
/// gate on `building_affects_logistics`: only a port / inland hub can change
/// the anchor set, and no building type changes traversal cost (that is
/// road_level — place_road still clears unconditionally). Player-rate UI
/// sites keep the unconditional clear; over-clearing at click rate is free.
inline void invalidate_logistics_caches(world& w)
{
    w.astar_cost_cache.clear();
    w.logistics_flood_fields.clear(); // same contract: any traversal/anchor change stales it
    w.leg_flood_fields.clear();       // BL-1186: the leg-confined floods and pairs, and the
    w.leg_path_cache.clear();         // per-body port set (a port completing or idling moves
    w.body_port_tiles.clear();        // it: building_affects_logistics), on the same contract
    w.body_reach_cost.clear();
    w.lp_anchor_fields.clear();       // BL-1117: road-weighted and anchor-keyed, so it stales
                                      // on exactly the events the three above do
    w.power_grid_of_province.clear(); // BL-1230: the power grid is a function of road_level
    w.power_grid_built = false;
}

/// True when a state change on this building TYPE can alter a cached logistics
/// answer — i.e. it can appear in the anchor set (is_supply_anchor: completed,
/// non-idled ports and inland hubs). Every other type affects neither anchors
/// nor traversal, so its completion/idle/demolition needs no cache clear.
inline bool building_affects_logistics(building_type t)
{
    return t == building_type::port || t == building_type::inland_logistics_hub;
}

// ---------------------------------------------------------------------------
// Active Logistic Points (BL-596 — LOGISTICS.md § Logistic Points)
// ---------------------------------------------------------------------------
// LP is a per-tick RATE, never a stock (Ben, ruling on NR-343, 2026-08-20):
// this function is PURE and UNCACHED — call it fresh every tick, never store
// its result on `world` or across a call boundary. A generator per anchor
// tile only (cities, built-and-active ports/inland hubs — the SAME anchor
// set body_reach_field seeds from, never a per-corp pool).

/// This tick's ephemeral active-LP capacity at every supply anchor on @p
/// body, keyed by anchor tile, at @p lp_per_anchor_tick each. An anchorless
/// body (or one where @p lp_per_anchor_tick <= 0) returns an empty map —
/// "no active LP exists here", not an error.
///
/// FIRST CONSUMER: BL-596's `run_unit_march` (economy_system.cpp), which
/// builds one of these per body it encounters marching units on, then
/// decrements it as units draw against their nearest anchor this tick.
/// EXTENSION POINT for BL-597 (passive convoy LP draw, a later item): call
/// this again for the convoy layer's own draw against the same anchor set,
/// rather than re-deriving it.
std::unordered_map<entity_id, float> active_lp_anchor_pools(world& w, entity_id body,
                                                             float lp_per_anchor_tick);

// ---------------------------------------------------------------------------
// Passive Logistic Points (BL-597 — LOGISTICS.md § Logistic Points, the bifold
// half BL-596 left as an extension point)
// ---------------------------------------------------------------------------
// LOGISTICS.md's bifold table names ONE generated rate per anchor, split by
// USE — "Passive LP | Drawn by: the market's own flow" vs "Active LP | Drawn
// by: militaries draw active only" — never two authored numbers. So BL-597
// does not add a second `passive_lp_per_anchor_tick`; both halves draw the
// SAME per-anchor pool this scaffolding builds, contested within one tick by
// whichever consumer reaches an anchor first. That contention is what makes
// "war flips the queue" (LOGISTICS.md's "finding worth keeping") observable
// rather than a written rule with nothing to bite on.

/// Per-body -> per-anchor-tile -> LP remaining THIS TICK. The shared shape
/// both `run_unit_march` (active) and `commit_convoy` (passive, BL-597) draw
/// down. Never stored on `world` — a caller-owned scratch object, exactly
/// like the local `lp_pools_by_body` BL-596 built inside `run_unit_march`
/// alone; BL-597 promotes that shape here so ANY caller wanting the two
/// halves to contend can hand the SAME instance to both, while a caller that
/// wants one consumer tested in isolation (a harness exercising only the
/// march pass, say) can pass none and get a private, always-fresh map,
/// unchanged from BL-596's original behaviour.
using lp_pool_map = std::unordered_map<entity_id, std::unordered_map<entity_id, float>>;

/// Fetch (or lazily build via `active_lp_anchor_pools`) @p body's entry inside
/// @p pools_by_body, so two consumers sharing one `lp_pool_map` instance
/// within a tick see each other's draws on first touch rather than each
/// re-deriving their own copy. Factored out of BL-596's `run_unit_march` (its
/// own inline lazy-build lambda) so BL-597's `commit_convoy` calls the exact
/// same fetch-or-build rather than a second copy of it.
std::unordered_map<entity_id, float>& lp_pool_for_body(lp_pool_map& pools_by_body, world& w,
                                                        entity_id body, float lp_per_anchor_tick);

/// An anchor tile in @p pool (a body's per-anchor LP pool) at least
/// intra-body path cost from @p from_tile (`intra_body_path(from_tile, anchor)`,
/// origin -> anchor), with a FIXED CHOICE among exact ties — usually the lowest
/// tile id, not always (logistics.cpp § build_lp_anchor_field: rounding can
/// fold a one-ulp gap into a tie) — so hash-map iteration over @p pool cannot
/// matter. Returns `null_entity` if none of @p pool's anchors is reachable
/// from @p from_tile. Factored out of BL-596's `run_unit_march` (its own
/// inline nearest-anchor reduction over a marching unit's current position)
/// so BL-597's `commit_convoy` reuses it for a convoy's dispatch tile rather
/// than inventing a second anchor-selection rule (LOGISTICS.md rule 2: adopt
/// the node half, refuse a second distance model).
///
/// ONE FIELD PER BODY (BL-1117, settle tick one). Answered from the body's
/// `lp_anchor_field` — one multi-source Dijkstra over @p pool's anchors, built
/// on first use and cached on `world::lp_anchor_fields` — never from the
/// per-pair path cache. The cost is the anchor's own flood distance at
/// @p from_tile — the from_tile -> anchor travel cost `intra_body_path` reads
/// for this pair — so the answer is a pure function of the body's tiles and
/// @p pool's KEY SET: identical warm or cold, before or after a load. Builds no
/// `logistics_flood_fields` entry.
entity_id nearest_lp_anchor(world& w, entity_id body, entity_id from_tile,
                            const std::unordered_map<entity_id, float>& pool);

/// The nearest-anchor field @p nearest_lp_anchor answers from, over @p pool's
/// key set (built or rebuilt as needed). Exposed for the purity/exactness
/// harness (tools/verify/lp_anchor_field_check.cpp) and for a future
/// Throughput lens; the reference is valid until the next logistics-cache clear
/// or the next call for this body with a different anchor set.
const lp_anchor_field& body_lp_anchor_field(world& w, entity_id body,
                                            const std::unordered_map<entity_id, float>& pool);

// ---------------------------------------------------------------------------
// Physical scale and travel time (Ben, 2026-08-12)
// ---------------------------------------------------------------------------
//
// WHAT WAS MISSING. Before this, the codebase had no tile scale at all and no
// intra-body travel time. Convoy speed was `1 / distance_in_AU` — an
// INTERPLANETARY calibration — and `body_distance_au` returns 0 for two markets
// on the same body, so speed clamped to 1.0 and every intra-body convoy arrived
// in exactly one econ tick (90 days) whether it crossed one tile or all 312.
// Distance cost money and never cost time.
//
// That is why tripling the map (hard_coded_world.hpp) could not on its own make
// distance feel bigger: with travel time constant, a bigger map means the same
// 90 days buys three times the reach.

/// Reference figures for an Earth-mass world. Named rather than inlined because
/// they are the calibration this whole model hangs on.
inline constexpr float earth_radius_km = 6371.0f;

/// Kilometres across one tile on @p body — the physical scale the game has
/// never had.
///
/// DERIVED, NOT AUTHORED. Planetology already generates `home_mass` in Earth
/// masses; a rocky planet's radius follows its mass as roughly R ∝ M^0.27, so
/// the circumference and therefore the tile width fall out of a scalar the
/// generation chain already settled. That keeps this a *consequence* of an
/// upstream stage rather than a magic number sitting beside it.
///
/// At Earth mass on the 312-column grid this is ~128 km per tile, which puts a
/// day's march (~25 km) at about a fifth of a tile and makes a tile a
/// region-sized unit rather than a field.
///
/// Returns 0 for an unknown body, which callers must treat as "no scale" rather
/// than "instant".
float body_km_per_tile(const world& w, entity_id body);

/// Days a laden convoy takes to cross one *effective* tile — that is, one unit
/// of `logistics_path::cost`, which is already terrain-weighted (plains 1.0,
/// mountain 2.0). Terrain cost IS a time multiplier, so the existing A* weights
/// do double duty here rather than needing a parallel table.
///
/// Two modes, because they differ by roughly five times and that difference is
/// the whole reason coastal trade is worth designing (BL-188):
///   - land: an ox-and-cart caravan, ~25 km/day
///   - sea:  a coasting vessel, ~130 km/day
inline constexpr float caravan_km_per_day = 25.0f;
inline constexpr float coastal_km_per_day = 130.0f;

/// Days in one econ tick. MIRRORS `sim_loop::econ_tick_days` (core/sim_loop.hpp,
/// 30 × 3 = 90) and is restated here rather than included because `world/*` must
/// not depend on `core/*` — the headless harnesses link the world layer alone.
/// If the tick length ever changes, these two must change together; the
/// stepped-clock harness asserts they agree.
inline constexpr int econ_tick_days_world = 90;

/// Econ ticks a convoy needs to traverse @p path on @p body, minimum 1.
///
/// The result is deliberately quantised to whole econ ticks: the economy
/// resolves quarterly, so a convoy that would take 40 days still lands on the
/// next clearing. What changes is that a long haul now takes MANY quarters and
/// a short one takes a single quarter, where previously every haul took exactly
/// one regardless of length.
int convoy_travel_ticks(const world& w, entity_id body, const logistics_path& path);

/// BL-1186: days ONE leg of a multi-leg route takes — @p path_cost effective tiles at
/// its own mode's speed (sea: coastal, otherwise caravan), on convoy_travel_ticks' scale.
/// 0 for a zero-length leg or a body with no scale. A route sums its legs' days and
/// quantises once (travel_ticks_for_days), so a short land leg is not rounded up to a
/// whole quarter on its own.
float leg_travel_days(const world& w, entity_id body, float path_cost, convoy_mode mode);

/// Whole econ ticks for @p days, minimum 1 — convoy_travel_ticks' own quantisation.
int travel_ticks_for_days(float days);

// ---------------------------------------------------------------------------
// Convoy position (BL-458 — a convoy already HAS a position)
// ---------------------------------------------------------------------------
// `convoy_component` stores no path and no tile, which reads at first like
// interdiction needing new state. It does not. The full tile path is derivable
// from the two market endpoints, and `progress` interpolates a head along it.
// body_surface_canvas.cpp has derived exactly that since BL-152 to draw the
// vision beam; this is that derivation MOVED out of the renderer, so the world
// and the picture cannot disagree about where a convoy is.
//
// THE ORIENTATION RULE LIVES HERE, and that is the point of the move.
// `intra_body_path` caches on the ORDERED (src, dst) key (BL-1126: a path is
// directed) but stores its reconstructed tile sequence lo->hi whichever way the
// pair was asked — so the cached path runs source->destination only when the
// source tile happens to be the numerically lower id. Every reader therefore
// owes a conditional reverse.
// A reader that forgets it puts the head at the WRONG END of the lane roughly
// half the time, and on screen the beam looks fine either way, so nothing
// catches it. One owner of the rule, asserted in both directions by
// tools/verify/interdiction.cpp.

/// A convoy's lane as tiles. `body` is the body both endpoints sit on, or
/// `null_entity` for an inter-body (space) lane, which has no tile path at all.
/// `tiles` runs SOURCE -> DESTINATION and is empty when the lane is inter-body,
/// when either endpoint market / centre tile is unresolved, or when no path
/// exists.
struct convoy_route
{
    entity_id              body = null_entity;
    std::vector<entity_id> tiles;
};

/// Derive @p cv's lane, oriented source->destination. Non-const `w` because
/// `intra_body_path` populates the A* cache; nothing else is mutated and no
/// game state is created. (BL-458's design writes this as taking a
/// `const world&`; that is not achievable without a second, uncached A*.)
convoy_route convoy_route_tiles(world& w, const convoy_component& cv);

/// Index of the head along a lane of @p tile_count tiles at @p progress (0..1).
/// Factored out so the renderer's glide — which offsets progress by the
/// fraction through the current econ tick — and the sim's discrete read share
/// ONE rounding rule. Returns -1 for an empty lane.
int convoy_head_index(std::size_t tile_count, float progress);

/// The tile @p cv's head occupies right now, or `null_entity` when it has none:
/// an inter-body leg in transit, an unresolved endpoint, or an unreachable
/// lane. The interdiction trigger's single question.
entity_id convoy_tile_at(world& w, const convoy_component& cv);
