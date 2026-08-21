#pragma once

#include "components.hpp"

#include <cstdint>
#include <vector>

// ---------------------------------------------------------------------------
// Combat engine (BL-272)
//
// SCOPE. "Real tactics" means doctrine PARAMETERS, never battlefields. A
// battle is not a tactical simulation with maneuver, positioning or ticks of
// its own — it is ONE deterministic evaluation of matchup x doctrine x
// terrain x supply x season. This is the load-bearing constraint: BL-271's
// future history-sim sandbox intends to run millions of engagements across
// seeds, so anything that scales per-battle cost (pathing, unit-by-unit
// simulation, a sub-tick loop) is dead on arrival there. If a change to this
// file adds a loop whose cost scales with battlefield SIZE rather than with
// the number of distinct unit types on each side, it has drifted out of scope.
//
// TACTICS ARE DATA. A doctrine_row is a set of modifiers against the class
// matchup matrix and the terrain scalars, nothing more — see doctrine_row
// below. Unit ROSTERS (the concrete set of unit types and their stats per
// era) are BL-274's job, not this file's: resolve_battle takes fully-resolved
// army_stack_entry values, never a roster lookup, so it has no idea whether
// it is refereeing 1200 BCE or 1960 CE. Do not add anything here that only
// makes sense for one era.
//
// INTEGER ARITHMETIC ONLY IN THE OUTCOME PATH. Battle outcomes are gate-path
// decisions (they move borders in the future sandbox), so no floats decide
// who wins. All modifiers are integers in "per-mille" (1000 = neutral/100%),
// and every tie is broken by an explicit, documented rule rather than `<`
// ambiguity — see resolve_battle's comment for the exact tie-break.
//
// NAVAL IS STRATEGIC-ONLY (this cut). unit_class::naval exists so callers can
// tag naval presence, but resolve_battle excludes naval entries from the
// tactical calculation entirely (zero power, zero weight in the matchup
// average) rather than resolving naval combat. Tactical naval resolution is
// explicitly deferred.
//
// SIEGES ARE A DOCTRINE CHOICE. There is no separate siege resolution path in
// this first cut — an army besieging a position picks a doctrine_row with an
// `invest` or `assault` stance (siege_stance below) rather than the engine
// branching into different code.
// ---------------------------------------------------------------------------

/// The five unit classes the class matchup matrix is keyed on. Deliberately
/// coarse (class x class, not type x type) so the matrix stays readable and
/// extensible; per-type flavour is layered on top via army_stack_entry's own
/// modifiers rather than growing the matrix.
enum class unit_class : uint8_t
{
    infantry = 0,
    cavalry  = 1,
    ranged   = 2,
    siege    = 3,
    naval    = 4, ///< Strategic-only presence — see file header. Contributes
                  ///< zero tactical power in resolve_battle.
};

inline constexpr int unit_class_count = 5;

/// The season a battle is fought in. Only affects the attrition contribution
/// (via season_attrition_multiplier in combat.cpp) — defence and matchup are
/// season-blind.
enum class season : uint8_t
{
    spring = 0,
    summer = 1,
    autumn = 2,
    winter = 3,
};

/// A siege doctrine's stance, folded into doctrine_row rather than given a
/// separate resolution path (see file header § sieges).
enum class siege_stance : uint8_t
{
    field   = 0, ///< Ordinary open-field engagement — most battles.
    assault = 1, ///< Storm a held position outright: higher losses, decisive.
    invest  = 2, ///< Sit and starve it out: lower losses, grinds on attrition.
};

/// One typed unit-type's contribution to an army stack, already resolved to
/// its combat-relevant numbers. Deliberately NOT a lookup key into a roster
/// table — see file header. `type_id` is carried through for the caller's
/// own bookkeeping (which world unit_component(s) this stack summarises) but
/// resolve_battle never interprets it.
struct army_stack_entry
{
    uint16_t   type_id = 0;   ///< Opaque caller identifier; unread by combat.cpp.
    unit_class cls      = unit_class::infantry;
    int        count    = 0;  ///< Units in this stack entry. Must be >= 0.

    /// Per-type modifier layered on the class matchup, per-mille, added to the
    /// class's base power before matchup is applied. E.g. a strong single
    /// type inside the infantry class carries a positive value here without
    /// needing its own row in the class matrix.
    int type_power_mod = 0;
};

