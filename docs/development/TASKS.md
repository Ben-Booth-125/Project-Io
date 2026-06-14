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

## Corporation lens (promoted from TODO § Canvas [4])

Requirements: [REQUIREMENTS.md § corporation-lens](req/REQUIREMENTS.md#corporation-lens)

Prerequisite: TODO § Canvas [3] "Design the lens system" should produce
`docs/ui/LENSES.md` before task D starts. Task A writes the corporation-specific
section regardless — stub the other lens entries if the full doc has not yet landed.

- **[2] A — Settle corporation-lens semantics and write the corporation section of
  `docs/ui/LENSES.md`.** Record these decisions: (a) a "corporate-owned tile" is any
  tile on which a corporation holds a building (`building_component.tile` lookup via
  `w.corporations[].assets`) — no influence radius for this iteration; (b) the lens
  is Planetary-only; (c) the player corp uses `presentation::faction_colour(0)`,
  rivals use the per-corp hash already used for building markers; (d) unowned tiles
  show terrain colour with no tint. Create `docs/ui/LENSES.md` if absent; stub the
  other four lens sections. Files: `docs/ui/LENSES.md`. Deps: foundation.
  Satisfies: R8, R9.

- **[2] B — Add the `corporation` glyph to `icons.{hpp,cpp}`.** Shape: a filled
  square with a centred inner dot (a "seal" silhouette, distinct from the
  extraction-site filled diamond and the port/unit filled triangle — verify against
  ICONS.md § Open clarifications before drawing). Follow the shared `(dl, centre, r,
  colour)` contract. Files: `src/ui/icons.{hpp,cpp}`. Deps: A. Parallel-safe with C.
  Satisfies: R2.

- **[1] C — Add `corporation` to `overlay_mode` and wire its strip button in
  `overlay.cpp`.** If B has not yet landed, use `icons::placeholder` for the button
  and annotate with a `// TODO: swap to icons::corporation` comment — resolved at
  integration. Files: `src/ui/ui_state.hpp`, `src/ui/overlay.cpp`. Deps: A.
  Parallel-safe with B. Satisfies: R1, R3.

- **[3] D — Implement the corporation lens render pass in `body_surface_canvas.cpp`.**
  Build a `tile_id → entity_id (corporation)` lookup from `w.corporations` and their
  `assets` → `building_component.tile` at draw time (acceptable cost at prototype
  tile counts). Under `overlay_mode::corporation`: tint owned tiles with their
  corporation's colour; give player-corp tiles a thin border in
  `presentation::faction_colour(0)`; render unowned tiles in terrain colour. Guard
  the entire pass behind the `overlay_mode::corporation` branch — no render change on
  Solar or Circumplanetary. Files: `src/ui/body_surface_canvas.cpp`. Deps: B, C.
  Satisfies: R4, R5, R6, R7.

Parallelisation note: A → {B, C} → D. B ∥ C (disjoint file scopes: icons vs.
ui_state/overlay). D integrates both and is the canvas hotspot — run in the main
session. If C runs as a sub-agent, it should leave a comment stub for the icon and
let the main session swap it in at integration.

---

*The four earlier Canvas groups (Circumplanetary max zoom, Map lens icons,
Political-layer render, Faction-lens default) are complete — see the 2026-06-14
DEVLOG entry "Layer 3 foundations" and the updated TODO.md § Canvas.*
