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

/// Write a checked mission onto @p s and set its course: to the zone's nearest tile, or
/// home for `none`. A ship that has broken off keeps its course home and takes the new
/// mission up when it sails again.
void set_ship_mission(world& w, sea_ship& s, sea_mission m, std::vector<uint32_t> provinces);

// ---- The passes -------------------------------------------------------------

struct sea_day_tick
{
    int sailed     = 0; ///< Tiles sailed, summed.
    int detections = 0; ///< (viewer, ship, province) rolls that found their ship.
    int hits       = 0; ///< Ships that took damage.
    int broke_off  = 0; ///< Ships that turned for home today.
    int sunk       = 0; ///< Ships erased today.
};

/// Run ONE day (see the file comment). @p day_tick seeds its rolls.
sea_day_tick run_sea_day(world& w, const recipe_registry& reg, int day_tick);

struct ship_upkeep_tick
{
    int ships     = 0; ///< Alive after the pass.
    int disbanded = 0; ///< Orphaned: no owner, or no own port left to call home.
    int rehomed   = 0; ///< Home port lost, another of the nation's taken up.
    int refunded  = 0; ///< Recruits returned by a hire whose port was lost before it stood.
    int unmet     = 0; ///< Ships whose upkeep draw went short.
};

/// The sea half of the upkeep pass, run right after the wings' on the same rule
/// (MILITARY.md § Upkeep): a ship with no owner is disbanded; a raised ship whose home
/// port is gone or lost takes up the nearest other own port by water, and is disbanded with
/// none it can reach;
/// a hire still mustering at a lost port is dropped and refunds its crew. A raised ship
/// draws `ship_upkeep_goods` at its home port, its supply steps toward the share met,
/// and one lying there damaged refits by drawing goods.
ship_upkeep_tick run_ship_upkeep(world& w, const recipe_registry& reg, economy_report& report);