/// A doctrine's modifiers against the matchup matrix and the terrain scalars.
/// Pure data — "phalanx: devastating frontal, brittle flank, useless in
/// mountain" is exactly this struct with (frontal_bonus > 0, flank_fragility
/// > 0, mountain_penalty > 0). Adding a new doctrine is adding a new value of
/// this struct; it never requires touching resolve_battle (R3).
struct doctrine_row
{
    /// Bonus to this side's power in a head-on engagement, per-mille. Can be
    /// negative (a doctrine that is bad at frontal assaults).
    int frontal_bonus = 0;

    /// How much this side's power is reduced by its own doctrine's exposure
    /// to being flanked, per-mille, 0..1000. Modelled as an intrinsic
    /// weakness of the formation rather than conditioned on the opponent
    /// actually flanking — a first-cut simplification (see combat.cpp).
    int flank_fragility = 0;

    /// Reduction to this side's power when the battle's terrain_landform is
    /// `mountain`, per-mille, 0..1000. Not a general terrain scalar (those
    /// come from terrain_combat.hpp) — this is doctrine-specific ("useless in
    /// mountain"), so it only fires on that one landform.
    int mountain_penalty = 0;

    /// The stance for a siege-flavoured doctrine; `field` for everything else.
    siege_stance stance = siege_stance::field;
};

/// Which side came out ahead. Ties are broken in the defender's favour (see
/// resolve_battle) so this enum never needs a third "draw" value that callers
/// would have to handle separately.
enum class battle_result : uint8_t
{
    attacker_victory = 0,
    defender_victory = 1,
};

/// The result of one resolve_battle call. All losses are fractions
/// (per-mille, 0..1000) of the SIDE'S OWN committed unit counts, not absolute
/// numbers — resolve_battle never mutates an army_stack_entry or a world
/// unit_component, so applying the fraction to real unit counts is the
/// caller's job (both the future BL-271 sim loop and the 1960+ era code).
struct battle_outcome
{
    battle_result result;

    int attacker_power = 0; ///< Final computed power after every modifier, for logging.
    int defender_power = 0;

    int attacker_losses_permille = 0; ///< 0..1000, fraction of attacker's committed units lost.
    int defender_losses_permille = 0; ///< 0..1000, fraction of defender's committed units lost.

    /// How lopsided the margin was, 0..1000 (0 = a near-exact tie, 1000 = the
    /// loser was reduced to nothing). Callers use this to scale territorial
    /// and morale consequences rather than resolve_battle picking a
    /// consequence magnitude itself — that stays a sim-loop concern.
    int decisiveness = 0;
};

/// Resolve one battle as a pure, deterministic function of its inputs:
/// matchup matrix x formation doctrine x terrain scalars x supply state x
/// season. Same inputs always produce the same battle_outcome — no RNG, no
/// hidden state, nothing read from the world beyond what is passed in.
///
/// `attacker_supply` / `defender_supply` are 0..1000 (0 = cut off, 1000 =
/// fully supplied); values outside that range are clamped. `terrain_comp` /
/// `terrain_lf` feed terrain_defence/terrain_attrition (terrain_combat.hpp)
/// exactly as the 1960+ era already reads them — this file adds no terrain
/// model of its own.
///
/// TIE-BREAK: if attacker_power == defender_power after every modifier, the
/// defender wins (holding ground is the default outcome of an inconclusive
/// engagement). This is the one place a strict `<`/`>` comparison would be
/// ambiguous, so it is called out explicitly rather than left to comparison
/// order.
///
/// naval entries in either stack contribute zero power and zero weight to
/// the matchup average (see file header § naval). Passing an all-naval stack
/// on one side is legal and resolves to that side losing with near-zero
/// power rather than an error — resolve_battle never rejects an input.
battle_outcome resolve_battle(const std::vector<army_stack_entry>& attacker,
                               const doctrine_row&                 attacker_doctrine,
                               const std::vector<army_stack_entry>& defender,
                               const doctrine_row&                 defender_doctrine,
                               terrain_substrate                    terrain_sub,
                               terrain_cover                        terrain_cov,
                               std::uint8_t                         terrain_density,
                               terrain_landform                     terrain_lf,
                               season                               battle_season,
                               int                                  attacker_supply,
                               int                                  defender_supply);
