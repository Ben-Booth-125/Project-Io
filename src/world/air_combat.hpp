#pragma once

// ---------------------------------------------------------------------------
// BL-1449 — air combat (MILITARY.md § Air, Ben 2026-10-28)
// ---------------------------------------------------------------------------
// An air wing is BASED at an airbase and ASSIGNED to provinces in range with a
// mission — superiority, tactical or strategic bombing, recon. It never stands on
// a tile. Missions are flown DAILY.
//
// THE REPRESENTATION. A wing is an `air_wing` in `world::wings`, not a
// unit_component in `world::units`, so no land pass can mistake it for a ground
// unit: the march, battle contact and tile-contact blocking, the fronts, the land
// upkeep pass and the blackboard's unit section all walk `w.units` and never see a
// wing. What the air reaches on the ground it reaches through named seams here:
// tactical hits land on units, strategic hits on `building_component::
// condition_permille`, the superiority share slows a march (`air_march_cost_permille`,
// read by run_unit_march) and reveals units (`world::air_revealed`).
//
// THE DAY. `run_air_day` flies one day: musters tick, every flying wing spreads its
// day over its provinces (1/N each), superiority power is summed per (province,
// nation), fighters shoot hostile wings over their provinces and ground air
// defence shoots every hostile mission, bombing lands scaled by the share and
// raised by recon, damaged buildings repair by drawing goods, and the reveal set is
// rebuilt. Wing damage is computed from the day's starting strength and applied at
// its end, so the visit order decides nothing about who shoots first.
// `run_air_missions` flies @p days of them; on main it is called once per economy
// tick with the quarter's 90 days, beside the battle pass. BL-1445 (move by days)
// moves the call onto the daily step with days = 1 — a call-site move.
//
// DETERMINISM. `world::wings` and every map here are ordered; units and buildings
// are walked in ascending id; integer arithmetic throughout save the shelf draw
// (the economy's own float path). No RNG.
// HEADLESS. Pure world layer: no SDL, no Lua.
// ---------------------------------------------------------------------------

#include "air_params.hpp"
#include "components.hpp"
#include "corp_command.hpp" // corp_command_result
#include "entity.hpp"
#include "unit_designs.hpp" // air_combat_stats
#include "unit_roster.hpp"  // roster_gate

#include <cstdint>
#include <vector>

struct world;
struct economy_report;
class recipe_registry;

// ---- The air roster ---------------------------------------------------------

/// One hireable air type: the air rows of the roster (MILITARY.md § Air, six
/// classes). Its numbers are its class's (`air_params`, Lua) plus its design's kit.
struct air_roster_row
{
    const char* name; ///< Mechanism name. Never an Earth proper noun.
    air_class   cls;
    roster_gate gate; ///< The campaign gate, as a land row's (ore / energy access).
};

/// The air rows, one per class, in air_class order. APPEND-ONLY: a wing saves its row.
const std::vector<air_roster_row>& air_roster_table();

/// The air rows @p nation's shelves make available now (the land rows' binary gate).
std::vector<uint16_t> available_air_rows(const world& w, entity_id nation);

// ---- A wing's numbers -------------------------------------------------------

/// A wing's class numbers plus its recorded design's kit — what it flies with.
struct air_wing_stats
{
    int  hp = 0, air = 0, superiority_permille = 0, soft = 0, hard = 0;
    int  strategic = 0, recon = 0, range = 0, ad_vulnerability_permille = 1000;
    bool has_morale = true;
};

air_class      wing_class(const air_wing& wg);
air_wing_stats wing_stats(const recipe_registry& reg, const air_wing& wg);

/// HP fraction x morale x supply, per-mille. A drone's morale reads 1000.
int wing_strength_permille(const recipe_registry& reg, const air_wing& wg);

/// May class @p c fly mission @p m (its authored mission list)? `none` always.
bool air_class_flies(const recipe_registry& reg, air_class c, air_mission m);

/// Does a building of @p t lose throughput with its condition — the only kind a
/// strategic mission targets? Producers (output), the marketplace and the port
/// (trade points), hubs, stations and ports (Logistic Points), and the University and
/// research institute (research points). NOT the military base or the airbase
/// (BL-1449 fix round: their muster and sortie are not scaled by condition, so they
/// are excluded from strategic targeting rather than bombed to no effect), nor the
/// launchpad or schooling, which have no throughput the condition reaches.
bool condition_bites(building_type t);

/// Tactical and strategic missions bomb; superiority and recon do not.
bool air_mission_bombs(air_mission m);

// ---- Range ------------------------------------------------------------------

/// The straight-line hex distance between two tiles of one body (odd-r rows,
/// east-west cylinder wrap). -1 when either is not a tile or they lie on two bodies.
int air_tile_distance(const world& w, entity_id a, entity_id b);

/// The distance from @p from (a tile) to @p province's NEAREST tile; -1 when the
/// province does not exist or lies on another body.
int air_province_distance(const world& w, entity_id from, uint32_t province);

/// The tile @p wing flies from: its airbase's tile, or null when the base is gone.
entity_id wing_base_tile(const world& w, const air_wing& wg);

