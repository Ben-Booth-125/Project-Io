#pragma once
// BL-1452 (carried supplies; docs/military/MILITARY.md § Carried supplies, Ben 2026-10-29).
//
// A unit carries three stocks — food, ammunition and equipment — each per-mille of
// full (`unit_component::food_permille`, `ammo_permille`, `equipment_permille`). Once a
// DAY (`run_carried_supplies_day`, from the daily military pass) each stock is drained
// by what the unit did and refilled by the share its supply factor names:
//
//   drain     food: `food_per_day` every day (the ration);
//             ammunition: `ammo_per_battle_day` on a day the unit fought;
//             equipment: `equipment_attrition_per_day` every day, plus
//             `equipment_per_battle_day` on a day it fought;
//   resupply  supply_factor_permille x today's spend / 1000 (floor) — the share of
//             today's need — so a unit its network covers in full stays full whatever
//             it does, and a unit covered at half loses half its daily need a day.
//             At FULL cover only, the network also RESTOCKS one full daily rate (food:
//             the ration; ammunition: a battle day; equipment: attrition + a battle
//             day), capped at full: without it a stock once emptied could never
//             refill, and a fully supplied unit at zero food would starve for ever
//             (BL-1452 implementer's call; MILITARY.md says how a unit drains, not
//             how an empty one refills). Short cover never catches up.
//             The supply factor is the ONE supply quantity (the one decay rule,
//             nation_step.cpp `run_nation_garrison_upkeep`); there is no second.
//
// The round reads the stocks, not the supply factor (`carried_attack_permille`,
// `carried_combat_permille`): ammunition (floored at 20%) and equipment each scale
// every combat stat in proportion and multiply with morale; below full food the
// attack takes a flat -15%. At zero food the unit STARVES: it loses its full
// establishment over 180 days (integer carry, `starvation_carry`) and its readiness is
// capped at its men over establishment. Integer arithmetic throughout; each unit's day
// reads only itself, walked in ascending unit id.

#include "components.hpp"

#include <set>

struct world;

/// The authored daily rates, `scripts/economy.lua` § `economy.military.carried_supplies`.
/// Per-mille of a full stock per day. The defaults match the authored values so a
/// hand-built harness registry runs the same supply day as the game.
struct carried_supply_params
{
    int food_per_day                = 33; ///< The ration: a full stock feeds ~30 days unsupplied.
    int ammo_per_battle_day         = 50; ///< Spent per day fighting: 20 days of fire, floor aside.
    int equipment_attrition_per_day = 1;  ///< Wear on the march and in camp.
    int equipment_per_battle_day    = 10; ///< Lost to battle, per day fighting.
};

/// DESIGN CONSTANTS (Ben, 2026-10-29), not rates: the doc fixes them.
inline constexpr int k_hunger_penalty_permille = 150; ///< Flat -15% attack and readiness recovery.
inline constexpr int k_ammo_floor_permille     = 200; ///< Out of shells, it still fights with small arms.
inline constexpr int k_starvation_days         = 180; ///< Days a starving unit takes to lose its establishment.

/// True when the unit's food is below full: the flat hunger step applies.
inline bool unit_hungry(const unit_component& u) { return u.food_permille < 1000; }

/// True when the unit's food is gone: it starves.
inline bool unit_starving(const unit_component& u) { return u.food_permille <= 0; }

/// The ammunition and equipment scale on EVERY combat stat (soft, hard, defence,
/// breakthrough), per-mille: ammunition floored at 20%, equipment unfloored,
/// multiplied. 1000 for a full unit.
int carried_combat_permille(const unit_component& u);

/// THE SUPPLY TERM ON A UNIT'S HITS, per-mille: `carried_combat_permille` with the
/// flat hunger step below full food. The land round multiplies hits by it, beside
/// morale (battle_system.cpp `build_combatants` -> `land_combatant::supply`).
int carried_attack_permille(const unit_component& u);

/// The readiness-recovery scale, per-mille: 850 when hungry, else 1000.
int carried_recovery_permille(const unit_component& u);

/// The readiness ceiling starvation imposes: men over establishment, per-mille, when
/// the unit is starving; 1000 otherwise.
int carried_readiness_cap(const unit_component& u);

/// What one supply day did, for the day result and the harnesses.
struct carried_supplies_day
{
    int hungry   = 0; ///< Units below full food at the day's end.
    int starving = 0; ///< Units at zero food at the day's end.
    int starved  = 0; ///< Men lost to starvation today.
};

/// ONE DAY of carried supplies for every unit with men standing, ascending unit id:
/// drain, resupply at the supply factor's share, starvation, the readiness cap.
/// @p fought is the units in contact today (they fought today's rounds). A unit it
/// reduces to zero men is removed by the one disband path (`disband_orphaned_units`).
carried_supplies_day run_carried_supplies_day(world& w, const carried_supply_params& p,
                                              const std::set<entity_id>& fought);

/// True when `run_carried_supplies_day` would change some unit out of contact today
/// — what keeps the daily military pass walking a quiet quarter while a unit's
/// stocks are moving. A unit fully supplied and full is inert.
bool carried_supplies_pending(const world& w, const carried_supply_params& p);
