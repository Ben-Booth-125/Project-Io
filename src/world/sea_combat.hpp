#pragma once

// ---------------------------------------------------------------------------
// BL-1430 — the sea theatre's core (MILITARY.md § Sea, Ben 2026-11-05)
// ---------------------------------------------------------------------------
// A ship is raised at a port, sails tile by tile to a mission zone of sea
// provinces, and works it. This file owns the ship itself: the four classes, the
// port door, sailing, and the daily detect-and-hit pass.
//
// THE REPRESENTATION. A ship is a `sea_ship` in `world::ships`, not a
// unit_component in `world::units`, for the reason a wing is not: no land pass (the
// march, battle contact, tile blocking, fronts, land upkeep, the blackboard's unit
// section) walks it, so none can mistake it for a ground unit.
//
// THE DAY. `run_sea_day` runs one day from the daily military pass: musters tick,
// ships sail, each ship's PRESENCE is read (a ship working a zone is spread over it
// at 1/N each, as a wing's day is; any other ship is in the province under it),
// each side's detection is compared with each hostile ship's stealth, and every
// detected ship takes its hits. Damage is computed from the day's starting strength
// and applied at the end, so the visit order decides nothing about who fires first.
//
// DETERMINISM. `world::ships` and every map here are ordered. A detection roll is a
// pure hash of (campaign seed, day, province, viewer, target), so no stream state
// exists to save and a replay rolls the same day. Integer arithmetic throughout,
// save the shelf draw (the economy's own float path).
// HEADLESS. Pure world layer: no SDL, no Lua.
// ---------------------------------------------------------------------------

#include "components.hpp"
#include "corp_command.hpp" // corp_command_result
#include "entity.hpp"
#include "sea_params.hpp"
#include "unit_roster.hpp"  // roster_gate

#include <cstdint>
#include <vector>

struct world;
struct economy_report;
struct interception_record;
class recipe_registry;

// ---- The sea roster ---------------------------------------------------------

/// One hireable ship type, one per class. Its numbers are its class's (`sea_params`).
struct ship_roster_row
{
    const char* name; ///< Mechanism name. Never an Earth proper noun.
    ship_class  cls;
    roster_gate gate; ///< The campaign gate, as a land row's (ore / energy access).
};

/// The sea rows, one per class, in ship_class order. APPEND-ONLY: a ship saves its row.
const std::vector<ship_roster_row>& ship_roster_table();

/// The sea rows @p nation's shelves make available now (the land rows' binary gate).
std::vector<uint16_t> available_ship_rows(const world& w, entity_id nation);

/// A ship's class; `transport` for a row the table does not hold.
ship_class ship_class_of(const sea_ship& s);

/// BL-1527: what a ship fights with: its class's numbers plus the kit it recorded.
struct ship_live_stats
{
    int     hp        = 1;
    int     attack    = 0;
    int     detection = 0;
    int     stealth   = 0;
    int     days_per_tile_permille = 1;
    int     capacity  = 0;
    uint8_t targets   = 0;
};
ship_live_stats ship_stats(const recipe_registry& reg, const sea_ship& s);

/// The sea recruits a hire of @p row spends; 0 for a row the table does not hold.
int ship_crew(const recipe_registry& reg, uint16_t row);

/// May class @p c work mission @p m? `none` always.
bool ship_class_works(const recipe_registry& reg, ship_class c, sea_mission m);

/// HP x morale x supply, per-mille.
int ship_strength_permille(const sea_ship& s);

// ---- Water ------------------------------------------------------------------

/// The SEA tile a ship of port @p port lies at: the port's own tile if it is sea, else the
/// nearest sea within two steps, first in step and side order. Null if none. A lake is not
/// sea (`is_sea`), as a lakeshore is no coast; no ship ever stands on or crosses one.
entity_id port_mouth_tile(const world& w, entity_id port);

/// The sea path from @p from to the nearest of @p goals: every tile after @p from,
/// ending on the goal. Empty when @p from is a goal. @p reachable says which empty it is.
std::vector<entity_id> sea_route(const world& w, entity_id from,
                                 const std::vector<entity_id>& goals, bool* reachable = nullptr);

/// Is @p province within @p range steps over water of any of @p nation's own completed,
/// standing ports?
bool sea_province_in_range(const world& w, entity_id nation, uint32_t province, int range);

// ---- The doors --------------------------------------------------------------

/// @p nation's own COMPLETED, not decommissioned port on @p tile — the lowest id
/// where there are several — or null.
entity_id own_port_at(const world& w, entity_id nation, entity_id tile);

