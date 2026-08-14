#pragma once

#include "combat.hpp"
#include "components.hpp"
#include "entity.hpp"

#include <cstdint>
#include <vector>

// ---------------------------------------------------------------------------
// Campaign battle resolver (BL-315) — the SECOND combat path, deliberately.
//
// WHY THERE ARE TWO. combat.hpp's resolve_battle answers "province beats
// province, this year" in one scored evaluation, because the Era -1 sim runs
// millions of those and cannot afford a loop. This file answers a different
// question: two forces standing on a TILE, fought out over a short span, with
// a player watching who may pull out partway. Folding both into one function
// would either flatten the campaign fight into a dice roll or drag tile-grain,
// mid-fight state into a generation pass with no room for it. Ben ruled the
// two-path split 2026-08-13, overturning this item's own earlier
// engine-parity strand. Do not "unify" it back.
//
// WHAT IS SHARED, SO THE TWO DO NOT DRIFT. The roster (unit_roster.hpp) and
// the CALIBRATION — the class matchup matrix, the base powers, the terrain and
// supply scalars — are single-sourced: every round below scores its powers by
// calling resolve_battle itself and reading the two power numbers out. What
// this file adds on top is the round structure, the seeded swing, and
// withdrawal. It deliberately does NOT reuse resolve_battle's loss/decisiveness
// model, which is tuned for a whole war-year in one shot; per-round attrition
// is this file's own (see campaign_battle_params).
//
// DETERMINISM. The randomness is real to the player and absent to the engine:
// every draw comes from a splitmix64 stream folded from the battle's own
// identity (participants + tile + tick + world seed), consumed in a fixed
// order — attacker swing, then defender swing, once per round. No wall clock,
// no global generator, and nothing whose consumption order depends on the
// iteration order of a container. The same save replayed fights the same
// battle; the player, who cannot see the seed, faces a genuinely uncertain one.
//
// HEADLESS. Pure world-layer: no SDL, no Lua, no rendering, no input.
// ---------------------------------------------------------------------------

/// What a battle is folded from to seed its stream. Every field must be stable
/// across a save/load round-trip — that is the whole replay guarantee. Note the
/// attacker/defender fields are NOT interchangeable: the roles are asymmetric
/// in the resolution, so they are asymmetric in the seed too.
struct campaign_battle_identity
{
    entity_id attacker      = null_entity; ///< Commanding force / corp entity.
    entity_id defender      = null_entity;
    uint32_t  tile_index    = 0;           ///< The tile the fight stands on.
    uint64_t  tick          = 0;           ///< Campaign tick the battle opened.
    uint32_t  world_seed    = 0;           ///< So two worlds with equal ids still differ.
};

/// Tuning for the campaign path, kept as data so changing the feel is a data
/// change rather than a code change (the corp_ai_params precedent). All
/// per-mille unless named otherwise.
struct campaign_battle_params
{
    /// How many rounds a fight runs before it is called a stalemate. "A short
    /// time" (Ben) — long enough to read the fight and pull out, short enough
    /// that a campaign tick is not a tactical game.
    int max_rounds = 6;

    /// Half-width of the per-round power swing. 600 = each side's scored power
    /// is multiplied by a uniform draw in [400, 1600] that round. This is the
    /// whole uncertainty budget: raise it and strength predicts victory less.
    /// Widened from 300 (BL-400, NR-204): at 300 a 1.4:1 attacker won 99% of
    /// fights, which Ben ruled too safe. Measured at 600 over 1000 seeds per
    /// ratio: 1.0:1 -> 50%, 1.1:1 -> 63%, 1.2:1 -> 76%, 1.4:1 -> 89%,
    /// 2.0:1 -> 99.8% — a 1.4:1 edge now loses about one fight in ten while
    /// a 2:1 edge stays near-certain. Picked from the printed sweep curve
    /// (campaign_battle_harness's C4 table), not asserted from arithmetic.
    int swing_permille = 600;

