# Project Io — Backlog (design bodies)

This file holds the **rich design prose** for every backlog item — the problem, the intended
resolution, the settled design (or a note that design is still owed), and file pointers — keyed
by item. It is the **design authority for an open item** while the work is unrealised.

**Metadata authority is [`backlog.json`](backlog.json)** — status, priority, difficulty,
sequencing, and file scope live there and are queryable. The `[priority+difficulty glyph]`
markers in the item headings below are **non-authoritative display history**; `backlog.json`
wins on any disagreement. The glyph maps 1:1 to the JSON `status` (`✓` = `designed`,
`~` = `design-owed`).

**Method authority is [`DELIVERY.md`](DELIVERY.md)** — the Delivery lifecycle, design-state
model, depth verbs, batch semantics, priority/difficulty scales, and the worktree sub-agent
model. The active worklist is [`REFINED.md`](REFINED.md).

## Canvas

*(BL-001 promoted to REFINED.md v0.0.6 batch — body removed.)*

*(BL-002, BL-003, BL-004, BL-005, BL-006, BL-007 promoted to REFINED.md v0.0.6 batch — bodies removed.)*

- **[F3 ✓] Clarify the time control view.** **Design settled (2026-06-15)** in
  `docs/ui/TIME_CONTROLS.md` § Production clock view — the production clock is fixed to what the
  player needs at a glance, in priority order: **where we are** (year+quarter, then month+day);
  **when the economy next resolves** — the econ tick surfaced **explicitly** as a worded
  **days-until-next-quarter countdown** (`Q2 in 47d`) beside the clean overlay-less quarter
  fill ([C2]); and **how fast** (the 1–5 speed curve + pause). **Keyboard shortcuts settled:**
  `Space` toggles pause/resume, `1`–`5` set the multiplier, routed through the shared
  `canvas_command` vocabulary (consistent with `CANVASES.md` § Keyboard). The two-column split is
  a free-to-change implementation detail; the *content* is the requirement. What remains is
  **implementation** (v0.0.8 UI polish): the countdown readout in the `app.cpp` time panel, and the
  `Space`/`1`–`5` bindings through `canvas_command`. Only production-polish open questions remain
  (exact countdown phrasing; whether to also show the absolute next-quarter date — noted in
  TIME_CONTROLS.md § Open questions). `src/`-changing → brief-spanning requirement at promotion.
  See `docs/ui/TIME_CONTROLS.md` / `docs/ui/LAYOUT.md`.

### Further map lenses (lens-ideas Q&A, 2026-06-16)

A brainstorm pass over *what else is informative as a map lens* (Q&A 2026-06-16). Five lens
ideas were accepted, plus one meta item. They extend the lens family in `docs/ui/LENSES.md`
(`overlay_mode` in `src/ui/ui_state.hpp`; render passes in `src/ui/body_surface_canvas.cpp`;
strip controls in `src/ui/overlay.cpp`). Glyph additions propagate to `docs/ui/ICONS.md`
§ Map-lens glyphs.

**Built 2026-06-16:** the **Population** (per-tile habitability tint) and **Scarcity**
(single-resource translucent heatmap) lenses landed as Planetary render passes (see DEVLOG and
`docs/ui/LENSES.md` § Population/Scarcity lens; the population *density* half is deferred with the
population layer). The remaining Briefs below are `~` (design owed) — each needs a settle pass
(rung applicability, legend/key, glyph, interaction) before promotion.

- **[B3 ~] Production / Output lens.** *(Written 2026-06-16, lens-ideas Q&A.)* Read the map as a
  *production-intensity surface*: where value is actually being made. **Intensity metric settled:
  the sell value of all of a tile's building outputs** (output qty × resolved market price,
  summed across the recipe's products) — not raw output count, so a high-value low-volume site
  reads hot and a bulk low-value one cooler. A strip lens (`overlay_mode`), Planetary tint over
  producing tiles, plain terrain where nothing produces; idle/exhausted buildings read cold. No
  data dependency — reads the economy/`building_report` and `market_component` prices already
  present. Supports the Layer-4 goal (siting and reading the production loop). **Design owed:**
  the value→opacity normalisation (per-body, like Resource?), whether idle vs active gets a
  distinct treatment, glyph, legend. Authority `docs/ui/LENSES.md`.

