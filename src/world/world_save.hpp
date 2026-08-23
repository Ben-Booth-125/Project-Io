#pragma once

#include "world.hpp"

#include <cstdint>
#include <iosfwd>

// ---------------------------------------------------------------------------
// World snapshot -- the flat-binary save (BL-536)
// ---------------------------------------------------------------------------
// The whole-world serialiser BL-107 has been blocked on since 2026-07-05, where
// it is named as "a WORLD-SNAPSHOT flat-binary serialiser... no backlog item
// exists for it yet". This is that serialiser, and the magic + version header
// below is BL-107's design realised on the stream it was always about.
//
// It is the FOURTH stream in the project and the first that covers the world
// rather than one slice of it. The other three -- `history_log` (IOHL),
// `order_book` (IOOB) and `procurement` (IOPC) -- established the convention
// this follows: magic first, version second, and a mismatch REJECTED rather
// than reinterpreted as a changed struct layout.
//
// WHAT IS IN THE STREAM, in three buckets, because the second is the one that
// bites (docs/development/backlog.json BL-536):
//
//   1. SERIALISED -- every authoritative container on `world`, plus the
//      well-known entities, the belt, and every id counter INCLUDING the
//      allocator cursor (world::next_entity_id).
//   2. REBUILT, never read -- `body_tile_index`, `astar_cost_cache`,
//      `body_reach_cost`, `body_market_index` and its two stamps, plus
//      `ai_decisions` and `current_day_tick`. Pure functions of what bucket 1
//      holds; writing them would only create a second thing to keep in
//      agreement. `clear_derived_state` is what a load calls.
//   3. NOT HERE -- the app-layer envelope (sim tick, world_params, the
//      generation report, the app histories, the ui_state slice). That lives
//      in `src/core/save_game.{hpp,cpp}`, deliberately: `world/*` stays SDL-
//      and UI-free, so the headless harness can exercise this half alone.
//
// corp_modifiers looks like bucket 2 and is NOT -- see the note on
// `write_world_snapshot`, and NR-510.
//
// The history log and the province partition are written by EMBEDDING the
// existing `write_history_log` / `read_history_log` pair whole, nested magic
// and all, rather than by re-writing their records here. Reuse over a second
// definition of intricate bytes (NR-512).
// ---------------------------------------------------------------------------

/// Leading magic identifying a world snapshot: the bytes 'I','O','S','V'.
/// `read_world_snapshot` rejects any stream not starting with exactly this,
/// rather than guessing at a layout (BL-107).
inline constexpr uint32_t world_save_magic =
    (uint32_t('I')) | (uint32_t('O') << 8) | (uint32_t('S') << 16) | (uint32_t('V') << 24);

/// Format version. BUMP THIS on any change to what is written or in what order
/// -- an added field, a reordered one, a widened enum, a change to
/// `resource_count`. `read_world_snapshot` rejects a mismatch rather than
/// attempting to reinterpret a differently-shaped record.
///
/// Broader versioning (migration, forward compatibility) stays deferred until
/// the data model stabilises, per docs/tech/TECH_FOUNDATIONS.md. For the
/// prototype, rejection IS the migration story.
///
/// Bumped to 2 by BL-546: `corp_reputation` (one float per pair) was replaced
/// in place by `sentiment` (two floats per pair, Access and Trust). A v1
/// snapshot's records are half the width, so every field after them would be
/// misread — which is exactly the case strict version equality exists for.
inline constexpr uint32_t world_save_version = 2;

/// Write @p w as a complete world snapshot.
///
/// Writes magic + version, then every serialised container in a fixed order.
/// Derived caches are skipped (see `clear_derived_state`). `corp_modifiers` IS
/// written despite being described on `world` as derived: its stored ORDER is
/// the cross-tick earn order, which a re-fold from `earned_techs` x the gate
/// table cannot reconstruct, and `modified_scalar` folds in stored order over
/// operations that do not commute (NR-510).
///
/// Never fails by return value -- an unwritable stream surfaces through the
/// stream's own state, which the caller in `save_game.cpp` checks once for the
/// whole file rather than per field.
///
/// @param w   World to serialise.
/// @param out Destination stream, opened binary.
void write_world_snapshot(const world& w, std::ostream& out);

/// Read a world snapshot written by `write_world_snapshot` into @p w.
///
/// ATOMIC ON FAILURE. The stream is read into a scratch `world` and moved into
/// @p w only once every field has been read successfully, so a wrong magic, a
/// mismatched version, a corrupt length prefix or a truncated tail leaves @p w
/// EXACTLY as it was. A caller never has to distinguish "failed cleanly" from
/// "failed halfway".
///
/// On success the derived caches are already cleared, so the first read of any
/// of them rebuilds against the loaded world.
///
/// @param w  Destination world, untouched unless the whole read succeeds.
/// @param in Source stream, opened binary.
/// @return   True if the snapshot was read whole; false on any rejection.
bool read_world_snapshot(world& w, std::istream& in);

/// Drop every derived cache on @p w so the next read of each rebuilds.
///
/// Called by `read_world_snapshot` on success. Exposed because the round-trip
/// harness asserts the rebuild produces identical contents, which means it
/// needs to clear a freshly-generated world the same way a load does.
///
/// Clears: `body_tile_index`, `astar_cost_cache`, `body_reach_cost`,
/// `body_market_index` (and its count/max-id stamps), `ai_decisions`, and
/// `current_day_tick`. Does NOT touch `corp_modifiers` -- see above.
///
/// @param w World whose caches are dropped.
void clear_derived_state(world& w);
