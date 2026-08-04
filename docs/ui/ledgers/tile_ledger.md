# Tile Ledger (History) — design Q&A

> **⚠ THE BANNER THAT WAS HERE WAS FALSE — corrected 2026-08-04.** It read "DEFERRED — migration
> not yet done … still a FLOATING ImGui window … does not yet use `foldout_begin`/`foldout_end` or
> `nav_button` tabs." **The migration landed.** `tile_inspector.cpp:39` opens with
> `ui::foldout_begin("Tile Ledger")` and `:80-84` draw three `nav_button` tabs. Because that banner
> sat at the top, every *(proposed)* marker below inherited a false premise — read them as
> describing a surface that in most respects exists.
>
> **The three views are Story / Chain / Tiles**, not the Tiles / Buildings / Market this doc
> describes. Story is a dated biography; Chain holds the generation charts. So "History" is no
> longer aspirational — the slot earned its name. *(BL-281 retires the Tiles view; see § 2.)*

> **Working design doc** for the ledger-mockup pass (Power BI). Strawman answers — Ben revises.
> Menu slot: `rail slot 9 "History"` · Source: `src/ui/tile_inspector.cpp` · Mock table(s): `buildings.csv`, `markets.csv`, (tile-field data: NONE exported) · Related: `BL-119` (audit), `BL-122` (column host), `BL-281` (retire the Tiles view)
> Host: shell fold-out column (BL-122) — migrated.

## 1. Top question — the one thing this answers at first glance
**"What is on this body's surface, tile by tile?"** The current window is a per-body ground-truth dump: pick a body from the `Body` combo, and the main table lists every tile as one row (X, Y, composition, landform, hazard, habitability, then one deposit column per resource — 6 + `resource_count` columns). Secondary questions it already answers below the table: **"what have I built here and how staffed is it?"** (Buildings section: type, `[x,y]`, workforce %) and **"what does this body's market look like?"** (Market section: supply/demand/price/base per resource). The name "History" on the rail slot is aspirational — the code today is a *current-state* tile inspector, not a history/timeline.

## 2. Sub-levels — views & default

**As built** (`tile_inspector.cpp:80-84`):

| View | Answers (one question) | Content |
|---|---|---|
| **Story** | What happened to this body? | The dated biography — planetology, plate, and history-ladder events in sequence |
| **Chain** | How did this body come out the way it did? | The generation charts (`generation_charts.cpp`) |
| **Tiles** | What terrain & deposits does this body have? | The tile table: X, Y, composition, landform, hazard, habitability, per-resource deposit (`—` when zero) |

**BL-281 (retire the History Tiles view) removes the third**, leaving Story + Chain — a surface
about how the body came to be, with per-tile ground truth living where tile selection already puts
it. So the table below, and the Market-view recommendation that follows, are superseded twice over:
the views changed, and the remaining one is on its way out.

*The original proposal, kept for the reasoning:* Tiles / Buildings / Market, defaulting to Tiles.
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
