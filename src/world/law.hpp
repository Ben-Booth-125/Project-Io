#pragma once

#include "components.hpp"
#include "condition_set.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

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
/// unconditional once enacted". `enacted` is the governing-body switch; an
/// un-enacted law is inert and changes nothing.
///
/// BL-480: A LAW HAS AN AUTHOR. `enacting_nation` names the nation that passed
/// it, and the law's reach is that nation's jurisdiction — the levy touches only
/// flows landing on tiles the author owns (`world::tile_to_nation`). A law with
/// no author cannot exist: every seeding path assigns one, and a record whose
/// author is null (or dangling) is ill-formed and charges NOTHING — defensively
/// inert rather than silently authorless, because an authorless charge is
/// exactly the money-destroying flow this item removed.
struct law
{
    std::string     id;      ///< Stable identifier, e.g. "LAW-EXTRACTION-LEVY".
    std::string     name;    ///< Display name for the budget ledger and the laws surface.
    condition_set   conditions;                                ///< Empty = always-on once enacted.
    law_effect_kind effect  = law_effect_kind::extraction_levy;
    bool            enacted = false;

    /// BL-480: the nation that enacted this law. Never null on a well-formed
    /// record; the levy is a TRANSFER into this nation's treasury, and its
    /// jurisdiction (the tiles it owns) bounds who pays.
    entity_id enacting_nation = null_entity;

    /// Effect magnitude, read per family. For `extraction_levy`: credits charged
    /// per unit of raw output extracted this tick.
    float rate = 0.0f;

    /// Resource the effect is restricted to, or `all_resources` for every one.
    /// A levy on iron ore and a levy on everything are the same record.
    static constexpr int all_resources = -1;
    int scope_resource = all_resources;
};

/// The per-corp effects in force this tick, resolved once from the enacted set.
/// A resolved bundle rather than a live query, so the money loop reads fixed
/// arrays instead of re-evaluating predicates per building (the determinism
/// argument and the performance one point the same way).
///
/// BL-480 reshaped the bundle from one flat rate array to ONE SCHEDULE PER
/// AUTHOR NATION, because the levy is now a transfer: the money loop must know
/// which treasury each charged credit lands in, and jurisdiction (which tiles
/// the levy reaches) is the author's, not the law list's. Laws by the same
/// nation still stack additively into one schedule (the old L6 rule, kept).
struct law_effects
{
    struct levy_schedule
    {
        /// The nation whose treasury the charge is credited to, and whose
        /// territory (`world::tile_to_nation`) bounds which output is charged.
        entity_id enacting_nation = null_entity;

        /// Credits per unit of extracted output, by resource. Zero where no
        /// enacted law by this nation reaches that resource.
        std::array<float, resource_count> extraction_levy = {};
    };

    /// One schedule per author nation reached, in first-appearance order over
    /// the world's authored law list — deterministic, like everything else here.
    std::vector<levy_schedule> levies;

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

/// BL-480: choose the nation that authors the seeded prototype law set,
/// deterministically from generation output alone:
///
///   1. The player corporation's home nation (`corporation_component::
///      home_nation`) — the nation whose jurisdiction covers the player's
///      starting extraction, so the seeded law is one that BINDS the player and
///      the laws surface reads "whose law, what it costs me" non-vacuously.
///   2. Fallback (no player corp / no home nation): the nation with the most
///      tiles, ties broken by lowest entity id.
///   3. No nations at all: `null_entity` — and then NO law is seeded, because a
///      law with no author cannot exist.
///
/// Pure over generation state; no RNG stream is consumed.
entity_id choose_levy_author(const world& w);

/// The one-law seed for the prototype (BL-343, reshaped by BL-480): an
/// extraction levy on all raw output, ENACTED AT GENERATION by its author
/// nation (`choose_levy_author`). Enactment is a governing-body act, not a
/// player one — the balance-ledger checkbox is gone, and a nation CHOOSING to
/// enact mid-campaign is the granted nation-grain scored decision (2026-08-18
/// grant), deliberately not built this sprint: this function is the seam it
/// will call through.
///
/// Seeds nothing when no author nation exists (a law with no author cannot
/// exist), which is also why a nation-free harness world stays law-free.
///
/// @param w    World; `laws` is appended to (or left untouched — see above).
/// @param rate Credits per unit of raw output extracted in the author's
///             jurisdiction.
void seed_prototype_laws(world& w, float rate = 1.0f);