- **[B3 ~] Placement-suitability surface (a lens on *tile selection*, not a strip lens).**
  *(Written 2026-06-16, lens-ideas Q&A.)* **Explicitly *not* an `overlay_mode` strip lens** — it is
  a surface tied to **tile selection / armed construction placement**: for a selected (or armed-to-
  build) building type, tint every tile by validity/affinity — valid terrain highlighted, invalid
  greyed, "thrives here" stronger than "merely allowed". A siting aid answering *where can this go,
  and where is it best*. Reads the terrain enums and `src/world/placement_rules.hpp` already in
  place; couples to the Construction Ledger / armed-placement flow (§ Ledger [A4] Construction).
  **Design owed:** the exact trigger (tile selection vs armed-build mode vs both), the
  valid/affine/invalid colour treatment, where it lives in the canvas-state model given it is *not*
  an `overlay_mode`. Authority `docs/ui/LENSES.md` (note the distinction), `docs/economy/PRODUCTION.md`.

- **[F4 ~] Reach / Logistics-range lens.** *(Written 2026-06-16, lens-ideas Q&A.)* A static
  *reach* surface — how far deployment or supply can extend from ports/launchpads — complementing
  the Supply lens (which shows flow) with a distance/falloff field. **Flagged as needing more
  scoping** (deferred, `F`): the notion of "reach" is underdefined until the Supply/Layer-5 route
  model exists, and partly overlaps it. **Design owed:** what reach means (logistical cost radius?
  deployment range? both), its relationship to the [S5] Supply layer, rung (likely multi-rung like
  Supply), whether it is a distinct lens at all or a Supply sub-mode. Couples to § Supply [S5].
  Authority `docs/ui/LENSES.md` § Supply.

- **[C3 ~] Meta: per-lens Solar / Circumplanetary representation.** *(Written 2026-06-16,
  lens-ideas Q&A.)* The lens system is **Planetary-first** today; the rung-applicability table in
  `docs/ui/LENSES.md` only sketches coarser representations (Supply route lines, Market price
  strip). This meta-Brief is to **consider each lens's implication on the Solar and
  Circumplanetary canvases deliberately** — each one is *very different* and has distinct needs (a
  Resource density has no inter-body surface; Production might roll up per-body; Supply is
  inherently all-rung; Market has a per-body strip). Not a build — a **design sweep** producing,
  per lens, a settled "what (if anything) does this show on the upper two rungs and why". Output is
  an updated rung-applicability table + per-lens rung notes in `docs/ui/LENSES.md`. **Design owed:**
  the sweep itself. Authority `docs/ui/LENSES.md` § Rung applicability,
  `docs/ui/{SOLAR,CIRCUMPLANETARY}.md`.

### v0.0.6 brief intake — lens & legibility (2026-06-16)

A list-pass over noticed lens/UI issues (v0.0.6 brief-intake session, 2026-06-16). Expanded from
the user's notes; design is deliberately **owed** (`~`) — settle each before promoting. They extend
the lens family (`overlay_mode` in `src/ui/ui_state.hpp`; passes in `src/ui/body_surface_canvas.cpp`;
strip in `src/ui/overlay.cpp`); glyph/colour work propagates to `docs/ui/ICONS.md`,
`src/ui/presentation.hpp`. *(Companion non-lens items from the same intake are filed under their own
categories: § Ledger (multiple-markets visibility), § Trade (warm-start pass), § Workforce
(habitability→workforce rule), § Infrastructure (building rules, construction pricing), § Environment
(country rename + generation).)*

- **[B2 ~] Lens strip: single-select, nullable, ordered & renamed.** *(Written 2026-06-16, v0.0.6
  brief intake.)* The lens control strip must be **single-select with a null/none state** (no lens =
  plain terrain) and **default to "Corporation ownership"** at campaign start. Also fixes the strip's
  **lens ordering and display names**. The strip is a **curated subset of useful lenses, not the
  exhaustive set** — some lenses are reached from a menu / the selection element instead (couples to
  the Market-boundary Brief below). **Design owed:** the order, the final labels, the strip-vs-menu
  split. Authority `docs/ui/LENSES.md`.

