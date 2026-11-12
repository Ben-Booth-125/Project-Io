#pragma once

#include "entity.hpp"
#include "orbital_node.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

struct world;

// ---------------------------------------------------------------------------
// Space assets (BL-1327) — the data model and the one legal verb
// ---------------------------------------------------------------------------
// docs/economy/SPACE_ASSETS.md § Era 1 near space (Ben, 2026-10-15). A space
// asset is a NATION's owned object beyond the planetary surface: launched and
// paid for from its treasury, placed after a travel time, kept by a quarterly
// upkeep, and — unpaid — decaying and then lost. Corporations supply the goods
// through the ordinary market; they never own one in Era 1.
//
// BL-1581 (SPACE_ASSETS.md § The orbital graph, § Orbital slots): A DESTINATION
// IS AN ORBITAL NODE (orbital_node.hpp). Nothing finer exists: no altitude, no
// inclination. A launch names a node; its PRICE and TRAVEL TIME come from the
// cheapest `orbital_path` from the owner's home SURFACE to it (thrust cost ->
// credits and propellant; days -> quarters). The legacy (body, slot) pair is
// kept on the asset, DERIVED from the node (surface node -> surface slot, any
// other -> orbit), because the v51 save section and the station cross-checks
// read it. The REACH tier survives only as the travel-time FLOOR — derived,
// never stored — so nothing arrives faster than it did before the graph:
//
//   home_orbit    any orbital node of the owner's home body
//   moon_orbit    an orbital node of a body whose parent IS the home body
//   moon_surface  that body's surface node
//   far_orbit     any other body's orbital node   (+ a per-AU floor term)
//   far_surface   any other body's surface node   (+ a per-AU floor term)
//
// WHICH CLASS MAY GO WHERE (space_launch_legal_node): a satellite or a station
// core any orbital node; a rover a surface node off the home body; a crewed
// mission that, or any orbital node; a station module or truss any orbital node
// (the station layer sends it to its station's node and nowhere else). The home
// surface is never a destination; the star carries no node.
//
// SLOTS. A satellite or a station core, in transit or in place, HOLDS one slot
// of its node (`orbital_slots_held`); a lost one frees it at once; a launch to a
// full node is refused whole. Modules ride inside their core's slot.
//
// BL-1582 (SPACE_ASSETS.md § Launching): THE LAUNCH GATE AND ITS GOODS. A launch
// is refused whole unless the nation holds `space_dispatch` (Rocketry), owns a
// STAFFED Launchpad on its home body (`space_has_staffed_pad`), and holds the
// class's key (a satellite's role key; the station core's key — the module and
// upgrade keys are the station layer's). It BUYS `spacecraft_components` (a lot
// per class) and `propellant` (per class, per Δv of the path) off the HOME
// market's shelf (`space_home_market`) at its posted price, under the
// fair-price ceiling; the treasury pays the goods' value plus the class's base
// credits plus the path's thrust credits. A shelf that cannot fill the whole
// lot, or a treasury that cannot pay the whole sum, refuses the launch whole.
// The goods leave the market's real inventory: the launch consumes them.
//
// WHAT IS NOT HERE, deliberately: any scorer that chooses to launch
// (AI_OPPONENT.md § 11 — no grant exists for it) and any UI. The nation-level
// command seam over these verbs is space_command.hpp.
//
// DETERMINISM. `world::space_assets` is a std::map: the quarterly step walks it
// ascending id with no sort. Costs are float arithmetic over static inputs
// (authored params, a body's fixed orbital radius); no RNG, no clock.
// ---------------------------------------------------------------------------

