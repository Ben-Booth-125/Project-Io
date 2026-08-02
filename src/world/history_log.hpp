#pragma once

#include "hard_coded_world.hpp" // generation_report — seed_genesis_history's input
#include "world.hpp"

#include <cstdint>
#include <iosfwd>

// ---------------------------------------------------------------------------
// World history log — serialisation + the genesis-chapter bridge (BL-208)
// ---------------------------------------------------------------------------
// world.hpp declares the shape (history_topic, world_history_entry,
// world::history_log); this header is the FIRST flat-binary serialisation seam
// in world/* (docs/tech/TECH_FOUNDATIONS.md: "the first thing to add when the
// serialiser lands is a leading magic + version header" — BL-107), plus the
// bridge that copies PLANETOLOGY's per-body dated history into world state.
//
// Deliberately standalone from the decision/agency/trade_route sources: those
// are wired additively at their own existing emission sites (corp_ai.cpp,
// economy_system.cpp, supply_system.cpp) using ONLY world.hpp's
// history_topic/world_history_entry, so none of those translation units need
// to include this header or link history_log.cpp — avoiding a TU ripple into
// every hand-written tools/verify/README.md recipe that already links them.
// Only app.cpp (the genesis hook) and a harness that exercises the
// serialiser/bridge directly need this file.
// ---------------------------------------------------------------------------

/// Leading magic identifying a history-log stream: the bytes 'I','O','H','L'.
/// read_history_log rejects any stream that does not start with this exactly
/// (BL-107's "first thing to add" rule) rather than guessing at a layout.
inline constexpr uint32_t history_log_magic =
    (uint32_t('I')) | (uint32_t('O') << 8) | (uint32_t('H') << 16) | (uint32_t('L') << 24);

/// Format version. Bump on any layout change to `world_history_entry` or the
/// stream shape; read_history_log rejects a mismatched version rather than
/// attempting to reinterpret a differently-shaped record.
inline constexpr uint32_t history_log_version = 1;

/// Sanity ceiling on one entry's `event`/`consequence` byte length. Guards
/// read_history_log against allocating a huge buffer from a corrupt or
/// maliciously-crafted length prefix; no real entry approaches this.
inline constexpr uint32_t history_log_max_field_bytes = 1u << 20; // 1 MiB

/// Sanity ceiling on the stream's declared entry count. Guards
/// read_history_log against an eager large allocation from a corrupt or
/// maliciously-crafted count BEFORE the per-entry field reads get a chance to
/// fail on a truncated stream — a claimed count near UINT32_MAX would
/// otherwise size a reservation in the hundreds of gigabytes. No real
/// campaign's log approaches this.
inline constexpr uint32_t history_log_max_entries = 1u << 24; // ~16.7M entries

/// Bridge generation output into world state — BL-208's first bridge from
/// `generation_report` (presentation-only; per hard_coded_world.hpp it never
/// otherwise reaches `world`) into `world::history_log`.
///
/// For each body in @p report, copies:
///   - its `state.history` dated lines (deep-time + historical) as
///     `topic = genesis`, tagged with that body's world entity id;
///   - its `state.checkpoints` branch decisions as `topic = checkpoint`,
///     tagged the same way, with a timestamp resolved from the body's own
///     dated lines (checkpoint_record itself carries no timestamp — see
///     history_log.cpp for the resolution rule).
/// The two are merged and stable-sorted into one chronological (oldest-first)
/// run per body before appending, so a per-body read of `history_log` is a
/// coherent biography rather than "every history line, then every checkpoint".
///
/// A body in @p report with no matching entity in `w.bodies` (by name) is
/// skipped defensively; this should not happen on a real generation_report.
///
/// Call exactly once per generation, after `make_hard_coded_world` produces
/// both @p w and @p report (app::setup_world's hook). Not idempotent — calling
/// it twice on the same world duplicates the genesis chapter.
///
/// @param w      World whose `history_log` receives the new entries (appended).
/// @param report The generation_report produced alongside @p w.
void seed_genesis_history(world& w, const generation_report& report);

/// Write @p w's `history_log` as: a 4-byte magic + a uint32_t version header,
/// then a uint32_t entry count, then each entry (int64_t timestamp; uint8_t
/// topic; uint32_t body; uint32_t corp; length-prefixed `event`; length-
/// prefixed `consequence`). Binary, native byte order — the project's settled
/// flat-binary convention (no other world/* stream is byte-order-normalised
/// either). Writes nothing else from `world`; this is the history log only.
///
/// @param w   World whose `history_log` is serialised.
/// @param out Binary output stream, positioned where the stream should start.
void write_history_log(const world& w, std::ostream& out);

/// Read a stream written by write_history_log back into @p w.history_log,
/// REPLACING its current contents on success. Rejects (returns false, leaving
/// @p w.history_log untouched) rather than proceeding to interpret garbage
/// bytes as entries when: the magic does not match, the version is not
/// `history_log_version`, the stream is short/truncated, an entry's topic byte
/// does not name a known `history_topic`, or a length-prefixed field exceeds
/// `history_log_max_field_bytes`.
///
/// @param w  World whose `history_log` is replaced on success.
/// @param in Binary input stream, positioned where the stream should start.
/// @return   True on a fully-read, well-formed stream; false otherwise.
bool read_history_log(world& w, std::istream& in);
