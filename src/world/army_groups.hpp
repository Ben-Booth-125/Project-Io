#pragma once

// BL-1737 (army groups; docs/military/MILITARY.md § Army groups, Ben 2026-11-22).
//
// An army group is a named, lasting set of one nation's land units, which may span
// tiles. The record (`world::army_groups`) holds only its owner and ordinal; its
// members are the units whose `unit_component::army_group` names it, so a unit that
// dies leaves its group with nothing to update. `prune_army_groups` removes a group
// no unit names, once a day and after every `set_army_group`.
//
// Player-only: the verb is the seat's alone. A rival grouping its units would be a
// new widening under docs/ai/AI_OPPONENT.md § 11, and no grant gives it.
//
// Deterministic: every walk is ascending id; no state outside the world.

#include "corp_command.hpp"
#include "entity.hpp"

#include <cstdint>
#include <string>
#include <vector>

struct world;

/// The most army groups one nation may hold. Reachable by command, so bounded.
inline constexpr std::size_t k_army_groups_max = 99;

/// "1st Army", "2nd Army", "11th Army", "21st Army" ...; "Army" for ordinal 0.
std::string army_group_name(uint32_t ordinal);

/// The group's name, or "" when @p group is not one.
std::string army_group_name(const world& w, entity_id group);

/// The standing units (`world::units`) of @p group, ascending id. A unit aboard a
/// ship is a member too but stands nowhere, so it is not listed.
std::vector<entity_id> army_group_members(const world& w, entity_id group);

/// True when any unit, standing or aboard a ship, names @p group.
bool army_group_has_member(const world& w, entity_id group);

/// @p owner's army groups, ascending ordinal (then id).
std::vector<entity_id> army_groups_of(const world& w, entity_id owner);

/// The lowest ordinal (from 1) @p owner's groups do not use.
uint32_t lowest_free_army_ordinal(const world& w, entity_id owner);

/// Every check `set_army_group` makes, in its order; `applied` when it would land.
/// `actor` is the commanding nation, `unit` the subject, `group` the counterparty,
/// `order` 1 for a new group (with no group named) or 0.
corp_command_result set_army_group_check(const world& w, entity_id actor, entity_id unit,
                                         entity_id group, uint32_t order);

/// The verb. Checks first, so a refusal mutates nothing; on a new group its id goes
/// out through @p out_group.
corp_command_result apply_set_army_group(world& w, entity_id actor, entity_id unit,
                                         entity_id group, uint32_t order,
                                         entity_id* out_group = nullptr);

/// Clears any unit's `army_group` naming no group of its owner, then removes every
/// group no unit names. Returns the groups removed. Idempotent.
std::size_t prune_army_groups(world& w);
