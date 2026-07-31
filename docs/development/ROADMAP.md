# Project Io — Roadmap

The milestone map from the current state through the **v0.1.0** prototype cut and on into the
**expanded prototype** (v0.1.x → v0.3.0). This document is **forward-facing and lean**: it names
the version sequence, the *theme* of each minor, and the done-definition for the prototype cut. It
deliberately does **not** enumerate individual items — that lives in the backlog
([`backlog.json`](backlog.json), metadata + design prose; [`BACKLOG.md`](BACKLOG.md) is a legacy
drain) and the active worklist [`REFINED.md`](REFINED.md). The roadmap sits *above* both: it says which theme each minor
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

## Where we are — v0.0.9 shipped, v0.1.0 in progress

*Latest tag `v0.0.9`, cut 2026-07-05; the v0.1.0 work is landing on `main` (status 2026-07-31).*
The interactive economy loop is mechanically complete, legible, and discovery-gated:

- **Layer 0–2** — SDL3 + fixed-timestep simulation + economy tick + sol2; the core data
  model; the Solar / Circumplanetary / Planetary canvases with the minimap zoom ladder.
- **World generation (v0.0.3)** — procedural tiles (two-axis terrain), nations (Voronoi
  BFS countries with weighted sizes and history-pass merges — 14 at v0.0.3, **43** on the
  default seed since BL-221 (pre-national ladder) landed 2026-07-30), and corporations
  (asset placement, financial profile).
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
appropriately scoped, and the budget is itemised. What remains before the cut is the v0.1.0
terrain strand and the quality audit — see § v0.1.0 below.

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

**v0.0.9 — Budget clarity + polish (cut 2026-07-05).** The remaining legibility rough edges
cleared ahead of the v0.1.0 audit: the in-app system menu (BL-070), the economy-ledger and
construction-panel legibility bugs (BL-081/082), the corp-emblem glyph family (BL-090), and the
hover-card activity line (one of the two BL-089 activity-fog deferrals). Per-item record in
`DEVLOG.md`.

**v0.1.0 — in progress (status 2026-07-31).** Landed on `main` since the tag: **roads &
planetary logistics** (BL-146–149, complete 2026-07-10 — the generated per-nation lattice with
three road tiers, cities as free logistics hubs, the inland logistics-hub building; built ahead
of the cut rather than waiting for the v0.1.1 slot first planned for it); the **2026-07-08
lens/UI review batch** (BL-133–145 + BL-150, all complete); the **legibility cut-blockers**
(BL-174, BL-176–179, merged 2026-07-30); the **landform render** (BL-231) and its **spanning
markers** (BL-232), both landed; a first cut of the **continent lens** (BL-226, landed
2026-07-30 — the item itself stays open); and the opening of the generation/history arc
(BL-167 planetology chain complete; BL-220/221 history-ladder foundations — post-v0.1.0-themed
work that landed early). What remains toward the cut is named under § v0.1.0 below.

---

## The arc from here

The map no longer ends at the prototype cut. Ben's **2026-07-09 refocus** extends it: **v0.1.0**
still cuts the economy-loop prototype; then the **v0.1.x** band lays groundwork for an *expanded
prototype*, **v0.2.0** carries the AI opponent (the player refocus is intended for the same era
but not yet versioned — see § v0.2.0), and **v0.3.0** brings the political layer and the filter
system online. The v0.0.x themes have all shipped; the roll-up is § Where we are, the per-item
record `DEVLOG.md`.

### v0.1.0 — Quality audit + legibility polish + cut

*Theme: make it read cleanly, prove it holds up, then ship.* The legibility strands have landed —
the 2026-07-08 lens/UI review batch, the legibility cut-blockers, and roads with them (§ Where we
are). The live cut set (`version_goal: v0.1.0`, status 2026-07-31) is the **terrain/landform
strand** plus two owed surfaces: BL-231/232 (landform render + spanning markers — landed),
BL-226 (continent lens — first cut landed 2026-07-30, item open), BL-233 (terrain combat
modifiers — owed), BL-234 (font glyph range — owed), and BL-162 (tile construction panel —
owed). Then the **quality audit** — three instruments — and the cut.

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
rest of the band (v0.1.2–v0.1.5) is deliberately design-forward — each now has a placeholder
`design-owed` item that firms into real design as it is reached.

