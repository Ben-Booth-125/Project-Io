#pragma once

#include "components.hpp" // resource_type
#include "entity.hpp"
#include "orbital_node.hpp"
#include "stations.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

struct world;

// ---------------------------------------------------------------------------
// Space units (BL-1594 designs, BL-1595 movement) — docs/military/SPACE_FLEETS.md
// § A space unit is raised to a design, § Thrust — the Δv budget, § Where a space
// unit is, § Raising and launching (Ben, 2026-11-10).
// ---------------------------------------------------------------------------
// A SPACE UNIT is a nation's ship at an orbital node or on a path between two.
// A FLEET is a nation's units at one node, under one order (`order_space_fleet`).
//
// DESIGNS. A hull class (`space_hull`) fixes the role and its slots; each slot
// takes one catalogue item (`space_item_catalogue`). Every slot a hull has must be
// filled. One live design per (nation, hull): `set_space_unit_design`, refused when
// the nation does not hold the hull's key or any item's key (space_tech_keys.hpp,
// `nation_has_mechanic`). A unit carries a SNAPSHOT of the design it was raised to.
//
// THE Δv BUDGET — THE ROCKET EQUATION. A design has a dry mass (hull + items), a
// tank capacity in units of the `propellant` good, and an exhaust speed `ve` from
// its drive. A unit carries `propellant` (good units == mass units) and `cargo`
// (mass units). Its remaining Δv is
//     dv = ve x ln((dry + cargo + propellant) / (dry + cargo))
// and a move of Δv `d` burns  (dry + cargo + propellant) x (1 - e^(-d / ve)).
// So cargo shortens the reach, and a refuel restores it. The budget is the
// propellant on board, never a stored Δv: the Δv is read off it.
//
// WHERE A UNIT IS (`space_unit_state`): idle at an orbital node; docked at a
// station (berthed at a working Dock of its own nation — allies are not modelled:
// relations expose no cheap "ally", so a station docks its owner's units only);
// in transit (from `node` to `dest`, `days_remaining` days left); landed (a Lander
// at a surface node). A move is paid WHOLE at the order, and the unit leaves at
// once along the cheapest path of the DEFAULT orbital graph (`orbital_graph_params{}`,
// as every other caller reads it); the DAY step (`run_space_unit_day`, the world's day cadence) counts the
// days down and lands it idle (or landed) at the destination.
//
// RAISING. At a Launchpad (`raise_space_unit_at_pad`): the nation's completed,
// standing, staffed pad on the named body; the hull must be surface-launchable
// (never a Cruiser); the unit appears idle at that body's LOW ORBIT. The launch buys
// off THAT body's market shelf (the pad tile's market): the kit goods, a full tank
// of propellant, and the climb's propellant = climb Δv x `launch_propellant_per_dv`
// x the unit's wet mass. At a station's WORKING Shipyard (`raise_space_unit_at_
// shipyard`): any hull, no climb, the goods off the owner's HOME shelf (its capital
// tile's market); the unit appears docked at the station (the yard holds it: a
// shipyard raise needs no free Dock berth, but it counts against the berths after).
// Crewed hulls draw their crew from the astronaut pool. Refused WHOLE on any
// shortfall — key, design, pad, yard, shelf, treasury, crew.
//
// REFUELLING (`refuel_space_unit`): docked at a station with a working Fuel depot,
// from the station's depot stock (already paid for; free to draw, as much as it
// holds); or idle in a body's low orbit where its owner holds a staffed Launchpad on
// that body, buying a full top-up off that body's shelf (refused whole when the
// shelf or treasury is short). The depot stock is filled each QUARTER
// (`run_space_unit_quarter`) up to `depot_stock_per_depot` x working depots, bought
// off the owner's home shelf as far as the shelf and treasury allow.
//
// UPKEEP AND DECAY — THE SPACE-ASSET RULE SHAPE. Each unit pays its hull's upkeep
// whole each quarter from its owner's treasury, or pays nothing and counts an
// unpaid quarter; at `quarters_to_loss` consecutive unpaid quarters it is LOST:
// erased from the store, its crew with it. A paid quarter clears the count.
//
// BATTLES are space_battle.hpp's (BL-1597); a move ordered for an engaged unit
// is recorded there as a withdrawal.
//
// WHAT IS NOT HERE: convoys and trade dispatch, unloading cargo, debris,
// bombardment, any AI, any UI, the agent seam. The seams left for them: the
// armed-unit list at a node (`space_armed_units_at`) and each unit's readable hull
// points, firepower and armour (`space_unit_firepower`, `space_unit_armour`).
//
// DETERMINISM. Every store is a std::map walked ascending; arithmetic is float/
// double over static inputs; std::exp / std::log only on finite positive values.
// No RNG, no clock.
//
// SAVED: world::space_units, world::space_unit_designs, world::station_depot_stock
// — the v83 trailing section (world_save.hpp). Folded into state_hash when any is
// non-empty.
// ---------------------------------------------------------------------------


