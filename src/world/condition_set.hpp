#pragma once

#include "components.hpp" // entity_id, resource_type, building_type

#include <cstdint>
#include <string>
#include <vector>

struct world; // forward-declared: evaluation reads the world, the type does not own it.

// ---------------------------------------------------------------------------
// condition_set (BL-342) — the shared predicate laws, techs and quests all read
// ---------------------------------------------------------------------------
// BL-155 (laws) and BL-156 (techs) independently settled on the SAME object and
// neither built it:
//
//   > `condition_set` = a flat AND-list of atomic conditions — no nested OR-mesh
//   > (mirrors BL-087 resolution 1: binary tree, no re-converging mesh). Keeps
//   > evaluation order deterministic and the predicate legible.
//
// This header is that object, and nothing more. It owns no law, no tech and no
// UI; BL-343 and BL-344 are the consumers. Three properties are load-bearing:
//
//   1. PURE AND DETERMINISTIC. `evaluate` reads a `const world&` and nothing
//      else — no RNG, no clock, no cached mutable state. An enacted law is read
//      every economy tick, so this sits directly on the determinism invariant
//      (.claude/rules/io-standing-rules.md § Determinism). Where a measure sums
//      over an unordered container, it sums in ascending entity-id order.
//   2. AN EMPTY SET IS TRUE. BL-155: "most laws are unconditional once enacted
//      (empty condition_set = always-on)". The degenerate case is the COMMON
//      case, so it is the cheap one rather than a bolted-on special case.
//   3. A SUBJECT MAY BE MILITARY. BL-094's design test applied at the
//      foundation: a subject enum that enumerates only economic quantities is
//      exactly the failure the governing-body pivot is trying to avoid. Two
//      military subjects ship here — not because the prototype needs them, but
//      because a shape is only proven by an instance.
// ---------------------------------------------------------------------------

/// What an atomic condition measures. The first six are `tech_tree.hpp`'s
/// original six `condition` labels promoted from descriptive strings into real,
/// resolvable subjects — kept because they were chosen against real authored
/// content, and re-deriving them abstractly would be inventing where evidence
/// already exists. The last two are the military instances property 3 demands.
///
/// Each subject names the `condition` field that QUALIFIES it (which resource,
/// which building type, which tech); the others are ignored for that subject.
enum class condition_subject : uint8_t
{
    // --- the six promoted labels ------------------------------------------
    research,  ///< Qualified by `key` (a tech id). 1 if the subject corp has earned it, else 0.
    structure, ///< Qualified by `structure`. Count of that building type the subject corp owns.
    stockpile, ///< Qualified by `resource`. Units of it across all of the subject corp's pools.
    market,    ///< Qualified by `resource`. Mean resolved price across every market in the world.
    surplus,   ///< The subject corp's cash balance.
    era,       ///< Campaign era of the subject corp — see `measure_condition` for the honest caveat.

    // --- the military instances (BL-094's test, applied at the foundation) --
    military_units,    ///< Total unit count the subject corp fields.
    military_strength, ///< Summed combat strength of the units the subject corp fields.

    // --- research (BL-455, 2026-08-17) --------------------------------------
    // corporation_component::science accumulates per tick from every completed
    // research_institute (BL-332) and, until this value existed, was read by
    // NOTHING — no gate, no surface, no scorer, not the blackboard. Three sites
    // in all of src/: the declaration, the Lua param load, and the write.
    //
    // Appended last: condition_subject is a uint8_t inside a SERIALISED
    // condition_set, so a value may be appended but never inserted.
    //
    // REACHED, NOT SPENT — and this matches BL-344 rather than inventing a
    // second model. A tech_gate is a PREDICATE over corp state: it asks whether
    // a condition holds, and holds again next tick. Nothing in the gate system
    // debits anything. So science is a threshold a corp passes ("at_least N"),
    // not a currency it pays down. Making it spendable would need an entirely
    // different mechanism and would be a design change, not a wiring job.
    science,           ///< The subject corp's accumulated research points.
};

