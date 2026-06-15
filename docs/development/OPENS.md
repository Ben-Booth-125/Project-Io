# Project Io — OPENS

**Opens** are the open items — the backlog of intent **not yet realised in code**.
Each is a **Brief** (the unit of described intent; see `../GLOSSARY.md`): it
**describes the problem or the intended resolution** — a change to make, a feature
to build, or a doc to write — with enough context and file pointers to pick it up
later. Briefs are deliberately deferred while higher-priority work takes precedence.

This file is **design-focused**. A Brief is the *high-level framework* from which
tasks are later cut — it carries the problem, the intended outcome, the design (or a
note that design is still owed), and rough file pointers. It does **not** carry
implementation detail: how to break the work into steps, scope it to files, or
order/parallelise it is all deferred to TASKS promotion (see below). A Brief need
only carry enough to *inform* that later planning.

## Design state — the two open states

Every Brief here is **not yet implemented** (implemented work leaves this file). What
varies is whether its **design is settled**. A Brief is therefore in one of two
states, marked by a glyph in its `[…]` marker:

- **`✓` — designed, not implemented.** The design is settled; the Brief is
  **promote-ready**. What remains is execution: decompose into tasks, scope to files,
  build, verify. (A `✓` Brief may still be *blocked* on a dependency existing — that
  is a sequencing fact, not a design gap.)
- **`~` — not designed, not implemented.** Design is still owed. The Brief names the
  problem and the open questions, but **must be designed before it is promoted**. Doing
  the design *is* the next action — typically a settle-into-the-doc pass, not code.

The glyph rides the priority/difficulty marker: **`[<priority><difficulty> <state>]`**
— e.g. `[B3 ✓]` (designed, promote-ready), `[F4 ~]` (deferred, design owed). The
state is about *readiness to promote*, orthogonal to priority (importance) and
difficulty (size): an urgent Brief can still be `~`, a parked one can be `✓`.

**Design happens here, not mid-flight.** Pausing to settle a `~` Brief's design (into
its authority doc) beats redesigning during a publish — redesign in place is costly
(see CLAUDE.md § Design-direction Q&A and the Batch Publish documentation discipline).
A `~` Brief is the signal that a design pass is owed *before* the work is actionable.

## OPENS vs. TASKS

This file (OPENS.md) holds **described intent**. [`TASKS.md`](TASKS.md) holds the
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

**Only `✓` (designed) Briefs are promotable.** A `~` Brief is designed first — its
design pass settles into the authority doc and flips it to `✓` — and only then is it
promoted. OPENS.md is **entirely forward-facing**: it holds only intent not yet
realised. When work lands, its Brief is **removed** here (the record of what was built
lives in the DEVLOG, not in OPENS) — leaving behind only any genuinely open follow-up
as its own forward Brief. TASKS.md is the transient execution list, cleared as tasks
complete. See [`TASKS.md`](TASKS.md) for the task format.

### Depth verbs — how far to take a Brief

These verbs name *how far* an instruction should carry a Brief, so a request signals
its own effort and nothing ambiguous is read as low-effort. Prefer them over vague
phrasing ("look at", "do", "sort out") when the depth matters:

- **Design** — **design depth only.** Settle a `~` Brief's open questions into its
  authority doc and flip it to `✓`, then **stop**. No tasks, no code. Use when a Brief
  is not yet promotable and the design is the next action.
- **Promote** — **planning depth only.** Break a `✓` Brief into TASKS.md tasks and write
  its REQUIREMENTS.md table, then **stop**. No code is written. Use when the plan
  should be reviewed before execution.
