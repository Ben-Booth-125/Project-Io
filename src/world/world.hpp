#pragma once

#include "components.hpp"
#include "corp_command.hpp" // corp_decision_ring (BL-202 strategic decision log)

#include <cstdint>
#include <map>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

/// The system's single asteroid belt — a band between two orbital radii. The
/// belt is not a body (it owns no entity); it is rendered as a thick, translucent
/// textured ring on the Solar canvas, with notable asteroids sitting within it as
/// separate, selectable body entities drawn over the band. A system with no belt
/// has outer_radius_au <= inner_radius_au.
struct asteroid_belt
{
    float inner_radius_au = 0.0f; ///< Inner edge of the band, AU from the star.
    float outer_radius_au = 0.0f; ///< Outer edge of the band, AU from the star.

    /// Whether the system has a belt to draw.
    /// @return True when the band has positive width.
    bool present() const { return outer_radius_au > inner_radius_au && outer_radius_au > 0.0f; }
};

/// Result of an intra-body pathfind (BL-077): the terrain-weighted path cost, whether the
/// cheapest path crosses ocean (=> sea mode, else land), and whether the endpoints connect.
/// Symmetric in its endpoints (edge cost is the average of the two tiles), so it caches under
/// a canonicalised (body, lo_tile, hi_tile) key.
struct logistics_path
{
    float cost          = 0.0f;
    bool  crosses_ocean = false;
    bool  reachable     = false;
    /// The tile sequence of the best path, in canonical (lo→hi) endpoint order —
    /// i.e. from min(src,dst) to max(src,dst), since the weighted path is symmetric
    /// and cached on the unordered pair. A caller that dispatched src→dst reverses
    /// this when src != lo. Empty when unreachable; a single tile when src == dst.
    /// Populated by intra_body_path (BL-152, for the convoy vision beam); the cost
    /// fields above stand alone for callers that ignore it.
    std::vector<entity_id> tiles;
};

/// ECS registry. Entities are plain integer IDs; components are stored in
/// per-type maps. The registry owns all component data for the lifetime of
/// the simulation.
struct world
{
    // --- entity management ---

    /// Allocate a new entity ID. IDs increment monotonically and are never
    /// reused within a session.
    ///
    /// @return A unique, non-zero entity_id.
    entity_id create_entity();

    // --- well-known entities ---

    /// The player's corporation entity. Set by world construction; used by
    /// budget and unit ownership in later layers. No component is attached
    /// yet — the ID alone is sufficient until the budget layer is reached.
    entity_id player_entity = null_entity;

    /// The system's central star (a body entity of type body_type::star). Drawn
    /// at the Solar canvas centre; its name titles the minimap when the Solar
    /// canvas is shown there.
    entity_id star_body = null_entity;

    /// The corporation's home planet. The game opens on this body's surface.
    entity_id home_body = null_entity;

    /// Current sim day tick, mirrored from the sim loop each frame by app. A derived
    /// convenience so read-only UI surfaces can age trade routes for the activity fog
    /// (body_activity_visibility, BL-089) without threading the tick through every
    /// render signature. Not authoritative sim state and not serialised.
    int current_day_tick = 0;

    /// The system's asteroid belt (a band, not a body). belt.present() is false
    /// when the system has no belt.
    asteroid_belt belt;

    // --- component stores ---
    std::unordered_map<entity_id, body_component>      bodies;
    std::unordered_map<entity_id, tile_component>      tiles;
    std::unordered_map<entity_id, building_component>  buildings;
    std::unordered_map<entity_id, stockpile_component> stockpiles;
    std::unordered_map<entity_id, market_component>    markets;
    std::unordered_map<entity_id, unit_component>             units;

    /// Population centre entities keyed by their entity ID. Populated by
    /// generate_population_centres() after tile generation; empty until that
    /// call is made for a body. No AI behaviour in the prototype.
    std::unordered_map<entity_id, population_centre_component> population_centres;

    /// Maps a population centre entity ID to the tile entity it occupies.
    /// Written alongside population_centres by generate_population_centres().
    std::unordered_map<entity_id, entity_id>                   population_centre_tile;

    /// Procedural city name per population centre — the human-readable identity used
    /// by the market ledger's market/city selector and the CSV export. Assigned by
    /// generate_population_centres() from an INDEPENDENT seeded stream (so it does not
    /// perturb world generation). A market's city name resolves via its centre_tile.
    std::unordered_map<entity_id, std::string>                 population_centre_name;