/// The Era 1 classes (SPACE_ASSETS.md). Launch infrastructure is a BUILDING,
/// not a space asset, so it is not a class here. Append-only: the value is
/// written to the save.
enum class space_asset_class : uint8_t
{
    satellite      = 0, ///< Uncrewed orbital instrument.
    rover          = 1, ///< Uncrewed surface unit landed on another body.
    crewed_mission = 2, ///< A crew sent to a body (return is not modelled here).
    station_module = 3, ///< One launch of a crewed orbital platform: a module or a truss (stations.hpp).
    station_core   = 4, ///< BL-1583: a station's core — opens the grid (stations.hpp). Saved from v81.
};
inline constexpr std::size_t space_asset_class_count = 5;

/// BL-1583 (SPACE_ASSETS.md § Space stations): what a `station_module` asset
/// is. The roster's thirteen fittable kinds, then `truss` — an upgrade's segment,
/// which rides in no slot. Append-only: the value is written to the save (v81).
/// The roster's dials are `station_params` (stations.hpp).
enum class station_module_kind : uint8_t
{
    research             = 0,
    defence              = 1,
    power                = 2,
    dock                 = 3, ///< Edge only.
    habitation           = 4,
    life_support         = 5,
    fuel_depot           = 6,
    shipyard             = 7, ///< Edge only.
    microgravity_factory = 8,
    observatory          = 9,
    comms_relay          = 10,
    cargo_hold           = 11,
    market               = 12,
    truss                = 13, ///< An upgrade's truss segment: no slot, never fitted.
};
/// Every kind, truss included (the dial table's width).
inline constexpr std::size_t station_module_kind_count = 14;
/// The kinds a slot may hold: every kind before `truss`.
inline constexpr std::size_t station_fittable_kind_count = 13;
/// "No module": a core, a non-station asset, or an empty template slot.
inline constexpr uint8_t k_no_station_module = 0xFF;

/// Which slot of a body a destination names. Append-only (saved).
enum class space_slot : uint8_t
{
    orbit   = 0,
    surface = 1,
};

/// A destination: a named slot on a body. `body` is any body entity; the reach
/// tier is derived from it (`space_reach_of`).
struct space_destination
{
    entity_id  body = null_entity;
    space_slot slot = space_slot::orbit;
};

/// The reach tier — DERIVED from (owner's home body, destination), never stored.
enum class space_reach : uint8_t
{
    home_orbit   = 0,
    moon_orbit   = 1,
    moon_surface = 2,
    far_orbit    = 3,
    far_surface  = 4,
};
inline constexpr std::size_t space_reach_count = 5;

/// Lifecycle. Append-only (saved).
enum class space_asset_state : uint8_t
{
    in_transit = 0, ///< Launched; arrives at `arrival_tick`.
    active     = 1, ///< In place, upkeep paid.
    decaying   = 2, ///< In place, upkeep unpaid for `unpaid_quarters` (>= 1) econ ticks (months, BL-1642).
    lost       = 3, ///< Gone. Kept as a record; never steps again.
};

/// One space asset. SERIALISED (world_save.cpp, the v51 trailing section).
/// A satellite's role (SPACE_ASSETS.md § Satellites). Append-only (saved).
enum class space_satellite_role : uint8_t
{
    none        = 0, ///< Not a satellite.
    comms       = 1,
    navigation  = 2,
    observation = 3,
    survey      = 4,
};
inline constexpr std::size_t space_satellite_role_count = 5;

struct space_asset
{
    entity_id         id    = null_entity; ///< From the world's entity allocator.
    entity_id         owner = null_entity; ///< A NATION entity.
    space_asset_class cls   = space_asset_class::satellite;
    space_destination dest{};
    space_asset_state state = space_asset_state::in_transit;
    int32_t           launch_tick  = 0;    ///< Econ tick (month) of launch.
    int32_t           arrival_tick = 0;    ///< Econ tick it becomes active; >= launch_tick.
    float             upkeep       = 0.0f; ///< Credits per econ tick (a month), fixed at launch.
    int32_t           unpaid_quarters = 0; ///< Consecutive unpaid econ ticks (months); 0 when paid.
    float             launch_cost  = 0.0f; ///< Credits the launch debited (the quote), for the tick's report.