- **Implement (don't commit)** — **code depth.** Promote, then write and build the
  code, but hold the commit. The result is a *code-complete* group (built, not yet
  fully verified or committed; see TASKS.md § Definition of "complete").
- **Publish** — **full depth.** The entire lifecycle below, carried through to a
  committed, verified change. Unless paired with "promote only", **publish always
  means take it all the way to committed code** — promote, complete (implement +
  review + verify), and commit.

When a verb is not given, assume **publish** (full depth) for a single named `✓` Brief
and ask if the scope is large; for a `~` Brief, **design** is the implied first step.
The depth verb is how you override that.

### Publish

**Publish** is the full lifecycle for acting on a Brief:

0. **Brief-spanning requirement (gate — if the Brief changes `src/`).** *Before* a Brief
   that will modify `src/` is decomposed into tasks, write a **brief-spanning requirement**
   in [`req/REQUIREMENTS.md`](req/REQUIREMENTS.md) — one requirement covering the Brief *as a
   whole* rather than a single task. This is **usually a visual-verification requirement** (the
   `visual` class — a `scripts/verify/<feature>.lua` check); for a Brief with no visible
   surface it is the equivalent end-to-end `headless` check. It is the acceptance gate the
   finished Brief is verified against, and it is written first so decomposition (step 1) is
   shaped by how the whole Brief will be proven. Doc-only Briefs are exempt; per-task
   requirements still follow in step 2.
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

#### Publishing multiple Briefs together (Batch Publish — barrier semantics)

Publishing more than one Brief in the same work block is a **Batch Publish** (see
[`../GLOSSARY.md`](../GLOSSARY.md) **Batch Publish**). The five steps
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

#### Batch Publish — documentation-coverage discipline

A Batch Publish **usually executes already-designed (`✓`) Briefs — design is not normally part
of the batch.** Design is settled into the Briefs beforehand; **pausing to design beats
redesigning in place** (redesign is costly). If a broken-up session forces *rapid redesign* of
work already attempted, **refactor the in-flight tasks back into Briefs** (intent returned to
OPENS, as when cancelling a group) and make a clean **second attempt** from the Brief — do not
redesign mid-batch. The step-5 Q&A catches *incidental* calls, not absent up-front design.

A Batch Publish carries a documentation discipline a single-Brief Publish does not. It runs
*around* the five code steps above (the determination up front, the review reminders and Q&A
at the close):

1. **Doc-coverage determination (up front).** For each Brief in the set, decide whether the
   design docs **already record** the implementation it will produce — or whether that
   implementation is a **direct consequence of already-documented behaviour**. Briefs that
   pass need no doc work. Briefs that fail are flagged **doc-changing** and carry the steps
   below. *(This step exists because the >C pass shipped code — e.g. the clustered
   corporation-holdings revision — whose design doc was left stale; the determination is the
   guard against that.)*
2. **Per-Brief documentation collision map.** For each doc-changing Brief, map the **documents**
   it will change — the doc analogue of the source-file collision map (step 3 above). Briefs
   with **disjoint doc scopes are parallel-safe**: **fan out sub-agents to write the doc
   changes** concurrently; Briefs touching the same doc stay sequential. Encourage the fan-out
   wherever the doc scopes are disjoint.
3. **Transient change note per doc.** Every changed doc carries a **minor transient
   "what was changed" note** — a **visible `> ⟳` blockquote** at the point of change (the
   standard form, so the reviewer sees it in any rendered view) recording the edit and that it
   is pending user review. The note is **removed once the user has reviewed** the change (it is
   transient, not permanent doc content).
4. **Standing review reminders (`S`-tier).** The Batch Publish **always adds an `S`-tier Brief
   per changed doc** under § Documentation, so the user is reminded to review each doc change.
5. **Design-direction Q&A (proportional).** When the batch made **non-trivial or ambiguous
   design calls**, it closes by raising a **Q&A** clarifying the design direction those calls
   surfaced, recorded with the session in the DEVLOG (see
   `docs/development/DEVELOPMENT_PRACTICES.md` § Design-direction Q&A). Skip it for a batch that
   made no calls worth surfacing — the step is proportional, not mechanical.

#### Commit format for a published Brief

```
<Brief title>

Tasks: <N completed>, <N cancelled>
Requirements: <N completed>, <N pending>, <N failed>
```

- The **title** is the Brief's own title (the bold heading text, without the
  `[<priority><difficulty> <state>]` marker).
- The **Tasks line** counts completed and cancelled tasks from the TASKS.md group;
  omit the cancelled count if zero.
- The **Requirements line** counts by final state: *completed* (all verifications
  passed), *pending* (deferred — verification not yet run), *failed* (verification
  did not pass and the task was cancelled rather than fixed). Omit pending/failed
  counts if zero.
- No further body text is required unless an individual task warrants a note.

### Priority and difficulty

Every Brief carries a **`[<priority><difficulty> <state>]`** marker — e.g. `[A3 ✓]`,
`[SSS2 ✓]`, `[F4 ~]`. The leading letter(s) are the **priority** (importance); the
trailing digit is the **difficulty** (a rough time estimate); the glyph is the
**design state** (§ Design state above). Priority and difficulty are independent: a
trivial Brief can be urgent, and a large one can be parked.

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