/// Every check `hire_ship` makes before money moves, READ-ONLY, in its order: @p row is
/// a real sea row (rejected_invalid) the nation fields now (rejected_invalid); @p tile is
/// a tile (rejected_invalid) carrying the nation's own completed port with water at its
/// mouth (rejected_placement); its sea pool holds the class's crew (rejected_no_recruits).
/// The payment legs are the seam's (corp_command.cpp). @p port_out receives the port.
corp_command_result hire_ship_check(const world& w, const recipe_registry& reg, entity_id nation,
                                    entity_id tile, uint16_t row, entity_id* port_out = nullptr);

/// Every check `assign_sea_mission` makes, READ-ONLY, in its order: the ship exists
/// (rejected_invalid) and is @p nation's (rejected_not_owner); @p mission is a mission
/// (rejected_invalid) its class works (rejected_invalid); `none` names no province, every
/// other mission 1..8 distinct real provinces (rejected_invalid); each is water, in range
/// and reachable by this ship (rejected_placement); the assignment changes something
/// (rejected_state). The sorted set lands in @p out_set.
corp_command_result sea_mission_check(const world& w, const recipe_registry& reg,
                                      entity_id nation, entity_id ship, uint32_t mission,
                                      const uint32_t* provinces, std::size_t n,
                                      std::vector<uint32_t>* out_set = nullptr);

/// BL-1527: every check `set_ship_design` makes, READ-ONLY: the seat's nation alone
/// (rejected_not_owner; no grant lets a rival design); the class, slot and item exist and
/// fit (rejected_invalid); the item is researched (rejected_tech_locked); the slot does
/// not already hold it (rejected_state).
corp_command_result ship_design_check(const world& w, entity_id nation, uint32_t cls,
                                      uint16_t slot, uint16_t item);

/// BL-1527: every check `refit_ship` makes, READ-ONLY, in its order: the ship exists
/// (rejected_invalid) and is @p nation's (rejected_not_owner); its kit differs from the
/// live design, it is raised, and it has no mission to leave (rejected_state); it lies at
/// its home port's mouth with no course (rejected_placement). The payment is the seam's.
corp_command_result refit_ship_check(const world& w, entity_id nation, entity_id ship);

/// BL-1531: every check `embark_unit` makes, READ-ONLY, in its order: the unit exists
/// (rejected_invalid), is @p nation's (rejected_not_owner), has men and is a land row
/// (rejected_invalid); the ship exists (rejected_invalid), is @p nation's (rejected_not_owner)
/// and is a transport (rejected_invalid); the unit is in no battle and not refitting, and the
/// ship is raised, not broken off, has no course or landing, and has room (rejected_state);
/// the unit stands on or beside @p nation's own standing, open port, and the ship lies at that
/// port's mouth (rejected_placement).
corp_command_result embark_check(const world& w, const recipe_registry& reg, entity_id nation,
                                 entity_id unit, entity_id ship);

/// Take a checked unit aboard: out of `world::units` and into the ship. Its order, front and
/// defence post end there.
void embark_unit(world& w, entity_id unit, entity_id ship);

/// BL-1531: every check `land_units` makes, READ-ONLY, in its order: the ship exists
/// (rejected_invalid), is @p nation's (rejected_not_owner) and is a transport (rejected_invalid).
/// A null @p tile recalls a landing: one must be under way with no unit in the surf
/// (rejected_state). Otherwise the ship is raised, not broken off, has units aboard, and has no
/// landing in the surf (rejected_state); @p tile is a land tile touching the sea, on own, friend's
/// or hostile ground, never a neutral's, with its water reachable and in the class's range
/// (rejected_placement); every unit aboard may stand on it (rejected_terrain).
corp_command_result land_units_check(const world& w, const recipe_registry& reg, entity_id nation,
                                     entity_id ship, entity_id tile);

/// Write a checked landing order onto @p s and set its course; a null @p tile sails it home.
void set_ship_landing(world& w, const recipe_registry& reg, sea_ship& s, entity_id tile);

/// Write a checked mission onto @p s and set its course: to the zone's nearest tile, or
/// home for `none`. A ship that has broken off keeps its course home and takes the new
/// mission up when it sails again.
void set_ship_mission(world& w, sea_ship& s, sea_mission m, std::vector<uint32_t> provinces);

// ---- Sea control ------------------------------------------------------------

/// BL-1530 (MILITARY.md § Sea): @p nation's share, per-mille, of the surface attack on patrol,
/// escort or blockade in @p province against the sides hostile to it either way. Submarines,
/// transports and ships not on station count for nothing. 1000 when uncontested, 0 with none.
int sea_control_share_permille(const world& w, const recipe_registry& reg, uint32_t province, entity_id nation);

