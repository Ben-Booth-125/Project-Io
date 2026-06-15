# Project Io — TODO

Parked thoughts and **described additions** — recorded but not yet actioned, and
not yet committed designs. Each **Brief** (the unit of described intent; see
`../GLOSSARY.md`) **describes the problem or the intended resolution** — a change
to make, a feature to build, or a doc to write — with enough context and file
pointers to pick it up later. Briefs are deliberately deferred while
higher-priority work takes precedence.

A Brief does **not** carry implementation detail: how to break the work into
steps, scope it to files, or order/parallelise it is all deferred to TASKS
promotion (see below). A Brief need only carry enough — the problem, the
intended outcome, and rough file pointers — to *inform* that later planning.

## TODO vs. TASKS

This file (TODO.md) holds **described intent**. [`TASKS.md`](TASKS.md) holds the
**active, prioritised, actionable worklist** — the concrete, file-scoped,
dependency-marked A–F breakdown we execute against.

The workflow is one-directional. When we decide to act on a Brief, we
**promote** it into TASKS.md by breaking it into ordered, individually-scopable
tasks. Promotion is where we do the planning a Brief deliberately omits:

- **Decompose** the intent into the smallest independently-buildable steps
  (foundation first, then dependants).
- **Scope each step to its files**, so collisions between steps are visible.
- **Mark dependencies and parallelisation** — which steps are independent roots
  that can run concurrently (potentially as parallel sub-agents on disjoint
  files), and which must wait. Steps that edit the same files stay sequential.

TODO.md is **entirely forward-facing**: it holds only intent not yet realised. When
work lands, its Brief is **removed** here (the record of what was built lives in the
DEVLOG, not in TODO) — leaving behind only any genuinely open follow-up as its own
forward Brief. TASKS.md is the transient execution list, cleared as tasks complete.
See [`TASKS.md`](TASKS.md) for the task format.

### Depth verbs — how far to take a Brief

These verbs name *how far* an instruction should carry a Brief, so a request signals
its own effort and nothing ambiguous is read as low-effort. Prefer them over vague
phrasing ("look at", "do", "sort out") when the depth matters:

- **Promote** — **planning depth only.** Break the Brief into TASKS.md tasks and write
  its REQUIREMENTS.md table, then **stop**. No code is written. Use when the plan
  should be reviewed before execution.
- **Implement (don't commit)** — **code depth.** Promote, then write and build the
  code, but hold the commit. The result is a *code-complete* group (built, not yet
  fully verified or committed; see TASKS.md § Definition of "complete").
- **Publish** — **full depth.** The entire lifecycle below, carried through to a
  committed, verified change. Unless paired with "promote only", **publish always
  means take it all the way to committed code** — promote, complete (implement +
  review + verify), and commit.

When a verb is not given, assume **publish** (full depth) for a single named Brief and
ask if the scope is large; the depth verb is how you override that.

### Publish

**Publish** is the full lifecycle for acting on a Brief:

1. **Create tasks** — promote the Brief into TASKS.md: decompose into ordered,
   file-scoped, dependency-marked tasks (see TASKS.md § Task format).
2. **Create requirements** — write or link requirements in
   [`req/REQUIREMENTS.md`](req/REQUIREMENTS.md) for each task group, following the
   requirements policy there.
3. **Check parallel-safety** — build the collision map and resolve any scope collision
   before execution. Tasks with **disjoint file scopes are parallel-safe — fan them out
   to concurrent sub-agents**; only same-file (colliding) tasks stay sequential. This
   step exists to *enable* sub-agent concurrency wherever the file schema is safe.
4. **Complete tasks** — implement, review, and verify each task against its
   requirements (TASKS.md § Definition of "complete"). Tasks may be **cancelled**
   mid-session if they prove out of scope, blocked, or superseded; note the
   cancellation reason alongside the task entry.
5. **Commit** — once all tasks are complete or cancelled, create a single commit
   for the Brief using the format below.

#### Publishing multiple Briefs together (barrier semantics)

When more than one Brief is published in the same work block, the five steps
above run as **barriers across the entire set**, not Brief-by-Brief. Every Brief in
the set must clear step *N* before **any** Brief begins step *N+1*:

1. **Create tasks** for *all* Briefs — every Brief is promoted into TASKS.md before
   any requirements are written.
2. **Create requirements** for *all* Briefs.
3. **Check parallel-safety** across the *combined* task set — the collision map spans
   every Brief's tasks at once, so cross-Brief file collisions (e.g. two Briefs both
   touching `selection_panel.cpp`) are resolved before execution. Disjoint tasks
   across the whole set are fanned out to concurrent sub-agents.
