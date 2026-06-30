# Project Io — Roadmap

The milestone map from the current state to **v0.1.0**, the finished prototype. This
document is **forward-facing and lean**: it names the version sequence, the *theme* of each
minor, and the done-definition for v0.1.0. It deliberately does **not** enumerate individual
items — that lives in the backlog ([`backlog.json`](backlog.json) metadata + [`BACKLOG.md`](BACKLOG.md)
design prose) and the active worklist [`REFINED.md`](REFINED.md). The roadmap sits *above* both: it
says which theme each minor carries; the backlog and worklist say what work realises it.

The prototype validates the **economy loop only**. The full scope and its exclusions
(Conflict, Research, Policy, Diplomacy beyond a data-model stub) are owned by
[`../tech/TECH_FOUNDATIONS.md`](../tech/TECH_FOUNDATIONS.md); the development constraints and
tone that govern *how* the work is done live in
[`DEVELOPMENT_PRACTICES.md`](DEVELOPMENT_PRACTICES.md). This document owns only the milestones.

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

## Where we are — v0.0.7

The interactive economy loop is mechanically complete:

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

The economy is now *interactive* — the player builds, manages, and observes the effects. The
gap is *playability*: all bodies are revealed from the start (no discovery), rival corporations
are passive stubs, and the player lacks the intelligence signals needed to make strategic
building decisions. Closing that gap drives the road to v0.1.0.

---

## The road to v0.1.0

Three more minor versions, then the cut. The v0.0.6 and v0.0.7 themes have shipped; the
remaining road is discovery, budget clarity, and the final quality gate.

### v0.0.8 — Discovery & intelligence

*Theme: give the economy its missing strategic dimension — information asymmetry.* The economy
is interactive but trivially legible: all bodies are fully revealed, rivals are passive, and
nothing gates *where* the player should build or *why*. This minor closes that gap with three
focused strands:

- **Survey system.** Bodies start *unsurveyed* — the tile map and deposit profile are hidden
  until a player-dispatched survey action completes (credit cost + N ticks). The solar system
  canvas marks each body's survey status; the planetary canvas shows a locked state until
  survey is complete. Earth begins surveyed. This is the exploration loop: identify a promising
  body by orbital position and type, spend to survey, then decide whether the deposit profile
  justifies building. *Deposit richness is revealed at survey time as a band (rich / moderate /
  sparse); exact amounts are confirmed once an extraction site is placed.*
- **Visibility model.** On-canvas markers for competitor buildings are visible — they are
  physical structures and unambiguously present. Hover cards on competitor installations show
  type and owner only; production rates and stockpile quantities are **private**. Market
  supply/demand aggregates and prices are **public** (the intentional intelligence channel).
  Players infer competitor state from market signals: a rising price in a resource Kepler
  Industries is known to extract means their output is down, or demand elsewhere is up — not
  a fact to read off a panel. This rule shapes which decisions are interesting and which are
  trivial.
- **Population legibility.** The mechanics already simulate habitability→workforce feedback;
  what's missing is *legibility*: the player needs to see why workforce is constrained, not
  just observe a lower rate. The Population lens, hover cards, and the Selection panel should
  surface: population centre scale, local habitability, and the derived workforce cap — enough
  that the player can reason about siting (build labour-heavy industries near large, habitable
  centres; use autonomous extraction on hostile bodies). No new mechanics; only surfacing what
  is already simulated.

The economic picture this creates: the player is not omniscient. They must spend to learn, read
markets to infer competitor state, and site buildings where the data supports it. The Opportunity
lens and the market trend plots become the primary tools for that reasoning.

### v0.0.9 — Budget clarity + polish

*Theme: make the financial pressure legible.* The budget loop runs — income from sales,
expenditure on wages and maintenance, balance going negative — but the player cannot easily read
*why* their balance is moving or how much runway they have. Three strands:

- **Full budget breakdown.** Income vs. expenditure itemised clearly (by building type, by
  resource, by logistics cost); the running balance shown against a projected time-to-zero so
  the player feels the pressure and can act before bankruptcy.
- **Debt system.** When balance goes negative, interest accrues each tick — the economy
  bankruptcy harness already models this; the player-facing surface does not. Interest charges
  should appear in the budget breakdown as a distinct line so the player knows when they have
  entered a self-accelerating decline.
- **Polish pass.** Time-control clarity (non-linear speed curve, days-until-next-quarter
  countdown); menu vocabulary cleanup; icon consistency sweep; any outstanding lens or UI
  completeness work from v0.0.8.

### v0.1.0 — Quality audit + cut

*Theme: prove it holds up, then ship.* No new systems. Three audit instruments, then the cut:

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

---

## Done-definition — v0.1.0 (full economy loop)

v0.1.0 is the **economy loop, validated and playable end-to-end** — not the full game. It is
cut when all of the following hold:

- The player can **construct and manage** buildings — placement with cost and validation,
  recipe and workforce control — not merely observe authored assets. *(Shipped v0.0.7.)*
- **Population centres** ground workforce supply via habitability feedback, and the player can
  read why workforce is constrained and make siting decisions accordingly. *(Mechanics shipped
  v0.0.7; legibility surface owed in v0.0.8.)*
- Goods **move between bodies** via supply convoys, so **price diverges spatially** and
  logistics affects margin. *(Shipped v0.0.7.)*
- The player must **survey** a body to reveal its tile map and deposit profile — discovery is
  gated, not omniscient. *(Owed v0.0.8.)*
- **Corporate intelligence is appropriately scoped**: competitor buildings are visible on-canvas;
  their production rates and stockpiles are private; market prices and supply/demand aggregates
  are public — the player reasons from market signals rather than reading off a ledger. *(Owed
  v0.0.8.)*
- The **full budget** is legible — income vs. expenditure itemised, debt interest visible, runway
  readable — so the player can act before bankruptcy rather than discover it. *(Owed v0.0.9.)*
- The **read surfaces** — the ledger family, hover cards, trend plots, and the lens strip —
  make stockpiles, markets, balances, workforce, and construction legible at a glance. *(Mostly
  shipped v0.0.7; completeness in v0.0.8–9.)*
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

*Updated 2026-06-30.* The v0.0.6 and v0.0.7 themes are fully delivered; see DEVLOG for the
per-session record. The active frontier is **v0.0.8 — Discovery & Intelligence**. No per-session
slice list is frozen here — the backlog for v0.0.8 is still being designed (BL-064 survey system
is the first item to promote); sessions will rebuild the slice list at promotion from the live
backlog. The next session's first act is to design-settle and promote the survey system item.

**What ships next (v0.0.8):**

1. **BL-064 — Survey system** *(design-owed, filed 2026-06-30)*. Bodies start unsurveyed; a
   survey action (credit cost + N ticks) reveals the tile map and deposit richness band. See the
   backlog item for the full design.
2. **BL-065 — Visibility model** *(design-owed, filed 2026-06-30)*. Explicit rules for what
   the player can see about competitors: building presence public, production and stockpiles
   private, market aggregates and prices public. Hover card updated for competitor markers.
3. **Population legibility** — surfacing the habitability→workforce chain visibly in the
   Population lens, hover cards, and Selection panel. Lightweight (no new mechanics; backlog
   item needed if the coverage gap is confirmed at promotion).

**v0.0.9 queue (budget clarity + polish):**

- Full budget breakdown (income/expenditure by category, projected runway)
- Debt-interest system (on-screen visibility of the self-accelerating decline)
- Time-control clarity pass (BL-008, currently parked — reassess at v0.0.9 open)
- Icon consistency sweep and menu vocabulary cleanup

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
