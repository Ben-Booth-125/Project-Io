#pragma once

// ---------------------------------------------------------------------------
// Logistic Points — ONE POOL PER NATION (BL-1309; LOGISTICS.md § Logistic
// Points, "One pool, made by buildings, consumed by production", Ben 2026-10-14)
// ---------------------------------------------------------------------------
// Logistic Points are a per-tick RATE held by each nation, never banked:
//
//   * MADE ONLY BY BUILDINGS, each on its own network. A completed, running
//     logistics hub makes points carried along ROADS (a higher road tier is a
//     cheaper step, so the full-strength distance is longer); a railway station
//     along RAIL; a port over SEA. Cities make none. Whoever built a source, it
//     adds to the pool of the nation whose GROUND it stands on. It makes its
//     points IN PROPORTION to the upkeep goods it receives (Ben 2026-10-14:
//     half its stone, half its points; never zeroed outright) — its upkeep
//     supply factor, `building_supply_scalar`.
//   * REACH FADES WITH DISTANCE. A building is reached at full strength out to
//     its source's threshold distance along that source's network, then the
//     reach falls linearly to zero over the fade length. Its COVERAGE is the
//     best reach any of its host nation's sources gives it. Off a network a step
//     is impassable (Ben, 2026-10-24: the network is the roads and the rail), so
//     points reach no tile a road or line does not lead to. `lp_offnet_step`
//     survives only as the market-attraction leg's price basis (NR-1050).
//   * PRODUCTION CONSUMES THEM. Each producing building (extraction site,
//     processing facility: complete, not idled) NEEDS `lp_need_per_run` points
//     per unit of its run (labour assigned x workforce target). Its claim on the
//     pool is need x coverage; the pool grants claims (below) and the building
//     produces in proportion to need granted / need — the scalar the economy
//     step folds into the building's labour, so half the points is half the
//     output.
//   * THE HOST NATION'S POOL, ONE CLASS (BL-1405). After the fold every building
//     on a nation's ground is the nation's, and draws on that nation's pool as one
//     queue. The share is MECHANICAL: while points suffice every claim is met;
//     short, claims are served in routed queue order. The state / champion /
//     other share rule (`set_logistics_shares`, BL-1309) RETIRED with the
//     corporations (Ben, 2026-10-23): the verb is refused and the stored rule is
//     ignored. `lp_class` keeps its three slots for the ledger's shape only;
//     every claim is `state`.
//   * A building on nobody's ground has no pool: it produces nothing.
//
// PURE AND UNCACHED: `resolve_logistic_points` is a function of the world's
// buildings, tiles, road levels, track and ownership, recomputed every tick.
// Every walk is in ascending id order and every field is a bounded Dijkstra
// with (cost, tile id) ordering, so the answer is replay-exact.
//
//   * A UNIT'S SUPPLY DRAWS THE SAME POOL, AND CLAIMS FIRST (BL-1445; LOGISTICS.md
//     § How a tile's points get to it, "A unit's supply draws on the pool, and
//     claims first", Ben 2026-10-28, overturning the 2026-10-16 march draw).
//     Movement draws no points: a march is paid in days. Once a quarter each unit
//     asks the points its supplies need - their QUANTITY, as a shipment's draw is
//     its cargo - PAID AT THE CLOSEST ROAD of its network (its own tile when the
//     network reaches it; BL-1456, below), routed against its own nation's
//     sources exactly as a building's request is (`lp_route_to`: the claim is
//     need x cover). The supply claims are served in ascending unit id, BEFORE any
//     producer (NR-917 kept), and PRODUCTION'S GRANTS ARE COMPUTED ON WHAT IS LEFT.
//     So guns, butter and freight compete in one currency, and a campaign visibly
//     starving its own industry is the intended, legible cost of an army in the
//     field. A unit more than 8 tiles from any tile its network reaches is
//     granted nothing.
//
//     OFF THE ROAD, UP TO 8 TILES (BL-1456; LOGISTICS.md § A unit draws off the
//     road, Ben 2026-10-29): a unit off its network is PAID AT THE CLOSEST ROAD -
//     the nearest tile its network reaches within 8 steps over crossable land, ties
//     to the lowest tile id (`unit_supply_draw_tile`) - and served by that tile's
//     cover. More than 8 from any, it draws nothing. Recomputed every resolve.
//
//     WHOSE POOL: THE UNIT'S NATION'S, routed from that nation's own sources -
//     never the pool of a foreign ground it stands on, which would fund an
//     invasion out of the invaded nation's points.
//
//     BL-1352 retired the second, unrelated system this used to be: a flat
//     20/tick pool at EVERY POPULATION CENTRE (`active_lp_anchor_pools`), which
//     the marches and the Throughput lens read while this pool ran the economy.
//     That is why cities appeared to supply logistics when the doc had already
//     ruled they make none.

