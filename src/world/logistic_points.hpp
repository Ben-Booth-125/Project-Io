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
// ARMIES (interim, until the military off-network reach lands): marching units
// keep the anchor-pool gate they have always had (`active_lp_anchor_pools`,
// logistics.hpp) — a separate interim quantity, not this pool, and no longer
// drawn by trade.

#include "components.hpp"
#include "world.hpp"

#include <array>
#include <map>
#include <tuple>
#include <cstddef>
#include <unordered_map>
#include <vector>

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

// --- Calibration (BL-1309, measured on the shipped 1960 start; see the
// --- logistic_points_harness M rows). Distances are in network-cost units: one
// --- step along the source's own network costs 1 (a road step costs
// --- `lp_road_step[tier]`), one step off it costs `lp_offnet_step`.

/// Points a source makes per tick, by network.
inline constexpr std::array<float, lp_network_count> lp_source_points = {120.0f, 150.0f, 120.0f};
/// Full-strength distance, by network.
inline constexpr std::array<float, lp_network_count> lp_full_reach = {6.0f, 10.0f, 12.0f};
/// Fade length beyond the full-strength distance (reach 0 at full + fade).
inline constexpr std::array<float, lp_network_count> lp_fade_length = {6.0f, 8.0f, 8.0f};
/// A road step's cost by tier (index 1..3). Index 0 is "no road": off the network.
inline constexpr std::array<float, 4> lp_road_step = {3.0f, 1.0f, 0.7f, 0.5f};
/// One step off a source's network (land for rail and road; land for sea).
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
    float     pool   = 0.0f;
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
    entity_id  nation   = null_entity;
    lp_network network  = lp_network::road;
};

struct lp_tick
{
    std::vector<lp_nation_row>   nations;   ///< ascending nation id
    std::vector<lp_source_row>   sources;   ///< ascending building id
    std::vector<lp_building_row> buildings; ///< ascending building id
    /// building id -> fraction, for the economy step's lookup.
    std::unordered_map<entity_id, float> fraction;
};

/// The network a building type makes points along, or false when it makes none.
bool lp_source_network(building_type t, lp_network& out);

/// True when @p b is a running source (complete, not decommissioned, not state-idle).
bool lp_source_running(const building_component& b);

/// True when @p b is a producing building that consumes points this tick.
bool lp_consumer(const building_component& b);

/// Points @p b needs at its run (0 for a non-consumer).
float lp_need(const building_component& b);

/// Reach in [0,1] of a source on network @p n at network distance @p d.
float lp_reach(lp_network n, float d);

/// Network distance field from @p seeds on @p body for network @p n, bounded at
/// that network's full + fade: tile -> distance, only tiles within the bound.
std::unordered_map<entity_id, float> lp_distance_field(world& w, entity_id body, lp_network n,
                                                       const std::vector<entity_id>& seeds);

/// Resolve this tick's pools, claims and grants. Pure; mutates only caches.
lp_tick resolve_logistic_points(world& w);

/// Per-evaluation memo for `lp_coverage_at`: the host nation's source fields,
/// keyed (nation, network, body). Clear it after any source is built.
struct lp_reach_memo
{
    std::map<std::tuple<entity_id, int, entity_id>, std::unordered_map<entity_id, float>> fields;
    void clear() { fields.clear(); }
};

/// The coverage a building standing on @p tile would get from the running
/// sources of the nation whose ground it is ([0,1]; 0 on nobody's ground).
float lp_coverage_at(world& w, lp_reach_memo& memo, entity_id tile);

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
};

/// Place logistics hubs at the fold so every producing building on a nation's
/// ground is covered and every pool meets its need with margin. A hub stands on
/// the market-centre tile serving the first uncovered building (ascending id) —
/// or the building's own tile when that centre is off the nation's ground — owned
/// by the nation (state industry) for a major, else by the uncovered building's
/// owner. Complete and zero-staffed, as every generated holding is. Deterministic.
lp_seed_report seed_logistics_sources(world& w);

void print_lp_seed_report(const lp_seed_report& r);