- **[F3 ~] Supply-routes lens — market-connection graph.** *(Written 2026-06-16, v0.0.6 brief
  intake.)* A lens drawing the **graph of trade connections between markets**: one edge per active
  route, **thickness/colour from the net worth of all trades on it** (thick green = net positive
  value flow, red = negative). User's render note: it *reads* like an infinite loop but **the graph
  is rendered once per frame** — structure the build so edge aggregation is computed once, not
  recomputed per-edge. **Build-blocked on Layer 5 / the order book** (no inter-market trades to graph
  yet); decide whether this is the [S5] Supply lens itself or a distinct "trade-value" mode.
  **Design owed:** edge/value aggregation, the once-per-frame structure, rung applicability.
  Authority `docs/ui/LENSES.md` § Supply.

- **[C3 ~] Market lens → market-boundary UI; relocate the price readout.** *(Written 2026-06-16,
  v0.0.6 brief intake.)* Replace the current **market-price** map UI with a **market-boundary** lens
  (tinting each market's catchment — couples to multiple tile-centred markets, § Trade). **Keep the
  price-UI code** — it becomes a lens **accessed via a menu / the selection element**, off the strip.
  **Design owed:** boundary rendering (catchment fill/outline), where the price lens is reached from,
  relationship to the Market Ledger (§ Ledger [A4]). Authority `docs/ui/LENSES.md` § Market lens.

- **[F3 ~] Lens colour-scheme pass (deferred to a colour session).** *(Written 2026-06-16, v0.0.6
  brief intake.)* A board-wide reconsideration of lens colours — some too harsh, some too soft.
  **Deferred to a dedicated colour-scheme design session.** Also in scope: **on-canvas text labels**
  naming items for some lenses (country/market), and a **legend for the Corporation lens**.
  **Design owed:** the whole pass. Authority `docs/ui/LENSES.md`; identity colours in
  `src/ui/presentation.hpp`.

- **[B3 ~] Replace Habitability main-lens with Population / "Opportunity".** *(Written 2026-06-16,
  v0.0.6 brief intake.)* Habitability is a good *field* but not a top-level lens. Replace it on the
  strip with **Population** (more useful), or a hybrid **"Opportunity"** lens — a tile's **max yield**
  evaluated *without* regard to the specific resource or logistics. (The per-tile habitability tint
  already built stays available, just not as a primary strip lens.) Couples to the
  habitability→workforce rule (§ Workforce) and the population layer. **Design owed:** Population vs.
  Opportunity vs. both; what "max yield" means; legend. Authority `docs/ui/LENSES.md` § Population lens.

- **[C3 ~] Scarcity lens — blur to a market-level heatmap.** *(Written 2026-06-16, v0.0.6 brief
  intake.)* The Scarcity lens (built, per-tile translucent heatmap) should read as a **market-level
  field** more than a per-tile one — a **blurred/fuzzy heatmap** that still overlaps tiles enough to
  give tile info. Feasibility question: can the lens be driven by a **fuzzy colour not tied to tile
  boundaries** — e.g. render **spheres / pixelated boundaries** rather than crisp per-tile cells?
  **Design owed (keep open if not feasible):** whether a non-tile-bound fuzzy field is renderable in
  the immediate-mode canvas, the smoothing approach, market-vs-tile weighting. Authority
  `docs/ui/LENSES.md` § Scarcity lens.

- **[C3 ~] Resource-density lens — uniform colour, deposit-shape.** *(Written 2026-06-16, v0.0.6
  brief intake.)* The Resource lens should be **always single-select**, use a **uniform colour** (not
  a per-level gradient), and **render the shape of the deposit** — the **whole contiguous deposit**
  when resource levels are present. Test two treatments: **resource-level as a colour bloom** within
  the contiguous deposit, **and** a flat version that does not track per-tile levels at all.
  **Design owed:** contiguous-deposit detection, bloom vs. flat, the single-colour choice.
  Authority `docs/ui/LENSES.md` (Resource lens stub), `docs/economy/TILES.md`.

- **[C4 ~] Tooltip simplification — lens-specific, "why" not "what".** *(Written 2026-06-16, v0.0.6
  brief intake.)* Tooltips give too much data. Reframe them: **less information, lens-specific**, and
  aimed at **explaining *why* a system behaves as it does** — not at gathering data for quick
  decisions (that is the ledgers' job). **Defer nested tooltips.** Flagged by the user as a **large,
  multi-round, detailed Brief touching every visible entity** — a sweep, not a single edit. Couples to
  the hover-card primitive ([B3 ✓] above) and `docs/ui/TOOLTIP.md`. **Design owed:** the per-entity,
  per-lens content sweep. Authority `docs/ui/TOOLTIP.md`.

## Menu

*(BL-021 promoted to REFINED.md v0.0.6 batch — body removed.)*

*(BL-022 promoted to REFINED.md v0.0.6 batch — body removed.)*

*(BL-023 removed from backlog — stale, already settled into docs/ui/MENU.md.)*

## Ledger

*(BL-024 promoted to REFINED.md v0.0.6 batch — body removed.)*

- **[B3 ~] Surface multiple markets in the economy / market ledgers.** *(Written 2026-06-16, v0.0.6
  brief intake.)* The economy ledger shows **one market table per body** — Kepler reads as a single
  market — so the multiple-tile-centred-markets mechanism (§ Trade [B3]) is **invisible**. Make the
  market read surfaces show **each market on a body distinctly** (a per-market breakdown / selector,
  not one aggregate table); the single-market case should read as "1 of N". *(Partly gated on
  generation seeding >1 centre per body — § Trade [B3] — but the ledger should be ready to show them
  now.)* User flagged this as **vital**. **Design owed:** the per-market table/selector layout.
  Authority the Market Ledger ([A4] below), `docs/SYSTEMS.md` § Trade.

**The Layer-4 read-surface family (decomposed 2026-06-15).** The single "Market lens & ledger
family" Brief is now **decomposed into the discrete Briefs below** — four ledgers (the Market
lens render pass landed 2026-06-16, see § Completed). They share design conventions, stated once
here so each Brief need not repeat them:

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

*(BL-026, BL-027, BL-028, BL-029 promoted to REFINED.md v0.0.6 batch — bodies removed.)*

### Selection info element

Follow-up intent for the Selection info element (design in `docs/ui/SELECTION.md`;
shared per-entity content builders in `entity_summary.{hpp,cpp}`):

*(BL-030 promoted to REFINED.md v0.0.6 batch — body removed.)*

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
  — **Clarification (2026-06-16, v0.0.6 brief intake): each lens must yield a *distinct* selection.**
    The resolution table above already encodes this (a lens picks which entity wins); this note pins
    it as an explicit requirement and adds a second strand — **consider which smaller ledgers** each
    lens's selection can open to give *interesting* per-lens info, not just route to the one big
    ledger. Folds into this Brief; the smaller-ledger catalogue is **design owed**.

## Documentation

*(BL-033 promoted to REFINED.md v0.0.6 batch — body removed.)*

*(BL-033, BL-034 promoted to REFINED.md v0.0.6 batch — bodies removed.)*

## Trade

The market layer. Per the 2026-06-14 Q&A, **market resolution collapses into Layer 3** and
price resolution has now landed; **inter-body trade stays open**. Markets are a per-body
exchange, **distinct from corp stockpile pools**. Design authority `docs/SYSTEMS.md` § Trade,
`docs/economy/RESOURCES.md`.

- **[B3 ~] Economy warm-start pre-generation pass — surface auto-buy / auto-sell.** *(Written
  2026-06-16, v0.0.6 brief intake.)* When the simulation starts, **every economy function that
  currently yields a profit must already be reflected** in the opening state — a pre-generation /
  warm-start settle so the economy opens at (or near) steady state, not mid-transient. It must be
  **legible what is in excess and auto-sold, and what is being auto-bought** at the start. Builds on
  the v0.0.4 warm-start; couples to the order book (below). Fits the v0.0.6 legibility strand.
  **Design owed:** how many settle-ticks, and what the surfaced "auto-sold / auto-bought" readout
  looks like (likely a Balance/Market ledger block). Authority `docs/SYSTEMS.md` § Trade.

- **[B3 ~] Seed multiple market centres from capitals / population.** *(Written 2026-06-16,
  market-centres publish.)* The **mechanism** for multiple tile-centred markets per body landed
  2026-06-16 (market `centre_tile`, nearest-centre catchment routing `market_for_tile`, clearing
  routed per corp holding — see DEVLOG and `docs/SYSTEMS.md` § Trade). What remains is **generation
  seeding more than one centre per body**: the present world still authors **one** market per body
  (the behaviour-preserving degenerate case), so no body yet exercises the multi-market path in the
  live economy. **Design owed (deferred to the population layer):** how many markets a body has and
  what seeds them (capital + population centres?), and — once seeded — the **Market lens** tinting
  each market's catchment rather than the whole body (`docs/ui/LENSES.md` § Market lens). Couples to
  NATION/POPULATION generation (capital/centre placement); touches `src/world/hard_coded_world.cpp`
  (market authoring) and later the generation pass. **Open follow-ups noted at landing:** finer
  per-building supply/demand splitting (today a corp's whole body-aggregate routes to one nearest
  market, not split across catchments), and how convoys move goods *between* intra-body markets
  (couples to Supply / Layer 5 and the [A4] inter-body model). Authority `docs/SYSTEMS.md` § Trade,
  `docs/ui/LENSES.md` § Market lens.

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

- **[B3 ~] Habitability gates max workforce, not production directly.** *(Written 2026-06-16, v0.0.6
  brief intake.)* A population/workforce rule the user likes: **habitability should not scale a
  building's production output directly** — instead it caps the **building's max workforce fraction**.
  Above ~**0.6** habitability → **100% base workforce**; below, a reduced fraction. **Open reference
  note:** *tech will later raise the fraction above 100%* — e.g. habitability 0.99 could eventually
  support **~250%** of base workforce. Couples to the contention scalar ([A4] step 2 below) and the
  building workforce-target control ([S4] § Infrastructure building management); pairs with the
  Habitability→Population/Opportunity lens (§ Canvas). **Design owed:** the habitability→fraction
  curve, the tech-driven over-100% multiplier model. Authority `docs/economy/POPULATION.md`,
  `docs/SYSTEMS.md` § Workforce.

*(BL-042 promoted to REFINED.md v0.0.6 batch — body removed.)*

## Infrastructure

- **[B4 ~] Building rules — stricter placement & a per-building justification process.** *(Written
  2026-06-16, v0.0.6 brief intake.)* Placement is too loose — buildings appear **infinite** and a
  **Port can be placed anywhere**. This Brief defines (a) **stricter per-building placement / limit
  rules** (counts, terrain, adjacency, finite slots — couples to the substrate slot model,
  § Environment), and (b) a **documented process for justifying each building's rules** — a
  per-building table where the user **approves** terrain, limits, and rationale (**user response
  required** as part of resolving this Brief). Also the **building-ledger UI direction**: the
  Construction/building ledger becomes a **secondary window** opening a **larger horizontal,
  cross-canvas panel**; **primary selection on a tile always gives a tile overview** with navigation
  to a **finite set of further ledger options** (perhaps only "building" — left open). Couples to
  `src/world/placement_rules.hpp`, the Construction Ledger ([A4] § Ledger), and the tile Selection
  element. **Design owed:** the per-building rule table + approval pass, the placement limits, the
  ledger-as-secondary-window layout. Authority `docs/economy/PRODUCTION.md`, `docs/ui/SELECTION.md`.

- **[B3 ~] Construction pricing — buildings cost resources (per-building, approved).** *(Written
  2026-06-16, v0.0.6 brief intake.)* Buildings must **cost resources** to construct, set
  **per-building and approved** the same way as the building rules above — a documented, user-approved
  cost table. Couples to the build-cost model already noted in [S4] building management (the
  labour/material split in `scripts/economy.lua`) and the construction spend ([A3] § Selection build
  front door). **Design owed:** the per-building resource/cost table and its approval pass. Authority
  `docs/economy/PRODUCTION.md`, `scripts/economy.lua`.

- **[B4 ✓] Logistics network & infrastructure model.** Surfaced by the 2026-06-15
  Q&A: the convoy-mode model ([S5] Supply, [A4] inter-body markets) makes each convoy mode —
  **land / sea / air / space** — depend on infrastructure, but the infrastructure layer itself was
  **undesigned**. **Design settled (2026-06-15), at feasibility-probe depth** — it positions the
  data model and names the gates/costs, deliberately *not* a full space-logistics build (no space
  gameplay; the [A4] inter-body probe sidesteps all of this but the Era gate).

  **The unifying rule — infrastructure *gates* and *costs* a mode.** Each convoy mode carries two
  infrastructure relationships, both feeding the [S5] convoy model:
  — **Gate.** Mode *M* is available between markets *A* and *B* **iff** *M*'s required endpoint
    infrastructure exists (and, where relevant, operates) at **both** *A* and *B*. The
    source/destination pair selects the mode (per [S5]): inter-body → **space**; intra-body →
    **land** by default, **sea** when the route must cross water, **air** where airfields exist.
  — **Cost.** Each mode carries a `base_logistics_cost` multiplier in the Lua economy-constants
    registry (`scripts/economy.lua`), ordered **land < sea < air < space**, feeding [S5]'s
    `base_logistics_cost × distance × qty` term. Infrastructure *modulates* this (a road tile
    lowers the land term; see below).

  **The four modes, each open question settled:**
  — **Land — a road is a *tile attribute*, and land mode is *ungated*.** Of the three options
    (tile attribute / built network / per-edge capacity), settle on a **tile attribute** — a
    `road_level` on `tile_component` — **not** a graph or per-edge structure (a deliberate
    data-creep guard: it stays inside the existing tile data model with no new topology). Land mode
    is **always available across contiguous land intra-body** with no built prerequisite; a road is
    an **optional cost-reducer** — a road tile multiplies the per-unit-distance land cost *down*
    when a route crosses it. Roads are a **deferred tuning follow-on**: the prototype land mode
    works without them. *(Confirmed by Q&A 2026-06-15.)*

  **Open consideration — the logistic-strength model is not fully settled (Q&A 2026-06-15).** The
  per-mode cost term above is the *prototype* model, but logistics will eventually carry more than
  point-to-point goods convoys: it must also feed **unit supply** and **population supply**. That
  points toward a richer **emanation / cross-section "fuel" model** for the **land / air / sea**
  modes — supply *radiates* from sources and **attenuates across distance and terrain** (a supply
  *field* with a per-tile/per-cross-section strength), rather than only discrete convoy legs — so a
  position's reachable supply is a continuous quantity that thins with distance and is contested
  along its path. **Space is a separate, larger consideration** (the convoy/launch model above
  stands for it). The target *feel* is **Shadow Empire's** logistics (despite the genre/theme/tonal
  difference) — recorded as a durable design-reference note in `docs/SYSTEMS.md` § Supply. This is
  an **open follow-on**, not settled here: the prototype keeps the simple per-mode-cost convoy
  model; the emanation model is the direction to grow toward (it couples to unit supply [Conflict]
  and population supply [Workforce / Population] when those land).
  — **Sea — implicit water path, *gated on the Port building*.** The water route itself is
    **implicit** (any navigable water between the two endpoints — no built lanes, no pathing graph
    in the prototype). Sea mode is **gated on a `Port`** (already in the Era 0 building set,
    `docs/economy/ERAS.md`) at **both** endpoints; the Port is the sea-logistics access node and
    the natural carrier of per-node throughput capacity later.
  — **Air — gated on an *Airfield* (a new, deferred building).** Air mode is gated on an
    **Airfield** building at both endpoints: **fast, low-capacity, high per-unit cost**. The
    Airfield is **not in the prototype building set** — air mode is **designed but unbuilt**, so
    prototype intra-body logistics uses land (and sea, once Ports are placed) only. Adding the
    Airfield building is the open follow-on.
  — **Space — gated by the existing Era-1 infrastructure.** Space mode requires a **`Launchpad`**
    operational at the **origin** (gates leaving the gravity well — Era 0 *build*, Era 1 *operate*,
    per `ERAS.md`) and an **`Orbital Port`** at the **destination** (receiving). The Era 0→1 gate
    (`ERAS.md`) already controls whether any space body is reachable; the space mode adds only the
    endpoint-building requirement. Space launches **MUST be player-directed** (per [S5]) — leaving
    the gravity well is never auto-dispatched.

  **Capacity is deferred.** Per-node throughput cap (a bigger Port / Orbital Port carries more per
  Tick) is the natural infrastructure tuning lever but is **out of prototype scope** — named here so
  the Port/Orbital Port data model positions for it, not built.

  **Prototype reality.** The only infrastructure that actually *gates* in the prototype is the
  **Era-1 space gate** (already built, `ERAS.md`); land is ungated. **Open follow-ons (deferred):**
  the `road_level` tile attribute + land cost-reducer; the **Airfield** building + air mode; and
  per-node throughput **capacity**. **Build-couples to** the [S5] convoy Brief (it consumes the
  per-mode `base_logistics_cost` and the gates) — land with or before [S5]; sea/air/space
  endpoint-gating folds in as the buildings land. Files at promotion: `scripts/economy.lua`
  (per-mode cost constants), `src/world/components.hpp` (`road_level` on `tile_component`, when the
  road follow-on lands), `src/world/placement_rules.hpp` (Airfield, when it lands), the [S5]
  convoy mode-selection/gating seam. Authority `docs/SYSTEMS.md` § Infrastructure / § Supply (the
  two durable `> Open design note` blocks there fold in when this lands); couples to the [S5] convoy
  Brief. *(Design settled inline; propagation to `docs/SYSTEMS.md` tracked under § Documentation
  when the work lands.)*