    /// Strength (per-mille of the force that marched in) at or below which a
    /// side breaks and the fight ends early.
    int rout_threshold = 300;

    /// Per-round attrition on the side that LOST the round: a flat base plus a
    /// share of the round's margin. Applied multiplicatively to that side's
    /// REMAINING strength, so a long fight compounds rather than sums.
    int round_loser_loss_base   = 90;
    int round_loser_loss_margin = 400;

    /// Per-round attrition on the side that WON the round. Falls as the round
    /// gets more lopsided — a rout costs the winner little.
    int round_winner_loss_base   = 45;
    int round_winner_loss_margin = 20;
    int round_winner_loss_floor  = 5;

    /// WITHDRAWAL COST. Disengaging is a first-class verb, not a failure state,
    /// but it is never free — otherwise committing force carries no risk and
    /// the fight is a coin flip you can always re-toss. The cost has three
    /// terms, and the second and third are what make late withdrawal dear:
    ///   base        — the flat price of turning your back at all;
    ///   per_round   — every round already fought adds to it (a force in
    ///                 contact cannot simply walk away);
    ///   pursuit     — a share of how far BEHIND the withdrawing side is, so
    ///                 breaking off while losing badly is where armies die.
    /// Note the rounds already fought have ALSO cost their own attrition, so
    /// the total price of a late withdrawal compounds twice over.
    int withdraw_base_permille      = 20;
    int withdraw_per_round_permille = 25;
    int withdraw_pursuit_scale      = 400;
};

/// Which side is breaking off. `none` means fight it out.
enum class withdrawing_side : uint8_t
{
    none     = 0,
    attacker = 1,
    defender = 2,
};

/// How the fight ended. Withdrawal is its own end state rather than a flavour
/// of defeat — the whole point is that pulling out is a decision, not a loss.
enum class campaign_battle_end : uint8_t
{
    in_progress       = 0, ///< Only seen on a live campaign_battle_state.
    attacker_broken   = 1, ///< Attacker fell to the rout threshold.
    defender_broken   = 2,
    attacker_withdrew = 3,
    defender_withdrew = 4,
    stalemate         = 5, ///< max_rounds elapsed with both sides still standing.
};

/// One round's public record — what a watching player is shown, and what a
/// commander reads before deciding whether to pull out.
struct campaign_battle_round
{
    int round = 0;                    ///< 1-based.
    int attacker_power = 0;           ///< Scored power AFTER the round's swing.
    int defender_power = 0;
    battle_result round_winner = battle_result::defender_victory;
    int attacker_strength_permille = 0; ///< Remaining strength AFTER this round.
    int defender_strength_permille = 0;
};

/// The result of a whole campaign battle.
///
/// Losses are per-mille fractions of each side's OWN committed count, exactly
/// as battle_outcome's are — this resolver never mutates a unit_component, so
/// applying the fraction to real units is the caller's job.
struct campaign_battle_outcome
{
    campaign_battle_end end = campaign_battle_end::stalemate;

    /// Who held the field. On a withdrawal the side that stayed holds it, even
    /// if it was losing on points — ground is held by whoever is still on it.
    battle_result result = battle_result::defender_victory;

    int rounds_fought = 0;

    int attacker_losses_permille = 0; ///< 1000 - final strength.
    int defender_losses_permille = 0;
    int attacker_strength_permille = 0; ///< What marched away, 0..1000.
    int defender_strength_permille = 0;

    /// How lopsided the finish was, 0..1000, from the final strength gap.
    int decisiveness = 0;

    /// The folded stream seed, carried out for logging and replay diffing.
    uint64_t stream_seed = 0;

    std::vector<campaign_battle_round> rounds;
};

