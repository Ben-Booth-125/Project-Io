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

## NAV_SLOT_PANEL_SYNC (promoted from BACKLOG § BL-055)

Requirements: `req/requirements.json § nav-slot-sync`

- **[2] A — Exclusive-open nav-slot toggle.** In `nav_pane.cpp`, add a `close_all_panels(ui_state&)` helper that sets every `show_*` panel boolean to `false`. At the top of each slot's toggle handler, call it before setting the target flag — but only when toggling ON (check the prior value; if it was already `true`, the slot is toggling off, so skip the close-all and just clear the target). This gives a genuine exclusive toggle: clicking an open slot closes it; clicking a closed slot closes any other open panel first. Files: `src/ui/nav_pane.cpp`. Deps: foundation. Satisfies: R1.

Parallelisation note: single task, independent root.

---

## Supply Layer — convoys, logistics costs, inter-body market coupling (promoted from BACKLOG § BL-039, BL-038, BL-045)

Requirements: `req/requirements.json § supply-layer`

Items folded in: **BL-045** (LOGISTICS_NETWORK — cost constants land as task B, before the convoy model consumes them); **BL-038** (INTER_BODY_MARKETS — destination crediting and inter-body coupling land as task E, the final supply-seam task). Both are inseparable at this depth; they do not get their own commit.

Tasks A and B are parallel-safe (disjoint files). A, B → C → D → E serially (shared economy/supply/budget seam). F is disjoint from C–D and can run concurrently with either.

- **[3] A — Convoy component + per-Tick advance.** Define `convoy_component` in `components.hpp`: `entity_id source_market`, `entity_id dest_market`, `enum class convoy_mode { land, sea, air, space } mode`, `resource_type cargo_resource`, `float cargo_qty`, `float progress` (0→1), `float speed`. Create `src/world/supply_system.{hpp,cpp}` with `advance_convoys(world&)` that iterates `world.convoys` (a `std::vector<convoy_component>` hanging off `world`) and increments each convoy's `progress` by `speed` per Tick. Mark arrived convoys (`progress >= 1.0`) with a flag but do not credit yet — that is task E. Files: `src/world/components.hpp`, `src/world/supply_system.hpp`, `src/world/supply_system.cpp`. Deps: foundation. Parallel-safe with B. Satisfies: R1, R2.

- **[2] B — Per-mode logistics-cost constants (BL-045).** Add a `logistics` table to `scripts/economy.lua`: `base_cost_per_unit_distance` keyed by mode string (`"land"`, `"sea"`, `"air"`, `"space"`), ordered land < sea < air < space (reasonable round numbers; tune later). This closes BL-045's cost-constants contribution; endpoint-gate checks fold into task D. Files: `scripts/economy.lua`. Deps: foundation. Parallel-safe with A. Satisfies: R3.

- **[2] C — Logistical-cost budget deduction.** In `budget_system.cpp`, extend `apply_budget` (or a new `apply_logistics_costs(world&, const recipe_registry&)` pass) to debit the dispatching corp's balance at convoy creation time: `base_logistics_cost[mode] × distance × cargo_qty`. For intra-body convoys, `distance` = tile-distance between source and dest tiles; for inter-body convoys, `distance` = Euclidean distance between the two bodies' `solar_body_component::position` fields. Load the mode-cost table from the recipe_registry or a new Lua accessor. Files: `src/world/budget_system.cpp`. Deps: A, B. Satisfies: R4.

- **[3] D — Auto-dispatch trigger.** In `supply_system.cpp`, add `dispatch_convoys(world&, const recipe_registry&)` called each economy Tick. For each `(corp, body, resource)` where market demand exceeds local pool supply, search other `(corp, body)` pools for a surplus of that resource and a reachable source (prototype: all bodies treated as reachable; space mode requires a `building_type::launchpad` in the source corp's assets on that body — check `world.assets`). Dispatch a new `convoy_component` if an affordable source exists (cost ≤ arbitrage margin), committing cargo from the source pool at dispatch. Player-directed dispatch (sell-order matched to an off-body counterparty) is deferred. Files: `src/world/supply_system.cpp`. Deps: C. Satisfies: R5, R6.

- **[3] E — Arrival crediting + inter-body market coupling (BL-038).** In `supply_system.cpp`, after `advance_convoys` marks a convoy arrived: credit the destination `(corp, body)` pool with `cargo_qty` of `cargo_resource`; insert the delivery quantity into the destination body's `market_component.supply[cargo_resource]` so the next clearing pass reprices it; retire the convoy. Cargo was already committed from the source pool at dispatch (task D). This closes the BL-038 (INTER_BODY_MARKETS) coupling: two bodies' markets link **only** through what convoys actually deliver, with no abstract price-coupling term. Files: `src/world/supply_system.cpp`, `src/world/market_clearing.hpp`, `src/world/market_clearing.cpp`. Deps: D. Satisfies: R7, R8.

- **[3] F — Supply lens render passes.** Under `overlay_mode::supply`: on the Planetary canvas, draw a route-segment glyph on source and destination tiles of active convoys; on the Circumplanetary canvas, draw a throughput badge per body (convoy count or cargo total); on the Solar canvas, draw a line per active inter-body convoy between the two body positions. Add a `supply` entry to the lens strip in `overlay.cpp` and author `icons::supply` (a convoy/arrow glyph) in `icons.{hpp,cpp}`. Files: `src/ui/body_surface_canvas.cpp`, `src/ui/circumplanetary_canvas.cpp`, `src/ui/solar_system_canvas.cpp`, `src/ui/overlay.cpp`, `src/ui/icons.hpp`, `src/ui/icons.cpp`. Deps: A (convoy component exists to read). Parallel-safe with C, D (disjoint files). Satisfies: R9.

Parallelisation note: A ∥ B (disjoint files); {A, B} → C → D → E (serial — shared supply/budget/clearing seam); A → F ∥ {C, D} (F reads convoys for display; disjoint from budget/clearing). Integration wiring (`advance_convoys` + `dispatch_convoys` into `app.cpp`'s economy step; `world.convoys` initialisation in `hard_coded_world.cpp`) stays in the main session after E lands.