- **[B3 ✓] Implement the Resource lens render pass.** The lens *design* is now settled
  (`docs/ui/LENSES.md` § Resource lens, 2026-06-15) and the glyph exists
  (`ui::icons::resource`). What remains is the **functional lens**: add
  `overlay_mode::resource` to `src/ui/ui_state.hpp`; a strip button (calling
  `icons::resource`) + `overlay_mode_name` entry in `src/ui/overlay.cpp`; and the guarded
  Planetary render pass in `src/ui/body_surface_canvas.cpp` — **highest-value mode** (tint each
  tile by its richest deposit's identity colour, opacity by magnitude, via
  `presentation_of(res).colour`) and a **single-resource mode** (player-picked resource → density
  heatmap) with a lens-local resource selector and the on-canvas gradient colour key the design
  specifies. Data is already present (tile `resource_deposit`). Authority `docs/ui/LENSES.md`.

- **[F3 ~] Visual-verification harness — golden-image diffing (deferred).** Phase 1 outputs
  PNGs for human/Claude inspection only. A later iteration could add committed reference
  images + a pixel-tolerance diff for automatic pass/fail on the `visual` class. **Design owed:**
  golden storage, a tolerance model (anti-aliasing / font jitter), and a CI decision.
  Builds on `write_png_rgba` (`src/core/png_writer.cpp`) and `app::run_verify`.

- **[C1 ✓] Corporation lens player-tile border is redundant.** Found during the 2026-06-14
  visual verification: under the corporation lens the player's tile is filled
  `faction_colour(0)` *and* outlined `faction_colour(0)`, so the border is invisible
  against its own fill (R5 still holds via the distinct fill colour, but the border adds
  nothing). Recolour the player border for contrast — e.g. `palette::selection` (white) or
  the dark outline — so the player's holdings pop against both their own fill and rivals.
  Touches the corporation branch in `src/ui/body_surface_canvas.cpp`; update
  `docs/ui/LENSES.md` § Corporation lens to match.

- **[C2 ✓] Resolve icon silhouette collisions & contract mismatch.** **Design settled
  (2026-06-15) in `docs/ui/ICONS.md` § Open clarifications 1–2:** (a) redraw the
  **extraction-site** marker to a distinct **faceted ore/mineral silhouette** (off the
  gem-diamond pip); (b) redraw **unit/convoy** as a **true open chevron (V)**, separating it from
  the filled `port` triangle *and* matching the header's "chevron" wording. Remaining is the
  `icons.cpp` redraw + header contract sync. Touches `src/ui/icons.{hpp,cpp}`; `docs/ui/ICONS.md`.

- **[C2 ✓] Settle icon outline & colour conventions.** **Design settled (2026-06-15) in
  `docs/ui/ICONS.md` § Shared conventions:** *every canvas-placed filled marker outlines* (so
  `unit` gains the dark outline; the resource pip is the documented outline-less exception), and
  `colour` is **fill** for the building/faction/corporation/unit/pip families, **stroke** for
  supply/market/ledger/placeholder/resource-lens. Remaining is bringing `icons.cpp` into line.
  Touches `src/ui/icons.{hpp,cpp}` and `docs/ui/ICONS.md`. *(Can land with the collision Brief —
  same files.)*

- **[C2 ✓] Verify icon usage is consistent across the app.** Audit every `ui::icons::*` call
  site against `docs/ui/ICONS.md`: that the right glyph is used for each meaning, that sizes
  (the `r` half-extent) and colour sources are consistent within a context, that no two
  glyphs collide in a shared surface, and that the catalogue in ICONS.md matches the actual
  call sites. Produce a short findings list and fix the cheap discrepancies; promote anything
  larger to its own Brief. Call sites today: `body_surface_canvas.cpp` (building markers),
  `overlay.cpp` (lens buttons), `nav_pane.cpp` (ledger/placeholder), `entity_summary.cpp` /
  `tile_inspector.cpp` (resource swatches). Touches whichever call sites drift; reference
  `docs/ui/ICONS.md`.

- **[C2 ✓] Reference distances for bodies are rung-relative.** **Design settled (2026-06-15)** in
  `docs/ui/SOLAR.md` and `docs/ui/CIRCUMPLANETARY.md`: Solar rung → star at 0 AU; Circumplanetary
  rung → parent body at 0 AU (moon reads distance from its parent). Remaining is implementation:
  make `draw_body_summary` (`entity_summary.cpp`) read the reference from the current rung rather
  than hard-coding the star, and apply to any on-canvas distance label. The Circumplanetary hover
  tooltip already does this. Touches `src/ui/entity_summary.cpp`.

