#pragma once

#include "combat.hpp"
#include "components.hpp"
#include "entity.hpp"
#include "land_combat_stats.hpp" // BL-1421

#include <cstdint>
#include <vector>

// ---------------------------------------------------------------------------
// Campaign battle resolver (BL-315) — the SECOND combat path, deliberately.
//
// WHY THERE ARE TWO. combat.hpp's resolve_battle answers "region beats
// region, this year" in one scored evaluation, because the Era -1 sim runs
// millions of those and cannot afford a loop. This file answers a different
// question: two forces standing on a TILE, fought out over a short span, with
// a player watching who may pull out partway. Folding both into one function
// would either flatten the campaign fight into a dice roll or drag tile-grain,
// mid-fight state into a generation pass with no room for it. Ben ruled the
// two-path split 2026-08-13, overturning this item's own earlier
// engine-parity strand. Do not "unify" it back.
//
// NOTHING IS SHARED BUT THE TERRAIN TABLE (Ben, 2026-10-26; MILITARY.md § The
// campaign force model). A round here never calls resolve_battle. Each unit
// deals soft attack against the target's soft share and hard attack against its
// hard share; the target absorbs with defence or breakthrough, raised by
// terrain_defence; landed damage splits between men (HP) and readiness. A unit
// at readiness 0 leaves the fight. The 2026-08-13 reading that the two paths
// shared a calibration is overturned: they answer different questions with
// different models, and only terrain_combat's table is read by both.
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
    /// THE ENVELOPE THE FIGHT IS FRAMED BY — the province id since BL-467
    /// (Ben's ruling 1, 2026-08-19: "hostile units standing in the same province
    /// are drawn into one battle; the province frames the fight").
    ///
    /// It replaces the tile index in the same slot of the same fold, deliberately:
    /// BL-467's design says to fold the province WHERE campaign_battle_identity
    /// folds tile_index today, same splitmix64 shape, role order preserved. Doing
    /// it in place rather than appending a field keeps the stream one field wide
    /// and keeps the asymmetry argument below unchanged.
    ///
    /// Units still stand on TILES — BL-315's tile-token ruling is untouched. The
    /// two grains answer different questions: the tile is where a unit is, the
    /// province is who it is in a fight with.
    uint32_t  province      = 0;
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

    /// BL-1421: THE LAND ROUND. Hits are milli-hits, per man per round, on the
    /// scale of unit_roster.cpp's class table.
    ///
    /// Absorbed hits — those within the target's defence (or breakthrough) —
    /// land at this share; hits beyond it land in full. The gap is what makes
    /// attacking dear: infantry absorbs with defence when it holds and with its
    /// far smaller breakthrough when it goes forward.
    int absorbed_land_permille = 250;

    /// The share of landed damage that kills men (HP); the rest spends
    /// readiness. Readiness takes the larger share, so most fights end in a
    /// retreat rather than a death — which is how real units fail.
    int hp_share_permille = 300;

    /// Milli-hits of HP damage that kill one man.
    int hp_cost = 400;

    /// Milli-hits of readiness damage, per man, that spend a unit's whole
    /// readiness.
    int readiness_cost = 185;

    /// How much the battle ground's terrain_defence divides the ATTACKER's hits:
    /// hits x 1000 / (1000 + terrain_defence x this / 1000). At 1000 a mountain
    /// (terrain_defence 1000) halves them.
    int terrain_attack_scale = 1000;

    /// Side readiness (the strength reading) at or below which the phase reads
    /// WAVERING. A reading only — no unit breaks on it.
    int wavering_readiness = 300;

    /// Morale a battle's winners gain, and its losers (and every unit that
    /// retreats) lose, per-mille (battle_system.cpp).
    int morale_win  = 50;
    int morale_loss = 100;

    /// Out of contact, per tick: readiness regained (scaled by supply, never above
    /// morale), and the most morale moves toward the unit's supply.
    int readiness_recovery_permille = 100;
    int morale_recovery_permille    = 10;

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

    /// BL-467, Ben's ruling 3: SEVERAL ROUNDS PER TICK. Authored data rather than
    /// code (the corp_ai_params precedent), so tuning the window a commander gets
    /// is a data edit.
    ///
    /// At the shipped `max_rounds` 6 and this first-cut 3, a full battle spans two
    /// ticks and the player gets exactly one real between-tick decision — which is
    /// the whole point of a withdrawal window. Raise it and the fight resolves
    /// before anyone can react; lower it and a campaign tick becomes a tactical
    /// game, which Ben's "a short time" ruled out.
    int rounds_per_tick = 3;
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
    attacker_broken   = 1, ///< No attacking unit is left fighting (all spent or dead).
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
    int attacker_power = 0;           ///< Milli-hits that side LANDED this round (BL-1421).
    int defender_power = 0;
    battle_result round_winner = battle_result::defender_victory; ///< More landed; defender on a tie.
    int attacker_strength_permille = 0; ///< Side readiness AFTER this round (`side_readiness`).
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

