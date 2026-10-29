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
//   1. FOOTPRINT IS THE DEFENDED TILE (Ben, 2026-10-27, overturning the
//      2026-08-19 province envelope, which let a line of units along a province
//      boundary make attack impossible). A battle opens when a unit tries to
//      march onto a tile a nation it is hostile to holds; it is fought over that
//      tile, and each side's stacks on tiles adjacent to it, out of every other
//      battle, join. Units never abstract into a pool — they keep their tiles.
//   2. TERRAIN IS ONE PAIR for the whole battle, exactly as the resolver already
//      takes it. Per-unit terrain was offered and declined.
//   3. PACING IS BY THE DAY (Ben, 2026-10-28, overturning "several rounds per
//      tick"): `campaign_battle_params::rounds_per_day` rounds every day, from the
//      daily military pass (`run_military_day`, economy_system.hpp), and contact
//      is checked every day.
//
// WHAT MAKES IT DETERMINISTIC, which is the whole difficulty. The containers a
// battle is discovered FROM are unordered (`world::units`, `world::corporations`),
// so every stage recovers order explicitly:
//
//   - the attacks a march pass registers are SORTED by (tile, attacker,
//     defender) before any battle is opened, so creation order cannot depend on
//     visit order or hash layout;
//   - `world::battles` is KEPT in that order, so stepping is order-independent;
//   - participating unit ids are stored ASCENDING, because the per-unit loss
//     remainder is assigned by ascending id;
//   - the ground is the defended tile, so there is no pick to order;
//   - and no stage consumes an RNG draw outside `step_campaign_battle`, which
//     draws exactly twice per round in a fixed order.
//
// Authority: docs/military/MILITARY.md; the item is backlog.json BL-467.
// HEADLESS. Pure world-layer: no SDL, no Lua, no rendering, no input.
// ---------------------------------------------------------------------------

#include "campaign_battle.hpp"
#include "entity.hpp"

#include <cstdint>
#include <set>
#include <vector>

struct world;
class recipe_registry;

/// THE PHASE VOCABULARY (BL-468/BL-469), and the reason it lives in the WORLD
/// layer rather than in either surface that reads it.
///
/// Rounds are mechanically uniform — every one is the same two draws against the
/// same scoring. The phase is a READING of a round, not a property of it: the
/// opening reads as a skirmish, the middle as commitment, a large swing as a
/// crisis, readiness running low as wavering. It exists so a player
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
    wavering,   ///< A side's readiness is at or below `wavering_readiness`.
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
    entity_id tile     = null_entity; ///< The defended tile (the battle's identity).
    uint32_t  province = 0;           ///< The province `tile` lies in, for the text.
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

/// The CURRENT price of breaking off, per-mille of the side's standing men, with its
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

/// Step every live battle by ONE DAY: `rounds_per_day` rounds each (BL-1445, move
/// by days). Called from the daily military pass (`run_military_day`) immediately
/// BEFORE `run_unit_march`, so a unit that is in contact fights today before it can
/// march on. A unit this pass reduces to zero men is removed the same day by the one
/// orphan cleanup (`disband_orphaned_units`), which the daily pass runs straight after
/// this (Ben, 2026-10-28). There is deliberately no second disband mechanism.
/// Out-of-contact recovery is NOT stepped here: the daily pass calls
/// `recover_out_of_contact` after the march.
///
/// THERE IS NO DISCOVERY HERE ANY MORE (Ben, 2026-10-27). The province
/// co-presence trigger is gone: a battle opens only when a march tries to step
/// onto a tile a nation the marcher is hostile to holds, which `run_unit_march`
/// registers and opens through `open_tile_battles`. Standing next to an enemy, or
/// in its province, starts nothing.
///
/// When a battle concludes with the ATTACKER holding the field, its attacking
/// stack (`active_battle::attacking_units`, still in the fight and standing)
/// advances onto the defended tile — unless a unit of any other nation still
/// stands there. Joined stacks stay where they stood.
battle_tick run_battles(world& w, const recipe_registry& reg, int tick);