    // ---- BL-1583 / BL-1585: the station fields (stations.hpp). SERIALISED in
    // the v81 station section, only for an asset where any differs from these
    // defaults; folded into state_hash in the station block.
    entity_id station      = null_entity; ///< The station this core/module/truss belongs to (its core's id).
    int16_t   station_slot = -1;          ///< Grid slot index (row * width + col); -1 for a truss.
    uint8_t   module       = k_no_station_module; ///< A station_module's `station_module_kind`.
    int32_t   crew         = 0;           ///< Astronauts aboard, drawn from the pool at launch (BL-1585).
    int32_t   refit_done_tick = -1;       ///< A refit's first working econ tick; -1 when not refitting.

    // ---- BL-1581 / BL-1586: where it sits on the orbital graph, and a
    // satellite's role. SERIALISED in the v82 node section (world_save.cpp);
    // folded into state_hash with the asset. `dest` is derived from `node`.
    orbital_node node{};                          ///< The orbital node it occupies (or travels to).
    space_satellite_role role = space_satellite_role::none; ///< Satellites only; none otherwise.

    // ---- BL-1598 (SPACE_ASSETS.md § Debris): a satellite's or station core's
    // debris WEAR, in [0, 1); lost at 1 (debris.hpp). SERIALISED in the BL-1598
    // section only where above zero; folded into state_hash there.
    float wear = 0.0f;

    // ---- BL-1614 (CIRCUMPLANETARY.md, the transit segment): where an IN-TRANSIT
    // asset left from and the econ tick it left, so the canvas can draw the path
    // and the share travelled. A launch records the home surface and its launch
    // tick; a move the node it left and the tick of the order. Cleared (body
    // null, tick 0) when the asset arrives — only a travelling asset carries it.
    // SERIALISED in the BL-1614 section only where `transit_from.body` is set;
    // folded into state_hash there.
    orbital_node transit_from{};      ///< Origin of the current transit; body null when none.
    int32_t      depart_tick = 0;     ///< Econ tick the current transit left; in [launch_tick, arrival_tick].
};

/// BL-1614: does @p a carry a recorded transit origin?
inline bool space_asset_has_transit_origin(const space_asset& a)
{
    return a.transit_from.body != null_entity;
}

/// The hard ceiling on `space_asset_params::quarters_to_loss`. The step clamps
/// the dial into [1, this]; the save reader, which cannot see the registry,
/// validates an asset's unpaid count against it.
/// Counted in econ ticks (months): 192, the 64 quarters (BL-1642; the name is kept).
inline constexpr int32_t k_space_quarters_to_loss_ceiling = 192;

//// The dials. Defaults are the BL-1327 first cut, re-cut for the graph and the
/// goods by BL-1581/1582 (in-band; see that report). Credits are the
/// treasury's units. Not yet a Lua key: the registry carries these defaults
/// (`recipe_registry::space_assets`).
struct space_asset_params
{
    /// Per class: the payload's BASE credits, before thrust and goods.
    /// BL-1583: the station core (index 4) is priced above a module — it opens the grid.
    std::array<float, space_asset_class_count> launch_cost{10000.0f, 20000.0f, 60000.0f, 40000.0f, 50000.0f};

    /// Per class: upkeep a tick (a month), fixed onto the asset at launch. A third of
    /// the quarter tick's {500, 1000, 3000, 2000, 2500} (BL-1642).
    std::array<float, space_asset_class_count> upkeep{500.0f / 3.0f, 1000.0f / 3.0f, 1000.0f,
                                                      2000.0f / 3.0f, 2500.0f / 3.0f};

    /// BL-1581: credits per Δv unit of the path (the thrust cost's own price,
    /// beside the propellant it burns). Home low orbit climbs ~10 Δv.
    float credits_per_dv = 250.0f;

    /// BL-1582: `spacecraft_components` per launch, by class (the payload).
    std::array<float, space_asset_class_count> components_lot{25.0f, 40.0f, 60.0f, 30.0f, 50.0f};