#include "components.hpp"
#include "world.hpp"

#include <array>
#include <functional>
#include <map>
#include <set>
#include <tuple>
#include <utility>
#include <cstddef>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class recipe_registry;

/// Who on a nation's ground a building's claim is counted under. BL-1405: one
/// class - every claim is `state`; `champion` and `other` are retired slots.
enum class lp_class : std::uint8_t
{
    state    = 0, ///< the host nation's own state industry
    champion = 1, ///< the host nation's champion corporation
    other    = 2, ///< every other owner: other firms, foreign firms
};
inline constexpr std::size_t lp_class_count = 3;

/// Which network a source makes points along.
enum class lp_network : std::uint8_t
{
    road = 0, ///< inland logistics hub
    rail = 1, ///< railway station
    sea  = 2, ///< port
};
inline constexpr std::size_t lp_network_count = 3;

// --- Calibration (BL-1309, measured on the shipped 1960 start; BL-1360 retuned the
// --- road tiers and rail). Distances are in network-cost units: a road step costs
// --- `lp_road_step[tier]` (one step off the road costs `lp_offnet_step`), a sea step
// --- costs 1 and a landing on the coast 1, with no step inland (BL-1396), and rail
// --- counts TRACK STEPS between stations - point to point, no off-network step at all.

/// Points a source makes per tick, by network.
inline constexpr std::array<float, lp_network_count> lp_source_points = {120.0f, 150.0f, 120.0f};
/// Full-strength distance, by network.
///
/// BL-1360 (Ben, 2026-10-16; LOGISTICS.md § One pool): a better road carries points
/// further - Track 6 tiles, Road 8, Highway 10 at full strength, fading to ZERO AT
/// DOUBLE (12 / 16 / 20). The road entry stays one cost threshold, 6, and the TIER
/// is carried by the step cost below: 6 / 1.0 = 6, 6 / 0.75 = 8, 6 / 0.6 = 10 tiles.
/// Rail carries 20 track steps, delivered point to point (lp_distance_field), and
/// fades to zero at double (40) - the road's rule (Ben, 2026-10-23, NR-1056).
/// BL-1396 (Ben, 2026-10-21): a port stretches FIFTY water tiles at full strength,
/// fading to zero at double (100) - the road's rule at sea's scale - and over water
/// only: its points land on the coastal tiles they reach and go no further inland.
inline constexpr std::array<float, lp_network_count> lp_full_reach = {6.0f, 20.0f, 50.0f};
/// Fade length beyond the full-strength distance (reach 0 at full + fade). Every
/// network's equals its full reach, which is what "zero at double" means.
inline constexpr std::array<float, lp_network_count> lp_fade_length = {6.0f, 20.0f, 50.0f};
/// A road step's cost by tier (index 1..3). Index 0 is "no road": off the network.
/// BL-1360: 1.0 / 0.75 / 0.6, so the tiers reach 6 / 8 / 10 tiles at full strength.
inline constexpr std::array<float, 4> lp_road_step = {3.0f, 1.0f, 0.75f, 0.6f};
/// One step off a road source's network (onto land with no road). Rail takes no
/// off-network step (point to point, BL-1360), and neither does sea: a port's points
/// land on the coast and stop (BL-1396).
inline constexpr float lp_offnet_step = 3.0f;
/// Points a producing building needs per tick per unit of run
/// (workforce_assigned x workforce_target/100).
inline constexpr float lp_need_per_run = 10.0f;

/// One producing building's line.
struct lp_building_row
{
    entity_id building = null_entity;
    entity_id owner    = null_entity;
    entity_id nation   = null_entity; ///< host nation (ground), null = nobody's ground
    lp_class  cls      = lp_class::other;
    float     need     = 0.0f; ///< points needed at its run
    float     coverage = 0.0f; ///< [0,1], the best reach of the host's sources
    float     granted  = 0.0f; ///< points granted (<= need x coverage)
    float     fraction = 0.0f; ///< granted / need in [0,1] — the output scalar
};

