#pragma once

#include "entity.hpp"
#include "orbital_node.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <utility>
#include <vector>

struct world;
struct space_unit_params;
struct station_params;

// ---------------------------------------------------------------------------
// Space battles (BL-1597) — docs/military/SPACE_FLEETS.md § Battles (Ben, 2026-11-10)
// ---------------------------------------------------------------------------
// A BATTLE IS FOUGHT AT AN ORBITAL NODE between two nations. It OPENS on a day when
// both are ARMED there and either is hostile to the other (`is_hostile`, either
// direction — the gate the ground war, the air war and interdiction read; a war
// writes it, a peace clears it). A nation is armed at a node when it has a unit
// there with firepower that is not in transit (`space_armed_units_at`) or a station
// there with a working Defence module. One record per (node, nation pair); the
// battle's participants are NOT stored — every unit of either side at the node, not
// in transit, takes part each day it is fought, so a unit that arrives joins it.
//
// THE DAY (`run_space_battle_day`, after `run_space_unit_day` on the world's day
// cadence). Ascending: open the new battles, then fight every open battle once,
// ascending battle id:
//   1. FIRE. Each side's POOL is fixed at the day's start: the firepower of its
//      units at the node plus `defence_firepower` per working Defence module of its
//      stations there. The two pools land SIMULTANEOUSLY: each walks the other
//      side's TARGET ORDER — escorts (Frigates), then capital ships (Cruisers), then
//      unarmed hulls, each class ascending id; then each station's working Defence
//      modules, ascending station then module id ("take damage last"). On each
//      target the pool first spends the target's ARMOUR (absorbed), then lands up to
//      its hull points; a target at zero is destroyed and the rest of the pool walks
//      on; a pool that cannot clear a target's armour lands nothing and stops there.
//   2. LOSSES. A unit at zero hull points is destroyed — erased, its crew with it.
//      A Defence module at zero is lost (the space-asset loss: the quarterly station
//      step frees its slot). Its damage between days lives in the battle record.
//   3. STATIONS FALL. A station of a side left with no armed unit at the node and no
//      working Defence module, while the other side is still armed there, is lost:
//      its core is lost and the quarterly station step takes the rest.
//   4. WITHDRAWAL. A move ordered for an engaged unit (`order_space_unit` /
//      `order_space_fleet`) is checked whole as any move is and RECORDED, not flown:
//      the unit takes this one more day of fire, then the survivors leave along the
//      ordered path, paying its Δv from their propellant as the move verbs do.
//   5. THE END. A side no longer armed at the node is out: WITHDREW when its units
//      left today, DESTROYED otherwise. The battle closes when either side is out, or
//      on PEACE (neither hostile to the other any more). A closed record stays as the
//      BATTLE REPORT the UI reads, the newest `report_history` closed ones kept.
//
// THE DEBRIS SEAM (BL-1598 owns debris): every loss adds its mass — a unit's dry mass
// plus its cargo, `defence_module_mass` per module, `station_core_mass` per core — to
// the battle's `mass_destroyed` and to the day report's per-node list. Nothing here
// makes debris.
//
// DETERMINISM. Every store is a std::map or a sorted vector walked ascending; float
// arithmetic over saved inputs in a fixed order; no RNG, no clock.
//
// SAVED: world::space_battles, world::next_space_battle_id — the trailing section
// after the space units (world_save.hpp). Folded into state_hash when non-empty.
// ---------------------------------------------------------------------------

/// The dials. First in-band cut; see the report. Not a Lua key yet: the registry
/// carries these defaults (`recipe_registry::space_battles`).
struct space_battle_params
{
    float   defence_firepower   = 12.0f; ///< A working Defence module's fire a day.
    float   defence_armour      = 6.0f;  ///< Absorbed off each day's fire on a module.
    float   defence_hull_points = 40.0f; ///< A module's hull points in a battle.
    float   defence_module_mass = 6.0f;  ///< Mass a lost module adds to `mass_destroyed`.
    float   station_core_mass   = 20.0f; ///< Mass a lost core adds.
    int32_t report_history      = 16;    ///< Closed reports kept; clamped into [0, k_space_battle_history_max].
    // BL-1599 (space_bombard.hpp), first in-band cut:
    float   battery_firepower   = 12.0f; ///< An orbital defence battery's fire a day at full condition (a Defence module's).
    float   bombard_condition_per_firepower = 1.0f; ///< Condition per-mille a bombarding unit's firepower point takes a day (a missile frigate's 18 ~ a bomber's 12).
    float   bombard_force_per_permille      = 0.1f; ///< `force_used` magnitude per per-mille of condition lost (battles pass men lost).
};

