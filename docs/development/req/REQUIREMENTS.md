# Project Io — Requirements

One table per active TODO item. Each table is created when the item is promoted to
TASKS.md and removed when the item is complete. The task group in TASKS.md links here
by section; tasks carry `Satisfies: Rn` fields pointing at individual rows.

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

1. **At promotion** — create a section here named after the TODO item slug. Derive
   requirements from the TODO description's success criteria; add a row per testable
   outcome. Link it from the TASKS.md group header as
   `Requirements: [REQUIREMENTS.md § <slug>](req/REQUIREMENTS.md#<slug>)`.
2. **As tasks land** — update statuses. A `failed` status does not block the item:
   add a note, leave the row as `pending`, refine the responsible task, and retry.
3. **On completion** — when all rows are `complete` (or `failed` rows are accepted as
   explicitly out of scope), remove the group from TASKS.md and the item from TODO.md.
   Delete the section from this file.

**Scope:** apply requirements tables to items of difficulty 3 and above. For
difficulty 1–2 items an inline `Verification:` note in the task entry is sufficient.

**DEVLOG convention:** every session entry's **Status** line records the requirement
count, e.g.:
`Status: Complete — 5/6 requirements met (R4 failed; see REQUIREMENTS.md § corporation-lens).`

### Agent workflow

A planning agent writes both the TASKS.md group and this file's section together.
An implementation agent reads only its task group (from TASKS.md) and the matching
section here — it does not need the full TODO backlog or DEVLOG history in context.

---

## Active requirements

### corporation-lens

| ID | Requirement | Verification | Status | Notes |
|----|-------------|--------------|--------|-------|
| R1 | `overlay_mode::corporation` is declared in `src/ui/ui_state.hpp` | `code: overlay_mode::corporation` | pending | |
| R2 | `icons::corporation` is declared in `icons.hpp` and implemented in `icons.cpp` as a filled square with inner dot | `code: icons::corporation` + `visual` | pending | |
| R3 | A corporation lens button is visible in the overlay strip on the Planetary canvas | `visual` | pending | |
| R4 | Under the corporation lens, tiles with a corporate building are tinted in that corporation's colour | `visual` | pending | |
| R5 | Player-corporation tiles are visually distinct from rival tiles (border or distinct colour) | `visual` | pending | |
| R6 | Tiles with no corporate buildings render in terrain colour, not nation tint | `visual` | pending | |
| R7 | The corporation lens render pass is guarded by `overlay_mode::corporation` and exists only in `body_surface_canvas.cpp` | `code: overlay_mode::corporation` in `body_surface_canvas.cpp`; absent from `solar_system_canvas.cpp` and `circumplanetary_canvas.cpp` | pending | |
| R8 | `docs/ui/LENSES.md` exists and contains a Corporation lens section | `doc: docs/ui/LENSES.md` | pending | |
| R9 | `docs/ui/LENSES.md` records the ownership definition (building-tile semantics, no influence radius) | `doc: docs/ui/LENSES.md` | pending | |