/// One unit in a land fight (BL-1421). The resolver is world-free, so the
/// caller builds these from `unit_component`s, steps, and writes `men` and
/// `readiness` back. A vector of these is ALWAYS in ascending unit id: the
/// men-loss remainder is handed out in vector order.
struct land_combatant
{
    entity_id         unit      = null_entity;
    int               men       = 0;    ///< HP: men fit to fight. Mutated by the round.
    int               readiness = 1000; ///< Per-mille. Mutated by the round; 0 = out of the fight.
    int               morale    = 1000; ///< Per-mille. Scales the hits this unit deals.
    int               supply    = 1000; ///< Per-mille. Scales the hits this unit deals.
    land_combat_stats stats{};          ///< unit_land_stats(), resolved at build.
};

/// Still in the fight: men standing and readiness left.
inline bool land_combatant_fighting(const land_combatant& c)
{
    return c.men > 0 && c.readiness > 0;
}

/// A side's STRENGTH READING since BL-1421: its men-weighted mean readiness,
/// per-mille, over combatants with men standing (0 when none). What the card,
/// the phase, the dispatch and the withdrawal price read.
int side_readiness(const std::vector<land_combatant>& side);

/// A battle in progress. THE SPAN IS MODELLED EXPLICITLY: a caller steps this
/// round by round, so the withdrawal window is a real window rather than a
/// parameter pretending to be one. The UI drives it a round at a time; a
/// headless caller runs resolve_campaign_battle below.
///
/// Everything mutable to the fight lives here, including the rng state, so
/// stepping is a pure function of (state, params).
struct campaign_battle_state
{
    // CARRIED FOR THE STREAM SHAPE ONLY since BL-1421. The land round reads its
    // forces from the caller's combatants, not from these stacks, and reads no
    // doctrine or season; the fields stay so a saved battle keeps its v57 shape.
    // begin_campaign_battle leaves the stacks empty.
    std::vector<army_stack_entry> attacker;
    std::vector<army_stack_entry> defender;
    doctrine_row attacker_doctrine{};
    doctrine_row defender_doctrine{};

    // The three tile axes (BL-519). The defaults are the BL-519 spelling of the
    // old `grassland` default, not a new one: grass on sedimentary ground at the
    // density decompose_biome grades a grassland to.
    terrain_substrate   terrain_sub     = terrain_substrate::sedimentary;
    terrain_cover       terrain_cov     = terrain_cover::grass;
    std::uint8_t        terrain_density = 150u;
    terrain_landform    terrain_lf      = terrain_landform::plains;
    season              battle_season = season::summer;
    int attacker_supply = 1000; ///< Mean supply at open, for the record; the round reads each unit's.
    int defender_supply = 1000;

    int attacker_strength_permille = 1000; ///< `side_readiness`, refreshed after every round.
    int defender_strength_permille = 1000;

    uint64_t rng_state  = 0; ///< Advanced once per draw; never re-seeded mid-fight.
    uint64_t stream_seed = 0; ///< The folded seed, kept for the outcome record.

    int rounds_fought = 0;
    campaign_battle_end end = campaign_battle_end::in_progress;

    std::vector<campaign_battle_round> rounds;
};

// ---------------------------------------------------------------------------
// The world-held record (BL-467)
// ---------------------------------------------------------------------------

/// One battle in progress, as `world` holds it.
///
/// WHY THIS IS A RECORD AND NOT A CALL. Until BL-467 both resolvers were pure
/// functions with no production caller — the military layer could COMPUTE a fight
/// but could not HAVE one, because nothing owned a fight between ticks. This
/// struct is that ownership: it is what makes a battle a thing in the world with
/// a duration, rather than an answer somebody computed.
///
/// IDENTITY is (province, attacker, defender). A corp pair fights at most one
/// battle per province at a time — a third corp arriving opens its own battles
/// against each of them rather than joining or splitting this one.
struct active_battle
{
    uint32_t  province = 0;            ///< The envelope. Ruling 1: the province frames the fight.
    entity_id attacker = null_entity;  ///< The corp whose hostility opened it.
    entity_id defender = null_entity;

    /// Participating unit ids per side, ASCENDING. Ascending because the
    /// per-unit loss remainder is assigned by ascending entity id, so the order
    /// is load-bearing rather than cosmetic (see distribute_losses).
    std::vector<entity_id> attacker_units;
    std::vector<entity_id> defender_units;

