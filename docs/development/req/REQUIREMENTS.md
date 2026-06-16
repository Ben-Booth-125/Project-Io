# Project Io — Requirements (policy)

This file is the **requirements policy and schema guide**. It is *not* the requirement
record itself.

**The requirement data — every requirement the project has ever set, and how it was
resolved — lives in [`requirements.json`](requirements.json), which is the single source
of truth and the permanent history.** This Markdown file holds only the *rules*: the
record schema, the verification classes, the promote/complete/cancel workflow, and the
no-tool verification policy. When the two ever disagree, `requirements.json` is canon for
*data* and this file is canon for *policy*. References elsewhere to "the REQUIREMENTS.md
archive" mean the `status: "complete" | "cancelled"` records in `requirements.json`.

---

## Where the data lives

`requirements.json` is a wrapper object: `{ version, note, groups[] }`. One object per
promoted Brief; each carries a `rows[]` array of individual requirements. A group is
**active** while its Brief is in flight (`status: "active"`) and becomes permanent history
on resolution (`status: "complete"` or `"cancelled"`) — records are **never deleted**, so
the file is the project's complete requirement history.

### Group object

| Field | Meaning |
|-------|---------|
| `brief` | the Brief slug (the stable key) |
| `title` | the Brief's human title |
| `difficulty` | declared difficulty (OPENS 1–5 scale), or `null` |
| `importance` | declared importance, or `null` |
| `evaluated_difficulty` | retrospective second-guess at the real difficulty once the work landed, or `null` |
| `status` | `active` \| `complete` \| `cancelled` |
| `resolved` | resolution date (`YYYY-MM-DD`), or `null` while active |
| `resolution` | one-paragraph outcome (the old `Resolved:` line) |
| `promoted_from` | OPENS/TODO origin pointer, or `null` |
| `batch` | the publish-set umbrella this Brief belonged to, or `null` |
| `rows` | the requirement records |

### Row object

| Field | Meaning |
|-------|---------|
| `id` | sequential within the Brief (`R1`, `R2`, …) — referenced by TASKS.md `Satisfies:` fields and DEVLOG status lines |
| `requirement` | a single testable outcome, present tense |
| `verification` | **always an array** of classes (below); a qualifier is kept inline as one element, e.g. `"code: diff_rgba"`, `"doc: docs/ui/LENSES.md"` |
| `status` | `pending` \| `complete` \| `failed` |
| `result_metric` | the structured outcome (e.g. `"3/3 goldens PASS <=0.0082%"`), or `""` |
| `notes` | iteration detail: date/reason of failure, or the change made before the next attempt |

**Verification classes:** `build` (clean compile); `code` (symbol/string present in
source — qualify with the pattern); `doc` (file/section exists — qualify with the path);
`headless` (headless harness produces expected output); `visual` (screenshot + golden /
human confirmation); `manual` (human review, no automated check).

**Scope:** apply a full requirement group to Briefs of difficulty 3 and above. For
difficulty 1–2 Briefs an inline `Verification:` note in the task entry is sufficient.

**DEVLOG convention:** every session entry's **Status** line records the requirement
count, e.g. `Status: Complete — 5/6 requirements met (R4 failed; see requirements.json
§ corporation-lens).`

---

## Workflow

1. **At promotion** — append a group object to `requirements.json` with `status: "active"`,
   `resolved: null`, and a `rows[]` derived from the Brief's success criteria (one row per
   testable outcome). Link it from the TASKS.md group header as
   `Requirements: requirements.json § <slug>`.
2. **As tasks land** — update row `status`. A `failed` row does not block the Brief: add a
   note, leave it `pending` or `failed`, refine the responsible task, and retry.
3. **On completion** — when all rows are `complete` (or `failed` rows are accepted as
   explicitly out of scope), flip the group `status` to `"complete"`, set `resolved` +
   `resolution`, and remove the group from TASKS.md and the Brief from OPENS.md. The record
   **stays in the JSON** as permanent history.
4. **On cancellation** — a cancelled group (see TASKS.md § Cancelling a task group) flips to
   `status: "cancelled"` with a `resolution` recording the reason. Its rows keep the real
   status they reached. Re-promoting flips it back to `"active"` and continues from there.

### Verifying when no skill or tool exists

A task is only **complete** (see TASKS.md § Definition of "complete") when each of its
requirements has actually had its **Verification** *run*. When that verification can be
performed with an available skill or tool — a `build`, a `code` grep, the headless harness,
an existing visual-check skill — run it and record the result.

For the `visual` class specifically, a tool now **does** exist: the headless
visual-verification harness (`ProjectIo --verify scripts/verify/<name>.lua`; see
DEVELOPMENT_PRACTICES.md § Visual verification). Author or extend a verify script and
inspect the PNG captures rather than deferring to a manual human check.

When **no** skill or tool exists to perform a requirement's verification, do not silently
downgrade it to an assumption. Instead:

1. **Determine a method.** Define a concrete, repeatable way to test the requirement — what
   is exercised, what input, what observable pass/fail signal.
2. **Author it as a tool, then push it to a skill.** Build the method as a concrete tool — a
   `tools/verify/*.cpp` headless harness, a `scripts/verify/*.lua` check, a script, or a
   documented procedure — then **promote it to a skill** so the next requirement of the same
   shape reuses a permanent, discoverable asset (the `verifier-visual` / `verifier-headless`
   skills are the model; see CLAUDE.md § Skills → *Tool creation is skill creation*).
   **Creating a skill needs user permission:** attempt to author the tool, push it to a
   skill; **if skill creation is denied, request running the tool as a one-off** for this
   requirement. Prefer a saved artifact over a one-off manual check. Either way, record the
   method (or a pointer to it) in the row's `notes`.
3. **Defer only when it needs design.** If establishing the method is impossible without
   non-trivial design — new infrastructure, an architectural decision, or its own scoping —
   do **not** block the task. Record the testing-method work as an [`../OPENS.md`](../OPENS.md)
   Brief (with file pointers and enough context to pick up), leave the requirement `pending`
   with the deferral reason in `notes`, and proceed. A requirement whose method is deferred is
   **not** complete, and the task carrying it is at best *code-complete* until the method lands
   and the verification is run.

### Agent workflow

A planning agent writes both the TASKS.md group and the matching `requirements.json` group
together. An implementation agent reads only its task group (from TASKS.md) and the matching
JSON record — it does not need the full OPENS backlog or DEVLOG history in context.

### Querying

Because the record is structured data, it can be queried directly for insight rather than
read end to end — e.g. all `failed` rows, the count by verification class, every `visual`
requirement and its golden, or all Briefs resolved on a given date. A standalone query tool
is a candidate follow-on (an OPENS Brief), not part of this file.
