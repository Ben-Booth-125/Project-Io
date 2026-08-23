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

/// THE PHASE VOCABULARY (BL-468/BL-469), and the reason it lives in the WORLD
/// layer rather than in either surface that reads it.
///
/// Rounds are mechanically uniform — every one is the same two draws against the
/// same scoring. The phase is a READING of a round, not a property of it: the
/// opening reads as a skirmish, the middle as commitment, a large swing as a
/// crisis, an approach to the rout threshold as wavering. It exists so a player
/// can tell at a glance whether a fight is worth staying in.
///
/// It is derived HERE, once, because two surfaces consume it — the dispatch
/// stream (BL-468) and the card's phase readout (BL-469) — and BL-468's design
/// says in as many words that they must never disagree. Deriving it twice, once
/// per surface, is exactly how they would come to.
enum class battle_phase : uint8_t
{
    opening,    ///< First rounds. Contact made, nothing decided.
    committed,  ///< The middle. Both sides are in it now.
    crisis,     ///< A large swing this round — the fight turned.
    wavering,   ///< A side is near the rout threshold.
    broke_off,  ///< A withdrawal was honoured this tick, with its price paid.
    concluded,  ///< The fight ended. Who holds the field, and what it cost.
};

/// What ONE battle did in ONE tick — the material a surface needs to say it, and
/// nothing more.
///
/// PURE DATA, AND DELIBERATELY. The world layer must not depend on `src/ui`, and
/// the text is a pure function of this record plus the battle identity, so the
/// dispatch layer can never move the simulation — no draw, no state, no ordering
/// effect. BL-468 states that as a requirement; this struct is what makes it
/// structurally true rather than a discipline someone has to keep.
///
/// It is REPORTED rather than stored because a concluded battle is erased at the
/// end of the tick it ends. Without this the aftermath — who held the field and
/// what it cost, which is the one line a player most needs — would be gone before
/// any surface could read it.
struct battle_dispatch
{
    uint32_t  province = 0;
    entity_id attacker = null_entity;
    entity_id defender = null_entity;

    battle_phase phase = battle_phase::opening;

    int rounds_fought = 0;   ///< Total, not this tick's batch.
    int rounds_this_tick = 0;
    int max_rounds = 0;      ///< So a surface can print "round 4 / 6" without knowing params.

    int attacker_strength_permille = 0; ///< After this tick's rounds.
    int defender_strength_permille = 0;
    int attacker_swing_permille = 0;    ///< Change across this tick's batch; negative is loss.
    int defender_swing_permille = 0;

    int attacker_men_lost = 0;          ///< Actual men, this tick.
    int defender_men_lost = 0;

    /// Set only when `phase` is `broke_off` or `concluded`.
    withdrawing_side withdrew = withdrawing_side::none;
    campaign_battle_end end = campaign_battle_end::in_progress;
    entity_id field_held_by = null_entity; ///< Concluded only: the side that stayed.

    /// The seed of the battle's own stream. A surface folds this with the round
    /// index to pick a phrase, so wording is stable for a given fight and varies
    /// between fights — WITHOUT consuming a draw (the BL-290 tongue-bank idiom).
    uint64_t stream_seed = 0;
};

/// The phase a battle reads as, given its state and what just happened. Pure.
battle_phase read_battle_phase(const active_battle& b, int attacker_swing, int defender_swing,
                               const campaign_battle_params& params);

/// The CURRENT price of breaking off, per-mille of remaining strength, with its
/// three terms separated so a surface can show them (BL-469 asks for exactly
/// this: "the three terms legible ... updating as rounds pass").
///
/// Exposed rather than recomputed in the UI: the resolver already owns this
/// arithmetic, and a second copy in a card is a second place for it to drift from
/// what withdrawing actually costs.
struct withdrawal_price
{
    int base = 0;      ///< The flat price of turning your back at all.
    int per_round = 0; ///< Every round already fought adds to it.
    int pursuit = 0;   ///< Scaled by how far BEHIND the withdrawing side is.
    int total = 0;     ///< base + per_round + pursuit, clamped as the resolver clamps it.
};