4. **Complete tasks** — this barrier is the load-bearing one: **all** tasks across
   **all** Briefs must reach a terminal state (complete or cancelled) before the set
   advances. No Brief is committed while another Brief still has a task in flight.
   A blocked or out-of-scope task is *cancelled* (per TASKS.md § Cancelling a task
   group), not left pending — the barrier closes on terminal states, not on success.
5. **Commit** — still **one commit per Brief** (the per-Brief format below), but
   none of these commits is created until the step-4 barrier has closed for the
   whole set. The commits are then made back-to-back, one per Brief.

The rule, stated once: *publish the set breadth-first, not depth-first.* Do not
drive a single Brief end-to-end and then start the next; advance the whole set
through each step together. This keeps the requirement set, the collision map, and
the "everything builds together" guarantee coherent across the Briefs that shipped
in one block.

#### Commit format for a published Brief

```
<Brief title>

Tasks: <N completed>, <N cancelled>
Requirements: <N completed>, <N pending>, <N failed>
```

- The **title** is the Brief's own title (the bold heading text, without the
  `[<priority><difficulty>]` marker).
- The **Tasks line** counts completed and cancelled tasks from the TASKS.md group;
  omit the cancelled count if zero.
- The **Requirements line** counts by final state: *completed* (all verifications
  passed), *pending* (deferred — verification not yet run), *failed* (verification
  did not pass and the task was cancelled rather than fixed). Omit pending/failed
  counts if zero.
- No further body text is required unless an individual task warrants a note.

### Priority and difficulty

Every Brief carries a **`[<priority><difficulty>]`** marker — e.g. `[A3]`, `[SSS2]`,
`[F4]`. The leading letter(s) are the **priority** (importance); the trailing digit is
the **difficulty** (a rough time estimate). The two are independent: a trivial Brief can
be urgent, and a large one can be parked.

**Priority** ranks importance, ascending: **`F` · `C` · `B` · `A` · `S` · `SSS`**.
`F` is **deferred** — parked, out of current scope, revisit later (it replaces the old
difficulty-6 status). `SSS` is **do immediately**. `C → B → A → S` is the ordinary
working gradient from low to high. Re-rate against the **current goal**: a Brief that
advances it ranks higher; fixes to past work and minor future-note tweaks that do not
advance it rank lower, however tidy they would be. *(Goal as of 2026-06-15: getting
Layer 4 — population centres + building management — working.)*

**Difficulty** (1–5) is an approximate *time-to-do*, not a measure of importance. The
scale is **non-linear** — each step is roughly 3–4× the last:

- **1** — **~5 minutes.** A trivial one-file tweak, no design decisions.
- **2** — **~20 minutes.** A small, contained change in one system; light design.
- **3** — **~1 hour.** One subsystem taken end-to-end with verification.
- **4** — **~3 hours.** Multiple files or real design plus new verification.
- **5** — **~12+ hours.** A large multi-part build. **A `5` should normally be broken
  down** into smaller Briefs (or promoted into a multi-group TASKS set) rather than taken
  on whole — treat it as a flag that the intent is still too big to act on directly.

There is no difficulty 6; "deferred" is the **`F`** priority, not a difficulty.

## Categories

Every Brief below sits under exactly one category. The allowed categories are the
**UI** categories — **Canvas**, **Menu**, **Ledger**, **Documentation**,
**Known Bug** — and the **game-system** categories mirroring `docs/SYSTEMS.md`:
**Trade**, **Conflict**, **Budget**, **Resources**, **Supply**,
**Infrastructure**, **Workforce**, **Exploration**, **Environment**, **Research**,
**Policy**, **Diplomacy**. File each new Brief under its category heading, creating
the heading if it is the first Brief for that category. Only categories that
currently hold Briefs appear as sections below.

---

## Canvas

- **[A4] Layer 4 UI groundwork.** Mature the prototype ImGui shell toward what Layer 4's
  building-management interactions need: a **click-to-place construction flow** on the
  Planetary canvas, a **building-management panel** (recipe / workforce / sell-order
  controls), and the wiring of these into the nav rail and ledgers. The current shell is
  explicitly "debugging-grade" (`docs/ui/LAYOUT.md`); this is the interaction/shell pass
  Layer 4 piles onto. Overlaps the ledger family and the menu-items Brief — coordinate at
  promotion. See `docs/ui/{LAYOUT,MENU}.md` and the Layer 4 Brief under § Infrastructure.

