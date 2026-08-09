# Project Io — REFINED (active worklist)

## Render-precision audit (promoted from BL-215) — **COMPLETE**

Requirements: requirements.json § text-wrap-render-audit (R1–R6)

- **[3] A — `src/ui/text_fit.{hpp,cpp}`: the shared fit/wrap module + overflow recorder;
  delete profile_panel.cpp's private helpers.** Files: `src/ui/text_fit.hpp`,
  `src/ui/text_fit.cpp`, `src/ui/profile_panel.cpp`. Deps: foundation. Satisfies: R1, R2.
- **[1] B — display floor: SDL_SetWindowMinimumSize scaled by UI scale; settings floor
  1280x720; DEVELOPMENT_PRACTICES § Display environment.** Files: `src/core/app.cpp`,
  `docs/development/DEVELOPMENT_PRACTICES.md`. Deps: independent. Satisfies: R3.
- **[3] C — charts.cpp measure-first rework (legend_width, measure_chart, legend_place,
  measured gutters, threshold ladder, two-line titles) + generation_charts wiring.**
  Files: `src/ui/charts.{hpp,cpp}`, `src/ui/generation_charts.cpp`. Deps: A. Satisfies: R4.
- **[2] D — remaining sites: tile_inspector question wrap, chat tab-strip line breaks,
  selection pager fit_width, header figures fit_text, canvas key measures.** Files:
  `src/ui/tile_inspector.cpp`, `src/ui/chat_panel.cpp`, `src/ui/selection_panel.cpp`,
  `src/ui/header_panel.cpp`, `src/ui/body_surface_canvas.cpp`. Deps: A. Satisfies: R6.
- **[2] E — verify bindings (clipping / expect_no_clipping), overflow-frame naming,
  `scripts/verify/text_overflow_floor.lua`, LAYOUT.md container amendments.** Files:
  `src/core/app.cpp`, `scripts/verify/text_overflow_floor.lua`, `docs/ui/LAYOUT.md`.
  Deps: A. Satisfies: R5, R6.

Parallelisation note: A is the foundation; B independent; C/D/E all need A's header.
Run in one session, sequential A → B → C → D → E (shared files app.cpp and charts).

## Header chrome tightening (promoted from BL-312, BL-313)

Requirements: requirements.json § header-chrome-tightening (R1–R5)

- **[2] A — Minimap flush to the right edge (and bottom, if it doesn't break the
  selection-band alignment).** Files: `src/core/app.cpp`. Deps: foundation.
  Satisfies: R1, R2.
- **[2] B — Time panel: two-column reflow (left 1/3 progress, right 2/3 controls).**
  Files: `src/core/app.cpp`. Deps: independent of A (disjoint code regions in the
  same file). Satisfies: R3, R4, R5.

Parallelisation note: both tasks touch `app.cpp` but disjoint blocks (the minimap
draw block vs the time-panel draw block) — one session, sequential, not fanned out
(too small to be worth worktree overhead).

## Tech tree radial canvas (promoted from BL-310) — **COMPLETE**

