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
  **nine-slot** menu set and its **curated player-facing order** are recorded in `docs/ui/MENU.md`
  § Menu set and ordering (Corporation overview · Budget · Workforce/Population · Research · Market
  Ledger · Construction · Corp. Strategy · Diplomacy · History; Exploration off-rail). Remaining is
  **implementation** (promote when the L4 ledger family lands): wire the slots to their ledgers in
  the curated order, apply the three renames (Budget / Corp. Strategy / History), drop the
  Exploration slot, and add the reserved placeholders. `src/`-changing → brief-spanning requirement
  at promotion. Files: `src/ui/nav_pane.cpp`, `src/ui/icons.{hpp,cpp}` (per-menu glyphs).

- **[B3 ✓] Corporation overview dashboard.** Slot 1 of the nav rail (`docs/ui/MENU.md`) is a new
  top-level **at-a-glance roll-up** above the per-system ledgers. **Design settled (inline,
  2026-06-15):**
  — **What it summarises (four blocks, read-only).** (1) **Money** — running balance (negatives
    red), last-tick net ±/qtr, and the recent-balance sparkline already maintained for the header
    (`app` balance-history buffer); (2) **Holdings** — building count by type/state
    (active / idle / exhausted) and estimated stockpile valuation (player `(corp,body)` pools at
    market price), the same figures the header values; (3) **Production** — top output goods this
    tick and the limiting-input bottleneck count (from `economy_report` / `building_report`);
    (4) **Alerts** — a short derived list (idle/exhausted buildings, negative balance, unmet
    market demand on a player-sold good, workforce contention < 1). It is a *roll-up of figures
    the per-system ledgers own*, not a new data source.
  — **How it links.** Each block is a **launcher**: clicking Money opens the Balance Ledger,
    Holdings/Production the Construction Ledger, Alerts routes to the relevant ledger per alert
    kind (reuses the `focus_on_entity` non-spatial routing seam, § Selection). The dashboard never
    duplicates a ledger's detail — it summarises and hands off.
  — **Form.** A **floating ledger window** like the others (honours *ledgers-start-closed*), using
    the uniform `ledger_chrome` size/anchor. Not a persistent panel — the persistent at-a-glance
    surface is already the header; the dashboard is the openable deep version.
  Files: new `src/ui/corporation_panel.{hpp,cpp}`, nav-rail slot 1 wiring (`nav_pane.cpp`,
  `app.cpp`), per-block glyphs (`icons.{hpp,cpp}`). Reads `w.player_entity`, the economy report,
  the player balance history. `src/`-changing → brief-spanning requirement at promotion.
  *(Design settled inline; authority-doc propagation to `docs/ui/MENU.md` tracked under
  § Documentation.)*

- **[C1 ✓] Canonical nav-rail ordering rule.** **Settled by Q&A (2026-06-15):** the canonical
  order is the **curated player-facing sequence** the user fixed directly, now recorded in
  `docs/ui/MENU.md` § Menu set and ordering — a **nine-slot rail** (Exploration drops off and
  routes to the Explorer surface). Order: Corporation overview · Budget · Workforce/Population ·
  Research · Market Ledger · Construction · Corp. Strategy · Diplomacy · History. It is a
  deliberate workflow order, not strict SYSTEMS.md tier order; the `tier-idx` column in MENU.md
  keeps it auditable against the tier list. Three slots are **renamed with broadened scope** —
  **Budget** (was Balance Ledger → full budget system), **Corp. Strategy** (was Policy → standing
  laws/strategy, possibly goals later), **History** (was Tile Ledger → generation history +
  post-generation advisory) — each scope settled in MENU.md. Doc-only → no `src/` change; settled
  into `docs/ui/MENU.md`.

## Ledger

- **[C2 ✓] Tile Ledger default body.** The Tile Ledger should default its selected
  body to the **current view's main body** — the Circumplanetary view's anchor, or
  the Planetary view's body — rather than the lowest id. The existing default
  ordering is otherwise fine. Touches the body-selector default in
  `src/ui/tile_inspector.cpp` (read `ui_state.active_body` /
  `circumplanetary_anchor`).

