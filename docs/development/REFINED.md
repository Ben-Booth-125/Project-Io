# Project Io — REFINED (active worklist)

> Drained 2026-07-31 (doc sweep): thirteen stale COMPLETE sections removed per the
> retain-one policy — their record lives in DEVLOG.md and req/requirements.json.

# Sprint 2 — BL-210 (oral-history pivot): the settlement rewrite (2026-08-02) — **IN PROGRESS**

Requirements: `req/requirements.json` § checkpoint-branch-model (BL-217), and two not-yet-added
groups for BL-218/BL-219 (added when each is promoted — strict dependency chain, not a fan-out:
BL-218 needs BL-217 landed in `main`; BL-219 needs BL-218 landed). Each runs as its own
worktree-isolated agent, sequenced rather than parallel, since each reads the previous item's
actual code (not just its design).

## Checkpoint/branch model (BL-217) — Files: `src/world/planetology.{hpp,cpp}`,
harness extending `tools/verify/planetology_sweep.cpp`, `docs/generation/PLANETOLOGY.md`.
Satisfies checkpoint-branch-model R1-R6. **In progress.**

## Nations settlement rewrite (BL-218) — not yet promoted; depends on BL-217 landing.

## Corporations history rewrite (BL-219) — not yet promoted; depends on BL-218 landing.

---

# Sprint 1 — Procedural generation v1: the food cluster (2026-08-02) — **COMPLETE**

All three items landed: BL-166 (Hydroponics Bay), BL-168 (Fishing Wharf), BL-170 (rivers &
freshwater generation). Two worktree agents (rivers; the food-building pair, which shared files
so ran sequenced in one worktree), merged and verified in the main session — full CTest 37/37 on
both merges, no regressions. Per-item detail: backlog.json BL-166/BL-168/BL-170 `resolution`;
requirements.json § hydroponics-bay / § fishing-wharf / § river-generation, all rows complete.

The **active, prioritised, actionable worklist** (formerly TASKS.md). Unlike the backlog
([`backlog.json`](backlog.json) metadata + [`BACKLOG.md`](BACKLOG.md) design bodies), every
entry here is a concrete, file-scoped, individually-buildable step ready to execute. Tasks are
**promoted** from a `designed` (`✓`) backlog item (see [`DELIVERY.md`](DELIVERY.md)) and cleared
as they complete — this file is transient and is expected to be empty between work blocks.

> **Proportionality (see DELIVERY.md § Proportionality, and Rule 0).** Promoting an item into
> this file is for *substantial* (Full-mode) work. A quick low-risk high-value change — a
> one-file fix, an obvious cleanup, a cheap optimisation — does **not** need a task group or a
> REQUIREMENTS table: make and verify it directly, then commit. Reach for the full lifecycle
> only where its coordination cost pays back; applying it to trivial work is over-engineering.

## Task format

List tasks in **execution order**, grouped by the item they were promoted
from. Each task carries:

- **A group-scoped ID letter** (A, B, C, …), so dependencies and parallel pairs
  can be named.
- **A difficulty** in brackets (the backlog 1–5 time scale; tasks carry difficulty
  only — priority is a backlog-level triage concept, not a per-task field).
- **A one-line action** — imperative; what to change.
- **File scope** — the files the task is expected to touch. This is what makes
  collisions between tasks visible.
- **Dependencies** — which sibling tasks must land first (or "foundation" /
  "independent root").
- **Parallelisation** — whether it can run concurrently with a sibling, and
  whether as a sub-agent. Only true when the file scopes are **disjoint**.

End each group with a **parallelisation note**: the dependency shape and which
roots are safe to fan out. Concurrent sub-agents run in **separate git worktrees**
(the isolation mechanism); the collision map is a *splitting heuristic* for carving
focused agents, not a hard disjointness gate. Agents build and commit on their own
worktree branch; the integrating session merges, builds, and verifies. See
[`DELIVERY.md`](DELIVERY.md) § Sub-agents & worktrees for the authoritative model.