/// The hull classes. Append-only (saved).
enum class space_hull : uint8_t
{
    hauler  = 0,
    lander  = 1,
    tug     = 2,
    frigate = 3,
    cruiser = 4,
};
inline constexpr std::size_t space_hull_count = 5;

/// The slot kinds. A design holds one item id per kind; a kind the hull lacks
/// holds `k_no_space_item`. Append-only (saved).
enum class space_unit_slot : uint8_t
{
    drive      = 0,
    tanks      = 1,
    armament   = 2, ///< Frigate, Cruiser.
    armour     = 3, ///< Frigate, Cruiser.
    bay        = 4, ///< Hauler, Lander: a cargo or passenger bay.
    debris_arm = 5, ///< Tug.
};
inline constexpr std::size_t space_unit_slot_count = 6;

inline constexpr uint8_t k_no_space_item = 0xFF;

/// A design: one catalogue id per slot kind. Saved.
using space_design = std::array<uint8_t, space_unit_slot_count>;

enum class space_unit_state : uint8_t
{
    idle       = 0,
    docked     = 1,
    in_transit = 2,
    landed     = 3,
};
inline constexpr std::size_t space_unit_state_count = 4;

/// One space unit. SERIALISED (v83).
struct space_unit
{
    entity_id        id    = null_entity;
    entity_id        owner = null_entity; ///< A NATION.
    space_hull       hull  = space_hull::hauler;
    space_design     design{};            ///< Snapshot of the design raised to.
    space_unit_state state = space_unit_state::idle;
    orbital_node     node{};              ///< Where it is; the origin while in transit.
    orbital_node     dest{};              ///< In transit: where it goes. Otherwise == node.
    int32_t          days_remaining = 0;  ///< In transit: >= 1. Otherwise 0.
    entity_id        station = null_entity; ///< Docked: the station. Otherwise null.
    float            propellant  = 0.0f;  ///< Good units aboard, in [0, tank].
    float            cargo       = 0.0f;  ///< Mass units carried, in [0, cargo cap]. (Loading is a convoy's; owed.)
    float            hull_points = 0.0f;  ///< In (0, max].
    int32_t          crew        = 0;     ///< Astronauts aboard.
    float            upkeep      = 0.0f;  ///< Credits a quarter, fixed at raise.
    int32_t          unpaid_quarters = 0;
};

/// Does hull @p h have slot @p s?
bool space_hull_has_slot(space_hull h, space_unit_slot s);

/// A good a hull or an item is built from, per unit raised.
struct space_good
{
    resource_type res = resource_type::spacecraft_components;
    float         qty = 0.0f;
};
inline constexpr std::size_t k_space_goods_max = 3;

/// One hull class's base stats.
struct space_hull_spec
{
    const char* name;
    float       mass;          ///< Dry mass units.
    float       hull_points;
    int32_t     crew;          ///< Astronauts drawn at raise.
    float       upkeep;        ///< Credits a quarter.
    bool        surface_launch;///< May be raised at a Launchpad.
    std::string_view key;      ///< The far-space mechanic key that opens the hull (space_tech_keys.hpp).
    std::array<space_good, k_space_goods_max> goods;
};

/// One catalogue item.
struct space_item
{
    const char* id;            ///< Stable string id.
    const char* name;          ///< Mechanism name. Never a proper noun.
    space_unit_slot  slot;
    uint8_t     hull_mask;     ///< Bit (1 << hull) per hull whose slot it fits.
    std::string_view key;      ///< The far-space mechanic key that opens it (space_tech_keys.hpp).
    float       mass;          ///< Dry mass added.
    float       exhaust;       ///< Drives: the exhaust speed ve (Δv units). 0 otherwise.
    float       tank;          ///< Tanks: propellant capacity (good units).
    float       cargo;         ///< Bays: cargo capacity (mass units).
    int32_t     passengers;    ///< Bays: passenger berths (not crew; not drawn from the pool).
    float       firepower;
    float       armour;
    float       hull_points;
    std::array<space_good, k_space_goods_max> goods;
};

/// The hull table, `space_hull` order.
const std::array<space_hull_spec, space_hull_count>& space_hull_table();

/// The item catalogue; an item's id is its index.
const std::vector<space_item>& space_item_catalogue();

