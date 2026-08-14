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

**The history is physically time-sliced (2026-08-14, BL-421 — mirroring the backlog's
hot/cold split).** A resolved group's `rows` and `resolution` move to
`docs/development/archive/requirements-<quarter>.json` via
`tools/session/archive_requirements.js`; the group keeps its index fields plus an
`archived` pointer in `requirements.json`. Nothing is deleted — the move is reversible
(`--restore`), `tools/session/requirements_query.js` resolves archived groups
transparently, and `story_check.js` reads through the pointer. **Amend a landed group's
rows or resolution in the cold file**, not here. In-flight (`pending`) groups always stay
hot and whole.

### Group object

| Field | Meaning |
|-------|---------|
| `brief` | the backlog item slug (the stable key) — **the JSON key stays `brief`; it means a backlog item** (see `../BACKLOG.md` / `backlog.json`) |
| `title` | the item's human title |
| `difficulty` | declared difficulty (backlog 1–5 scale), or `null` |
| `importance` | declared importance, or `null` |
| `evaluated_difficulty` | retrospective second-guess at the real difficulty once the work landed, or `null` |
| `status` | `pending` \| `complete` \| `cancelled` — corrected 2026-08-04: **`pending` is the in-flight word in practice, not `active`**, which zero groups use. The legacy duplicates `completed` (9 groups) and `closed` (1, the NR-075 retroactive cut audit) were normalised to `complete` on 2026-08-14 by `archive_requirements.js`, which keeps normalising on sight. |
| `resolved` | resolution date (`YYYY-MM-DD`), or `null` while in flight |
| `resolution` | one-paragraph outcome (the old `Resolved:` line) |
| `promoted_from` | backlog/TODO origin pointer, or `null` |
| `batch` | the delivery-set umbrella this item belonged to, or `null` |
| `rows` | the requirement records |

### Row object

| Field | Meaning |
|-------|---------|
| `id` | sequential within the Brief (`R1`, `R2`, …) — referenced by REFINED.md `Satisfies:` fields and DEVLOG status lines |
| `requirement` | a single testable outcome, present tense |
| `verification` | **always an array** of classes (below); a qualifier is kept inline as one element, e.g. `"code: diff_rgba"`, `"doc: docs/ui/LENSES.md"` |
| `status` | `pending` \| `complete` \| `failed` — the `completed` duplicates were normalised to `complete` on 2026-08-14 (`archive_requirements.js` keeps normalising on sight); one `cancelled` and one `partial` remain as recorded anomalies. Do not widen the enum. |
| `result_metric` | the structured outcome (e.g. `"3/3 goldens PASS <=0.0082%"`), or `""` |
| `notes` | iteration detail: date/reason of failure, or the change made before the next attempt |

**Verification classes:** `build` (clean compile); `code` (symbol/string present in
source — qualify with the pattern); `doc` (file/section exists — qualify with the path);
`headless` (headless harness produces expected output); `visual` (screenshot + golden /
human confirmation); `manual` (human review, no automated check); `review` (the static,
no-compile `verifier-review` pass over an integrated diff — added by DELIVERY step 4a and
undocumented here until 2026-08-04, though 21 rows already use it).

**Scope — two thresholds, and they are not the same one.** DELIVERY step 0 is the harder gate:
**any item that changes `src/` writes one item-spanning requirement first**, difficulty
irrelevant, so decomposition is shaped by it. Beneath that, a **full requirement group** is for
items of difficulty 3 and above; for difficulty 1–2 an inline `Verification:` note in the task
entry is enough. Doc-only items are exempt from both. *(Reconciled 2026-08-04 — this section
previously stated only the difficulty threshold, which read as licence to skip step 0 on a small
`src/` change.)*

**DEVLOG convention:** every session entry's **Status** line records the requirement
count, e.g. `Status: Complete — 5/6 requirements met (R4 failed; see requirements.json
§ corporation-lens).`

---

## Workflow

1. **At promotion** — append a group object to `requirements.json` with `status: "active"`,
   `resolved: null`, and a `rows[]` derived from the Brief's success criteria (one row per
   testable outcome). Link it from the REFINED.md group header as
   `Requirements: requirements.json § <slug>`.
2. **As tasks land** — update row `status`. A `failed` row does not block the Brief: add a
   note, leave it `pending` or `failed`, refine the responsible task, and retry.
3. **On completion** — when all rows are `complete` (or `failed` rows are accepted as
   explicitly out of scope), flip the group `status` to `"complete"`, set `resolved` +
   `resolution`, and remove the group from REFINED.md and the item from the backlog. The record
   **stays in the JSON** as permanent history.
4. **On cancellation** — a cancelled group (see REFINED.md § Cancelling a task group) flips to
   `status: "cancelled"` with a `resolution` recording the reason. Its rows keep the real
   status they reached. Re-promoting flips it back to `"active"` and continues from there.

### Verifying when no skill or tool exists

A task is only **complete** (see REFINED.md § Definition of "complete") when each of its
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
   do **not** block the task. Record the testing-method work as a backlog item in
   [`../BACKLOG.md`](../BACKLOG.md) (with file pointers and enough context to pick up), leave the requirement `pending`
   with the deferral reason in `notes`, and proceed. A requirement whose method is deferred is
   **not** complete, and the task carrying it is at best *code-complete* until the method lands
   and the verification is run.

### Agent workflow

A planning agent writes both the REFINED.md group and the matching `requirements.json` group
together. An implementation agent reads only its task group (from REFINED.md) and the matching
JSON record — it does not need the full backlog or DEVLOG history in context.

### Querying

Because the record is structured data, it is queried, not read end to end. The tool is
**`node tools/session/requirements_query.js`** (BL-421): default output is the in-flight
groups, index fields only; `--status`/`--grep`/`--batch` filter; a `<brief>` argument or
`--full` fetches whole groups with archived rows resolved transparently; `--failed` and
`--class <verification class>` are row-level sweeps over the whole history; `--count`,
`--fields`, `--table` shape the output. Do not load `requirements.json` whole — the
in-flight view is a few KB; the history answers through the tool.