    /// BL-1582: `propellant` per Δv unit of the path, by class (a heavier
    /// payload burns more). A satellite to home low orbit (~10 Δv) burns 50 —
    /// the BL-644 purchase lump's propellant.
    std::array<float, space_asset_class_count> propellant_per_dv{5.0f, 8.0f, 12.0f, 6.0f, 10.0f};

    /// BL-1581: in-game days per quarter, for the path's days -> quarters.
    /// The path's quarters are its days over this, ROUNDED DOWN, then held to
    /// the reach floor below — so a home orbit is still reached the quarter it
    /// launches, and nothing arrives sooner than before the graph.
    /// BL-1642: the tick is a month, so this is 30 days per TICK (the "quarters" in
    /// these names count econ ticks, now months).
    float days_per_quarter = 30.0f;

    /// Per reach tier: the travel-time FLOOR in ticks (further = slower). Months:
    /// three times the quarter tick's {0, 1, 1, 2, 3} (BL-1642).
    std::array<int32_t, space_reach_count> travel_quarters{0, 3, 3, 6, 9};

    /// Far tiers only: extra FLOOR ticks per AU of heliocentric separation
    /// between the home body's system position and the target's, floored.
    /// 3 months, the quarter (BL-1642).
    float far_quarters_per_au = 3.0f;

    /// Consecutive unpaid quarters at which a decaying asset is LOST. The first
    /// unpaid quarter makes it decaying; a paid quarter restores it to active.
    /// Clamped into [1, k_space_quarters_to_loss_ceiling] where it is read.
    int32_t quarters_to_loss = 12; // months: a year, the 4 quarters (BL-1642)

    /// Far tiers only: the AU-derived extra quarters are clamped to this before
    /// the integer cast, so no separation can overflow a tick.
    int32_t max_extra_quarters = 1200; // months (BL-1642; was 400 quarters)

    /// BL-1582: the fair-price ceiling (`price_band_params::reservation_mult`,
    /// BL-1172: it governs every goods draw). NOT a space dial: the command seam
    /// (space_command.cpp) copies the registry's value in; 0 = off, which for a
    /// launch means "buy whatever the shelf holds" (`shelf_admits`, off_buys).
    float reservation_mult = 0.0f;
};

/// BL-1581: may @p cls occupy node kind @p k of a body, given whether that body
/// is the owner's home? The class/node legality table (see the file comment).
bool space_launch_legal_node(space_asset_class cls, orbital_node_kind k, bool home_body);

/// The home-independent half of the table: may @p cls ever occupy @p slot?
/// satellite and station_module are orbit-only, rover surface-only, a crewed
/// mission either. The save reader holds every stored asset to this.
bool space_slot_legal(space_asset_class cls, space_slot slot);

/// The legacy slot a node names: surface for the surface node, else orbit.
inline space_slot space_slot_of(orbital_node_kind k)
{
    return k == orbital_node_kind::surface ? space_slot::surface : space_slot::orbit;
}

/// Does @p cls hold a slot of its node? Satellites and station cores do.
inline bool space_class_holds_slot(space_asset_class cls)
{
    return cls == space_asset_class::satellite || cls == space_asset_class::station_core;
}

/// BL-1581: the slots of @p n held now — every satellite and station core at
/// it that is not lost (in transit to it, or in place). Pure.
int orbital_slots_held(const world& w, orbital_node n);

/// BL-1582: does @p nation own a STAFFED Launchpad on @p body — completed, not
/// decommissioned, not idle, workforce assigned, not on an occupied tile (the
/// astronaut-centre rule)? Its state buildings, walked ascending.
bool space_has_staffed_pad(const world& w, entity_id nation, entity_id body);

/// BL-1582: the market a nation's launch buys off: its capital tile's market
/// when that stands on the home body, else the lowest-id market on the home
/// body. `null_entity` when the home body has none.
entity_id space_home_market(const world& w, entity_id nation);