/// A design's stats, evaluated. Pure.
struct space_design_stats
{
    bool    valid       = false; ///< Every slot of the hull filled with an item that fits it, no other slot filled.
    float   dry_mass    = 0.0f;
    float   exhaust     = 0.0f;  ///< ve.
    float   tank        = 0.0f;  ///< Propellant capacity.
    float   dv_capacity = 0.0f;  ///< Full tank, empty hold: ve x ln((dry + tank) / dry).
    float   cargo       = 0.0f;
    int32_t passengers  = 0;
    int32_t crew        = 0;
    float   firepower   = 0.0f;
    float   armour      = 0.0f;
    float   hull_points = 0.0f;
    float   upkeep      = 0.0f;
    std::array<float, resource_count> goods{}; ///< Kit goods per unit raised (hull + items), propellant excluded.
};

space_design_stats evaluate_space_design(space_hull h, const space_design& d);

/// The dials. First in-band cut; see the report. Not a Lua key yet: the registry
/// carries these defaults (`recipe_registry::space_units`).
struct space_unit_params
{
    float launch_propellant_per_dv = 0.1f; ///< Climb propellant per Δv of climb per unit of wet mass.
    float repair_per_day          = 1.0f;  ///< Hull points a docked unit regains a day.
    float depot_stock_per_depot   = 200.0f;///< A station's propellant stock cap per working Fuel depot.
    int32_t quarters_to_loss      = 4;     ///< Clamped into [1, k_space_quarters_to_loss_ceiling].
    int32_t berths_per_dock       = 2;
    /// The fair-price ceiling every draw obeys (components.hpp `shelf_admits`). 0 =
    /// off: buy whatever the shelf holds (the launch/procurement OFF behaviour).
    float reservation_mult        = 0.0f;
};


enum class space_unit_result : uint8_t
{
    ok = 0,
    rejected_invalid,           ///< A hull, slot, item, node or design out of range or ill-formed.
    rejected_unknown_nation,
    rejected_unknown_unit,
    rejected_not_owner,
    rejected_tech_locked,       ///< The hull's or an item's key is not held.
    rejected_no_design,         ///< No live design for the hull.
    rejected_not_launchable,    ///< A hull a surface may not launch (Cruiser).
    rejected_no_pad,            ///< No completed, staffed Launchpad of the nation on the body.
    rejected_no_market,         ///< No shelf to buy from.
    rejected_shelf_short,       ///< The shelf cannot fill the whole lot.
    rejected_insufficient_treasury,
    rejected_no_crew,
    rejected_unknown_station,
    rejected_no_shipyard,       ///< No working Shipyard module.
    rejected_no_dock,           ///< No working Dock module, or the berths are full.
    rejected_busy,              ///< In transit (cannot be ordered), or docked / not docked as the verb needs.
    rejected_wrong_node,        ///< Not at the station's node; not in a low orbit with a pad.
    rejected_no_route,          ///< No path, or the destination is the node it is at.
    rejected_surface,           ///< A surface node for a hull other than a Lander.
    rejected_insufficient_dv,   ///< The budget cannot pay the move.
    rejected_no_fuel,           ///< Refuel: nothing to draw (no working depot, empty stock, full tank).
};

/// What a raise, a move or a refuel did.
struct space_unit_op
{
    space_unit_result result = space_unit_result::rejected_invalid;
    entity_id unit    = null_entity;           ///< The unit raised (or the first unit ordered).
    float     credits = 0.0f;                  ///< Debited from the treasury.
    float     propellant = 0.0f;               ///< Propellant bought / drawn / burned.
    float     climb_propellant = 0.0f;         ///< Raise at a pad: the climb's share of `propellant`.
    float     dv      = 0.0f;                  ///< A move: the path's Δv.
    int32_t   days    = 0;                     ///< A move: transit days.
    entity_id market  = null_entity;           ///< The shelf bought from.
    /// BL-1597: the units are engaged in a battle, so the move is RECORDED as a
    /// withdrawal (checked whole, nothing paid yet): they take one more day of fire
    /// and the battle day flies them (space_battle.hpp). `propellant` is the burn
    /// the move would make now.
    bool      withdrawing = false;
};

/// THE DESIGN VERB. Rejected whole: unknown nation, hull out of range, a design
/// the hull cannot hold (`evaluate_space_design(...).valid` false), or any key not held.
space_unit_result set_space_unit_design(world& w, entity_id nation, space_hull h, const space_design& d);

/// The nation's live design for @p h, or nullptr.
const space_design* space_unit_design_of(const world& w, entity_id nation, space_hull h);

