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
//     costs `lp_offnet_step`, so points reach only a few tiles beyond it.
//   * PRODUCTION CONSUMES THEM. Each producing building (extraction site,
//     processing facility: complete, not idled) NEEDS `lp_need_per_run` points
//     per unit of its run (labour assigned x workforce target). Its claim on the
//     pool is need x coverage; the pool grants claims (below) and the building
//     produces in proportion to need granted / need — the scalar the economy
//     step folds into the building's labour, so half the points is half the
//     output.
//   * THE HOST NATION'S POOL, SHARED. Every building on a nation's ground — its
//     state's, its champion's, every other firm's (foreign ones too) — draws on
//     that nation's pool. With no rule set the share is MECHANICAL: every claim
//     is granted the same fraction, min(1, pool / total claims) — in proportion
//     to need. A nation that SETS a rule (`set_logistics_shares`, the player's
//     verb; no AI nation sets one — no grant covers it) gives each class its
//     share of the pool; within a class claims are granted pro rata, and a
//     class's unused share spills, once, to the classes still short, in
//     proportion to what they still lack.
//   * A building on nobody's ground has no pool: it produces nothing.
//
// PURE AND UNCACHED: `resolve_logistic_points` is a function of the world's
// buildings, tiles, road levels, track and ownership, recomputed every tick.
// Every walk is in ascending id order and every field is a bounded Dijkstra
// with (cost, tile id) ordering, so the answer is replay-exact.
//
//   * A MARCH DRAWS THE SAME POOL, AND CLAIMS FIRST (BL-1352; LOGISTICS.md
//     § How a tile's points get to it, "A march draws on the nation's pool too,
//     and claims first"). Movement a player or a rival directs draws the host
//     nation's points before any producer does: the marching claims are served
//     in the march pass's own order (mobilised corps first, then ascending unit
//     id), each all-or-nothing, and PRODUCTION'S GRANTS ARE COMPUTED ON WHAT IS
//     LEFT. So guns, butter and freight compete in one currency, and a campaign
//     visibly starving its own industry is the intended, legible cost.
//
//     WHOSE POOL: THE MOVER'S. A corporation's march draws its home nation's
//     points, a nation's garrison its own - "Movement a player or rival DIRECTS"
//     (LOGISTICS.md). Never the pool of the ground it stands on: that first cut
//     immobilised a unit for good once its points ran out on unowned wilderness
//     (the refusal moves nothing, so it re-fires for ever) and funded an invasion
//     out of the invaded nation's own pool.
//
//     INTERIM, and narrower than the doc's eventual rule: there is no reach test
//     on a march, because the off-network military reach is still "military detail
//     later" (LOGISTICS.md), so a march anywhere draws its own nation's pool at
//     full strength.
//
//     BL-1352 retired the second, unrelated system this used to be: a flat
//     20/tick pool at EVERY POPULATION CENTRE (`active_lp_anchor_pools`), which
//     the marches and the Throughput lens read while this pool ran the economy.
//     That is why cities appeared to supply logistics when the doc had already
//     ruled they make none.

#include "components.hpp"
#include "world.hpp"

#include <array>
#include <map>
#include <tuple>
#include <cstddef>
#include <unordered_map>
#include <vector>

class recipe_registry;

/// Who on a nation's ground a building's claim is counted under.
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
// --- costs 1 (a land step `lp_offnet_step`), and rail counts TRACK STEPS between
// --- stations - it is point to point and takes no off-network step at all.

/// Points a source makes per tick, by network.
inline constexpr std::array<float, lp_network_count> lp_source_points = {120.0f, 150.0f, 120.0f};
/// Full-strength distance, by network.
///
/// BL-1360 (Ben, 2026-10-16; LOGISTICS.md § One pool): a better road carries points
/// further - Track 6 tiles, Road 8, Highway 10 at full strength, fading to ZERO AT
/// DOUBLE (12 / 16 / 20). The road entry stays one cost threshold, 6, and the TIER
/// is carried by the step cost below: 6 / 1.0 = 6, 6 / 0.75 = 8, 6 / 0.6 = 10 tiles.
/// Rail carries 20 track steps, delivered point to point (lp_distance_field). Sea is
/// untouched by the ruling and keeps 12 full / 8 fade - the double-distance fade is
/// the ROAD's rule, not generalised.
inline constexpr std::array<float, lp_network_count> lp_full_reach = {6.0f, 20.0f, 12.0f};
/// Fade length beyond the full-strength distance (reach 0 at full + fade). The road's
/// equals its full reach, which is what "zero at double" means on every tier.
inline constexpr std::array<float, lp_network_count> lp_fade_length = {6.0f, 8.0f, 8.0f};
/// A road step's cost by tier (index 1..3). Index 0 is "no road": off the network.
/// BL-1360: 1.0 / 0.75 / 0.6, so the tiers reach 6 / 8 / 10 tiles at full strength.
inline constexpr std::array<float, 4> lp_road_step = {3.0f, 1.0f, 0.75f, 0.6f};
/// One step off a source's network (land, for road; land, for sea). Rail takes no
/// off-network step: it delivers point to point (BL-1360).
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
    float     pool   = 0.0f;         ///< made this tick, before the march's claim
    /// BL-1352: the marching claim on this pool, and what it was granted. Production
    /// claims against `pool - march_granted`, never against `pool`.
    float     march_claim   = 0.0f;
    float     march_granted = 0.0f;
    std::array<float, lp_network_count> made{}; ///< by network
    std::array<int,   lp_network_count> sources{};
    std::array<float, lp_class_count>   claim{};
    std::array<float, lp_class_count>   granted{};
    bool      rule_set = false;
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
};

