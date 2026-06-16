# Project Io — Requirements

One table per Brief that has been promoted to TASKS.md. A table is created at
promotion and **kept permanently** — when its Brief completes (or is cancelled) the
section is moved to the **Completed / cancelled** archive at the foot of this file, not
deleted. This file is therefore a **permanent record** of every requirement the project
has ever set and how it was resolved. The task group in TASKS.md links here by section;
tasks carry `Satisfies: Rn` fields pointing at individual rows.

---

## Guide

### Table format

| ID | Requirement | Verification | Status | Notes |
|----|-------------|--------------|--------|-------|

**Columns:**
- **ID** — sequential within the Brief (R1, R2, …). Referenced in TASKS.md task
  `Satisfies:` fields and in DEVLOG status lines.
- **Requirement** — a single testable outcome in the present tense ("The `corporation`
  glyph is declared in `icons.hpp`").
- **Verification** — one of: `build` (clean compile); `code: <pattern>` (symbol or
  string present in source); `doc: <path>` (file or section exists); `headless`
  (headless harness produces expected output); `visual` (screenshot + human
  confirmation); `manual` (human review, no automated check).
- **Status** — `pending` | `complete` | `failed`.
- **Notes** — iteration detail: date of failure, reason, or change made before the
  next attempt.

### Workflow

1. **At promotion** — create a section under **Active requirements** named after the
   Brief slug. Derive requirements from the Brief description's success criteria; add
   a row per testable outcome. Link it from the TASKS.md group header as
   `Requirements: [REQUIREMENTS.md § <slug>](req/REQUIREMENTS.md#<slug>)`.
2. **As tasks land** — update statuses. A `failed` status does not block the Brief:
   add a note, leave the row as `pending`, refine the responsible task, and retry.
3. **On completion** — when all rows are `complete` (or `failed` rows are accepted as
   explicitly out of scope), remove the group from TASKS.md and the Brief from OPENS.md,
   then **move this section, intact, to the Completed / cancelled archive** at the foot
   of the file. Add a `Resolved:` line above the table (date + outcome, e.g.
   `Resolved: 2026-06-14 — complete, all rows met`). **Never delete a section** — the
   archive is the project's permanent requirement history.
4. **On cancellation** — a cancelled group (see TASKS.md § Cancelling a task group)
   moves to the archive the same way, with a `Resolved:` line recording the cancellation
   and reason. Its rows keep the real status they reached. If the Brief is later
   re-promoted, copy the section back up to **Active requirements** and continue from
   there.

**Scope:** apply requirements tables to Briefs of difficulty 3 and above. For
difficulty 1–2 Briefs an inline `Verification:` note in the task entry is sufficient.

**DEVLOG convention:** every session entry's **Status** line records the requirement
count, e.g.:
`Status: Complete — 5/6 requirements met (R4 failed; see REQUIREMENTS.md § corporation-lens).`

### Verifying when no skill or tool exists

A task is only **complete** (see TASKS.md § Definition of "complete") when each of
its requirements has actually had its **Verification** *run*. When that verification
can be performed with an available skill or tool — a `build`, a `code:` grep, the
headless harness, an existing visual-check skill — run it and record the result.

For the `visual` class specifically, a tool now **does** exist: the headless
visual-verification harness (`ProjectIo --verify scripts/verify/<name>.lua`; see
DEVELOPMENT_PRACTICES.md § Visual verification). Author or extend a verify script
and inspect the PNG captures rather than deferring to a manual human check.

When **no** skill or tool exists to perform a requirement's verification, do not
silently downgrade it to an assumption. Instead:

1. **Determine a method.** Define a concrete, repeatable way to test the
   requirement — what is exercised, what input, what observable pass/fail signal.
2. **Author it as a tool, then push it to a skill.** Build the method as a concrete
   tool — a `tools/verify/*.cpp` headless harness, a `scripts/verify/*.lua` check, a
   script, or a documented procedure — then **promote it to a skill** so the next
   requirement of the same shape reuses a permanent, discoverable asset rather than
   re-deriving it (the `verifier-visual` / `verifier-headless` skills are the model;
   see CLAUDE.md § Skills → *Tool creation is skill creation*). **Creating a skill
   needs user permission:** attempt to author the tool, push it to a skill; **if skill
   creation is denied, request running the tool as a one-off** for this requirement.
   Prefer a saved artifact over a one-off manual check. Either way, record the method
   (or a pointer to it) in the row's Notes.
3. **Defer only when it needs design.** If establishing the method is impossible
   without non-trivial design consideration — it needs new infrastructure, an
   architectural decision, or its own scoping — do **not** block the task. Record
   the testing-method work as a [`../OPENS.md`](../OPENS.md) Brief (with file pointers
   and enough context to pick up), leave the requirement `pending` with the
   deferral reason in Notes, and proceed. A requirement whose method is deferred is
   **not** complete, and the task carrying it is at best *code-complete* until the
   method lands and the verification is run.

### Agent workflow

A planning agent writes both the TASKS.md group and this file's section together.
An implementation agent reads only its task group (from TASKS.md) and the matching
section here — it does not need the full OPENS backlog or DEVLOG history in context.

---

*No active requirements. The worklist is empty between work blocks; sections appear here
when a Brief is promoted, and move to the archive below on completion or cancellation.*

---

## Completed / cancelled (archive)

Permanent record of resolved requirement groups, newest first. Each carries a
`Resolved:` line and is retained verbatim; re-promote by copying a section back up to
**Active requirements**.

### market-lens-render

Resolved: 2026-06-16 — complete, all rows met. Promoted from OPENS § Canvas —
**[B3] Market lens render pass**. `overlay_mode::market`: a body-wide diverging warm↔cool
Planetary wash keyed to `price/base_price` (markets are per-body, so the tint is uniform —
a refinement of the spec's "per-tile … relative to body mean"), and a Circumplanetary per-body
price strip (in `circumplanetary_canvas.cpp`, the correct rung — not `solar_system_canvas.cpp`
as the handoff's file list said). Reuses the Resource lens's shared good-selector + on-canvas key.
3 blessed goldens PASS at ≤0.0082%.

| ID | Requirement | Verification | Status | Notes |
|----|-------------|--------------|--------|-------|
| R1 | **Brief-spanning:** under the Market lens the Planetary surface washes the body by the selected good's relative price (warm = dear, cool = cheap), shows an on-canvas diverging key + good-selector, and the Circumplanetary rung shows a per-body price strip — golden-verified. | `visual` (`scripts/verify/market_lens.lua` golden) | complete | 3/3 goldens PASS ≤0.0082%, exit 0 |
| R2 | Planetary: a diverging warm↔cool tint keyed to `price[g]/base_price[g]` (neutral at the floor ratio 1.0), uniform across the active body (per-body market). | `code` + `visual` | complete | `diverging_colour`; iron ×0.57 cool wash captured |
| R3 | An on-canvas diverging key (cheap ↔ dear) renders with the selected good's name. | `visual` | complete | `draw_market_key`; name + ratio |
| R4 | The good-selector (shared in form with the Resource selector, bound to `lens_resource`) picks the displayed good. | `code` + `visual` | complete | iron→steel re-keys the wash |
| R5 | Circumplanetary: when the Market lens is active, a per-body price strip lists the anchor body's market prices with the selected good highlighted. | `visual` | complete | 7 goods listed, Fe row highlighted; `circumplanetary_canvas.cpp` |
| R6 | The golden runs `verify.econ_step` first so prices diverge from base, then captures; the market pass reads the resolved `market_component.price`. | `visual` + `doc` | complete | `econ_step(12)` + new `show_panel` hook to clear the panel |

### resource-lens-render

Resolved: 2026-06-16 — complete, all rows met. Promoted from OPENS § Canvas —
**[B3] Resource lens render pass**. `overlay_mode::resource`: highest-value mode tints each tile
by its richest deposit's identity hue at a per-body magnitude-normalised opacity (composited over
terrain via `lerp_colour`); single-resource mode is a heatmap of one selected good. A lens-local
"Single" toggle + a shared resource combo (bound to `ui_state.lens_resource`, reused by Market)
drive it; an on-canvas gradient key sits inset past the nav rail. 4 blessed goldens PASS at ≤0.0089%.

| ID | Requirement | Verification | Status | Notes |
|----|-------------|--------------|--------|-------|
| R1 | **Brief-spanning:** under the Resource lens the Planetary surface tints tiles by deposit density (highest-value default + single-resource heatmap), shows an on-canvas gradient key, and the resource strip button is present — golden-verified against a blessed reference. | `visual` (`scripts/verify/resource_lens.lua` golden) | complete | 4/4 goldens PASS ≤0.0089%, exit 0 |
| R2 | `overlay_mode::resource` exists and is wired: a strip button (the `icons::resource` glyph), `overlay_mode_name` ("Resource density"), short name, `overlay_from_name`, and the strip `modes[]` list. | `code` + `build` | complete | enum + `overlay.cpp` + `app.cpp`; build clean |
| R3 | Highest-value mode: each tile tints to its richest deposit's `presentation_of(res).colour` at an opacity scaled by that deposit's magnitude (normalised per body); zero-deposit tiles keep terrain. | `code` + `visual` | complete | per-body normalised; `lerp_colour` composite; ranks by richness (weight deferred — LENSES.md note) |
| R4 | Single-resource mode: a lens-local selector picks a resource; every tile tints that resource's colour at its per-tile magnitude (zero → terrain). | `code` + `visual` | complete | iron/coal heatmaps captured; shared combo with Market |
| R5 | An on-canvas key renders: a sparse→dense gradient bar, plus the selected resource's name+swatch (single mode) or a swatch list of the body's present resources (highest-value mode). | `visual` | complete | left-edge key inset past nav rail, clear of chrome |
| R6 | A `verify` hook drives the lens mode + selected resource headlessly so the golden is reproducible; the lua check is named in the `verifier-visual` skill or run via it. | `code` + `doc` | complete | `verify.set_lens_resource` / `set_resource_mode`; run via `verifier-visual` |

### orphan-island-assignment

Resolved: 2026-06-15 — complete, all rows met. Promoted from OPENS § Nation generation —
**[C2] Orphan-island assignment**. A deterministic post-pass (`assign_orphan_islands` in
`nation_generation.cpp`) groups unclaimed non-ocean land into cardinal-adjacency components and
assigns each whole component to the nearest claimed tile's nation across water. `world_audit`
reports **6048/6048 Kepler land tiles owned, 0 unclaimed** (was ~12% unclaimed).

| ID | Requirement | Verification | Status | Notes |
|----|-------------|--------------|--------|-------|
| R1 | After `generate_nations`, every non-ocean land tile on Kepler is assigned to a nation (zero unclaimed land). | `headless` (world_audit orphan-land count == 0) | complete | 0 unclaimed of 6048 |
| R2 | Each orphan land component is assigned, whole, to the nearest nation measured across water (deterministic, seed-stable). | `headless` + `code` | complete | no RNG; raster-order, distance/index tie-break |

### corp-starting-holdings

Resolved: 2026-06-15 — complete, all rows met. Promoted from OPENS § Corporation generation —
**[B4] Revise the corporation starting-holdings shape**. The flat `k_min_holdings`/`k_max_holdings`
range was retired for a focus-shaped `holdings_range` (extraction 3–4, processing 2–3, trade 1–2);
the anchor + nearest-tile clustering is retained. `world_audit` confirms all 8 corps sit within
their focus ceilings (counts now 1–3, was 3–6) and the S1 `can_place` placement check stays PASS.

| ID | Requirement | Verification | Status | Notes |
|----|-------------|--------------|--------|-------|
| R1 | Corporations open with a **lean, focus-shaped** holding set — per-focus counts smaller than the retired flat 3–6 spread. | `headless` (world_audit per-corp counts within focus bounds) | complete | 8/8 corps within ceiling |
| R2 | The flat `k_min_holdings`/`k_max_holdings` range is retired in favour of a focus-shaped count. | `code` (range constants gone) | complete | replaced by `holdings_range` |
| R3 | Every placed asset still passes `placement_rules::can_place` — no placement regression. | `headless` (world_audit S1 stays PASS) | complete | S1 R2 + seam checks PASS |
| R4 | Holdings remain clustered within the home nation's territory. | `code` + `headless` | complete | anchor/neighbourhood logic unchanged |

### golden-image-diff

Resolved: 2026-06-15 — complete, all rows met. Promoted from OPENS § Canvas —
**[F3] Visual-verification harness — golden-image diffing**. PNG reader + diff added to
`png_writer`; `run_verify` gained a compare/bless step and a golden dir derived from the
script path; `--bless` added to `main.cpp`. End-to-end gate (R5) verified: a clean re-run
PASSed at 0.0056% differing, a deliberately wrong golden FAILed at 56.44% with a diff image
and a non-zero exit. `verifier-visual` SKILL.md documents the bless flow + tolerance knobs.

| ID | Requirement | Verification | Status | Notes |
|----|-------------|--------------|--------|-------|
| R1 | `png_writer` exposes `read_png_rgba` that reads a PNG produced by `write_png_rgba` back to byte-identical RGBA pixels (stored-block zlib only — the writer's own output). | `headless` (round-trip via golden compare) | complete | round-trip exercised by the PASS run (0.0056%) |
| R2 | `png_writer` exposes a diff that counts a pixel as differing when its max channel delta > `T`, fails when the differing fraction > `F`, and can emit a highlighted diff image; both knobs are caller-overridable. | `headless` + `code: diff_rgba` | complete | `diff_rgba` takes `T`; `F` owned at the call site |
| R3 | `run_verify` compares each capture against `scripts/verify/golden/<name>.png` when present, logs PASS/FAIL, and writes a diff image to `screenshots/diff/<name>.png`; absent golden = capture-only (today's behaviour). | `code` + visual run | complete | diff written to `screenshots/diff/` |
| R4 | A `--bless` mode writes captures into the golden directory instead of comparing, so an intentional change regenerates goldens. | `code` + run | complete | `header_populated` golden blessed |
| R5 | End-to-end: a golden check PASSes against a blessed golden and FAILs against a deliberately altered capture (the tolerance knobs discriminate). | `headless`/visual harness run | complete | PASS 0.0056% / FAIL 56.44%, exit 0 / 1 |

### corporation-generation-revision

Resolved: 2026-06-15 — complete, all rows met. Promoted from TODO § Corporation generation —
**[B4] Revise the corporation generation strategy** + **[C3] Model pre-game profit** (coordinated,
same passes). Drafted by a sub-agent (holdings) then revised in the main session: the agent's
pre-game warm-start hand-built a *duplicate* economy registry inside generation — excised in favour
of the existing app-startup warm-start (which reuses the loaded registry). Verified via
`tools/verify/world_audit` and a clean `ProjectIo` Debug build.

| ID | Requirement | Verification | Status | Notes |
|----|-------------|--------------|--------|-------|
| R1 | Each corporation opens with a clustered set of multiple assets (target 3–6), not a single building. | `headless` | complete | world_audit: 15 extraction assets across 8 corps, all valid |
| R2 | The asset mix reflects the corp's `industrial_focus` (extraction/processing/trade anchored). | `manual` | complete | `focus_asset_pattern`; reviewed |
| R3 | Holdings cluster spatially around a focus-weighted anchor (nearest-first placement). | `manual` | complete | distance-sorted neighbourhood; reviewed |
| R4 | Every placed asset passes `placement_rules::can_place` (never ocean / never a deposit-less extraction site). | `headless` | complete | world_audit: 0 invalid placements |
| R5 | Opening balances and (corp, body) pools reflect a multi-tick operating history, seeded without duplicating the economy constants. | `build` | complete | app warm-start 2→12 ticks, reuses the loaded registry |

### player-sell-orders

Resolved: 2026-06-15 — complete, all rows met. Promoted from TODO § Trade —
**[A3] Player-driven sell orders & preferential purchasing**. The **sell-orders** half is
done; **preferential purchasing** is split out as its own deferred Brief (it needs a matched
order-book the anonymous pooled clearing lacks). Verified via `tools/verify/econ_harness`
(SO.1–3) and a clean `ProjectIo` Debug build.

| ID | Requirement | Verification | Status | Notes |
|----|-------------|--------------|--------|-------|
| R1 | `sell_order` is storable as player game-state (`ui_state.sell_orders`) and passed to `clear_markets` each tick. | `build` | complete | moved to components.hpp; wired in `app::step_economy` |
| R2 | A standing player sell order sells up to its quantity from the (corp, body) pool, valued at `max(resolved_price, floor_price)`. | `headless` | complete | SO.2 |
| R3 | The auto-surplus path yields a resource the player has a standing order for (manual control overrides the greedy auto-sell). | `headless` | complete | SO.2/SO.3 (order sells, pool debited) |
| R4 | The building-management UI authors orders (resource / quantity / floor) on the in-view body and lists/removes them. | `build` | complete | `draw_sell_orders_section` |

### build-front-door

Resolved: 2026-06-15 — complete, all rows met. Promoted from TODO § Selection info element —
**[A3] Tile Selection element as the build front door**. Makes the v0.0.5 construction scaffold
functional: a single `construct_building` world function (validation + cost spend + component
authoring), reached from the tile Selection element (the build front door) and the placement-mode
canvas click. Verified via `tools/verify/construction_harness` (11/11 PASS) and a clean `ProjectIo`
Debug build.

| ID | Requirement | Verification | Status | Notes |
|----|-------------|--------------|--------|-------|
| R1 | `construct_building` validates the tile via `placement_rules::can_place` (rejects ocean / wrong-deposit) with no mutation on failure. | `headless` | complete | C.R1 |
| R2 | On success it creates the building (+ stockpile), authors it (target / staffing), appends it to the corp's assets, and debits the registry build cost. | `headless` | complete | C.R2 |
| R3 | A constructed processing facility is seeded with the default recipe so it is productive. | `headless` | complete | C.R3 |
| R4 | Construction is refused for insufficient funds (no build, no spend). | `headless` | complete | C.R4 |
| R5 | Unknown corp / tile are rejected. | `headless` | complete | C.R5 |
| R6 | The tile Selection element hosts a "Build here" affordance (buildable types + cost) that enqueues a request the app executes; the placement-mode canvas click enqueues the same. | `build` | complete | UI builds clean; logic shared with R1–R5 |

### lens-system-design

Resolved: 2026-06-15 — complete, all rows met. Promoted from TODO § Canvas — **[B3] Design the
lens system (complete the stubs)**. Design-doc Brief plus the one new glyph. The Resource-lens
*render pass* (enum + strip button + Planetary pass) is a new follow-on implementation Brief left
in TODO. Verified by doc inspection + a clean build of the new glyph.

| ID | Requirement | Verification | Status | Notes |
|----|-------------|--------------|--------|-------|
| R1 | LENSES.md settles Supply / Market / Faction / Resource to the Corporation section's depth (per-lens spec, rung table, legend, interaction notes). | `doc: docs/ui/LENSES.md` | complete | |
| R2 | The Resource lens is specified (deposit density: highest-value + single-resource modes, gradient legend). | `doc: docs/ui/LENSES.md` | complete | |
| R3 | A distinct `ui::icons::resource` lens glyph is declared and implemented, and catalogued in ICONS.md. | `build` | complete | overload of the resource pip |

### workforce-pool

Resolved: 2026-06-15 — complete, all rows met. Promoted from TODO § Workforce —
**[A4] Workforce pool & population coupling**, step 1 (pool without population;
POPULATION.md § Workforce model). Step 2 (population-derived supply) remains in TODO
under the same Brief, coupled to the S4 population-centre work. Verified via
`tools/verify/econ_harness` (WF.R2–R5 + all pre-existing Layer 3 assertions unchanged)
and a clean `ProjectIo` Debug build.

| ID | Requirement | Verification | Status | Notes |
|----|-------------|--------------|--------|-------|
| R1 | `world` holds an authored per-`(corp, body)` workforce supply with a default fallback and an accessor (`workforce_supply`). | `code: workforce_supply` | complete | |
| R2 | `run_economy_step` computes a per-`(corp, body)` contention scalar `min(1, supply/demand)` and reports it on `economy_report.workforce_contention`. | `headless` | complete | WF.R2 |
| R3 | Effective workforce (`workforce_assigned × contention`) scales extraction and processing output, reported per building (`building_report.effective_workforce`). | `headless` | complete | WF.R3 |
| R4 | Wages in `apply_budget` are paid on effective (allocated) workforce, not the requested target. | `headless` | complete | WF.R4 |
| R5 | Default supply leaves single-building corps uncontended; an over-built `(corp, body)` is throttled. | `headless` | complete | WF.R5 + unchanged L3 rows |
| R6 | The economy panel surfaces per-`(corp, body)` contention when below 1.0. | `code: contention` | complete | `draw_workforce` |

### layer4-ui-groundwork

`Resolved: 2026-06-15 — complete; all 8 rows met. The fifth v0.0.5 enabler (held from the
original Layer 4 foundations set), published scaffold-only on branch v0.0.5. Verified via the
ProjectIo Debug build, code grep, and scripts/verify/construction_panel.lua (panel capture).`

The fifth v0.0.5 enabler: the Layer 4 building-management **interaction scaffold** — seams and
panel shells, **no economic mutation** (the functional loop is v0.0.6). Promoted from TODO § Canvas [A4].

| ID | Requirement | Verification | Status | Notes |
|----|-------------|--------------|--------|-------|
| R1 | `ui_state` carries a `construction_state` placement sub-state: an `active` flag, a `building_type`, and a `resource_type` target. | `code: struct construction_state` + `build` | complete | `ui_state.hpp:35`. |
| R2 | `ui_state` carries `show_construction_panel`, defaulting **false** (ledgers start closed, per MENU.md). | `code: show_construction_panel = false` | complete | `ui_state.hpp:62`. |
| R3 | When placement mode is active, the Planetary canvas draws a ghost building marker at the hovered tile, coloured by validity (positive=valid / negative=invalid via `placement_rules::can_place`). | `visual` + `code: can_place` | complete | Code-verified (`body_surface_canvas.cpp:433-451`). The ghost is hover-driven; the headless harness has no synthetic mouse, so the on-canvas capture is not feasible — same harness limit recorded for the body-label and frame-stutter checks. |
| R4 | A left-click in placement mode mutates neither the world nor the selection (scaffold seam); tile selection occurs only when placement mode is inactive. | `code: !state.construction.active` + `build` | complete | `body_surface_canvas.cpp:492-495`. |
| R5 | The construction panel renders with the shared ledger chrome (`ledger_window_size`/`ledger_window_spawn`, `ImGuiCond_Once`) and defaults closed. | `code: ledger_window_size` + `visual` | complete | `construction_panel.cpp:169-170`; screenshots/construction_panel_build.png. |
| R6 | The panel's Build section sets the placement-mode state (building type + extraction target) and offers a Cancel that clears it. | `code` + `visual` | complete | Capture shows "Placing: Extraction Site → Iron Ore" + target list + Cancel. |
| R7 | The panel shows read-only detail for the building on the selected tile (type / target / recipe / workforce / cost) with **disabled** stub recipe / workforce / sell-order controls. | `visual` + `code: BeginDisabled` | complete | Disabled stubs `construction_panel.cpp:142-155`; panel shell + empty-state captured (no tile-selection verify hook exists to drive a populated capture headlessly). |
| R8 | The panel is reachable from a nav-rail slot and wired into `app::render`, with `verify` API hooks driving it headlessly. | `code` + `build` + `visual` | complete | nav slot 6 (`nav_pane.cpp`), `app.cpp` render call + `show_construction`/`place_mode` hooks; capture shows the panel open and the nav glyph. |

### v0.0.5 Layer 4 foundations publish set (placement-rules-seam … uniform-ledger-chrome)

`Resolved: 2026-06-15 — complete; all 16 rows across the four groups met. Published as a
barrier set on branch v0.0.5, one commit per Brief plus a tracking close-out. Verified via
the ProjectIo Debug build, tools/verify/econ_stability (100-tick stability), and
tools/verify/world_audit (placement seam + negative controls).`

### placement-rules-seam

| ID | Requirement | Verification | Status | Notes |
|----|-------------|--------------|--------|-------|
| R1 | A screen-independent `src/world/placement_rules.{hpp,cpp}` declares the prototype-extractable resource set and an `is_ocean_tile` / `richest_extractable` helper family. | `code: placement_rules` + `build` | complete | ProjectIo Debug build clean. |
| R2 | `can_place(const tile_component&, building_type, resource_type target) → bool` enforces: extraction only on a non-zero deposit of the target / valid terrain, never ocean; processing/port on any non-ocean land. | `code: can_place` + `build` | complete | world_audit negative controls (ocean / zero-deposit rejected, processing accepted). |
| R3 | `corporation_generation.cpp` Pass 3 (`place_starting_asset`) calls the seam with no behaviour change to generation (same placements). | `headless` (world_audit placement counts unchanged) | complete | world_audit: 3 extraction assets, 0 invalid (unchanged from S1). |
| R4 | `world_audit` asserts every placed extraction asset passes `can_place`, and ocean / zero-deposit tiles fail it. | `headless` | complete | world_audit: "can_place agrees with placement + negative controls: PASS". |

### econ-stability-harness

| ID | Requirement | Verification | Status | Notes |
|----|-------------|--------------|--------|-------|
| R1 | `tools/verify/econ_stability.cpp` runs `run_economy_step → clear_markets → apply_budget` over 100 ticks on a small fixed world. | `headless` | complete | econ_stability: "ran 100 ticks without tripping a stability assertion". |
| R2 | Over the run, every market price stays within `[0.25×, 4×] base_price` and does not diverge/oscillate unboundedly. | `headless` | complete | econ_stability R2 PASS. |
| R3 | No NaN/Inf appears in any price, pool quantity, or balance across the run. | `headless` | complete | econ_stability R3 PASS. |
| R4 | Deposit reserves decrease monotonically toward exhaustion; balances do not diverge unboundedly. | `headless` | complete | econ_stability: reserve 1200 → 7.42 monotonic; balances bounded. |
| R5 | `econ_stability` is named in the `verifier-headless` skill and its exe has a settings.json allow rule. | `doc: .claude/skills/verifier-headless/SKILL.md` | complete | Skill + README updated; `Bash(& ".\econ_stability*)` added to settings.json (user-approved) and the CLAUDE.md mapping. |

### workforce-model-design

| ID | Requirement | Verification | Status | Notes |
|----|-------------|--------------|--------|-------|
| R1 | `docs/economy/POPULATION.md` specifies the corporation-wide (or per-body) labour **pool** model. | `doc: docs/economy/POPULATION.md` | complete | § Workforce model → The labour pool (settled per-`(corp, body)`). |
| R2 | It specifies proportional **contention** when total building workforce demand exceeds pool supply. | `doc:` | complete | § Contention (uniform `supply/demand` scalar). |
| R3 | It specifies how workforce **supply and wages derive from population centres**, and the split between what the player **sets** vs. what the system **allocates**. | `doc:` | complete | § Player-set vs. system-allocated + § Wages. |
| R4 | It records the upgrade path from the L3 authored `workforce_assigned` constant to the pool model. | `doc:` | complete | § Upgrade path from the authored constant (3-step additive migration). |

### uniform-ledger-chrome

| ID | Requirement | Verification | Status | Notes |
|----|-------------|--------------|--------|-------|
| R1 | `src/ui/ledger_chrome.hpp` declares one shared ledger-window **size** constant and one shared **spawn-anchor** constant, anchored clear of the profile/header chrome. | `code: ledger_chrome` + `build` | complete | `ledger_window_size` / `ledger_window_spawn`; build clean. |
| R2 | Both the Tile Ledger (`tile_inspector.cpp`) and the Economy panel (`economy_panel.cpp`) use the shared constants with `ImGuiCond_Once` (no per-window literal size/offset). | `code:` (no 820/560/760/620 literals remain) + `build` | complete | Both re-pointed; literals removed; build clean. |
| R3 | `docs/ui/LAYOUT.md` § Uniform ledger-window chrome references the shared constants. | `doc: docs/ui/LAYOUT.md` | complete | Section now records the implemented constants. |

### price-resolution

`Resolved: 2026-06-15 — complete; all 4 rows met. Verified via tools/verify/econ_harness
(resolved iron/steel prices and corp balances at the eased sqrt(D/S) target) and the
ProjectIo build.`

| ID | Requirement | Verification | Status | Notes |
|----|-------------|--------------|--------|-------|
| R1 | `clear_markets` resolves each traded `market_component.price[r]` toward `base_price[r] × sqrt(demand[r]/supply[r])` each tick. | headless | complete | econ_harness: iron S20/D4 → 1.809. |
| R2 | The resolved price is clamped to `[0.25×, 4×] base_price[r]` and eased from the prior price by an EMA (smoothing 0.5); zero-supply pushes to the ceiling, zero-demand to the floor. | headless | complete | econ_harness: steel D0 floored then eased base 8 → 5.0. |
| R3 | Sales income and purchase expenditure in the returned `corp_cash_flow` are valued at the **resolved** price, not `base_price`. | headless | complete | econ_harness: corp balances 1027.18 / 996.76 at resolved price. |
| R4 | The change builds clean and the budget loop (reading the flows) reflects resolved-price cash. | build | complete | ProjectIo Debug build clean. |

### deposit-depletion

`Resolved: 2026-06-15 — complete; all 5 rows met. Verified via tools/verify/world_audit
(reserve seeding over the generated world) and tools/verify/econ_harness (draw-down,
taper, exhaustion) plus the ProjectIo build.`

| ID | Requirement | Verification | Status | Notes |
|----|-------------|--------------|--------|-------|
| R1 | `resource_remaining[r]` is seeded at generation to `resource_deposit[r] × deposit_reserve_factor` for every non-zero deposit; richness is unchanged. | headless | complete | world_audit: 65382 deposits, 0 mis-seeded (factor 400). |
| R2 | Extraction draws its credited output from `resource_remaining`, decrementing it each tick. | headless | complete | econ_harness: reserve 1e6 → 1e6−20; full-rate draw at ample reserve. |
| R3 | Output tapers down over the last `deposit_taper_ticks` of nominal yield as the reserve nears zero. | headless | complete | econ_harness: reserve 80 → output 10 (half nominal). |
| R4 | A building whose reserve falls below `deposit_min_taper` of nominal reports `exhausted` ("out of resources") and produces nothing; the state is distinct from idle and shown in the economy panel. | headless | complete | econ_harness: reserve 5 → exhausted, output 0; panel State column. |
| R5 | Deposits never refill (finite); the change builds clean. | build | complete | No refill path; ProjectIo Debug build clean. |

### balance-header

`Resolved: 2026-06-15 — complete; all 4 rows met. Verified via scripts/verify/header.lua
(capture showed BALANCE Cr 126.3k green, STOCKPILE Cr 0, NET +810/qtr green, rising
sparkline) and the ProjectIo build.`

| ID | Requirement | Verification | Status | Notes |
|----|-------------|--------------|--------|-------|
| R1 | The header renders the player corporation's running `balance`, with negatives flagged red (`palette::negative`). | visual | complete | header_populated.png: balance green (positive). |
| R2 | The header renders an estimated stockpile valuation (player `(corp,body)` pools summed at market price). | visual | complete | header_populated.png: STOCKPILE Cr 0 (extractor sells surplus each tick; renders). |
| R3 | The header renders the last-tick net change as a coloured signed per-quarter figure plus a sparkline of recent balances. | visual | complete | header_populated.png: NET +810/qtr green + rising sparkline. |
| R4 | `app` maintains a capped balance-history buffer pushed each `step_economy()`; the build is clean. | build | complete | m_balance_history (cap 64); ProjectIo Debug build clean. |

### Layer 3 economy publish set (economy-data-model … placement-rules-audit)

`Resolved: 2026-06-15 — complete; all 27 rows across the seven groups met. Published as a
barrier set, one commit per Brief after a doc-refactor commit. Verified via
tools/verify/econ_harness, tools/verify/world_audit, and scripts/verify/economy_panel.lua.
See DEVLOG § "Layer 3 economy published".`

### economy-data-model

| ID | Requirement | Verification | Status | Notes |
|----|-------------|--------------|--------|-------|
| R1 | `building_component` declares `resource_type target_resource` and `uint16_t recipe`, with a `no_recipe` sentinel constant. | `code: target_resource` + `code: no_recipe` in components.hpp | complete | |
| R2 | `tile_component` declares a reserved `resource_remaining` array (resource-indexed), unused in L3. | `code: resource_remaining` | complete | |
| R3 | `corporation_component` declares a `float balance`. | `code: balance` | complete | |
| R4 | `world` holds a `(corporation, body) → stockpile_component` pool with a `pool_for(corp, body)` accessor. | `code: pool_for` + `build` | complete | Deterministic `std::map` keyed by pair. |

### recipe-registry

| ID | Requirement | Verification | Status | Notes |
|----|-------------|--------------|--------|-------|
| R1 | `scripts/recipes.lua` authors the prototype processing recipes (steel, refined fuel, food rations) as `{inputs, outputs}` with reagent support. | `doc: scripts/recipes.lua` | complete | |
| R2 | `recipe_registry` loads recipes via sol2 (protected calls only) into C++ tables addressable by the `building_component.recipe` id. | `code: recipe_registry` + `code: protected_function` / safe_script | complete | |
| R3 | A recipe is a struct of input/output quantities indexed by `resource_type`; Lua resource names map to the enum. | `code: struct recipe` | complete | |
| R4 | Per-`building_type` economy constants (`base_rate`, `maintenance`, `base_wage`, `build_cost`) and global `t_full`/`t_idle` are queryable from the registry. | `code:` accessor + `build` | complete | |
| R5 | `scripts/economy.lua` authors those constants with legible round defaults. | `doc: scripts/economy.lua` | complete | |

### production-simulation

| ID | Requirement | Verification | Status | Notes |
|----|-------------|--------------|--------|-------|
| R1 | `run_economy_step` credits each extraction building's (corp, body) pool with its `target_resource` at `base_rate × richness × workforce × (1 − hazard)`. | `headless` (pool rises after a step) | complete | Method: `verify.econ_step` hook + headless harness. |
| R2 | Processing consumes inputs pool-first and accrues outputs, using the two-threshold (`t_full`/`t_idle`) partial-run model. | `headless` | complete | |
| R3 | A building below `t_idle` on its limiting input idles (no output, recorded idle). | `headless` | complete | |
| R4 | Workforce applies as a single linear scalar at both stages. | `code:` + `headless` | complete | |
| R5 | Deposits do not deplete — `resource_remaining` is untouched by the step. | `code:` (no write to resource_remaining) | complete | |

### market-clearing

| ID | Requirement | Verification | Status | Notes |
|----|-------------|--------------|--------|-------|
| R1 | Per body market, supply = each corp's surplus listed for sale (pool above processor needs this tick). | `headless` (market.supply populated) | complete | |
| R2 | Demand = processor input shortfalls auto-bought from the market. | `headless` | complete | |
| R3 | Transactions clear at `base_price`; `market.price` stays at `base_price`. | `code:` + `headless` | complete | |
| R4 | A framework hook for player-driven sell orders exists (no-op in L3). | `code:` (hook present) | complete | |

### budget-system

| ID | Requirement | Verification | Status | Notes |
|----|-------------|--------------|--------|-------|
| R1 | Income = goods sold × `base_price` credited to `corporation.balance`. | `headless` (balance moves) | complete | |
| R2 | Expenditure = input purchases × `base_price` + per-building maintenance + wages (`workforce × base_wage`). | `headless` | complete | |
| R3 | Balance opens at `starting_capital` and may go negative (no insolvency consequence). | `code:` + `headless` | complete | |

### economy-panel

| ID | Requirement | Verification | Status | Notes |
|----|-------------|--------------|--------|-------|
| R1 | The panel shows each (corp, body) pool's per-resource quantities. | `visual` | complete | Method: `scripts/verify/economy_panel.lua` after `verify.econ_step`. |
| R2 | It shows each building's current output rate and idle/active state (+ limiting input for processors). | `visual` | complete | |
| R3 | It shows body market supply/demand figures. | `visual` | complete | |
| R4 | It shows the per-corporation balance, with negative values flagged red. | `visual` | complete | |

### placement-rules-audit

| ID | Requirement | Verification | Status | Notes |
|----|-------------|--------------|--------|-------|
| R1 | A findings list records whether Pass 3 placement matches PRODUCTION.md rules (no extraction on zero-deposit/invalid terrain; never `ocean`). | `manual` (findings recorded in DEVLOG) | complete | |
| R2 | Cheap gaps are fixed so every placed extraction asset sits on a tile with a non-zero deposit of a valid target; larger gaps promoted. | `headless` (audit assertion over a generated world) | complete | Method: headless harness checks placed assets. |

---

Two groups completed **2026-06-14** before this permanent-history policy was adopted, so
their full requirement tables were deleted under the old "delete on completion" lifecycle
rather than archived. Their authoritative record lives in the DEVLOG; reconstructed
summaries are kept here so the archive is not silent about them:

### selection-go-to-planetary

`Resolved: 2026-06-14 — complete, all rows (R1–R5) met.`

'Go to' on a body now descends to its Planetary tile surface (`focus_on_entity` →
`focus_on_surface`); the tile branch is a no-op. Verified with the new
`verify.go_to` hook + `scripts/verify/selection_go_to.lua`: Kepler, Cinder, and
Selene all land on their tile grids, so the "only works for Kepler" symptom was an
unhelpful landing rung, not an id/lookup failure. Closed the duplicate Known Bug row
and the two Selection rows in one change.

| ID | Requirement | Verification | Status | Notes |
|----|-------------|--------------|--------|-------|
| R1 | `focus_on_entity` routes a **body** selection through `focus_on_surface` (Planetary rung), not `focus_on_body`. | `code: focus_on_surface` in the body branch of `focus_on_entity` | complete | `view_nav.cpp` body branch now calls `focus_on_surface`. |
| R2 | `focus_on_entity` does **nothing** for a **tile** selection (surface already shown; pan-to-tile out of scope). | `code:` (tile branch is a no-op / early return) | complete | Tile branch is an early `return`. |
| R3 | 'Go to' lands on the Planetary surface for **any** body, not only Kepler. | `visual` (a non-Kepler body lands on its tile grid) | complete | `go_to_00_kepler_home` / `_01_cinder_planet` / `_02_selene_moon` all show tile grids; minimap re-anchors to each body. |
| R4 | `docs/ui/SELECTION.md` 'go to' table + click model read "body → Planetary surface; tile → no-op". | `doc: docs/ui/SELECTION.md` | complete | Per-kind 'go to' table updated. |
| R5 | A durable `verify.go_to(name)` hook drives `focus_on_entity`, and `scripts/verify/selection_go_to.lua` captures the landing. | `code: go_to` + `doc: scripts/verify/selection_go_to.lua` | complete | Reusable method for R3. |

### generation-ledger-design

`Resolved: 2026-06-14 — complete, all rows (R1–R4) met.`

Design-only deliverable: `docs/generation/GENERATION_LEDGER.md` authored and indexed
from `CLAUDE.md`. Settles the per-tile derivation breadcrumb, per-body histograms,
regenerate-on-demand data lifetime, and surfacing (Ledger window + Planetary field
lens), with the shared tile-derivation content builder noted against the hover-card /
Selection overlap.

| ID | Requirement | Verification | Status | Notes |
|----|-------------|--------------|--------|-------|
| R1 | `docs/generation/GENERATION_LEDGER.md` exists and specifies the **per-tile derivation breadcrumb**. | `doc: docs/generation/GENERATION_LEDGER.md` | complete | § What the ledger presents → Per-tile derivation breadcrumb. |
| R2 | It specifies **per-body summaries** (composition / landform histograms, ocean threshold). | `doc:` (§ per-body) | complete | § Per-body summaries. |
| R3 | It settles **persist-vs-regenerate** (deterministic → regenerate on demand). | `doc:` (§ data lifetime) | complete | § Data lifetime — regenerate on demand. |
| R4 | It settles **surfacing** (Ledger window vs. Planetary overlay lens) and notes the hover-card / Selection overlap. | `doc:` (§ surfacing) | complete | § Surfacing. |

### frame-stutter-measure

`Resolved: 2026-06-14 — cancelled. Verification needs live frame-time instrumentation
that does not yet exist; the measurement method is itself the deferred design work.
Intent refined and returned to TODO § Known Bug.`

Reached the design barrier the no-tool policy (§ Verifying when no skill or tool
exists, step 3) describes: classifying the stutter requires observing a live present
loop, which the headless harness cannot do, and building that instrument is non-trivial
design in its own right. Baseline established by code read: vsync is on
(`SDL_SetRenderVSync(m_renderer, 1)`, `app.cpp:77`), there is no frame cap and no
per-frame timing readout. Cancelled rather than left pending; the refined item carries
this baseline and the instrument-first requirement.

| ID | Requirement | Verification | Status | Notes |
|----|-------------|--------------|--------|-------|
| R1 | A repeatable frame-time measurement method exists that classifies the stutter. | `manual` (live window) | failed | No headless tool observes a live present loop; building the live instrument is deferred design. |
| R2 | The hardware-limit config (vsync / frame cap / present mode) is settled against the measurement. | `manual` | failed | Blocked on R1; provisional pre-economy regardless. |

### body-label-stepping

`Resolved: 2026-06-14 — cancelled. Root cause confirmed (R1 complete); the fix and its
temporal verification are deferred. Intent refined and returned to TODO § Known Bug.`

Diagnosis confirmed by code read: the label position derives from the live float `pos`
every frame (`solar_system_canvas.cpp:218–224`, no rounding), so the stepping is
`ImDrawList::AddText` snapping glyphs to the integer pixel grid while the dot
(`AddCircleFilled`) is sub-pixel anti-aliased — the "glyph placement quantisation" path
the item hypothesised, not stale position. The fix (sub-pixel text, or accept and
document) and its smoothness check need live animation over time, which no headless tool
can observe. Cancelled with the finding recorded; R1 kept its real `complete` status.

| ID | Requirement | Verification | Status | Notes |
|----|-------------|--------------|--------|-------|
| R1 | The stepping is confirmed to be `AddText` glyph-grid quantisation (label position from the live float `pos`; dot is sub-pixel AA, text is not). | `code:` (`label_pos` from float `pos`, no rounding) | complete | `solar_system_canvas.cpp:218–224`. |
| R2 | The fix renders labels smoothly (sub-pixel text or accepted-and-documented). | `visual` over time | failed | Temporal/live; no headless tool to observe motion. Deferred to TODO. |

### visual-verification-harness-phase-2

`Resolved: 2026-06-14 — complete, all rows (V7–V12) met.`

Phase 2 of the harness: a shared canvas command vocabulary backing both player keyboard
navigation and the verify API, a reusable Lua helper library that removes per-script pan
math, and promotion of a proven check to a permanent `verifier-visual` skill. Settled in
session: shared command layer (not independent), one general skill (not per-feature),
`center_tile` via a canvas-consumed pending request (no duplicated transform). See DEVLOG
§ "Visual-verification harness (Phase 2)" (2026-06-14).

| ID | Requirement | Verification | Status | Notes |
|----|-------------|--------------|--------|-------|
| V7 | A `canvas_command` enum and `canvas_command_from_name` mapping exist in `src/ui/canvas_command.hpp`, covering descend, ascend, body next/prev, pan up/down/left/right, zoom in/out, and lens next/prev/clear. | `code: enum class canvas_command` + `code: canvas_command_from_name` | complete | Strand A foundation. |
| V8 | `apply_canvas_command` mutates `ui_state` for every command, and `app::process_events` maps the keybinding table (CANVASES.md § Keyboard) to it, guarded by ImGui keyboard capture. The verify API routes `verify.command(name)` through the *same* dispatch. | `code: apply_canvas_command` (process_events + run_verify call sites) + `visual` (a verify script issuing `verify.command` produces the expected navigation captures) | complete | `handle_key_down` (app.cpp) + `verify.command` both call `apply_canvas_command`. Keyboard injection itself is not headlessly exercisable; the shared dispatch it calls is. |
| V9 | `verify.center_tile(col, row[, zoom])` centres the named tile on the Planetary canvas using the canvas's own transform — no pan arithmetic in Lua. | `visual` (refactored `corporation_lens.lua` centres tile (22,82) as in Phase 1) + `code: center_tile` | complete | Verified 2026-06-14: player (22,82) and rival (42,63) tiles centre exactly. Pending-centre request consumed inside `body_surface_canvas`. |
| V10 | `scripts/verify/lib.lua` provides `sweep_overlays(prefix)`, `tour_buildings(zoom)`, and `frame_tile(col, row, zoom)` layered over the low-level verify API. | `visual` (a script requiring the lib runs each helper and produces captures) + `code: function` (the three helpers) | complete | Auto-loaded by the harness from the script's directory (no `require`). |
| V11 | `scripts/verify/corporation_lens.lua` is refactored onto `lib.lua`/`center_tile` with no hand-computed `set_pan` literals, and reproduces the Phase 1 captures. | `visual` (re-run matches Phase 1 R2–R6 evidence) | complete | Re-run 2026-06-14 reproduces R2–R6 captures via `sweep_overlays`/`frame_tile`. |
| V12 | A `verifier-visual` skill exists under `.claude/skills/verifier-visual/` that runs `ProjectIo --verify <script>` for a given script and reports the captures; "authorising" a check is adding/pointing at a `scripts/verify/*.lua`. | `doc: .claude/skills/verifier-visual/SKILL.md` | complete | Single general skill (owner's call), not per-feature. |

### visual-verification-harness (Phase 1)

`Resolved: 2026-06-14 — complete, all rows (V1–V6) met.`

Headless `--verify` capture mode, dependency-free PNG writer, and the `verify` Lua API
that makes the `visual` requirement class runnable without a human at the screen. Full
table predates this policy; see DEVLOG § "Visual-verification harness (Phase 1) +
Corporation lens closed" (2026-06-14) for the V1–V6 outcomes.

### corporation-lens

`Resolved: 2026-06-14 — complete, all 9 rows (R1–R9) met.`

First **cancelled** (4/9 met — R1, R7, R8, R9 — when the visual rows R2–R6 had no runnable
verification tool), then **re-verified and closed** once the visual-verification harness
landed: R2–R6 confirmed via PNG inspection. Full table predates this policy; see DEVLOG
§ "Corporation lens" and § "Visual-verification harness (Phase 1) + Corporation lens
closed" (both 2026-06-14) for the per-row outcomes.
