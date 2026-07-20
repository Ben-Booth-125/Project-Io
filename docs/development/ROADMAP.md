# Project Io — Roadmap

The milestone map from the current state through the **v0.1.0** prototype cut and on into the
**expanded prototype** (v0.1.x → v0.3.0). This document is **forward-facing and lean**: it names
the version sequence, the *theme* of each minor, and the done-definition for the prototype cut. It
deliberately does **not** enumerate individual items — that lives in the backlog
([`backlog.json`](backlog.json) metadata + [`BACKLOG.md`](BACKLOG.md) design prose) and the active
worklist [`REFINED.md`](REFINED.md). The roadmap sits *above* both: it says which theme each minor
carries; the backlog and worklist say what work realises it.

**v0.1.0 validates the economy loop only** — its scope and exclusions (Conflict, Research, Policy,
Diplomacy beyond a data-model stub) are owned by
[`../tech/TECH_FOUNDATIONS.md`](../tech/TECH_FOUNDATIONS.md). Everything **past v0.1.0** — the
expanded-prototype milestones below — sits *beyond* that scope by design, and is named here as
**direction, not committed scope**: each theme earns its item-level detail (and its own
done-definition) when it reaches promotion. The development constraints and tone that govern *how*
the work is done live in [`DEVELOPMENT_PRACTICES.md`](DEVELOPMENT_PRACTICES.md); this document owns
only the milestones.

---

## Versioning grain

One **minor version** (`0.0.x`) carries **one coherent theme** — a layer, or a cluster of
enablers, taken end-to-end and cut as a release (see `DEVELOPMENT_PRACTICES.md` § Cutting a
release). This is the grain established by v0.0.3 (the whole world-generation spine in one
bump). A minor is not a fixed quantum of work; it is a theme that reads as one thing.

A minor may prove too large and **split in practice** at promotion into the worklist — that is expected,
not a roadmap failure. The plan below is the *intended* shape; the split point is called out
where one is likely.

---

## Where we are — v0.0.8

*Cut 2026-07-04, theme: **Discovery & intelligence**.* The interactive economy loop is
mechanically complete and now has its missing strategic dimension — information asymmetry:

- **Layer 0–2** — SDL3 + fixed-timestep simulation + economy tick + sol2; the core data
  model; the Solar / Circumplanetary / Planetary canvases with the minimap zoom ladder.
- **World generation (v0.0.3)** — procedural tiles (two-axis terrain), nations (14 Voronoi
  BFS countries with weighted sizes and history-pass merges), and corporations (asset
  placement, financial profile).
- **Layer 3 economy (v0.0.4)** — extraction → processing → per-`(corp, body)` stockpile →
  per-body market with supply/demand **price resolution** → budget; finite **deposit
  depletion**; warm-start; player balance in the header.
- **Layer 4 foundations (v0.0.5)** — placement-rules seam, stability harness, workforce pool
  groundwork, ledger-window chrome; the golden-image visual-verification harness; the full
  **map lens strip** (Corp → Country → Resource → Market → Population → Opportunity →
  Production → Scarcity); multiple market centres per body (tile-centred catchment); world-gen
  fixes (orphan-island claim, lean corp holdings).
- **Improved core-loop (v0.0.6)** — matched price-time **order book**; **saturated substrate**
  (nation-owned background supply/demand filling the world); supply **convoy** routing with
  logistics costs and inter-body price convergence; the Market/Balance/Construction **ledger
  family** + Corp Dashboard; market centre seeding from population centres; warm-start surface.
- **Interactive & legible (v0.0.7)** — player **construction** (placement + resource build
  cost + terrain/deposit validation + slot rules); **recipe and workforce control** per
  building; **population centres** (static MVP + dynamic habitability→workforce feedback);
  **selectable entity markers** (buildings, market centres) with lens-contextual hover cards;
  canvas hit-testing and the Selection panel; the **placement-suitability surface** (armed
  build mode tints affinity tiles); the **lens-driven selection** panel; full **hotkey system**
  with F1 cheat-sheet overlay; balance/market **trend plots** in the Economy panel and Market
  Ledger; app-driven mouse for deterministic verify captures; cross-platform build (Linux
  primary, Windows CI).