    /// Nation entities keyed by their entity ID. Populated by generate_nations()
    /// after tile generation; empty until that call is made for a body.
    std::unordered_map<entity_id, nation_component>    nations;

    /// Maps a tile entity ID to the nation entity ID that owns it.
    /// Absent entries are unclaimed (ocean tiles and bodies without nation generation).
    /// Written by generate_nations() alongside the nation_component.tiles list.
    std::unordered_map<entity_id, entity_id>           tile_to_nation;

    /// Corporation entities keyed by their entity ID. Populated by
    /// generate_corporations() after nation generation; empty until that call
    /// is made. Exactly one entry will have corporation_component::is_player == true,
    /// and world::player_entity will equal that entry's key.
    std::unordered_map<entity_id, corporation_component> corporations;

    /// Shared stockpile pool keyed by (corporation, body). This is the Layer 3
    /// economy's working store — extraction and processing credit/draw it, the
    /// market lists surplus from it. A `std::map` (not unordered) so iteration is
    /// deterministic, mirroring the `tile_to_nation` design rationale: keeping the
    /// pool here off `building_component`/`body_component` lets the economy systems
    /// stay on disjoint files. The per-building `stockpile_component` is unused in L3.
    std::map<std::pair<entity_id, entity_id>, stockpile_component> corp_body_pools;

    /// Active convoys — goods in transit. Appended by dispatch_convoys, advanced by
    /// advance_convoys, and retired (erased) by credit_arrived_convoys in
    /// supply_system.hpp. A std::vector (not a map) because convoys have no persistent
    /// entity ID — they are identified by index while in flight. Order is
    /// dispatch-time insertion; stable between ticks.
    std::vector<convoy_component> convoys;

    /// Persistent trade routes — durable body-pair lanes a corporation's commerce
    /// has run (BL-088). Upserted by credit_arrived_convoys when a convoy completes
    /// a lane; never erased (aging to 'stale' is a read-time concern owned by the
    /// commercial-sphere fog, BL-089). A std::vector mirroring `convoys`; insertion
    /// order, stable between ticks. Joins the flat-binary serialisation seam
    /// symmetrically when that path exists (none is wired in world/* yet).
    std::vector<trade_route> trade_routes;

    /// Proximity-glimpse stamps (BL-099) — the sim day tick at which a player convoy
    /// last passed within `glimpse_radius_au_default` (AU) of this body while completing
    /// an inter-body lane. Sampled once at the discrete completion tick by
    /// record_proximity_glimpses (orbits have already advanced for that frame), so no
    /// past position is ever reconstructed — the fog reads the stamp, never recomputes
    /// geometry. Held off `body_component` (the `corp_body_pools` rationale) to keep the
    /// body's future flat-binary layout untouched. std::map for deterministic iteration.
    std::map<entity_id, int> body_last_glimpse_tick;

    /// Lazily-built per-body raster index (grid_y*grid_width + grid_x -> tile entity, or
    /// null_entity for an absent cell), for O(1) neighbour lookup in intra-body pathfinding
    /// (BL-077). A derived cache, not authored state: built on first use by body_tile_grid(),
    /// a pure function of the body's tiles (independent of tiles-map iteration order).
    std::unordered_map<entity_id, std::vector<entity_id>> body_tile_index;

    /// Route-cost cache for intra-body A* (BL-077), keyed by (body, lo_tile, hi_tile) with the
    /// tile pair canonicalised (the weighted path is symmetric). A derived cache; invalidated
    /// when road_level changes (road placement, BL-147). Keeps per-Tick per-lane A* off the
    /// dispatch hot path.
    std::map<std::tuple<entity_id, entity_id, entity_id>, logistics_path> astar_cost_cache;

    /// Strategic AI decision log (BL-202): a fixed 256-entry ring of the most
    /// recent corp commands + score rationale, in deterministic application
    /// order. Derived observability (the chat feed / harness read it), not
    /// save-format state — it does not join the serialisation seam.
    corp_decision_ring ai_decisions;

    /// Stockpile pool for a (corporation, body) pair, inserting an empty pool on
    /// first access. The single point through which the economy systems read and
    /// write the shared pool.
    ///
    /// @param corp Corporation entity id.
    /// @param body Body entity id.
    /// @return     Reference to the (corp, body) stockpile, created if absent.
    stockpile_component& pool_for(entity_id corp, entity_id body)
    {
        return corp_body_pools[std::make_pair(corp, body)];
    }