/// Outcome of a launch, or of a quote. Not saved (only reported).
enum class space_launch_result : uint8_t
{
    launched = 0,
    rejected_invalid,              ///< Class, node kind or role out of range; non-finite cost.
    rejected_unknown_nation,       ///< `nation` names no nation.
    rejected_unknown_body,         ///< The destination body does not exist, or the owner has no home body.
    rejected_illegal_destination,  ///< The star, the home surface, or a class/node pair the table refuses.
    rejected_insufficient_treasury,///< The nation's treasury cannot pay the whole cost.
    // ---- BL-1581 / BL-1582, appended ----
    rejected_tech_locked,          ///< Rocketry, the role's key or the core's key is not held.
    rejected_no_launchpad,         ///< No staffed Launchpad of the nation's on its home body.
    rejected_no_slot,              ///< The node's slots are all held.
    rejected_no_route,             ///< No orbital path from the home surface reaches the node.
    rejected_shelf,                ///< The home market's shelf cannot fill the whole goods lot.
    // ---- BL-1599, appended ----
    rejected_blockaded,            ///< The home body is blockaded (space_bombard.hpp): no launch reaches orbit.
};

struct space_launch_quote
{
    space_launch_result result = space_launch_result::rejected_invalid;
    space_reach         reach  = space_reach::home_orbit;
    float               cost   = 0.0f;  ///< Credits, the whole launch: credits + goods_value.
    float               credits = 0.0f; ///< Base credits + thrust credits.
    float               goods_value = 0.0f; ///< components x price + propellant x price.
    float               components  = 0.0f; ///< `spacecraft_components` the launch buys.
    float               propellant  = 0.0f; ///< `propellant` the launch buys.
    entity_id           market = null_entity; ///< The shelf the goods come off.
    double              dv     = 0.0;   ///< The path's thrust cost.
    double              days   = 0.0;   ///< The path's days.
    int32_t             travel_quarters = 0;
    entity_id           asset  = null_entity; ///< Set by a successful launch only.
};

/// A body's distance from the star in AU: its own orbital radius if it orbits
/// the star, else its top-level ancestor's (a moon reads its planet's). 0 for an
/// unknown body. Bounded walk. (BL-1583: the station's solar falloff reads it.)
float space_heliocentric_au(const world& w, entity_id body);

/// The owner's home body: its capital tile's body, else `w.home_body`.
/// `null_entity` when neither resolves.
entity_id space_home_body_of(const world& w, entity_id nation);

/// BL-1630 (SPACE_ASSETS.md § Launching, "A staffed pad bids for its launch"):
/// the standing lot a launch-ready nation bids for on its home market — one
/// satellite's goods to its home low orbit: `components_lot[satellite]`
/// `spacecraft_components` and `propellant_per_dv[satellite]` x the path's Δv
/// of `propellant`, at @p market (`space_home_market`). True only for a nation
/// holding Rocketry with a staffed Launchpad on its home body, a home market and
/// a path to its low orbit; false, outputs zeroed, otherwise. Pure.
bool space_standing_lot(const world& w, const space_asset_params& p, entity_id nation,
                        entity_id& market, float& components, float& propellant);

/// PURE. Price @p cls to @p node for @p nation and say whether it is legal,
/// gated open, slotted, stocked and affordable — `result` is what
/// `launch_space_asset` would return. @p role is a satellite's role (required,
/// not `none`) and `none` for every other class. Reads only. Checked in this
/// order, so each refusal is reachable alone: invalid, nation, body, legality,
/// technology, launchpad, slot, route, shelf, treasury.
space_launch_quote quote_space_launch(const world& w, const space_asset_params& p,
                                      entity_id nation, space_asset_class cls,
                                      orbital_node node,
                                      space_satellite_role role = space_satellite_role::none);

