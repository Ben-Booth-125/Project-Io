# Corporation — design Q&A

> **Two surfaces share the word, and this doc is about the second.**
>
> - **Slot 1** is the **Corporation overview dashboard** — `src/ui/corporation_dashboard.cpp`.
>   Four roll-up cards (Production / Trade / Workforce / Finance), each resting as one verdict line
>   ("N making, N idle, X/qtr" · "N lanes, N convoys run" · "N % of labour demand met" ·
>   "±X/qtr, balance Y") and expanding in place or to a full-canvas drill. It is about the
>   **player's own corporation**, not the rival field. Owned by `BL-248` (corporation dashboard roll-ups).
> - **Slot 8** (Diplomacy, provisionally) hosts the **all-corporations table** this doc describes —
>   `src/ui/corporation_panel.cpp`, `foldout_begin("Corporations")`.
>
> Everything below describes the slot-8 rival-field table. Its long-run home is `BL-262`
> (scoring system), which owns the banded "how do I stand against whom" read, and `BL-449`
> (stance needs a surface) / `BL-475` (corp ledger stance detail), which own the stance column.

> **Working design doc** for the ledger-mockup pass (Power BI). Strawman answers — Ben revises.
> Menu slot: `nav rail slot 8` · Source: `src/ui/corporation_panel.cpp` · Mock table(s): `corporations.csv`, `cashflow.csv`
> Host: shell fold-out column, ~380px @1720.

## 1. Top question — the one thing this answers at first glance
**"Who are the corporations, and where do I stand among them?"** The table answers this literally: one row per corp, `Corporation | Reach | Capital | Share | Stance`, the player's row tinted (`IM_COL32(255,255,255,40)`). The first-glance read is the **standing profile** — am I ahead of or behind my rivals, on which axis. The three axes are `corp_standing` (`src/world/standing.hpp`): **Reach** (distinct bodies with ≥1 owned building), **Capital** (`balance`), **Share** (this corp's clearing income over total clearing income this tick). **The player's row shows exact figures; every rival row shows a band label only** — negligible / minor / notable / major / dominant, from fixed boundaries where a value exactly on a boundary lands in the higher band. Rival internals are private under the competitor-visibility rule (`DISCOVERY.md`), so no rival balance is ever printed, and **no totals or aggregate row exists** — `BL-262`'s hard rule. Secondary questions: *by what strategy is each competing* (focus mix), *how big is each* (building count), *who is climbing vs bleeding* (net cash direction, in `cashflow.csv`), and — the column the others don't carry — *how do we stand toward each other*. This is the **drill-down onto the field of players**; whole-economy aggregate belongs to Economy, not here.

**Stance column.** Per rival row: the label (Hostile / Friend / Neutral — `is_hostile`, `are_friends`), then the transition presses. **Declare Hostile** opens a confirm (*"Dissolves any friendship. Not unilaterally reversible."* → Declare / Cancel); **Offer Friendship** sends an offer (row reads *Offer sent*) and **Accept Friendship** answers one; **Return to Neutral** withdraws. Hostility is directed and declared, friendship symmetric and mutually chosen (`RELATIONS.md`, Ben 2026-08-17). The player's own row shows `-`.

**Ownership boundary (Corporation vs Economy).** This doc **owns the rival-field ranking**. Economy's Corps view claims a similarly-named balance list, but the two must not both answer it: **Economy defers the per-corp ranking to Corporation** and confines its own first glance to *whole-economy aggregate* (player totals, sector balance, income/expenditure trend). One question, one home.

## 2. Sub-levels — views & default

| View | Answers (one question) | Content |
|---|---|---|
| **Standings** (default) | How is each corporation placed vs. me? | The table above — name (player-tinted), Reach / Capital / Share (exact for the player, banded for rivals), Stance with its presses; footer `Corporations: N`. Proposed addition: a rank position and a signed **net/tick** direction from `cashflow.csv` so "who is climbing" is legible — banded for rivals like everything else. |
| **Profiles** *(proposed)* | What is each rival's *shape*? | Per-corp identity card list: home nation, focus, building count (`assets.size()`), starting vs. current balance — the columns the narrow table cannot hold (Home Nation, Status, Buildings), as a browsable second view rather than crammed back into the table. |