*(BL-046, BL-047, BL-048, BL-049 promoted to REFINED.md v0.0.6 batch — bodies removed.)*

## Environment

The world-generation layer — terrain, nations, corporations. Design authority:
`docs/generation/{TILE,NATION,CORPORATION}_GENERATION.md`.

### Cross-cutting

- **[B4 ~] Generate the saturated nation-owned background substrate.** **Raised 2026-06-15** by the
  B4 design-direction Q&A: the user expected starting holdings to read as a *highly saturated* world,
  but the settled premise (`GENERATION_STRATEGY.md` § The economic premise) makes corporations
  **lean specialists** *on purpose* — the saturation is the **Nation AI's broad background
  industry**, explicitly "not the player's playing field… not surfaced as manageable detail." That
  substrate is currently **described but never generated**: nations get territory, resource
  profiles, and political character, but no actual background industrial presence, so the world does
  not yet *feel* saturated. This Brief is that missing mechanism — how the nation-owned substrate is
  represented and generated so the map reads as a saturated earth-like economy without inflating
  corporation holdings (which stay lean per the just-landed B4 revision).

  **Design settled (best-guess primary direction, 2026-06-16 Q&A).** The user chose the direction
  live and invited further ideas; the primary is committed, the speculative parts are open notes
  below. *(Documentation only this session — no code; the substrate stays gated behind the lens
  batch and v0.0.6 work.)*
  — **Form — per-tile industry field + market aggregate.** A scalar **industry/productivity field
    per tile** (nation-owned), aggregated into a **per-(nation, body) economic aggregate** that is
    the market interface. No per-building entities for the background industry — this gives a
    visibly saturated map *and* market depth while sidestepping the per-body entity multiplication
    the inter-body-market probe ([A4], § Trade) is wary of.
  — **Generation — slot/resource-consuming, not free paint.** The field is laid down by a
    procedural pass that **consumes shared tile capacity**: a tile exposes a finite number of
    **building-slots** (and draws on its resource/deposit profile), and substrate industry occupies
    them like any holding. This makes saturation a *real, shared budget* rather than a cosmetic
    tint — and unifies cleanly with the competitive choice below (player displacement = reclaiming
    occupied slots).
  — **Leading generation approach (open) — population-seeded ripple.** The user's instinct: fold
    substrate into the **population stage** of generation — manufacturing dense at population centres,
    **rippling outward (stronger → weaker)** with distance. Recorded as the leading approach; whether
    it is a population sub-pass or a standalone substrate pass is left open (see open notes).
  — **Market coupling — both supply and demand.** The aggregate injects **both** background
    production and background consumption into the per-body markets, giving them liquidity (the
    player has both substrate buyers to sell to and substrate sellers to buy from).
  — **Dynamic, not static.** The substrate **evolves over Ticks**: background industry grows into
    **unsaturated, resource-available** tiles and is **gated by resource discovery & research**
    (it does not pile onto already-saturated tiles — "building where tiles are saturated is bad").
    The exact growth cadence/rules are an open note.
  — **Player interaction — competitive (displaceable).** The player can **out-compete / buy out**
    substrate-occupied slots on a tile, converting background capacity into managed holdings. (Watch:
    keep this *reclaiming slots*, not turning the substrate into individually-managed detail the
    premise rules out.)
  — **Visibility — map-lens overlay.** Surfaces as an **industry-density / productivity lens**
    (off by default), not ambient base-map clutter. Best-guess; **the user will personally flag the
    final visual treatment for v0.2.0.**

  **Open notes (residual sub-design — settle before promoting to TASKS):**
  — *Generation home:* population sub-pass (centre-dense, rippling) vs. a standalone substrate pass —
    leaning population-seeded.
  — *Dynamic growth model:* the per-Tick growth cadence and how research / resource-discovery feed
    expansion; interaction with the building-tier open item (`GENERATION_STRATEGY.md` § Open).
  — *Slot/capacity model:* how per-tile building-slots and resource consumption are budgeted, and how
    the **shared** budget is split between substrate and player/AI holdings (the displacement seam).
  — *Lens visual treatment:* final call deferred to v0.2.0 (user to flag).
  — *(Suggested, not yet chosen)* correlate the initial field to **population × deposit profile** so
    industry clusters where workforce and resources coincide — a single generative source the lens,
    the markets, and displacement all read from.

  Couples to NATION_GENERATION / POPULATION (the generation pass), the economy/market layer
  ([A4] inter-body markets, § Trade), and the deferred building-tier item. Authority
  `docs/generation/GENERATION_STRATEGY.md` § The economic premise + § Open / cross-cutting.
  *(Primary direction settled; residual sub-design owed — settle the open notes before promoting.)*

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

