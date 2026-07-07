# Corporation — design Q&A

> **Working design doc** for the ledger-mockup pass (Power BI). Strawman answers — Ben revises.
> Menu slot: `nav rail slot 1 "Corporations"` · Source: `src/ui/corporation_panel.cpp` · Mock table(s): `corporations.csv`, `cashflow.csv` · Related: `BL-121` (audit), `BL-122` (fold-out host), `BL-123` (Selection resize)
> Host: shell fold-out column (BL-122), ~480px @1720.

## 1. Top question — the one thing this answers at first glance
**"Who are the corporations, and where do I stand among them?"** The current panel already answers this literally: a sorted, one-row-per-corp table of `Corporation | Focus | Balance`, with the player's row tinted (`IM_COL32(255,255,255,40)`) and balance coloured red/green. The first-glance read is the **balance ranking** — am I ahead of or behind my rivals, and by how much. Secondary questions that justify sub-views: *by what strategy is each competing* (focus mix — extraction/processing/trade), *how big is each* (building/asset count), and *who is winning right now vs. who is bleeding* (net cash direction, which lives in `cashflow.csv` but is not yet shown). This is the **drill-down onto the field of players**; whole-economy aggregate (my totals, sector balance, income/expenditure trend) belongs to Economy, not here.

**Ownership boundary (Corporation vs Economy).** This doc **owns the rival-field ranking** — the corp-balance-rank question, one row per corp, player-tinted and sorted. Economy's default view claims a similarly-named "Standing," but the two must not both answer it: **Economy must defer the per-corp balance ranking to Corporation** and confine its own first glance to *whole-economy aggregate* (player totals, sector balance, income/expenditure trend). If a rival-ranking read is wanted from Economy, it links here rather than re-rendering the field. One question, one home.

## 2. Sub-levels — views & default

| View | Answers (one question) | Content |
|---|---|---|
| **Standings** (default) | How is each corporation placed vs. me? | The current table — name (player-tinted), `Focus`, `balance` red/green, sorted. Add a rank position and a signed **net/tick** column from `cashflow.csv` so "who is climbing" is legible, not just the static balance snapshot. |
| **Profiles** | What is each rival's *shape*? | Per-corp identity card list: home nation, focus, building/asset count (`assets.size()`), starting vs. current balance. This absorbs the two columns BL-121 dropped (Home Nation, Status, Buildings) into a browsable second view rather than cramming them back into the narrow table. |

Default on open: **Standings**. It is the ranking that answers the top question at a glance.
Cross-cutting selectors (NOT views, exempt from the toggle rule): **none needed** — the corp set is small and fully enumerable (8 corps in mock). A future *sort selector* (by balance / net / building count) would be a selector, not a view.

**Asset-count ownership (three-way).** The Profiles building/asset-count column (`assets.size()`) is the **per-corp** building count, and Corporation **owns** it — the authoritative "how many buildings does *this corp* have" answer. That is distinct from Balance's **Assets** figure (the player corp's own valuation) and from Economy's **Sector** composition bar (which owns only the *aggregate* building composition across the whole field, not any per-corp count). So: per-corp counts here, player-self valuation in Balance, whole-field composition in Economy — no three-way overlap.

If Ben prefers to keep Corporation genuinely single-question: drop Profiles and let a **row-click route full detail to the Selection panel** (which is exactly what the code does today — `s.selected_entity = id`). That keeps one table, no tabs, and leans on the shared Selection element for per-corp depth. This is the leaner option and I lean toward it — see open questions.

## 3. Lens on open
Arms **`overlay_mode::corporation`** (per-corp tile tint, player-corp border) — the candidate lens named for this slot, and consistent with Ben's "opening a menu usually should arm a lens." Rationale: the Corporation ledger is *about the field of players*, and the corporation lens paints exactly that field onto the canvas (whose buildings are whose), so the ledger and the map answer the same question in two registers. **Fixed on open**, not sub-view-following — both Standings and Profiles are about the same "who owns what" question, so there is no reason to swap the lens between them. Note: `ui_state.overlay` defaults to `none` at campaign start (BL-013 reversal), so arming here is an explicit set, not the default.

## 4. Data — live vs plumbing gaps
Maps cleanly to live world state — every field the panel needs already exists on `corporation_component` (`src/world/components.hpp:389`):
- **name / focus / balance / is_player** — live, all four rendered today.
- **home_nation** (`entity_id`) — live, but needs a nation-name lookup to display (mock `corporations.csv` carries the resolved string `home_nation`).
- **assets** (`std::vector<entity_id>`) — live; `assets.size()` is the building count (mock `buildings` column).
- **net / income / expenditure / interest** — live in the economy money loop (`budget_system.hpp`); surfaced in mock `cashflow.csv`. The **net/tick column** proposed for Standings needs this wired into the panel — it is **not currently read** by `corporation_panel.cpp`.

Plumbing gaps to flag:
- **`starting_capital` is a placeholder** (0 in mock `corporations.csv`, and `since_start` is just a dup of `balance`). Any "growth since start" framing in Profiles is not backed by real data until the exporter emits a true opening balance.
- **No time-series per rival.** `player_timeseries.csv` covers only the player corp; there is no per-corp balance history, so a "who is climbing over time" sparkline is not pluggable today — the signed net/tick is the best available proxy.

## 5. Close / toggle semantics
Nav-rail slot-1 icon toggles the ledger open/closed. With **two views**: re-clicking the **currently-active tab (Standings or Profiles) closes the ledger** — not collapse-to-overview. Switching between the two tabs is an ordinary view change. If Ben takes the **single-table option** (no tabs), the toggle collapses to just the rail icon: click to open, click again to close; a row-click selects a corp and routes to the Selection element (mutually exclusive — selecting closes the ledger to give the column to the Selection, per the settled selection rule). Opening the ledger arms the corporation lens; closing it does **not** auto-disarm the lens (lens state is canvas-owned and persists).

## Open questions for Ben
- **Two views or one table?** I lean **one table + Selection-panel drill-down** (matches current code, avoids duplicating detail the Selection already shows per BL-123). Confirm, or keep the **Standings/Profiles** split if you want rival profiles browsable without a canvas selection.
- **Net/tick column.** Add the signed net-per-tick from `cashflow.csv` to Standings so ranking shows *direction*, not just a static balance? It needs new plumbing into the panel.
- **Lens: fixed or follow?** I set it **fixed = corporation** on open. Confirm you don't want it disarmed when the ledger closes (I left the lens persisting).
- **Sort selector.** Do you want a by-balance / by-net / by-buildings sort combo (a selector, not a view), or is the fixed deterministic `entity_id` sort enough for the prototype?