**The Layer-4 read-surface family (decomposed 2026-06-15).** The single "Market lens & ledger
family" Brief is now **decomposed into the five discrete Briefs below** — four ledgers plus the
Market lens render pass. They share design conventions, stated once here so each Brief need not
repeat them:

- **Player-focused, with a corp selector.** Every ledger **defaults to the player corporation**
  (`w.player_entity`) and offers a selector to view another corporation's figures.
  Cross-corporation side-by-side comparison is **left open** (a later option, not in these Briefs).
- **Shared substrate.** All build on the presentation / format / icon helpers and the per-entity
  content builders (`entity_summary.{hpp,cpp}` — shared with the Selection element and hover-card
  Briefs; *share, do not duplicate*), the uniform `ledger_chrome` size/anchor, and
  *ledgers-start-closed*. They read the world `(corp,body)` pool map, `market_component`,
  `corporation_component`, and the economy report.
- **Promotion order.** Economy-panel refit is the foundation (the others lift its conventions);
  Market / Balance / Construction ledgers are then disjoint-file dependents (parallel-safe at
  promotion); the Market lens is independent of all four. Each `src/`-changing Brief takes a
  brief-spanning requirement at promotion. *(Design settled inline; propagation to
  `docs/ui/{LENSES,LAYOUT}.md` tracked under § Documentation.)*

- **[A3 ✓] Economy panel — second pass (foundation refit).** The L3 panel
  (`src/ui/economy_panel.{hpp,cpp}`) is a debug dump (every corp, every section). Refit it to the
  family conventions above: player-default with a corp selector, `ledger_chrome` chrome, and the
  shared content builders — so it becomes the convention reference the other three ledgers lift
  from rather than a parallel debug surface. Scope: presentation only, no new economy data. Files:
  `src/ui/economy_panel.{hpp,cpp}`.

- **[A4 ✓] Market Ledger.** Per-body market detail — supply / demand / **resolved price** per
  resource, the player's buys and sells this tick, and the player's standing sell-orders
  (`ui_state.sell_orders`) with their floor prices. The on-screen counterpart of the **Market
  lens**; selecting under the market lens routes here (§ Selection resolution). **Body selector
  defaults to the current view's main body** (the Tile Ledger pattern). Files: new
  `src/ui/market_ledger.{hpp,cpp}`, nav-rail slot 5. Reads `market_component`, the economy report.

- **[A4 ✓] Balance Ledger.** The corporate money loop made legible: income vs. expenditure broken
  down (sales, input purchases, maintenance, wages — the four `apply_budget` flows) and the running
  balance over recent ticks (the same capped history the header sparkline uses). The deep view the
  Corporation dashboard Money block launches into. Files: new `src/ui/balance_ledger.{hpp,cpp}`,
  nav-rail slot 2. Reads `corporation_component.balance`, the budget flows, the balance history.

- **[A4 ✓] Construction Ledger (construction-in-progress only).** **Scope settled by Q&A
  (2026-06-15): there is no broad buildings-overview ledger.** A standalone all-buildings overview
  proved more "good to know" than goal-driving, so building *inventory* is read where the player
  cares about it — **own** buildings in the Corporation dashboard's holdings roll-up ("good for
  me"), **competitors'** buildings in the Market Ledger ("competition") — not in a dedicated slot.
  What this slot *is*: the **active-construction** view — the build queue, with **what is being
  built, its cost, and progress** per the construction system; building on *one* tile stays a
  targeted action through the tile Selection element ([A3] § Selection). **Relationship to the
  v0.0.5 scaffold:** the Construction panel (`src/ui/construction_panel.{hpp,cpp}`) **becomes** this
  in-progress view — refit the scaffold's armed-placement Build section, drop the broad-overview
  table ambition. Files: `src/ui/construction_panel.{hpp,cpp}` (refit/rename), nav-rail slot 6 (per
  the curated order, § Menu). Reads the player assets, `building_report`,
  `src/world/placement_rules.hpp`. *(Settled into `docs/ui/MENU.md`.)*