    /// Authored effective workforce supply per (corp, body) — Layer 4 step 1 of the
    /// labour-pool model (docs/economy/POPULATION.md § Workforce model). Absent
    /// entries fall back to `default_workforce_supply`; population centres replace
    /// this authored value with a population-derived figure in step 2. Held off the
    /// component structs (the `corp_body_pools` rationale) so the economy stays on
    /// disjoint files.
    static constexpr float default_workforce_supply = 3.0f;
    std::map<std::pair<entity_id, entity_id>, float> workforce_supply_overrides;

    /// Background-industry aggregate per (nation, body). Populated once at world-gen;
    /// injected into market supply/demand each tick by inject_substrate_demand.
    /// Uses std::map for deterministic iteration (same rationale as corp_body_pools).
    std::map<std::pair<entity_id, entity_id>, nation_substrate> nation_substrates;

    /// Effective workforce available to `corp` on `body` this tick. The labour the
    /// corporation's buildings on that body contend for under the pool model.
    ///
    /// @param corp Corporation entity id.
    /// @param body Body entity id.
    /// @return     Authored supply if present, else `default_workforce_supply`.
    float workforce_supply(entity_id corp, entity_id body) const
    {
        const auto it = workforce_supply_overrides.find(std::make_pair(corp, body));
        return (it != workforce_supply_overrides.end()) ? it->second : default_workforce_supply;
    }

    /// Tick-boundary state hash (BL-204): an FNV-1a checksum over a deterministic
    /// canonicalisation of the econ-tick snapshot — every corporation's balance,
    /// every building's dial state (workforce target/assigned, recipe, decommissioned,
    /// ticks_remaining), every market's resolved price array, and every corp/body
    /// stockpile pool. Sorted by entity id (map/unordered_map iteration order is not
    /// itself trusted) so two structurally-identical worlds hash identically
    /// regardless of container internals.
    ///
    /// Two roles, one function: (1) today, a same-seed-two-runs regression primitive
    /// for the AI skill harness (BL-204) — a divergence flags a determinism leak in
    /// the corp-AI seam; (2) later, the lockstep desync detector floated in
    /// MULTIPLAYER_PRINCIPLES.md — a remote peer's hash mismatch at a tick boundary
    /// is the desync signal. `tick` is folded in so a hash is tick-scoped (comparing
    /// hashes across different ticks is meaningless by construction).
    ///
    /// @param tick The sim day tick this snapshot is taken at (folded into the hash).
    /// @return An FNV-1a 64-bit checksum of the canonicalised snapshot.
    uint64_t state_hash(int tick) const;

private:
    uint32_t m_next_id = 1; ///< Zero is null_entity; live IDs start at 1.
};

// ---------------------------------------------------------------------------
// Ownership accessors (BL-068 — competitor information asymmetry)
// ---------------------------------------------------------------------------
// Ownership lives in `corporation_component::assets` (the forward list of owned
// buildings); there is no reverse index, so these scan the ~dozen corporations.
// Read-side only — no stored backing, no tick, no determinism impact. A reverse
// `building_to_corp` map is the natural O(1) backing if profiling ever warrants;
// the accessor is the stable seam either way.

/// Resolve the corporation that owns @p building by scanning each corporation's
/// `assets`. Siblings of `pool_for` / `workforce_supply`.
///
/// @param w        Read-only world state.
/// @param building Building entity id to resolve.
/// @return         Owning corporation id, or `null_entity` if unowned.
entity_id owner_corp_of(const world& w, entity_id building);

/// True iff @p building is owned by the player's corporation. The single branch
/// point for the visibility rule (BL-068): everything not player-owned is treated
/// uniformly as a rival.
///
/// @param w        Read-only world state.
/// @param building Building entity id to test.
/// @return         Whether the owning corporation has `is_player` set.
bool is_player_owned(const world& w, entity_id building);

/// Resolve the body a market entity sits on. Collapses market-level identity to
/// body-level for the trade-route / fog systems (a body may host several markets;
/// all share one lane for visibility). Sibling of `owner_corp_of`.
///
/// @param w      Read-only world state.
/// @param market Market entity id to resolve.
/// @return       Owning body id, or `null_entity` if the market is unknown.
entity_id body_of_market(const world& w, entity_id market);