withdrawal_price quote_withdrawal(const active_battle& b, withdrawing_side side,
                                  const campaign_battle_params& params = {});

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

    /// One record per battle that did something this tick — the material the
    /// dispatch stream (BL-468) and the card (BL-469) read. Includes battles that
    /// CONCLUDED this tick, which the record itself no longer holds.
    std::vector<battle_dispatch> dispatches;
};

/// Discover new battles, then step every live one. Called from `run_economy_step`
/// immediately BEFORE `run_unit_march`, so a unit that is in contact fights this
/// tick before it can march out next tick — and before `run_unit_upkeep`, whose
/// existing orphan cleanup is what disbands a unit this pass reduced to zero.
/// There is deliberately no second disband mechanism.
///
/// TWO DISCOVERY TRIGGERS since BL-571 (nation garrisons): corp-vs-corp
/// directed hostility (the original — `w.corp_hostile_pairs`), and corp-vs-
/// nation over an ACTIVE MERCENARY CONTRACT (`active_mercenary_contract_for`,
/// below) — no `declare_hostile` row is authored for the second; the contract
/// itself is the hostility. See MILITARY.md § Nation garrisons.
battle_tick run_battles(world& w, const recipe_registry& reg, int tick);

// ---------------------------------------------------------------------------
// BL-571 — the second trigger: corp vs. nation, over an active mercenary
// contract
// ---------------------------------------------------------------------------

/// Whether @p corp holds an ACTIVE mercenary contract targeting @p province —
/// the second battle trigger's hostility test (MILITARY.md § Nation
/// garrisons: "no `declare_hostile` row is authored for this pair — the
/// contract itself is the hostility"). Returns the contract's entity id, or
/// `null_entity` when none is active.
///
/// STUB (BL-571, nation garrisons). The real answer is BL-573 (contract
/// templates — Wave 4 of this batch), which has not landed: no contract
/// store exists anywhere in `world` yet. This is hard-coded to `null_entity`
/// always, so `run_battles`' second trigger is fully wired and reachable but
/// cannot fire from any production state today — "so this task is not
/// blocked waiting on wave 4" (the item's own brief). BL-573 replaces this
/// function's BODY with a real lookup; the SIGNATURE already carries
/// everything the trigger needs.
entity_id active_mercenary_contract_for(const world& w, entity_id corp, uint32_t province);

/// Open a battle between @p attacker's and @p defender's units standing in
/// @p province — the existing-battle check, terrain pick and
/// `begin_campaign_battle` call either discovery trigger performs, factored
/// out to one place. ENTITY-ID-AGNOSTIC: @p attacker and @p defender are read
/// only as unit owners, so this opens a corp-vs-corp, corp-vs-nation or
/// (symmetrically) nation-vs-corp battle exactly alike.
///
/// EXPOSED so BOTH triggers share one code path AND so a harness can drive a
/// corp-vs-nation battle directly, without a live mercenary contract to
/// discover it through (BL-573 has not landed — see
/// `active_mercenary_contract_for`) — the same "expose it so it can be
/// asserted" precedent `battle_ground` already sets.
///
/// Returns false and leaves `w.battles` untouched if the pair is already
/// fighting in @p province, if either side has no standing unit there, or if
/// `stack_can_fight` rejects either side (§ the degenerate 0-vs-0 case,
/// `stack_can_fight`'s own doc comment).
bool open_battle(world& w, int tick, uint32_t province, entity_id attacker, entity_id defender);

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
/// for the seam and the card (BL-469).
const active_battle* find_battle(const world& w, entity_id corp, uint32_t province);

/// The battle a click on @p province should select, or nullptr.
///
/// THE PLAYER'S OWN FIGHT FIRST, then the sorted order. More than one battle can
/// stand in one province — a third corp arriving opens its OWN battles against
/// each participant rather than joining theirs — so "the first one" is a choice,
/// and this is the honest one: the fight you are in is the one you need the card
/// for. Falling back to sorted order keeps the pick deterministic when none of
/// them is yours. Flagged for Ben; see the review log.
const active_battle* first_battle_in(const world& w, uint32_t province);
