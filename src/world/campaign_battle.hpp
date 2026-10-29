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
// DETERMINISM. A battle carries NO RANDOMNESS (Ben, 2026-10-29): no draw, no
// swing, no per-battle roll. Every round is the deterministic arithmetic of the
// forces, the ground, the vision and the supply, so the same inputs fight the same
// battle every time. The seed folded from the battle's identity (participants +
// tile + tick + world seed) survives only to vary the dispatch wording. Nothing
// here depends on the iteration order of a container.
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
    /// THE DEFENDED TILE (Ben, 2026-10-27, overturning the 2026-08-19 province
    /// envelope): a battle is fought over the tile the defender stands on, which
    /// the attacker tried to step onto (MILITARY.md § Battles). It sits in the
    /// slot of the fold the province (and before it the tile index) held, same
    /// splitmix64 shape, role order preserved.
    entity_id tile          = null_entity;
    uint64_t  tick          = 0;           ///< Campaign tick the battle opened.
    uint32_t  world_seed    = 0;           ///< So two worlds with equal ids still differ.
};

/// Tuning for the campaign path, kept as data so changing the feel is a data
/// change rather than a code change (the corp_ai_params precedent). All
/// per-mille unless named otherwise.
struct campaign_battle_params
{
    // NO ROUND CAP (Ben, 2026-10-29: "Battles shouldn't have maximum durations.
    // They continue while one unit is trying to move and end when readiness runs
    // out. Some battles may easily take 90-180 days."). The `max_rounds` stalemate
    // that stood here (6) is gone: a fight ends only when a side is broken (no unit
    // of it left fighting) or a side withdraws, and the attacker cancelling its move
    // IS its withdrawal (`attacker_withdrew`, priced below).

    // NO SWING (Ben, 2026-10-29: "Let's not add battle randomness ... remove the
    // swing too"; MILITARY.md § Battles). The per-round power swing (`swing_permille`
    // 600, a uniform draw in [400, 1600] per side per round) is gone with its two
    // draws: every round's hits are the deterministic arithmetic, so the same forces
    // on the same ground with the same supply fight the same battle. This overturns
    // NR-204 ("a narrower swing made the edge too safe").

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

    /// A BATTLE IS WEEKS LONG, NOT DAYS (Ben, 2026-10-29; MILITARY.md § Battles).
    /// Two terms on the damage a side takes in a day, applied to the landed hits
    /// after the visible share, so men and readiness slow together and a battle
    /// kills the same share of its men it always did, over more days:
    ///
    ///   day_damage_permille — every day's landed damage x this. At 125 (1/8) a
    ///     parity fight of base infantry, which the round once spent in about seven
    ///     days, runs about forty (move_by_days D6; 167 measured 29.8, the low edge).
    ///   the ROUT TERM — when the side dealing the damage out-hits the side taking
    ///     it by more than `rout_ratio_permille` (raw hits, the
    ///     ground included), the damage is raised by the excess ratio CUBED:
    ///     x (ratio / rout_ratio)^3, capped at `rout_cap_permille`. Below the line
    ///     nothing changes, so the contested band (about 1.5:1, where breaking a
    ///     defender lies) is the longest fight; above it a rout stays fast — at
    ///     3:1 the beaten side takes 8x the base, about the old pace.
    /// Measured in campaign_battle_harness C4 and move_by_days_harness D6.
    int day_damage_permille = 125;
    int rout_ratio_permille = 1500;
    int rout_cap_permille   = 12000;

    /// LIKE FORCES ENTRENCH, AND SUPPLY DECIDES THE STALEMATE (Ben, 2026-10-29;
    /// MILITARY.md § Battles). When neither side out-hits the other by more than
    /// `entrench_ratio_permille` (raw hits, the ground included —
    /// the rout term's measure, mirrored), the battle is DUG IN for that day:
    ///   - the day's landed damage KILLS fewer men: its men share is scaled by
    ///     `entrench_casualty_permille` (dug-in troops lose cohesion, not lives); the
    ///     readiness share lands in full, so it is the stocks that decide;
    ///   - every fighting unit regains `land_combatant::entrench_recovery` readiness
    ///     after the day's damage, never above its morale - the caller sets it from
    ///     the unit's carried supplies (`unit_entrench_recovery_permille`), so a fed,
    ///     armed side holds and a starved one erodes and breaks.
    /// Leaving the band leaves entrenchment the same day. No randomness is added (Ben,
    /// 2026-10-29): a battle has no draw at all.
    /// THE CALIBRATION (BL-1453): 1250 is the rout line's mirror nearer parity. An
    /// open parity fight of base infantry spends the attacker's readiness at about
    /// 20 a day (day_damage 125: some fifty days with no recovery - the 30-60 the
    /// ruling asks of a parity battle its supplies do not cover); a fully supplied
    /// unit regains 40 a day, above the 28 or so a modern row (quality 1.44) loses at
    /// parity, so two fed sides hold (move_by_days D6: live at day 400); and a dug-in day
    /// kills a tenth of the men an open one does, so a year of holding does not
    /// bleed the fed side out.
    int entrench_ratio_permille    = 1250;
    int entrench_casualty_permille = 100;
    int entrench_recovery_permille = 40;

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

