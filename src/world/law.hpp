#pragma once

#include "components.hpp"
#include "condition_set.hpp"

#include <array>
#include <cstdint>
#include <string>

struct world; // forward-declared; law.cpp reads it.

// ---------------------------------------------------------------------------
// Laws (BL-343) — the thinnest end-to-end slice of BL-155's law object
// ---------------------------------------------------------------------------
// BL-155 settled the law object (a `condition_set` plus a polymorphic effect),
// a ten-law list, and a four-family effect taxonomy — and is scoped design-only,
// so v0.1.3 had a complete design and nothing a player could touch. This builds
// ONE law, enacted, read at the enforcement seam, changing a number the player
// can already read: the extraction levy (BL-155's law #1, family (a) margin
// modifiers), landing as its own line in the budget ledger.
//
// THE ENFORCEMENT SEAM — the one real design decision, settled here:
//
//   A LAW IS A MODIFIER OVER THE MARKET, NEVER AN OVERRIDE OF IT.
//
// That is the principle already established when price clamps were vetoed
// (2026-07-11), precisely because a clamp fights price resolution rather than
// shifting a flow's cost. So the levy applies where the flow is ACCOUNTED, not
// where the price is RESOLVED: extraction output is priced by the market
// exactly as before, and the levy is a separate accounted cost in
// `apply_budget`. Two consequences worth stating — the market stays the only
// thing that sets prices, and the player sees the tax as its own number rather
// than as an unexplained worse price.
//
// Evaluation happens ONCE PER LAW PER CORP PER TICK (`evaluate_laws`), before
// the money loop reads it, so ordering is fixed and the determinism invariant
// holds.
//
// BL-094's design test, honestly applied: an extraction levy reaches economic
// outcomes and not military ones, and pretending otherwise would be dishonest.
// What this owes instead is that NOTHING in the law record or the effect
// dispatch assumes an economic subject — `law_effect_kind` is an open taxonomy
// a military effect joins without reshaping anything, and the predicate is
// BL-342's, which already carries military subjects. Build the seam so the
// answer can become yes; do not claim it already is.
// ---------------------------------------------------------------------------

/// Which family of effect a law applies. One family ships (BL-155 family (a),
/// margin modifiers); the enum is the extension point for the other three
/// (production/permission/relationship) and for the military effects BL-315's
/// conflict spine will want. Adding one is a new enumerator plus a new branch in
/// `apply_law_effect` — no change to the record, the predicate, or the seam.
enum class law_effect_kind : uint8_t
{
    extraction_levy = 0, ///< Family (a): a per-unit levy on raw extraction output.
};

/// One law. `conditions` is empty for the common case — BL-155: "most laws are
/// unconditional once enacted". `enacted` is the player/governing-body switch;
/// an un-enacted law is inert and changes nothing.
struct law
{
    std::string     id;      ///< Stable identifier, e.g. "LAW-EXTRACTION-LEVY".
    std::string     name;    ///< Display name for the budget ledger and the laws surface.
    condition_set   conditions;                                ///< Empty = always-on once enacted.
    law_effect_kind effect  = law_effect_kind::extraction_levy;
    bool            enacted = false;

    /// Effect magnitude, read per family. For `extraction_levy`: credits charged
    /// per unit of raw output extracted this tick.
    float rate = 0.0f;

    /// Resource the effect is restricted to, or `all_resources` for every one.
    /// A levy on iron ore and a levy on everything are the same record.
    static constexpr int all_resources = -1;
    int scope_resource = all_resources;
};

/// The per-corp effects in force this tick, resolved once from the enacted set.
/// A resolved bundle rather than a live query, so the money loop reads a fixed
/// array instead of re-evaluating predicates per building (the determinism
/// argument and the performance one point the same way).
struct law_effects
{
    /// Credits per unit of extracted output, by resource. Zero where no enacted
    /// law reaches that resource; levies stack additively when several do.
    std::array<float, resource_count> extraction_levy = {};

    /// Whether any enacted law reached this corp at all — lets a surface say
    /// "no laws in force" rather than showing a row of zeroes.
    bool any = false;
};

/// Resolve every enacted law against `subject_corp` exactly once, in the world's
/// authored law order. Pure and deterministic: it reads `const world&` and calls
/// BL-342's `evaluate`, which reads nothing else.
///
/// @param w            Read-only world state (the law list and everything the
///                     predicates measure).
/// @param subject_corp Corporation the laws are being resolved for.
/// @return             The effects in force on that corp this tick.
law_effects evaluate_laws(const world& w, entity_id subject_corp);

/// The one-law seed for the prototype (BL-343): an extraction levy on all raw
/// output, UN-ENACTED. Appended to `world::laws` at world setup so the surface
/// has something to switch on, and so an un-enacted law is the shipped default
/// (a levy that charged from turn one would be a balance change smuggled in as
/// a feature).
///
/// @param w    World; `laws` is appended to.
/// @param rate Credits per unit of raw output when enacted.
void seed_prototype_laws(world& w, float rate = 1.0f);