- **[C2 ~] Rename "Faction" → "Country" (doc- and code-wide).** *(Written 2026-06-16, v0.0.6 brief
  intake.)* Rename the **Faction presence** lens to **Countries**, and propagate a **doc-wide and
  code-wide rename** of *faction* → *country* where it refers to nations (the lens label, GLOSSARY
  term, LENSES/SELECTION wording). **Caveat to resolve:** `faction_colour()` is currently the
  **corporation** identity colour (player vs. rival), *not* the nation lens — so the rename must
  disentangle "faction-as-nation" from "faction-as-corp-colour" rather than blanket-replace. **Design
  owed:** the exact term boundary (nation / country vs. corp colour) and the rename sweep list.
  Authority `docs/GLOSSARY.md`, `docs/ui/LENSES.md`, `src/ui/presentation.hpp`.

- **[B4 ~] Country generation — more countries, generated "in history".** *(Written 2026-06-16,
  v0.0.6 brief intake.)* The world needs **more countries** than today's nation generation produces,
  generated with a sense of **history** rather than a flat Voronoi partition. For v0.1.0 the target
  is **clustering seeds — some larger, some smaller countries** (a varied size distribution, not
  uniform territories). Couples to NATION_GENERATION (Voronoi BFS today) and the saturated-substrate
  Brief (§ Environment cross-cutting). **Design owed:** the clustered-seed sizing model, what
  "generated in history" means at prototype depth, the count target. Authority
  `docs/generation/NATION_GENERATION.md`.

