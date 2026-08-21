#pragma once

#include "components.hpp"
#include "condition_set.hpp"
#include "modifier_set.hpp"

#include <string>
#include <vector>

struct world; // forward-declared; tech_gate.cpp reads it.

// ---------------------------------------------------------------------------
// Tech gates (BL-344) — the one place a tech actually gates something
// ---------------------------------------------------------------------------
// More of the tech system was built than v0.1.4's design-forward status
// suggested: `tech_tree.{hpp,cpp}` holds a tree and BL-310 shipped a radial
// constellation viewer on F9. What was missing was that `tech_tree.hpp:49`
// stored the gate as
//
//     std::string condition; ///< research | structure | stockpile | market | ...
//
// — a LABEL. It described what the gate would be about; it could not resolve.
// So the tree was a picture of a system rather than the system, and no tech had
// ever been earned. This file is the loop closed: a real predicate (BL-342's
// `condition_set`), per-corp earned state, and ONE content gate that changes
// what the player may do.
//
// WHY THIS LIVES HERE AND NOT IN tech_tree.cpp
// `tech_tree.cpp` loads the tree from `scripts/tech_tree.lua` and is therefore
// EXCLUDED from the SDL/Lua-free world superset (CMakeLists: IO_WORLD_SOURCES
// filters it out). A gate that gates construction has to be linkable by
// `construction.cpp` and by a headless harness, so the gate table lives in this
// Lua-free TU and the Lua tree READS it — `tech_tree.cpp` looks each authored
// node up by id and copies the predicate in. ONE authority for what a gate
// requires; the Lua file authors identity and topology, never the predicate.
//
// WHY THE MILITARY BASE AND NOT A SMELTER
// BL-094's design test, quoted in BL-344: "does this system reach military as
// well as economic outcomes? If a technology can only unlock a building, it is
// being designed for the corporate player we are pivoting away from." Gating
// `building_type::military_base` (BL-325) costs exactly what gating a smelter
// would have cost, and it is the instance that proves the pillar the
// governing-body pivot cares about. BL-332 (military points and research) is the
// natural follow-on and should be sequenced against this.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// The effect union (BL-479) — what earning a tech DOES
// ---------------------------------------------------------------------------
// Until this item the whole effect vocabulary was one field, `unlocks_structure`
// — a 9:1 condition-to-effect asymmetry against BL-342's predicate side. The
// union widens it to exactly TWO arms: unlock a structure (BL-344's existing
// behaviour, unchanged) or move a scalar (a `scalar_modifier`, modifier_set.hpp).
// Closed on purpose: no callback, no Lua hook — an effect is data, so it stays
// deterministic, serialisable, and legible in the F9 viewer's prose.

/// Which arm of the union a `tech_effect` is.
enum class tech_effect_kind : uint8_t
{
    unlock_structure = 0, ///< Earning permits constructing `structure` (BL-344).
    modify_scalar,        ///< Earning applies `modifier` to the earning corp (BL-479).
};

/// One effect of earning a tech. A tagged union over POD arms rather than a
/// std::variant: the inactive arm is inert defaults, the layout is flat and
/// future-serialisable, and a reader can see the whole vocabulary in one screen.
struct tech_effect
{
    tech_effect_kind kind = tech_effect_kind::unlock_structure;

    /// `unlock_structure` arm: the building type earning permits.
    building_type structure = building_type::none;

    /// `modify_scalar` arm: the scalar moved for the EARNING CORP — research is
    /// not a world fact, so an effect never reaches a corp that has not earned
    /// the tech (applied by `advance_tech_gates`, read by `world::modified_scalar`).
    scalar_modifier modifier{};

    /// The two arms' authoring spellings — a misauthored effect (an unlock arm
    /// carrying a modifier, or vice versa) cannot be written through these.
    static tech_effect unlock(building_type t)
    {
        tech_effect e;
        e.kind      = tech_effect_kind::unlock_structure;
        e.structure = t;
        return e;
    }
    static tech_effect modify(modifier_subject subject, modifier_op op, float magnitude)
    {
        tech_effect e;
        e.kind               = tech_effect_kind::modify_scalar;
        e.modifier.subject   = subject;
        e.modifier.op        = op;
        e.modifier.magnitude = magnitude;
        return e;
    }
};