- **[B3 ✓] Market lens render pass.** Build the **Market lens** (`overlay_mode::market`) to the
  spec already settled in `docs/ui/LENSES.md` § Market lens: **Planetary** per-tile diverging
  warm↔cool price tint for the player-selected good (warm = dear vs. body mean, cool = cheap),
  with the on-canvas gradient key and good-selector (shared in form with the Resource lens's
  picker); plus the **Circumplanetary** per-body price summary strip. Mirrors the Resource-lens
  build (add `overlay_mode::market`, strip button + `overlay_mode_name`, guarded render passes).
  **No data dependency remains** — price resolution landed in v0.0.4. Files:
  `src/ui/ui_state.hpp`, `src/ui/overlay.cpp`, `src/ui/body_surface_canvas.cpp`,
  `src/ui/solar_system_canvas.cpp` (Circumplanetary summary). Authority `docs/ui/LENSES.md`.

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

- **[B4 ✓] Lens-driven selection resolution.** Resolve the entity under the pointer *through the
  active lens* rather than by a fixed kind order. **Design settled (2026-06-15)** — re-rated up
  from F because the ledger family above depends on this routing rule. The two concepts left open
  are now defined:
  — **The kind stack and "lowest valid".** At a pointer position the candidate entities form a
    fixed **specificity stack**, most-specific first: *building → market-listing → tile → body*.
    "Lowest valid" = the **first entity in that order that is both present at the position and
    *valid under the active lens*** (next bullet). "Present" means hit-tested at the position
    (buildings/listings require the marker-drawing prerequisite — the canvas-hit-testing Brief
    below — until then only tile/body are present). The plain canvas (no lens) resolves to the
    **tile** (today's behaviour), with **body** as the fallback when no tile is under the pointer.
  — **The lens defines validity (the resolution table).** The active `overlay_mode` selects which
    stack kinds are valid, and the lowest valid one wins:

    | Lens | Valid kinds (lowest-first) | Resolves to | Routes to |
    |---|---|---|---|
    | none (terrain) | tile → body | tile | Tile Ledger |
    | Corporation | building → tile → body | owning **corporation** of the building/tile | Balance Ledger |
    | Faction | tile → body | owning **nation** of the tile | (nation ledger — deferred) |
    | Resource | tile → body | tile (its **deposit** profile) | Tile Ledger |
    | Market | market-listing → tile → body | **market / listing** | Market Ledger |
    | Supply | building → tile → body | route/stockpile at the tile | (supply view — Layer 5) |

    So the same hover position resolves to a *different* entity per lens (the Corporation lens
    reads a building's owner, the Market lens the tile's market), and the resolving lens **also
    names the ledger the selection routes to** — the same `focus_on_entity` dispatch seam the
    non-spatial 'go to' routing (above) uses. Rows whose target ledger does not exist yet
    (nation, supply) degrade to selecting the underlying tile until that ledger lands.
  Couples the Selection element (Focus state, `docs/ui/SELECTION.md`) to the lens system
  (`docs/ui/LENSES.md`) and the ledger family. **Build-blocked on** the canvas-hit-testing Brief
  (building/listing markers) for the lens rows that need them; the tile/body rows are buildable
  now. Files at promotion: the selection-resolution seam (`draw_selection_panel` / hover dispatch),
  reads `overlay_mode`. *(Design settled inline; propagation to `docs/ui/{SELECTION,LENSES}.md`
  tracked under § Documentation.)*

## Documentation

Standing **`S`-tier review reminders** raised by the 2026-06-15 retroactive doc-coverage pass
(the Batch Publish § documentation discipline). Each names a doc reconciled with code that had
landed without a doc update; each carries a **transient "what was changed" note** in the doc
itself. **Review the change, then remove the transient note** and clear the Brief. *(These are
review actions, not design — marked `✓`.)*

- **[A3 ✓] Propagate the 2026-06-15 v0.1.0 design pass into the authority docs.** The inline
  OPENS designs are being settled into their authority docs. **Most propagated in the 2026-06-15
  design-Q&A pass** (these were reviewed live with the user — no transient `⟳` note needed, per
  `DEVELOPMENT_PRACTICES.md` § Design-direction Q&A). Status:

  | Settled Brief | Authority doc | Status |
  |---|---|---|
  | Corporation overview dashboard; nav-rail ordering (9-slot, renames) | `docs/ui/MENU.md` | **propagated** |
  | Lens-driven selection resolution | `docs/ui/SELECTION.md`, `docs/ui/LENSES.md` | **propagated** |
  | Inter-body markets (convoy-coupled, Selene example) | `docs/SYSTEMS.md` § Trade | **propagated** |
  | Resource generation scarcity (seeded 0–1, v0.2) | `docs/economy/RESOURCES.md`, `docs/generation/TILE_GENERATION.md` | **propagated** |
  | Known-bug fixes (frame-stutter HUD, body-label) | new `docs/development/KNOWN_BUGS.md` | **propagated** (relocated out of OPENS) |
  | Market lens render pass | `docs/ui/LENSES.md` | already present (§ Market lens) |
  | Ledger family decomposition | `docs/ui/LAYOUT.md` | **owed** |
  | Preferential purchasing | `docs/SYSTEMS.md` § Trade | **deferred** — under active redesign, left to the owning agent |
  | Supply routing (Layer 5) | `docs/SYSTEMS.md` § Supply + new `docs/economy/SUPPLY.md` | **owed** |

  Remaining owed: the LAYOUT.md ledger-family note and the Supply/Layer-5 § Supply + `SUPPLY.md`
  settle. Preferential purchasing is intentionally left for the agent reworking its clearing model.

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

_(The MENU.md menu-set and SYSTEMS.md § Trade review reminders were cleared in the 2026-06-15
design-Q&A pass — both docs were reviewed live with the user and their transient notes removed.)_

## Trade

The market layer. Per the 2026-06-14 Q&A, **market resolution collapses into Layer 3** and
price resolution has now landed; **inter-body trade stays open**. Markets are a per-body
exchange, **distinct from corp stockpile pools**. Design authority `docs/SYSTEMS.md` § Trade,
`docs/economy/RESOURCES.md`.

- **[B4 ✓] Preferential purchasing (choosing counterparties).** Split from the sell-orders Brief
  (the **sell-orders** half landed 2026-06-15). What remains is letting the buyer **choose
  counterparties** rather than the flat anonymous auto-buy. **Design settled (2026-06-15) — the
  matched order-book model:**
  — **From pooled clearing to a matched book.** `clear_markets` today aggregates supply/demand per
    resource and clears at one resolved price with no per-seller identity. Replace the per-resource
    clear with a **per-(body, resource) order book**: sell orders (seller corp, qty, floor price —
    `ui_state.sell_orders` already carries the player's) and buy orders (buyer corp, qty, max
    price). **Matching = price-time priority** — cheapest seller first, then earliest, against the
    highest bidder; trades clear at the seller's ask, advancing toward the same EMA-eased resolved
    price (preserve the v0.0.4 price curve as the *reference/anchor* price, not the clearing
    mechanism). The anonymous auto-buy/auto-sell become **default orders** the AI emits, so the
    book degrades to today's behaviour when no one expresses a preference.
  — **The preference itself.** A buyer's order carries an optional **counterparty preference** — a
    ranked/avoid list of seller corps — applied as a matching bias: a preferred seller wins ties
    and tolerates a configurable price premium; an avoided seller is matched only as last resort.
    Surfaced later through the Market Ledger (§ Ledger) where the player sees and picks sellers.
  — **Order fields + v0.2.0 notes (Q&A 2026-06-15).** Full-order-book scope **confirmed** (the
    original sell-order Brief was ambiguous). Every order carries a **price min/max** *and* the
    counterparty preference above. **Deferred to the v0.2.0 roadmap** (out of prototype scope):
    **corporate contracts** (standing bilateral supply agreements) and **international tariffs**
    (nation-imposed cross-border trade cost).
  — **Couples to Layer 5.** A counterparty is only reachable if logistics can connect buyer and
    seller — so this lands **with or after** the Supply layer (the [S5] Supply-routing Brief
    below): reachability/logistical cost is an input to the matching bias.
  **Build-blocked on Layer 5** (reachability), but the matching model is now settled. Touches
  `src/world/market_clearing.{hpp,cpp}`; authority `docs/SYSTEMS.md` § Trade. *(Design settled
  inline; propagation to `docs/SYSTEMS.md` § Trade tracked under § Documentation.)*

- **[A4 ✓] Inter-body / international markets.** Cross-body price linkage and trade between bodies —
  required by the v0.1.0 done-definition (*price diverges spatially; logistics affects margin*),
  re-rated up from F. **Design settled (2026-06-15):** each body keeps its **own** market and its
  own locally-resolved price (no global clearing) — divergence is the *point*. Bodies are linked
  **only through Supply convoys** ([S5] below): a convoy moving good *g* from body A to body B adds
  its cargo to B's supply at delivery and removed it from A's at dispatch, so prices converge or
  diverge purely as a function of what logistics actually carry, net of logistical cost. There is
  **no abstract price-coupling term** — the convoy *is* the coupling. A profitable arbitrage is
  therefore A-price + per-unit logistical cost < B-price, which the player reads off the two
  bodies' Market Ledgers / the Market lens. **Refined (Q&A 2026-06-15): the coupling is
  market-to-market, not body-to-body** — a convoy links two *markets*, carries a **mode**
  (land / sea / air / space) each gated on its infrastructure, and **space distance is Euclidean,
  centre-to-centre between the markets' parent bodies**. **Build-blocked on Layer 5** (no convoys
  yet); the model is settled. Authority `docs/SYSTEMS.md` § Trade / § Supply.
  — **Prototype scope (2026-06-15) — feasibility probe, not space gameplay.** We are **not scoping
    space gameplay**: no corporation and no nation on Selene (or any off-Earth body). The first build
    is a **minimal probe of the inter-body market logic alone** — the bare market-to-market coupling
    on the existing bodies — explicitly to test whether it is **feasible or whether it triggers
    data-creep** (markets / pools / convoys multiplying per body). Defer the convoy-mode +
    infrastructure richness ([B4 ~] logistics network, § Infrastructure) and any off-Earth faction
    presence until the probe proves the model carries its weight.
  *(Design settled inline; propagation tracked under § Documentation.)*

