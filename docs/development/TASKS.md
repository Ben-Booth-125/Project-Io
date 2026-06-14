# Project Io — TASKS

The **active, prioritised, actionable worklist**. Unlike [`TODO.md`](TODO.md)
(described intent), every entry here is a concrete, file-scoped,
individually-buildable step ready to execute. Tasks are **promoted** from a
TODO.md item (see TODO.md § TODO vs. TASKS) and cleared as they complete — this
file is transient and is expected to be empty between work blocks.

## Task format

List tasks in **execution order**, grouped by the TODO item they were promoted
from. Each task carries:

- **A group-scoped ID letter** (A, B, C, …), so dependencies and parallel pairs
  can be named.
- **A difficulty** in brackets (the TODO.md 1–6 scale).
- **A one-line action** — imperative; what to change.
- **File scope** — the files the task is expected to touch. This is what makes
  collisions between tasks visible.
- **Dependencies** — which sibling tasks must land first (or "foundation" /
  "independent root").
- **Parallelisation** — whether it can run concurrently with a sibling, and
  whether as a sub-agent. Only true when the file scopes are **disjoint**.

End each group with a **parallelisation note**: the dependency shape and which
roots are safe to fan out. Run concurrent tasks only when their file scopes do
not overlap; keep same-file tasks sequential. Spawn a sub-agent for a parallel
branch only when it is genuinely disjoint and self-contained, and have the
integrating session run the build — sub-agents should not build or commit.

### Template

```
## <Group name> (promoted from TODO § <item>)

Requirements: [REQUIREMENTS.md § <slug>](req/REQUIREMENTS.md#<slug>)

- **[<difficulty>] A — <action>.** Files: `<paths>`. Deps: foundation. Satisfies: R1, R2.
- **[<difficulty>] B — <action>.** Files: `<paths>`. Deps: A. Parallel-safe with C. Satisfies: R3.
- **[<difficulty>] C — <action>.** Files: `<paths>`. Deps: A. Parallel-safe with B. Satisfies: R4.

Parallelisation note: A → {B, C}; B ∥ C (disjoint files). Promote D once B and C
land; D depends on both.
```

Requirements policy and table format are in [`req/REQUIREMENTS.md`](req/REQUIREMENTS.md).

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
(and its TODO item removed) when its requirements are complete by this definition,
or its remaining rows are explicitly accepted as out of scope. See also
[`../GLOSSARY.md`](../GLOSSARY.md) **Complete (task state)**.

## Cancelling a task group

TASKS.md is a **working state**: a group is meant to be driven to *complete* (see
above) in **one working block**. A group that cannot be — blocked, out of time, or
superseded — is **cancelled** rather than left half-tracked. Cancelling a group:

1. **Marks its requirements `failed`** in [`req/REQUIREMENTS.md`](req/REQUIREMENTS.md)
   (with the reason in Notes), so the failed attempt is on record. Rows genuinely met
   before the block stalled keep their real status. The section is then **moved to that
   file's Completed / cancelled archive** with a `Resolved:` line recording the
   cancellation — it is never deleted. Re-promoting copies it back to Active.
2. **Rewrites the group's task intent back into [`TODO.md`](TODO.md)** as described
   intent, **merging into a related existing TODO item** where one exists rather than
   duplicating.
3. **Removes the task stubs** (the A–F entries) from this file.

Cancelling reverts *tracking*, not committed code — code already landed stays in the
tree; its intent simply returns to the backlog to be re-promoted later. A group is
thus always in one of two terminal states: **completed**, or **cancelled** back to
TODO. See also [`../GLOSSARY.md`](../GLOSSARY.md) **Cancelled (task state)**.

---

## Dividing work across agents & authoring tasks

This is the method used to promote a TODO item and (optionally) fan it out to
parallel sub-agents. It is descriptive of how the v0.0.3 Environment groups were
run; follow it when promoting future work.

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

*No active groups. The worklist is empty between work blocks.*

Completed 2026-06-14 (see DEVLOG, newest first):

- *Publish block — Selection info element + Known Bug (six groups, barrier set).
  **Completed:** Go-to planetary landing + Kepler-only reliability (R1–R5;
  `focus_on_entity` body → `focus_on_surface`, tile → no-op; `verify.go_to` +
  `selection_go_to.lua`); Generation Ledger design (R1–R4; `GENERATION_LEDGER.md`).
  **Cancelled back to TODO:** Non-spatial 'go to' routing (blocked — no nation/corp
  ledgers); Canvas hit-testing (blocked — entities not drawn as selectable markers);
  Frame stutter measurement (R1/R2 failed — verification needs live instrumentation);
  Body labels stepping (R1 complete, R2 failed — root cause confirmed, fix +
  temporal verification deferred). See REQUIREMENTS.md archive.*

- *Visual-verification harness — Phase 2: shared `canvas_command` vocabulary
  (keyboard + verify API), `verify.center_tile`/`command`/`buildings`,
  `scripts/verify/lib.lua` helpers, `corporation_lens.lua` refactored, and the
  `verifier-visual` skill. All V7–V12 complete.*
- *Visual-verification harness — Phase 1: headless `--verify` capture mode, PNG
  writer, `verify` Lua API. All V1–V6 complete.*
- *Corporation lens: re-verified with the new harness — R2–R6 confirmed
  `complete`; the cancelled group is now closed.*

Remaining harness follow-up (golden-image diffing) lives in [TODO.md](TODO.md) § Canvas.
