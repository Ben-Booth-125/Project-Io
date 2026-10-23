# Balance — design Q&A

> **Working design doc** for the ledger-mockup pass (Power BI). Strawman answers — Ben revises.
> Menu slot: `rail slot 2 "Budget"` · Source: `src/ui/balance_ledger.cpp` · Mock table(s): `cashflow.csv`, `player_timeseries.csv` · Owning items: `BL-155` (laws & policy surface), `BL-186` (laws ledger)
> Host: shell fold-out column, ~380px @1720 (derived — `shell_column_width(disp.x)`, 380–460 by resolution).

## 1. Top question — the one thing this answers at first glance
**"Am I solvent, and how long do I have?"** The solvency read on this surface is the **profit chart** — profit per economy tick (income − expenditure) over the last twelve ticks, drawn by `draw_profit_chart` with a zero baseline kept in range so an early-game loss-making series still reads as a climb. The runway figure itself ("how long until the treasury runs dry") lives in the **header**, not here; Budget answers *why* the runway is what it is. The supporting decomposition below the chart: the **policy levers** (tax tier and wage tier), the **laws in force**, *what I own* (the Assets block: building count, income, cargo value), and *which buildings earn it* (the top-eight profit ranking with rank change against a year ago).

## 2. Sub-levels — views & default

The surface is **one flat panel** — `SeparatorText` sections stacked in a single scroll, with no `nav_button` tabs. The sections, in order:

| Section | Answers (one question) | Content |
|---|---|---|
| **Nation header + profit chart** | Am I earning or bleeding? | The player nation's name, then the twelve-tick profit polyline (gold, zero baseline, K-formatted gutter) from `player_plot_history.income − expenditure` |
| **Policy levers** | What posture have I set? | Two five-tier controls, **Taxes** (`ui.budget_tax_tier`) and **Wages** (`ui.budget_wage_tier`), each with a tooltip saying what it trades. The tiers are UI state only; their mechanics are `BL-155` (laws & policy surface) |
| **Laws** | Which laws bind me, whose are they, what do they cost? | One line per `world::laws` entry — name, author nation, rate per unit (or *repealed*); tooltip carries the levy explanation and any `condition_set` requirements. Read-only: enactment is the author nation's act, never a press here |
| **Assets** | What do I own that generates this? | Buildings owned (every building on the player nation's ground), Income this tick (`report.budgets[player].income`), Cargo Value (`player_cargo_value` — the trades' cargo in transit, shared with the header's CARGO) |
| **Top buildings by profit** | Where does the profit come from? | `rank_player_buildings_by_profit`, top eight, profit per quarter coloured by sign, and a **vs yr** column (▲/▼ places moved against `prior_rank`, the ranking four economy ticks earlier, or *new*) |

**Proposed one-question-per-view tabs for the mockup** — Treasury (treasury, runway, starting treasury + since-start delta) / Cashflow (the signed per-tick flow table: income, inputs bought, maintenance, wages, net) / Trend (treasury over ticks with income and expenditure bands, from `player_timeseries.csv`) / Assets (by-type and by-body roll-up). The itemised cashflow table is deliberately **not** on the live surface — Ben's call was that it is too much detail here and belongs in a dedicated breakdown menu.

**Default view on open:** Treasury (the solvency read is the reason to open Budget).
**Cross-cutting selectors (NOT views, exempt from toggle rule):** **none.** Budget is **player-only** — pinned to the player's nation; there is no nation combo, because rival treasuries are private under the competitor-visibility rule (`DISCOVERY.md`).

## 3. Lens on open
**None.** This is the one ledger that is genuinely about *your money*, not a map field — treasury, burn, and runway have no per-body or per-tile spatial expression to arm. Opening Budget leaves `overlay_mode` as-is rather than forcing a lens. The nearest candidate is the **Nations** lens (each nation's territory in its colour), but that answers "who is where," not "how is my money". Recommend: **none**, fixed.

## 4. Data sources
- Treasury, starting treasury, since-start delta, building count → the player nation's treasury and holdings (`nation_component::treasury`; re-subjected from the retired corporation's balance, Ben, 2026-10-23) — world state.
- Per-tick cashflow lines (income / expenditure / maintenance / wages / net) → the nation's quarterly return, populated by `apply_budget` (`FINANCE.md`; `../../GLOSSARY.md` § Quarterly return). There is no interest line: a nation never runs into debt (`../../politics/NATIONS.md` § What a nation owns). Mock mirror: **`cashflow.csv`** (a pre-2026-10-23 corporation row; its interest column has no subject).
- Profit series → `player_plot_history.income` / `.expenditure`, kept by the app per economy tick. Mock mirror: `player_timeseries.csv` (tick, balance, income, expenditure — the balance column reads as the treasury).
- Runway → a smoothed trailing net over `balance_history` plus the treasury, read in the header.
- Laws → `world::laws` (`law::name`, `enacting_nation`, `rate`, `enacted`, `conditions`).

**Scope boundary — this ledger owns the cashflow table.** **Balance owns the player nation's money detail** — solvency, cashflow, runway, all off the nation's budget feed. Any other surface wanting the player's cashflow is asking for the identical table and defers here; a second view of the same six lines is a duplicate, not a summary.

**Mock-data notes:**
- **`starting_capital` is a placeholder in mock** (`corporations.csv` carries starting_capital=0, since_start=balance dup), so the "Since start" line reads as the full treasury in mock data; don't tune off the CSV here. The mock tables predate the retirement of the corporation (Ben, 2026-10-23).
- Assets on the live surface is a bare count plus two scalars; a by-type / by-body roll-up would join `buildings.csv` (building_id, type, output, active, exhausted).

## 5. Close / toggle semantics
Budget is a **standard rail-slot ledger**: the slot-2 icon toggles the ledger open/closed. Once tabbed (§2), re-clicking the **currently-active sub-view tab CLOSES the ledger** (not collapse-to-Treasury). There is no cross-cutting selector to exempt. Opening Budget closes whichever other ledger held the column (accordion, `close_all_panels`).

## Open questions for Ben
- **Does the Trend view earn a tab, or fold into Treasury?** The profit chart is the live trend read; `player_timeseries.csv` implies a fuller balance chart. Is that its own one-question view ("how did I get here"), or a sparkline under the Treasury figure? A full tab is cleaner per the one-question rule; a sparkline is cheaper.
- **How far does Assets go?** Bare building count plus income and cargo value, or a by-type/by-body roll-up joining `buildings.csv`? If it grows, does it risk duplicating the Nation overview — and should it therefore stay a thin count here?
- **Does the itemised cashflow table return here, or in a dedicated breakdown menu?** Ben's ruling removed it from this surface; the Cashflow tab above is the case for bringing it back as its own view.
- **Runway unit — quarters or years as the headline?** The header shows "~N quarters (X yr)". Which is the primary number for the at-a-glance read on the mockup?