## Supply

The logistics layer (Layer 5) — the physical movement of goods between bodies. **Newly authored
2026-06-15** to close the v0.1.0 done-definition gap (*goods move between bodies via supply
convoys*); the layer previously had no Brief, only the gated Supply-lens spec and the deferred
inter-body-market note. Design authority `docs/SYSTEMS.md` § Supply, `docs/ui/LENSES.md` § Supply.

- **[S5 ✓] Supply routing — convoys (Layer 5).** The spatial-strategic layer that makes price
  diverge between bodies. **Design settled at prototype depth (2026-06-15); umbrella — splits into
  Briefs at promotion.** The model:
  — **Convoy entity.** A convoy carries `(source market, destination market, mode {land|sea|air|
    space}, cargo {resource, qty}, progress 0–1, speed)` — the coupling is **market-to-market**, and
    the **mode** (each gated on its infrastructure) is settled by the source/destination pair (space
    for inter-body, land/sea/air for intra-body / terrestrial). It is created when goods are
    dispatched from a source pool toward a
    destination market/pool; it advances a fixed fraction of `progress` per Tick (linear, no
    orbital mechanics in the prototype); on arrival it credits the destination `(corp,body)` pool
    / market supply and is retired. Cargo leaves the source pool at **dispatch**, not arrival
    (goods in transit are committed). New component on the world; deterministic per-Tick advance
    alongside the economy step.
  — **Logistical cost.** Each convoy costs **per-unit-distance** budget (a `base_logistics_cost ×
    distance × qty` term from the Lua economy-constants registry; for space convoys **distance is
    Euclidean, body-centre to body-centre** between the markets' parent bodies). The cost is a budget
    outflow at dispatch (or amortised per Tick) — this is
    the term that makes a distant arbitrage marginal and grounds *logistics affects margin*.
  — **Dispatch trigger (Q&A 2026-06-15: auto is the rule, player-direction the exception).** The
    **auto path** is the default — it fills a destination shortfall from the cheapest reachable
    source so the loop runs without micromanagement; standing player-direction of *every* convoy is
    **deprecated** as a baseline. Player-direction remains for a sell order / buy match whose
    counterparty is on another body (couples to the [B4] preferential-purchasing book). **Exception
    (open note):** Era 0 (perhaps Era 1) **space launches / missions MUST be player-directed** —
    leaving the gravity well is an explicit decision, never auto-dispatched; terrestrial
    (land/sea/air) convoys auto-dispatch. Reachability = both bodies known (Exploration is a
    data-model stub in the prototype, so treat all prototype bodies as reachable).
  — **Surfaces.** Unlocks the **Supply lens** (`docs/ui/LENSES.md` § Supply — the one all-rung
    lens: Solar route lines, Circumplanetary throughput badge, Planetary per-tile segment) and the
    inter-body half of the **Market** read surfaces. Folds into the Construction/Market ledgers per
    `docs/ui/MENU.md` (no own nav slot).
  — **Decomposition at promotion (foundation-first):** (1) convoy component + per-Tick advance;
    (2) logistical-cost budget term + economy-constants entries; (3) dispatch triggers (auto, then
    player-directed); (4) destination crediting + inter-body market effect ([A4] inter-body
    markets); (5) Supply lens render passes. 1→2→3→4 serial (shared economy/budget seam); 5
    disjoint. Each `src/`-changing → brief-spanning requirement at promotion.
  Files at promotion: new `src/world/supply_system.{hpp,cpp}`, `src/world/components.hpp` (convoy),
  `src/world/budget_system.cpp` (cost), `scripts/economy.lua` (constants),
  `src/world/market_clearing.{hpp,cpp}` (delivery), the canvas render passes (Supply lens).
  **The largest remaining v0.1.0 build** — a `5`; treat as v0.0.7's whole theme. *(Design settled
  inline at prototype depth; a fuller settle into `docs/SYSTEMS.md` § Supply + a new
  `docs/economy/SUPPLY.md` is owed and tracked under § Documentation.)*