/// Does @p nation hold @p province: a surface force there, at or past the authored bar?
bool sea_holds(const world& w, const recipe_registry& reg, uint32_t province, entity_id nation);

/// Recompute `world::closed_ports`: a standing port whose mouth lies in a province held by a
/// side with a ship on blockade there, hostile by its own declaration to the port's nation.
/// Drops the logistics caches when the set changes. Run at the end of each sea day.
void refresh_closed_ports(world& w, const recipe_registry& reg);

// ---- The passes -------------------------------------------------------------

struct sea_day_tick
{
    int sailed     = 0; ///< Tiles sailed, summed.
    int detections = 0; ///< (viewer, ship, province) rolls that found their ship.
    int hits       = 0; ///< Ships that took damage.
    int broke_off  = 0; ///< Ships that turned for home today.
    int sunk       = 0; ///< Ships erased today.
    int landed     = 0; ///< Landings that came ashore unopposed today.
    int contested  = 0; ///< Landings that met a defender today.
    int troops_lost = 0; ///< Land units lost with a transport, or stranded in the surf.
};

/// Run ONE day (see the file comment). @p day_tick seeds its rolls.
sea_day_tick run_sea_day(world& w, const recipe_registry& reg, int day_tick);

/// BL-1529 (MILITARY.md § Sea, Raid): take cargo off the sea legs this tick's convoys
/// crossed. Runs between `advance_convoys` and `credit_arrived_convoys`, which spends the
/// sweep it reads. For each day of a crossing, each raider on station in the province whose
/// nation has declared the cargo's owner hostile rolls, lowest ship id first: its take chance
/// per day x its share of the zone x its attack / (its attack + the attack of the owner's
/// escorts there that can hit its class), the escorts counting only on a day the owner's side
/// detects it. A submarine sinks what it takes; a surface raider lands a prize at its nearest
/// own port in range, and sinks it with none. The goods left their source at dispatch, so
/// neither path mints. The rolls are pure hashes; nothing is saved.
std::vector<interception_record> raid_convoys(world& w, const recipe_registry& reg, int tick);

struct ship_upkeep_tick
{
    int ships     = 0; ///< Alive after the pass.
    int disbanded = 0; ///< Orphaned: no owner, or no own port left to call home.
    int rehomed   = 0; ///< Home port lost, another of the nation's taken up.
    int refunded  = 0; ///< Recruits returned by a hire whose port was lost before it stood.
    int unmet     = 0; ///< Ships whose upkeep draw went short.
    int troops_lost = 0; ///< BL-1531: land units lost with a disbanded transport.
};

/// The sea half of the upkeep pass, run right after the wings' on the same rule
/// (MILITARY.md § Upkeep): a ship with no owner is disbanded; a raised ship whose home
/// port is gone or lost takes up the nearest other own port by water, and is disbanded with
/// none it can reach;
/// a hire still mustering at a lost port is dropped and refunds its crew. A raised ship
/// draws `ship_upkeep_goods` at its home port, its supply steps toward the share met,
/// and one lying there damaged refits by drawing goods.
ship_upkeep_tick run_ship_upkeep(world& w, const recipe_registry& reg, economy_report& report);

// ---- The opening fleet --------------------------------------------------------

/// What `seed_major_opening_fleets` did.
struct opening_fleet_report
{
    int majors       = 0; ///< majors given the fleet
    int ships        = 0; ///< ships laid, all majors
    int no_sea_port  = 0; ///< majors with no sea-mouthed port: no ship
    int off_capital  = 0; ///< majors whose capital province holds no sea-mouthed port
    int spilled      = 0; ///< ships berthed past the capital port
};

/// BL-1532 — A MAJOR'S OPENING FLEET IS FIXED (MILITARY.md § Sea, Ben 2026-11-07). On each
/// major (ascending id) the kit's @p fleet (ship_class integers) is laid raised, each on
/// its class's base kit (the nation's live design), at the mouth of a port: the major's
/// own completed sea-mouthed ports are ordered capital port first (in the capital's
/// province), then by straight-line distance from the capital, then by tile and id; the
/// first @p per_port ships berth at the first port, the next @p per_port at the second,
/// wrapping round. A major with no sea-mouthed port opens with no ship. Its sea pool
/// opens at @p sea_recruits either way. Minors are untouched. RNG-free. Run after the
/// last generation step that places or removes a port.
opening_fleet_report seed_major_opening_fleets(world& w, const std::vector<std::uint8_t>& fleet,
                                               int per_port, int sea_recruits);

/// Print the `[opening_kit] fleet` line.
void print_opening_fleets(const opening_fleet_report& r);
