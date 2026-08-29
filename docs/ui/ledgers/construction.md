# Construction — design Q&A

> **Working design doc** for the ledger-mockup pass (Power BI). Strawman answers — Ben revises.
> Menu slot: `rail slot 3 "Construction"` · Source: `src/ui/construction_panel.cpp` · Mock table(s): `buildings.csv`
> Host: shell fold-out column, ~380px @1720 (derived — `shell_column_width(disp.x)`, 380–460 by resolution).

## 1. Top question — the one thing this answers at first glance
**"What is happening with my construction, and what is it costing me?"** — the in-flight read. The placement verb itself does **not** live here: building is a *targeted* action, so the build front door sits on the **tile Selection card** (extractable-target radios, cost-annotated build buttons, affordability gate — `selection.md`), and per-building configuration (workforce, production method, mothball, dismantle) sits on the **building Selection card**. That is the menus-are-broad-ledgers rule in `MENU.md`: a rail slot is earned by an overview, never by a press on one thing. What remains broad — and what this surface answers — is the **queue**: every player building still under construction, its analog rate, ETA and paused status, and the estimated per-quarter opex of the lot.

## 2. Sub-levels — views & default

The surface is a single **Construction** view — `draw_queue_section` under `foldout_begin("Construction")`, no `nav_button` strip.

| View | Answers (one question) | Content |
|---|---|---|
| **Construction** *(only view)* | "What's in flight, and what is it costing?" | **Estimated cost: N / quarter** (`estimated_quarterly_construction_cost` — `compute_building_opex` maintenance + wages summed over every player building with `ticks_remaining > 0`, the same formula the budget loop uses), then the queue table: one row per in-progress build with a progress cell colour-coded dim (<25 %) / mid (25–75 %) / high (>75 %) |

**Construction is durative and material-gated.** A building under construction advances each economy tick by a rate in [0,1] set by how much of its per-tick material need (`resource_build_cost[r] / build_duration_ticks`) the local market (`market_for_tile`) can supply; below `1 / max_stretch` the build is **paused**. `construction_rate`, `construction_status` ("Building… ~N qtrs", "(materials scarce)", "Paused - market can't supply materials") and `construction_status_colour` (green on schedule / amber scarce / red paused) mirror `run_construction`'s formula so the panel and the Selection front door show the same number. The building carries `ticks_remaining` (an integer gate) and `construction_progress` (the fractional accumulator that lets a starved build stretch over many ticks). The display word is **qtr**, never tick (NR-002, Ben 2026-08-01) — a Tick is literally a calendar quarter.

**Proposed second view — Buildings:** "How do I configure what I have?" — per-building rows with type / target / recipe / build cost / maintenance, a workforce slider (0–200 %), a recipe combo where more than one recipe is era-allowed, and Decommission. This is the overview form of what the building Selection card does one building at a time; the open question is whether the roster earns a tab or the Selection card is enough.

**Default view on open:** Construction.
**Cross-cutting selectors that are NOT views (toggle-exempt):** none on the live view. The proposed Buildings view's recipe combo and workforce slider are in-row selectors, not sub-views.

## 3. Lens on open
**Proposal: arm `opportunity`** (per-tile best-building net margin) on open — it is the literal "where should I build?" signal, and Ben's stated preference is that opening a menu usually arms a lens. If a Buildings view is added, have the lens **follow the sub-view**:
- **Construction → `opportunity`** (best-margin tiles light up as the next build candidates).
- **Buildings → `production`** (per-tile output intensity — see what your estate is yielding while you tune it). Alternative: `none`, since configuration is about one already-selected building, not the map.

The panel arms no lens on open; the mapping above is the proposal.

## 4. Data sources
- **Queue** — `world::buildings` filtered to `is_player_owned` and `ticks_remaining > 0`; rate from `construction_rate` (market supply against `recipe_registry::economics(type).resource_build_cost` and `build_duration_ticks`, `reg.construction().max_stretch`).
- **Estimated cost** — `compute_building_opex` (`budget_system.hpp`) over the same set, with `body_mean_habitability` feeding the workforce term.
- **Build cost, arming, placement** — `recipe_registry::economics()` (`build_cost`, `resource_build_cost[]`) and `placement_rules::k_extractable`, read by the tile Selection card rather than here.
- **Per-building configuration** — `building_component::workforce_target`, `active_recipe_index` / `recipe`, `decommissioned`, written through `try_switch_recipe` and the workforce verbs from the building Selection card.

**Maps to mock `buildings.csv`:** `type`, `output`, `active`, `exhausted`, `corp` / `body` — enough to mock a built-inventory roster (filter to player corp = 30310 Genom Systems). The CSV rows are the *built* estate, not a queue; it carries no `workforce_target` / `recipe` / `decommissioned` / `maintenance` column and no progress or ticks-left, so neither the live queue nor a Buildings roster can be reproduced from it without extending the exporter. There is no per-tile margin or output export for an `opportunity` / `production` mock either.

## 5. Close / toggle semantics
Rail slot 3 icon toggles the whole Construction ledger open/closed. With a single view the toggle is the icon alone; once a second tab exists, re-clicking the **currently-active tab closes the ledger entirely** (releases the column), per the universal toggle rule — not "collapse to Construction". Switching to a *different* tab just changes view. In-row selectors (recipe, workforce) are exempt. Opening Construction closes whichever other ledger held the column (accordion, `close_all_panels`); the Selection band is independent of the column and stays where it is.

## Open questions for Ben
- **Does a Buildings roster earn a tab?** Configuration lives per building on the Selection card. Is an all-buildings roster (sortable, with the same controls inline) a broad overview that earns a second view here, or does it duplicate the card and belong nowhere?
- **Queue table shape.** Per row: building type, tile, progress %, ETA in quarters, status (on schedule / scarce / paused), and cost remaining — or a leaner three-column read? The progress colour bands (25 / 75 %) are a strawman.
- **Lens: follow-the-view or fixed `opportunity`?** Confirm the mapping (Construction→opportunity / Buildings→production), or pin one lens for the whole ledger.