---

# The disclosure spine (2026-08-01) — **COMPLETE**

All fifteen tasks landed. Build clean, **CTest 35/36** (the one failure is the
pre-existing `econ_stability` Debug timing bound, BL-258 — not a regression), and
**23 goldens blessed and passing at 0.0000%** across the three new/rewritten visual
checks. Requirements `requirements.json` § drill-through-fold (R1–R7),
§ chart-question-log (R1–R5), § corp-dashboard-rollups (R1–R5) are all complete.
Per-item detail: backlog.json BL-214 / BL-247 / BL-248 `resolution`; the idiom's
authority is now `docs/ui/LAYOUT.md` § Drill-through and `docs/ui/MENU.md` § Slot 1.

Raised on delivery and **filed separately rather than absorbed**: the visual-golden
suite has been stale since 2026-07-31 — most checks fail on *world content* (terrain,
generated names, balances) because `src/world/` moved after the 07-30 bless and the
visual half of the cut gate was never re-established. **BL-259**. Summary retained one
cycle; the task detail below is the record of how the batch was decomposed.

Three items promoted as one Batch Delivery: **BL-214** (drill-through idiom) →
**BL-247** (chart question log) → **BL-248** (corporation dashboard roll-ups). A
dependency chain, not a fan-out: BL-214's shared control is the thing the other two
call, and BL-214/BL-247 share four files. **Run main-session-serial** — worktree
agents would collide on `generation_charts.cpp` and `selection_panel.cpp` for no
wall-clock gain on a chain.

**Two design calls taken with Ben, 2026-08-01, asked with the measurements (Rule 0b):**

1. **The Selection band opens expanded-in-place.** Folding its metric card to one line
   would leave ~220 px of a *fixed* 260 px rect empty — the objection the superseded
   three-level design raised against "Glance everywhere", which the binary note never
   resolved. Fixed-rect surfaces therefore rest showing their chart; the chevron opens
   the combined full-screen view. Folded-by-default governs scrolling ledger cards.
2. **The wizard folds per chain stage.** Each stage rests as its verdict line and
   expands to that stage's full view — the wizard becomes the teacher of the idiom, as
   BL-214's Decision 5 argued, without a first-time player meeting one long scroll.

**One decision taken on Ben's behalf:** the fold overlay **joins the Esc ladder**, one
rung below the BL-196 drill (order: exit-confirm → system menu → pop drill → fold
overlay → hide selection → open menu). BL-214's Decision 10 kept depth off the ladder,
but it reasoned about an *in-place stepper*; a full-screen mode with no keyboard exit is
a defect, not a principle. Recorded here and in LAYOUT.md rather than silently taken.

## Drill-through fold (promoted from backlog.json § BL-214)

Requirements: requirements.json § drill-through-fold (R1–R7).

Goal: one binary idiom — **folded** (a verdict line + a chevron) and **expanded** (a true
full-screen overlay showing everything at once). Supersedes the three-level
Glance/Read/Study stepper per Ben's 2026-07-31 post-exemplar note.

**A gates everything.** The state model falls out of *expanded is an overlay*: only one
thing can be expanded at a time, so the state is a single `(surface, key)` target rather
than a per-surface remembered level.

- **[2] A — Foundation: the fold module + its one line of state.** `detail_surface`,
  `fold_state`, `is_expanded / expand / fold`, `fold_chevron`, `fold_overlay_begin/end`,
  and BL-247's `why_note` (same disclosure family, same `ui_state` dependency).
  Files: `src/ui/detail_level.{hpp,cpp}`, `src/ui/ui_state.hpp`. Deps: foundation.
  Satisfies: BL-214 R1, R3; BL-247 R2, R3.
- **[2] B — Esc ladder gains the fold rung.** Files: `src/core/app.cpp`. Deps: A.
  Satisfies: R4.
- **[2] C — Selection band: chevron on the metric card's pager row; the overlay hosts the
  chart, its legend and the BL-196 drill.** Files: `src/ui/selection_panel.cpp`,
  `src/ui/selection_card.cpp`. Deps: A. Satisfies: R1, R2, R5.
