# Project Io — REFINED (active worklist)

> Drained 2026-08-12: BL-367 (multi-building management surface), 1 task COMPLETE (light-weight
> enough for one direct pass) — removed per the retain-one policy. Record lives in backlog.json
> BL-367's `resolution` field and requirements.json § multi-building-management-surface (R1–R5).
> construction_panel.cpp's building-detail view groups a tile's buildings by stack
> (`tile_stacks`/`draw_tile_stack_list`), mirroring `placement_rules::stack_members`'s own
> grouping; a single-building tile still routes straight to its detail. selection_panel.cpp's
> Manage button and body_surface_canvas.cpp's marker-click/fallback both now count
> buildings-per-tile instead of grabbing an arbitrary first match. On-canvas marker gained a
> "+N" count badge, staggered past the corp-identity tag. Docs: SELECTION.md, ICONS.md,
> question_log.json updated. ProjectIo builds clean, launches without crash; a live screenshot
> of a real multi-building tile is owed (NR-173), expected to surface naturally given BL-365's
> background firms.

> Drained 2026-08-11: BL-365 (background industry keystone), Sprint 10's keystone and the
> reason BL-130/BL-263/BL-368 were pulled in out of turn — 2 worktree tasks COMPLETE
> (T1 core swap, T2 docs) — removed per the retain-one policy. Record lives in backlog.json
> BL-365's `resolution` field and requirements.json § background-industry-keystone (R1–R7).
> Real background corporations (`corporation_component.is_background`) replace the abstract
> nation-substrate injection: `generate_background_firms` calibrates per body until real
> production reaches ~90% of real demand (measured, not authored), background firms run the
> FULL corp_ai scored-utility layer per Ben's explicit 2026-08-11 call, `nation_substrate` /
> `inject_substrate_demand` are deleted outright. `pregame_balance_harness 80` (real world)
> ALL PASS, plateau moved from ~185k cr to ~42k cr (real competitors now claim market share).
> `nation.resource_abundance` deliberately kept — independent of the substrate mechanism
> (NR-172). Three new NEEDS_REVIEW entries: NR-170 (pre-existing population_mvp failure,
> unrelated), NR-171 (data_creep_harness 1500-tick building growth from the larger corp
> population — not a leak, needs a longer-horizon rerun to confirm convergence), NR-172
> (resource_abundance keep decision). `ai_skill_harness` golden-band drift (NR-169) not
> re-blessed this session — left for a dedicated stewardship pass, now that the
> substrate-to-real-firms transition BL-365 itself anticipated as the likely trigger is done.

> Drained 2026-08-11: BL-130 (real market inventory), the last link in BL-365's blocker chain,
> 1 task COMPLETE — removed per the retain-one policy. Record lives in DEVLOG.md (2026-08-11
> entry), backlog.json BL-130's `resolution` field, and requirements.json
> § real-market-inventory (R1–R5). Found and fixed a real, pre-existing bug while diagnosing an
> unrelated crash: BL-368's three new resources were never registered in
> `recipe_registry.cpp`'s Lua name table, so the actual game had been crashing on startup since
> BL-368 landed earlier this session. Also fixed three existing harnesses whose fixtures assumed
> the retired unconditional-auto-buy model. BL-365 (the keystone) is now unblocked.

> Drained 2026-08-11: BL-263 (spontaneous market emergence), promoted out of turn to unblock
> BL-365 (blocked_on BL-130, which requires BL-263), 1 task COMPLETE — removed per the
> retain-one policy. Record lives in DEVLOG.md (2026-08-11 entry), backlog.json BL-263's
> `resolution` field, and requirements.json § spontaneous-market-emergence (R1–R5). Corrected an
> earlier stale-exe reading in NR-169 in passing: BL-368/BL-263 do not move `ai_skill_harness`'s
> golden bands beyond what BL-366 already did. BL-130 (real market inventory) is next — now the
> only thing standing between here and BL-365 itself.

