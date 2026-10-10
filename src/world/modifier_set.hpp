#pragma once

#include <cstdint>

// ---------------------------------------------------------------------------
// modifier_set (BL-479) — the closed vocabulary of scalars a meta effect moves
// ---------------------------------------------------------------------------
// The condition side of the meta layer has one shared object (BL-342's
// `condition_set`, read by laws, techs and quests alike). This header is the
// EFFECT side's counterpart: the closed list of SIMULATION SCALARS a meta
// effect may move, plus the one pure function that applies a modifier to a
// value.
//
// CORRECTED 2026-08-22 (Ben's ruling, design register). This comment used to
// say the list was "the scalars a tech (BL-479) OR A LAW (BL-480) may move…
// authored once and shared". BL-480 landed `law_effect_kind` as its own enum
// and law.{hpp,cpp} never includes this header, so the claim was false — and a
// header that misdescribes the shape a future author is extending is the same
// class of defect as tech_node::condition being a descriptive string.
//
// THE EFFECT SIDE IS TWO FAMILIES, DELIBERATELY:
//
//   * SCALAR effects (this file). A subject here names one scalar the
//     simulation already computes, and a modifier moves it. Read by tech
//     (the nation's held nodes, via nation_tech.hpp's `nation_scalar`, BL-1273;
//     the per-corp tech_gate union and world::modified_scalar before it).
//   * MONEY-FLOW effects (law.hpp's `law_effect_kind`). A levy or a tariff is
//     a TRANSFER between two balances, not a scalar the simulation computes —
//     there is no subject here it could be expressed as, and inventing one
//     would distort this vocabulary rather than share it.
//
// The one-vocabulary rule still binds WITHIN each family: two effects of the
// same kind must not name different subject enums. Across the two families it
// does not apply, because they are not the same kind of thing.
//
// Authority: docs/META_LAYER.md § The asymmetry.
//
// Three properties are load-bearing, mirroring condition_set's:
//
//   1. CLOSED. No callback, no Lua hook, no open-ended string subject. A
//      modifier is data — {subject, op, magnitude} — so every effect is
//      deterministic, serialisable and legible in a ledger line. A subject not
//      named here cannot be moved by the meta layer, and that is the point.
//   2. APPEND-ONLY ONCE SERIALISED. `modifier_subject` is a uint8_t inside
//      records that will cross the save seam (BL-107); a value may be appended
//      but never inserted, renumbered or removed — the condition_subject rule
//      (BL-455). One appender per batch (the Fall-arc collision rule).
//   3. A SUBJECT MAY BE COLLAPSE-ADJACENT. BL-477: "each era has a 'collapse'
//      state to avoid — and that's what defines meta." A vocabulary that only
//      names growth-curve dials would build the corporate-player meta the
//      governing-body pivot is moving away from, so the roster carries
//      `collapse_strain` from day one — unwired today, exactly as BL-342
//      shipped military subjects before anything read them: a shape is only
//      proven by an instance.
// ---------------------------------------------------------------------------

/// What a scalar modifier moves. Each subject names ONE scalar the simulation
/// already computes; the wiring note says where (or that it is vocabulary
/// only — named now so tech and law author against the same closed list, wired
/// when an item needs it and NOT before).
///
/// Deliberately small. A subject earns its row by being a scalar some real
/// lever wants to move, not by symmetry with a sibling.
enum class modifier_subject : uint8_t
{
    extraction_rate = 0, ///< Units/tick an extraction site draws. WIRED: the owning
                         ///  corp's home nation's held nodes, folded in
                         ///  `extraction_nominal` (economy_system.cpp), the single
                         ///  definition of one site's nominal draw. Scope: the
                         ///  site's resource.
    // BEN'S RULE, 2026-08-22: an unwired subject must NAME THE ITEM THAT WILL
    // WIRE IT. Vocabulary-only is a PROMISE, not a state — "a shape is only
    // proven by an instance" justifies the first unwired entry and does not
    // justify the fifth. A subject that cannot name an item has not earned its
    // row, and should be removed rather than left waiting.
    processing_yield,    ///< Output units per recipe batch. WIRED (BL-1274): folded
                         ///  per output good in `credit_processing_outputs`
                         ///  (economy_system.cpp), where a run's output accrues.
                         ///  Scope: the output good, the recipe, its group.
    unit_upkeep,         ///< A unit's per-tick upkeep draw (BL-454). Wired by: BL-543
                         ///  (the value anchor turns the authored rates on).
    logistics_cost,      ///< A corporation's convoy leg cost. WIRED (BL-1274): folded
                         ///  into the priced leg in `price_convoy_leg`
                         ///  (supply_system.cpp), the one seam every corporate
                         ///  dispatch prices through. Scope: the cargo good.
    wage_floor,          ///< The wage term of building opex (BL-049). Wired by: BL-538
                         ///  (the schooling budget line raises labour productivity).
    collapse_strain,     ///< The era's collapse pressure (BL-477): the ancient
                         ///  era's imperial strain (the Fall arc's accumulator),
                         ///  the industrial era's rupture proximity. Wired by:
                         ///  BL-477, which keeps the ACCUMULATOR — Ben ruled
                         ///  2026-08-22 that events EXPRESS the collapse metagame
                         ///  rather than driving it, so BL-548 reads this subject
                         ///  and does not own it.
    national_research,   ///< A nation's research points a quarter. WIRED (BL-1274),
                         ///  appended: scope "all" with op add is a "+N% national
                         ///  research" share, summed into `research_multiplier`;
                         ///  scope "building:university" with op multiply scales
                         ///  the University term of `advance_research`.
};

/// How a modifier combines with the value it moves. The three ops the BL-479
/// design names (+ / − / ×) and no more — an open-ended expression tree would
/// re-open the callback door property 1 closes.
enum class modifier_op : uint8_t
{
    add = 0,  ///< value + magnitude
    subtract, ///< value − magnitude
    multiply, ///< value × magnitude (the common case: "+25%" is ×1.25)
};

/// One scalar modifier: `<subject> <op> <magnitude>`. The `modify_scalar` arm
/// of a node's effect (nation_tech.hpp, BL-1274) and, later, of a law's effect
/// (BL-480). Trivially copyable, fixed layout — a future save-format record.
struct scalar_modifier
{
    modifier_subject subject   = modifier_subject::extraction_rate;
    modifier_op      op        = modifier_op::multiply;
    float            magnitude = 1.0f;
};

/// Apply one modifier to a value. Pure, branch-on-enum arithmetic — the whole
/// effect model, on purpose. Fold order over a modifier LIST is the caller's
/// contract (`nation_scalar` folds in NODE-INDEX order — BL-1274), because add and multiply do not commute and determinism requires
/// the order to be stated rather than accidental.
inline float apply_scalar_modifier(float value, const scalar_modifier& m)
{
    switch (m.op)
    {
        case modifier_op::add:      return value + m.magnitude;
        case modifier_op::subtract: return value - m.magnitude;
        case modifier_op::multiply: return value * m.magnitude;
    }
    return value; // unreachable for a well-formed op; never invent arithmetic
}