- **[3] D — History ledger: Chain stages fold to a verdict line; Story and Tiles take the
  chevron.** Files: `src/ui/tile_inspector.cpp`. Deps: A. Satisfies: R1.
- **[3] E — Wizard: per-stage fold.** Files: `src/core/app.cpp`,
  `src/ui/generation_charts.{hpp,cpp}`. Deps: A. Satisfies: R6.
- **[1] F — LAYOUT.md § Drill-through** — the binary model, the one-expanded-at-a-time
  invariant, the Esc rung, and the extension recipe. Files: `docs/ui/LAYOUT.md`.
  Deps: C, D, E. Satisfies: R7.
- **[1] G — Visual check.** Files: `scripts/verify/drill_through_fold.lua`. Deps: C, D, E.
  Satisfies: R1, R2, R5, R6.

Parallelisation note: A → {B ∥ C ∥ D ∥ E} → {F ∥ G}. Serial in the main session; C/E
share files with the BL-247 group below.

## Chart question log (promoted from backlog.json § BL-247)

Requirements: requirements.json § chart-question-log (R1–R5).

Goal: a chart declares, on request, the question it answers and why that evidence
justifies it — two lines, closed by default, never a tutorial that tells the player what
to ask. `why_note` itself lands in task A above; these tasks author the pairs.

- **[1] H — `chart_row` takes an optional question/why pair; author one per generation
  chart.** Files: `src/ui/generation_charts.{hpp,cpp}`. Deps: A, E.
  Satisfies: R1, R4, R5.
- **[1] I — Pairs on the Selection band's production chart and the drill time-series.**
  Files: `src/ui/selection_panel.cpp`, `src/ui/selection_card.cpp`. Deps: C.
  Satisfies: R1, R4.
- **[1] J — Visual check.** Files: `scripts/verify/chart_question_log.lua`. Deps: H, I.
  Satisfies: R1, R3.

Parallelisation note: H ∥ I → J. Both are call-site edits over task A's helper.

## Corporation dashboard roll-ups (promoted from backlog.json § BL-248)

Requirements: requirements.json § corp-dashboard-rollups (R1–R5).

Goal: nav slot 1 becomes the real dashboard — Production / Trade / Workforce / Finance as
four roll-up cards on the fold model, each with a card-specific drill and a question log.
The exemplar's *interaction* transfers; its numbers do not.

- **[3] K — Data layer: the four roll-up summaries, derived from `world` +
  `economy_report`.** Files: `src/ui/corporation_dashboard.{hpp,cpp}`. Deps: A.
  Satisfies: R2.
- **[3] L — The four cards, their drills, and their question logs.**
  Files: `src/ui/corporation_dashboard.cpp`. Deps: K, A, H. Satisfies: R1, R3, R4.
- **[1] M — Wire slot 1; retire the duplicate all-corporations table.** The Economy
  panel's Corps view already carries it. Files: `src/core/app.cpp`,
  `src/ui/corporation_panel.{hpp,cpp}` (removed). Deps: L. Satisfies: R5.
- **[1] N — MENU.md § Slot 1.** Files: `docs/ui/MENU.md`. Deps: M. Satisfies: R5.
- **[2] O — Checks: headless roll-up arithmetic + the visual pass.**
  Files: `tools/verify/corp_dashboard_harness.cpp`, `scripts/verify/corp_dashboard.lua`.
  Deps: M. Satisfies: R1, R2, R3, R4.

Parallelisation note: K → L → M → {N ∥ O}. The chain is real — L draws what K derives.

---

## <Group name> (promoted from BACKLOG § <item>)

Requirements: requirements.json § <slug>

- **[<difficulty>] A — <action>.** Files: `<paths>`. Deps: foundation. Satisfies: R1, R2.
- **[<difficulty>] B — <action>.** Files: `<paths>`. Deps: A. Parallel-safe with C. Satisfies: R3.
- **[<difficulty>] C — <action>.** Files: `<paths>`. Deps: A. Parallel-safe with B. Satisfies: R4.

