# History ledger — design Q&A

> **Working design doc** for the ledger-mockup pass (Power BI). Strawman answers — Ben revises.
> Menu slot: `rail slot 9 "History"` · Source: `src/ui/tile_inspector.cpp` (the file keeps the surface's original name; the fold-out title is `"Tile Ledger"`) · Mock table(s): `buildings.csv`, `markets.csv` (tile-field data: none exported) · Owning items: `BL-425` (Ages lazy sim), `BL-304` (generation field-overlay lenses)
> Host: shell fold-out column (`foldout_begin`), docked.

## 1. Top question — the one thing this answers at first glance
**"How did this body get to be what it is?"** — three views answer three related but distinct
questions in one column: *Story* ("what happened here?" — the oral-history biography drawn from
`generation_report::body_entry::state.history`), *Chain* ("how did the generation chain arrive at
this?" — the wizard's own stage charts, redrawn from the persisted report, grouped System / Life /
Legacy), and *Ages* ("how did its polities rise and fall?" — the Era −1 political history replayed
as an animated map over the body's real terrain). The slot is **generation history, not a live
event log** (`MENU.md` § slot 9). Per-tile ground truth — terrain, buildings, market — lives where
tile selection already puts it: the canvas and the Selection element, never here.

## 2. Sub-levels — views & default

Three `nav_button` views on `s.history_view` (`view_story` / `view_chain` / `view_ages`; an
out-of-range saved index lands on Story rather than on nothing):

| View | Answers (one question) | Content |
|---|---|---|
| **Story** | What happened here, in order? | Body summary line (`name | AU | WxH tiles`), then the dated biography — planetology, plate, and history-ladder events in sequence (`format_history_date`, whose unit comes from the date's own magnitude so deep-time and historical lines share one loop), each with an optional wrapped consequence line underneath; "No recorded history for this body" when the report has none. Takes the full-canvas disclosure control alone (the in-place one has nothing to expand) |
| **Chain** | How did the generation chain arrive at this body? | The wizard's stage charts (`generation_charts.cpp`), re-rendered from the persisted `generation_report`, grouped into three rounds (**System / Life / Legacy**) via a second `nav_button` strip; one collapsing accordion per stage, only the round's first stage open by default. Every body side by side — the comparison *is* the view. No view-level disclosure control: each stage carries its own |
| **Ages** | How did its polities rise and fall? | Transport (Play / Pause, Restart, a `%d CE` year slider, ~120 years a second), the replayed map over the body's actual terrain, and the multipolarity line `year CE | N regions | N powers`. The sim is cached on the body's entity id **and** the generation's identity, so a regenerated world never replays the previous world's history over new regions. "Never settled" bodies say so instead of replaying. No disclosure control: the map sizes itself to its column |

**Default view:** whatever `s.history_view` was last left on (persisted in `ui_state` so a verify
script can park it) — there is no forced default-to-Story on every open.

**Cross-cutting selector (NOT a view, exempt from the toggle rule):** the **Body** combo, shown on
*Story* only — *Chain* compares every body, and *Ages* follows Story's choice. It defaults to
`s.active_body` when that is a non-star surface body, else the lowest-id body.

## 3. Lens on open
**None.** Nothing in `tile_inspector.cpp` arms an `overlay_mode` on open or on view switch, and no
authority resolves whether it should; `LENSES.md` documents no menu-triggered arm for slot 9. The
natural candidates are `BL-304`'s generation field overlays (heightmap, moisture, band, plate),
which are the map twin of the Chain view — an open question below.

## 4. Data sources
- **World state, no mock needed:** the whole surface reads the persisted `generation_report` per
  selected body (`body_entry.state.history` for Story, the stage intermediates for Chain, the
  settlement and history-sim state for Ages) plus `w.bodies` and `w.tiles` for the Ages terrain.
  This surface is the *most* live of the ledger family; it needs no CSV to function in-app.
- **Mock tables for a Power BI mockup:** `buildings.csv` and `markets.csv` carry the current-state
  estate and markets, which this surface does not show. There is **no `tiles.csv`** and no
  history / chain export in the mock set, so none of the three views can be mocked from CSV; a
  per-body event export (date, event, consequence) would be the exporter addition a mockup pass
  needs.

## 5. Close / toggle semantics
The rail slot-9 icon toggles the ledger open/closed; re-clicking the **currently-active sub-view
tab CLOSES the ledger** (`nav_button` passing `p_open`), not collapse-to-overview — standard
toggle-rule behaviour (`io-standing-rules.md` § Toggle rule). The **Body** combo is a
cross-cutting selector, not a view — switching bodies never closes anything; the Chain round strip
and the Ages transport are in-view controls. As a docked ledger it shares the shell fold-out
column with the other rail slots, so opening History closes whatever else occupied that column
(accordion, `close_all_panels`).

## Open questions for Ben
- **Arm a lens on open?** Fixed `resource` on open, a Chain → generation-field overlay pairing
  (`BL-304`), or leave it unwired — which, if any, is worth building?
- **Is a history / tile export in scope for a future mockup pass?** Without it none of the three
  views can be mocked in Power BI.
- **Does Ages belong under History, or under a future Diplomacy surface?** It is generation
  history and so fits the slot's rule, but its multipolarity read is the number the nation layer
  will want live.
