#pragma once

#include "entity.hpp"
#include "orbital_node.hpp"

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

struct world;
struct space_unit_day_report;
struct space_battle_day_report;

// ---------------------------------------------------------------------------
// Debris (BL-1598) — docs/economy/SPACE_ASSETS.md § Debris (Ben, 2026-11-10)
// ---------------------------------------------------------------------------
// DEBRIS IS ITS OWN HAZARD, held per ORBITAL NODE as a debris LEVEL (mass units),
// `world::space_debris`. Not part of the climate commons, though it behaves like
// one: every nation's wreckage fouls the same node. All of it is arithmetic on the
// level; there is no roll anywhere.
//
// WHAT MAKES DEBRIS. A loss leaves `debris_per_mass` x its mass at its node:
//   - a space unit destroyed in a battle, and a lost Defence module or station core
//     there: the battle day's `mass_destroyed` per node (space_battle.hpp) — the
//     DAY step deposits it;
//   - a space unit destroyed by debris itself (below): its dry mass + cargo;
//   - a satellite, station core or station module LOST in the quarter — to upkeep,
//     to a fallen station's sweep, or to wear: `satellite_mass`, `station_core_mass`,
//     `station_module_mass` (a truss counts as a module). The nation step watches
//     the assets in place before the space steps (`watch_space_losses`) and deposits
//     for every one the steps lost (`run_space_debris_quarter`); a module removed by
//     its owner is erased, not lost, and leaves nothing;
//   - a space unit lost to upkeep: abandoned where it sits, its dry mass + cargo.
// Only an orbital node holds debris; a loss at a surface leaves none.
//
// WHAT DEBRIS DOES.
//   - WEAR (each quarter). Every satellite and station core IN PLACE (active or
//     decaying) at a node gains `wear_per_debris` x the node's level, the levels read
//     at the pass's start. At wear >= 1 it is LOST, and its mass becomes debris after
//     the pass — the CASCADE: a crowded node's losses raise the level the next quarter
//     wears on. Wear lives on the asset (`space_asset::wear`, in [0, 1)).
//   - HULL DAMAGE (each day). Every space unit sitting at an orbital node (idle or
//     docked) takes `unit_damage_per_debris` x the node's level off its hull points;
//     a unit that ARRIVED today also takes one day's damage at every node its path
//     PASSED THROUGH (the default graph's cheapest path, its two ends excluded), in
//     path order. A unit at zero is destroyed — erased, its crew with it — and its
//     mass becomes debris at the node it died at, after the day's pass.
//
// HOW DEBRIS CLEARS (each quarter, after the wear).
//   - TUGS. Every Tug IDLE at an orbital node with a debris arm fitted removes
//     `tug_clear_per_quarter`, ascending unit id, never below zero. A tug clears where
//     it is parked: moving it there is the order, so there is no separate verb.
//   - DRAG. Every LOW-ORBIT level falls by `low_orbit_drag` (a fraction) a quarter.
//     High orbit and the Lagrange points never clear by themselves.
//   - A level at or below `k_space_debris_floor` is erased.
//
// THE CALL ORDER. The DAY (economy_system's space day): `run_space_unit_day`, then
// `run_space_battle_day`, then `run_space_debris_day` with both reports. The QUARTER
// (nation_step): `watch_space_losses` before the space-asset step, then after the
// space-unit quarter `run_space_debris_quarter`.
//
// DETERMINISM. `space_debris` is a std::map keyed by the node's total order; every
// pass walks a std::map ascending; float arithmetic over saved inputs in a fixed
// order; no RNG, no clock.
//
// SAVED: world::space_debris, each `space_asset::wear` above zero, and each open
// battle's `space_battle::quiet_days` above zero — the BL-1598 trailing section
// (world_save.hpp). Folded into state_hash when any is non-empty.
// ---------------------------------------------------------------------------

/// The dials. First in-band cut; see the report. Not a Lua key yet: the registry
/// carries these defaults (`recipe_registry::space_debris`).
struct space_debris_params
{
    float debris_per_mass        = 1.0f;   ///< Debris a unit of lost mass leaves.
    float satellite_mass         = 4.0f;   ///< A lost satellite's mass.
    float station_core_mass      = 20.0f;  ///< A lost station core's mass (outside a battle).
    float station_module_mass    = 6.0f;   ///< A lost station module's (or truss's) mass.
    float wear_per_debris        = 0.004f / 3.0f; ///< Wear a tick (a month; a quarter 0.004, BL-1642) per unit of level (1 = lost).
    float unit_damage_per_debris = 0.002f; ///< Hull points a day per unit of level.
    float tug_clear_per_quarter  = 2.0f;   ///< Level one idle Tug removes a tick (a month; a quarter 6.0, BL-1642; name kept).
    float low_orbit_drag         = 0.0274113f; ///< Fraction of a low-orbit level drag clears a tick: 1 - 0.92^(1/3), the quarter's 0.08 a month (BL-1642).
};

/// Below this a level is erased (and the save reader refuses one at or below it).
inline constexpr float k_space_debris_floor = 0.01f;
/// The save reader's ceiling on a level, and the step's.
inline constexpr float k_space_debris_max   = 1.0e7f;
/// The save reader's ceiling on the node count.
inline constexpr std::size_t k_space_debris_nodes_max = 1u << 16;

/// The level at @p node (0 when clean).
float space_debris_at(const world& w, orbital_node node);

/// Add @p mass x `debris_per_mass` to @p node's level, capped at `k_space_debris_max`.
/// A surface node, a non-finite or non-positive mass, adds nothing. Returns what it added.
float add_space_debris(world& w, const space_debris_params& dp, orbital_node node, float mass);

/// What one debris day did — reported, never stored.
struct space_debris_day_report
{
    float deposited   = 0.0f; ///< Debris added (battles, then units destroyed today).
    float damage      = 0.0f; ///< Hull points debris took off units.
    int   units_lost  = 0;
    int   crew_lost   = 0;
    int   passed      = 0;    ///< Pass-through charges (unit x node).
};

/// THE DAY (see the header comment). Nothing at all when the store is empty and the
/// battle day destroyed nothing.
void run_space_debris_day(world& w, const space_debris_params& dp, const space_unit_day_report& units,
                          const space_battle_day_report& battles, space_debris_day_report* out = nullptr);

/// What the quarter step compares against: the assets that could leave debris and
/// were not lost, and the units, before the space steps ran.
struct space_loss_watch
{
    std::vector<entity_id> assets; ///< Ascending: satellites, cores, modules, not lost, at an orbital node.
    struct unit_mark
    {
        entity_id    id = null_entity;
        orbital_node node{};
        float        mass = 0.0f; ///< Dry mass + cargo.
    };
    std::vector<unit_mark> units;  ///< Ascending id.
};

/// Snapshot the watch (pure).
space_loss_watch watch_space_losses(const world& w);

/// What one debris quarter did — reported, never stored.
struct space_debris_quarter_report
{
    float deposited   = 0.0f; ///< From the quarter's losses (the watch).
    int   worn_out    = 0;    ///< Satellites and cores lost to wear.
    float cascade     = 0.0f; ///< Debris their loss added.
    float cleared     = 0.0f; ///< By tugs.
    float dragged     = 0.0f; ///< By drag.
};

/// THE QUARTER (see the header comment). Nothing at all when the store is empty,
/// nothing was lost and no asset carries wear.
void run_space_debris_quarter(world& w, const space_debris_params& dp, const space_loss_watch& watch,
                              space_debris_quarter_report* out = nullptr);
