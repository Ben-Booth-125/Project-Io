#pragma once

// ---------------------------------------------------------------------------
// BL-467 — battle state in world: the engagement trigger and the per-tick step
// ---------------------------------------------------------------------------
// THE GAP THIS CLOSES. Before this pass, `resolve_battle` and
// `resolve_campaign_battle` were compiled, harnessed and CALLED BY NOTHING —
// MILITARY.md said so in its "what is absent" list. The military layer could
// compute a fight but could not have one, because nothing discovered that a
// fight should happen and nothing owned it between ticks. This file is both
// halves: the trigger, and the stepping.
//
// THE THREE RULINGS IT IMPLEMENTS (Ben, 2026-08-19, elicitation form):
//
//   1. FOOTPRINT IS THE PROVINCE ENVELOPE. Hostile units standing in the same
//      province are drawn into one battle; the province frames the fight. Units
//      never abstract into a pool — they keep their tiles, and BL-315's
//      tile-token ruling is untouched. The two grains answer different
//      questions: the tile is WHERE a unit is, the province is WHO it is in a
//      fight with.
//   2. TERRAIN IS ONE PAIR for the whole battle, exactly as the resolver already
//      takes it. Per-unit terrain was offered and declined.
//   3. PACING IS SEVERAL ROUNDS PER TICK — `campaign_battle_params::
//      rounds_per_tick`, authored data rather than code.
//
// WHAT MAKES IT DETERMINISTIC, which is the whole difficulty. The containers a
// battle is discovered FROM are unordered (`world::units`, `world::corporations`),
// so every stage recovers order explicitly:
//
//   - candidates are gathered, then SORTED by (province, attacker, defender)
//     before any battle is opened, so creation order cannot depend on hash
//     layout;
//   - `world::battles` is KEPT in that order, so stepping is order-independent;
//   - participating unit ids are stored ASCENDING, because the per-unit loss
//     remainder is assigned by ascending id;
//   - the terrain pair is an argmax broken by ascending tile id;
//   - and no stage consumes an RNG draw outside `step_campaign_battle`, which
//     draws exactly twice per round in a fixed order.
//
// Authority: docs/military/MILITARY.md; the item is backlog.json BL-467.
// HEADLESS. Pure world-layer: no SDL, no Lua, no rendering, no input.
// ---------------------------------------------------------------------------

#include "campaign_battle.hpp"
#include "entity.hpp"

#include <cstdint>
#include <vector>

struct world;
class recipe_registry;

/// What one tick's battle pass did. Reported rather than asserted — the counts
/// are what a harness reads to know the pass is not silently inert, which is the
/// failure mode an engagement trigger actually has.
struct battle_tick
{
    int opened     = 0; ///< Battles created this tick by the trigger.
    int stepped    = 0; ///< Battles that fought at least one round this tick.
    int rounds     = 0; ///< Total rounds fought across all battles.
    int concluded  = 0; ///< Battles that ended this tick (any end reason).
    int withdrew   = 0; ///< Battles ended by an honoured withdrawal request.
    int losses     = 0; ///< Total men removed from unit_component::count.
};

/// Discover new battles, then step every live one. Called from `run_economy_step`
/// immediately BEFORE `run_unit_march`, so a unit that is in contact fights this
/// tick before it can march out next tick — and before `run_unit_upkeep`, whose
/// existing orphan cleanup is what disbands a unit this pass reduced to zero.
/// There is deliberately no second disband mechanism.
battle_tick run_battles(world& w, const recipe_registry& reg, int tick);

/// Request that @p corp break off a battle it is fighting in @p province.
///
/// @p against NAMES THE FIGHT, and it is not optional decoration: a corp can be
/// in more than one battle in one province, because a third corp arriving opens
/// its OWN battles against each existing participant rather than joining theirs.
/// With only (corp, province) the call would leave whichever counterparty sorted
/// lowest — deterministic, but not a decision the player made. Pass `null_entity`
/// to mean "the first, in sorted order", which is the honest reading of an
/// unspecified request rather than a hidden choice.
///
/// Honoured at the TICK BOUNDARY, before the next round batch — which is what
/// makes the withdrawal window a real window rather than a same-instant escape.
/// Returns false and MUTATES NOTHING if there is no such battle, or if @p corp is
/// not a participant in it: this is reachable from the AI-facing seam, where a
/// rejection must leave the world byte-identical.
bool request_withdraw(world& w, entity_id corp, uint32_t province,
                      entity_id against = null_entity);

/// The ground a battle would anchor on, given a defending side's units: the
/// DEFENDER-side tile with the highest `terrain_defence` in the envelope, ties
/// broken by ascending tile id (ruling 2, settled 2026-08-19).
///
/// EXPOSED SO IT CAN BE ASSERTED. The rule is load-bearing — it decides how much
/// the defender's ground is worth in every fight — but a battle is erased the
/// moment it concludes, and with a 1v1 at the shipped pacing that is often the
/// same tick it opened. A rule only testable when a fight happens to run long is
/// a rule that stops being tested quietly, so the choice is a named function
/// rather than a private step inside discovery.
///
/// Returns false and touches nothing if no unit resolves to a real tile.
bool battle_ground(const world& w, const std::vector<entity_id>& defender_units,
                   terrain_substrate& sub, terrain_cover& cov, std::uint8_t& density,
                   terrain_landform& lf);

/// True iff @p unit is a participant in a live battle. The one `if` BL-470's
/// march_unit dispatch left a named gap for: a unit in contact may not be given
/// a new movement order, because walking away from a fight is a WITHDRAWAL with
/// a price, not a march with none.
bool unit_in_battle(const world& w, entity_id unit);

/// The battle @p corp is fighting in @p province, or nullptr. Read-side helper
/// for the seam and the future card (BL-469).
const active_battle* find_battle(const world& w, entity_id corp, uint32_t province);
