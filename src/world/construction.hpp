#pragma once

#include "components.hpp"
#include "recipe_registry.hpp"
#include "world.hpp"

/// Outcome of a player-driven construction attempt. Only `placed` mutates the
/// world; every refusal leaves the world untouched.
enum class construction_result : uint8_t
{
    placed = 0,          ///< Building created, added to the corp, and paid for.
    invalid_tile,        ///< placement_rules::can_place_in_world rejected the tile (ocean / wrong deposit / not coastal).
    out_of_range,        ///< BL-323 S2: too far from a city / port / logistics hub to be supplied.
    insufficient_funds,  ///< The corporation cannot afford the build cost.
    no_corp,             ///< The corporation entity does not exist.
    no_tile,             ///< The tile entity does not exist.
    slot_occupied,       ///< The body's per-type cap is full (Launchpad: max 1 per body).
    insufficient_materials, ///< Unreachable since BL-095-lite folded material cost into credits (kept for switch compat).
    tech_locked,         ///< BL-344: the corporation has not earned the tech that unlocks this type.
    era_locked,          ///< BL-433: this building type is not part of the campaign's era band (e.g. a Launchpad at 0 CE).
    wrong_air,           ///< The recipe cannot run on this body's air (propellant routes, Ben 2026-10-09).
    needs_road,          ///< BL-1347: no road reaches the tile (a building), or the named tile carries none to lay from (BL-1348, a road).
    foreign_ground,      ///< BL-1357: a road may cross a border and no further -- the named tile is another nation's.
    // BL-692 removed `depth_locked` (BL-428): chain depth no longer gates a
    // build, so the value became unreachable. Tech is the only method lock now.
};

/// BL-323 S3 site-time multiplier for building `type`/`target` on `tile`: landform
/// x reach x stack, each 1.0 in the cheapest case (PRODUCTION.md § Construction site
/// time). It scales the build's duration in days, and since the quarter's draw is the
/// authored cost over the AUTHORED duration, it scales the materials and cash drawn too.
/// Const: reads the reach field without building it (an unbuilt field reads 1.0).
float construction_site_multiplier(const world& w, const recipe_registry& reg,
                                   entity_id tile, building_type type, resource_type target);

/// The workforce a newly constructed building of @p type is staffed at — what
/// `construct_building` sets `workforce_assigned` to. Passive infrastructure
/// (port, inland logistics hub, military base) staffs at zero; everything else,
/// the University and the research institute included (Ben, 2026-10-10;
/// PRODUCTION.md), at 0.5. Exposed so a fixture staffs a building the way the
/// build door does rather than inventing a state construction never makes.
float construction_initial_workforce(building_type type);

/// Construction is measured in DAYS (Ben, 2026-10-24; PRODUCTION.md § Construction
/// is measured in days): the days a build of `type`/`target` on `tile` takes at full
/// rate — the authored duration (quarters x 90 days) times the site multiplier,
/// rounded to a whole day, floored at 1; 0 for an instant (undurationed) type. What
/// `construct_building` sets `days_remaining` to.
int construction_build_days(const world& w, const recipe_registry& reg,
                            entity_id tile, building_type type, resource_type target);

/// The quarters those days span (ceil(days / 90)): what `construct_building` sets
/// `ticks_remaining`, the "still building" gate, to.
int construction_build_ticks(const world& w, const recipe_registry& reg,
                             entity_id tile, building_type type, resource_type target);

/// BL-1066 (Ben, 2026-09-23): what a build will actually draw, priced now — the flat
/// `build_cost` plus the materials at the tile market's current price, times the site
/// multiplier, plus the construction capacity drawn per build tick. The ONE figure the affordability gate, the Build door and the rival
/// scorer price a build at, so no reader can commit to less than the build will draw.
float construction_capex(const world& w, const recipe_registry& reg, entity_id tile,
                         building_type type, resource_type target,
                         std::uint16_t recipe = no_recipe);

/// Attempt to construct a building of `type` (targeting `target` for an extraction
/// site) on `tile` for corporation `corp`, paying the registry build cost from the
/// corporation's balance. The Layer 4 build front door (docs/ui/SELECTION.md, the
/// tile Selection element) and the construction-panel placement seam both route
/// through this single function, which mirrors the generation-time placement
/// (placement_rules + the building-component authoring in corporation_generation.cpp)
/// but is player-driven and debits the balance.
///
/// On success: a new building entity is created with a `stockpile_component`, its
/// `building_component` is authored (workforce staffed at 0.5, 0 for a port; the
/// extraction target set; a processing facility seeded with the default "steel"
/// recipe, reconfigurable later via building management), the building is appended
/// to the corporation's `assets`, the build cost is debited, and `out_building`
/// receives the new id.
///
/// @param w            World; mutated only on success.
/// @param reg          Loaded registry (build cost + the default recipe).
/// @param corp         Corporation building (and paying) — usually w.player_entity.
/// @param tile         Target tile entity.
/// @param type         Building type to construct.
/// @param target       Extraction target (ignored for non-extraction types).
/// @param out_building Set to the new building id on success; null_entity otherwise.
/// @param recipe       Processing recipe to seed (BL-162). `no_recipe` (the default,
///                     so no existing caller changes) keeps the historic "steel" seed;
///                     the construction ledger passes the recipe its row was priced on,
///                     so the estimate and the built building describe the same thing.
/// @return             `placed` on success, else the reason it was refused.
construction_result construct_building(world& w, const recipe_registry& reg,
                                       entity_id corp, entity_id tile,
                                       building_type type, resource_type target,
                                       entity_id& out_building,
                                       std::uint16_t recipe = no_recipe);

