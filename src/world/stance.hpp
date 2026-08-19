// Corp stance (BL-448) — a directed hostility map and a canonicalised friendship
// map between corporations, plus the four verbs that mutate them. This is the
// substrate a later engagement trigger (BL-315) and a UI surface (BL-449) will
// read; neither is built here.
//
// NOT src/world/standing.{hpp,cpp} — that file is BL-262's coarse power read
// (reach/capital/market-share bands), a different question entirely. "Stance"
// is how a corp feels about another; "standing" is how strong it is.
//
// SETTLED 2026-08-17 (Ben, on NR-302): the HYBRID reading.
//   - Hostility is DIRECTED: you can be attacked without agreeing (the ambush
//     property BL-458 needs — a corp can be at war and not know it yet).
//   - Friendship is SYMMETRIC: you cannot be befriended without agreeing, so a
//     friendship row is always evidence both corps chose it. Reached through an
//     offer/accept handshake; a pending, unaccepted offer is NOT a stance and is
//     stored in its own table (invariant 1) so no consumer can read it as one.
//
// Two more invariants this shape needs, both non-optional (see BL-448's design):
//   - Declaring hostility dissolves any existing friendship row ATOMICALLY, in
//     the same call, rather than leaving a contradictory pair for a consumer to
//     arbitrate.
//   - Consumers ask the right question of the right table: `is_hostile` is
//     directional, `are_friends` is not, and the two are never collapsed behind
//     one `stance_between()` accessor that hides which is which.
//
// A rejected mutation leaves every table untouched — the AI-facing / command-seam
// standing rule (io-standing-rules.md: validate before mutating, reject the whole
// command on violation, never partial-apply).

#pragma once

#include "world.hpp"

#include <cstdint>
#include <utility>

/// Canonicalises a corp pair for the symmetric friendship/offer-adjacent lookups
/// that don't care about direction: always (min id, max id). Two ids that are
/// equal canonicalise to themselves — callers reject self-pairs before this, it
/// never has to decide what a self-friendship would mean.
std::pair<entity_id, entity_id> canonical_pair(entity_id a, entity_id b);

/// Directed: true iff `from` has declared hostility toward `to`. NOT necessarily
/// reciprocated — a corp can be hostile toward one that has never heard of it,
/// which is the whole point of the ambush property (BL-458's future use).
bool is_hostile(const world& w, entity_id from, entity_id to);

/// Symmetric: true iff `a` and `b` hold a mutual, accepted friendship row.
/// Order-independent — `are_friends(a, b) == are_friends(b, a)` always.
bool are_friends(const world& w, entity_id a, entity_id b);

/// True iff `offerer` has a live, unaccepted friendship offer outstanding to
/// `target`. Directed — an offer from A to B does not read as one from B to A.
bool has_pending_friendship_offer(const world& w, entity_id offerer, entity_id target);

/// Outcome of a stance mutation. Only `applied` mutates the world; every
/// rejection leaves every stance table untouched.
enum class stance_result : uint8_t
{
    applied = 0,
    rejected_invalid, ///< Unknown corp, or from == to (a corp cannot stance itself).
    rejected_state,   ///< No-op in current state — already hostile, offer already
                       ///< pending, nothing there to return to neutral, etc.
};

/// Declares `from` hostile toward `to`. Unilateral — `to` need not agree, need
/// not know, and need not be reachable in any other sense; that is deliberate
/// (see the header comment). Atomically dissolves any existing friendship row
/// between the pair as part of the SAME mutation (invariant 2) — a corp is
/// never left reading as both hostile and friendly toward the same target.
/// Rejects (mutating nothing) if `from == to`, either corp is unknown to `w`,
/// or `from` is already hostile toward `to`.
stance_result declare_hostile(world& w, entity_id from, entity_id to);

/// Offers friendship from `offerer` to `target`. Records a PENDING offer only —
/// not a stance (invariant 1); nothing reads this as friendship until accepted.
/// Rejects if `offerer == target`, either corp is unknown, the pair is already
/// friends, `offerer` is currently hostile toward `target` (declare
/// return_to_neutral first — a corp does not get to threaten and court in the
/// same breath), or an identical offer is already pending.
stance_result offer_friendship(world& w, entity_id offerer, entity_id target);

/// `target` accepts a pending offer from `offerer`. Moves the offer into the
/// canonicalised friendship table and clears the pending entry in the same
/// mutation. Rejects if no such offer is pending (including: it was never
/// made, was made in the other direction, or was already accepted/withdrawn).
stance_result accept_friendship(world& w, entity_id offerer, entity_id target);

/// Unilateral return-to-neutral, issued by `corp` against `counterparty`:
/// clears `corp`'s own directed hostility toward `counterparty` (if any) and/or
/// the symmetric friendship row between them (if any) — either party may
/// dissolve a friendship, matching the design's "unilateral by either party"
/// rule. Also withdraws any pending offer between the two, in either
/// direction, since a corp returning to neutral should not leave a stale offer
/// live. Rejects (`rejected_state`, mutating nothing) if none of the above
/// existed to clear.
stance_result return_to_neutral(world& w, entity_id corp, entity_id counterparty);
