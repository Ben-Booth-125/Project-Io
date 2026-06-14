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

*No active requirements. Both the visual-verification harness (Phase 1) and the
corporation-lens groups completed 2026-06-14 — sections removed per the lifecycle
above. See the DEVLOG entries "Visual-verification harness (Phase 1)" and
"Corporation lens".*
