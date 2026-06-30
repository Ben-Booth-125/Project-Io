# Project Io — REFINED (active worklist)

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

### Template

```
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

## BL-053 country generation (promoted 2026-06-29) — **COMPLETE**

Delivered: Pass 1b growth weights + Pass 2c merge in `nation_generation.cpp`; Kepler 18 seeds →
14 nations (sizes 24..2150 tiles). world_audit BL-053 R1/R2 green; full headless suite green.
Design propagated to NATION_GENERATION.md. Tasks A–D complete.

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

---

*v0.0.6 Batch Delivery — **Completed** 2026-06-16. 20 items delivered across 6 waves:
Wave A (BL-033 inline + BL-034 doc propagation); Wave B parallel (BL-002+003 icon
silhouettes/outlines, BL-001/006/005/024 corp lens/hover-card/rung-distances/tile-ledger,
BL-007 speed curve, BL-004 icon audit — clean); Wave C parallel (BL-026 null delta —
already conformant, BL-047 population static MVP, BL-049 building management); Wave D
parallel (BL-027 Market Ledger, BL-028 Balance Ledger, BL-029 Construction Ledger, BL-022
Corp Dashboard); Wave E main session (BL-042 workforce pool step 2, BL-021 menu curated
order, BL-030 non-spatial goto); Wave F main session (BL-048 population dynamic half).
Build green at every wave integration. See DEVLOG for session record.*

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

Remaining harness follow-up (golden-image diffing) lives in [BACKLOG.md](BACKLOG.md) § Canvas.

---

## NAV_SLOT_PANEL_SYNC (promoted from BACKLOG § BL-055) — **COMPLETE**

Requirements: `req/requirements.json § nav-slot-sync`

- **[2] A — Exclusive-open nav-slot toggle.** ✓ Files: `src/ui/nav_pane.cpp`. Satisfies: R1.

R1 complete. Build clean.

---

## Supply Layer — convoys, logistics costs, inter-body market coupling (promoted from BACKLOG § BL-039, BL-038, BL-045) — **COMPLETE**

Requirements: `req/requirements.json § supply-layer`

Items folded in: **BL-045** (cost constants); **BL-038** (inter-body market coupling).

- **[3] A — Convoy component + per-Tick advance.** ✓ Satisfies: R1, R2.
- **[2] B — Per-mode logistics-cost constants (BL-045).** ✓ Satisfies: R3.
- **[2] C — Logistical-cost budget deduction.** ✓ Satisfies: R4.
- **[3] D — Auto-dispatch trigger.** ✓ Satisfies: R5, R6.
- **[3] E — Arrival crediting + inter-body market coupling.** ✓ Satisfies: R7.
- **[3] F — Supply lens render passes.** ✓ Satisfies: R9.
- **R8 — Two-body price convergence.** ✓ Extended supply_advance.cpp; price_b 3.625→0.923 over 8 delivery ticks.
- **R9 — supply_lens.lua golden.** ✓ verify.seed_convoy API added; 3/3 golden PASS 0.00%.

All R1–R9 complete. 2026-06-17.

---

## Cross-platform build — Linux + Windows (promoted from BACKLOG § BL-057)

Requirements: requirements.json § cross-platform-build

Context: source is already Linux-clean; the only source gap was `src/ui/fonts.cpp`'s
hardcoded Windows font paths. Dev moves to Linux, playtest stays on Windows; CI guards
both. Tasks A, C, and (the draft of) the workflow were authored in the Cowork sandbox,
which **cannot build** — so they are *code-complete, unverified*; the build/visual/headless
verification (R1–R4, R6) lands on the Linux box / in CI.

- **[2] A — Font portability fix.** Bundled-first candidate list loaded cwd-relative
  (`assets/fonts/DejaVuSans.ttf` → Linux system fonts → Windows system fonts → ProggyClean);
  bundle the font + license; copy `assets/` next to the exe at build time. Files:
  `src/ui/fonts.cpp`, `src/ui/fonts.hpp`, `assets/fonts/DejaVuSans.ttf`,
  `assets/fonts/DejaVuSans-LICENSE.txt`, `CMakeLists.txt`. Deps: foundation.
  Satisfies: R4, R5. *(drafted — needs build+render verify.)*
- **[3] B — Native Linux build recipe.** Confirm a `cmake` configure+build of the full app
  on Linux and document the dep set + commands. Files: `docs/tech/TECH_FOUNDATIONS.md`.
  Deps: A. Parallel-safe with C. Satisfies: R1, R2.
- **[3] C — CI workflow.** GitHub Actions: `ubuntu-latest` builds app + builds/runs the
  headless harnesses; `windows-latest` builds app; FetchContent dep caching. Files:
  `.github/workflows/build.yml`. Deps: A. Parallel-safe with B. Satisfies: R3, R6.
  *(drafted — first Actions run will likely need a dep-set tweak.)*
- **[4] D — Visual-verify offscreen spike.** Make `ProjectIo --verify` render headlessly
  (`SDL_VIDEODRIVER=dummy` + software renderer, else Xvfb); if green, add the visual tier to
  CI. Files: `src/core/app.cpp`, `.github/workflows/build.yml`. Deps: A. Satisfies: R7.
  *(spike — may bounce back to the backlog if it needs design.)*

Parallelisation note: A → {B, C, D}; B ∥ C (disjoint files: docs vs workflow). D shares
`build.yml` with C, so sequence D after C. A is the foundation everything builds on.

---

## v0.0.6 Batch Delivery — Improved Core-Loop (promoted 2026-06-17) ✓ COMPLETE

All 6 items shipped 2026-06-17. 27/27 econ_harness tests PASS, 24/24 visual goldens PASS.
3 commits: `6691c7a` (BL-050 + BL-036), `a980079` (BL-037), `8cf72a7` (BL-025 + BL-035).
BL-056 bankruptcy harness committed in Wave 1 merge (`b4e2b45`).

---

## v0.0.6 Batch Delivery — Improved Core-Loop (archived)

Six items: BL-050 (saturated substrate), BL-037 (order book), BL-056 (bankruptcy harness),
BL-036 (market-centre seeding), BL-025 (multi-market ledger dashboard), BL-035 (warm-start
surface). Barrier semantics; full requirements in `req/requirements.json §` saturated-substrate,
order-book, econ-bankruptcy, market-centre-seeding, multi-market-dashboard, warm-start-surface.

**Fan-out call:** Wave 1 fans out to three parallel sub-agents (BL-050 data model, BL-037
order book, BL-056 harness) — file sets are disjoint and each agent is a coherent vertical
slice. Waves 2–4 are sequential in the main session. Reason: Wave 1 items together span
~d11 of work and share no files; worktree isolation absorbs any overlap at wiring.

### Wave 1 (sub-agents — parallel)

#### BL-050 — Saturated substrate data model

Requirements: `req/requirements.json § saturated-substrate`

- **[2] A — Add substrate_density to tile_component; add nation_substrate aggregate to world.**
  Add `float substrate_density = 0.0f` to `tile_component` (components.hpp). Add
  `nation_substrate` struct (background_supply[resource_count], background_demand[resource_count])
  to `components.hpp`; add `std::map<std::pair<entity_id,entity_id>, nation_substrate> nation_substrates`
  (keyed by (nation, body)) to `world.hpp`. Files: `src/world/components.hpp`, `src/world/world.hpp`.
  Deps: foundation. provides: `tile_component::substrate_density`, `nation_substrate`, `world::nation_substrates`.
  Satisfies: saturated-substrate R1, R2.
- **[2] B — Substrate generation pass in nation_generation.cpp.** After territory assignment
  (Pass 2), for each tile owned by a nation: find the nearest `population_centre_component`
  centre on that body; set `substrate_density = max(0, 1 − dist / ripple_radius) × centre_strength`
  where `ripple_radius` is a constant (e.g. 8 tiles) and `centre_strength` is `centre.scale /
  max_scale`. Accumulate the per-(nation, body) `nation_substrate` aggregate in
  `world.nation_substrates`. Files: `src/world/nation_generation.cpp`. Deps: A.
  provides: populated `tile_component::substrate_density`, populated `world::nation_substrates`.
  Satisfies: saturated-substrate R3.
- **[2] C — Inject substrate supply/demand into market clearing.** In `economy_system.cpp`,
  before market clearing, call `inject_substrate(world& w)`: for each `(nation, body)` key in
  `nation_substrates`, add `background_supply[r]` and `background_demand[r]` to the appropriate
  body's market `supply[r]` / `demand[r]`. Scale: `background_supply[r] = nation_substrates[(n,b)].background_supply[r]`
  (already authored by B). Files: `src/world/economy_system.{hpp,cpp}`. Deps: A, B.
  Satisfies: saturated-substrate R4.

Parallelisation note: A → B → C (linear; A provides the types B uses, B populates data C reads).
Sub-agent does NOT touch `hard_coded_world.cpp` — integration wiring stays main session.
**Uses Bash tool with heredoc for git commits; PowerShell is blocked by the allow rule.**

#### BL-037 — Order book (preferential purchasing)

Requirements: `req/requirements.json § order-book`

- **[2] A — Add buy_order struct and order-book clearing signature.** Add `buy_order`
  (corp, body, resource, quantity, max_price, preferred_seller = null_entity) to
  `components.hpp`. Change `clear_markets` signature to accept both sell and buy order
  lists; keep the old default-empty overload as a shim. Files: `src/world/components.hpp`,
  `src/world/market_clearing.hpp`. Deps: foundation.
  provides: `buy_order`, updated `clear_markets` signature.
  Satisfies: order-book R1, R2.
- **[3] B — Implement matched order-book clearing.** Replace the pooled supply/demand
  aggregate in `clear_markets` with per-resource order books: AI corps emit default sell
  orders (all surplus at 0 floor) and default buy orders (all shortfall at 999 max) so
  the book degrades to today's behaviour when no explicit orders exist. Match price-time
  priority: cheapest-first among sellers, highest-bidder-first among buyers; trades clear
  at the seller's ask. Apply preferred_seller bias: a preferred seller wins ties and is
  tolerated up to +10% premium over the cheapest unpreferred option; avoided seller is last
  resort. EMA price (`market_component.price[r]`) is updated from the volume-weighted
  average of cleared trades this tick (same formula as before). Files:
  `src/world/market_clearing.{hpp,cpp}`. Deps: A.
  Satisfies: order-book R3, R4, R5.

Parallelisation note: A → B. Disjoint from BL-050 (which injects into supply/demand arrays
only — it does not touch market_clearing.cpp). Disjoint from BL-056.
**Uses Bash tool with heredoc for git commits; PowerShell is blocked.**

#### BL-056 — Economy bankruptcy harness

Requirements: `req/requirements.json § econ-bankruptcy`

- **[3] A — Author econ_bankruptcy.cpp harness.** `tools/verify/econ_bankruptcy.cpp`: call
  `make_hard_coded_world()` + load recipes + load economy; tick `run_economy_step` +
  `clear_markets` + `apply_budget` in a loop. Bankruptcy trigger: player corporation
  cannot cover maintenance for a tick AND `balance <= -5 × start_money`. Ceiling: 500
  in-game years. On exit, print: summary (years to bankruptcy or "solvent at ceiling"),
  per-building cash burn per year, per-resource net flow per year. Files:
  `tools/verify/econ_bankruptcy.cpp`. Deps: foundation. Satisfies: econ-bankruptcy R1, R2, R3.
- **[1] B — Register econ_bankruptcy in CMakeLists.txt.** Add `econ_bankruptcy` target
  following the existing `econ_harness` pattern. Files: `CMakeLists.txt`. Deps: A.
  Satisfies: econ-bankruptcy R4.

Parallelisation note: A → B. Fully disjoint from BL-050 and BL-037.
**Uses Bash tool with heredoc for git commits; PowerShell is blocked.**

### Integration 1 (main session — after Wave 1 merges)

Wire BL-050 into `hard_coded_world.cpp`: ensure `make_hard_coded_world` calls nation
generation after population centres exist (order check), so `substrate_density` is
populated and `nation_substrates` is filled. Run build + headless verify.

### Wave 2 (main session)

#### BL-036 — Seed multiple market centres

Requirements: `req/requirements.json § market-centre-seeding`

- **[3] A — Seed multiple market centres per body from population centres.** In
  `hard_coded_world.cpp`, update market authoring: for each body, find population centres
  above a threshold `scale` (e.g. ≥ 0.3); create one `market_component` per qualifying
  centre, with `centre_tile` set to the population centre's tile. Keep the single-market
  fallback for bodies with no qualifying centre (null_entity). Files:
  `src/world/hard_coded_world.cpp`. Deps: Integration 1. Satisfies: market-centre-seeding R1, R2, R3.

Parallelisation note: single task. Depends on BL-050 integration (population centres
must exist before seeding derives from them).

### Wave 3 (main session)

#### BL-025 — Multi-market ledger dashboard

Requirements: `req/requirements.json § multi-market-dashboard`

- **[3] A — Rework Market Ledger to show all markets on a body as a dashboard.**
  Change `draw_market_ledger` to: show a combo to select the **body** (not the market);
  below, render one section per market on that body (header = "Market N of M at tile
  (x,y)", then the existing resource table for that market). When a body has one market
  the layout is identical to today. Files: `src/ui/market_ledger.cpp`. Deps: Wave 2
  complete (multiple markets exist). Satisfies: multi-market-dashboard R1, R2.

Parallelisation note: single task; sequential after BL-036.

### Wave 4 (main session)

#### BL-035 — Economy warm-start surface

Requirements: `req/requirements.json § warm-start-surface`

- **[2] A — Surface opening supply/demand health in Market Ledger.** Add a header section
  to `draw_market_ledger`: one row per resource showing opening state — if `supply > demand`,
  show "(excess → auto-sold)"; if `demand > supply`, show "(short → auto-bought)"; if
  balanced, omit. Settle-tick count is a `constexpr int settle_ticks = 2` in `app.cpp`
  (the existing warm-start already runs 2 ticks; expose the constant). Files:
  `src/ui/market_ledger.cpp`, `src/core/app.cpp`. Deps: Wave 3. Satisfies: warm-start-surface R1, R2.

Parallelisation note: single task; sequential after BL-025.

---

**Collision map:**

| File | Items |
|---|---|
| `src/world/components.hpp` | BL-050-A, BL-037-A |
| `src/world/world.hpp` | BL-050-A |
| `src/world/nation_generation.cpp` | BL-050-B |
| `src/world/economy_system.{hpp,cpp}` | BL-050-C |
| `src/world/market_clearing.{hpp,cpp}` | BL-037-A, BL-037-B |
| `tools/verify/econ_bankruptcy.cpp` | BL-056-A |
| `CMakeLists.txt` | BL-056-B |
| `src/world/hard_coded_world.cpp` | BL-036-A + BL-050 integration (main session) |
| `src/ui/market_ledger.cpp` | BL-025-A, BL-035-A |
| `src/core/app.cpp` | BL-035-A |

**BL-050-A and BL-037-A both touch components.hpp.** This is the one cross-agent shared
file. BL-050 adds `substrate_density` + `nation_substrate`; BL-037 adds `buy_order`.
These are independent struct additions — the agents operate on disjoint regions of the
file. Worktree isolation absorbs the write; the main session resolves any trivial merge
conflict at integration.

---

## Lens & Legibility — Batch Delivery — **COMPLETE** (2026-06-17)

All seven items delivered, verified (22/22 requirements, deterministic goldens), docs
propagated, items removed from the backlog. See DEVLOG § Lens & Legibility Batch
Delivery. Tasks A–H below retained for one cycle as the decomposition record.

<details>
<summary>Completed task breakdown (retained for reference)</summary>

Requirements: `req/requirements.json §` lens-strip-single-select, faction-to-country-rename,
resource-density-flat, population-opportunity-lens, production-output-lens,
scarcity-market-heatmap, meta-per-lens-upper-rungs.

The full lens strip (`Corp → Country → Resource → Market → Population → Opportunity →
Production → Scarcity`) bar the Market slot (gated on multi-market seeding, BL-036). All
work converges on `body_surface_canvas.cpp` + `ui_state.hpp` + `overlay.cpp` +
`presentation.{hpp,cpp}` + `icons.{hpp,cpp}` — a single-file-concentrated render refactor,
so **no code fan-out** (worktree-merge cost on the shared fill chain exceeds the win; see
DELIVERY.md § Sub-agents — "passes inside one file are sequential, hotspots stay in the main
session"). Sequential, foundation-first. Doc propagation fans out at the close; the
`verifier-review` skill runs the review barrier.

### Wave 0 — foundation (main session)

- **[2] A — Enum + signature + default.** Add `overlay_mode::opportunity`, `overlay_mode::production`;
  rename `overlay_mode::faction` → `country`. Default lens → `corporation` (`ui_state.hpp`
  field **and** `app.cpp:283`). Extend `draw_body_surface_canvas` to take
  `const recipe_registry&` + `const economy_report&`; update the call site (`app.cpp:689`,
  pass `m_registry`, `m_last_econ_report`) and `body_surface_canvas.hpp`. Bump
  `canvas_command.cpp overlay_mode_count` (5 → 10) and refresh its stale comment.
  Files: `src/ui/ui_state.hpp`, `src/ui/body_surface_canvas.hpp`, `src/core/app.cpp`,
  `src/ui/canvas_command.cpp`. Deps: foundation.
  provides: `overlay_mode::{country,opportunity,production}`, new canvas signature.
  Satisfies: lens-strip R2, faction-to-country R1.
- **[2] B — corp_colour rename.** `palette::faction_colour` → `corp_colour`,
  `faction_slot_count` → `corp_slot_count`, `faction_table` → `corp_table`; update call sites
  (`presentation.cpp`, the `body_surface_canvas.cpp` local lambda, any others). `nation_colour`
  untouched. Files: `src/ui/presentation.hpp`, `src/ui/presentation.cpp`,
  `src/ui/body_surface_canvas.cpp`. Deps: foundation. provides: `palette::corp_colour`.
  Satisfies: faction-to-country R2.
- **[2] C — Country glyph + labels.** `icons::faction` → `icons::country`; strip label
  "Faction presence" → "Countries", short "Faction" → "Country"; the `faction` case in
  `overlay.cpp` switches and `app.cpp` name parse (`"country"`, keep `"faction"` alias).
  Files: `src/ui/icons.hpp`, `src/ui/icons.cpp`, `src/ui/overlay.cpp`, `src/core/app.cpp`.
  Deps: A. provides: `icons::country`. Satisfies: faction-to-country R1, R3.

### Wave 1 — lens render passes (main session, sequential in `body_surface_canvas.cpp`)

- **[3] D — Resource lens → flat contiguous fill.** Replace the magnitude-opacity tint with an
  8-connected flood fill from any tile with >0 of `lens_resource`, uniform flat fill over the
  whole deposit; always single-resource. Update `draw_resource_key`. Files:
  `src/ui/body_surface_canvas.cpp`, `src/ui/overlay.cpp` (drop the Single checkbox / highest-value
  path). Deps: A. Satisfies: resource-density R1, R2.
- **[3] E — Scarcity lens → per-market shortfall blocks.** Replace the deposit-based per-tile
  heatmap with `shortfall = max(0, demand−supply)` of `lens_resource`, per-market via
  `market_for_tile`, normalised across the body's markets, uniform tint per catchment. Update
  `draw_scarcity_key`. Files: `src/ui/body_surface_canvas.cpp`. Deps: A. Satisfies:
  scarcity-market R1, R2.
- **[3] F — Production lens.** `overlay_mode::production` pass: per built tile, intensity =
  Σ(output qty × `market.price`) from the `economy_report` building rows (processor outputs split
  by recipe proportions); log scale vs the body's producing-tile mean; idle/exhausted = cold.
  New `draw_production_key`. Files: `src/ui/body_surface_canvas.cpp`. Deps: A. Satisfies:
  production-output R1, R2.
- **[3] G — Opportunity lens.** `overlay_mode::opportunity` pass: per tile, best valid building's
  net margin (extraction: deposit×base_rate×price; processing: Σout×price − Σin×price; − maintenance)
  over types valid on the terrain (`placement_rules`), diverging red→green normalised per body.
  New `draw_opportunity_key`. Files: `src/ui/body_surface_canvas.cpp`. Deps: A. Satisfies:
  population-opportunity R1.

### Wave 2 — strip ordering (main session)

- **[2] H — Strip order + selector.** Reorder `modes[]` to Corp, Country, Resource, Market,
  Population, Opportunity, Production, Scarcity; add the `opportunity`/`production` glyph cases to
  `draw_lens_icon`, `overlay_mode_name`, `overlay_mode_short_name`; keep the
  resource/market/scarcity good-selector. Files: `src/ui/overlay.cpp`. Deps: C, D, F, G.
  Satisfies: lens-strip R1, R3.

### Wave 3 — review barrier, build, verify

- Run **`verifier-review`** over the integrated diff (step 4a), resolve any Critical.
- Build (`cmake`); fix to green.
- **`verifier-visual`** for each `visual` row: author/extend `scripts/verify/{lens_strip,
  country_lens,resource_lens,population_lens,opportunity_lens,production_lens,scarcity_lens}.lua`,
  bless goldens.

### Wave 4 — docs (fan-out: disjoint authority docs)

- Propagate to `docs/ui/LENSES.md` (all six lenses + BL-012 rung table), `docs/ui/ICONS.md`
  (country/opportunity/production glyphs), `docs/GLOSSARY.md` (Faction→Country). Each a disjoint
  doc → parallel doc agents.

### Collision map

| File | Tasks |
|------|-------|
| `src/ui/ui_state.hpp` | A |
| `src/ui/body_surface_canvas.hpp` | A |
| `src/core/app.cpp` | A, C |
| `src/ui/canvas_command.cpp` | A |
| `src/ui/presentation.{hpp,cpp}` | B |
| `src/ui/icons.{hpp,cpp}` | C |
| `src/ui/body_surface_canvas.cpp` | B, D, E, F, G (hotspot — sequential, main session) |
| `src/ui/overlay.cpp` | C, D, H (sequential) |

`body_surface_canvas.cpp` and `overlay.cpp` are touched by most tasks → **no concurrent agents**;
the win from worktree-splitting a single shared fill chain is negative. Foundation (A/B/C) first;
render passes D/E/F/G are independent *regions* of the same file done back-to-back; H last.

</details>

---

## v0.0.7 Batch Delivery — Building management, population & supply (promoted 2026-06-17)

Nine items. Barrier semantics — all tasks reach terminal state before commits.
Fan-out: Wave 1 items are file-disjoint; main session runs sequentially (parallel win marginal at this size).

**Collision map:**

| File | Items |
|---|---|
| `src/world/economy_system.cpp` | BL-041 |
| `src/world/placement_rules.{hpp,cpp}` | BL-043 |
| `src/world/construction.{hpp,cpp}` | BL-043, BL-044 |
| `src/world/recipe_registry.{hpp,cpp}` | BL-044 |
| `scripts/economy.lua` | BL-044 |
| `src/ui/icons.{hpp,cpp}` | BL-059 |
| `src/ui/ui_state.hpp` | BL-059, BL-060 |
| `src/ui/body_surface_canvas.cpp` | BL-059, BL-015, BL-060, BL-031, BL-010 (hotspot — sequential) |
| `src/ui/overlay.cpp` | BL-015 |
| `src/ui/hover_card.{hpp,cpp}` | BL-060 (new files) |
| `src/ui/selection_panel.cpp` | BL-031, BL-032 |

### Wave 1 — Economy + Placement foundation (main session, sequential)

#### BL-041 — Habitability gates max workforce

Requirements: `req/requirements.json § habitability-workforce`

- **[3] A — Apply habitability cap to workforce demand.** Before the contention loop in `run_economy_step`, derive `hab_cap_by_body` (mean population-centre habitability weighted by scale; default 1.0 for bodyless). Apply `min(1.0f, hab / 0.6f)` as a scalar on `b.workforce_assigned` when summing `demand_by_body`. Files: `src/world/economy_system.cpp`. Deps: foundation. Provides: `hab_cap_by_body` scalar. Satisfies: R1, R2, R3, R4.

Parallelisation note: single task, economy_system.cpp only. Disjoint from BL-043.

#### BL-043 — Stricter building placement rules

Requirements: `req/requirements.json § building-placement-rules`

- **[2] A — Add is_coastal + can_place_in_world helpers.** In `placement_rules.hpp/.cpp`: add `is_coastal(const world& w, entity_id tile_id) -> bool` (checks hex neighbours for ocean); add `can_place_in_world(const world& w, entity_id tile_id, building_type, resource_type) -> bool` that calls `can_place` then layers coastal (port) and body-count (launchpad ≤ 1) checks. New `world.hpp` include in placement_rules.cpp. Files: `src/world/placement_rules.{hpp,cpp}`. Deps: foundation. Provides: `is_coastal`, `can_place_in_world`. Satisfies: R2, R3, R4.
- **[1] B — Update construction.cpp to use can_place_in_world; add slot_occupied.** In `construction.hpp`, add `slot_occupied` to `construction_result`. In `construction.cpp`, replace `can_place(tc, ...)` with `can_place_in_world(w, tile, type, target)` and return `slot_occupied` when the launchpad cap fires. Files: `src/world/construction.{hpp,cpp}`. Deps: A. Satisfies: R1, R5.
- **[1] C — Show slot_occupied message in construction_panel.** In `construction_panel.cpp`, add "Already placed on this body." message for `slot_occupied`. Files: `src/ui/construction_panel.cpp`. Deps: B. Satisfies: R5.

Parallelisation note: A → B → C (linear). Disjoint from BL-041.

### Wave 2 — Canvas markers + Market boundary lens (main session, sequential — BSC hotspot)

#### BL-059 — Selectable entity markers

Requirements: `req/requirements.json § selectable-markers`

- **[1] A — Add marker_hit_zone struct and hit-zone list to ui_state.** Define `marker_hit_zone { entity_id id; enum class kind { building, market_centre, unit }; ImVec2 centre; float radius; }` in `ui_state.hpp`; add `std::vector<marker_hit_zone> marker_hit_zones` to `ui_state`. Files: `src/ui/ui_state.hpp`. Deps: Wave 1 done. Provides: `marker_hit_zone`, `ui_state::marker_hit_zones`. Satisfies: R4.
- **[1] B — Add icons::market_centre glyph.** A small circle with a cross (+) in `icons.hpp/.cpp` — distinct from building/unit glyphs. Files: `src/ui/icons.{hpp,cpp}`. Deps: foundation. Provides: `icons::market_centre`. Satisfies: R3.
- **[2] C — Draw building + market-centre markers and register hit zones.** In `draw_body_surface_canvas`, after lens overlays and before Selection chrome: iterate buildings on the active body, compute screen position from tile grid coords, draw `icons::building` scaled with zoom, register a `marker_hit_zone`. Iterate markets on the active body, draw `icons::market_centre` at the centre_tile position, register zone. Clear `state.marker_hit_zones` at the top of the function and fill during this pass. Files: `src/ui/body_surface_canvas.cpp`. Deps: A, B. Satisfies: R1, R2, R3, R4, R5.

Parallelisation note: A ∥ B (disjoint files); both → C (sequential in BSC).

#### BL-015 — Market boundary lens

Requirements: `req/requirements.json § market-boundary-lens`

- **[3] A — Replace market price-wash with catchment-tint in BSC.** In `draw_body_surface_canvas` under `overlay_mode::market`: remove per-tile price diverging wash; add a pre-pass that assigns each market an index colour (cycling palette, same alpha as Corporation lens tint); tint each tile by `market_for_tile`'s result market's colour. Add `draw_market_boundary_key`. Files: `src/ui/body_surface_canvas.cpp`. Deps: BL-059-C done. Satisfies: R1, R2, R3, R4.
- **[1] B — Update overlay.cpp market tooltip.** Change the strip tooltip for `overlay_mode::market` from price-wash description to "Market catchment boundaries". Files: `src/ui/overlay.cpp`. Deps: A. Satisfies: R3.

Parallelisation note: A → B (BSC hotspot — sequential after BL-059-C).

### Wave 3 — Hover card, hit-testing, construction pricing (main session)

#### BL-060 — Hover-card primitive

Requirements: `req/requirements.json § hover-card`

- **[2] A — New hover_card.{hpp,cpp}.** `draw_hover_card(ImVec2 cursor, int hover_ticks, std::function<void()> content)` renders after `kHoverDelay = 20` frames of stable hover: an ImGui child window (no title bar, semi-opaque dark background, 4px rounding), max width 200px, positioned just above the cursor. Files: `src/ui/hover_card.{hpp,cpp}`. Deps: BL-059 done. Satisfies: R2.
- **[2] B — Wire tile hover to hover_card in BSC.** Track a `hover_tile_ticks` counter in `draw_body_surface_canvas`; pass `content` = tile name + key stat (terrain type + habitability for plain canvas; selected resource deposit for Resource lens). Files: `src/ui/body_surface_canvas.cpp`, `src/ui/ui_state.hpp` (hover counter). Deps: A. Satisfies: R1, R3.

Parallelisation note: A → B (B needs the hover_card API).

#### BL-031 — Canvas hit-testing for entity markers

Requirements: `req/requirements.json § canvas-hit-testing`

- **[2] A — Hit-test marker_hit_zones before tile in BSC click handler.** In the single-click handler of `draw_body_surface_canvas`: before resolving the clicked tile, iterate `state.marker_hit_zones` and select the entity whose circle contains the cursor (prioritise building > market-centre, closest wins on tie). Set `state.selected_entity` to the winning entity. Files: `src/ui/body_surface_canvas.cpp`. Deps: BL-059 done. Satisfies: R1, R2.
- **[1] B — selection_panel building branch.** When `selected_entity` maps to a building (found in `w.buildings`), render: building type + target/recipe, tile name, workforce slider (read-only for now). Files: `src/ui/selection_panel.cpp`. Deps: A. Satisfies: R3.

Parallelisation note: A → B.

#### BL-044 — Construction pricing — buildings cost resources

Requirements: `req/requirements.json § construction-pricing`

- **[2] A — Add resource_build_cost to building_economics + author in Lua.** Add `std::array<float, resource_count> resource_build_cost = {}` to `building_economics` in `recipe_registry.hpp`. In `economy.lua`, add `resource_costs` table per building type (extraction_site: 20 steel; processing_facility: 25 steel; port: 20 steel; launchpad: 50 steel + 20 refined_fuel). Update `recipe_registry.cpp` to load it. Files: `src/world/recipe_registry.{hpp,cpp}`, `scripts/economy.lua`. Deps: Wave 1 done. Provides: `building_economics::resource_build_cost`. Satisfies: R2.
- **[2] B — Check + consume resource cost in construct_building.** Add `insufficient_materials` to `construction_result` (construction.hpp). In `construct_building`, after the funds check, verify `pool_for(corp, tile_body)` has enough of each `resource_build_cost[r]`; if short return `insufficient_materials`. On success, deduct from pool. Files: `src/world/construction.{hpp,cpp}`. Deps: A. Satisfies: R1, R3, R4.
- **[1] C — Show resource cost in construction_panel.** Display the resource cost next to the budget cost for each building type in the Build section. Show "insufficient_materials" message. Files: `src/ui/construction_panel.cpp`. Deps: B. Satisfies: R4.

Parallelisation note: A → B → C (linear).

### Wave 4 — Placement suitability + Lens-driven selection (main session)

#### BL-010 — Placement-suitability surface

Requirements: `req/requirements.json § placement-suitability`

- **[3] A — Tile-selection suitability pass in BSC.** When `state.selected_entity` is a tile: determine the tile's best valid building (richest extractable deposit → extraction_site, else any land → processing_facility). For each other tile: if `can_place` rejects → dark overlay (IM_COL32(0,0,0,90)); if terrain is affine (same composition class for the building type) → coloured tint (IM_COL32(100,200,100,60)); else no signal. Files: `src/ui/body_surface_canvas.cpp`. Deps: Wave 3 done. Satisfies: R1, R2, R3.

Parallelisation note: single task, BSC hotspot.

#### BL-032 — Lens-driven selection resolution

Requirements: `req/requirements.json § lens-driven-selection`

- **[4] A — Lens-contextual content in selection_panel.** Extend `draw_selection_panel` to branch on `w.overlay` (the active lens): tile + Corporation lens shows ownership + corp colour; tile + Production lens shows per-building output rates from the economy report; tile + Market lens shows market catchment + price row; building selected shows management controls (workforce target, decommission). Files: `src/ui/selection_panel.cpp`. Deps: BL-031 done (building selection exists). Satisfies: R1, R2.

Parallelisation note: single task.

---

*v0.0.7 Batch Delivery **COMPLETE** (shipped 2026-06-17; see ROADMAP § v0.0.7 and the DEVLOG).
The breakdown above is retained for one cycle as the decomposition record. The stale "resume"
pointer is cleared — the active worklist below is the v0.0.8 frontier.*

---

## v0.0.8 — Discovery & Intelligence — BL-067 Survey system (promoted 2026-06-30) — **COMPLETE**

Delivered 2026-06-30. All tasks A–F complete. `survey_harness` R2–R6 (41 assertions PASS);
`survey.lua` 4 goldens PASS at 0.0000%. Design propagated to systems.md / SOLAR / PLANETARY /
SELECTION / ICONS / GLOSSARY; BL-067 marked complete in backlog.json; BL-068 unblocked. The task
breakdown below is retained for one cycle as the decomposition record.

Requirements: `docs/development/req/requirements.json § survey-system`. Full mode (difficulty 5,
priority A). Independent root — unblocks BL-068 (visibility model piggybacks on the region mask).
Authoritative design: `backlog.json` BL-067 `design` field.

**Mode / fan-out call:** kept in the **main session, sequential**. The world/* logic (A, B) is a
tight foundation; the UI surfaces (D, E, F) collide on the canvas/selection hotspots and need the
shared `survey_state` types from A — the worktree-split win is negative for a single-item chain.

- **[2] A — Survey data model.** Add `enum class survey_phase` and `struct survey_state` to
  `components.hpp`; carry `survey_state survey` inline on `body_component`. Files:
  `src/world/components.hpp`. Deps: foundation. **provides:** `survey_phase`, `survey_state`,
  `body_component::survey`. Satisfies: R1–R6 (data substrate).
- **[3] B — Survey system logic.** New `src/world/survey_system.{hpp,cpp}`:
  `survey_cost`/`survey_schedule` (size×distance, pure), `region_count`/`region_of_tile`/
  `region_reveal_index` (deterministic raster partition, no RNG), `advance_surveys(world&, int days)`
  (phase crossing + `regions_done` bump), `dispatch_survey(world&, entity_id) -> survey_dispatch_result`
  (player-balance guard + upfront debit + schedule). `home_body` seeded surveyed at world-gen / first
  access. Files: `src/world/survey_system.{hpp,cpp}`, `src/world/world.hpp` (home-surveyed seam if
  needed). Deps: A. **provides:** `advance_surveys`, `dispatch_survey`, `survey_dispatch_result`,
  region helpers. **consumes:** A's types. Satisfies: R2, R3, R4, R5, R6.
- **[2] C — Headless harness.** `tools/verify/survey_harness.cpp`: cost/duration formulas (R2),
  deterministic partition + raster order (R3), home surveyed (R4), concurrent independence (R5),
  dispatch guards + upfront debit (R6). Files: `tools/verify/survey_harness.cpp`. Deps: B.
  Satisfies: R2–R6 verification.
- **[1] D — Icons + ui_state seam.** Add `icons::survey_badge` + `icons::unknown` (the `?` glyph)
  per ICONS.md recipe; add `entity_id pending_survey_dispatch = null_entity` to `ui_state`. Files:
  `src/ui/icons.{hpp,cpp}`, `src/ui/ui_state.hpp`. Deps: foundation. **provides:** `icons::survey_badge`,
  `icons::unknown`, `ui_state::pending_survey_dispatch`.
- **[2] E — App integration.** Per-day `advance_surveys` crossing in `app::run` (mirror the
  econ-tick crossing; new `m_last_survey_day`); execute `pending_survey_dispatch` in render (mirror
  the construction request seam); seed `home_body` surveyed at startup. Files: `src/core/app.{hpp,cpp}`.
  Deps: B, D. **consumes:** `advance_surveys`, `dispatch_survey`, `ui_state::pending_survey_dispatch`.
- **[3] F — Canvas + selection surfaces.** Solar badge (`solar_system_canvas.cpp`) per phase;
  Planetary region mask (`body_surface_canvas.cpp`) — masked regions dark/locked, revealed render
  normally, header scan progress; Selection-panel Survey section (`selection_panel.cpp`) keyed on
  phase (Dispatch button with cost+ETA / En route / Surveying k∕N / Surveyed). Files:
  `src/ui/solar_system_canvas.cpp`, `src/ui/body_surface_canvas.cpp`, `src/ui/selection_panel.cpp`.
  Deps: D, E. **consumes:** region helpers, `survey_state`, `icons::survey_badge`/`unknown`,
  `pending_survey_dispatch`. Satisfies: R1 (visual).

Parallelisation note: A → B → C; A,D foundation → E → F. Linear chain; no fan-out. Build after B
(headless), then full `cmake` after F. `verifier-review` over the integrated diff before the full
compile. Visual R1 via `verifier-visual` (`scripts/verify/survey.lua`) once the app builds.

Collision map:

| File | Tasks |
|---|---|
| `src/world/components.hpp` | A |
| `src/world/survey_system.{hpp,cpp}` | B |
| `src/world/world.hpp` | B (home-surveyed seam) |
| `tools/verify/survey_harness.cpp` | C |
| `src/ui/icons.{hpp,cpp}`, `src/ui/ui_state.hpp` | D |
| `src/core/app.{hpp,cpp}` | E |
| `src/ui/solar_system_canvas.cpp`, `src/ui/body_surface_canvas.cpp`, `src/ui/selection_panel.cpp` | F |