- **v0.1.1 — Shell & legibility follow-through.** *(Re-themed 2026-07-31: roads — BL-146–149 —
  landed complete ahead of the v0.1.0 cut, retiring the "roads are an invisible tile attribute"
  concern; § Where we are.)* The live set is shell and read-surface work: the road tier legend
  (BL-184) and road fog dimming (BL-185), building stack capacity (BL-193), the drill-through
  disclosure idiom (BL-214), the text-wrap render audit (BL-215), chat pinning (BL-216), and the
  building-selection tile format (BL-229); the hover glance-then-stick (BL-230) is already
  landed. Still the concrete build minor.
- **v0.1.2 — Laws** (**BL-155** law/policy surface design, **BL-186** laws ledger UI). First pass
  at the law / policy surface — what a law *is* as a data object, how it gates or modifies
  economic (and later political) behaviour, and its ledger surface. Design + stub.
- **v0.1.3 — Techs** (**BL-156**). Early design toward the tech / quest system — the condition-set
  gate model (gate = quest = tech) that BL-087 reframed and the v0.3.0 filter system formalises.
  Design only; precursor to BL-087.
- **v0.1.4 — Military systems** (**BL-157**). The Conflict dimension's first data-model footing —
  units, forces, and the seams they need in the world model. Stub, not mechanics (Conflict proper
  stays post-cut scope).
- **v0.1.5 — Politics (stub)** (**BL-158**). A data-model stub only — enough political layer for
  the v0.2.0 nation actor to have something to own, deferring the working system to v0.3.0. The
  band's last minor before the refocus.

### v0.2.0 — The AI opponent (versioned), and the refocus (intended, unversioned)

*Versioned theme: the AI opponent.* The backlog's live v0.2.0 set is the corp-AI arc: stage A —
the deterministic scored-utility layer over the corp-command seam — already landed (BL-202,
complete; `src/world/corp_ai.hpp`). Queued: BL-203 (predictive spending), BL-204 (AI skill
harness), BL-205 (corp chat log), BL-207 (persona counsel packs), and the trade-policy pair
BL-160/161 (auto-exchange policy, counterparty allow/deny).

*Intended theme, not yet versioned: the refocus — change who the player is.* The player pivots
from **corporation** to **nation** as the strategic actor — owning research, military, and
intelligence — while the corporation stays the **economic** actor, prototyped as a single
chartered corp (= today's player corp) so the v0.0.x economy loop survives intact underneath and
the nation is a thin strategic layer above it (**BL-094**, settled 2026-07-04). This is the hinge
from *economy sandbox* toward *grand strategy*: the laws / techs / military / politics stubbed
across v0.1.x now hang off an actor that can own them.

**Open sequencing question (2026-07-31).** BL-094 (player-nation pivot) carries no version goal
in the backlog; the versioned v0.2.0 set is the AI opponent. Whether the pivot shares v0.2.0
with the AI set or takes its own minor is Ben's call, not yet made.

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
  shipped v0.0.7–8; the remaining rough edges cleared in v0.0.9.)*
- The build is **green** and the loop is **verified** (headless economy/generation harnesses
  and visual capture checks).
- **Performance and data growth hold** (the v0.1.0 audit): frame and econ-tick budgets met,
  and counts/memory plateau over a long idle run rather than creeping.
- Excluded throughout, by scope: Conflict, Research, Policy, and Diplomacy beyond the
  data-model stub.

When these hold, the prototype has validated what it set out to validate, and v0.1.0 is the
milestone that says so.

---

## Sequencing — live in SPRINTS.md and REFINED.md

*Superseded 2026-07-31.* The ~100-line publish plan that lived here (last updated 2026-07-04)
is retired: everything it sequenced — the v0.0.6–v0.0.8 sessions and the v0.0.9 polish batch —
was delivered in full (per-item record in `DEVLOG.md`; roll-up in § Where we are). Its two
boundary-queued holdouts resolved too: planetary logistics (BL-077) and the economy-dynamism
pair (BL-078/079) shipped, and BL-011 (reach/logistics lens) — listed there as parked — is
complete.

Live sequencing no longer belongs in the roadmap. The weekly goal/retro rhythm is
[`SPRINTS.md`](SPRINTS.md), the active worklist is [`REFINED.md`](REFINED.md), and the method
(Batch Delivery, collision maps, worktrees) is [`DELIVERY.md`](DELIVERY.md). This document
stays theme-level.