/// Remove @p building from the world entirely, freeing its tile (Ben's 2026-07-22
/// call: "how can I remove the building and build something else").
///
/// The inverse of `construct_building`, and distinct from the `decommissioned`
/// flag — which idles a building in place (it stops producing and stops paying
/// wages, but still occupies its tile and still pays material upkeep). Idling is
/// reversible and keeps the asset; demolition destroys it.
///
/// No refund. The build cost was spent turning credits into an installation on a
/// tile; removing the installation does not turn it back. A salvage return is a
/// separate design question, not a default.
///
/// On success the building's `building_component` and `stockpile_component` are
/// erased and its id is removed from the owning corporation's `assets`. Anything
/// still holding the id (an `economy_report` row from the last tick) must resolve
/// it through `w.buildings` and tolerate the miss — the report is regenerated on
/// the next tick.
///
/// @param w        World; mutated only on success.
/// @param corp     Owner demolishing — must own the building. BL-1402: a
///                 corporation (its `assets`) or a NATION (its `state_assets`,
///                 through `demolish_state_building`).
/// @param building Building entity to remove.
/// @return         True on success; false if the building does not exist or
///                 @p corp does not own it.
bool demolish_building(world& w, entity_id corp, entity_id building);

/// BL-1296 (state industry; NATIONS.md § State industry, "The player's
/// controls"): build a STATE processing facility running @p recipe on @p tile
/// for @p nation — the corporation build door, owned by the nation and
/// committed against its treasury. The same checks construct_building makes
/// (era, tech, air, tile, placement), asked of the nation's champion (same home
/// nation, same tree), plus: the nation is a major, @p recipe is one the state
/// runs (`state_runs`, state_industry.hpp), and @p tile is the nation's own
/// ground. Affordability is the treasury's UNCOMMITTED part
/// (`state_uncommitted_treasury`) against the build's whole capex; payment is
/// pay-as-you-build from the treasury (run_construction), and a tick the
/// treasury cannot fund pauses the site. The building joins the nation's
/// `state_assets`. Mutates only on `placed`.
/// BL-1399 (Ben, 2026-10-23; NATIONS.md § What a nation owns): THE NATION'S BUILD
/// DOOR - @p nation builds @p type (with @p target / @p recipe as construct_building
/// takes them) on @p tile, any sector, for itself: no corporation between. The nation
/// is the actor every gate asks about - its own tech tree, and its OWN GROUND (a tile
/// another nation holds, or none does, is refused `foreign_ground`). Paid
/// pay-as-you-build from the treasury's uncommitted part; @p founding (generation's)
/// skips the commitment gate. The building joins the nation's `state_assets`.
/// Mutates only on `placed`. The construct_state_* doors below are this door.
construction_result construct_nation(world& w, const recipe_registry& reg,
                                     entity_id nation, entity_id tile,
                                     building_type type, resource_type target,
                                     std::uint16_t recipe, entity_id& out_building,
                                     bool founding = false);

construction_result construct_state_building(world& w, const recipe_registry& reg,
                                             entity_id nation, entity_id tile,
                                             std::uint16_t recipe, entity_id& out_building,
                                             bool founding = false);
// @p founding (BL-1296 fix round): the fold's founding — the commitment gate is
// skipped (the founder pays the capex itself, `hand_over_state_industry`).

/// BL-1255 (LOGISTICS.md § 4c): build a railway STATION for @p nation's state
/// industry on @p tile — a city tile touching track on the nation's own ground —
/// committed against the treasury's uncommitted part, built pay-as-you-build like
/// any state building (construct_for's checks, station placement included).
/// BL-1309 (LOGISTICS.md § Logistic Points, "One pool"): a major nation builds
/// an inland logistics hub on its own ground, paid from the uncommitted treasury,
/// joining its state_assets. Any other type is refused (invalid_tile).
construction_result construct_state_logistics(world& w, const recipe_registry& reg,
                                              entity_id nation, entity_id tile,
                                              building_type type, entity_id& out_building);

