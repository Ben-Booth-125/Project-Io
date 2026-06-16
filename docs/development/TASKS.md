# Project Io — TASKS

The **active, prioritised, actionable worklist**. Unlike [`OPENS.md`](OPENS.md)
(described intent), every entry here is a concrete, file-scoped,
individually-buildable step ready to execute. Tasks are **promoted** from a
designed (`✓`) Brief (see OPENS.md § OPENS vs. TASKS) and cleared as they complete — this
file is transient and is expected to be empty between work blocks.

> **Proportionality (see CLAUDE.md § Proportionality and session boundaries).** Promoting a
> Brief into this file is for *substantial* work. A quick low-risk high-value change — a
> one-file fix, an obvious cleanup, a cheap optimisation — does **not** need a task group or a
> REQUIREMENTS table: make and verify it directly, then commit. Reach for the full lifecycle
> only where its coordination cost pays back; applying it to trivial work is over-engineering.

## Task format

List tasks in **execution order**, grouped by the Brief they were promoted
from. Each task carries:

- **A group-scoped ID letter** (A, B, C, …), so dependencies and parallel pairs
  can be named.
- **A difficulty** in brackets (the OPENS.md 1–5 time scale; tasks carry difficulty
  only — priority is an OPENS-level triage concept, not a per-task field).
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
## <Group name> (promoted from OPENS § <Brief>)

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
(and its Brief removed) when its requirements are complete by this definition,
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
2. **Rewrites the group's task intent back into [`OPENS.md`](OPENS.md)** as described
   intent, **merging into a related existing Brief** where one exists rather than
   duplicating.
3. **Removes the task stubs** (the A–F entries) from this file.

Cancelling reverts *tracking*, not committed code — code already landed stays in the
tree; its intent simply returns to the backlog to be re-promoted later. A group is
thus always in one of two terminal states: **completed**, or **cancelled** back to
OPENS. See also [`../GLOSSARY.md`](../GLOSSARY.md) **Cancelled (task state)**.

## Pausing a task group (deliberate handoff)

Driving a group to *complete* in one block is the default, **not** a mandate (see CLAUDE.md
§ Proportionality and session boundaries). When ending a session early serves the work — the
batch is large, context is drifting, or a natural checkpoint is reached — **pause** the group
rather than force completion or cancel it. A paused group is a deliberate scoping choice,
distinct from a *cancelled* one (which reverts intent to OPENS): the tasks stay in this file,
ready for the next session to resume.

Pausing is only legitimate if the stop is **clean and resumable**:

1. **TASKS.md is true to state** — completed tasks marked done, the in-flight task marked as
   the resume point, untouched tasks left as-is. No silent half-edits.
2. **The build is green, or the breakage is noted** — if the tree does not build, say exactly
   why and what the next session must finish to green it.