/// The nation's completed, standing, staffed Launchpad on @p body (lowest id), or null.
entity_id space_staffed_pad_on(const world& w, entity_id nation, entity_id body);

/// THE RAISE VERBS. Rejected WHOLE — nothing written, not even the allocator.
space_unit_op raise_space_unit_at_pad(world& w, const space_unit_params& p, entity_id nation,
                                      space_hull h, entity_id body);
space_unit_op raise_space_unit_at_shipyard(world& w, const space_unit_params& p,
                                           const station_params& stp, entity_id nation,
                                           space_hull h, entity_id station);

/// The Δv @p u has left, with its cargo aboard. Pure.
float space_unit_dv_remaining(const space_unit& u);

/// The propellant a move of @p dv burns for @p u. Pure; +inf when the design has no drive.
float space_unit_burn_for(const space_unit& u, float dv);

/// THE MOVE VERBS. One unit, or the fleet: every unit of @p nation at @p from not
/// in transit. The path is `orbital_path` from the units' node; each unit pays it
/// from its own budget; refused WHOLE when any unit cannot. A docked unit undocks
/// as it leaves. A surface destination is a Lander's only. BL-1597: a unit engaged
/// in an open battle (`space_unit_engaged`) does not leave at once — the checked
/// move is recorded as its WITHDRAWAL (`space_unit_op::withdrawing`).
space_unit_op order_space_unit(world& w, const space_unit_params& p, entity_id nation,
                               entity_id unit, orbital_node to);
space_unit_op order_space_fleet(world& w, const space_unit_params& p, entity_id nation,
                                orbital_node from, orbital_node to);

/// BL-1597: the move itself, flown now — the battle day's departure of a recorded
/// withdrawal. Every id must be a unit at @p from not in transit; then the move
/// verbs' check and payment, refused whole.
space_unit_op depart_space_units(world& w, const std::vector<entity_id>& ids, orbital_node from,
                                 orbital_node to);

/// Dock @p unit (idle at @p station's node) at the station: the same nation's, a
/// working Dock, a free berth (`berths_per_dock` per working Dock).
space_unit_result dock_space_unit(world& w, const space_unit_params& p, const station_params& stp,
                                  entity_id nation, entity_id unit, entity_id station);
space_unit_result undock_space_unit(world& w, entity_id nation, entity_id unit);

/// The berths @p station offers (working Docks x berths_per_dock) and the units docked.
int space_station_berths(const world& w, const space_unit_params& p, const station_params& stp,
                         entity_id station);
int space_station_docked(const world& w, entity_id station);

/// Refuel to a full tank (see the header comment for the two places).
space_unit_op refuel_space_unit(world& w, const space_unit_params& p, const station_params& stp,
                                entity_id nation, entity_id unit);

/// The seams battles read (space_battle.hpp): the nation's units at @p node with a
/// firepower above zero and not in transit, ascending id; and a unit's figures.
std::vector<entity_id> space_armed_units_at(const world& w, entity_id nation, orbital_node node);
float space_unit_firepower(const space_unit& u);
float space_unit_armour(const space_unit& u);
float space_unit_max_hull_points(const space_unit& u);

/// What one day did — reported, never stored.
struct space_unit_day_report
{
    int   arrived  = 0;
    float repaired = 0.0f;
};

/// ONE DAY: ascending id, every in-transit unit counts a day off and arrives at
/// zero (idle, or landed at a surface); every docked unit at a station that still
/// has a working Dock regains `repair_per_day`, capped; a docked unit whose
/// station is gone or has no working Dock is undocked idle. An empty store does
/// nothing at all.
void run_space_unit_day(world& w, const space_unit_params& p, const station_params& stp,
                        space_unit_day_report* out = nullptr);

/// What one quarter did — reported, never stored.
struct space_unit_quarter_report
{
    int   paid = 0, unpaid = 0, lost = 0, crew_lost = 0;
    float upkeep_paid = 0.0f;
    float depot_bought = 0.0f;   ///< Propellant bought into depots.
    float depot_spent  = 0.0f;   ///< Credits for it.
};

/// THE QUARTER, inside the nation step after the station step: each unit's upkeep
/// (ascending id; unpaid counts; lost at `quarters_to_loss`), then each station's
/// depot fill (ascending station id), then dropping the depot stock of any station
/// that no longer exists. Empty stores do nothing at all.
void run_space_unit_quarter(world& w, const space_unit_params& p, const station_params& stp,
                            int econ_tick, space_unit_quarter_report* out = nullptr);

/// Ceilings the save reader holds the stream to.
inline constexpr float k_space_depot_stock_max = 1.0e9f;