/// One nation's pool this tick.
struct lp_nation_row
{
    entity_id nation = null_entity;
    float     pool   = 0.0f;         ///< made this tick, before the supply claims
    /// BL-1445: what this nation's units ASKED for their supplies (the quantity, before
    /// cover), and what they were granted. Production claims against
    /// `pool - supply_granted`, never against `pool`.
    float     supply_claim   = 0.0f;
    float     supply_granted = 0.0f;
    std::array<float, lp_network_count> made{}; ///< by network
    std::array<int,   lp_network_count> sources{};
    std::array<float, lp_class_count>   claim{};
    std::array<float, lp_class_count>   granted{};
    bool      rule_set = false; ///< BL-1405: the share rule retired; always false
    /// BL-1361: what production LEFT in each pot once its queue was served - the
    /// points a shipment leaving this nation's ground may draw (LOGISTICS.md § How a
    /// tile's points get to it: "a shipment draws what production left"). With a share
    /// rule each class keeps its own pot; with none, pot 0 holds all of it. The trade
    /// pass draws these down (`lp_trade_draw`).
    std::array<float, lp_class_count>   left{};
    /// BL-1361: points shipments drew this tick, every class together.
    float     trade_drawn = 0.0f;
};

/// BL-1361: a market's centre tile as a shipment's ANCHOR - the nation whose pool a
/// shipment leaving that market draws, and the routed cover there (`lp_route.cover`):
/// the share of a request the anchor's qualifying sources can carry. No nation (the
/// centre is nobody's ground) draws on no pool and admits nothing.
struct lp_trade_anchor
{
    entity_id nation = null_entity;
    float     cover  = 0.0f;
};

/// A source as the resolver read it.
struct lp_source_row
{
    entity_id  building = null_entity;
    entity_id  tile     = null_entity;
    entity_id  nation   = null_entity; ///< host nation, or null on nobody's ground: it feeds NO pool
    lp_network network  = lp_network::road;
    /// The points it actually makes this tick, upkeep folded in. Carried rather than
    /// left to each reader to recompute: a surface that re-derived it is how the lens
    /// and the economy came to disagree in the first place (BL-1352).
    float      made     = 0.0f;
    /// BL-1356 (Ben, 2026-10-21, NR-1057): on the chain back to its nation's capital.
    /// Only a connected source adds `made` to the pool; an isolated one makes points
    /// nobody can draw. True where the nation has no capital on this body.
    bool       connected = true;
};

/// One unit's supply request, handed IN by the economy step (BL-1445,
/// `collect_supply_claims`), which owns what a unit's supplies are. Served in the
/// order given (ascending unit id).
struct lp_supply_claim
{
    entity_id unit   = null_entity;
    entity_id nation = null_entity; ///< the unit's own nation, whose pool serves it; null = no pool
    entity_id tile   = null_entity; ///< the tile it stands on; the request is PAID at `unit_supply_draw_tile` of it (BL-1456)
    float     need   = 0.0f;        ///< the quantity of its supplies this quarter
};

/// One unit's answer.
struct lp_supply_row
{
    entity_id unit    = null_entity;
    entity_id nation  = null_entity;
    entity_id tile    = null_entity;
    float     need    = 0.0f;
    float     cover   = 0.0f; ///< the tile's routed cover from the nation's sources, [0,1]
    float     granted = 0.0f; ///< <= need x cover, and what the pool had left
    /// BL-1456 (Ben, 2026-10-29; LOGISTICS.md § A unit draws off the road): the tile the
    /// request was PAID at - the unit's own tile when its network reaches it, else the
    /// closest tile of its network within `lp_offroad_draw_steps` steps over crossable
    /// land (`unit_supply_draw_tile`), else null (past its roads: share 0). `cover` is
    /// this tile's. Recomputed every resolve, never saved.
    entity_id paid_tile  = null_entity;
    /// Steps from the unit's tile to `paid_tile` (0 on the network); -1 with no paid tile.
    int       draw_steps = -1;
    /// granted / need in integer per-mille (truncated); 1000 for a request of zero.
    /// The share `run_nation_garrison_upkeep` folds into the one decay rule.
    int       share_permille = 1000;
};