/// Out of contact, every unit recovers: readiness toward its morale (scaled by its
/// supply, never above it), morale toward its supply. DAILY (Ben, 2026-10-28), from
/// the daily military pass after the march; the authored rates
/// (`readiness_recovery_permille`, `morale_recovery_permille`) are per day. Each
/// unit's step reads only itself, so the visit order cannot matter; a fully supplied
/// unit at full readiness and morale is untouched.
void recover_out_of_contact(world& w, const campaign_battle_params& p = {});

/// True when `recover_out_of_contact` would change some unit today - what keeps the
/// daily military pass walking a quiet quarter while units are still recovering.
bool recovery_pending(const world& w, const campaign_battle_params& p = {});

/// One attack a march pass registered: @p unit, of @p attacker, tried to step
/// onto @p tile, which @p defender's units hold.
struct tile_attack
{
    entity_id tile     = null_entity;
    entity_id attacker = null_entity;
    entity_id defender = null_entity;
    entity_id unit     = null_entity;
    /// @p unit's order as it stood when the attack registered. The march pass clears
    /// the live order; `open_tile_battles` puts this back on every unit of the stack
    /// that the battle did NOT take (a refused open, or a unit already pulled into
    /// another fight), so a refused attack never leaves its unit idle.
    movement_order order;
};

/// Open a battle over @p tile, which @p defender's units hold, attacked by
/// @p attacking_units of @p attacker (the stack whose march made the attack).
///
/// SIDES (MILITARY.md § Battles, "adjacent stacks join"): the attacking stack,
/// plus @p attacker's units on tiles hex-adjacent to @p tile that are in no live
/// battle; the defending stack on @p tile (whatever else it is fighting), plus
/// @p defender's units on adjacent tiles in no live battle. EACH SIDE DRAWS ON
/// ITS OWN NATION ONLY — an ally's stack beside the tile does not join. The
/// ground is @p tile.
///
/// Returns false and leaves `w.battles` untouched if the pair is already
/// fighting over @p tile (in either role order), if no defending unit stands on
/// it, or if no attacking unit has readiness left to start a fight (a spent
/// attacker starts nothing; spent defenders are still caught and overrun).
/// ENTITY-ID-AGNOSTIC: the two sides are read only as unit owners. Exposed so a
/// harness can open a battle without a march.
bool open_tile_battle(world& w, int tick, entity_id tile, entity_id attacker, entity_id defender,
                      const std::vector<entity_id>& attacking_units);

/// Open every battle @p attacks registered, in (tile, attacker, defender) order,
/// each identity's units merged into one attacking stack. So two attacks onto
/// one tile in one tick open it once, and which battle claims an adjacent stack
/// first is a property of the ids, never of the march's visit order.
///
/// AN ORDER IS SPENT ONLY BY THE BATTLE IT OPENS. Every attacking unit the opened
/// battle's `attacking_units` does not hold - the whole stack when the open is
/// refused (the defender walked off in the same pass, the attacker is spent, the
/// pair already fights there), or a unit another battle of this pass pulled in
/// first - gets back the order it registered with (`tile_attack::order`), so it
/// carries on next tick against fresh state: onto the tile if it is now empty, an
/// attack again if the stack is still there, or held by its battle until released.
/// The restore runs in the same sorted order as the opens, so it adds no
/// dependence on the visit order.
/// @return battles opened.
int open_tile_battles(world& w, int tick, std::vector<tile_attack> attacks);

/// Request that @p corp break off the battle it is fighting over @p tile.
///
/// @p against NAMES THE FIGHT: a nation can be in more than one battle over one
/// tile, because a third nation attacking the same tile opens its OWN battle.
/// Pass `null_entity` to mean "the first, in sorted order", which is the honest
/// reading of an unspecified request rather than a hidden choice.
///
/// Honoured at the TICK BOUNDARY, before the next round batch — which is what
/// makes the withdrawal window a real window rather than a same-instant escape.
/// Returns false and MUTATES NOTHING if there is no such battle, or if @p corp is
/// not a participant in it: this is reachable from the AI-facing seam, where a
/// rejection must leave the world byte-identical.
bool request_withdraw(world& w, entity_id corp, entity_id tile,
                      entity_id against = null_entity);

