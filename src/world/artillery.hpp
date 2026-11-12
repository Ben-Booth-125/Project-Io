#pragma once

// ---------------------------------------------------------------------------
// BL-1643 — artillery fires shells (MILITARY.md § Artillery fires shells, Ben 2026-11-11)
// ---------------------------------------------------------------------------
// Artillery is no missile launcher. It has a RANGE in tiles - the class's
// `artillery_params::base_range` plus its gun's `kit_item::art_range` - and each day a
// standing battery STRIKES one hostile unit within that range from where it stands,
// without entering the target's province (`run_artillery_day`, on the daily military
// pass). Three kit traits deepen it (`k_art_*`, unit_designs.hpp): self-propelled guns
// march as fast as the force they support (`artillery_march_rate`), counter-battery radar
// answers hostile artillery in range first, and precision shells land more of each strike.
//
// THE STRIKE uses the land round's arithmetic: the battery's soft and hard attack against
// the target's hardness, per man, `rounds_per_day` rounds, times the round's
// `day_damage_permille`, times `strike_share_permille` and the landed share - then into men
// and readiness through the same bank tactical bombing uses (`world::air_hit_bank`). No
// draw anywhere: integer arithmetic over ascending ids.
//
// The fronts place a front's artillery behind the line at its range (fronts.cpp,
// `front_battery_tile`); a rival's fronts do the same, through the same step.
//
// DETERMINISM. Ascending unit ids; integer arithmetic; no RNG.
// HEADLESS. Pure world layer: no SDL, no Lua.
// ---------------------------------------------------------------------------

#include "components.hpp"
#include "entity.hpp"

#include <cstdint>
#include <set>
#include <vector>

struct world;
class recipe_registry;

/// The range of @p u in tiles: the class's base plus its recorded kit's guns, never past a
/// unit's sight (`missile_params::sight_tiles`), so a battery never fires blind. 0 for any
/// unit that is not artillery.
int artillery_range_tiles(const recipe_registry& reg, const unit_component& u);

/// The `k_art_*` traits of @p u's recorded kit; 0 for any unit that is not artillery.
uint8_t artillery_traits(const unit_component& u);

/// Is @p u a battery that may fire today: artillery with men, standing (no march order),
/// on the ground (not aboard, not in the surf), not refitting and not in @p fighting.
bool artillery_ready(const recipe_registry& reg, const unit_component& u, entity_id uid,
                     const std::set<entity_id>& fighting);

/// The hostile unit battery @p uid would strike today, or null: of the units of a nation
/// its owner has declared hostile (the battle trigger's directed test) standing on land
/// within its range - hostile artillery first when it carries counter-battery radar, then a
/// unit in a live battle, then the nearest, ties to the lower id.
entity_id artillery_target(const world& w, const recipe_registry& reg, entity_id uid,
                           const std::set<entity_id>& fighting);

/// One strike's men killed, by whose battery on whose unit: the war's input (`note_war_losses`).
struct artillery_loss
{
    entity_id firer  = null_entity; ///< The battery's nation.
    entity_id struck = null_entity; ///< The struck unit's nation.
    int       men    = 0;           ///< Men the strike killed (> 0).
};

/// What one day of artillery fire did.
struct artillery_day_tick
{
    int strikes    = 0; ///< Batteries that fired at a target.
    int men_killed = 0; ///< Men the strikes killed, summed.
    std::vector<artillery_loss> losses; ///< Each killing strike, in battery id order.
};

/// THE DAY'S BOMBARDMENT. Every ready battery, ascending id, picks its target and its hits
/// against the START of the day; then every strike lands. So two batteries shelling each
/// other trade fire on equal terms whatever their ids. A unit the strikes take to no men is
/// left for the day's disband (`disband_orphaned_units`). Men killed emit `force_used` as a
/// battle's do, and are returned in `losses` for the war's battle inputs. @p fighting is the
/// set of units in live battles at the start of the step.
artillery_day_tick run_artillery_day(world& w, const recipe_registry& reg, const std::set<entity_id>& fighting);

/// Is any battery able to strike today? The daily pass's wake test.
bool artillery_fire_pending(const world& w, const recipe_registry& reg);

/// The march rate (tiles of traversal cost a day) of @p u given its class's @p base rate:
/// a self-propelled battery assigned to a front marches as fast as the force it supports -
/// the slowest of that front's other units, when that is faster than its own class - and
/// every other unit at @p base.
float artillery_march_rate(const world& w, const recipe_registry& reg, const unit_component& u, float base);
