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

    /// Family (a) again, but with a SECOND PARTY: an ad-valorem duty on a sale
    /// made in the enacting nation's own market to a buyer domiciled elsewhere.
    /// It is the first law effect whose proceeds go somewhere — the extraction
    /// levy debits a corp and credits nobody, because in 1960 there was no
    /// nation treasury to credit. There is now (nation_component::treasury), and
    /// the tariff is a TRANSFER: the buyer is debited exactly what the enacting
    /// nation is credited, never a credit more or less. `law::rate` is the
    /// FRACTION of the trade's value, not a per-unit charge.
    import_tariff = 1,
};

/// One law. `conditions` is empty for the common case — BL-155: "most laws are
/// unconditional once enacted". `enacted` is the player/governing-body switch;
/// an un-enacted law is inert and changes nothing.
struct law
{
    std::string     id;      ///< Stable identifier, e.g. "LAW-EXTRACTION-LEVY".
    std::string     name;    ///< Display name for the budget ledger and the laws surface.

    /// WHO ENACTED IT. Null for the prototype's corp-facing laws, which have no
    /// author because they had nowhere to send their proceeds. An `import_tariff`
    /// REQUIRES one: the author is both the jurisdiction whose market the duty
    /// applies in and the treasury it is paid into, so a tariff with a null
    /// author is inert by construction rather than by a special case.
    entity_id       author_nation = null_entity;
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

/// True iff ANY enacted law in @p w is an import tariff. The whole tariff pass
/// in the clearing tick hangs off this: when it is false — the shipped default,
/// since no tariff is enacted at world setup — not one line of tariff arithmetic
/// runs and the world is bit-identical to the pre-tariff build.
bool any_import_tariff_enacted(const world& w);

/// The import-duty rate levied by @p nation on a sale of @p resource in its own
/// market to a foreign buyer, as a fraction of the trade's value. Rates from
/// several enacted laws STACK ADDITIVELY, matching the extraction levy's own
/// rule, and the total is clamped to [0, 1] so a stack of laws can never charge
/// a buyer more than the goods are worth.
///
/// Pure and deterministic: it walks `w.laws` in the world's authored order and
/// reads nothing else. Zero for a null nation, an unauthored law, or a resource
/// no enacted tariff reaches.
///
/// This is a RATE SET BY LAW, and deliberately not a nation deciding anything:
/// the standing grant for nation behaviour (io-standing-rules § Determinism &
/// data model, 2026-08-18) admits a tariff rate and excludes a nation planner,
/// and nothing here chooses, scores or schedules.
float nation_tariff_rate(const world& w, entity_id nation, resource_type resource);

/// The one-law seed for the prototype (BL-343): an extraction levy on all raw
/// output, UN-ENACTED. Appended to `world::laws` at world setup so the surface
/// has something to switch on, and so an un-enacted law is the shipped default
/// (a levy that charged from turn one would be a balance change smuggled in as
/// a feature).
///
/// @param w    World; `laws` is appended to.
/// @param rate Credits per unit of raw output when enacted.
void seed_prototype_laws(world& w, float rate = 1.0f);