struct lp_tick
{
    std::vector<lp_nation_row>   nations;   ///< ascending nation id
    std::vector<lp_source_row>   sources;   ///< ascending building id
    std::vector<lp_building_row> buildings; ///< ascending building id
    std::vector<lp_supply_row>   supplies;  ///< BL-1445, in the order claims were served (ascending unit id)
    /// building id -> fraction, for the economy step's lookup.
    std::unordered_map<entity_id, float> fraction;
    /// unit id -> its supply share per-mille, for the upkeep pass's lookup (BL-1445).
    std::map<entity_id, int> supply_share;
    /// BL-1361: market id -> its anchor, for every market, read at resolve.
    std::map<entity_id, lp_trade_anchor> trade_anchors;
    /// BL-1361: true once `resolve_logistic_points` filled this tick. A shipment
    /// committed against a report nobody resolved (a fixture) resolves it first.
    bool resolved = false;
};

/// BL-1361 (LOGISTICS.md § How a tile's points get to it, "A shipment draws points
/// again", Ben 2026-10-16): the units of a @p units cargo leaving @p market that its
/// anchor admits for @p trader this tick - ONE point per unit of cargo, what moves and
/// not how far. The anchor's cover caps the share (cover x cargo, the fade's rule for
/// any request); the trader's pot of what production left caps the rest. Never refused
/// whole: it returns what is admitted, 0 only when nothing is - and, as the retired cap
/// did, never a crumb below one unit unless the cargo's covered share is smaller. Pure.
float lp_trade_admit(const world& w, const lp_tick& t, entity_id trader, entity_id market,
                     float units);

/// BL-1361: draw @p units points for a shipment @p trader sent from @p market, from
/// the pot `lp_trade_admit` read. Call only with what it admitted.
void lp_trade_draw(const world& w, lp_tick& t, entity_id trader, entity_id market, float units);

/// BL-1361: the points still left at @p market's anchor - its nation's pots together,
/// 0 where the anchor has no cover or no nation. What `market_component::
/// trade_lp_spare` records after the trade pass.
float lp_trade_left(const lp_tick& t, entity_id market);

/// The network a building type makes points along, or false when it makes none.
bool lp_source_network(building_type t, lp_network& out);

/// True when @p b is a running source (complete, not decommissioned, not state-idle).
bool lp_source_running(const building_component& b);

/// True when @p b is a producing building that consumes points this tick.
bool lp_consumer(const building_component& b);

/// Points @p b needs at its run (0 for a non-consumer).
float lp_need(const building_component& b);

/// BL-1360: the tolerance on "full strength". 0.6 is not exact in float, so a sum of
/// steps that is 6.0 in arithmetic can land a hair either side of it. Ten straight
/// highway steps measure 5.9999995 (under, logistic_points_harness L7), but a path
/// mixing tiers is not guaranteed to, and the ruling's "Road 8, Highway 10" must not
/// hinge on rounding. Hygiene, not a measured failure. Every
/// reader that asks "is this full strength?" goes through `lp_full_strength`, so the
/// figure and the words beside it cannot disagree.
inline constexpr float lp_full_eps = 1e-4f;

/// True when network distance @p d is inside @p n's full-strength reach.
inline bool lp_full_strength(lp_network n, float d)
{
    return d <= lp_full_reach[static_cast<std::size_t>(n)] + lp_full_eps;
}

/// Reach in [0,1] of a source on network @p n at network distance @p d.
float lp_reach(lp_network n, float d);

/// BL-1396: the any-water walk from @p seeds - water (sea or lake, NR-1061) crossed on all
/// six hex sides, a landing on the shore ending the walk, every step 1, bounded at the
/// sea's reach (or @p bound). No longer a port's field (Ben, 2026-10-25: points ride the
/// lanes, `lp_lane_walk`): it is the MEASURE of the fifty within which two ports are
/// laned (`lay_port_lanes`), and what generation's bridge reads.
std::unordered_map<entity_id, float> lp_sea_walk(world& w, entity_id body,
                                                 const std::vector<entity_id>& seeds);
std::unordered_map<entity_id, float> lp_sea_walk(world& w, entity_id body,
                                                 const std::vector<entity_id>& seeds, float bound);