/// One marching unit's claim, handed IN by the march pass (BL-1352). The pass owns
/// both the points a unit would move on and the order claims are served in, so the
/// resolver takes them rather than deriving a second copy of either.
struct lp_march_claim
{
    entity_id unit   = null_entity;
    entity_id nation = null_entity; ///< ground the unit stands on; null = no pool
    float     need   = 0.0f;        ///< march points it would spend this tick
};

/// One marching unit's answer.
struct lp_march_row
{
    entity_id unit    = null_entity;
    entity_id nation  = null_entity;
    float     need    = 0.0f;
    float     granted = 0.0f; ///< `need` when served, 0 when refused: a march is all or nothing
};

struct lp_tick
{
    std::vector<lp_nation_row>   nations;   ///< ascending nation id
    std::vector<lp_source_row>   sources;   ///< ascending building id
    std::vector<lp_building_row> buildings; ///< ascending building id
    std::vector<lp_march_row>    marches;   ///< BL-1352, in the order claims were served
    /// building id -> fraction, for the economy step's lookup.
    std::unordered_map<entity_id, float> fraction;
    /// unit id -> points granted, for the march pass's lookup (BL-1352).
    std::unordered_map<entity_id, float> march_granted;
};

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

/// Network distance field from @p seeds on @p body for network @p n, bounded at
/// that network's full + fade: tile -> distance, only tiles within the bound.
/// Road and sea: every tile the walk reaches. RAIL (BL-1360): point to point - the
/// seed station tiles themselves (0) and the OTHER station tiles the rail network
/// joins them to (track steps), and no other tile at all.
std::unordered_map<entity_id, float> lp_distance_field(world& w, entity_id body, lp_network n,
                                                       const std::vector<entity_id>& seeds);

/// Resolve this tick's pools, claims and grants. Pure; mutates only caches.
///
/// @param marches BL-1352: the marching claims, in the order they are to be served
///        (the march pass's own order). Each is granted whole or refused, before any
///        producer claims, so production sees `pool - march_granted`. Empty (the
///        default) is a tick with nothing marching, and leaves every grant exactly
///        as it was before the march joined this pool.
lp_tick resolve_logistic_points(world& w, const std::vector<lp_march_claim>& marches = {});

/// Per-evaluation memo for `lp_coverage_at`: the host nation's source fields,
/// keyed (nation, network, body). Clear it after any source is built.
struct lp_reach_memo
{
    std::map<std::tuple<entity_id, int, entity_id>, std::unordered_map<entity_id, float>> fields;
    void clear() { fields.clear(); }
};

/// Where a tile's coverage came from: the network whose reach is best there and
/// the network distance to that network's nearest source. `reached` is false when
/// no field of the host nation reaches the tile at all (BL-1353's hover card).
struct lp_coverage_why
{
    bool       reached  = false;
    lp_network network  = lp_network::road;
    float      distance = 0.0f;
};

/// The coverage a building standing on @p tile would get from the running
/// sources of the nation whose ground it is ([0,1]; 0 on nobody's ground).
/// @param why Optional; receives the network and distance the figure came from.
float lp_coverage_at(world& w, lp_reach_memo& memo, entity_id tile,
                     lp_coverage_why* why = nullptr);

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
};

/// Place logistics hubs at the fold so every producing building on a nation's
/// ground is covered and every pool meets its need with margin. A hub stands on
/// the market-centre tile serving the first uncovered building (ascending id) —
/// or the building's own tile when that centre is off the nation's ground — owned
/// by the nation (state industry) for a major, else by the uncovered building's
/// owner. Complete and zero-staffed, as every generated holding is. Deterministic.
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

void print_lp_seed_report(const lp_seed_report& r);
