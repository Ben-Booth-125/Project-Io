# Project Io — Requirements

One table per TODO item that has been promoted to TASKS.md. A table is created at
promotion and **kept permanently** — when its item completes (or is cancelled) the
section is moved to the **Completed / cancelled** archive at the foot of this file, not
deleted. This file is therefore a **permanent record** of every requirement the project
has ever set and how it was resolved. The task group in TASKS.md links here by section;
tasks carry `Satisfies: Rn` fields pointing at individual rows.

---

## Guide

### Table format

| ID | Requirement | Verification | Status | Notes |
|----|-------------|--------------|--------|-------|

**Columns:**
- **ID** — sequential within the item (R1, R2, …). Referenced in TASKS.md task
  `Satisfies:` fields and in DEVLOG status lines.
- **Requirement** — a single testable outcome in the present tense ("The `corporation`
  glyph is declared in `icons.hpp`").
- **Verification** — one of: `build` (clean compile); `code: <pattern>` (symbol or
  string present in source); `doc: <path>` (file or section exists); `headless`
  (headless harness produces expected output); `visual` (screenshot + human
  confirmation); `manual` (human review, no automated check).
- **Status** — `pending` | `complete` | `failed`.
- **Notes** — iteration detail: date of failure, reason, or change made before the
  next attempt.

### Workflow

1. **At promotion** — create a section under **Active requirements** named after the
   TODO item slug. Derive requirements from the TODO description's success criteria; add
   a row per testable outcome. Link it from the TASKS.md group header as
   `Requirements: [REQUIREMENTS.md § <slug>](req/REQUIREMENTS.md#<slug>)`.
2. **As tasks land** — update statuses. A `failed` status does not block the item:
   add a note, leave the row as `pending`, refine the responsible task, and retry.
3. **On completion** — when all rows are `complete` (or `failed` rows are accepted as
   explicitly out of scope), remove the group from TASKS.md and the item from TODO.md,
   then **move this section, intact, to the Completed / cancelled archive** at the foot
   of the file. Add a `Resolved:` line above the table (date + outcome, e.g.
   `Resolved: 2026-06-14 — complete, all rows met`). **Never delete a section** — the
   archive is the project's permanent requirement history.
4. **On cancellation** — a cancelled group (see TASKS.md § Cancelling a task group)
   moves to the archive the same way, with a `Resolved:` line recording the cancellation
   and reason. Its rows keep the real status they reached. If the item is later
   re-promoted, copy the section back up to **Active requirements** and continue from
   there.

**Scope:** apply requirements tables to items of difficulty 3 and above. For
difficulty 1–2 items an inline `Verification:` note in the task entry is sufficient.

**DEVLOG convention:** every session entry's **Status** line records the requirement
count, e.g.:
`Status: Complete — 5/6 requirements met (R4 failed; see REQUIREMENTS.md § corporation-lens).`

### Verifying when no skill or tool exists

A task is only **complete** (see TASKS.md § Definition of "complete") when each of
its requirements has actually had its **Verification** *run*. When that verification
can be performed with an available skill or tool — a `build`, a `code:` grep, the
headless harness, an existing visual-check skill — run it and record the result.

For the `visual` class specifically, a tool now **does** exist: the headless
visual-verification harness (`ProjectIo --verify scripts/verify/<name>.lua`; see
DEVELOPMENT_PRACTICES.md § Visual verification). Author or extend a verify script
and inspect the PNG captures rather than deferring to a manual human check.

When **no** skill or tool exists to perform a requirement's verification, do not
silently downgrade it to an assumption. Instead:

1. **Determine a method.** Define a concrete, repeatable way to test the
   requirement — what is exercised, what input, what observable pass/fail signal.
2. **Implement it and save it for reuse.** Build the method as a durable, named
   artifact wherever feasible — a headless harness case, a script under `scripts/`,
   a documented procedure, or a new skill — so the next requirement of the same
   shape reuses it rather than re-deriving it. Prefer a saved artifact over a
   one-off manual check. Record the method (or a pointer to it) in the row's Notes.
3. **Defer only when it needs design.** If establishing the method is impossible
   without non-trivial design consideration — it needs new infrastructure, an
   architectural decision, or its own scoping — do **not** block the task. Record
   the testing-method work as a [`../TODO.md`](../TODO.md) item (with file pointers
   and enough context to pick up), leave the requirement `pending` with the
   deferral reason in Notes, and proceed. A requirement whose method is deferred is
   **not** complete, and the task carrying it is at best *code-complete* until the
   method lands and the verification is run.

### Agent workflow

A planning agent writes both the TASKS.md group and this file's section together.
An implementation agent reads only its task group (from TASKS.md) and the matching
section here — it does not need the full TODO backlog or DEVLOG history in context.

---

## Active requirements

*No active requirements. The worklist is empty between work blocks; sections appear here
when a TODO item is promoted, and move to the archive below on completion or
cancellation.*

---

## Completed / cancelled (archive)

Permanent record of resolved requirement groups, newest first. Each carries a
`Resolved:` line and is retained verbatim; re-promote by copying a section back up to
**Active requirements**.

Two groups completed **2026-06-14** before this permanent-history policy was adopted, so
their full requirement tables were deleted under the old "delete on completion" lifecycle
rather than archived. Their authoritative record lives in the DEVLOG; reconstructed
summaries are kept here so the archive is not silent about them:

### visual-verification-harness-phase-2

`Resolved: 2026-06-14 — complete, all rows (V7–V12) met.`

Phase 2 of the harness: a shared canvas command vocabulary backing both player keyboard
navigation and the verify API, a reusable Lua helper library that removes per-script pan
math, and promotion of a proven check to a permanent `verifier-visual` skill. Settled in
session: shared command layer (not independent), one general skill (not per-feature),
`center_tile` via a canvas-consumed pending request (no duplicated transform). See DEVLOG
§ "Visual-verification harness (Phase 2)" (2026-06-14).

| ID | Requirement | Verification | Status | Notes |
|----|-------------|--------------|--------|-------|
| V7 | A `canvas_command` enum and `canvas_command_from_name` mapping exist in `src/ui/canvas_command.hpp`, covering descend, ascend, body next/prev, pan up/down/left/right, zoom in/out, and lens next/prev/clear. | `code: enum class canvas_command` + `code: canvas_command_from_name` | complete | Strand A foundation. |
| V8 | `apply_canvas_command` mutates `ui_state` for every command, and `app::process_events` maps the keybinding table (CANVASES.md § Keyboard) to it, guarded by ImGui keyboard capture. The verify API routes `verify.command(name)` through the *same* dispatch. | `code: apply_canvas_command` (process_events + run_verify call sites) + `visual` (a verify script issuing `verify.command` produces the expected navigation captures) | complete | `handle_key_down` (app.cpp) + `verify.command` both call `apply_canvas_command`. Keyboard injection itself is not headlessly exercisable; the shared dispatch it calls is. |
| V9 | `verify.center_tile(col, row[, zoom])` centres the named tile on the Planetary canvas using the canvas's own transform — no pan arithmetic in Lua. | `visual` (refactored `corporation_lens.lua` centres tile (22,82) as in Phase 1) + `code: center_tile` | complete | Verified 2026-06-14: player (22,82) and rival (42,63) tiles centre exactly. Pending-centre request consumed inside `body_surface_canvas`. |
| V10 | `scripts/verify/lib.lua` provides `sweep_overlays(prefix)`, `tour_buildings(zoom)`, and `frame_tile(col, row, zoom)` layered over the low-level verify API. | `visual` (a script requiring the lib runs each helper and produces captures) + `code: function` (the three helpers) | complete | Auto-loaded by the harness from the script's directory (no `require`). |
| V11 | `scripts/verify/corporation_lens.lua` is refactored onto `lib.lua`/`center_tile` with no hand-computed `set_pan` literals, and reproduces the Phase 1 captures. | `visual` (re-run matches Phase 1 R2–R6 evidence) | complete | Re-run 2026-06-14 reproduces R2–R6 captures via `sweep_overlays`/`frame_tile`. |
| V12 | A `verifier-visual` skill exists under `.claude/skills/verifier-visual/` that runs `ProjectIo --verify <script>` for a given script and reports the captures; "authorising" a check is adding/pointing at a `scripts/verify/*.lua`. | `doc: .claude/skills/verifier-visual/SKILL.md` | complete | Single general skill (owner's call), not per-feature. |

### visual-verification-harness (Phase 1)

`Resolved: 2026-06-14 — complete, all rows (V1–V6) met.`

Headless `--verify` capture mode, dependency-free PNG writer, and the `verify` Lua API
that makes the `visual` requirement class runnable without a human at the screen. Full
table predates this policy; see DEVLOG § "Visual-verification harness (Phase 1) +
Corporation lens closed" (2026-06-14) for the V1–V6 outcomes.

### corporation-lens

`Resolved: 2026-06-14 — complete, all 9 rows (R1–R9) met.`

First **cancelled** (4/9 met — R1, R7, R8, R9 — when the visual rows R2–R6 had no runnable
verification tool), then **re-verified and closed** once the visual-verification harness
landed: R2–R6 confirmed via PNG inspection. Full table predates this policy; see DEVLOG
§ "Corporation lens" and § "Visual-verification harness (Phase 1) + Corporation lens
closed" (both 2026-06-14) for the per-row outcomes.