/// A PORT'S FIELD (Ben, 2026-10-25; LOGISTICS.md § One pool, "Points ride the lanes"):
/// from @p seeds (a port's tile) onto a laned water tile beside it (sea, or a lake: NR-1061),
/// along laned water on all six hex sides, LANDING only on a standing port's tile beside a
/// lane - every step 1, a landing ending the walk, bounded at the sea's full reach plus
/// fade. Open water no lane crosses, and a shore with no port, are never reached. What lp_distance_field
/// returns for lp_network::sea.
std::unordered_map<entity_id, float> lp_lane_walk(world& w, entity_id body,
                                                  const std::vector<entity_id>& seeds);

/// Network distance field from @p seeds on @p body for network @p n, bounded at
/// that network's full + fade: tile -> distance, only tiles within the bound.
/// Road: every tile the walk reaches. Sea: the lanes and the port tiles they land on
/// (`lp_lane_walk`, Ben 2026-10-25). RAIL (BL-1360): point to point - the
/// seed station tiles themselves (0) and the OTHER station tiles the rail network
/// joins them to (track steps), and no other tile at all.
std::unordered_map<entity_id, float> lp_distance_field(world& w, entity_id body, lp_network n,
                                                       const std::vector<entity_id>& seeds);

/// Resolve this tick's pools, claims and grants. Pure; mutates only caches.
///
/// @param supplies BL-1445: the units' supply requests, in the order they are to be
///        served (ascending unit id). Each is routed on its tile and served need x
///        cover while its nation's pool lasts, before any producer claims, so
///        production sees `pool - supply_granted`. Empty (the default) is a tick with
///        no unit, and leaves every grant exactly as it was before supply joined.
lp_tick resolve_logistic_points(world& w, const std::vector<lp_supply_claim>& supplies = {});

/// One running source as routing reads it (BL-1356).
struct lp_source_ref
{
    entity_id  building = null_entity;
    entity_id  tile     = null_entity;
    lp_network network  = lp_network::road;
};

/// Per-evaluation memo for routing (`lp_route_to`, `lp_coverage_at`). BL-1356: a
/// tile's points come from INDIVIDUAL sources, each tested on its own reach, so the
/// memo holds each source's own field rather than one field per network. Clear it
/// after any source is built, idled or demolished, or any road changes.
struct lp_reach_memo
{
    /// (nation, body) -> that nation's running sources on that body, ascending building id.
    std::map<std::pair<entity_id, entity_id>, std::vector<lp_source_ref>> sources;
    /// source building -> its own distance field (`lp_distance_field` from its tile alone).
    std::map<entity_id, std::unordered_map<entity_id, float>> fields;
    /// (nation, body, anchor tile) -> the sources CONNECTED to that anchor - the nation's
    /// capital (Ben, 2026-10-21): the chain (Ben, 2026-10-20), a source whose reach
    /// touches the anchor or the tile of a source already in the set, closed outward
    /// until nothing more joins.
    std::map<std::tuple<entity_id, entity_id, entity_id>, std::set<entity_id>> connected;
    void clear() { sources.clear(); fields.clear(); connected.clear(); }
};

/// How a tile's request is routed (BL-1356; LOGISTICS.md § How a tile's points get
/// to it, the four mechanism rules, Ben 2026-10-20). A source QUALIFIES for a tile
/// when its own reach touches the tile and it is CONNECTED to its nation's capital
/// (its reach touches the capital or a connected source's tile - the chain);
/// the qualifying sources are taken nearest first, each carrying its reach x the
/// request, until the request is covered.
struct lp_route
{
    /// Qualifying reaches summed, at most 1: the share of a request that may be granted.
    float      cover    = 0.0f;
    /// True when at least one source qualifies.
    bool       reached  = false;
    /// The NEAREST qualifying source carries full strength here: the request is
    /// "inside the falloff range" and is queued ahead of every request that is not.
    bool       full     = false;
    /// The nearest qualifying source's distance as a fraction of its network's full
    /// reach, so road, rail and sea order on one scale. Orders requests within a band.
    float      norm     = 0.0f;
    lp_network network  = lp_network::road; ///< the nearest qualifying source's network
    float      distance = 0.0f;             ///< and its network distance to the tile
    /// How many qualifying sources the cover was summed from (1 = no top-up).
    int        used     = 0;
    /// A source reached the tile but is not connected to the capital (no chain back),
    /// so it was not counted - the reason a tile beside a hub can read 0.
    bool       blocked  = false;
};