3. **A one-line handoff** records where to resume ("resume here: D — wire the panel into
   `app.cpp`; B/C landed and verified").

A paused group is therefore *not* a terminal state — it is an explicit, recorded intermission.
The barrier semantics for a multi-Brief set still hold *within* a session; pausing is how a
session boundary is drawn *between* them.

---

## Dividing work across agents & authoring tasks

This is the method used to promote a Brief and (optionally) fan it out to
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

## Archived — Layer 3 economy publish set

The eight-Brief Layer 3 economy set has been **published** (all groups complete) and
cleared from the active worklist. Its requirement tables are archived in
[`req/REQUIREMENTS.md`](req/REQUIREMENTS.md) and the session is recorded in the DEVLOG
(2026-06-14 — Layer 3 economy published). The detailed group breakdown below is retained
here for one cycle as the record of how the set was decomposed and executed; it is not an
active worklist.

<details>
<summary>Completed group breakdown (retained for reference)</summary>

### Group 1 — economy-data-model (promoted from TODO § Resources, Brief 1)

Requirements: [REQUIREMENTS.md § economy-data-model](req/REQUIREMENTS.md#economy-data-model)

- **[3] A — Extend the component structs.** Add to `building_component` an extraction
  `target_resource` (`resource_type`) and a processing `recipe` id (`uint16_t`, with a
  `no_recipe` sentinel constant); add a reserved `resource_remaining` array (unused in L3)
  to `tile_component`; add a running `balance` (`float`) to `corporation_component`. Files:
  `src/world/components.hpp`. Deps: foundation. Satisfies: R1, R2, R3.
- **[3] B — Add the (corp, body) stockpile pool to `world`.** A `std::map<std::pair<entity_id,
  entity_id>, stockpile_component>` (deterministic ordering, `tile_to_nation` pattern) keyed
  by (corporation, body), with an inline `pool_for(corp, body)` accessor that lazily inserts.
  Files: `src/world/world.hpp`. Deps: A. Satisfies: R4.

Parallelisation note: A → B (same translation-unit family, sequential). Foundation for
every other group; must land and build before Groups 3–6 integrate.

### Group 2 — recipe-registry (promoted from TODO § Resources, Brief 2)

Requirements: [REQUIREMENTS.md § recipe-registry](req/REQUIREMENTS.md#recipe-registry)

- **[3] A — Author the Lua data layer.** `scripts/recipes.lua` (processing recipes:
  `{ inputs = {res=qty}, outputs = {res=qty} }`, multi-in/out + reagents) and
  `scripts/economy.lua` (per-building-type constants: `base_rate`, `maintenance`,
  `base_wage`, `build_cost`, plus global `t_full` / `t_idle` thresholds). Legible round
  defaults. Files: `scripts/recipes.lua`, `scripts/economy.lua`. Deps: foundation.
  Satisfies: R1, R5.
- **[3] B — Build the C++ registry.** `recipe_registry` loads both scripts via sol2
  (protected calls only) into indexed C++ tables: a `recipe` struct (input/output arrays
  indexed by `resource_type`) addressed by the `recipe` id stored on `building_component`,
  and an economy-constants lookup by `building_type`. Resource names in Lua map to
  `resource_type` by a name table. Files: `src/world/recipe_registry.{hpp,cpp}`. Deps: A.
  Satisfies: R2, R3, R4.

Parallelisation note: A → B. File-disjoint from Group 1, but shares the `recipe`-id
convention defined in Group 1.A (`uint16_t`, `no_recipe` sentinel) — keep the two in
agreement. Depended on by Groups 3 and 5.

### Group 3 — production-simulation (promoted from TODO § Resources, Brief 3)

Requirements: [REQUIREMENTS.md § production-simulation](req/REQUIREMENTS.md#production-simulation)

- **[4] A — Extraction + processing + workforce step.** `run_economy_step(world&, const
  recipe_registry&)` iterates each corporation's `assets`; for **extraction_site** credits
  the (corp, body) pool with `target_resource` at `base_rate × deposit_richness ×
  workforce × (1 − hazard)` (no depletion); for **processing_facility** runs its `recipe`
  pool-first with the two-threshold partial-run model (`t_full`/`t_idle`), accruing outputs
  and recording per-building idle/active + limiting-input state for the panel. Deposits do
  not deplete (`resource_remaining` untouched). Files: `src/world/economy_system.{hpp,cpp}`.
  Deps: Group 1, Group 2. Satisfies: R1, R2, R3, R4, R5.

Parallelisation note: single sequential group (one new file pair). Reads the registry
(Group 2) and the pool/fields (Group 1). Input shortfalls are surfaced for Group 4 to
auto-buy; the balance arithmetic is Group 5's.

### Group 4 — market-clearing (promoted from TODO § Trade, Brief 4)

Requirements: [REQUIREMENTS.md § market-clearing](req/REQUIREMENTS.md#market-clearing)

- **[3] A — Per-body market clearing.** `clear_markets(world&, ...)` at the econ tick: per
  body market, **supply** = each corp's surplus listed for sale (pool above its processors'
  needs this tick); **demand** = processor input shortfalls auto-bought; transactions clear
  at `base_price` (`price` stays seeded at `base_price`). Returns the per-(corp) buy/sell
  cash-flow transactions for Group 5. Includes a no-op framework hook for player-driven sell
  orders. Files: `src/world/market_clearing.{hpp,cpp}`. Deps: Group 1, Group 3. Satisfies:
  R1, R2, R3, R4.

Parallelisation note: single sequential group. Consumes Group 3's shortfall/surplus
figures; emits transactions consumed by Group 5.

### Group 5 — budget-system (promoted from TODO § Budget, Brief 5)

Requirements: [REQUIREMENTS.md § budget-system](req/REQUIREMENTS.md#budget-system)

- **[3] A — Corporate money loop.** `apply_budget(world&, const recipe_registry&, const
  market transactions)` moves each `corporation_component.balance`: income = goods sold ×
  `base_price`; expenditure = input purchases × `base_price` + per-building `maintenance` +
  wages (`workforce × base_wage`). Balance may go negative. Files:
  `src/world/budget_system.{hpp,cpp}`. Deps: Group 2, Group 4. Satisfies: R1, R2, R3.

Parallelisation note: single sequential group. Reads Group 4 transactions + Group 2
constants; writes `balance` (the field Group 1.A added).

### Group 6 — economy-panel (promoted from TODO § Ledger, Brief 6)

Requirements: [REQUIREMENTS.md § economy-panel](req/REQUIREMENTS.md#economy-panel)

- **[3] A — The observability panel.** `draw_economy_panel(world&, ...)`: per (corp, body)
  pool quantities; each building's current output rate + idle/active (+ limiting input for
  processors); body market supply/demand; per-corp balance (negative flagged red). Read-only.
  Built on `presentation.hpp` / `format.hpp` / `icons.hpp`. Files:
  `src/ui/economy_panel.{hpp,cpp}`. Deps: Groups 1, 3, 4, 5. Satisfies: R1, R2, R3, R4.

Parallelisation note: single sequential group; last to integrate. Main session wires it
into the shell (`app.cpp` / `nav_pane.cpp`).

### Group S1 — placement-rules-audit (promoted from TODO § Infrastructure, Brief S1)

Requirements: [REQUIREMENTS.md § placement-rules-audit](req/REQUIREMENTS.md#placement-rules-audit)

- **[3] A — Audit & fix the placement guard.** Audit `corporation_generation.cpp` Pass 3
  (`place_starting_asset`) against `docs/economy/PRODUCTION.md` placement rules: extraction
  only on tiles with a non-zero deposit of the target type (or valid terrain), never on
  `ocean`. Produce a findings list; fix cheap gaps (e.g. extraction tiles with zero deposit
  of any target); promote larger gaps. Files: `src/world/corporation_generation.cpp`. Deps:
  independent (but coordinates with the integration authoring `target_resource` on placed
  assets — same file, main session). Satisfies: R1, R2.

Parallelisation note: independent root; same file as the field-authoring integration, so
done in the same main-session pass as that wiring.

### Group S2 — kepler-biome-balance (promoted from TODO § Environment, Brief S2)

Requirements: difficulty 2 — inline. **Verification:** `headless` (regenerate Kepler and
report forest+wetland tile fraction; target ≳ 3% combined, up from ~1.5%).

- **[2] A — Widen the habitable belt.** Lower Pass 2 equatorial ocean `bias_amp` and/or
  decouple the volcanic/forest belts from the wettest equatorial rows in
  `tile_generation.cpp`, so forest+wetland are less starved. Files:
  `src/world/tile_generation.cpp`. Deps: independent root.

Parallelisation note: independent root; disjoint from every other group.

---

### Set-wide parallelisation note (Publish step 3 — collision map)

| File | Groups that write it |
|------|----------------------|
| `src/world/components.hpp` | 1 |
| `src/world/world.hpp` | 1 |
| `scripts/recipes.lua`, `scripts/economy.lua` | 2 |
| `src/world/recipe_registry.{hpp,cpp}` | 2 |
| `src/world/economy_system.{hpp,cpp}` | 3 |
| `src/world/market_clearing.{hpp,cpp}` | 4 |
| `src/world/budget_system.{hpp,cpp}` | 5 |
| `src/ui/economy_panel.{hpp,cpp}` | 6 |
| `src/world/corporation_generation.cpp` | S1 + integration |
| `src/world/tile_generation.cpp` | S2 |
| `src/world/hard_coded_world.cpp`, `src/core/app.{hpp,cpp}` | integration (main session only) |

**Write-sets are disjoint** across all eight groups (each owns a new file pair or one
generator), so they are nominally parallel-safe. **Compile/data dependencies** force the
order regardless: 1 → 2 → 3 → 4 → 5 → 6, with S1 and S2 independent. Integration hotspots
(`hard_coded_world.cpp`, `app.cpp`, `corporation_generation.cpp` field authoring) stay in
the main session. Per session policy no sub-agents are spawned, so the whole set runs
sequentially in dependency order; the map confirms no same-file races when wiring.

</details>

---

## Archived — Layer 3 finalisation publish set

The five-Brief set has been **published** (all groups complete) and cleared from the
active worklist. Requirement tables are archived in
[REQUIREMENTS.md](req/REQUIREMENTS.md) and the session is recorded in the DEVLOG
(2026-06-15). The breakdown below is retained for one cycle as the record of how the
set was decomposed; it is not an active worklist.

<details>
<summary>Completed group breakdown (retained for reference)</summary>

Five Briefs pulled to finalise the production economy (price resolution, deposit
depletion, pre-game warm start, the player balance header, and the deferred-ledger
design principle). Run as a **barrier set** (TODO § Publishing multiple Briefs together):
all groups clear each step before any advances. Kept in the main session, sequential —
A/B share the `econ_harness` seam and C/D co-evolve through `app.cpp`.

### Group A — price-resolution (promoted from TODO § Trade)

Requirements: [REQUIREMENTS.md § price-resolution](req/REQUIREMENTS.md#price-resolution)

- **[3] A — Resolve price from supply/demand and clear at it.** In `clear_markets`,
  after supply and demand are accumulated, set each market `price[r]` toward
  `base_price[r] × sqrt(demand/supply)`, clamped to `[0.25×, 4×] base_price` and eased
  by an EMA (smoothing 0.5) from the prior `price[r]`; value this tick's sales and
  purchases (the returned `corp_cash_flow`) at the **resolved** price rather than
  `base_price`. Handle the zero-supply / zero-demand edges via the clamp. Update the
  one-line "valued at base_price" comments in `budget_system.cpp` /
  `market_clearing.hpp` to "resolved price". Files: `src/world/market_clearing.{hpp,cpp}`,
  `src/world/budget_system.cpp` (comment only). Deps: foundation (Layer 3 already landed).
  Satisfies: R1, R2, R3, R4.

Parallelisation note: single sequential group. Disjoint from B/D source; shares the
`tools/verify/econ_harness.cpp` verification file with B (main session edits both).

### Group B — deposit-depletion (promoted from TODO § Environment)

Requirements: [REQUIREMENTS.md § deposit-depletion](req/REQUIREMENTS.md#deposit-depletion)

- **[4] A — Seed reserves, draw them down with taper, report exhaustion.** Seed
  `tile_component.resource_remaining[r] = resource_deposit[r] × deposit_reserve_factor`
  (a hard-coded constant; richness stays the rate multiplier) in `tile_generation.cpp`
  Pass 6. In `run_extraction`, draw the credited output from `resource_remaining`,
  tapering output over the last `deposit_taper_ticks` of nominal yield and reporting
  the building **exhausted** ("out of resources", a state distinct from idle) once the
  reserve falls below `deposit_min_taper` of nominal. Add `bool exhausted` to
  `building_report`. Surface the exhausted state in the economy panel's State column.
  Finite only — no refill. Files: `src/world/economy_system.{hpp,cpp}`,
  `src/world/tile_generation.cpp`, `src/ui/economy_panel.cpp`. Deps: foundation.
  Satisfies: R1, R2, R3, R4, R5.

Parallelisation note: single sequential group. Source-disjoint from A/D; shares
`econ_harness.cpp` with A (main session).

### Group C — pregame-ticks (promoted from TODO § Corporation generation)

Requirements: difficulty 2 — inline. **Verification:** `headless` (running the economy
pipeline twice from a cold world yields non-empty pools / moved balances) + `build`.

- **[2] A — Prime two economy ticks at startup.** After `load_economy()` in `app::run()`
  (not `run_verify`, which stays deterministic-cold), run `step_economy()` twice so the
  first on-screen frame shows warm pools, moved balances, and populated market
  supply/demand. Files: `src/core/app.cpp`. Deps: foundation. Parallel-safe in scope
  with D except both touch `app.cpp` → sequential, same session.

Parallelisation note: shares `app.cpp` with D; done in the same main-session pass.

### Group D — balance-header (promoted from TODO § Canvas)

Requirements: [REQUIREMENTS.md § balance-header](req/REQUIREMENTS.md#balance-header)

- **[3] A — Surface the player balance, valuation, net, and trend in the header.**
  Re-signature `draw_header_panel` to take the `world`, the player balance history, and
  the strip bounds; render the player corporation's running **balance**
  (`corporation_component.balance`, negatives red), an **estimated stockpile valuation**
  (player `(corp,body)` pools summed at market price), and the **last-tick net** as a
  coloured ±/qtr figure plus a small **sparkline** of recent balances. Maintain a
  capped balance-history buffer in `app`, pushed each `step_economy()` (so the primed
  ticks from C seed it). Files: `src/ui/header_panel.{hpp,cpp}`, `src/core/app.{hpp,cpp}`.
  Deps: foundation. Shares `app.cpp` with C. Satisfies: R1, R2, R3, R4.

Parallelisation note: shares `app.{hpp,cpp}` with C → sequential, same session. The
header source itself is disjoint from A/B.

### Group E — ledger-window-principle (promoted from TODO § Ledger)

Requirements: difficulty 2 — inline. **Verification:** `doc` (`docs/ui/LAYOUT.md` records
the uniform ledger-window rule; TODO § Ledger carries the standing Brief).

- **[2] A — Settle the uniform ledger-window chrome rule.** Defer the Market/Balance/
  Construction ledger family (unchanged in TODO) but record the single design principle
  it must follow: every ledger window shares **one size constant and one spawn anchor**
  (today `tile_inspector` is 820×560 @ +10 and `economy_panel` is 760×620 @ +40 — the
  inconsistency this rule resolves). Write the rule into `docs/ui/LAYOUT.md`, note it on
  the header doc cross-reference, and leave a `[2]` standing Brief under TODO § Ledger so
  the family inherits it. Files: `docs/ui/LAYOUT.md`, `docs/ui/HEADER.md`,
  `docs/development/TODO.md`. Deps: independent (doc-only). 

Parallelisation note: documentation-only; disjoint from all code groups.

---

### Set-wide collision map (Publish step 3)

| File | Groups |
|------|--------|
| `src/world/market_clearing.{hpp,cpp}` | A |
| `src/world/budget_system.cpp` (comment) | A |
| `src/world/economy_system.{hpp,cpp}` | B |
| `src/world/tile_generation.cpp` | B |
| `src/ui/economy_panel.cpp` | B |
| `src/ui/header_panel.{hpp,cpp}` | D |
| `src/core/app.{hpp,cpp}` | C + D (same session) |
| `tools/verify/econ_harness.cpp` | A + B (main session) |
| `docs/ui/LAYOUT.md`, `HEADER.md`, `docs/development/TODO.md` | E + TODO removals |

Source write-sets are disjoint across A/B/D; the only shared code file is `app.cpp`
(C + D), kept in one session. No sub-agents — the set is small and the parallel win is
marginal. Build + run `econ_harness` / `world_audit` / a header verify script after each
group.

</details>

---

## Session 1 — verification + world-gen foundation (Batch Publish)

Three Briefs, barrier semantics. F3 lands **first, alone** (golden-image diffing pays back
across every later visual check); then the two disjoint world-gen fixes fan out.

All three Session-1 groups (F3 golden-image-diff, C2 orphan-island-assignment, B4
corp-starting-holdings) are **complete and committed**; see the REQUIREMENTS.md archive. C2-A ∥
B4-A were fanned out to two concurrent sub-agents on disjoint generation files; the integrator
owned `world_audit.cpp`, the docs, the build, and verification.

---

*No active groups. The worklist is empty between work blocks.*

Completed 2026-06-16 / 2026-06-15 / 2026-06-14 (see DEVLOG, newest first):

- *Session 2 — the lens batch (2 Briefs, strictly serial, branch v0.0.5). **Completed:** the
  Resource lens (`overlay_mode::resource` — highest-value + single-resource deposit-density tint,
  per-body magnitude normalisation, shared lens-local selector, on-canvas gradient key) and the
  Market lens (`overlay_mode::market` — per-body diverging price wash keyed to `price/base_price`,
  Circumplanetary per-body price strip). Both reuse one shared good-selector + on-canvas key,
  inset past the nav rail. 12/12 requirements met; verified by `scripts/verify/resource_lens.lua`
  (4 goldens) and `market_lens.lua` (3 goldens), all PASS ≤0.0089%. One commit per Brief. No
  fan-out (both wrote the same hotspot files). See REQUIREMENTS.md archive.*

- *v0.0.5 Layer 4 UI groundwork — single Brief, scaffold scope (branch v0.0.5). **Completed:**
  the held fifth v0.0.5 enabler — construction interaction state on `ui_state`
  (`construction_state` + `show_construction_panel`); a hover-driven ghost placement marker
  and non-mutating click seam on the Planetary canvas (`body_surface_canvas.cpp`); the
  construction / building-management panel shell with armed-placement Build section and
  disabled management stubs (`src/ui/construction_panel.{hpp,cpp}`); nav-rail slot 6 + shell
  wiring + `verify` hooks (`nav_pane.cpp`, `app.cpp`). **No economic mutation** — the functional
  loop stays in v0.0.6. 8/8 requirements met; verified via the ProjectIo Debug build, code
  grep, and `scripts/verify/construction_panel.lua`. B ∥ C fanned out to two concurrent
  sub-agents. See REQUIREMENTS.md archive.*

- *v0.0.5 Layer 4 foundations — publish set (4 Briefs, barrier semantics, branch v0.0.5).
  **Completed:** reusable placement-rules seam (`src/world/placement_rules.{hpp,cpp}`, Pass 3
  re-pointed, no behaviour change); multi-tick economy-stability harness
  (`tools/verify/econ_stability.cpp`, 100 ticks); workforce-model design
  (`POPULATION.md` § Workforce model); uniform ledger-window chrome
  (`src/ui/ledger_chrome.hpp`, both ledgers re-pointed). 16/16 requirements met; verified via
  the ProjectIo build, `tools/verify/econ_stability`, and `tools/verify/world_audit`. One commit
  per Brief plus a tracking close-out. The roadmap's fifth v0.0.5 enabler (A4 Layer 4 UI
  groundwork) was deliberately held for a later pass. See REQUIREMENTS.md archive.*

- *Layer 3 finalisation — publish set (5 Briefs, barrier semantics). **Completed:**
  price resolution from local supply/demand (A); deposit depletion model (B); pre-game
  economy ticks / warm start (C); player balance header + design pass (D); uniform
  ledger-window chrome principle (E, family deferred). 13/13 requirements met; verified
  via `tools/verify/econ_harness`, `tools/verify/world_audit`, and
  `scripts/verify/header.lua`. Four functional commits (C+D merged — shared `app.cpp`)
  plus a tracking close-out. See REQUIREMENTS.md archive.*

- *Layer 3 economy — publish set (8 Briefs, barrier semantics). **Completed:** data-model
  foundation; recipe/economy registry (Lua); production simulation; per-body market clearing;
  corporate money loop; economy observability panel; building-placement audit (S1); Kepler
  biome balance (S2). 27/27 requirements met; verified via `tools/verify/econ_harness`,
  `tools/verify/world_audit`, and `scripts/verify/economy_panel.lua`. One commit per Brief
  after a doc-refactor commit. See REQUIREMENTS.md archive.*

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

Remaining harness follow-up (golden-image diffing) lives in [OPENS.md](OPENS.md) § Canvas.