    /// Out of contact, PER DAY (Ben, 2026-10-28: readiness and morale recover daily,
    /// on the day step with the march and the battles): readiness regained (scaled by
    /// supply, never above morale), and the most morale moves toward the unit's
    /// supply. The values are the per-tick figures carried onto the day unchanged, so
    /// a spent unit at full supply is fit again in ten days and a lost battle's morale
    /// (100) is regained in ten - a first cut on the day grain, not a measured one.
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
    ///
    /// THE PER-ROUND TERM IS CAPPED (BL-1453 fix round, 2026-10-29). With no round
    /// cap a battle can run 90-180 days, and 25 a round uncapped would reach the
    /// side's whole force by day 40. It grows 25 a round to `withdraw_per_round_cap`
    /// 250 (day 10) and holds there, so the first ten days price as they always did
    /// and the dearest exit — 20 + 250 + a pursuit of at most 400 — is 670 per-mille
    /// of the leaving side's men: a late exit stays dear and never takes them all.
    ///
    /// RE-CUT FOR WEEKS-LONG BATTLES (BL-1453 retune, 2026-10-29): 6 a round to the
    /// cap of 250 (day 42, about a parity battle's length), so a break-off in the
    /// first week of a forty-day fight is cheap and one at its end is dear. The
    /// dearest exit is still 20 + 250 + 400 = 670 per-mille of the leaving side's men.
    int withdraw_base_permille          = 20;
    int withdraw_per_round_permille     = 6;
    int withdraw_per_round_cap_permille = 250;
    int withdraw_pursuit_scale          = 400;

    /// PACING IS BY THE DAY (Ben, 2026-10-28; MILITARY.md § Battles ruling 3,
    /// BL-1445 move by days): rounds fought per DAY. Battles step every day, from
    /// the daily military pass (`run_military_day`), and contact is checked every
    /// day, so the day a march meets a hostile stack is the day its battle opens.
    ///
    /// One round a day, with no cap on the rounds: a battle runs until a side is
    /// broken or withdraws (move_by_days_harness D6 measures the days by force
    /// ratio). The round's damage is unchanged - its scale was already a day's
    /// fighting once a round is a day. Withdrawal is honoured at the day boundary,
    /// so a commander has a decision every day of the fight. Was `rounds_per_tick
    /// = 3` (two economy ticks, 180 days, to a stalemate).
    int rounds_per_day = 1;
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
    /// PRODUCED BY NOTHING since 2026-10-29 (Ben: battles have no maximum duration;
    /// the `max_rounds` stalemate is gone). Kept so a saved battle or dispatch that
    /// ended on it still reads; never renumber.
    stalemate         = 5,
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
    /// BL-1453 (MILITARY.md § Recon): the share of THIS unit the enemy side sees,
    /// per-mille — every hit landed on it is scaled by it. Set by the caller before
    /// each round (`visible_share_permille`); 1000 (fully seen) for a caller that
    /// models no recon, which leaves the round exactly as it was.
    int               visible   = 1000;
    /// BL-1453 (entrenchment): readiness, per-mille, this unit regains each DAY of
    /// an entrenched fight, from its carried supplies. Set by the caller
    /// (`unit_entrench_recovery_permille`); 0 for a caller that models none.
    int               entrench_recovery = 0;
    /// BL-1453 (cold review): the sub-unit remainders carried day to day - readiness
    /// in (milli-hits x 1000) against men x readiness_cost, men in milli-hits against
    /// hp_cost. Built from and written back to the unit (`unit_component::
    /// battle_ready_carry` / `battle_hp_carry`, saved).
    int64_t           ready_carry = 0;
    int64_t           hp_carry    = 0;
};