/// Route @p tile's request against @p nation's running sources on @p body.
lp_route lp_route_to(world& w, lp_reach_memo& memo, entity_id nation, entity_id body,
                     entity_id tile);

/// BL-1456 (Ben, 2026-10-29; LOGISTICS.md § A unit draws off the road, up to 8 tiles from
/// it): how far a unit may stand from its network and still draw its supply.
inline constexpr int lp_offroad_draw_steps = 8;

/// WHERE A UNIT'S SUPPLY IS PAID (BL-1456). @p unit_tile itself when @p nation's network
/// reaches it (`lp_route_to`'s `reached`: a connected source of the nation reaches the
/// tile - the resolver's one test of "on the network"); else the CLOSEST tile the network
/// reaches within @p max_steps steps of it over crossable land - a breadth-first walk
/// across the four cost-walk sides (`road_side_is_crossable`, the walk a retreat and every
/// cost walk takes), entering only land tiles, so a water gap is no step - ties to the
/// lowest tile id. null_entity when none is in bound: the unit is past its roads and
/// draws nothing. Pure and deterministic (layer by layer, sides in order, the winner the
/// minimum id of the first layer that holds one). The simulation calls it with the
/// default bound; the Logistics lens with a wider one, for display only.
/// @param steps_out Optional; receives the steps to the tile (0 on it), -1 with none.
entity_id unit_supply_draw_tile(world& w, lp_reach_memo& memo, entity_id nation,
                                entity_id unit_tile, int max_steps = lp_offroad_draw_steps,
                                int* steps_out = nullptr);
/// The same, on a memo of its own (a caller with no resolve in hand, e.g. placement).
entity_id unit_supply_draw_tile(world& w, entity_id nation, entity_id unit_tile,
                                int max_steps = lp_offroad_draw_steps, int* steps_out = nullptr);
/// The same walk with "on the network" handed in as @p reached - the one search the
/// two forms above wrap (they pass `lp_route_to`'s `reached`). For a caller asking of
/// many units at once, which builds the set once (`lp_reached_tiles`) and tests a tile
/// by a lookup rather than routing it.
entity_id unit_supply_draw_tile(world& w, const std::function<bool(entity_id)>& reached,
                                entity_id unit_tile, int max_steps = lp_offroad_draw_steps,
                                int* steps_out = nullptr);

/// Every tile on @p body where `lp_route_to(w, memo, nation, body, tile).reached` holds:
/// some source of @p nation that is connected to its capital reaches it with reach > 0.
/// Built from the same per-source fields routing reads, so a tile is in it exactly when
/// routing would say reached. For LOOKUP ONLY - its iteration order is a hash table's and
/// must never reach state. A derived read, never serialised.
std::unordered_set<entity_id> lp_reached_tiles(world& w, lp_reach_memo& memo, entity_id nation,
                                               entity_id body);

/// The hub chain @p target needs (BL-1395; LOGISTICS.md § Where generation puts the
/// sources, "The chain is laid outward from the capital"): the road hubs, in the order
/// they would be built, that bring @p target to full coverage on @p nation's chain -
/// a hub carrying it at full strength where one can stand touching the chain, else an
/// extension on the chain's edge, steered by the walk over the nation's own dry ground,
/// repeated. Nothing is placed; @p may_place says which tiles a hub may take. The
/// first tile is exactly the tile generation would place next, so generation and the
/// rival scorer (AI_OPPONENT.md § 11, 2026-10-21) lay one chain by one rule.
struct lp_chain_plan
{
    bool                   reachable = false; ///< the target can be covered this way
    std::vector<entity_id> tiles;             ///< hubs to build, in order (empty: covered now)
};
lp_chain_plan lp_plan_chain(world& w, lp_reach_memo& memo, entity_id nation, entity_id body,
                            entity_id target, const std::function<bool(entity_id)>& may_place,
                            int max_hubs = 64);

/// Where a tile's coverage came from: the NEAREST QUALIFYING source's network and
/// its network distance (BL-1356). `reached` is false when no source of the host
/// nation both reaches the tile and is connected to its capital (BL-1353's card).
struct lp_coverage_why
{
    bool       reached  = false;
    lp_network network  = lp_network::road;
    float      distance = 0.0f;
    int        used     = 0;     ///< sources the cover was topped up from (lp_route::used)
    bool       blocked  = false; ///< a source reached here but has no chain to the capital (lp_route::blocked)
};