    /// The ground the fight is ON: the defender-side tile whose terrain the
    /// resolver was handed at open (battle_system.cpp `defender_ground`, ruling
    /// 2 — highest terrain_defence, ties to the lowest tile id). STORED at open
    /// rather than re-derived, because the units it was picked from move and
    /// die while the terrain the battle reads does not (BL-1145 review): the
    /// battle card frames this tile. `null_entity` only if no defending unit
    /// resolved to a real tile — never for a battle discovery opens, whose
    /// units all stand in the province.
    /// Saved (world_save_version 30).
    entity_id ground_tile = null_entity;

    /// The resolver's own state: strengths, the stream, rounds fought, and the
    /// per-round trace. Held whole rather than copied field-by-field so the
    /// campaign resolver stays the single source of what a round does.
    campaign_battle_state state;

    /// A withdrawal asked for since the last round batch, honoured at the TICK
    /// BOUNDARY before the next one. Held here rather than applied on request
    /// because that delay IS the withdrawal window — applying it the instant it
    /// is asked for would make breaking off free of the round you are already in.
    /// Cleared once honoured.
    withdrawing_side withdraw_requested = withdrawing_side::none;
};

/// Ordering for `world::battles` — (province, attacker, defender), ascending.
/// The container is kept sorted by this so a walk over live battles is
/// order-independent, which matters because the corp and unit containers the
/// battles were discovered from are UNORDERED.
inline bool battle_order_less(const active_battle& a, const active_battle& b)
{
    if (a.province != b.province) return a.province < b.province;
    if (a.attacker != b.attacker) return a.attacker < b.attacker;
    return a.defender < b.defender;
}

/// Fold @p id into the battle's stream seed. Exposed so a caller can log it
/// and so a harness can assert that two different identities really do produce
/// different streams.
uint64_t campaign_battle_seed(const campaign_battle_identity& id);

/// Open a battle over two sides' combatants (each ascending by unit id).
/// Nothing is resolved yet — round 1 happens on the first step_campaign_battle
/// call, which is what gives a commander a window at all.
campaign_battle_state begin_campaign_battle(const campaign_battle_identity&    id,
                                            const std::vector<land_combatant>& attacker,
                                            const std::vector<land_combatant>& defender,
                                            terrain_substrate                  terrain_sub,
                                            terrain_cover                      terrain_cov,
                                            std::uint8_t                       terrain_density,
                                            terrain_landform                   terrain_lf);

/// Fight one round over @p attacker and @p defender, mutating their men and
/// readiness in place. Both sides' hits are computed from the state BEFORE the
/// round and applied together. A side with no unit fighting at the start of a
/// round deals nothing and absorbs nothing — its spent units are overrun, the
/// enemy's hits landing on them in full — and is broken. Returns false once the
/// battle is over (@p st.end set), at which point further calls are no-ops.
/// Consumes exactly two draws from the stream — attacker swing, then defender
/// swing — in that fixed order.
bool step_campaign_battle(campaign_battle_state&        st,
                          std::vector<land_combatant>&  attacker,
                          std::vector<land_combatant>&  defender,
                          const campaign_battle_params& params = {});

/// Break off. The withdrawing side pays the three-term parting cost IN MEN —
/// `quote_withdrawal`'s total, per-mille of its standing men, floor shares then
/// the remainder in vector order — and the fight ends; the side that stayed
/// holds the field. @p leaving is the withdrawing side's combatants. Consumes no
/// draws — disengaging is a decision, not another gamble. A no-op on an
/// already-finished battle or on withdrawing_side::none. @return men removed.
int withdraw_campaign_battle(campaign_battle_state&        st,
                             withdrawing_side              side,
                             std::vector<land_combatant>&  leaving,
                             const campaign_battle_params& params = {});

/// The parting cost, per-mille, that withdraw_campaign_battle charges @p side:
/// base + rounds fought x per-round + pursuit x how far behind, clamped.
int withdrawal_cost_permille(const campaign_battle_state& st, withdrawing_side side,
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
///
/// @p attacker and @p defender are taken by value and fought on copies; pass
/// out-params to read the men and readiness each side ended on.
campaign_battle_outcome resolve_campaign_battle(const campaign_battle_identity& id,
                                                std::vector<land_combatant>     attacker,
                                                std::vector<land_combatant>     defender,
                                                terrain_substrate               terrain_sub,
                                                terrain_cover                   terrain_cov,
                                                std::uint8_t                    terrain_density,
                                                terrain_landform                terrain_lf,
                                                withdrawing_side                withdrawer = withdrawing_side::none,
                                                int                             withdraw_after_round = 0,
                                                const campaign_battle_params&   params = {},
                                                std::vector<land_combatant>*    attacker_out = nullptr,
                                                std::vector<land_combatant>*    defender_out = nullptr);