## Resources

The resource economy's data and quality work — the substrate Layer 4 sits on. Design
authority: `docs/economy/RESOURCES.md`, `docs/economy/PRODUCTION.md`.

- **[B3 ✓] Resource generation — full-set deposit authoring + scarcity (direction settled; v0.2).**
  Generation today authors deposits for the seven-resource prototype subset; extend it to the full
  raw-material set with a plausible distribution and scarcity profile. **Design direction settled
  (2026-06-15), scheduled to the v0.2 solar-generation roadmap** (with the tile-generation
  refinement passes, § Environment): a **seeded per-resource rarity scalar** — a decimal in
  `[0, 1]` (0 = trace/absent, 1 = near-universal ambient), **raw-tier resources only** (refined and
  product goods are made, not mined, so carry no deposits). The scalar **modulates deposit
  frequency and magnitude** on top of the existing terrain affinity, so a low scalar keeps a good
  sparse even on affine terrain and a high one approaches the every-tile ambient floor; it is
  **seeded** so a campaign's exact distribution varies while the rarity *ordering* (consistent with
  each good's `RESOURCES.md` base-price rarity) stays stable. This is the *resource-economy* target;
  the generation *mechanics* that consume the scalar are the tile-generation Brief (§ Environment),
  the same v0.2 pass. Settled into `docs/economy/RESOURCES.md` § Deposit rarity & scarcity and
  `docs/generation/TILE_GENERATION.md` § Deferred. Touches the deposit pass in
  `src/world/tile_generation.cpp` (data-only — the deposit arrays already carry full enum width).
  `src/`-changing → brief-spanning requirement (a `headless` deposit-distribution audit) at
  promotion.

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