- **[F5 ~] Deferred — nation behaviour & production passes.** Per NATION_GENERATION.md
  § Open items: the nation *system* (tax, licences, war, infrastructure), the
  sentiment graph, historical fragmentation (exclaves/disputed zones), and
  non-Kepler jurisdiction. Out of prototype scope; design owed when in scope.

### Corporation generation

Design authority: `docs/generation/CORPORATION_GENERATION.md`.

Open follow-ons recorded in CORPORATION_GENERATION.md § Open items (out of prototype scope):
building tiers/levels, allied-corp/franchise origin, post-WW2 asset-mix grounding, the analytical
corp-selection/re-roll flow, franchising, nation-seeded privatisation, automated tax, Era-based
sovereignty, and diplomatic posture. *(The lean focus-shaped starting-holdings revision [B4]
landed 2026-06-15 — see DEVLOG and the CORPORATION_GENERATION § Pass 3 doc change.)*

## Known Bug

Known defects have moved out of OPENS into [`KNOWN_BUGS.md`](KNOWN_BUGS.md) — a known bug is a
*reported defect with a settled or owed fix*, not a unit of design intent, so it does not belong in
the Brief backlog. The **frame stutter / performance** entry (with the settled frame-time-HUD
measurement instrument) and the **body-label stepping** entry (accept-and-document with the
step-together mitigation) now live there.
