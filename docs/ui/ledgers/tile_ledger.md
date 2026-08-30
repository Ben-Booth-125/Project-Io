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

Four `nav_button` views on `s.history_view` (`history_view_id` in `ui/tile_inspector.hpp` —
`story` / `chain` / `ages` / `tectonics`; an out-of-range saved index lands on Story rather than
on nothing):

| View | Answers (one question) | Content |
|---|---|---|
| **Story** | What happened here, in order? | Body summary line (`name | AU | WxH tiles`), then the dated biography — planetology, plate, and history-ladder events in sequence (`format_history_date`, whose unit comes from the date's own magnitude so deep-time and historical lines share one loop), each with an optional wrapped consequence line underneath; "No recorded history for this body" when the report has none. Takes the full-canvas disclosure control alone (the in-place one has nothing to expand) |
| **Chain** | How did the generation chain arrive at this body? | The wizard's stage charts (`generation_charts.cpp`), re-rendered from the persisted `generation_report`, grouped into three rounds (**System / Life / Legacy**) via a second `nav_button` strip; one collapsing accordion per stage, only the round's first stage open by default. Every body side by side — the comparison *is* the view. No view-level disclosure control: each stage carries its own |
| **Ages** | How did its polities rise and fall? | Transport (Play / Pause, Restart, a signed-year slider reading `400 BCE` … `0 CE`, the run playing through in a fixed ~16 s whatever the span), the replayed map over the body's actual terrain, and the multipolarity line `year | N regions | N powers`. The replay is **generation's own era** — the span, clock and seed come from `era_minus_one_sim_params` / `era_minus_one_sim_seed`, never from a second set constructed at the call site (`world/era_minus_one.hpp` § why a fixture rather than a second derivation). The sim is cached on the body's entity id **and** the generation's identity, so a regenerated world never replays the previous world's history over new regions. "Never settled" bodies say so instead of replaying. No disclosure control: the map sizes itself to its column |
| **Tectonics** | Which plates made this ground, and where do they meet? | The plate field over the body, reached from the canvas as well as the tab — a plate press under the Continent lens routes here (`CONTINENTS.md` § the Continent lens). |

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

## The direction this surface is pointed (Ben, 2026-08-30)

**The slot answers the wrong question.** Every one of the four views above narrates
**generation** — how the body formed, how the chain arrived at it, how its polities settled, which
plates made its ground. All of it is true and none of it is *strategy*. Ben, 2026-08-30:

> History at present tells the story of generation. Rather it should in fact answer questions
> about strategy that goes deeper into the meta-game.

The examples he named, which are the shape of the target rather than a list to build:

- **Which provinces are claimed by other nations** — the territorial picture as a *claim* state,
  not a settled ownership fact.
- **Who your nation expects to be fighting soon** — an expectation, forward-looking, off the
  stance and sentiment quantities `RELATIONS.md` owns.
- **Which resources are becoming depleted** — a *derivative*, not a level: the deposit field has
  magnitudes today, and what this asks for is their trend.

The common shape is worth naming, because it is what makes the target one surface rather than
three: **all three are a trend or an expectation, not a state.** The existing views are all
state — a finished biography, a finished chain, a finished era. Every question above is about
where a line is *going*, which is why none of them can be read off the surfaces that exist.

**None of this is buildable yet, and that is the finding rather than an obstacle.** Provincial
claims distinct from holdings, a nation's expectation of war, and a depletion trend are three
different pieces of missing infrastructure, in three different layers. So the ledger **stays as
it is** — the direction is recorded here to be designed against, not acted on now.

## Open questions for Ben

### The strategic turn
- **Does History become the strategy surface, or does the strategy surface take the slot and
  generation-history move?** The four existing views are a coherent answer to a real question
  ("how did this body get to be what it is?") and the manual sends a player here for it. A slot
  that answers both is two ledgers sharing a rail icon.
- **Is the unit the BODY or the NATION?** Every view today is per-body, behind a body combo. All
  three of Ben's examples are per-**nation** — claims, expectations, depletion of *someone's*
  ground. If the answer is nation, the body combo is the wrong cross-cutting selector for the new
  half of the surface, and the seam runs right through the middle of the ledger.
- **What is a claim, and where does it live?** `NATIONS.md` owns territory and `province_holder`
  records who holds a province; neither carries a claim a nation asserts and does not hold. A
  claim is the thing the first question needs and the thing that does not exist.
- **Is "expects to be fighting soon" a derived read or a stored intent?** Derived off stance and
  sentiment it costs nothing and can be wrong in a way the player cannot appeal. Stored as a
  scored intent it is honest but puts a planner-shaped object in the nation layer, which the
  standing rules' nation grant admits only as deterministic scored utility.
- **Does a depletion trend need history the world does not keep?** Deposit magnitude is a level;
  a trend needs at least two samples. Nothing today records the deposit field's past, so this is
  a storage question (and a save-seam one) before it is a UI question.
- **Does any of this belong to History at all, or to Diplomacy and a Resources ledger?** The
  honest reading is that Ben's three examples split cleanly across two future surfaces, and the
  reason they feel like one is that all three are *what a player should be worrying about* —
  which may be the actual top question, and a different surface from either.

### Carried from the mockup pass
- **Arm a lens on open?** Fixed `resource` on open, a Chain → generation-field overlay pairing
  (`BL-304`), or leave it unwired — which, if any, is worth building?
- **Is a history / tile export in scope for a future mockup pass?** Without it none of the views
  can be mocked in Power BI.
- **Does Ages belong under History, or under a future Diplomacy surface?** It is generation
  history and so fits the slot's rule, but its multipolarity read is the number the nation layer
  will want live. The strategic turn above sharpens this rather than settling it.

### Raised by the Ages replay
- **What should Ages replay?** It re-runs the era from the settlement state as it stood *after*
  generation's own sim, so every region already exists and is owned. The measured consequence:
  the replay reports **0 battles and 0 conquests** and shows region count growing 1343 → 2090
  with the power count pinned at 12. It is a settlement time-lapse wearing a political one's
  label. The alternative is for generation to record its own `owner_changes` into the report and
  for this view to replay that instead of re-simulating — which is a save-format change, and
  therefore yours.
- **Is a multi-minute view acceptable at all?** Even replaying the right era, the run costs
  minutes on the drawing thread. If the answer to the previous question is "replay the recorded
  timeline", this dissolves; if it is "keep re-simulating", the sim needs to come off the draw
  thread with progress and a way back.