- **[B4 ~] Logistics network & infrastructure model (design owed).** Surfaced by the 2026-06-15
  Q&A: the convoy-mode model ([S5] Supply, [A4] inter-body markets) makes each convoy mode —
  **land / sea / air / space** — depend on infrastructure, but the infrastructure layer itself is
  **undesigned**. **Open questions to settle:** what a **road** is (a tile attribute? a built
  network? a per-edge capacity term?); whether **sea routes** are implicit (any navigable water
  path) or built (ports + lanes); **airfields**; and the **launchpad / spaceport** that gates the
  **space** mode (already named by the Era-1 gate in `docs/economy/ERAS.md`). Settle how each piece
  of infrastructure **gates and costs** its convoy mode and how it feeds the convoy
  logistical-cost term ([S5]). **Not on the immediate prototype path** — the inter-body market
  feasibility probe ([A4] § Trade) deliberately sidesteps it; this is the design that makes the
  full Supply layer real. Authority `docs/SYSTEMS.md` § Infrastructure / § Supply; couples to the
  [S5] convoy Brief. *(Design owed — a settle-into-the-doc pass before promotion.)*

- **[S5 ✓] Layer 4 — population centres + building management (index).** The next layer,
  **rescoped** from a pure production-UI overhaul into two coupled systems:
  **population centres** (the deferred `docs/economy/POPULATION.md` model — population
  scale / agglomeration, land-use trade-offs, habitability feedback, and the labour supply
  that grounds workforce) and **building management** — player **construction** (placement +
  build-cost spend + terrain/deposit validation via the placement-rules seam above),
  **recipe / workforce control**, and the **sell-order UI** — surfaced through the production
  UI (§ Canvas) and the market / balance / construction ledgers (§ Ledger). Build cost comes
  from the Lua economy-constants registry. **Umbrella Brief — an index, not promotable;** it is
  **now fully decomposed (2026-06-15)** into promote-ready children: the **population half** is the
  [S4 ✓] static-MVP + [A4 ✓] dynamic-half Briefs below; the **workforce pool** is [A4 ✓] § Workforce;
  the **building-management read surfaces** are the decomposed [A3/A4 ✓] ledger family (§ Ledger);
  player **construction** is the Construction/Buildings-overview ledger refit there + the tile
  Selection element ([A3] § Selection). No design is owed at this level — the umbrella stays only
  as the v0.0.6 index. Depends on the pre-L4 enablers (placement-rules seam, workforce-model
  design, the economy-test harness — all landed). See `docs/economy/{PRODUCTION,POPULATION}.md` and
  the milestone map in `docs/development/ROADMAP.md` (v0.0.6 — building management + population).

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