/// One earnable tech: a predicate, and the effects earning it applies.
struct tech_gate
{
    std::string   id;                ///< Tech id, matching scripts/tech_tree.lua.
    condition_set condition;          ///< What earns it. Never empty — see below.

    /// The unlock arm's target, or `building_type::none` for a tech that gates
    /// no structure. A MIRROR of the (at most one) unlock_structure entry in
    /// `effects`, maintained by `add_effect` — kept as a plain field so every
    /// pre-union reader (`structure_unlocked`, `gating_tech_for`, the BL-344
    /// harness) keeps its exact behaviour. Never author it directly; author
    /// through `add_effect` so the two cannot disagree.
    building_type unlocks_structure = building_type::none;

    /// The effect list — the union is the vocabulary, this is the store. At
    /// most ONE unlock_structure entry per tech (the pre-union model could not
    /// express more, and the mirror field cannot hold more); any number of
    /// modify_scalar entries, applied in this stored order at the earn moment.
    std::vector<tech_effect> effects;

    /// The single authoring point: appends to `effects` and keeps the
    /// `unlocks_structure` mirror true.
    void add_effect(const tech_effect& e)
    {
        effects.push_back(e);
        if (e.kind == tech_effect_kind::unlock_structure)
            unlocks_structure = e.structure;
    }
};

/// The prototype's authored gate table. Hand-written and small on purpose: a
/// tech that is not in this table is NOT earnable, which is a different state
/// from "unconditional". An empty `condition_set` is TRUE by definition
/// (BL-342 property 2), so an un-authored tech defaulting to an empty set would
/// silently earn itself on the first tick — absence has to be modelled by
/// absence from this table, never by an empty predicate.
///
/// @return The gate table, in a fixed authored order.
const std::vector<tech_gate>& prototype_tech_gates();

/// The gate for `tech_id`, or nullptr if that tech is not earnable.
const tech_gate* find_tech_gate(const std::string& tech_id);

/// Evaluate every gate for every corporation and record what is now earned.
/// Idempotent and monotonic: a tech, once earned, is never un-earned — a
/// research result is not a state you can fall out of by spending your money.
/// Runs in corporation-id order over a fixed gate order, so it is deterministic.
///
/// BL-479: the earn moment is also when a tech's `modify_scalar` effects land —
/// appended to `w.corp_modifiers[corp]` in the gate's stored effect order, once,
/// for the earning corp only. Monotonic exactly as the earned set is: applied on
/// the earn tick, never re-applied, never revoked.
///
/// @param w World; `earned_techs` (and `corp_modifiers`) are appended to.
/// @return  The number of (corp, tech) pairs newly earned this call.
int advance_tech_gates(world& w);

/// The same pass over an EXPLICIT gate table. This is the whole implementation
/// (the one-argument form runs it over `prototype_tech_gates()`); exposed so a
/// harness can drive a fixture tech through the identical earn/apply path the
/// live tick uses — the shipped table carries no modify_scalar tech yet, so
/// without this seam the modifier half could only be tested by authoring live
/// content, which is a design decision and not a test's to take.
///
/// @param w     World; `earned_techs` (and `corp_modifiers`) are appended to.
/// @param gates The gate table to evaluate, in its stored (authored) order.
/// @return      The number of (corp, tech) pairs newly earned this call.
int advance_tech_gates(world& w, const std::vector<tech_gate>& gates);

/// Whether `corp` may build `type` — i.e. either the type is ungated, or the
/// corp has earned the tech that gates it. The single read point, so the
/// placement check and the construction front door cannot disagree.
///
/// @param w    Read-only world state.
/// @param corp Corporation asking. `null_entity` disables the check entirely
///             (the behaviour every pre-BL-344 call site still gets).
/// @param type Building type to test.
/// @return     Whether that corp may construct that type.
bool structure_unlocked(const world& w, entity_id corp, building_type type);

/// The tech id gating `type`, or an empty string if the type is ungated. Lets a
/// refusal say WHICH tech is missing rather than just "locked" (BL-071's
/// teach-the-player-why rule, applied to the tech gate).
std::string gating_tech_for(building_type type);