The economy is now *interactive and legible*: discovery is gated, competitor intelligence is
appropriately scoped, and the budget is itemised. The gap left is narrower — a handful of open
polish items and the final quality audit. Closing that drives the remaining road to v0.1.0.

**v0.0.8 — Discovery & intelligence (cut 2026-07-04).** Gave the economy its missing strategic
dimension — information asymmetry — across three focused strands, all landed: a **survey
system** (BL-067) gating the tile map and deposit profile behind a player-dispatched action; a
**visibility model** (BL-068) making competitor buildings visible but their production/stockpiles
private, with markets as the public intelligence channel; and **population legibility** (BL-069)
surfacing the already-simulated habitability→workforce feedback so siting decisions are
reasoned, not guessed. The minor also absorbed the **legibility cluster** (BL-083–086 — settlement
markers, the industry-density substrate lens, always-on player presence, ambient opportunity
read), the **discovery-fog completion** (BL-088 persistent trade routes + BL-089 the
route-driven commercial-sphere activity fog, layered independently over the geographic survey
fog), **full budget/debt/per-building profitability legibility** (BL-072/073/074, pulled forward
from v0.0.9), **display options** (BL-076), and the QOL main-menu/campaign-start framing. The
discovery model — both fogs, the visibility rule, and their surfacing — is now landed design;
its authority is [`../ui/DISCOVERY.md`](../ui/DISCOVERY.md), not this roadmap or the backlog.
Per-item detail is in `DEVLOG.md`.

---

## The arc from here

The map no longer ends at the prototype cut. Ben's **2026-07-09 refocus** extends it: **v0.1.0**
still cuts the economy-loop prototype; then the **v0.1.x** band lays groundwork for an *expanded
prototype*, **v0.2.0** refocuses who the player is, and **v0.3.0** brings the political layer and
the filter system online. The v0.0.6–v0.0.8 themes have shipped, and *v0.0.9 — Budget clarity +
polish* was cut 2026-07-05 (in-app system menu BL-070, the ledger/panel legibility bugs
BL-081/082, the corp-emblem glyph family BL-090, one BL-089 deferral; per-item record in
`DEVLOG.md` and § Near-term publish plan).

### v0.1.0 — Quality audit + legibility polish + cut

*Theme: make it read cleanly, prove it holds up, then ship.* The prototype cut carries two strands.
First, a **legibility polish batch** — the 2026-07-08 lens/UI review (BL-133–145, BL-150): a
Country-lens legend, red→green per-tile value marks, containerised text (wrap-or-fit, never
truncate), a tile-detail ledger, a player-only budget view, and the compact time panel. Second,
the **quality audit** — three instruments, then the cut. *(Roads and planetary logistics, once
queued for this cut, now open v0.1.1.)*

**Audit instruments** — no new systems:

- **Frame budget.** Frame-time HUD: last / avg / max ms + 1% lows. Targets: **avg < 8 ms,
  max < 16.7 ms** panning the full Kepler tile grid (15,120 tiles). If max spikes the HUD says
  why — GPU present, draw-call volume, or allocation churn.
- **Econ-tick scaling.** Extend `econ_stability` to print tick time. Target: **well under 1 ms**
  for the prototype world; confirm it does not grow faster than linearly in bodies × corps.
- **Data creep.** Entity/pool/market/convoy counters + RSS memory readout run long (100→1000+
  ticks): counts and memory must **plateau**, not climb. A steady climb on an idle run names the
  unbounded structure.

Plus hygiene: warning-clean build, one-off static-analysis (cppcheck), headless harnesses green.
Final verification pass against the done-definition below, then the Cut.

### v0.1.x — Expanded-prototype groundwork

*Theme: ponder and stub what the expanded prototype will need — design-forward, data-model-first,
no committed systems yet.* Past the cut, the v0.1.x band is where the game's next dimensions get
their first shape: enough design and stubbing that the v0.2.0 refocus and the v0.3.0 political
layer land on positioned ground rather than a greenfield. v0.1.1 is a concrete build minor; the
rest of the band (v0.1.2–v0.1.6) is deliberately design-forward — each now has a placeholder
`design-owed` item (v0.1.2's procedural-generation revisit filed with research direction already
in hand, BL-187) that firms into real design as it is reached.