// ---------------------------------------------------------------------------
// Commercial-sphere activity fog (BL-089)
// ---------------------------------------------------------------------------
// The player's trade network is their intelligence network: where their goods
// flow, the world lights up. A body-level *activity* fog, independent of the
// geographic survey fog (BL-067) — a body can be Known (a route reaches it) yet
// unsurveyed. Derived on demand from trade_routes (BL-088) + live convoys +
// ownership + the current tick; nothing new is stored or serialised.

/// Activity visibility tier of a body, from the player's commercial reach.
enum class activity_vis : uint8_t
{
    unknown,      ///< Outside the player's network: a public astronomy dot only.
    known_stale,  ///< A player route once reached it, but traffic has gone cold.
    known,        ///< A fresh player route reaches it: a coarse market pulse reads.
    visible,      ///< A live player lane touches it, or the player owns a building there.
};

/// Freshness window in sim day ticks: a route whose last completion is within this
/// many ticks of "now" reads as `known`; older reads as `known_stale`. One quarter
/// (90 days) by default — a calibration constant (headless-tuned).
inline constexpr int route_fresh_ticks_default = 90;

/// Proximity-glimpse corridor half-width in AU (BL-099): a body whose closest approach
/// to a completed player lane's endpoint->endpoint segment is within this distance gets a
/// faint glimpse. A calibration constant (headless-tuned) — set so a frontier body one
/// hop off a major lane glimpses while distant bodies do not.
inline constexpr float glimpse_radius_au_default = 0.25f;

/// Freshness window in sim day ticks for a proximity glimpse (BL-099): a body glimpsed
/// within this many ticks of "now" reads as `known_stale`; older decays back to
/// `unknown`. A glimpse is fainter than a route, so it never rises to `known`/`visible`.
inline constexpr int glimpse_fresh_ticks_default = 90;

/// Body-level activity visibility for the player, derived from routes + live convoys
/// + ownership + the current tick. Pure and deterministic; no stored state.
/// `home_body` and any body the player owns a building on are always `visible`.
/// Independent of survey phase (a surveyed-but-unrouted body is still `unknown` for
/// activity; an unsurveyed-but-routed body is `known`).
///
/// @param w                 Read-only world state.
/// @param body              Body to classify.
/// @param now_tick          Current sim day tick (for route freshness).
/// @param route_fresh_ticks Freshness window; defaults to route_fresh_ticks_default.
/// @return                  The body's activity tier for the player.
activity_vis body_activity_visibility(const world& w, entity_id body, int now_tick,
                                      int route_fresh_ticks   = route_fresh_ticks_default,
                                      int glimpse_fresh_ticks = glimpse_fresh_ticks_default);

// ---------------------------------------------------------------------------
// Proximity-glimpse peek (BL-099)
// ---------------------------------------------------------------------------
// The third illumination geometry over the activity fog (after endpoints + corridors):
// a body a player convoy merely passes NEAR (not an endpoint) on a completed lane gets a
// faint, decaying glimpse. Deterministic by sample-and-store — body positions are mutated
// state (orbital_angle_rad advances per frame), NOT a pure function of tick, so they
// cannot be reconstructed at a later read; instead the closest-approach set is sampled
// once at the discrete completion tick and the glimpse tick is stored. No per-frame
// proximity test, no orbital-drift flicker, no RNG.

/// Closest approach (AU) of `body`'s current position to the lane's endpoint->endpoint
/// line segment, using the flat orbital-plane projection the sim uses (r*cos(theta),
/// r*sin(theta)). Pure read; factored out so the headless harness can assert the geometry
/// directly. Returns a large sentinel if `body` is an endpoint or any id is unknown.
float body_closest_approach_au(const world& w, entity_id body, entity_id lane_a, entity_id lane_b);

/// Sample every body's closest approach to the just-completed player lane (lane_a, lane_b)
/// and stamp a proximity glimpse (body_last_glimpse_tick[body] = tick) on any body within
/// `radius_au` that is neither an endpoint nor the star. Called from credit_arrived_convoys
/// at the discrete completion tick, when orbits have already advanced for the frame — so the
/// sampled positions ARE the completion-tick positions.
void record_proximity_glimpses(world& w, entity_id lane_a, entity_id lane_b, int tick,
                               float radius_au = glimpse_radius_au_default);