- **[B3] Design the lens system (complete the stubs).** `docs/ui/LENSES.md` now
  exists but only the **Corporation** lens section is settled (written alongside that
  lens's implementation); the **Supply / Market / Faction / Resource** sections are
  **stubs** recording current behaviour only. Finish the per-lens design for those
  four. The overlay control strip has four lenses today (`supply`, `market`,
  `faction`, `corporation` — `overlay_mode` in `src/ui/ui_state.hpp`). Settle for
  each remaining lens:

  — **Per-lens specification** (what is shown, on which rung, and at what level of detail):
    the three stubbed existing lenses (`Supply / Market / Faction`) plus the proposed
    **Resource** lens (deposit density: colour tiles by their highest-value deposit, or by a
    player-selected resource, with a gradient legend) — Resource still warrants its own icon
    and entry alongside the current four.

  — **Rung applicability table**: which lenses are meaningful at Solar, Circumplanetary, and
    Planetary. Supply and Market span all three rungs (route lines on Solar, price summaries
    at Circumplanetary, per-tile detail at Planetary); Faction/Corporation/Resource are
    Planetary-first but may have coarser Solar/Circumplanetary representations later.

  — **Icon vocabulary**: one distinct vector glyph per lens. Supply/Market/Faction/
    Corporation glyphs exist and are ratified in ICONS.md; the **Resource** glyph is the
    one still to spec.

  — **Legend format**: how the active lens is labelled on-canvas (chip, strip, or implicit
    via the icon highlight alone), and how a colour key is surfaced when the lens uses a
    palette (nation colours, resource identity colours).

  — **Interaction notes**: whether lenses are Planetary-only or propagate to the minimap;
    whether multiple lenses can be active simultaneously (currently no — single `overlay_mode`
    enum); whether any lens requires data not yet generated (e.g. Resource needs deposit data
    already present; Corporation needs `w.corporations` which already exists).

  Design authority once written: `docs/ui/LENSES.md` (this doc); icon glyphs also propagate
  to `src/ui/icons.hpp`. Refer to `docs/ui/CANVASES.md` for rung descriptions and
  `docs/ui/LAYOUT.md` for the strip's position in the shell.

- **[F3] Visual-verification harness — golden-image diffing (deferred).** Phase 1 outputs
  PNGs for human/Claude inspection only. A later iteration could add committed reference
  images + a pixel-tolerance diff for automatic pass/fail on the `visual` class. Needs:
  golden storage, a tolerance model (anti-aliasing / font jitter), and a CI decision.
  Builds on `write_png_rgba` (`src/core/png_writer.cpp`) and `app::run_verify`.

- **[C1] Corporation lens player-tile border is redundant.** Found during the 2026-06-14
  visual verification: under the corporation lens the player's tile is filled
  `faction_colour(0)` *and* outlined `faction_colour(0)`, so the border is invisible
  against its own fill (R5 still holds via the distinct fill colour, but the border adds
  nothing). Recolour the player border for contrast — e.g. `palette::selection` (white) or
  the dark outline — so the player's holdings pop against both their own fill and rivals.
  Touches the corporation branch in `src/ui/body_surface_canvas.cpp`; update
  `docs/ui/LENSES.md` § Corporation lens to match.

- **[C2] Resolve icon silhouette collisions & contract mismatch.** Two glyphs in
  `src/ui/icons.cpp` share a silhouette with another, distinguished only by colour or
  outline, and one contract is wrong (see `docs/ui/ICONS.md` § Open clarifications 1–2):
  (a) the **extraction-site** building marker and the **resource pip** are both filled
  diamonds; (b) the **port** building marker and the **unit/convoy** marker are both filled
  upward triangles; and the `icons.hpp` doc for `unit` calls it "an upward **chevron**" while
  the code draws a solid triangle. Decide per collision whether to redraw one glyph for a
  distinct silhouette (e.g. make `unit` a true open chevron, separating it from `port`) or
  to accept the overlap because the two never co-occur — then make the header contract and
  the implementation agree. Touches `src/ui/icons.{hpp,cpp}` and `docs/ui/ICONS.md`.

- **[C2] Settle icon outline & colour conventions.** The filled-glyph dark outline is applied
  inconsistently — `building` and `faction` carry it "for contrast on any terrain", but
  `unit` (also canvas-drawn) does not — and the `colour` parameter means *fill* for some
  glyphs and *stroke* for others (see `docs/ui/ICONS.md` § Open clarifications 3–4). Decide a
  rule (e.g. every canvas-placed filled glyph outlines; document the fill-vs-stroke meaning
  per family) and bring the implementations into line with it. Touches
  `src/ui/icons.{hpp,cpp}` and `docs/ui/ICONS.md`.

- **[C2] Verify icon usage is consistent across the app.** Audit every `ui::icons::*` call
  site against `docs/ui/ICONS.md`: that the right glyph is used for each meaning, that sizes
  (the `r` half-extent) and colour sources are consistent within a context, that no two
  glyphs collide in a shared surface, and that the catalogue in ICONS.md matches the actual
  call sites. Produce a short findings list and fix the cheap discrepancies; promote anything
  larger to its own Brief. Call sites today: `body_surface_canvas.cpp` (building markers),
  `overlay.cpp` (lens buttons), `nav_pane.cpp` (ledger/placeholder), `entity_summary.cpp` /
  `tile_inspector.cpp` (resource swatches). Touches whichever call sites drift; reference
  `docs/ui/ICONS.md`.

- **[C2] Reference distances for bodies are rung-relative.** A body's displayed
  distance should be measured from the **reference point of the current rung**, not
  always from the star. On the Solar rung the reference is the star (0 AU at the
  centre, as today); on the **Circumplanetary** rung the reference is the **parent
  body — 0 AU at the parent** — so a moon (or any local body) reads as its distance
  *from its parent*, which is the meaningful figure when that view is framed on the
  parent. Define the per-rung reference and apply it wherever a body distance is
  surfaced (the body stat block — `draw_body_summary` in `entity_summary.cpp` — and
  any on-canvas distance label). See `docs/ui/CIRCUMPLANETARY.md` and
  `docs/ui/SOLAR.md`.

- **[B4] Informative tooltip / hover-card system.** The single most important
  player-communication surface for a grand strategy game. Today there is one
  ad-hoc `ImGui::BeginTooltip` inside the Planetary canvas, plus the lightweight
  `ImGui::SetTooltip` body tooltips on the Solar / Circumplanetary canvases;
  there is no shared rich card. We want a *shared* hover-card primitive with a
  consistent structure — title line (name + type + icon), a short stat block,
  optional sectioned detail, and room for "why" annotations (e.g. how a price or
  yield was derived). It must work for every hoverable thing across all canvases
  and ledgers: bodies, tiles, buildings, markets, and later convoys and routes.
  It can now build on the shared presentation metadata, formatters, and icon
  helpers (`src/ui/presentation.hpp`, `format.hpp`, `icons.hpp`). Decide: a single
  `draw_hover_card(...)` helper vs. per-entity builders; instant vs. delayed
  reveal; how a "rich card" (LAYOUT.md popup elements) differs from the lightweight
  canvas tooltip. Likely earns its own `docs/ui/TOOLTIP.md`. Note the overlap with
  the **Selection info element** (Ledger) — both present per-entity detail; share
  the per-type content builders where it makes sense. Supports Layer 4 (building /
  market detail on hover). See `docs/ui/LAYOUT.md`.

- **[C2] Time-speed curve + econ-tick progress bar.** Two settled tweaks to the time
  column (the time panel in `src/core/app.cpp`; speed→rate mapping in `sim_loop`):
  — **Non-linear speed curve.** The speed buttons currently map linearly to a 1×–5×
    multiplier (`sim_loop::set_speed`, used as the per-step divisor in `step_ms`).
    Redefine the curve so the top end fast-forwards aggressively: buttons **1–3 keep
    their current fine-grained feel with button 3 as the 1× normal-play reference**,
    and buttons **4 → 4× and 5 → 16×** that reference — for skipping quiet quarters to
    the next economy tick. (Confirm at planning what 1 and 2 become if 3 is the 1×
    reference: whether they read as slower-than-realtime, or 3 is merely relabelled
    while 1/2/3 keep today's 1×/2×/3× rates.) Lever: the speed→multiplier mapping in
    `sim_loop.{hpp,cpp}` (`max_speed`, the `step_ms` divisor) and the button labels in
    the `app.cpp` time panel.
  — **Econ-tick progress bar: drop the % text.** The quarter-progress `ImGui::ProgressBar`
    (`app.cpp` time panel, fed by `ui::fmt::quarter_progress`) shows ImGui's default
    `xx%` overlay; suppress it (empty overlay string) so the bar reads as a clean
    animated fill toward the economy-tick boundary. See `docs/ui/TIME_CONTROLS.md` /
    `LAYOUT.md`.

- **[F3] Clarify the time control view.** Deferred. The current two-column time
  panel (calendar block + speed controls) is a prototype-grade layout. Revisit it
  later to settle the production design: what the player needs from the clock at a
  glance (date, quarter/economy-tick countdown, speed), whether to surface the
  economy-tick boundary more explicitly, keyboard shortcuts for pause/speed, and
  how the panel relates to the rest of the shell chrome. See `docs/ui/TIME_CONTROLS.md`
  / `docs/ui/LAYOUT.md`.

## Menu

- **[B3] Define the menu items from the systems.** Work out the important menu items
  driven by the game systems (`docs/SYSTEMS.md`), then **get feedback on the
  intended order before final implementation.** Layer 4 adds building-management and
  ledger menus, so the rail's content matters for it. See `docs/ui/MENU.md`.

## Ledger

- **[B2] Uniform ledger-window chrome.** Bring every ledger window onto a **single
  shared size constant and a single shared spawn-position constant** so the family reads
  as one consistent surface, per the settled principle in `docs/ui/LAYOUT.md` § Uniform
  ledger-window chrome. Today they diverge: the Tile Ledger opens at 820×560
  (`tile_inspector.cpp`) and the Economy panel at 760×620 (`economy_panel.cpp`), at
  different offsets. Introduce the two constants (anchored clear of the profile/header
  chrome, `ImGuiCond_Once`) and point both windows — and every future ledger in the
  **Market / Balance / Construction** family below — at them. Touches
  `src/ui/tile_inspector.cpp`, `src/ui/economy_panel.cpp`, and wherever the shared
  constants live (a small ledger-chrome header). Doc authority: `docs/ui/LAYOUT.md`.

- **[C2] Tile Ledger default body.** The Tile Ledger should default its selected
  body to the **current view's main body** — the Circumplanetary view's anchor, or
  the Planetary view's body — rather than the lowest id. The existing default
  ordering is otherwise fine. Touches the body-selector default in
  `src/ui/tile_inspector.cpp` (read `ui_state.active_body` /
  `circumplanetary_anchor`).

- **[A4] Market lens & the ledger family (Economy 2nd pass + Market / Balance /
  Construction ledgers).** Build out the economy's read surfaces now that the Layer 3
  loop runs — the Layer 4 read-surface layer. Likely splits into several Briefs at
  promotion; captured here as one family.
  — **Economy panel — second pass.** The L3 panel (`src/ui/economy_panel.{hpp,cpp}`) is a
    debug dump (every corp, every section). Refit it to the conventions below.
  — **Market Ledger.** Per-body market detail — supply / demand / price per resource, the
    player's buys and sells this tick, and listings — the on-screen counterpart of the
    **Market lens** (`overlay_mode::market`; its per-rung specification is the lens-design
    Brief under § Canvas). Selecting under the market lens should route to this ledger.
  — **Balance Ledger.** The corporate money loop made legible: income vs. expenditure broken
    down (sales, input purchases, maintenance, wages) and the running balance over recent ticks.
  — **Construction Ledger.** The build/asset view (per the **Construction** terminology and
    the Layer-4 building-construction Brief under § Infrastructure): the player's buildings
    with workforce / recipe / target, output and idle/active state, and — once construction
    exists — what is being built and its cost/progress.
  — **Player-focused, with a corp selector.** Every ledger **defaults to the player
    corporation** (`w.player_entity`) and offers a selector to view another corporation's
    figures. **Cross-corporation comparison (side-by-side) is left open** — note it as a
    later option, not in this Brief.
  Builds on the shared presentation / format / icon helpers and the per-entity content
  builders (overlaps the Selection element and hover-card Briefs — share the builders). Files:
  `src/ui/economy_panel.{hpp,cpp}` + new ledger files; nav-pane slots. Reads the world pool
  map, `market_component`, `corporation_component`, and the economy report. See
  `docs/ui/LENSES.md`, `docs/ui/LAYOUT.md`, and `docs/SYSTEMS.md` (§ Trade / § Budget).

### Selection info element

Follow-up intent for the Selection info element (design in `docs/ui/SELECTION.md`;
shared per-entity content builders in `entity_summary.{hpp,cpp}`):

- **[C2] Non-spatial 'go to' routing.** For nation / corporation selections (no
  canvas of their own), 'go to' should open the relevant ledger rather than
  navigate a canvas. The dispatch seam exists (`draw_selection_panel` →
  `focus_on_entity`); add the branches once those entity kinds and their ledgers
  exist (they arrive with the Layer 4 ledger family). *(Promoted then cancelled
  2026-06-14 — blocked: no `nation_ledger` / `corporation_ledger` target exists yet.)*

- **[C2] Canvas hit-testing for buildings / units / markets.** Only bodies and
  tiles are hit-tested on the canvases today; the other kinds are selectable only
  as Tile Ledger rows. Add canvas hit-testing so they can be single-click-selected
  directly (the panel already renders all five kinds). Depends on those entities
  being drawn as selectable canvas markers first. *(Promoted then cancelled
  2026-06-14 — blocked on that marker-drawing prerequisite.)*

- **[F4] Lens-driven selection resolution.** Deferred — **needs documentation before
  it is actionable.** Upgrade selection so the entity under the pointer is resolved
  *through the active lens* rather than by a fixed kind order. Two concepts must be
  defined first:
  — **"Lowest valid entity" under hover.** At a given pointer position several
    entities overlap (a building *on* a tile *on* a body). Hover should resolve to
    the **lowest (most specific) valid** entity in that stack. "Lowest" and "valid"
    both need a precise definition — the stack order of kinds, and what makes an
    entity a valid hover/selection target at all.
  — **Lens evaluates validity.** The active lens (`overlay_mode`) changes *what is
    valid*: e.g. under the Corporation lens the meaningful target may be the owning
    corporation, under Resource the tile's deposit, under terrain (no lens) the tile
    itself. So the **Selection info element is driven by the hovered entity,
    evaluated against the current lens** — the same hover position can resolve to a
    different entity per lens.
  — **Resolution names the ledger the selection drives.** The lens that resolves the
    hovered entity also chooses which ledger the selection routes to — tying this Brief to
    the **Market lens & ledger family** Brief under § Ledger. E.g. the **Market lens**
    resolves to a market / listing and routes to the **Market Ledger**; the **Corporation
    lens** to a corporation and the **Balance Ledger**; a building (construction view) to the
    **Construction Ledger**; terrain (no lens) to the tile in the Tile Ledger. The 'go to'
    routing for non-spatial kinds (above) is the same dispatch seam.
  This couples the Selection element (Focus state in `docs/ui/SELECTION.md`) to the
  lens system (`docs/ui/LENSES.md`) and the ledger family; the docs need the resolution
  rule written before code. Design authority once written: `SELECTION.md` (hover/Focus
  resolution) and `LENSES.md` (per-lens validity). Overlaps the hover-card Brief and the
  canvas hit-testing Brief above (the marker stack it hit-tests is the same stack this
  resolves through).

## Trade

The market layer. Per the 2026-06-14 Q&A, **market resolution collapses into Layer 3** and
price resolution has now landed; **inter-body trade stays open**. Markets are a per-body
exchange, **distinct from corp stockpile pools**. Design authority `docs/SYSTEMS.md` § Trade,
`docs/economy/RESOURCES.md`.

- **[A3] Player-driven sell orders & preferential purchasing.** Build out the manual
  side of the market the framework hook stubs: player-authored sell orders (what / how much /
  floor price) and **preferential purchasing** (choosing counterparties rather than the flat
  auto-buy). Surfaced in the Layer 4 building-management UI. Depends on market clearing (the
  `sell_order` hook already exists in `market_clearing.{hpp,cpp}`).

- **[F4] Inter-body / international markets (deferred).** Cross-body price linkage and trade
  between bodies — each body's market currently resolves in isolation. Out of Layer 3/4 scope;
  couples to Supply (Layer 5 logistics / logistical cost) once convoys exist.

## Resources

The resource economy's data and quality work — the substrate Layer 4 sits on. Design
authority: `docs/economy/RESOURCES.md`, `docs/economy/PRODUCTION.md`.

- **[S2] Automated economy-tick testing (multi-tick stability).** A headless harness
  (`tools/verify/*.cpp`, per the `verifier-headless` skill) that runs the economy loop —
  `run_economy_step` → `clear_markets` → `apply_budget` — over many ticks (e.g. 50–100) on a
  small fixed world and asserts it stays sane: prices stay within the `[0.25×, 4×]` clamp band
  and do not oscillate, no NaN/Inf, deposits deplete monotonically toward exhaustion, balances
  do not diverge unboundedly. Cheap insurance before Layer 4 builds UI on top of the loop; the
  existing `econ_harness` exercises only a single tick. Name the new harness in the
  `verifier-headless` skill and add its exe to the settings allow-list.

- **[B3] Resource generation — full-set deposit authoring + scarcity.** Generation today
  authors deposits for the seven-resource prototype subset; extend it to the full 23-resource
  set with a plausible distribution and scarcity profile (rare goods rare, ambient goods
  near-universal). Overlaps the tile-generation refinements Brief (§ Environment) — that one
  holds the generation *mechanics*; this one holds the *resource-economy* target. Touches the
  deposit pass in `src/world/tile_generation.cpp`; authority `docs/economy/RESOURCES.md`,
  `docs/generation/TILE_GENERATION.md`.

- **[B2] Resource realism pass.** A design review of the resource list, tiers, recipes, and
  per-body availability in `docs/economy/{RESOURCES,PRODUCTION}.md` for realism and coherence:
  do the production chains make sense, are quantities plausible, are any obvious resources or
  recipes missing or mis-tiered. Design/doc pass; feeds the generation and the economy depth
  Layer 4 operates over.

## Workforce

The labour scalar (`docs/SYSTEMS.md` § Workforce, `docs/economy/POPULATION.md`).

- **[S3] Workforce model design.** Settle the workforce model *before* Layer 4 building
  management exposes it. Today `workforce_assigned` is a flat authored 0–1 constant; L4 needs
  to know what the player actually controls. Design: the corporation-wide (or per-body) labour
  **pool**, contention when building demand exceeds supply, how wages and supply derive from
  population centres (couples to the Layer 4 population work), and what the player *sets* vs.
  what the system *allocates*. Design only — the implementation is the pool-coupling Brief
  below. Authority: `docs/economy/POPULATION.md`, `docs/SYSTEMS.md` § Workforce.

- **[A4] Workforce pool & population coupling.** The real model: a corporation-wide
  (or per-body) labour **pool** with proportional contention when building demands exceed supply,
  replacing the authored constant. Implements the design Brief above and couples to the Layer 4
  population-centre work — workforce supply and wages derive from population. In Layer 3,
  `workforce_assigned` is an authored constant 0–1, read-only, applied as a linear scalar; this
  brief is the upgrade path.

## Infrastructure

- **[SSS2] Extract a reusable placement-rules seam.** Building-placement validation
  (terrain/deposit rules: extraction only on a non-zero deposit of the target type, never on
  ocean, valid terrain per building type) currently lives *inside*
  `corporation_generation.cpp` Pass 3 (`place_starting_asset`). Layer 4 player construction
  needs the exact same check, so pull it into a reusable
  `src/world/placement_rules.{hpp,cpp}` (tile + building_type + target_resource → valid?),
  call it from Pass 3 (no behaviour change there), and headless-test it. Designing this
  decoupled from a screen, *before* L4, avoids a mid-build refactor — the single most useful
  prep for Layer 4. See `docs/economy/PRODUCTION.md` § Extraction (placement rules) and the
  S1 placement audit in the DEVLOG.

- **[S5] Layer 4 — population centres + building management.** The next layer,
  **rescoped** from a pure production-UI overhaul into two coupled systems:
  **population centres** (the deferred `docs/economy/POPULATION.md` model — population
  scale / agglomeration, land-use trade-offs, habitability feedback, and the labour supply
  that grounds workforce) and **building management** — player **construction** (placement +
  build-cost spend + terrain/deposit validation via the placement-rules seam above),
  **recipe / workforce control**, and the **sell-order UI** — surfaced through the production
  UI (§ Canvas) and the market / balance / construction ledgers (§ Ledger). Build cost comes
  from the Lua economy-constants registry. Large and multi-part; splits into several Briefs at
  promotion. Depends on the pre-L4 enablers (placement-rules seam, workforce-model design, the
  economy-test harness). See `docs/economy/{PRODUCTION,POPULATION}.md` and the build sequence
  in `docs/development/INITIAL_INSTRUCTIONS.md` (to be rewritten — see Documentation).

## Environment

The world-generation layer — terrain, nations, corporations. Design authority:
`docs/generation/{TILE,NATION,CORPORATION}_GENERATION.md`.

### Tile generation (terrain)

- **[B4] Tile generation refinements.** The larger production passes
  noted in `TILE_GENERATION.md` § Deferred: solar-parameter derivation from orbital
  mechanics, smooth (noise-blended) band transitions, tectonic plate-driven
  landforms, full deposit authoring for the non-prototype resources (the *mechanics*
  side of the Resource generation Brief under § Resources), and coastline
  refinement (enclosed seas, archipelagos, lakes).

### Nation generation

Design authority: `docs/generation/NATION_GENERATION.md`.

- **[C2] Orphan-island assignment (refinement).** The cardinal-adjacency Voronoi
  BFS cannot cross water, so landmasses disconnected from every seed stay
  unclaimed (~12% of Kepler land). Defensible as "unclaimed islands", but if full
  land coverage is wanted, add a post-pass assigning each orphan island component
  to the nearest nation across water. `nation_generation.cpp`.

- **[F5] Deferred — nation behaviour & production passes.** Per NATION_GENERATION.md
  § Open items: the nation *system* (tax, licences, war, infrastructure), the
  sentiment graph, historical fragmentation (exclaves/disputed zones), and
  non-Kepler jurisdiction. Out of prototype scope.

### Corporation generation

Design authority: `docs/generation/CORPORATION_GENERATION.md`.

- **[C3] Model pre-game profit in corporation generation.** A corporation does not
  appear from nothing — its starting `balance`/`starting_capital` and asset mix should reflect a
  simulated **pre-game operating history** (extraction + processing + trade running for some
  notional period) rather than flat authored values. Raised during the 2026-06-14 Layer 3 Q&A:
  the Layer 3 economy loop now exists, so the same loop can be run forward at generation time to
  seed a plausible opening balance and stockpiles (the cheap two-tick warm-start already lands;
  this is the longer-history version). Touches `src/world/corporation_generation.cpp` (Pass 4
  financial profile) and reuses the economy step. Not blocking Layer 4.

- **[F4] Deferred — corporation selection screen & behaviour.** Per
  CORPORATION_GENERATION.md § Open items: the analytical corp-selection/re-roll
  flow, franchising, nation-seeded privatisation, automated tax, Era-based
  sovereignty, and diplomatic posture. Out of prototype scope.

## Known Bug

- **[B4] Frame stutter / performance + hardware limits unconfigured.** The app
  already **stutters intermittently**. This may be benign for now, but the cause is
  not yet diagnosed and there is no frame-pacing or hardware-limit configuration in
  place (vsync / target frame-rate / present mode, and the per-frame draw budget for
  the dense tile grids — Kepler is 180×84 = 15,120 tiles redrawn each frame, plus
  the upcoming per-tile Faction-lens tint/border pass). Worth headroom before Layer 4's
  denser UI piles on. First **measure** before optimising: is the stutter GPU
  present-driven (vsync/composition), CPU draw-call volume (immediate-mode tile loop), or
  allocation churn per frame? Then settle the hardware-limit config (vsync on/off, frame
  cap, whether to cache static tile geometry / dirty-rect the canvas). Likely touches
  the render/present setup (SDL3 swap / vsync) and the canvas tile-draw loops.
  **Baseline established (2026-06-14, promoted then cancelled):** vsync is on
  (`SDL_SetRenderVSync(m_renderer, 1)`, `app.cpp:77`), with no frame cap and no
  per-frame timing readout. The blocker is the measurement itself — classifying the
  stutter needs frame-time instrumentation over a *live* present loop, which the
  headless harness cannot observe; building that live instrument is the deferred
  design work (a frame-time readout / log) that must land before R1/R2 can be run.

- **[C3] Body labels move in steps, not smoothly.** Re-logged. The font-oversampling
  pass (`src/ui/fonts.hpp`) improved glyph crispness but did **not** fix the motion
  artefact: body labels visibly advance only every few ticks while the body dot
  glides. **Root cause confirmed (2026-06-14, promoted then cancelled):** the label
  position derives from the live float `pos` every frame
  (`solar_system_canvas.cpp:218–224`, no rounding), so the stepping is
  `ImDrawList::AddText` snapping glyphs to the integer pixel grid while the dot
  (`AddCircleFilled`) is sub-pixel anti-aliased — the glyph-placement-quantisation
  path, not stale position. The **fix** (sub-pixel text positioning, or accept and
  document the limitation) and its smoothness check remain open: verifying smooth
  motion needs live animation observation, which the headless harness cannot do, so
  a testing method must be settled first. Same code path on the Circumplanetary
  canvas. See `SOLAR.md`.