/// The coverage a building standing on @p tile would get from the running
/// sources of the nation whose ground it is ([0,1]; 0 on nobody's ground).
/// @param why Optional; receives the network and distance the figure came from.
float lp_coverage_at(world& w, lp_reach_memo& memo, entity_id tile,
                     lp_coverage_why* why = nullptr);

/// The Logistic Points REACHING @p tile this tick (Ben, 2026-10-24: the Throughput
/// lens reads absolute points, LENSES.md § Throughput lens): every running source of
/// the nation whose ground the tile is that QUALIFIES for it - its own reach touches
/// the tile and it is connected to the capital, `lp_route_to`'s test - contributing
/// its reach there x the points it made this tick (`lp_source_row::made`, read off
/// @p t). Every qualifying source counts, not only the ones routing tops a request up
/// from: this is what is available at the tile, not what a request drew. 0 on
/// nobody's ground. A derived READ for the lens - pure, summed in ascending building
/// id, never serialised, never read by the simulation.
/// @param sources_out Optional; receives how many sources contributed.
float lp_points_reaching(world& w, lp_reach_memo& memo, const lp_tick& t, entity_id tile,
                         int* sources_out = nullptr);

/// The routed FLOW across one tile (Ben, 2026-10-24: the Logistics lens colours each
/// road by it, LENSES.md § Logistics lens). `requested` is the need of every producing
/// building whose routed request crosses the tile on its way to the capital - its own
/// tile's buildings and every building downstream; `granted` the points the resolver
/// GRANTED those same requests; and `available` what the chain COULD deliver there -
/// every source whose walk (building -> source, or a source -> capital link) crosses
/// the tile, counted once, its points made this tick x its reach at the tile. The lens
/// colours a road by available / requested. In LP/tick.
struct lp_flow
{
    float requested = 0.0f;
    float granted   = 0.0f;
    float available = 0.0f;
};

/// The routed flow of @p nation's requests on @p body: tile -> flow, for every tile at
/// least one request crosses. Each building's request is routed as `lp_route_to` routes
/// it (the same qualifying sources, nearest first, topped up until covered); each
/// source it draws on carries its share - its reach there, the last one cut to what
/// covers - of the need and of what @p t granted. That share lies on every tile of the
/// walk from the building back to the source, and on along the source's chain to the
/// capital. Neither walk is recorded by the resolver (it keeps distances and the set of
/// connected sources): both are READ BACK off the same per-source fields it routed on -
/// see `field_walk_back` and the chain tree in logistic_points.cpp. A derived READ for
/// the lens - pure, summed in ascending building id, never serialised, never read by
/// the simulation.
std::unordered_map<entity_id, lp_flow> lp_route_flow(world& w, lp_reach_memo& memo,
                                                     const lp_tick& t, entity_id nation,
                                                     entity_id body);

/// Validate a share rule as the value that lands (each finite, in [0,1], summing
/// to 1 within 1e-4). The seam rejects anything else whole.
bool lp_share_rule_valid(const std::array<float, lp_class_count>& share);

