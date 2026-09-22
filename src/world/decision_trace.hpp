#pragma once

// ---------------------------------------------------------------------------
// Per-rival decision trace (BL-704) — a streaming, opt-in observation sink.
// ---------------------------------------------------------------------------
//
// WHY THIS EXISTS RATHER THAN A READ OF `corp_decision_ring`.
// The ring (corp_command.hpp) holds 256 entries and WRAPS. Once it has wrapped,
// reading it at the end of a run yields the last 256 decisions and SILENTLY
// DROPS everything before them — a trace that lies by omission, which is the
// worst failure mode an evidence artifact can have.
//
// HOW SOON THAT BITES WAS MEASURED, not assumed (decision_trace_harness, seed 0,
// its hand-built registry): 119 decisions by tick 300, then a steady ~7 per 100,
// so the first wrap lands near tick 2200. That is much later than a
// "corps x commands per evaluation" estimate suggests — corps evaluate on a
// staggered cadence and most candidates are rejected. The streaming design still
// earns its place: over a campaign the overflow is a certainty rather than a
// possibility, the sink costs the same either way, and the shipped game's full
// Lua registry is a richer economy than that fixture's three recipes.
//
// This sink is written AS EACH DECISION IS
// RECORDED, at the single flush point in corp_ai.cpp, so it is complete by
// construction. `decision_trace_harness` asserts that completeness directly
// (line count == `corp_decision_ring::total`, the lifetime counter).
//
// THE TRACE IS AN OBSERVATION AND NEVER AN INPUT. Nothing in the scorer, the
// economy step, or any other simulation code reads this sink or its state, and
// its presence must not change a single decision. That is not a convention here
// but an asserted property: `decision_trace_harness` T1 runs the same seed for
// the same number of ticks with the sink open and with it closed and requires
// an identical `world::state_hash`. If that row ever goes red, the trace has
// become an input and the change that did it is wrong.
//
// This is also why the sink lives at file scope in its own TU rather than as a
// field on `world`: it is process-level observability, not world state. It
// therefore touches the flat-binary save/load seam NOT AT ALL — nothing here is
// persistent, so there is no serialisation owed.
//
// COST WHEN OFF. `g_enabled` is false unless `open()` succeeded or the
// environment opted in. The hot path is `if (g_enabled)` around an out-of-line
// call, so a disabled trace does no formatting, no allocation and no I/O — one
// predictable, always-false branch per decision.
//
// FORMAT: JSONL (one JSON object per line). See decision_trace.cpp for the
// schema and the reasoning behind choosing it over CSV.
//
// THREADING: the simulation is single-threaded; this sink assumes that and
// does no locking.

#include <string>

struct corp_decision;

namespace decision_trace
{

/// The hot-path gate. FALSE unless a sink is open. Read by `record` below and
/// by nothing in the simulation — see the header note on the never-an-input
/// property. Mutable process state, deliberately: it is write-only with respect
/// to the world, so it cannot perturb a replay.
inline bool g_enabled = false;

/// Open @p path for writing (truncating any existing file) and emit the meta
/// line. Sets `g_enabled` on success.
///
/// @return true if the sink is open. A false return leaves `g_enabled` false
///         and the simulation entirely unaffected — a trace that cannot be
///         written is a lost observation, never a failed tick.
bool open(const std::string& path);

/// Flush and close the sink, clearing `g_enabled`. Safe to call when closed.
void close();

/// True if `open()` currently holds a sink. For harnesses and reporting; the
/// simulation must not branch on this.
bool is_open();

/// Lifetime count of decisions written by this sink since the last `open()`.
/// Observability about the observer — used by the harness to cross-check the
/// written line count against `corp_decision_ring::total`.
unsigned long long written();

/// Out-of-line worker. Do not call directly; call `record`.
void record_(const corp_decision& d);

/// Append one decision to the trace. The whole cost when disabled is the
/// branch below.
inline void record(const corp_decision& d)
{
    if (g_enabled)
        record_(d);
}

} // namespace decision_trace