/// How a measured value is compared against `condition::operand`.
enum class condition_comparator : uint8_t
{
    at_least = 0,  ///< measure >= operand (the common case: "once you have N of these").
    greater_than,  ///< measure >  operand
    exactly,       ///< measure == operand (exact on integral subjects — see below).
    at_most,       ///< measure <= operand
    less_than,     ///< measure <  operand
};

/// True for subjects whose measure is a COUNT rather than a quantity. Comparison
/// on these rounds to an integer on both sides before comparing, so `exactly 3`
/// means exactly three and not "within a float epsilon of three" — the boundary
/// the harness asserts. Continuous subjects (stockpile, market, surplus,
/// military strength) compare as floats, because a stockpile of 3.5 units is a
/// real state and rounding it would be a lie.
bool condition_subject_is_integral(condition_subject s);

/// One atomic condition: `<subject> <comparator> <operand>`, plus the qualifier
/// the subject reads. Trivially copyable apart from `key`, which only the
/// `research` subject uses.
struct condition
{
    condition_subject    subject   = condition_subject::surplus;
    condition_comparator comparator = condition_comparator::at_least;
    float                operand    = 0.0f;

    /// Qualifier — read only by the subject that names it (see the enum above).
    resource_type resource  = resource_type::iron_ore; ///< `stockpile`, `market`.
    building_type structure = building_type::none;     ///< `structure`.
    std::string   key;                                 ///< `research` (a tech id).
};

/// A flat AND-list. Every condition must hold; an EMPTY set holds (property 2).
/// Deliberately not a tree: no OR, no nesting, no re-converging mesh.
struct condition_set
{
    std::vector<condition> all;

    /// Whether this predicate is unconditional (the common case for a law).
    bool always() const { return all.empty(); }
};

/// Measure one subject for `subject_corp`, without comparing it. Exposed so the
/// harness and any explanatory UI ("you have 2 of the 3 required") can read the
/// same number `evaluate` compares, rather than recomputing it.
///
/// @param c            The condition whose subject + qualifier to measure.
/// @param w            Read-only world state.
/// @param subject_corp Corporation the predicate is being asked about.
/// @return             The measured value; 0 when the subject cannot be resolved.
float measure_condition(const condition& c, const world& w, entity_id subject_corp);

/// Resolve one atomic condition to true/false.
bool evaluate_condition(const condition& c, const world& w, entity_id subject_corp);

/// Resolve a whole AND-list. Pure, deterministic, and true for an empty set.
///
/// The signature carries a SUBJECT CORP, which the BL-342 design sketch
/// (`evaluate(condition_set, const world&)`) did not: every consumer named by
/// BL-343 and BL-344 is per-corporation (a levy is charged to a corp; an earned
/// tech is per-corp state, not global), so a world-only predicate could not
/// answer either question. Recorded in NEEDS_REVIEW.json as a decision taken.
///
/// @param cs           The AND-list.
/// @param w            Read-only world state.
/// @param subject_corp Corporation the predicate is being asked about.
/// @return             True iff every condition holds (vacuously true if empty).
bool evaluate(const condition_set& cs, const world& w, entity_id subject_corp);

/// One-line human rendering of a condition, for the tech viewer and the laws
/// ledger ("Stockpile of Iron Ore >= 100"). Lives here so the two surfaces
/// cannot word the same predicate differently.
///
/// The pretty names for a resource / building type live in the UI layer
/// (`ui/presentation.hpp`), which the SDL/Lua-free world superset must not
/// depend on — so they arrive as optional resolvers. A caller in the world
/// layer (or a harness) passes neither and gets the qualifier's enum index;
/// every UI caller passes `resource_name` / `building_type_name` and gets the
/// same wording as every other UI caller.
std::string condition_text(const condition& c,
                           const char* (*resource_label)(resource_type)  = nullptr,
                           const char* (*structure_label)(building_type) = nullptr);