construction_result construct_state_station(world& w, const recipe_registry& reg,
                                            entity_id nation, entity_id tile,
                                            entity_id& out_building);

/// Sprint 53 (NATIONS.md § State industry, "A major's coastal seat has a state
/// port", Ben 2026-10-14): build a PORT for @p nation's state industry on
/// @p tile, a coastal tile of its own ground. @p founding as for
/// construct_state_building: the fold's port skips the commitment gate (the
/// founder pays its capex itself).
construction_result construct_state_port(world& w, const recipe_registry& reg,
                                         entity_id nation, entity_id tile,
                                         entity_id& out_building, bool founding);

/// BL-1255: what laying one track tile on @p tile costs — `rail.track_build_cost`
/// plus `rail.track_steel_per_tile` steel at the tile market's posted price.
float track_tile_cost(const world& w, const recipe_registry& reg, entity_id tile);

/// BL-1255: whether a cardinal neighbour of @p tile is track owned by @p owner.
bool track_tile_extends(world& w, entity_id owner, entity_id tile);

/// BL-1255: LAY ONE TILE OF TRACK for major @p nation (Ben, 2026-10-13: "track
/// costs the treasury per tile, plus steel per tile, with upkeep per tile").
/// Instant, like a road; debited up front from the treasury. Refused — nothing
/// mutated — unless: @p nation is a major (has a champion); the tile exists, is
/// land, carries no track, and is the nation's own ground; the railway exists in
/// this era; the tile extends one of the nation's OWN lines (a cardinal
/// neighbour is its track) or opens a line at a city; the tile's market shelf
/// holds the steel (it is BOUGHT: drawn off the shelf); and the uncommitted
/// treasury covers `track_tile_cost`.
construction_result lay_track(world& w, const recipe_registry& reg, entity_id nation,
                              entity_id tile);

/// BL-1296: demolish a state building @p nation owns — demolish_building for
/// the nation owner kind. No refund (the same rule). True on success; false if
/// the building does not exist or @p nation does not own it.
bool demolish_state_building(world& w, entity_id nation, entity_id building);

/// Attempt to place a road of tier `tier` across one SIDE of `tile` for corporation `corp`
/// (BL-147 core, BL-172 tier ladder, BL-1340 a road is an edge). A road is an EDGE between
/// two tiles, never a property of one (LOGISTICS.md § 4): on success it raises that
/// crossing — both halves, `side` of `tile` and the facing side of the neighbour — to
/// `tier` (1=Track, 2=Road, 3=Highway), which lowers the traversal cost of the step across
/// it, and invalidates the world's A* cost cache so dispatch sees the roaded cost.
/// Upgrade-in-place is allowed (raise a Track to a Highway); re-laying the same or a lower
/// tier is refused (`invalid_tile`), as is a side with no neighbour — a road to nowhere
/// cannot be expressed. The cost (registry `road_econ(tier)`) is a flat credit sum plus
/// materials priced from the tile's local market, debited up front (roads are instant,
/// unlike durative building construction). The build front door (SELECTION.md) and app's
/// pending-road handler route through this single function.
///
/// @param w     World; mutated only on success (the edge raised, cache cleared, balance debited).
/// @param reg   Loaded registry (per-tier road cost).
/// @param corp  Owner placing (and paying) — usually w.player_entity. BL-1402: a
///              corporation (paid from its balance) or a NATION acting as itself (paid
///              from its uncommitted treasury, never into debt, and booked on its state
///              ledger's construction line, as track is).
/// @param tile  Target tile entity — one end of the crossing.
/// @param tier  Road tier to place: 1=Track (default, = BL-147 behaviour), 2=Road, 3=Highway.
/// @param side  Hex side of `tile` the road crosses: 0=E, 1=NE, 2=NW, 3=W, 4=SW, 5=SE
///              (hex_neighbors.hpp). `k_road_side_auto` (the default) picks the cheapest
///              crossable neighbour deterministically — lowest traversal cost, ties to the
///              lower side index — which is what a caller with a tile but no direction
///              (the AI's road candidate, a UI that has not grown a direction) gets.
///              **It is the LAST parameter deliberately** (review of 0831de6d): `side` and
///              `tier` are both uint8_t, so an older 5-argument call that passed its tier
///              where `side` had been inserted compiled and silently read the tier as a
///              side. Appending it keeps every existing call correct.
/// @return      `placed` on success; `invalid_tile` (no neighbour that way, a side no walk
///              can cross, WATER AT BOTH ENDS — a crossing the player lays must touch land
///              at one end, Ben 2026-10-17 — or already at or above `tier`),
///              `insufficient_funds`, `no_corp`, or `no_tile`. A refusal mutates nothing
///              and costs nothing.
construction_result place_road(world& w, const recipe_registry& reg,
                               entity_id corp, entity_id tile, std::uint8_t tier = 1,
                               std::uint8_t side = k_road_side_auto);