inline constexpr int32_t     k_space_battle_history_max = 256;
/// Ceilings the save reader holds the stream to.
inline constexpr std::size_t k_space_battles_max        = 4096;
inline constexpr std::size_t k_space_battle_list_max    = 1u << 16;

/// Append-only (saved).
enum class space_battle_state : uint8_t
{
    open    = 0,
    decided = 1, ///< A side is out (destroyed or withdrew).
    peace   = 2, ///< Neither side is hostile to the other any more.
};

/// How a side stands at the close. Append-only (saved).
enum class space_battle_end : uint8_t
{
    holding   = 0, ///< Still armed at the node (or the battle is open / ended in peace).
    destroyed = 1,
    withdrew  = 2,
};

/// One side's tally — the report the UI reads.
struct space_battle_side
{
    entity_id nation        = null_entity;
    int32_t   units_lost    = 0;
    int32_t   crew_lost     = 0;  ///< On lost units, modules and station cores.
    int32_t   modules_lost  = 0;  ///< Defence modules.
    int32_t   stations_lost = 0;
    float     damage_landed = 0.0f; ///< Hull points this side's fire took off the other.
    space_battle_end end    = space_battle_end::holding;

    friend bool operator==(const space_battle_side&, const space_battle_side&) = default;
};

/// An engaged unit's recorded move: it leaves for `dest` after the next day's fire.
struct space_battle_withdrawal
{
    entity_id    unit = null_entity;
    orbital_node dest{};

    friend bool operator==(const space_battle_withdrawal&, const space_battle_withdrawal&) = default;
};

/// One battle. SERIALISED.
struct space_battle
{
    uint32_t           id    = 0;
    orbital_node       node{};
    std::array<space_battle_side, 2> sides{}; ///< sides[0].nation < sides[1].nation.
    space_battle_state state = space_battle_state::open;
    int32_t            opened_tick = 0;  ///< Econ tick (quarter) it opened.
    int32_t            days        = 0;  ///< Days fought.
    float              mass_destroyed = 0.0f; ///< The debris seam (BL-1598).
    std::vector<space_battle_withdrawal> withdrawing; ///< Open only; ascending unit id.
    std::vector<std::pair<entity_id, float>> module_damage; ///< Open only; Defence module id ascending, damage taken.

    friend bool operator==(const space_battle&, const space_battle&) = default;
};

/// Either direction of the stance hostility the ground war reads.
bool space_hostile(const world& w, entity_id a, entity_id b);

/// Is @p nation armed at @p node (an armed unit not in transit, or a station with a
/// working Defence module)?
bool space_armed_at(const world& w, const station_params& stp, entity_id nation, orbital_node node);

/// BL-1599: open the battle between @p a and @p b at @p node — the step-1 rule, callable
/// where a meeting is found (a convoy's escort meeting a hostile fleet). Refused (0)
/// when the pair is not two distinct nations hostile to each other (`space_hostile`);
/// an open battle for (node, pair) already standing is returned as is. Otherwise a new
/// record, sides ascending, with the next id; @p opened says which. Does not check that
/// both are armed: the battle day closes a side that is not.
uint32_t open_space_battle(world& w, entity_id a, entity_id b, orbital_node node, bool* opened = nullptr);

/// The open battle at @p node with @p nation as a side (lowest id), or nullptr.
const space_battle* space_battle_engaging(const world& w, entity_id nation, orbital_node node);

/// Is @p unit engaged — not in transit, at a node where its owner is a side of an
/// open battle?
bool space_unit_engaged(const world& w, entity_id unit);

/// Record a withdrawal for @p ids (each engaged, the move already checked whole by
/// the caller). A unit already withdrawing takes the new destination.
void record_space_withdrawal(world& w, const std::vector<entity_id>& ids, orbital_node dest);

/// What one battle day did — reported, never stored.
struct space_battle_day_report
{
    int   opened = 0, fought = 0, closed = 0;
    int   units_lost = 0, crew_lost = 0, modules_lost = 0, stations_lost = 0;
    int   departed = 0;            ///< Withdrawing units that left.
    int   departures_failed = 0;   ///< A recorded move the unit could no longer pay; it stays.
    float landed = 0.0f;           ///< Hull points landed, both sides, every battle.
    /// THE DEBRIS SEAM: mass destroyed today per node, battle id order.
    std::vector<std::pair<orbital_node, float>> mass_destroyed;
};

/// THE DAY (see the header comment). Nothing at all when there are no space units,
/// no stations and no open battle.
void run_space_battle_day(world& w, const space_unit_params& p, const station_params& stp,
                          const space_battle_params& bp, space_battle_day_report* out = nullptr);
