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
    ///
    /// An `import_tariff` (D4) reads the SAME field, deliberately: the author is
    /// both the jurisdiction whose market the duty applies in and the treasury it
    /// is paid into, so a tariff whose author is null is inert by construction
    /// rather than by a special case. A second author field was briefly carried
    /// here by the D4 branch and removed at integration — one law, one author.
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
/// THE DEFAULT RATE IS 0.1, DOWN FROM 1.0 (BL-635, 2026-08-26), and the reason
/// is a measurement rather than a preference. The levy is a FLAT charge in
/// credits per unit of raw output — not ad valorem — so its bite is decided
/// entirely by what a unit of raw output is worth, and at 1.0 it had drifted
/// past the whole ancient tier's prices. `scripts/world_gen.lua` authors stone
/// at 1.00 and timber at 1.50; a flat 1.0 therefore took 100% of a unit of
/// stone and 67% of a unit of timber before the miner saw a credit.
///
/// tools/verify/spawn_solvency.cpp measured the effect end to end on the shipped
/// 0 CE spawn: the seated corporation realised 1.27 credits per raw unit sold
/// and paid 1.14 credits of levy on it — a levy taking ~90% of the gross value
/// of everything it dug up, against 3.2% of the corp's outgoings by size, which
/// is why a size-ranked attribution alone would have missed it.
///
/// 0.1 puts it at ~10% of a unit of stone and ~4% of a unit of iron ore. That is
/// a tax on extraction, which is what the design says it is
/// (docs/economy/FINANCE.md § Levies). Nothing about the mechanism changed: it
/// is still flat per unit, still enacted at generation, still a transfer to the
/// author nation's treasury, still bounded by jurisdiction.
///
/// A caller that passes its own rate is unaffected — the law harnesses all do,
/// so this default moves only the generated world.
///
/// @param w    World; `laws` is appended to (or left untouched — see above).
/// @param rate Credits per unit of raw output extracted in the author's
///             jurisdiction.
void seed_prototype_laws(world& w, float rate = 0.1f);