// ---------------------------------------------------------------------------
// Generation: seed sources so the 1960 world produces (BL-1309)
// ---------------------------------------------------------------------------
struct lp_seed_report
{
    int nations          = 0;
    int hubs_placed      = 0;
    int consumers        = 0;
    int nobodys_ground   = 0; ///< consumers on no nation's ground (no pool, produce nothing)
    int fully_covered    = 0; ///< after seeding: coverage 1
    int partly_covered   = 0;
    int uncovered        = 0;
    float need_total     = 0.0f;
    float pool_total     = 0.0f;
    float granted_total  = 0.0f;
    int   sources_by_net[lp_network_count] = {0, 0, 0};
    /// Sources standing but idled by the state funding gate at the first tick
    /// (a state hub, station or port its treasury cannot fund): not counted in
    /// `sources_by_net`, and reaching nobody. Filled by `lp_seed_read_at_gate`.
    int   gate_idled_sources = 0;
    /// BL-1356: consumers no hub could be placed for - the centre does not reach
    /// them at full strength, and a hub on their own tile would not reach the centre
    /// (Ben's rule B), so it would deliver nothing. Left uncovered, counted here.
    int   unconnectable = 0;
    /// Of those, the ones on ground CUT OFF from their capital: no path of the nation's
    /// own dry tiles joins them (an island, or a holding beyond water or a neighbour).
    /// A road hub's chain cannot cross that, whatever its reach.
    int   unconnectable_cut_off = 0;
    /// BL-1367: the building ids counted in `unconnectable`, ascending - so the coverage
    /// census checks each uncovered consumer against the seeding's own reason, not a count.
    std::vector<entity_id> unconnectable_ids;
    /// BL-1356: ports laid to bridge ground cut off from its capital by water, and the
    /// islands so bridged (a home port, where none was connected, plus an island port).
    int   ports_placed    = 0;
    int   islands_bridged = 0;
    /// The port link (Ben, 2026-10-25): road crossings laid to join each link's ports to
    /// the network on their own coasts, and a linked island's stray fragments to its port.
    int   link_crossings  = 0;
    /// Every port lays lanes (Ben, 2026-10-25): lanes the seeding laid between ports
    /// (its opening pass over the standing ports, and each port it landed), the tiles they
    /// added to the water, and pairs within fifty no sea walk joins.
    int   port_lanes_laid        = 0;
    int   port_lane_tiles        = 0;
    int   port_lanes_unreachable = 0;
    /// Stacked hubs (Ben, 2026-10-26; `stack_logistics_hubs`): hubs stood on a tile
    /// already holding a source, at capitals and at market centres; the tiles that
    /// took any; and the hubs wanted but refused by the tile's stack cap or by the
    /// nation's funding (its whole first-tick state bill must still fit its treasury).
    int   hubs_stacked_capital   = 0;
    int   hubs_stacked_centre    = 0;
    int   stacked_tiles          = 0;
    int   stack_refused_cap      = 0;
    int   stack_refused_funds    = 0;
};

/// Place logistics sources at the fold so every producing building on a nation's ground is
/// covered and every pool meets its need with margin (LOGISTICS.md § Where generation puts the
/// sources). BL-1356: a source delivers only on the chain back to its nation's capital, so hubs
/// are laid OUTWARD along that chain to each consumer, steered over the nation's own dry ground;
/// ground cut off from the capital by water is bridged by a pair of ports; pool hubs stand only
/// where they join the chain. Owned by the nation (state industry) for a major, else by the
/// consumer's owner. Complete and zero-staffed, as every generated holding is. Deterministic.
lp_seed_report seed_logistics_sources(world& w);

/// The seeding's READING (consumers, coverage, pool, grant), refilled on @p w
/// as it stands. `seed_logistics_sources` ends with it.
void lp_seed_read(world& w, lp_seed_report& rep);

/// The reading AT THE FUNDING GATE (BL-1382 economy follow-up, Ben 2026-10-19):
/// a state source the gate will idle at the first tick (`state_gate_would_idle`)
/// is not a running source, so the reading is retaken with every state building
/// the gate would idle counted idle — restored afterwards, nothing else
/// written — and `gate_idled_sources` says how many sources that removed. The
/// fold calls it after the state reserve, so the printed line is what the first
/// tick will run.
void lp_seed_read_at_gate(world& w, const recipe_registry& reg, lp_seed_report& rep);

/// STACKED HUBS WHERE THE NEED WARRANTS (Ben, 2026-10-26: "allow multiple levels of
/// logistics hubs from generation, especially at the capital"; LOGISTICS.md § Where
/// generation puts the sources). After the seeding and the founding reserve: for each
/// nation (ascending id), at its capital first and then at each market centre on its
/// ground that already holds a connected road hub (ascending market id), the routed
/// requests crossing that tile (`lp_route_flow`'s `requested`) are set against what the
/// chain makes available there; the shortfall is met by further road hubs on the same
/// tile, one per `lp_source_points[road]`. Each is refused when the tile's
/// non-extraction stack cap is full, or when the nation's treasury would no longer carry
/// its whole first-tick state bill (`state_first_tick_bill`) with the hub's bound added -
/// so the funding gate funds the hub and everything it funded before. Nation-owned,
/// complete, staffed full.
/// One flow reading per nation, taken before any stacking: RNG-free and order-free.
void stack_logistics_hubs(world& w, const recipe_registry& reg, lp_seed_report& rep);
#define LP_HAS_STACK_HUBS 1 // lp_coverage_census reads it to build against an older tree

void print_lp_seed_report(const lp_seed_report& r);