> Drained 2026-08-11: BL-368 (real population demand + habitability tranche), Sprint 10's second
> foundation item, 1 task COMPLETE — removed per the retain-one policy. Record lives in DEVLOG.md
> (2026-08-11 entry), backlog.json BL-368's `resolution` field, and requirements.json
> § real-population-demand-habitability-tranche (R1–R5). Found and fixed a stale-doc issue in
> passing: the "known bug" BL-368's own design cited had already been fixed by BL-190
> (2026-07-31); MARKETS.md repeated the same stale claim, corrected this pass. BL-365 (the
> keystone) and BL-367/BL-130/BL-132/BL-369 stay `designed`, not yet promoted.

> Drained 2026-08-11: BL-366 (multi-building tile stack cap + urban transform), Sprint 10's
> first foundation item, 1 task COMPLETE — removed per the retain-one policy (light-weight enough
> to land in one direct pass rather than a multi-task promotion). Record lives in DEVLOG.md
> (2026-08-11 entry), backlog.json BL-366's `resolution` field, and requirements.json
> § multi-building-tile-urban-transform (R1–R5).

> Drained 2026-08-09: build-heavy v0.1.1 batch — render-precision audit (BL-215, 5 tasks
> COMPLETE) and selection always open (BL-266, 3 tasks COMPLETE; R4 golden re-bless pending,
> NR-104) — removed per the retain-one policy. Record lives in DEVLOG.md (2026-08-09 entry),
> backlog.json BL-215/BL-266 `resolution` fields, and requirements.json
> § text-wrap-render-audit (R1–R6) / § selection-always-open (R1–R4).

> Drained 2026-08-08: critique batch (BL-326/327/328/329/330), all 3 tasks COMPLETE — removed
> per the retain-one policy. Record lives in DEVLOG.md (2026-08-08 entry), backlog.json's five
> `resolution` fields, and requirements.json § critique-batch-ui-polish (R1–R6).
>
> Drained 2026-08-08: unit hire surface (BL-324), all 5 tasks COMPLETE — removed per the
> retain-one policy. Record lives in DEVLOG.md (2026-08-08 entry), backlog.json BL-324/BL-157
> `resolution` fields, and requirements.json § unit-hire-surface (R1–R7).
>
> Drained 2026-08-08: buildings rework first slice (BL-323 S1 partial + S3 + S4; S2/S2b were
> already landed pre-promotion), all 4 tasks COMPLETE — removed per the retain-one policy. Record
> lives in DEVLOG.md (2026-08-08 entry), backlog.json BL-323 `resolution` field, and
> requirements.json § buildings-rework-first-slice (R1–R7). **Not closed**: BL-323's full S1
> (processing chains needing new resource_type values) stays `designed`, next slice ready.

> Drained 2026-08-08: military base S1 (BL-325), all 4 tasks COMPLETE — removed per the
> retain-one policy. Record lives in DEVLOG.md (2026-08-08 entry), backlog.json BL-325's design
> field (§ S1 landed), and requirements.json § military-base-s1 (R1–R5). **Not closed**: BL-325's
> S2 (hire moves onto the base) and S3 (out-of-supply decay) stay `designed`, next slices ready.

> Drained 2026-08-10: hygiene batch (BL-351 sell-order over-commit, BL-352 hire-gate live store,
> BL-353 persona eval guard, BL-354 orbital tick purity, BL-355 warning sweep, BL-356 body→market
> index, BL-357 pop-growth aggregate, BL-358 determinism sweep, BL-359 deferred demolish, BL-360
> hot-path scans) — all seven agent slices COMPLETE, merged, harness suite green — removed per the
> retain-one policy. Record lives in DEVLOG.md (2026-08-10 hygiene entry), the ten backlog.json
> `resolution` fields, and requirements.json § sell-order-pool-overcommit through
> § hot-path-spatial-scans. BL-361 (app.cpp split), BL-362 (UI frame caches), BL-363 (misc
> sweep) were filed, not delivered.