Requirements: requirements.json § tech-tree-radial-canvas (R1–R10, all met). Round 1: Era 0/1
gate quests render as a radial constellation (rings = graph depth or authored tier, sectors =
quests), keystones larger/gold, Era-1 branch pairs colour-differentiated with an "excludes"
mark, nav slot 4 wired. Round 2 (same session, Ben's live-playtest feedback): converted to a
full-canvas takeover (`ui::canvas_rect()`, BL-265's task 1, first consumer) with a drawn
top-left `‹` return control; NR-054 resolved — the canonical ancient ladder JSON (71 nodes, 5
keystones) now renders on the Antiquity tab as a muted read-only history; Standing lines
dropped from rendering, tab 3 relabelled "Era 2" (placeholder only, data stays in
`tech_tree.lua`). Pan tried left-click, reverted to middle-click for consistency. Round 3 (same session): era
tabs are now icon-only, bigger, each icon a real tiny render of that era's own nodes (not a
generic glyph); on-canvas labels switched from bare id to name/short_name (short_name
hand-authored for the Era-1 keystones + branches this pass, other ~120 nodes fall back to
truncated name). Verified via `scripts/verify/tech_tree_panel.lua` — 3/3 golden PASS (tabs,
era1, antiquity), goldens re-blessed against every intentional change across all three rounds.

## Corp standing profile (promoted from BL-262, first slice) — **COMPLETE**

Requirements: requirements.json § corp-standing-profile (R1–R5, all met bar R1's visual leg,
which is manual/Ben's). `standing_harness` 41/41 PASS (boundary determinism for all three bands,
zero-total/empty-map no-NaN cases, player-exact/rival-banded visibility split, byte-identical
re-computation). Full app + harness both build clean (MSVC 14.44, `build_app.bat`). Production
axis deliberately absent this slice — no honest visible-information source exists yet (BL-068
privates recipe/workforce); documented in `standing.hpp`, tracked as a follow-on.

- **[4] A — `src/world/standing.{hpp,cpp}` (new): compute the three-axis profile.**
  Files: `src/world/standing.hpp`, `src/world/standing.cpp`. Deps: foundation.
  Satisfies: R1, R2, R3, R4, R5.
- **[2] B — wire into `corporation_panel.cpp`: own row exact, rival rows banded, no total.**
  Files: `src/ui/corporation_panel.cpp`, `src/ui/corporation_panel.hpp`. Deps: A.
  Satisfies: R1.
- **[2] C — cache this-tick `corp_cash_flow` for the panel to read (no save-format change).**
  Files: `src/core/app.cpp`, `src/core/app.hpp`. Deps: A. Parallel-safe with B (disjoint files);
  both need A's signature first, so effectively sequential after A in one agent's pass.
  Satisfies: R3.
- **[2] D — `tools/verify/standing_harness.cpp` (new): boundary determinism + sorted walk.**
  Files: `tools/verify/standing_harness.cpp`. Deps: A. Satisfies: R2.

Parallelisation note: A is the foundation (new struct/function signatures everything else
calls); B/C/D all depend on A's header. Single coherent vertical slice on a small, mostly-new
file set — one agent, sequential A→{B,C,D}, not fanned out.

# Mediterranean rift sea (BL-276, 2026-08-03) — **COMPLETE**

Requirements: requirements.json § mediterranean-rift-sea (R1–R4 all met).
Rift-basin mechanism (interior-segment corridor, adaptive width, rift-shoulder rim) +
intracratonic sag fallback in `run_continents`; two-bar acceptance gate (arena ≥ 300 ×3
attempts, floor ≥ 30 ×6) in `hard_coded_world.cpp`; new asserted `mediterranean_sweep`
harness. Measured over 500 seeds: floor 100%, arena 89.6% (~90% per Ben's call, hard tail
kept). continents_harness, world_determinism, determinism_harness, world_audit all PASS.

---

# Sprint 5 — Era −1 history sim: foundation wave (2026-08-02) — **COMPLETE**

Both items landed as two parallel worktree-isolated agents (disjoint files, no collision), merged
into `doc-compression` then fast-forwarded into `main`. Full CTest suite 41/41 green post-merge
(also the first real build/test pass on the previously-uncommitted BL-218/219 settlement backend).

**Combat engine (BL-272) — COMPLETE.** Files: `src/world/combat.{hpp,cpp}` (new),
`src/world/components.hpp` (`unit_component.type`), `tools/verify/combat_harness.cpp` (new).
unit-doctrine-combat R1–R4 all met; `combat_harness` 15/15 PASS. `resolve_battle` is the shared
entry point for the future Era −1 sim and the existing 1960+ era path.

**Province demography (BL-273) — COMPLETE.** Files: `src/world/settlement.{hpp,cpp}` (extended),
`docs/economy/POPULATION.md`, `tools/verify/demography_harness.cpp` (new). province-demography
R1–R4 all met; `demography_harness` 20/20 PASS; `settlement_harness` re-verified clean (21/21)
against the extended `province` struct.

**Next:** wave 2 (BL-274, era-keyed unit rosters) needs BL-272 landed in `main` — now true — and
is ready to promote. Wave 3 (BL-271, the sim loop) needs all three; not promoted yet.

---

> Drained 2026-08-02: BL-270 (action dictionary) and BL-268 (planetary canvas cull + cache),
> both COMPLETE, removed per the retain-one policy — their record lives in DEVLOG.md and
> req/requirements.json (§ action-dictionary, § planetary-pan-perf).

---

> Drained 2026-08-02: Sprint 2 (BL-210 oral-history pivot: BL-217 checkpoint/branch model, BL-208
> world history log, BL-218 nations settlement rewrite, BL-219 corporations history rewrite — all
> four `complete` in backlog.json, BL-218/219's code landed via commit 0b9351d) and Sprint 1
> (procgen v1's food cluster — BL-166/168/170, all complete) removed per the retain-one policy —
> record lives in DEVLOG.md, backlog.json `resolution` fields, and requirements.json
> § checkpoint-branch-model / § world-history-log / § settlement-history-rewrite /
> § hydroponics-bay / § fishing-wharf / § river-generation.

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

> Drained 2026-08-02: the disclosure spine (2026-08-01 — BL-214/BL-247/BL-248, all
> complete) removed per the retain-one policy — record lives in DEVLOG.md,
> backlog.json BL-214/BL-247/BL-248 `resolution`, and requirements.json
> § drill-through-fold / § chart-question-log / § corp-dashboard-rollups. Authority:
> `docs/ui/LAYOUT.md` § Drill-through, `docs/ui/MENU.md` § Slot 1. The visual-golden
> staleness it surfaced is filed separately as BL-259.

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