/// THE VERB. Launch @p cls to @p node for @p nation: the goods leave the home
/// shelf, the whole cost leaves the treasury. Rejected WHOLE — the world is not
/// touched, not even the entity allocator — when the quote is not `launched`.
///
/// THE TICK CONTRACT. The launch tick is `w.current_econ_tick`, read from the
/// world and never passed in: the econ tick ABOUT TO RUN, which every driver
/// mirrors onto the world before stepping it. The asset is stored `in_transit`
/// with `arrival_tick = launch_tick + travel_quarters`, and the nation step of
/// that same tick runs `run_space_asset_step` — so travel 0 is active (and pays
/// its first upkeep) in the launch quarter, travel 1 in the next, and each
/// further quarter of travel is exactly one more step.
space_launch_quote launch_space_asset(world& w, const space_asset_params& p,
                                      entity_id nation, space_asset_class cls,
                                      orbital_node node,
                                      space_satellite_role role = space_satellite_role::none);

/// Outcome of a satellite order (retask / move). Not saved.
enum class satellite_order_result : uint8_t
{
    ok = 0,
    rejected_invalid,       ///< A role or node out of range; not a satellite.
    rejected_unknown_nation,
    rejected_unknown_asset,
    rejected_not_owner,
    rejected_state,         ///< Lost; already that role / node; a move while not in place.
    rejected_tech_locked,   ///< The role's key is not held.
    rejected_launch,        ///< move: the move's quote refused it (see `launch`).
};

struct satellite_order
{
    satellite_order_result result = satellite_order_result::rejected_invalid;
    space_launch_result    launch = space_launch_result::rejected_invalid; ///< move only.
    float                  cost   = 0.0f; ///< move: credits debited (thrust + propellant).
};

/// Retask satellite @p asset to @p role (not `none`): the role's key must be
/// held. Free; takes effect at once. Rejected whole otherwise.
satellite_order retask_satellite(world& w, entity_id nation, entity_id asset, space_satellite_role role);

/// PURE: what `move_satellite` would do. A move is a path from where the
/// satellite SITS (SPACE_ASSETS.md § The orbital graph): it buys the path's
/// propellant off the home shelf and pays its thrust credits (no base credits,
/// no components — nothing new is built), needs a free slot at @p to and
/// Rocketry (no Launchpad: nothing leaves the ground), and puts the satellite
/// in transit to @p to for the path's quarters (no reach floor: a move is not a
/// launch). Its old slot frees at once.
satellite_order quote_move_satellite(const world& w, const space_asset_params& p, entity_id nation,
                                     entity_id asset, orbital_node to, space_launch_quote* detail = nullptr);
satellite_order move_satellite(world& w, const space_asset_params& p, entity_id nation,
                               entity_id asset, orbital_node to);

// What one quarterly step did — reported, never stored.
struct space_asset_step_report
{
    int   launched = 0;  ///< Assets whose launch_tick is this quarter.
    float launch_spent = 0.0f; ///< Their launch costs: the quarter's launch debit.
    int   arrived = 0;   ///< in_transit -> active this quarter.
    int   paid    = 0;   ///< Assets whose upkeep was paid.
    int   unpaid  = 0;   ///< Assets whose upkeep was not.
    int   lost    = 0;   ///< Assets lost this quarter.
    float upkeep_paid = 0.0f; ///< Credits debited from treasuries.
};

/// The quarterly step, once per econ tick inside the nation step. Ascending
/// asset id: an in-transit asset whose arrival tick has come becomes active;
/// every active or decaying asset then pays its upkeep WHOLE from its owner's
/// treasury, or pays nothing and counts an unpaid quarter (decaying; lost at
/// `quarters_to_loss`). A paid quarter clears the count and restores active.
/// Lost assets do nothing. An empty store does nothing at all. @p out, when
/// given, also receives the quarter's launches (assets whose launch tick is
/// @p econ_tick) and their summed cost, so the report carries the launch debit
/// beside the upkeep debit.
void run_space_asset_step(world& w, const space_asset_params& p, int econ_tick,
                          space_asset_step_report* out = nullptr);
