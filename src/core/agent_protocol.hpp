#pragma once

// ---------------------------------------------------------------------------
// agent_protocol — the --serve line protocol, host-independent (BL-412)
// ---------------------------------------------------------------------------
// BL-278's line protocol used to live entirely inside run_serve (src/main.cpp),
// which made the headless server the ONLY possible host. BL-412 gives the
// protocol a second host — the rendered app's agent seam (agent_seam.hpp) — so
// the request handlers move here, verbatim, and both hosts call them. One
// parser, one validation story, one place for the untrusted-boundary rules
// (io-standing-rules.md: an AI-facing seam is an untrusted input boundary).
//
// What lives here: parsing and servicing of CORPS / BODIES / BLACKBOARD /
// COMMAND / SHUTDOWN / unknown-op lines. What deliberately does NOT live here:
// TICK. The two hosts own their clocks differently — run_serve steps the world
// itself; the live seam defers to the app's sim clock (releasing it when the
// clock is gated) — so service_line only REPORTS a TICK and the host acts.
//
// The wire rules (BL-387/BL-396/BL-397), preserved exactly:
//   - every integer field parses WIDE and is range-checked against its
//     destination's REAL domain before any narrowing cast; floats are checked
//     as the FLOATS they land as (1e300 is a finite double and an infinite
//     float); a violation rejects the COMMAND whole. No truncation, no wrap,
//     no clamp — and a rejection mutates nothing.
//   - the session actor gates both writes (COMMAND -> rejected_not_owner) and
//     private reads (BLACKBOARD -> ERR + END); CORPS and BODIES stay public,
//     matching the BL-068 competitor-visibility rule.

#include "world/corp_command.hpp"
#include "world/entity.hpp"

#include <string>

struct world;
class recipe_registry;

namespace agent_protocol
{

/// The session actor (BL-387): the one corp this connection may act and read
/// as. `as_any` is the explicit research opt-in that lifts the gate.
struct session
{
    entity_id actor  = null_entity;
    bool      as_any = false;
};

/// What the host must do beyond writing the response `out` already carries.
enum class host_op : unsigned char
{
    none,     ///< Response complete; nothing else.
    tick,     ///< A TICK request: the host advances (or releases) its clock,
              ///< then answers "OK tick=<n>" itself. `out` is empty.
    shutdown, ///< SHUTDOWN: `out` carries "BYE"; the host ends the session
              ///< (run_serve exits the process; the live seam closes the
              ///< connection and the app keeps running).
};

/// Echo of a serviced COMMAND line, for the host's transcript (BL-412: a play
/// transcript is a replay artifact). Only meaningful when `was_command`.
struct command_echo
{
    bool                was_command = false; ///< The line's opcode was COMMAND.
    bool                parse_ok    = false; ///< Every field was in-domain (the command reached the seam).
    corp_command        cmd{};               ///< The parsed command (valid only when parse_ok).
    corp_command_result result = corp_command_result::rejected_invalid;
};

const char* result_name(corp_command_result r);

/// Service one request line against @p w. Appends the complete response —
/// newline-terminated line(s), byte-identical to what run_serve has always
/// printed — to @p out, and reports what the host owes beyond replying.
/// @p tick is the host's current econ tick counter: it stamps an applied
/// command's `cmd.tick` and is BLACKBOARD's default `ticks`.
///
/// Only a valid, actor-gated COMMAND mutates @p w (through apply_corp_command,
/// the player-grade seam). Every other opcode is read-only.
host_op service_line(const std::string& line, world& w, const recipe_registry& reg,
                     int tick, const session& s, std::string& out,
                     command_echo* echo = nullptr);

} // namespace agent_protocol