Parallelisation note: A → {B, C}; B ∥ C (disjoint files). Promote D once B and C
land; D depends on both.
```

Requirement records (the data + permanent history) live in
[`req/requirements.json`](req/requirements.json); the schema and workflow policy are in
[`req/REQUIREMENTS.md`](req/REQUIREMENTS.md).

## Definition of "complete"

A task is **complete** only when, for **every** requirement it satisfies, all three
of the following hold — completeness is measured against the requirements, not
against "the code is written":

- **Reviewed** — the change has been read back against each requirement it claims to
  satisfy, for correctness and for project conventions (naming, comments,
  determinism).
- **Implemented** — the change exists and the affected target builds clean.
- **Tested** — each requirement's **Verification** has actually been *run* and
  passed, not merely assumed. A requirement whose verification has no available
  skill or tool follows the verification-method policy in
  [`req/REQUIREMENTS.md`](req/REQUIREMENTS.md) (§ Verifying when no skill or tool
  exists) — until its method is run (or explicitly deferred and the requirement
  left `pending`), the task is **not** complete.

A task that is implemented and builds but whose requirements have not all been
reviewed and tested is *code-complete*, not complete. Only mark a group cleared
(and its item removed) when its requirements are complete by this definition,
or its remaining rows are explicitly accepted as out of scope. See also
[`../GLOSSARY.md`](../GLOSSARY.md) **Complete (task state)**.

## Cancelling a task group

REFINED.md is a **working state**: a group is meant to be driven to *complete* (see
above) in **one working block**. A group that cannot be — blocked, out of time, or
superseded — is **cancelled** rather than left half-tracked. Cancelling a group:

1. **Marks its requirements `failed`** in [`req/requirements.json`](req/requirements.json)
   (with the reason in `notes`), so the failed attempt is on record. Rows genuinely met
   before the block stalled keep their real status. The group `status` is then flipped to
   `"cancelled"` with a `resolution` recording the cancellation — the record is never
   deleted. Re-promoting flips it back to `"active"`.
2. **Rewrites the group's task intent back into the backlog** (a new or existing item in
   [`backlog.json`](backlog.json) / [`BACKLOG.md`](BACKLOG.md)) as described intent, **merging
   into a related existing item** where one exists rather than duplicating.
3. **Removes the task stubs** (the A–F entries) from this file.

Cancelling reverts *tracking*, not committed code — code already landed stays in the
tree; its intent simply returns to the backlog to be re-promoted later. A group is
thus always in one of two terminal states: **completed**, or **cancelled** back to
the backlog. See also [`../GLOSSARY.md`](../GLOSSARY.md) **Cancelled (task state)**.

## Pausing a task group (deliberate handoff)

Driving a group to *complete* in one block is the default, **not** a mandate (see CLAUDE.md
§ Proportionality and session boundaries). When ending a session early serves the work — the
batch is large, context is drifting, or a natural checkpoint is reached — **pause** the group
rather than force completion or cancel it. A paused group is a deliberate scoping choice,
distinct from a *cancelled* one (which reverts intent to the backlog): the tasks stay in this file,
ready for the next session to resume.

Pausing is only legitimate if the stop is **clean and resumable**:

1. **REFINED.md is true to state** — completed tasks marked done, the in-flight task marked as
   the resume point, untouched tasks left as-is. No silent half-edits.
2. **The build is green, or the breakage is noted** — if the tree does not build, say exactly
   why and what the next session must finish to green it.
3. **A one-line handoff** records where to resume ("resume here: D — wire the panel into
   `app.cpp`; B/C landed and verified").

A paused group is therefore *not* a terminal state — it is an explicit, recorded intermission.
The barrier semantics for a multi-item set still hold *within* a session; pausing is how a
session boundary is drawn *between* them.

---

## Dividing work across agents & authoring tasks

This is the method used to promote an item and (optionally) fan it out to
parallel sub-agents. It is descriptive of how the v0.0.3 Environment groups were
run; follow it when promoting future work.

> **Note (2026-06-16):** the **authoritative** sub-agent model is now
> [`DELIVERY.md`](DELIVERY.md) § Sub-agents & worktrees — **worktrees are the primary isolation
> mechanism** and agents build/commit on their own branch. The decomposition and
> file-mapping guidance below still holds (it is how you carve focused agents); where this
> section's older "disjoint write-sets / sub-agents don't commit" phrasing conflicts with the
> worktree model, DELIVERY.md wins.

### 1. Decompose, then map every task to its files

Break the intent into the **smallest independently-buildable steps**, foundation
first. For each step, write down the **exact files it will write** — not roughly,
literally. This file list is the single most important field: it is what makes
collisions visible and is the input to every parallelisation decision below. A
task whose file scope you cannot name yet is not ready to promote; it needs more
design first.

Build the **collision map** before deciding anything about agents: list each file
and the tasks that touch it. Hotspot files (touched by many tasks) and shared
headers/data-model files are where parallelism dies — see them early.

### 2. The rule for what can run concurrently

Two tasks may run as **concurrent agents only if their file write-sets are
disjoint.** No exceptions — two agents writing the same file race and corrupt
each other's edits. Consequences that follow directly from this rule:

- **Passes inside one generator do not parallelise.** This codebase keeps all
  passes of a generator in one `.cpp` (e.g. `tile_generation.cpp` holds six
  passes). They share a file, so they are sequential. Do **not** split a file
  into per-pass translation units just to win parallelism — fighting the
  codebase's structure costs more than it saves. Concurrency lives **across**
  groups, not within a generator.
- **Compile/data dependencies gate whole groups.** If group B reads a type or
  data that group A produces (corporations need the nation component to exist and
  nations to be populated), B cannot start until A has landed — regardless of
  whether their files happen to be disjoint.
- **Prefer whole-pipeline-per-agent over pass-per-agent.** When a group's passes
  share a file, give the *whole group* to one agent that works sequentially
  inside it, rather than trying to slice it.

### 3. Adjust the design to *create* disjointness (cheaply)

Small, principled data-model choices can turn a collision into two disjoint
scopes. Example from v0.0.3: storing tile→nation ownership in a `world` map
(`tile_to_nation`) instead of adding a field to `tile_component` kept the nation
group off `tile_generation.{hpp,cpp}`, so it could run concurrently with the tile
tuning group. Make these moves when they are also *good design*; never contort the
model purely for parallelism.

### 4. Keep hotspot files and integration in the main session

The file every group eventually touches (here, `hard_coded_world.cpp` — the
campaign wiring) is **never given to a sub-agent.** The integrating (main) session
owns it, wires each group's entry point as that group lands, runs the build, and
verifies. **Sub-agents do not build or commit** — they write code on a disjoint
scope and report their public signature + the one-line hook the integrator should
add. This keeps the one shared file single-writer and makes integration a small,
deliberate step rather than a merge.

### 5. Author a task so a cold agent can execute it

A sub-agent starts with **no memory of the planning conversation.** A task handed
to one must therefore carry, in its own text: the authoritative design doc to
read, the exact file scope (and an explicit "do not touch X" for the hotspots),
the project conventions (naming, comments, determinism), the entry-point/signature
to expose, the "do not build/commit" instruction, and the report-back shape.
Known footguns belong in the prompt too (e.g. the member-name-shadows-enum-type
qualification trap). If the task can't be executed from its text alone, it is
under-specified.

### 6. Run order

Group concurrent agents into **waves** of disjoint scopes; run dependent groups in
later waves. After each wave the integrator wires hooks, **builds, and verifies**
(headless harness for pure `world/*` logic per `reference_headless_build`; a full
`cmake` build to confirm the real target still links) before starting the next
wave. Verify retroactively — do not assume an agent's self-reported success.

---