/// Is @p province within @p wing's range (MILITARY.md: the straight-line tile
/// distance from the airbase to the province's nearest tile is within its range)?
bool air_province_in_range(const world& w, const recipe_registry& reg, const air_wing& wg,
                           uint32_t province);

/// Every province in @p wing's range, ascending id — what a right-click may name.
std::vector<uint32_t> air_provinces_in_range(const world& w, const recipe_registry& reg,
                                             entity_id wing);

// ---- The doors --------------------------------------------------------------

/// @p nation's own COMPLETED, not decommissioned airbase on @p tile — the lowest id
/// where there are several — or null.
entity_id airbase_at(const world& w, entity_id nation, entity_id tile);

/// Every check `hire_wing` makes before money moves, READ-ONLY, in its order:
/// @p row is a real air row (rejected_invalid) the nation fields now
/// (rejected_invalid); @p tile is a tile (rejected_invalid) carrying the nation's own
/// completed airbase (rejected_placement — a military base hosts no wings). The
/// payment legs are the seam's (corp_command.cpp). @p base_out receives the airbase.
corp_command_result hire_wing_check(const world& w, entity_id nation, entity_id tile,
                                    uint16_t row, entity_id* base_out = nullptr);

/// Every check `assign_air_mission` makes, READ-ONLY, in its order: the wing exists
/// (rejected_invalid) and is @p nation's (rejected_not_owner); @p mission is a mission
/// (rejected_invalid) its class flies (rejected_invalid); `none` names no province,
/// every other mission 1..8 distinct real provinces (rejected_invalid); each in range
/// (rejected_placement); a STRATEGIC mission's every province held by a nation @p nation
/// is hostile to (rejected_state) — tactical may fly anywhere, its targets being hostile
/// units; the assignment changes something (rejected_state).
/// @p provinces holds @p n ids in any order; the sorted set lands in @p out.
corp_command_result air_mission_check(const world& w, const recipe_registry& reg,
                                      entity_id nation, entity_id wing, uint32_t mission,
                                      const uint32_t* provinces, std::size_t n,
                                      std::vector<uint32_t>* out = nullptr);

/// Every check `set_air_design` makes, READ-ONLY: the seat's nation alone
/// (rejected_not_owner); @p cls an air class, @p slot one of its three, @p item an air
/// kit id that fits (rejected_invalid); researched (rejected_tech_locked); not already
/// fitted (rejected_state).
corp_command_result air_design_check(const world& w, entity_id nation, uint32_t cls,
                                     uint16_t slot, uint16_t item);

// ---- Superiority --------------------------------------------------------------

/// @p nation's share of the superiority air attack over @p province, per-mille,
/// against every nation hostile to it either way. 1000 when nobody hostile flies
/// superiority there (uncontested), 0 when only its enemies do.
int air_share_permille(const world& w, uint32_t province, entity_id nation);

/// The per-mille a march step onto @p tile costs @p mover: 1000, raised toward
/// 1000 + march_slow_permille as its share over the tile's province falls.
int air_march_cost_permille(const world& w, const recipe_registry& reg, entity_id mover,
                            entity_id tile);

// ---- The passes -----------------------------------------------------------------

struct air_day_tick
{
    int sorties          = 0; ///< Wing-provinces flown.
    int wings_hit        = 0; ///< Wings that took damage.
    int ad_shots         = 0; ///< (air defence unit, wing) engagements.
    int tactical_hits    = 0; ///< Tactical sorties that found a target.
    int men_killed       = 0;
    int strategic_hits   = 0; ///< Strategic sorties that found a building.
    int condition_lost   = 0; ///< Per-mille, summed.
    int condition_repaired = 0;
    int revealed         = 0; ///< Size of the reveal set after the day.
};

/// Fly ONE day (see the file comment). @p report receives the repair draws' wants
/// and purchases, as every shelf draw's.
air_day_tick run_air_day(world& w, const recipe_registry& reg, economy_report& report);

/// Fly @p days days, summed. The call `run_economy_step` makes beside the battles.
air_day_tick run_air_missions(world& w, const recipe_registry& reg, economy_report& report,
                              int days);

struct wing_upkeep_tick
{
    int wings     = 0; ///< Alive after the pass.
    int disbanded = 0; ///< Orphaned (owner or airbase gone) or shot down (HP 0).
    int unmet     = 0; ///< Wings whose upkeep draw went short.
};

/// The wing half of the upkeep pass, run right after `run_unit_upkeep` and on its
/// rule (MILITARY.md § Upkeep): a wing whose owner or airbase has been erased, or
/// whose HP is 0, is disbanded; a ready wing draws `wing_upkeep_goods` at its
/// airbase's tile (supply is drawn where the unit stands) and its supply factor steps
/// toward the share met by the unit table's decay / recovery. Also prunes the reveal
/// set and the hit bank of units this tick erased.
wing_upkeep_tick run_wing_upkeep(world& w, const recipe_registry& reg, economy_report& report);

/// The bonus per-mille an air defence unit's kit adds to its fire (MILITARY.md § Air
/// defence's kit is "for the air theatre"): launcher +0/+300/+600/+900, fire control
/// +0/+200/+400/+500 by variant.
int air_defence_kit_bonus_permille(const unit_design& kit);
