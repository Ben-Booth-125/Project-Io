#pragma once

#include <cstdint>

// ---------------------------------------------------------------------------
// modifier_set (BL-479) — the closed vocabulary of scalars a meta effect moves
// ---------------------------------------------------------------------------
// The condition side of the meta layer has one shared object (BL-342's
// `condition_set`, read by laws, techs and quests alike). This header is the
// EFFECT side's counterpart: the closed list of scalars a tech (BL-479) or a
// law (BL-480) may move, plus the one pure function that applies a modifier to
// a value. Authored once and shared — tech and law naming different subject
// enums would be the same defect as tech and law wording the same predicate
// differently, which is exactly what condition_set exists to prevent.
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
    extraction_rate = 0, ///< Units/tick an extraction site draws. WIRED (BL-479):
                         ///  applied per-corp in `extraction_nominal`
                         ///  (economy_system.cpp), the single definition of one
                         ///  site's nominal draw.
    processing_yield,    ///< Output units per recipe batch. Vocabulary only.
    unit_upkeep,         ///< A unit's per-tick upkeep draw (BL-454). Vocabulary only.
    logistics_cost,      ///< Per-unit haulage on the reach/convoy network. Vocabulary only.
    wage_floor,          ///< The wage term of building opex (BL-049). Vocabulary only.
    collapse_strain,     ///< The era's collapse pressure (BL-477): the ancient
                         ///  era's imperial strain (the Fall arc's accumulator),
                         ///  the industrial era's rupture proximity. The
                         ///  strain/cohesion-adjacent subject the era-collapse
                         ///  paradigm requires expressible. Vocabulary only.
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
/// of the tech effect union (tech_gate.hpp) and, later, of a law's effect
/// (BL-480). Trivially copyable, fixed layout — a future save-format record.
struct scalar_modifier
{
    modifier_subject subject   = modifier_subject::extraction_rate;
    modifier_op      op        = modifier_op::multiply;
    float            magnitude = 1.0f;
};

/// Apply one modifier to a value. Pure, branch-on-enum arithmetic — the whole
/// effect model, on purpose. Fold order over a modifier LIST is the caller's
/// contract (world::modified_scalar folds in stored order, which is earn
/// order), because add and multiply do not commute and determinism requires
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
