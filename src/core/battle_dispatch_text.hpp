#pragma once

// ---------------------------------------------------------------------------
// BL-468 — the words a battle dispatch is said in
// ---------------------------------------------------------------------------
// WHY THIS IS ITS OWN TRANSLATION UNIT, and it is not tidiness. The natural home
// for this would be `session_history.cpp`, beside the nation-voiced and counsel
// posters it sits with. But that file includes `core/app.hpp`, which includes
// <SDL3/SDL.h> — so anything living there cannot be syntax-checked without the
// full GUI stack and cannot be linked into a headless harness at all.
//
// Here, depending on nothing but `world/battle_system.hpp` and <string>, the
// phrase bank is both compilable and ASSERTABLE: `battle_engagement_harness`
// exercises it directly. Given that a dispatch line is exactly the kind of thing
// that goes wrong silently — a phase that never fires, a bank with a hole in it,
// a line that says the attacker won when the defender held — being able to test
// it is worth a file.
//
// THE TEXT IS A PURE FUNCTION of the dispatch record. No RNG stream is consumed:
// phrase selection folds `battle_dispatch::stream_seed` with the round index, the
// BL-290 tongue-bank idiom, so wording is stable for a given fight and varies
// between fights without the dispatch layer being able to move the simulation.
//
// SCI-FI VOICE, NEVER EARTH-FLAVOURED — the standing naming rule. Real history is
// a mechanism reference, never a name source, and that applies to the words a
// battle is reported in as much as to a nation's name.
// ---------------------------------------------------------------------------

#include "world/battle_system.hpp"
#include "world/nation_step.hpp" // BL-577: contract_dispatch — the mercenary-contract sibling below

#include <string>

struct world;

/// One line of battle traffic for the Field channel, from one dispatch record.
///
/// Names the corporations involved by looking them up in @p w; a corp that has
/// gone missing degrades to a neutral noun rather than an empty string, because a
/// dispatch with a hole in it reads as a bug and this layer must never be the
/// thing that looks broken.
std::string battle_dispatch_line(const world& w, const battle_dispatch& d);

/// The phase word both surfaces share (BL-468's "the same vocabulary keys the
/// battle card's phase readout, so the two surfaces never disagree").
const char* battle_phase_word(battle_phase p);

/// BL-577's sibling to `battle_dispatch_line`: one line of Public-channel
/// contract traffic, from one `contract_dispatch` record — offer issued,
/// accepted, completed, failed or abandoned. THE PUBLIC CHANNEL IS NATION-
/// VOICED ONLY (CHAT.md), so every phrasing speaks in the CLIENT nation's
/// first person ("We ..."), the same register `session_history.cpp`'s
/// `post_nation_agency_comms` already established; the contractor corp is
/// named where the event has one (a commercial engagement between named
/// counterparties, not a rival's internals — CONTRACTS.md never treats a
/// contract's existence as private the way DISCOVERY.md treats production).
///
/// Deterministic on the SAME terms as `battle_dispatch_line`: phrase choice
/// folds the record's own `id` (offers and contracts each draw from their own
/// monotonic allocator, so it is already a stable per-record scalar) with the
/// event `kind`, consuming no RNG draw.
std::string contract_dispatch_line(const world& w, const contract_dispatch& d);