/// BL-1453 (MILITARY.md § Recon, Ben 2026-10-29): THE VISIBLE SHARE of a defending
/// unit, per-mille — min(1, the attacker's recon stock / the unit's stealth), the stock
/// in milli-points. A target with no stealth is fully seen. The attacker's units are
/// always fully seen: the defender's hits are always full.
int visible_share_permille(int64_t recon_milli, int target_stealth);

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

    /// UNUSED since 2026-10-29 (battles carry no randomness, so no step draws).
    /// Still set to `stream_seed` at open, hashed and saved, so an older save loads
    /// and the save layout (v65) is unchanged. Nothing advances or reads it.
    uint64_t rng_state  = 0;
    /// The folded seed. No longer drives any draw; still read by the dispatch text,
    /// which folds it with the round index to vary wording (battle_dispatch_text.cpp).
    uint64_t stream_seed = 0;

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
/// IDENTITY is (tile, attacker, defender), the tile being the DEFENDED one
/// (Ben, 2026-10-27). A nation pair fights at most one battle per tile at a time
/// — a third nation attacking the same tile opens its own battle rather than
/// joining or splitting this one.
struct active_battle
{
    entity_id tile     = null_entity;  ///< The defended tile: identity and ground (MILITARY.md § Battles).
    /// The province `tile` lies in, DERIVED at open (and read from the stream on
    /// load) — kept for the province holder, the dispatch text and the card,
    /// which still speak of provinces. Never part of the identity.
    uint32_t  province = 0;
    entity_id attacker = null_entity;  ///< The nation whose attack opened it.
    entity_id defender = null_entity;

    /// Participating unit ids per side, ASCENDING. Ascending because the
    /// per-unit loss remainder is assigned by ascending entity id, so the order
    /// is load-bearing rather than cosmetic (see distribute_losses).
    std::vector<entity_id> attacker_units;
    std::vector<entity_id> defender_units;

    /// The ground the fight is ON: the defended tile, always equal to `tile`
    /// (ruling 2 since 2026-10-27). Kept as its own field because the battle card
    /// frames it and the stream has carried it since world_save_version 30; the
    /// identity tile is SAVED in this slot (v63), so a pre-v63 battle's key is
    /// its ground tile.
    entity_id ground_tile = null_entity;

    /// THE ATTACKING STACK, ascending: the units whose march made the attack. A
    /// subset of `attacker_units` — the rest joined from adjacent tiles. When the
    /// attacker holds the field these advance onto `tile`; joined stacks stay
    /// (Ben, 2026-10-27). Saved (world_save_version 63); empty for a pre-v63
    /// battle, which therefore advances no one.
    std::vector<entity_id> attacking_units;

    /// The resolver's own state: strengths, the stream, rounds fought, and the
    /// per-round trace. Held whole rather than copied field-by-field so the
    /// campaign resolver stays the single source of what a round does.
    campaign_battle_state state;

    /// BL-1453 (MILITARY.md § Recon): THE ATTACKER'S RECON STOCK, milli-points — one per
    /// battle, every defending unit's stealth compared against it. Opened at the recon
    /// of the attacker's units that faced the tile a whole day, grown after every day's
    /// round by the recon of its units in the battle, its adjacent units and its air,
    /// until it covers the stealthiest defender. Saved (world_save_version 67).
    int32_t attacker_recon_milli = 0;

    /// A withdrawal asked for since the last round batch, honoured at the TICK
    /// BOUNDARY before the next one. Held here rather than applied on request
    /// because that delay IS the withdrawal window — applying it the instant it
    /// is asked for would make breaking off free of the round you are already in.
    /// Cleared once honoured.
    withdrawing_side withdraw_requested = withdrawing_side::none;
};

/// Ordering for `world::battles` — (tile, attacker, defender), ascending.
/// The container is kept sorted by this so a walk over live battles is
/// order-independent, which matters because the corp and unit containers the
/// battles were discovered from are UNORDERED.
inline bool battle_order_less(const active_battle& a, const active_battle& b)
{
    if (a.tile != b.tile) return a.tile < b.tile;
    if (a.attacker != b.attacker) return a.attacker < b.attacker;
    return a.defender < b.defender;
}

/// BL-1453: is a fight in which one side deals @p a raw hits and the other @p b
/// ENTRENCHED — neither out-hitting the other by more than `entrench_ratio_permille`?
/// Two sides that deal nothing are not (a fight with no hits has nothing to dig in
/// against; it runs until the attacker withdraws).
bool battle_entrenched(int64_t a, int64_t b, const campaign_battle_params& params = {});

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
/// Consumes NO draw (Ben, 2026-10-29: battles carry no randomness): the round is
/// the deterministic arithmetic of its inputs.
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
///
/// @p round_budget (BL-1453): a battle has no maximum duration, and an entrenched
/// fully supplied pair holds by design, so this headless runner stops after that
/// many rounds and returns the fight as it stands (`end` in_progress). A harness
/// budget, never a game rule.
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
                                                std::vector<land_combatant>*    defender_out = nullptr,
                                                int                             round_budget = 3650);
