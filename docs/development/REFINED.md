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

## 2026-07-08 Batch — Backlog refinement pass (BL-011, BL-014, BL-016, BL-053, BL-063, BL-097) — **COMPLETE (residue noted)**

Five design-owed items designed and promoted in one session, fanned to 5 worktree agents (Wave 1:
reach/supply lenses, accessibility palette+contrast, accessibility UI-scale, country generation,
view-bounding audit) + 1 follow-on (Wave 2: view-bounding fix, informed by the audit's findings).
All branches merged clean except one `backlog.json` conflict (country-gen agent's worktree had
branched before the design-settling commit landed; reconciled by hand). Full app build green
throughout; `world_audit`/`world_determinism` re-run after every merge, ALL PASS, no regressions.

**BL-053** (country generation) fully complete: discovered the size-variance mechanism
(`merge_small_nations`) already existed from an earlier commit, retuned constants (14→24 nations,
34 seeds pre-merge), harness-verified (count [20,28] + max≥3× min both PASS).
**BL-063** (UI-scale) fully complete: discrete 1.0/1.25/1.5× font-atlas reload, persisted in
`options.cfg`, wired into the F10 Options window.
**BL-097** (view-bounding) fully complete: audit found 2 real bugs (time-panel height pinned to
`mm_h*0.5f`; Tile Ledger spawn anchored off the stale `profile_panel_width`), both fixed; the rest
of the shell (header/profile/nav-rail/minimap/explorer/economy-panel/lens-key) confirmed already
correct.
**BL-011/BL-014** (Reach + Supply-routes lenses) code-complete with one scope deviation: rendered
as an on-canvas key/readout rather than cross-body glow/edges, since `body_surface_canvas.cpp`
only ever draws the active body's own tile grid — the fuller cross-body visual is a follow-up for
`solar_system_canvas.cpp`.
**BL-016** (lens palette) code-complete for the palette half (Okabe-Ito/Viridis re-hue); on-canvas
country/market labels + Corporation-lens legend deferred as a `body_surface_canvas.cpp`/`overlay.cpp`
follow-up.

**Residue (recorded, not dropped):** visual goldens need re-blessing (lens hues changed); the
`palette::text_secondary` AA token isn't yet wired into ~90 existing `TextDisabled` call sites; a
`scripts/verify/*.lua` multi-resolution sweep script (for BL-097) wasn't authored; `docs/ui/ACCESSIBILITY.md`
still isn't written. Requirements `requirements.json` § {reach-supply-lenses, accessibility-strand-1}
remain `pending` on visual verification; § {country-generation-variety} complete; § {view-bounding-audit}
pending on the visual sweep. Permanent record: `docs/development/backlog.json` (BL-011/014/016/053/063/097
all flipped to `complete`), commits `35dd1fb`, `287d8e0`, `95788ef`→`004c5f9` (merge), `1ffe395`→`2506c97`
(merge), `47da542`→`02819a7` (merge). Summary retained one cycle.

---

## 2026-07-07 Batch — Economy dynamism (BL-078, BL-095, BL-096, BL-079, BL-112) — **COMPLETE**

Five interlocking economy items delivered 2026-07-07. **BL-078** redefined the nation substrate into a
tick-time elastic per-capita basket demand + abstract nation-capacity supply (price discovers via
`base×√(D/S)`, `[0.25×,4×]` band kept). **BL-095** made construction market-gated + pay-as-you-build
(3-regime rate, derived stock = prior-tick market supply, analog front-door). **BL-096** resource-carved
the market map (nation-gated by tradeable-resource concentration, fresh RNG offset `0xA5310096u`) +
distributed substrate across a body's markets. **BL-079** added narrow deterministic background-corp
agency (idle a loss-maker / switch a floored recipe; player exempt), reconciled the stale depletion docs,
and wrote the scoped standing-rule exception. **BL-112** upgraded `pregame_balance_harness` into the
economy gate (differentiated/elastic/live-margin/fillable/determinism — all PASS) and rekeyed the
Opportunity lens to unmet demand.

Verified: full app build + **19/19 headless tests green** (incl. new `construction_gate_harness`,
`corp_agency_harness`, and `world_audit` BL-096 assertions), `verifier-review` GO COMPILE, determinism
preserved (`world_determinism`/`econ_stability` green). The two UI slices (095 front-door, 112 lens) were
fanned to sub-agents; the determinism-critical tick core stayed main-session-serial. **Design decision
(Ben, 2026-07-07): the emergent milder opening — the player opens ~break-even/profitable rather than at
the intended net loss — is ACCEPTED**; the fillable-gap dynamism is the win. Requirements
`requirements.json` § {product-market-inert, market-stockpile-build-gate, market-resource-generation,
extraction-boombust-feedback, starting-economy-viability} all complete. Deferred follow-ons: BL-130
(real market inventory vs derived), BL-131 (player market destruction), BL-132 (full market
co-generation). Permanent record: DEVLOG 2026-07-07, PRODUCTION.md, GENERATION_STRATEGY.md, LENSES.md,
io-standing-rules.md. Summary retained one cycle.

---

## 2026-07-07 Batch — BL-126 + BL-113 (08:04:31 → 08:17:27, 12m 56s) — **COMPLETE**

Two-item batch. **BL-126** (toggle rule — `nav_button` re-click on the active tab closes the ledger
via an optional `bool* close`; wired at economy_panel + construction_panel; diff-1, build-green,
correct-by-inspection). **BL-113** (interactive-flow acceptance — recipe/workforce, sell, survey verify
primitives + three acceptance scripts driving the real commit path; sub-agent authored in a worktree,
main session integrated + built + ran all three to PASS; survey funds staged via `set_balance`;
sell_order floor-precedence assert known-weak with a 0 pool). Requirements `interactive-flow-acceptance`
R1–R3 complete. Commits `4e8c3fd`, `be92911`. Permanent record: DEVLOG 2026-07-07, LAYOUT.md,
DEVELOPMENT_PRACTICES.md § Acceptance flows. Summary retained one cycle.

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

## Cross-platform build — Linux + Windows (promoted from BACKLOG § BL-057) — **COMPLETE**

Closed 2026-07-04 by reconciliation: the promoted tasks were left non-terminal although the work
verifiably shipped. Font portability (bundled DejaVuSans), the native Linux build (GCC-14 /
sol2 v3.5.0 fixes), the Actions CI matrix (Linux g++-13/g++-14 + Windows MSVC — first run green
on all three jobs), and the Xvfb/software-renderer offscreen spike all landed and verified.
Permanent record: DEVLOG 2026-06-29 (first native Linux GUI build),
`req/requirements.json § cross-platform-build`, TECH_FOUNDATIONS § Building. **Residue (recorded
per the cancelling policy):** the golden re-bless under the software renderer is deferred —
`scripts/verify/bless_all.sh` exists; once re-blessed goldens land, the advisory `visual-verify`
CI job drops `continue-on-error` and becomes a hard gate. Summary retained one cycle
(from 2026-07-04).

---

## v0.0.8 Batch Delivery — Legibility pass + commercial-sphere fog (promoted 2026-07-04) — **COMPLETE**

All six items landed 2026-07-04, one commit each: BL-088 persistent trade routes (`57e8b7b`),
BL-083 population-centre markers (`ae64ed1`), BL-085 player presence (`370a0a7`), BL-084
industry-density lens (`1619b13`), BL-086 ambient opportunity read (`6878160`), BL-089
commercial-sphere fog (`af07ad0`); batch close `d3629a2`; v0.0.8 cut `8540878`. Verified by the
`trade_routes` + `commercial_fog` headless harnesses and blessed visual goldens. Permanent
record: DEVLOG 2026-07-04, `req/requirements.json`, `docs/ui/DISCOVERY.md`. Summary retained
one cycle (from 2026-07-04).

---

## v0.0.9 Batch Delivery — polish pass (delivered 2026-07-05) — **COMPLETE**

Five promote-ready polish items delivered in one batch, per-item commits: **BL-070** in-app system
menu (gear popup — Pause/Resume + Exit-with-confirm, Esc parity), **BL-081** economy-ledger
legibility (widened balances + dropped per-building table + un-cramped sibling Workforce/Pools/
Markets tables), **BL-082** construction panel height-capped clear of the bottom-left Selection
element during placement, **BL-090** corp-emblem glyph family (shared `ui::icons::corp_emblem` +
`palette::corp_emblem_shape`/`corp_identity_colour`, rendered for player + rivals on card /
selection header / on-canvas markers / hover), and **BL-089 deferral 1 of 2** (hover-card body
activity line). Sub-agents ran the three disjoint UI slices (BL-081, BL-090, BL-089-hover); the two
`app.cpp`-touching items (BL-070, BL-082) stayed in the main session; one integrating build.
Verified by the new `scripts/verify/v009_batch.lua` (4 Windows-blessed goldens) + build-clean;
interaction/hover-gated surfaces (BL-070 popup interior, BL-089 tooltip) code-verified.
**Deferred (recorded, not dropped):** the BL-089 proximity-glimpse peek (save-seam/determinism cost
disproportionate to a polish minor — re-assess at the v0.1.0 boundary). Permanent record: DEVLOG
2026-07-05, `req/requirements.json`, backlog resolutions, `docs/ui/{MENU,LAYOUT,SELECTION,ICONS,
DISCOVERY}.md`. Summary retained one cycle (from 2026-07-05).

---

## BL-099 — Commercial-fog proximity-glimpse peek (delivered 2026-07-08) — **COMPLETE**

BL-089 deferral 2 of 2, landed as its own item. **Sample-and-store** dissolved the determinism
objection that caused the deferral: body positions are mutated `orbital_angle_rad` (not pure in tick),
so `record_proximity_glimpses` samples the closest-approach set ONCE at a player convoy's discrete
completion tick (in `credit_arrived_convoys`, after orbits advanced for that frame) and stores the tick
in `world.body_last_glimpse_tick` (off `body_component`); `body_activity_visibility` returns
`known_stale` for a glimpsed-but-unrouted body within `glimpse_fresh_ticks_default` (90), never
`known`/`visible`, and a body's own route outranks a glimpse. R = `glimpse_radius_au_default` (0.25 AU).
The renderer was untouched (BL-089's blessed `known_stale` badge path). Built main-session-serial.

Verified: `commercial_fog_harness` 19/19 (10 new BL-099 assertions — geometry, tier, endpoint-exclusion,
decay, route-precedence, determinism) + full CTest **19/19** (determinism intact); on-canvas by-eye via
`scripts/verify/proximity_glimpse.lua` before/after. Requirements `requirements.json § proximity-glimpse`
R1+R2 complete. Authority propagated to `docs/ui/DISCOVERY.md`. Golden bless owed on the Linux box
(new `proximity_glimpse` goldens + `commercial_fog_solar` re-bless — its `known_stale` set widened).
Summary retained one cycle (from 2026-07-08).

---

## BL-077 — Planetary logistics: economic core (delivered 2026-07-08) — **COMPLETE**

Rescoped core of the logistics epic, built main-session-serial. **Data model** (A): tile_component.road_level
(default 0), land_use infrastructure value, per-body grid->tile index + A* route cache on world. **A* pathfinder**
(B, new world/logistics.{hpp,cpp}): canonical landform cost table (TILES.md), terrain-weighted A* over the tile grid
(4-cardinal, column wrap, road discount, ocean sea-leg + land/sea mode selection), symmetric edge cost, cached;
reuses nation_generation's Dijkstra shape, expansion_cost untouched (world-gen determinism). **Intra-body dispatch**
(C, supply_system.cpp): same-body market shortfalls source the corp's on-body pool, hauling from its representative
tile to the short market's centre_tile via A*, mode land/sea by ocean-crossing, cost reg.logistics_cost(mode)*dist*qty;
inter-body space path unchanged. **Harness** (D, tools/verify/logistics_harness.cpp).

Verified: logistics_harness 19/19 + full CTest **20/20** (econ_stability + pregame_balance stable on the real world,
determinism intact, supply_advance + trade_routes unaffected). Commits a202dc8 (facility) + this (dispatch).
Requirements planetary-logistics-core R1-R3 complete. Authority: SUPPLY.md. Summary retained one cycle (from 2026-07-08).