/// The ground a battle over @p tile is fought on: the defended tile's own
/// terrain (ruling 2, since 2026-10-27). Exposed so it can be asserted. Returns
/// false and touches nothing if @p tile is not a real tile.
bool battle_ground(const world& w, entity_id tile,
                   terrain_substrate& sub, terrain_cover& cov, std::uint8_t& density,
                   terrain_landform& lf);

/// True iff @p unit is a participant in a live battle. The one `if` BL-470's
/// march_unit dispatch left a named gap for: a unit in contact may not be given
/// a new movement order, because walking away from a fight is a WITHDRAWAL with
/// a price, not a march with none.
bool unit_in_battle(const world& w, entity_id unit);

/// The battle @p corp is fighting over @p tile, or nullptr.
const active_battle* find_battle(const world& w, entity_id corp, entity_id tile);

/// The battle a click on @p tile should select, or nullptr: the player's own
/// fight first, then the sorted order.
const active_battle* first_battle_at(const world& w, entity_id tile);

/// The battle a click on @p province should select, or nullptr — every battle
/// over a tile in @p province is a candidate (the canvas's province rung).
///
/// THE PLAYER'S OWN FIGHT FIRST, then the sorted order. More than one battle can
/// stand in one province, so "the first one" is a choice,
/// and this is the honest one: the fight you are in is the one you need the card
/// for. Falling back to sorted order keeps the pick deterministic when none of
/// them is yours. Flagged for Ben; see the review log.
const active_battle* first_battle_in(const world& w, uint32_t province);

// ---------------------------------------------------------------------------
// One nation per tile (Ben, 2026-10-27; MILITARY.md § Marching)
// ---------------------------------------------------------------------------

/// Every live battle's units, both sides — one walk, for a pass that asks
/// `unit_in_battle` of many units.
std::set<entity_id> units_in_live_battles(const world& w);

/// Every tile a unit (count > 0) of a nation OTHER than @p owner stands on.
std::set<entity_id> foreign_held_tiles(const world& w, entity_id owner);

/// What `resolve_shared_tiles` did.
struct shared_tile_fix
{
    int tiles     = 0; ///< tiles that held more than one nation's units
    int stepped   = 0; ///< units moved off one
    int disbanded = 0; ///< units with nowhere to step, disbanded (erased)
};

/// ONE NATION PER TILE, restored on a world that breaks it - a pre-v63 save, whose
/// rules let two nations stand on one tile (`read_world_snapshot` calls this for
/// such a stream). Tiles in ascending id: the nation whose LOWEST-ID unit (count >
/// 0) stands there keeps the tile; every other nation's units there, ascending id,
/// step off by the retreat rule (`retreat_tile`, without the battle's province
/// term): one crossable neighbour, land, held by no other nation's units as they
/// stand at that moment, preferring the unit's own nation's ground, ties to the
/// lowest tile id. A unit that steps off loses its order (its path started where it
/// no longer stands). A unit with nowhere to go is DISBANDED - erased, and struck
/// from every battle's lists. A world that keeps the rule is untouched.
shared_tile_fix resolve_shared_tiles(world& w);

/// The march path @p from -> @p dest on @p body for a unit of @p owner, in TRAVEL
/// order (path[0] == @p from), solved around every tile in @p blocked save @p dest
/// itself. The cached `intra_body_path` when its route crosses no blocked tile —
/// so a world with no contact marches exactly as before — else a search around
/// them (`intra_body_path_avoiding`). Empty if unreachable.
std::vector<entity_id> march_path(world& w, entity_id body, entity_id from, entity_id dest,
                                  const std::set<entity_id>& blocked);