- **[A4 ✓] Building management — functional recipe & workforce control.** The **interaction** half
  of "manage buildings" named in the v0.1.0 done-definition (`docs/development/ROADMAP.md`),
  distinct from the read-only Construction/Buildings-overview Ledger (§ Ledger) which *displays*
  these fields. **Newly authored 2026-06-15** — the done-definition's "recipe and workforce control"
  had no Brief; only the [S5] umbrella index and the construction-panel scaffold's *disabled*
  management stubs (`src/ui/construction_panel.{hpp,cpp}`, v0.0.5) referenced it. **Design settled
  (prototype depth):**
  — **Recipe control.** A processing facility's `recipe` (on `building_component`, today authored at
    generation) becomes player-settable via a **recipe selector** offering the recipes valid for
    that `building_type` (from `recipe_registry`). Setting it writes `building_component.recipe`;
    it takes effect at the next econ Tick. Constraint: only `building_type`-valid recipes; the
    `no_recipe` sentinel idles the facility.
  — **Workforce control.** Add a per-building **workforce target/cap** (new
    `building_component` field, 0..max) the player throttles. It feeds the existing contention
    scalar ([A4] § Workforce) — production and wages both scale by it — so throttling a building
    cuts its wage draw and frees pooled labour for contended neighbours. Target 0 = *idled by the
    player* (a state distinct from input-starved idle and from exhausted).
  — **Decommission (cost model settled, Q&A 2026-06-15).** Remove a building from the corp's
    `assets` and free its tile. Build cost splits into **labour + material**; decommission **refunds
    material** (minus a small pure-loss fraction) and **charges labour** for the teardown. Ripples to
    the build-cost representation — today a single Lua constant per `building_type`; it splits into
    **labour / material components** in `scripts/economy.lua`, which also re-grounds the construction
    spend ([A3] § Selection build front door) on the same two-part cost.
  — **Surface (settled, per menus-are-broad-ledgers).** These are **targeted** per-building
    actions → they live in the **tile Selection element building view** (`SELECTION.md`, the build
    front door), *not* a nav slot. The broad Construction Ledger shows each building's state and
    **links** to this control surface; it does not host the controls itself.
  Files at promotion: `src/world/components.hpp` (workforce-target field), the Selection-element
  building view / a building-management popup, `src/world/economy_system.cpp` (honour recipe +
  target), `src/world/budget_system.cpp` (wages off the target), the construction-panel scaffold
  refit. `src/`-changing → brief-spanning requirement at promotion. The interaction core of v0.0.6
  building management, alongside the [A4] ledger family (read) and [S4] population. *(Design settled
  inline; propagation to `docs/SYSTEMS.md` § Workforce / § Infrastructure, `docs/economy/PRODUCTION.md`,
  and `docs/ui/SELECTION.md` tracked under § Documentation.)*

