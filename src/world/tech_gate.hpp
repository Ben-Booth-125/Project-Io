#pragma once

#include "components.hpp"
#include "condition_set.hpp"

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

/// One earnable tech: a predicate, and the content earning it unlocks.
/// `unlocks_structure` is `building_type::none` for a tech that gates nothing
/// yet — earnable, recorded, but with no content behind it.
struct tech_gate
{
    std::string   id;                ///< Tech id, matching scripts/tech_tree.lua.
    condition_set condition;          ///< What earns it. Never empty — see below.
    building_type unlocks_structure = building_type::none;
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
/// @param w World; `earned_techs` is appended to.
/// @return  The number of (corp, tech) pairs newly earned this call.
int advance_tech_gates(world& w);

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