/// A battle in progress. THE SPAN IS MODELLED EXPLICITLY: a caller steps this
/// round by round, so the withdrawal window is a real window rather than a
/// parameter pretending to be one. The UI drives it a round at a time; a
/// headless caller runs resolve_campaign_battle below.
///
/// Everything mutable to the fight lives here, including the rng state, so
/// stepping is a pure function of (state, params).
struct campaign_battle_state
{
    std::vector<army_stack_entry> attacker;  ///< The forces that marched in, unscaled.
    std::vector<army_stack_entry> defender;
    doctrine_row attacker_doctrine{};
    doctrine_row defender_doctrine{};

    terrain_composition terrain_comp = terrain_composition::grassland;
    terrain_landform    terrain_lf   = terrain_landform::plains;
    season              battle_season = season::summer;
    int attacker_supply = 1000;
    int defender_supply = 1000;

    int attacker_strength_permille = 1000; ///< Remaining share of what marched in.
    int defender_strength_permille = 1000;

    uint64_t rng_state  = 0; ///< Advanced once per draw; never re-seeded mid-fight.
    uint64_t stream_seed = 0; ///< The folded seed, kept for the outcome record.

    int rounds_fought = 0;
    campaign_battle_end end = campaign_battle_end::in_progress;

    std::vector<campaign_battle_round> rounds;
};

/// Fold @p id into the battle's stream seed. Exposed so a caller can log it
/// and so a harness can assert that two different identities really do produce
/// different streams.
uint64_t campaign_battle_seed(const campaign_battle_identity& id);

/// Open a battle. Nothing is resolved yet — round 1 happens on the first
/// step_campaign_battle call, which is what gives a commander a window at all.
campaign_battle_state begin_campaign_battle(const campaign_battle_identity&      id,
                                            const std::vector<army_stack_entry>& attacker,
                                            const doctrine_row&                  attacker_doctrine,
                                            const std::vector<army_stack_entry>& defender,
                                            const doctrine_row&                  defender_doctrine,
                                            terrain_composition                  terrain_comp,
                                            terrain_landform                     terrain_lf,
                                            season                               battle_season,
                                            int                                  attacker_supply,
                                            int                                  defender_supply);

/// Fight one round. Returns false once the battle is over (@p st.end set), at
/// which point further calls are no-ops. Consumes exactly two draws from the
/// stream — attacker swing, then defender swing — in that fixed order.
bool step_campaign_battle(campaign_battle_state& st,
                          const campaign_battle_params& params = {});

/// Break off. Applies the withdrawal cost to @p side and ends the fight; the
/// side that stayed holds the field. Consumes no draws — disengaging is a
/// decision, not another gamble. A no-op on an already-finished battle or on
/// withdrawing_side::none.
void withdraw_campaign_battle(campaign_battle_state& st,
                              withdrawing_side              side,
                              const campaign_battle_params& params = {});

/// Read the finished (or in-progress) state as an outcome record.
campaign_battle_outcome campaign_battle_result(const campaign_battle_state& st);

/// Run a battle to conclusion with a SCRIPTED withdrawal plan — the headless
/// and AI-side path, and the one a harness drives. @p withdraw_after_round is
/// 1-based and means "fight that many rounds, then break off"; pass <= 0 (or
/// withdrawing_side::none) to fight it out.
///
/// This is a convenience wrapper over begin/step/withdraw and adds no rules of
/// its own, so a scripted run and a player-driven one resolve identically.
campaign_battle_outcome resolve_campaign_battle(const campaign_battle_identity&      id,
                                                const std::vector<army_stack_entry>& attacker,
                                                const doctrine_row&                  attacker_doctrine,
                                                const std::vector<army_stack_entry>& defender,
                                                const doctrine_row&                  defender_doctrine,
                                                terrain_composition                  terrain_comp,
                                                terrain_landform                     terrain_lf,
                                                season                               battle_season,
                                                int                                  attacker_supply,
                                                int                                  defender_supply,
                                                withdrawing_side                     withdrawer = withdrawing_side::none,
                                                int                                  withdraw_after_round = 0,
                                                const campaign_battle_params&        params = {});
