# Tile Ledger (History) — design Q&A

> **⚠ DEFERRED — migration not yet done.** This surface is still a **FLOATING** ImGui window (`ImGui::Begin("Tile Ledger", …)` with the shared `ledger_window_spawn`/`ledger_window_size` chrome). The BL-119 audit is complete but it has **not** been migrated into the fold-out column host, and does **not** yet use `foldout_begin`/`foldout_end` or `nav_button` tabs. This doc is here for completeness — everything below marked *(proposed)* describes the target migrated shape, not current code.

> **Working design doc** for the ledger-mockup pass (Power BI). Strawman answers — Ben revises.
> Menu slot: `rail slot 9 "History"` · Source: `src/ui/tile_inspector.cpp` · Mock table(s): `buildings.csv`, `markets.csv`, (tile-field data: NONE exported) · Related: `BL-119` (audit), `BL-122` (column host), `BL-078/079` (stockpile drain)
> Host: shell fold-out column (BL-122), ~480px @1720 *(target — not yet migrated)*.

## 1. Top question — the one thing this answers at first glance
**"What is on this body's surface, tile by tile?"** The current window is a per-body ground-truth dump: pick a body from the `Body` combo, and the main table lists every tile as one row (X, Y, composition, landform, hazard, habitability, then one deposit column per resource — 6 + `resource_count` columns). Secondary questions it already answers below the table: **"what have I built here and how staffed is it?"** (Buildings section: type, `[x,y]`, workforce %) and **"what does this body's market look like?"** (Market section: supply/demand/price/base per resource). The name "History" on the rail slot is aspirational — the code today is a *current-state* tile inspector, not a history/timeline.

## 2. Sub-levels — views & default
The floating window is currently a single scroll of three stacked sections. When migrated into the one-question-per-view column, it splits *(proposed)*:

| View | Answers (one question) | Content |
|---|---|---|
| **Tiles** | What terrain & deposits does this body have? | The tile table: X, Y, composition, landform, hazard, habitability, per-resource deposit (`—` when zero) |
| **Buildings** | What have I built here and how staffed? | Per-building bullets: `building_type_name`, `[x,y]`, `workforce_assigned` % |
| **Market** | What clears on this body right now? | Supply / demand / price (coloured by move vs base) / base price, per resource, with identity swatch |

**Default on open:** *Tiles* (the body's ground truth is the point of the surface).
**Cross-cutting selectors (NOT views, exempt from toggle rule):** the **Body** combo. It defaults to `s.active_body` when that's a non-star surface body, else the lowest-id body.

**Caveat for Ben:** the Market view here **duplicates the standalone Market ledger's question** and violates the one-question-per-surface taxonomy. Recommend the migrated Tile Ledger **drop Market entirely** (it exists here only because this window predates the Market ledger — the section's own comment calls itself "the functional specification for the production market ledger"). That leaves a clean two-view Tiles/Buildings surface, both genuinely per-tile/per-body.

## 3. Lens on open
**Arm `resource`** *(proposed, default)* — the tile deposit columns are exactly what the resource field-overlay paints, so opening History and seeing the deposits lit on-canvas is the natural pairing. If the migrated surface follows the active sub-view: **Tiles → `resource`**, **Buildings → `production`** (built extractors/processors are the production lens's subject). Market view (if kept) → `market`. Per Ben's "opening a menu usually should arm a lens," yes — fixed `resource` is the safe default; follow-the-view is the richer option.

## 4. Data — live vs plumbing gaps
- **Live today (world state, no mock needed):** the whole surface reads directly off `w.tiles`, `w.buildings`, `w.markets` per selected body — tile fields, building workforce, and market arrays are all live `tile_component` / `building_component` / `market_component` reads. This surface is the *most* live of the ledger family; it needs no CSV to function in-app.
- **Mock tables for the Power BI mockup:** `buildings.csv` (has `building_id, body_name, type, output, active, exhausted` — but **no workforce %/ tile coordinate**, so the mockup can't reproduce the `[x,y] workforce %` line) and `markets.csv` (per-body supply/demand/price/base — carries the **5x body_id-inflation caveat**; a Market view in the mockup will over-count until a `market_id`/region column is added to the exporter).
- **GAP — no tile-field export.** There is **no `tiles.csv`** in the mock set. The entire primary Tiles view (composition/landform/hazard/habitability/deposits) has **no plumbing to Power BI** — Ben cannot mock the main table from CSV today. If the mockup pass needs it, a `tiles.csv` exporter (one row per tile, columns mirroring the table) is a **new plumbing item**.
- **GAP — "History" implies a timeline that doesn't exist.** No per-tile time-series is captured anywhere (`player_timeseries.csv` is corp-level balance/income only). A true tile *history* (deposit depletion over ticks, build events) would be entirely new capture.

## 5. Close / toggle semantics
Once migrated: the rail slot-9 icon toggles the ledger open/closed; re-clicking the **currently-active sub-view tab CLOSES the ledger** (not collapse-to-overview). The **Body** combo is a cross-cutting selector, not a view — switching bodies never closes anything. As a ledger it is **mutually exclusive with the Selection element** in the shared column: opening History closes an open Selection; a new entity selection closes History. **Today** (still floating) none of this applies — it has an ImGui title-bar close button (`p_open`) and coexists with everything.

## Open questions for Ben
- **Drop the Market view?** It duplicates the standalone Market ledger's question and breaks one-question-per-surface. Recommend cutting it, leaving a clean Tiles/Buildings surface — confirm, or keep a stripped body-market glance here?
- **What does "History" actually mean for slot 9?** The code is a *current-state* inspector, not a timeline. Rename the slot to match the current tile-inspector behaviour, or commit to building real per-tile history capture (a new time-series exporter) to earn the name?
- **`resource` fixed vs follow-the-view lens?** Fixed `resource` on open is simplest; follow-the-view gives Buildings→`production`. Which pairing?
- **Is a `tiles.csv` exporter in scope for this mockup pass?** Without it the primary table can't be mocked in Power BI — worth the plumbing, or mock the tile table from hand-stubbed rows for now?