Default on open: **Standings**. It is the ranking that answers the top question at a glance.
Cross-cutting selectors (NOT views, exempt from the toggle rule): **none needed** — the corp set is small and fully enumerable (8 corps in mock). A future *sort selector* (by reach / capital / share) would be a selector, not a view.

**Asset-count ownership (three-way).** The Profiles building count (`assets.size()`) is the **per-corp** building count, and Corporation **owns** it. That is distinct from Balance's **Assets** figure (the player corp's own count, income and cargo value) and from Economy's **Sector** composition (the *aggregate* building composition across the whole field). Per-corp counts here, player-self in Balance, whole-field composition in Economy — no three-way overlap.

The leaner alternative, and the one the table already takes: **one table, no tabs, and a row-click routes full detail to the Selection element** (`Selectable` spanning all columns sets `s.selected_entity = id`). That keeps Corporation genuinely single-question and leans on the shared Selection element for per-corp depth. I lean toward it — see open questions.

## 3. Lens on open
Arms **`overlay_mode::corporation`** (per-corp tile tint, player-corp border) — the candidate lens for this slot, and consistent with Ben's "opening a menu usually should arm a lens." Rationale: the Corporation ledger is *about the field of players*, and the corporation lens paints exactly that field onto the canvas (whose buildings are whose), so the ledger and the map answer the same question in two registers. **Fixed on open**, not sub-view-following — both Standings and Profiles are about the same "who owns what" question. `ui_state.overlay` defaults to `none` at campaign start, so arming here is an explicit set. This is a proposal; the panel arms no lens.

## 4. Data sources
Every field the table needs exists on `corporation_component` (`src/world/components.hpp`) or `corp_standing`:
- **name / is_player** — `corporation_component`.
- **reach_bodies / capital_balance / market_share** and the three bands — `corp_standing`, computed by `standing.hpp` from `w.buildings` and the tick's clearing income; the UI decides which to print.
- **stance** — `is_hostile` / `are_friends` over the stance table (`stance.hpp`); the presses issue the stance verbs.
- **home_nation** (`entity_id`) — needs a nation-name lookup to display (mock `corporations.csv` carries the resolved string).
- **assets** (`std::vector<entity_id>`) — `assets.size()` is the building count (mock `buildings` column).
- **net / income / expenditure / interest** — the money loop (`budget_system.hpp`), surfaced in mock `cashflow.csv`; the proposed net/tick direction would read `report.budgets`, banded for rivals.

Mock-data notes:
- **`starting_capital` is a placeholder** (0 in mock `corporations.csv`, and `since_start` is just a dup of `balance`). Any "growth since start" framing in Profiles needs the exporter to emit a true opening balance.
- **No time-series per rival.** `player_timeseries.csv` covers only the player corp; there is no per-corp balance history, so a "who is climbing over time" sparkline has no feed — the signed net/tick is the available proxy, and under the visibility rule it would be banded anyway.

## 5. Close / toggle semantics
Nav-rail slot-8 icon toggles the ledger open/closed. With **two views**: re-clicking the **currently-active tab (Standings or Profiles) closes the ledger** — not collapse-to-overview. Switching between the two tabs is an ordinary view change. With the **single-table option** (no tabs), the toggle is the rail icon alone: click to open, click again to close; a row-click selects a corp and routes to the Selection element. The stance presses are in-row actions, never toggles. Opening the ledger arms the corporation lens (proposed); closing it does **not** auto-disarm the lens (lens state is canvas-owned and persists).

## Open questions for Ben
- **Two views or one table?** I lean **one table + Selection-element drill-down** (what the table does, and avoids duplicating detail the Selection already shows per corp). Confirm, or keep the **Standings/Profiles** split if you want rival profiles browsable without a selection.
- **Net/tick direction.** Add a signed (banded for rivals) net-per-tick to Standings so the ranking shows *direction*, not just a static snapshot? It needs `report.budgets` plumbed into the panel.
- **Lens: fixed or follow?** I set it **fixed = corporation** on open. Confirm you don't want it disarmed when the ledger closes (I left the lens persisting).
- **Sort selector.** Do you want a by-reach / by-capital / by-share sort combo (a selector, not a view), or is the fixed deterministic `entity_id` order enough for the prototype?
- **Does the table survive `BL-262` (scoring system), or does the scoring surface absorb it?** The banded profile is already the scoring system's first slice; the stance column is the part with no other home short of `BL-475` (corp ledger stance detail).