## Environment

The world-generation layer — terrain, nations, corporations. Design authority:
`docs/generation/{TILE,NATION,CORPORATION}_GENERATION.md`.

### Tile generation (terrain)

- **[F4 ~] Tile generation refinements (deferred — deep models owed).** The larger production
  passes noted in `TILE_GENERATION.md` § Deferred. **Triaged 2026-06-15** — these are *not* part
  of v0.1.0 design completion (they do not advance the prototype's economy loop) and each deserves
  its own focused settle pass:
  — **Full deposit authoring (the data half) is now covered** by the settled [B3 ✓] Resource
    generation Brief (§ Resources) — band table + affinity gates. This Brief keeps only the
    *generation-mechanics* passes below.
  — **Smooth (noise-blended) band transitions** and **coastline refinement** (enclosed seas,
    archipelagos, lakes) — buildable now (no new model needed), but cosmetic; deferred behind the
    economy work.
  — **Solar-parameter derivation from orbital mechanics** and **tectonic plate-driven landforms**
    — **genuinely speculative; design still owed** (the orbital-derivation formula, the plate
    model). These belong to a procedural-campaign milestone *beyond* the prototype, not v0.1.0.
  Kept as one deferred holder; promote individual passes only when a campaign needs them. Touches
  `src/world/tile_generation.cpp`; authority `docs/generation/TILE_GENERATION.md` § Deferred.

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

Known defects have moved out of OPENS into [`KNOWN_BUGS.md`](KNOWN_BUGS.md) — a known bug is a
*reported defect with a settled or owed fix*, not a unit of design intent, so it does not belong in
the Brief backlog. The **frame stutter / performance** entry (with the settled frame-time-HUD
measurement instrument) and the **body-label stepping** entry (accept-and-document with the
step-together mitigation) now live there.