> Drained 2026-08-10: hygiene batch wave 2 (BL-361 app.cpp decomposition, BL-362 UI frame
> caches, BL-363 misc sweep) — all 6 agent slices COMPLETE, merged, build + 25 harnesses green,
> five visual checks verified against pre-wave-2 control goldens — removed per the retain-one
> policy. Record lives in DEVLOG.md (2026-08-10 wave-2 entry), the three backlog.json
> `resolution` fields, and requirements.json § app-cpp-decomposition /
> § ui-frame-recompute-caches / § misc-hygiene-sweep. The review barrier caught three real
> faults before close (NR-130/133/134 carry the residuals).

## Nation/corp generation visibility (promoted from BL-305) — **PAUSED, no tasks started**

**Resume here.** Paused 2026-08-08 before any code (see NR-085): task A's file scope
(`hard_coded_world.cpp`, `app.cpp`) exactly matches uncommitted, unreviewed work already in the
tree from another session (the New World wizard's async real-surface preview pane + the Era −1
sim's terrain-view adapter — see DEVLOG's 2026-08-08 audit-note entry). Resume once that work
has either landed (rebase onto it) or is confirmed gone/safe to build around — check `git status`
for `src/ui/generation_preview.{cpp,hpp}` and `src/world/sim_terrain_build.hpp` first.

Requirements: requirements.json § nation-corp-generation-visibility (R1–R5)

- **[2] A — Live territory-carve stage.** Extend BL-256's generation-screen globe with a stage
  that animates the Voronoi BFS carve as it runs, rather than only being inspectable after
  generation completes. Files: `src/world/hard_coded_world.cpp`, `src/core/app.cpp`. Deps:
  foundation. Satisfies: R2.
- **[2] B — Corp asset-placement overlay.** Render corp asset seeding spatially on the same
  generation-screen canvas/globe as A. Files: `src/world/hard_coded_world.cpp`, `src/core/app.cpp`.
  Deps: A (shares the generation-screen staging A introduces). Satisfies: R3.
- **[1] C — Corp financial-profile card.** Surface financial-profile derivation as a card/ledger
  entry (not a canvas overlay) alongside B. Files: `src/core/app.cpp`. Deps: A. Parallel-safe
  with B (disjoint UI regions once A's staging exists). Satisfies: R4.
- **[1] D — Coverage pass.** Walk GENERATION_STRATEGY.md's pass map; confirm every step now has
  a named visibility surface (BL-256, BL-303/304, BL-211, and A–C above), and add an explicit
  "invisible by design" note to any step that doesn't. Files: `docs/generation/GENERATION_STRATEGY.md`,
  `docs/generation/NATION_GENERATION.md`, `docs/generation/CORPORATION_GENERATION.md`. Deps: A, B, C
  (needs the finished surface list to audit against). Satisfies: R5.

Parallelisation note: A is the foundation (the staging both B and C hang off); B ∥ C once A
lands (disjoint UI concerns — canvas vs. card); D runs last, after the surfaces it's auditing
exist.

> Drained 2026-08-09: header chrome tightening (BL-312/BL-313) — the work landed in 9ecbbcf but
> the section was never flipped; items were closed retroactively by the NR-075 cut audit and the
> requirement group closed 2026-08-09 (verification overtaken by the closure). Record lives in
> req/requirements.json (§ header-chrome-tightening) and NR-075.

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

> Drained 2026-08-09: corp standing profile (BL-262 first slice, 4 tasks COMPLETE,
> standing_harness 41/41), Mediterranean rift sea (BL-276, R1–R4 met, 500-seed sweep), and
> Sprint 5 Era −1 foundation wave (BL-272 combat engine + BL-273 province demography, harnesses
> green) — removed per the retain-one policy. Records live in DEVLOG.md and
> req/requirements.json (§ corp-standing-profile, § mediterranean-rift-sea,
> § unit-doctrine-combat, § province-demography).

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