- **[B3 ✓] Implement the hover-card primitive.** The hover-card *design* is now settled
  (`docs/ui/TOOLTIP.md`, 2026-06-15): one `draw_hover_card` dispatcher wrapping the existing
  `entity_summary` per-entity builders in `BeginTooltip`/`EndTooltip` (the card is SELECTION.md's
  Focus state — share, don't duplicate, the builders); lightweight title+stat instant, the rich
  "why"-annotated card on dwell. What remains is the **build**: the dispatcher, swapping the three
  ad-hoc tooltip call sites (`body_surface_canvas.cpp`, the Solar / Circumplanetary canvases) to
  it, and cross-referencing `TOOLTIP.md` from `LAYOUT.md` § UI popup elements and `SELECTION.md`.
  Open feel decisions flagged in TOOLTIP.md (reveal delay; rich-by-default vs. dwell; "why"
  verbosity) settle against a populated Layer 4 economy. Supports Layer 4 (building / market detail
  on hover). Authority `docs/ui/TOOLTIP.md`.

- **[C2 ✓] Time-speed curve + econ-tick progress bar.** **Design settled (2026-06-15)** in
  `docs/ui/TIME_CONTROLS.md` § Speed curve. Two implementation tweaks remain:
  — **Non-linear speed curve.** Map the five buttons to **1 → 0.25×, 2 → 0.5×, 3 → 1× (normal-play
    reference), 4 → 4×, 5 → 16×** — slow-motion at the bottom, aggressive fast-forward at the top.
    Lever: the speed→multiplier mapping in `sim_loop.{hpp,cpp}` (`max_speed`, the `step_ms`
    divisor) and the button labels in the `app.cpp` time panel.
  — **Econ-tick progress bar: drop the % text.** Suppress the quarter-progress `ImGui::ProgressBar`
    `xx%` overlay (empty overlay string) in the `app.cpp` time panel so it reads as a clean
    animated fill. See `docs/ui/TIME_CONTROLS.md` / `LAYOUT.md`.

- **[F3 ~] Clarify the time control view.** Deferred. The current two-column time
  panel (calendar block + speed controls) is a prototype-grade layout. **Design owed:** revisit it
  later to settle the production design — what the player needs from the clock at a
  glance (date, quarter/economy-tick countdown, speed), whether to surface the
  economy-tick boundary more explicitly, keyboard shortcuts for pause/speed, and
  how the panel relates to the rest of the shell chrome. See `docs/ui/TIME_CONTROLS.md`
  / `docs/ui/LAYOUT.md`.

## Menu

- **[B3 ✓] Define the menu items from the systems.** **Design settled (2026-06-15).** The
  ten-slot menu set and its **gameplay-loop ordering** are now recorded in `docs/ui/MENU.md`
  § Menu set and ordering (Corporation overview anchor → manage → trade & world → strategy
  clusters). Remaining is **implementation** (promote when the L4 ledger family lands): wire
  slots 2–6 to the [A4] ledgers, move the Tile Ledger from slot 8 to slot 6, and add the
  reserved placeholders + cluster separators. `src/`-changing → brief-spanning requirement at
  promotion. Files: `src/ui/nav_pane.cpp`, `src/ui/icons.{hpp,cpp}` (per-menu glyphs).

- **[B3 ~] Corporation overview dashboard — design.** Slot 1 of the nav rail (`docs/ui/MENU.md`)
  is a new top-level **at-a-glance roll-up** (balance, holdings, alerts) above the per-system
  ledgers. **Design owed before implementation:** what it summarises, how it links into
  the per-system ledgers, and whether it is a floating window or persistent panel. Authority
  `docs/ui/MENU.md`, `docs/ui/LAYOUT.md`; reads `w.player_entity`, the economy report.

- **[C1 ~] Canonical nav-rail ordering rule.** The rail is ordered by *gameplay-loop grouping*
  for now (`docs/ui/MENU.md`). **Design owed:** settle a **canonical, self-documenting ordering
  rule** later (e.g. strict SYSTEMS.md tier order) so the order is principled rather than ad hoc.
  Low priority; doc-only. See `docs/ui/MENU.md` § Open questions.

## Ledger

- **[C2 ✓] Tile Ledger default body.** The Tile Ledger should default its selected
  body to the **current view's main body** — the Circumplanetary view's anchor, or
  the Planetary view's body — rather than the lowest id. The existing default
  ordering is otherwise fine. Touches the body-selector default in
  `src/ui/tile_inspector.cpp` (read `ui_state.active_body` /
  `circumplanetary_anchor`).

- **[A4 ~] Market lens & the ledger family (Economy 2nd pass + Market / Balance /
  Construction ledgers).** Build out the economy's read surfaces now that the Layer 3
  loop runs — the Layer 4 read-surface layer. **Design owed:** likely splits into several
  Briefs at promotion (decompose the family), and the per-rung **Market lens** spec is its
  own undesigned lens Brief; settle both before promoting. Captured here as one family.
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

- **[F4 ~] Buildings overview ledger — design revision needed (deferred).** The earlier
  "tile build ledger / primary build path" framing is **deferred pending a design revision**.
  A nav-rail menu must be a **broad** ledger (see `docs/ui/MENU.md` § Menus are broad ledgers),
  so this menu has to function as an **overview of all the player's buildings** — every
  building across the player's holdings, with type / target / recipe / workforce / output /
  state, filterable; the standing management surface — **not** a per-tile targeted build menu.
  Building on *one* tile is a targeted action reached through the **tile Selection element**
  ([A3] below), which needs no reserved UI. **Design owed before promotion:** settle what the
  overview shows and how it filters/sorts; its relationship to the Construction panel built in
  v0.0.5 (`src/ui/construction_panel.{hpp,cpp}` — likely *becomes* this overview, refit from the
  current scaffold); and how it relates to the Construction Ledger in the [A4] ledger-family
  Brief above (probably the same surface). Authority `docs/ui/{MENU,LAYOUT,SELECTION}.md`,
  `docs/economy/PRODUCTION.md`; reuses `src/world/placement_rules.hpp`.

### Selection info element

Follow-up intent for the Selection info element (design in `docs/ui/SELECTION.md`;
shared per-entity content builders in `entity_summary.{hpp,cpp}`):

- **[C2 ✓] Non-spatial 'go to' routing.** For nation / corporation selections (no
  canvas of their own), 'go to' should open the relevant ledger rather than
  navigate a canvas. The dispatch seam exists (`draw_selection_panel` →
  `focus_on_entity`); add the branches once those entity kinds and their ledgers
  exist (they arrive with the Layer 4 ledger family). *(Designed; blocked on deps —
  promoted then cancelled 2026-06-14, no `nation_ledger` / `corporation_ledger` target
  exists yet.)*

- **[C2 ✓] Canvas hit-testing for buildings / units / markets.** Only bodies and
  tiles are hit-tested on the canvases today; the other kinds are selectable only
  as Tile Ledger rows. Add canvas hit-testing so they can be single-click-selected
  directly (the panel already renders all five kinds). Depends on those entities
  being drawn as selectable canvas markers first. *(Designed; blocked on that
  marker-drawing prerequisite — promoted then cancelled 2026-06-14.)*

- **[F4 ~] Lens-driven selection resolution.** Deferred — **needs documentation before
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

## Documentation

Standing **`S`-tier review reminders** raised by the 2026-06-15 retroactive doc-coverage pass
(the Batch Publish § documentation discipline). Each names a doc reconciled with code that had
landed without a doc update; each carries a **transient "what was changed" note** in the doc
itself. **Review the change, then remove the transient note** and clear the Brief. *(These are
review actions, not design — marked `✓`.)*

- **[S1 ✓] CORPORATION_GENERATION.md — revised holdings shape (specialist, lean).** Pass 3, the
  design principles, and § Open items were revised (2026-06-15) to the settled **specialist /
  lean / cluster-to-nation** shape, deriving from GENERATION_STRATEGY.md. The doc now describes
  the *design target*; the *code* still places the old flat 3–6 cluster until [B4] is promoted.
  Review the revised framing, then remove the transient `> ⟳` note at the top of
  `docs/generation/CORPORATION_GENERATION.md`.

- **[S1 ✓] GENERATION_STRATEGY.md — new generation-overview doc.** New doc (2026-06-15) holding the
  saturated-base / specialist-corporation premise and summarising the `generation/` family;
  registered in CLAUDE.md's doc map. Review it, then remove its transient `> ⟳` note.

- **[S1 ✓] SYSTEMS.md § Cross-cutting notes — specialist-corporation premise stub.** Added a
  cross-cutting note (2026-06-15) recording that nations own the broad economy and corporations
  are specialists, pointing to GENERATION_STRATEGY.md. Review the wording, then remove the
  transient HTML comment in `docs/SYSTEMS.md` § Cross-cutting notes.

- **[S1 ✓] MENU.md — defined menu set & ordering.** Added § Menu set and ordering (2026-06-15):
  the ten-slot set, gameplay-loop grouping, and slot mapping to systems. Review it, then remove
  the transient `> ⟳` note in `docs/ui/MENU.md` § Menu set and ordering.

- **[S1 ✓] POPULATION.md — population centres now in scope + decomposed.** Reconciled the stale
  "deferred for the prototype" framing to v0.0.6 static-MVP scope; added § Generation
  (nation-seeded, habitability-clustered) and § Implementation decomposition (2026-06-15).
  Review, then remove the transient `> ⟳` note at the top of `docs/economy/POPULATION.md`.

- **[S1 ✓] ICONS.md — icon clarifications 1–4 settled.** Recorded the extraction-site redraw,
  unit→chevron, the outline rule, and the fill-vs-stroke per-family rule (2026-06-15). Review,
  then remove the transient `> ⟳` note in `docs/ui/ICONS.md` § Shared conventions.

- **[S1 ✓] SOLAR.md / CIRCUMPLANETARY.md — rung-relative reference distance.** Added the
  rung-relative distance rule to both canvas docs (2026-06-15). Review, then remove the transient
  `> ⟳` note in `docs/ui/CIRCUMPLANETARY.md`.

- **[S1 ✓] TIME_CONTROLS.md — speed curve + progress-bar settled.** Added § Speed curve (non-linear,
  1×=button 3) and the dropped-%-text note (2026-06-15). Review, then remove the transient
  `> ⟳` note in `docs/ui/TIME_CONTROLS.md` § Speed curve.

- **[S1 ✓] Review SELECTION.md — tile "Build here" front door reconcile.** Added § The tile
  element is the build front door, recording the player-construction affordance
  (`construct_building`, placement-gated, affordability-gated) that landed without a doc entry.
  Confirm the framing (targeted build via the tile element vs. the broad nav-rail overview),
  then remove the transient note at the top of `docs/ui/SELECTION.md`.

- **[S1 ✓] Review SYSTEMS.md § Trade — standing sell-orders reconcile.** Added the standing
  sell-order / floor-price sentence (and the deferred-counterparty note) to § Trade. Confirm
  the wording matches the intended market design, then remove the transient HTML comment in
  `docs/SYSTEMS.md` § Trade.

## Trade

The market layer. Per the 2026-06-14 Q&A, **market resolution collapses into Layer 3** and
price resolution has now landed; **inter-body trade stays open**. Markets are a per-body
exchange, **distinct from corp stockpile pools**. Design authority `docs/SYSTEMS.md` § Trade,
`docs/economy/RESOURCES.md`.

- **[B4 ~] Preferential purchasing (choosing counterparties).** Split from the sell-orders
  Brief (the **sell-orders** half landed 2026-06-15: `ui_state.sell_orders` authored from the
  building-management panel, honoured at clearing with a floor price, the auto path yielding
  player-controlled resources — see DEVLOG / REQUIREMENTS § player-sell-orders). What remains is
  letting the buyer **choose counterparties** rather than the flat anonymous auto-buy. **Blocked
  on architecture, design owed:** `clear_markets` is an *anonymous pooled* exchange — supply and
  demand aggregate per resource and clear at one resolved price, with no per-seller matching.
  Preferential purchasing needs a **matched order book** (buyers see and pick sellers), which is a
  clearing restructure that couples to the Market Ledger family (§ Ledger) and to Supply/logistics
  (Layer 5, who you *can* reach). Design the matching model before promoting. Touches
  `src/world/market_clearing.{hpp,cpp}`; authority `docs/SYSTEMS.md` § Trade.

- **[F4 ~] Inter-body / international markets (deferred).** Cross-body price linkage and trade
  between bodies — each body's market currently resolves in isolation. Out of Layer 3/4 scope;
  couples to Supply (Layer 5 logistics / logistical cost) once convoys exist. Design owed when picked up.

## Resources

The resource economy's data and quality work — the substrate Layer 4 sits on. Design
authority: `docs/economy/RESOURCES.md`, `docs/economy/PRODUCTION.md`.

- **[B3 ~] Resource generation — full-set deposit authoring + scarcity.** Generation today
  authors deposits for the seven-resource prototype subset; extend it to the full 23-resource
  set with a plausible distribution and scarcity profile (rare goods rare, ambient goods
  near-universal). **Design owed:** the per-resource distribution + scarcity profile is a design
  call, not just a mechanical extension. Overlaps the tile-generation refinements Brief
  (§ Environment) — that one holds the generation *mechanics*; this one holds the
  *resource-economy* target. Touches the deposit pass in `src/world/tile_generation.cpp`;
  authority `docs/economy/RESOURCES.md`, `docs/generation/TILE_GENERATION.md`.

## Workforce

The labour scalar (`docs/SYSTEMS.md` § Workforce, `docs/economy/POPULATION.md`).

- **[A4 ✓] Workforce pool & population coupling — step 2 (population-derived supply).**
  **Step 1 landed 2026-06-15** (per-`(corp, body)` labour pool with authored supply,
  proportional contention scalar `min(1, supply/demand)` feeding both production and wages;
  `world::workforce_supply` / `workforce_supply_overrides`,
  `economy_report::workforce_contention`, `building_report::effective_workforce`, surfaced in
  the economy panel — see DEVLOG / REQUIREMENTS § workforce-pool). **Remaining (step 2):**
  replace the authored pool supply (`world::default_workforce_supply` / the overrides map) with
  one **derived from population centres** — population level → labour force → the share that
  contracts to the corporation — and let wage *level* track body habitability / population
  pressure. Approach settled in POPULATION.md § Workforce model step 2; couples directly to the
  **[S4] Population centres** Brief (§ Infrastructure): supply is a population output. Touches
  `src/world/economy_system.cpp` (supply derivation), the population data the S4 work adds, and
  `src/world/budget_system.cpp` (wage level). Best taken *with* or *after* S4.

## Infrastructure

- **[S5 ~] Layer 4 — population centres + building management.** The next layer,
  **rescoped** from a pure production-UI overhaul into two coupled systems:
  **population centres** (the deferred `docs/economy/POPULATION.md` model — population
  scale / agglomeration, land-use trade-offs, habitability feedback, and the labour supply
  that grounds workforce) and **building management** — player **construction** (placement +
  build-cost spend + terrain/deposit validation via the placement-rules seam above),
  **recipe / workforce control**, and the **sell-order UI** — surfaced through the production
  UI (§ Canvas) and the market / balance / construction ledgers (§ Ledger). Build cost comes
  from the Lua economy-constants registry. **Umbrella Brief — not directly promotable;** it
  splits into several Briefs at promotion (the **population-centre half is now its own [S4]
  Brief**, and the build UI is being reframed around the per-tile build ledger, § Ledger).
  Depends on the pre-L4 enablers (placement-rules seam, workforce-model design, the
  economy-test harness). See `docs/economy/{PRODUCTION,POPULATION}.md` and the milestone map in
  `docs/development/ROADMAP.md` (v0.0.6 — building management + population).

- **[S4 ✓] Population centres — static MVP (Briefs 1–5).** **Decomposed (2026-06-15);** the
  foundation-first sequence is in `docs/economy/POPULATION.md` § Implementation decomposition.
  The **static-MVP scope** is: **(1) land-use foundation** (`land_use` enum/field on
  `tile_component` + transition rules via `placement_rules`), **(2) population-centre model +
  the nation-seeded, habitability-clustered generation pass** (§ Generation), **(3) population
  demand** into `market_component.demand`, and **(5) agglomeration/scale bonus** on production.
  Brief **(4) workforce supply derivation** is [A4] § Workforce step 2 — it stays there, grounded
  by this. 1→2 is the serial foundation; 3 and 5 are disjoint dependents of 2 (parallel-safe).
  Each is `src/`-changing → brief-spanning requirements at promotion. Grounds the workforce pool
  and is the population half of v0.0.6. Authority `docs/economy/POPULATION.md`, `docs/SYSTEMS.md`
  § Workforce; `docs/development/ROADMAP.md` (v0.0.6).

- **[A4 ✓] Population centres — dynamic half (Briefs 6–7, v0.0.6 follow-up).** Deferred from the
  static MVP above: **(6) habitability aggregate + feedback** (body habitability from urban/amenity
  tiles → workforce efficiency) and **(7) population growth** (habitability/met-demand → level
  change over Ticks). The first indirect feedback loop in the economy. Decomposed in
  POPULATION.md § Implementation decomposition; best taken after the static MVP and the workforce
  pool are live. Authority `docs/economy/POPULATION.md` § Implementation decomposition,
  § Habitability and workforce efficiency.

## Environment

The world-generation layer — terrain, nations, corporations. Design authority:
`docs/generation/{TILE,NATION,CORPORATION}_GENERATION.md`.

### Tile generation (terrain)

- **[B4 ~] Tile generation refinements.** The larger production passes
  noted in `TILE_GENERATION.md` § Deferred: solar-parameter derivation from orbital
  mechanics, smooth (noise-blended) band transitions, tectonic plate-driven
  landforms, full deposit authoring for the non-prototype resources (the *mechanics*
  side of the Resource generation Brief under § Resources), and coastline
  refinement (enclosed seas, archipelagos, lakes). **Design owed:** each pass needs its
  model settled (the orbital-derivation formula, the plate model) before it is buildable.

### Nation generation

Design authority: `docs/generation/NATION_GENERATION.md`.

- **[C2 ✓] Orphan-island assignment (refinement).** The cardinal-adjacency Voronoi
  BFS cannot cross water, so landmasses disconnected from every seed stay
  unclaimed (~12% of Kepler land). Defensible as "unclaimed islands", but if full
  land coverage is wanted, add a post-pass assigning each orphan island component
  to the nearest nation across water. `nation_generation.cpp`.

- **[F5 ~] Deferred — nation behaviour & production passes.** Per NATION_GENERATION.md
  § Open items: the nation *system* (tax, licences, war, infrastructure), the
  sentiment graph, historical fragmentation (exclaves/disputed zones), and
  non-Kepler jurisdiction. Out of prototype scope; design owed when in scope.

### Corporation generation

Design authority: `docs/generation/CORPORATION_GENERATION.md`.

- **[B4 ✓] Revise the corporation starting-holdings shape.** **Design now settled (2026-06-15);
  code revision pending.** The target shape is recorded in CORPORATION_GENERATION.md § Pass 3
  and grounded in the new GENERATION_STRATEGY.md premise (saturated earth-like base; Nation AI
  owns the broad industry; corporations are **specialists**). The settled direction: holdings are
  a **lean, focus-coherent** set (not a flat 3–6 generic spread), **count shaped by focus**, and
  **clustered to the home nation** (the rigid anchor + nearest-tile pack is dropped). **Remaining
  work is execution + tuning:** fix the concrete prototype counts per focus and the within-nation
  cluster tightness, then revise `place_starting_assets` in
  `src/world/corporation_generation.cpp` (retire `k_min_holdings`/`k_max_holdings` flat range)
  and the `world_audit` expectations. `src/`-changing → needs a brief-spanning requirement at
  promotion. Couples to the [S1] doc-review Brief under § Documentation (holds the transient note
  until this lands). Open follow-ons recorded in CORPORATION_GENERATION.md § Open items: building
  tiers/levels, allied-corp/franchise origin, post-WW2 asset-mix grounding, the analytical
  corp-selection/re-roll flow, franchising, nation-seeded privatisation, automated tax, Era-based
  sovereignty, and diplomatic posture. Out of prototype scope.

## Known Bug

- **[B4 ~] Frame stutter / performance + hardware limits unconfigured.** The app
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
  per-frame timing readout. **Design owed:** the blocker is the measurement itself —
  classifying the stutter needs frame-time instrumentation over a *live* present loop,
  which the headless harness cannot observe; building that live instrument is the deferred
  design work (a frame-time readout / log) that must land before R1/R2 can be run.

- **[C3 ~] Body labels move in steps, not smoothly.** Re-logged. The font-oversampling
  pass (`src/ui/fonts.hpp`) improved glyph crispness but did **not** fix the motion
  artefact: body labels visibly advance only every few ticks while the body dot
  glides. **Root cause confirmed (2026-06-14, promoted then cancelled):** the label
  position derives from the live float `pos` every frame
  (`solar_system_canvas.cpp:218–224`, no rounding), so the stepping is
  `ImDrawList::AddText` snapping glyphs to the integer pixel grid while the dot
  (`AddCircleFilled`) is sub-pixel anti-aliased — the glyph-placement-quantisation
  path, not stale position. **Design owed:** the **fix** (sub-pixel text positioning, or
  accept and document the limitation) and its smoothness check remain open — verifying
  smooth motion needs live animation observation, which the headless harness cannot do, so
  a testing method must be settled first. Same code path on the Circumplanetary
  canvas. See `SOLAR.md`.
</content>
