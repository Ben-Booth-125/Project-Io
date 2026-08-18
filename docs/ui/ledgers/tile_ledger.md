# Tile Ledger (History) — design Q&A

> **⚠ THE BANNER THAT WAS HERE WAS FALSE — corrected 2026-08-04.** It read "DEFERRED — migration
> not yet done … still a FLOATING ImGui window … does not yet use `foldout_begin`/`foldout_end` or
> `nav_button` tabs." **The migration landed (BL-211, 2026-07-29).** `tile_inspector.cpp:39` opens
> with `ui::foldout_begin("Tile Ledger")` and `:80-84` draw three `nav_button` tabs; the surface is
> docked in the shell fold-out column (BL-122/BL-144), not a floating window. Because that banner
> sat at the top, every *(proposed)* marker below inherited a false premise — read them as
> describing a surface that in most respects exists.
>
> **The three views are Story / Chain / Tiles**, not the Tiles / Buildings / Market this doc
> describes. Story is a dated biography; Chain holds the generation charts. So "History" is no
> longer aspirational — the slot earned its name. See `MENU.md` § slot 9 for the rail-level
> framing. *(BL-281 retires the Tiles view; see § 2.)*

> **Working design doc** for the ledger-mockup pass (Power BI). Strawman answers — Ben revises.
> Menu slot: `rail slot 9 "History"` · Source: `src/ui/tile_inspector.cpp` · Mock table(s): `buildings.csv`, `markets.csv`, (tile-field data: NONE exported) · Related: `BL-119` (audit), `BL-122`/`BL-144` (column host), `BL-211` (Story/Chain/Tiles split), `BL-281` (retire the Tiles view)
> Host: shell fold-out column (BL-122/BL-144) — migrated, docked.

## 1. Top question — the one thing this answers at first glance
**"How did this body get to be what it is?"** — the three tabs answer three related but distinct
questions in one column: *Story* ("what happened here?" — the oral-history biography drawn from
`generation_report::body_entry::state.history`), *Chain* ("how did the generation chain arrive at
this?" — the wizard's own stage charts, redrawn from the persisted report, grouped System / Life /
Legacy), and *Tiles* ("what did the chain leave on the ground?" — the per-tile ground-truth table,
plus the buildings and market sections stacked below it). The rail slot's "History" name is now
earned by *Story* and *Chain*; *Tiles* stays a current-state inspector nested inside the same
surface.

## 2. Sub-levels — views & default

**As built** (`tile_inspector.cpp:80-84`):

| View | Answers (one question) | Content |
|---|---|---|
| **Story** | What happened here, in order? | The dated biography — planetology, plate, and history-ladder events in sequence (`format_history_date` + event text), each with an optional wrapped consequence line underneath |
| **Chain** | How did the generation chain arrive at this body? | The wizard's stage charts (`generation_charts.cpp`), re-rendered from the persisted `generation_report`, grouped into three rounds (**System / Life / Legacy**) via a second `nav_button` strip; one collapsing accordion per stage, only the round's first stage open by default |
| **Tiles** | What terrain, buildings, and market does this body have right now? | The tile table (X, Y, composition, landform, hazard, habitability, per-resource deposit, `—` when zero), then a **Buildings** section (type, `[x,y]`, workforce %) and a **Market** section (supply/demand/price/base per resource with identity swatch) stacked below it |

**BL-281 (retire the History Tiles view) removes the third**, leaving Story + Chain — a surface
about how the body came to be, with per-tile ground truth living where tile selection already puts
it. So the table above's third row, and the Market-view recommendation that follows, are superseded
twice over: the views changed, and the remaining one is on its way out.

**Default view:** whatever `s.history_view` was last left on (persisted in `ui_state` so a verify
script can park it) — there is no forced default-to-Story on every open.

*The original proposal, kept for the reasoning:* Tiles / Buildings / Market, defaulting to Tiles.
**Cross-cutting selector (NOT a view, exempt from the toggle rule):** the **Body** combo, shown on
*Story* and *Tiles* only — *Chain* hides it because that view compares every body side by side, so
per-body selection doesn't apply. It defaults to `s.active_body` when that's a non-star surface
body, else the lowest-id body.

**Resolved, not cut:** the Market section still lives inside *Tiles*, stacked under Buildings —
the BL-211 migration did **not** act on this doc's old recommendation to drop it. It still
duplicates the standalone Market Ledger's question; that overlap is accepted as shipped, not
revisited here.

## 3. Lens on open
Not wired. No code in `tile_inspector.cpp` arms an `overlay_mode` on open or on view switch — the
`resource`-on-open idea this doc floated was never implemented. **Open** — no authority resolves
this; LENSES.md does not document a menu-triggered arm for slot 9.

## 4. Data — live vs plumbing gaps
- **Live today (world state, no mock needed):** the whole surface reads directly off `w.tiles`,
  `w.buildings`, `w.markets`, and the persisted `generation_report` per selected body — tile
  fields, building workforce, market arrays, and the history/chain data are all live reads. This
  surface is the *most* live of the ledger family; it needs no CSV to function in-app.
- **Mock tables for the Power BI mockup:** `buildings.csv` (has `building_id, body_name, type,
  output, active, exhausted` — but **no workforce % / tile coordinate**, so the mockup can't
  reproduce the `[x,y] workforce %` line) and `markets.csv` (per-body supply/demand/price/base —
  carries the **5x body_id-inflation caveat**; a Market section in the mockup will over-count
  until a `market_id`/region column is added to the exporter).
- **GAP — no tile-field export.** There is **no `tiles.csv`** in the mock set. The Tiles view's
  primary table (composition/landform/hazard/habitability/deposits) has **no plumbing to Power
  BI** — Ben cannot mock the main table from CSV today. A `tiles.csv` exporter (one row per tile,
  columns mirroring the table) remains a **new plumbing item** if the mockup pass needs it.
- **No gap on history/chain data:** unlike the earlier draft of this doc, Story and Chain are
  **not** speculative — both read real, already-captured generation data (`body_entry.state`), so
  there is no missing time-series capture to call out here.

## 5. Close / toggle semantics
The rail slot-9 icon toggles the ledger open/closed; re-clicking the **currently-active sub-view
tab CLOSES the ledger** (`nav_button` passing `p_open`), not collapse-to-overview — standard
toggle-rule behaviour (`io-standing-rules.md` § Toggle rule). The **Body** combo is a
cross-cutting selector, not a view — switching bodies never closes anything. As a docked ledger it
shares the shell fold-out column with the other rail slots and the Selection element, so opening
History closes whatever else occupied that column.

## Open questions for Ben
- **Drop the Market section from Tiles?** It still duplicates the standalone Market Ledger's
  question and breaks one-question-per-surface — the BL-211 migration shipped without acting on
  this. Worth a follow-up cut, or is the overlap acceptable long-term?
- **Arm a lens on open?** No `overlay_mode` is wired today. Fixed `resource` on open, follow-the-view
  (Tiles→`resource`), or leave it unwired — which, if any, is worth building?
- **Is a `tiles.csv` exporter in scope for a future mockup pass?** Without it the primary table
  can't be mocked in Power BI.