- **v0.1.1 — Roads & planetary logistics.** BL-146–149: a generated per-nation road lattice with
  A\* cost integration, planetary road rendering + player-placeable infrastructure, cities as free
  logistics hubs (agglomeration discount), and the inland logistics-hub building. The one concrete
  build minor; it also retires the "roads are an invisible tile attribute" concern. *(Closed
  2026-07-20 with the BL-184/185 legibility follow-ons — road-tier legend + reach-fog dimming.)*
- **v0.1.2 — Procedural generation revisit** (**BL-187**, filed 2026-07-20). A deliberate look back
  at the world-generation pipeline (`GENERATION_STRATEGY.md`'s tile/nation/corporation passes),
  taking design inspiration from *Shadow Empire*'s layered-causality approach — geology/tectonics
  driving climate, climate driving terrain and resources, and only then an emergent political-history
  layer on top — rather than each pass authoring its surface output directly. Design-forward: scope
  which layers (if any) fit the prototype budget before promoting implementation. Inserted ahead of
  the Laws/Techs/Military/Politics band below, which each shift one minor later.
- **v0.1.3 — Laws** (**BL-155**, **BL-186**). First pass at the law / policy surface — what a law
  *is* as a data object and how it gates or modifies economic (and later political) behaviour, plus
  the Laws ledger player-facing surface. Design + stub.
- **v0.1.4 — Techs** (**BL-156**). Early design toward the tech / quest system — the condition-set
  gate model (gate = quest = tech) that BL-087 reframed and the v0.3.0 filter system formalises.
  Design only; precursor to BL-087.
- **v0.1.5 — Military systems** (**BL-157**). The Conflict dimension's first data-model footing —
  units, forces, and the seams they need in the world model. Stub, not mechanics (Conflict proper
  stays post-cut scope).
- **v0.1.6 — Politics (stub)** (**BL-158**). A data-model stub only — enough political layer for
  the v0.2.0 nation actor to have something to own, deferring the working system to v0.3.0. The
  band's last minor before the refocus.

### v0.2.0 — The refocus (nation as the strategic actor)

*Theme: change who the player is.* The player pivots from **corporation** to **nation** as the
strategic actor — owning research, military, and intelligence — while the corporation stays the
**economic** actor, prototyped as a single chartered corp (= today's player corp) so the v0.0.x
economy loop survives intact underneath and the nation is a thin strategic layer above it
(**BL-094**, settled 2026-07-04). This is the hinge from *economy sandbox* toward *grand strategy*:
the laws / techs / military / politics stubbed across v0.1.x now hang off an actor that can own
them.

### v0.3.0 — Politics + the filter system

*Theme: the political layer for real, and Era → Filter.* Two coupled deliverables:

- **Politics.** Promote the v0.1.x political stub into a working layer — the nation's political
  character, its relationships, and the levers the player-as-nation actually pulls.
- **The filter system (Era → Filter).** Rename and reframe **Era** as **Filter**: the world-state
  gate governing what content is available when (**BL-087**'s catastrophic-event / quest-tree model
  re-read as a *filter* over the world). A terminology change with reach — `ERAS.md`,
  `GLOSSARY.md`, the era enums, and any `era_*` symbols — folded into the work when it lands, not
  ahead of it (authority time-slice). *(Naming watch: "filter" sits near the map-lens vocabulary in
  `LENSES.md`; confirm the two read as distinct before the rename lands.)*

---

## Done-definition — v0.1.0 (the prototype cut)

v0.1.0 is the **economy loop, validated and playable end-to-end** — not the full game. It remains
the **prototype cut**; the expanded-prototype milestones above (v0.1.x → v0.3.0) are theme-level
and earn their own done-definitions as they firm up. v0.1.0 is cut when all of the following hold:

- The player can **construct and manage** buildings — placement with cost and validation,
  recipe and workforce control — not merely observe authored assets. *(Shipped v0.0.7.)*
- **Population centres** ground workforce supply via habitability feedback, and the player can
  read why workforce is constrained and make siting decisions accordingly. *(Mechanics shipped
  v0.0.7; legibility surface shipped v0.0.8 — BL-069.)*
- Goods **move between bodies** via supply convoys, so **price diverges spatially** and
  logistics affects margin. *(Shipped v0.0.7.)*
- The player must **survey** a body to reveal its tile map and deposit profile — discovery is
  gated, not omniscient. *(Shipped v0.0.8 — BL-067.)*
- **Corporate intelligence is appropriately scoped**: competitor buildings are visible on-canvas;
  their production rates and stockpiles are private; market prices and supply/demand aggregates
  are public — the player reasons from market signals rather than reading off a ledger. *(Shipped
  v0.0.8 — BL-068.)*
- The **full budget** is legible — income vs. expenditure itemised, debt interest visible, runway
  readable — so the player can act before bankruptcy rather than discover it. *(Shipped v0.0.8,
  pulled forward from v0.0.9 — BL-072/073/074.)*
- The **read surfaces** — the ledger family, hover cards, trend plots, and the lens strip —
  make stockpiles, markets, balances, workforce, and construction legible at a glance. *(Mostly
  shipped v0.0.7–8; remaining rough edges tracked in v0.0.9.)*
- The build is **green** and the loop is **verified** (headless economy/generation harnesses
  and visual capture checks).
- **Performance and data growth hold** (the v0.1.0 audit): frame and econ-tick budgets met,
  and counts/memory plateau over a long idle run rather than creeping.
- Excluded throughout, by scope: Conflict, Research, Policy, and Diplomacy beyond the
  data-model stub.

When these hold, the prototype has validated what it set out to validate, and v0.1.0 is the
milestone that says so.

---

## Near-term publish plan — sequencing the unblocked backlog

The milestone map above is **theme-level** and deliberately item-free. This section is the
**operational layer beneath it**: how the *currently-unblocked* items in [`BACKLOG.md`](BACKLOG.md)
are sliced into work sessions and delivered. It names items (which the map above does not) but
**does not duplicate their design** — each item's detail stays in the backlog; this is only the
*sequencing*. Treat it as the standing reference a session opens with, and update the slice list
as sessions complete. *(If it churns enough to feel out of place inside the lean roadmap, graduate
it to its own `PUBLISH_PLAN.md` and cross-link — for now it lives here so there is one place to
look.)*

### Batch Delivery is a strategy, not a code-sprint

A **Batch Delivery** ([`../GLOSSARY.md`](../GLOSSARY.md)) is the whole *process* of moving many
items from intent to committed code — **collision mapping, checkpoints, and session boundaries
included** — not just "write a lot of code." Its load-bearing parts:

- **Barrier semantics** — the set advances breadth-first; every item clears a Delivery step
  before any starts the next (DELIVERY.md § Batch Delivery).
- **Collision mapping** — the file write-sets that decide what can fan out and what stays serial
  (built *per session*, not frozen here — see below).
- **Session boundaries as checkpoints** — a large set is **paused** at a clean, resumable
  boundary rather than forced to complete (REFINED.md § Pausing a task group). The slices below
  *are* those boundaries.

### Cross-cutting rules (read once, apply every session)

- **Golden-image diffing is live (landed Session 1).** The F3 golden-image Brief shipped first,
  as planned, so every `visual` check is now automated pass/fail against a blessed golden rather
  than eyeball-only — the real lever against silent bugs. Author a `scripts/verify/*.lua` and
  bless a golden for each new visual requirement (via the `verifier-visual` skill); re-bless when
  a Brief deliberately changes a captured surface.
- **Build per commit, not per wave.** One commit per Brief already means a green tree at each
  commit. The only *extra* build worth inserting is right after editing a shared/integration file
  (`app.cpp`, `ui_state.hpp`, `overlay.cpp`). A green build proves it *links*, not that it is
  *correct* — correctness rides on the requirement verification, not the compiler.
- **Fan-out is rare here.** Most unblocked work is UI and **collides on shared files**, so it
  serialises in the main session. The DEVLOG bears this out: even the disjoint 8-Brief Layer 3 set
  ran sequentially. Reach for sub-agents only where file scopes are genuinely disjoint (the
  world-gen fixes; the self-contained harness).
- **Hotspot files** (perennial collision points — plan around them, keep single-writer in the
  main session): `body_surface_canvas.cpp` (border + Resource lens + Market lens + hover-card),
  `icons.{hpp,cpp}` (every icon Brief), `app.cpp`, `ui_state.hpp`, `overlay.cpp`.

### The sessions

*Updated 2026-07-04.* The v0.0.6, v0.0.7, and v0.0.8 themes are fully delivered; see DEVLOG for
the per-session record. *(BL-064/BL-065 references anywhere upstream of this point refer to items
renumbered on merge — BL-067/068 supersede them, to avoid a cross-branch collision.)*

*v0.0.8 is delivered in full* — see § Where we are above for the item roll-up, `DEVLOG.md` for the
per-session record, and [`../ui/DISCOVERY.md`](../ui/DISCOVERY.md) for the landed discovery-model
design.

**v0.0.9 — Budget clarity + polish — the polish batch is delivered (2026-07-05).** The five
promote-ready items landed in one batch: **BL-070** in-app system menu (corner gear popup:
Pause/Resume + Exit with confirm, Esc parity), **BL-081** economy-ledger legibility (widened
balances, per-building table dropped, sibling tables un-cramped), **BL-082** construction panel no
longer occluding the Selection element during placement, **BL-090** the corp-emblem glyph family
(player + rivals, on card / selection / markers / hover), and **1 of 2 BL-089 deferrals** (the
hover-card body activity line). Still open toward the cut: the **BL-089 proximity-glimpse** deferral
(held back — save-seam/determinism cost disproportionate to a polish minor; re-assess at the v0.1.0
boundary) and **BL-008** (reassessed — no further work). The delivered set is recorded per-item in
`DEVLOG.md`; what follows is the pre-delivery frame, retained for the boundary-queued items.

With the budget strands already shipped early, v0.0.9 was a **lighter polish minor**: no single
theme, but a set of open items worth clearing before the v0.1.0 audit. Grouped by what they're
*for*, not a task list:

- **Reachability & chrome.** An in-app system menu (BL-070, `design-owed`) for Exit/pause without
  the keyboard.
- **Legibility bugs.** Cramped economy-ledger cells (BL-081) and the construction panel occluding
  the Selection/Tile panel during placement (BL-082) — both `design-owed`, both regressions
  against the legibility bar v0.0.8 just raised.
- **Discovery fog follow-through.** The two deferrals BL-089 explicitly named at landing rather
  than re-opening the item: a refined proximity-glimpse illumination and a hover-card activity
  line. Small, scoped, and already speced in BL-089's own design note.
- **Time-control reassessment** (BL-008) — the countdown/speed-curve work shipped inside v0.0.8;
  this is the checkpoint to decide whether anything further earns a slot.
- **Corp-emblem promotion** (BL-090, `designed`) — lift the geometric emblem into a shared glyph
  family used consistently across map and selection markers.

**Queued toward the v0.0.9/v0.1.0 boundary, not this minor's scope:** planetary logistics
(BL-077, `designed` — terrain-weighted routing, roads & hubs, a larger vertical slice), the two
economy-dynamism items (BL-078 inert product market, BL-079 boom-bust vs. steady processing with
no competitive feedback — both `design-owed`), and the Era-1 tech/quest design gate (BL-087,
`design-owed`). Each either still needs a design settle or is large enough to warrant its own
assessment once the polish pass above clears.

### Out of scope for this plan (gated — do not pull in)

- **Design-owed (`~`) parked items**: BL-011 (Reach/logistics lens), BL-051 (tile-gen deep
  models), BL-054 (nation behaviour) — each needs a design settle before promotion.
- **Post-prototype**: Conflict, Research, Policy, Diplomacy beyond data-model stub; v0.2
  resource-generation scarcity / full deposit authoring; rival corp AI beyond passive stub.

### Collision maps: built per session, not frozen here

A deliberate choice. This plan records the **durable** facts — the slices, their order, and the
**hotspot files** — but **not** the fine-grained per-task collision table. That table is a derived
artifact that goes stale the moment a file is restructured or an item re-scoped, so it is **built
fresh at promotion** in REFINED.md (§ Dividing work across agents, step 1), where it is actually
consumed. A session reads the hotspot list here, then maps its own slice. Documenting the full map
here would only rot; documenting the hotspots is what carries between sessions.
