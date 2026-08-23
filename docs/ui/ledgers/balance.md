# Balance — design Q&A

> **Working design doc** for the ledger-mockup pass (Power BI). Strawman answers — Ben revises.
> Menu slot: `rail slot 2 "Budget"` · Source: `src/ui/balance_ledger.cpp` · Mock table(s): `cashflow.csv`, `player_timeseries.csv` · Owning items: `BL-155` (laws & policy surface), `BL-186` (laws ledger)
> Host: shell fold-out column, ~380px @1720 (derived — `shell_column_width(disp.x)`, 380–460 by resolution).

## 1. Top question — the one thing this answers at first glance
**"Am I solvent, and how long do I have?"** The solvency read on this surface is the **profit chart** — profit per economy tick (income − expenditure) over the last twelve ticks, drawn by `draw_profit_chart` with a zero baseline kept in range so an early-game loss-making series still reads as a climb. The runway figure itself ("how long until underwater") lives in the **header**, not here; Budget answers *why* the runway is what it is. The supporting decomposition below the chart: the **policy levers** (tax tier and wage tier), the **laws in force**, *what I own* (the Assets block: building count, income, cargo value), and *which buildings earn it* (the top-eight profit ranking with rank change against a year ago).

## 2. Sub-levels — views & default

The surface is **one flat panel** — `SeparatorText` sections stacked in a single scroll, with no `nav_button` tabs. The sections, in order:

| Section | Answers (one question) | Content |
|---|---|---|
| **Corp header + profit chart** | Am I earning or bleeding? | Corp name, then the twelve-tick profit polyline (gold, zero baseline, K-formatted gutter) from `player_plot_history.income − expenditure` |
| **Policy levers** | What posture have I set? | Two five-tier controls, **Taxes** (`ui.budget_tax_tier`) and **Wages** (`ui.budget_wage_tier`), each with a tooltip saying what it trades. The tiers are UI state only; their mechanics are `BL-155` (laws & policy surface) |
| **Laws** | Which laws bind me, whose are they, what do they cost? | One line per `world::laws` entry — name, author nation, rate per unit (or *repealed*); tooltip carries the levy explanation and any `condition_set` requirements. Read-only: enactment is the author nation's act, never a press here |
| **Assets** | What do I own that generates this? | Buildings owned (`cc.assets.size()`), Income this tick (`report.budgets[player].income`), Cargo Value (`player_stockpile_value`) |
| **Top buildings by profit** | Where does the profit come from? | `rank_player_buildings_by_profit`, top eight, profit per quarter coloured by sign, and a **vs yr** column (▲/▼ places moved against `prior_rank`, the ranking four economy ticks earlier, or *new*) |

**Proposed one-question-per-view tabs for the mockup** — Treasury (balance, debt flag, runway, starting capital + since-start delta) / Cashflow (the signed per-tick flow table: income, inputs bought, maintenance, wages, interest hidden when 0, net) / Trend (balance over ticks with income and expenditure bands, from `player_timeseries.csv`) / Assets (by-type and by-body roll-up). The itemised cashflow table is deliberately **not** on the live surface — Ben's call was that it is too much detail here and belongs in a dedicated breakdown menu.

**Default view on open:** Treasury (the solvency read is the reason to open Budget).
**Cross-cutting selectors (NOT views, exempt from toggle rule):** **none.** Budget is **player-only** — pinned to `w.player_entity`; there is no corporation combo, because rival treasuries are private under the competitor-visibility rule (`DISCOVERY.md`).

## 3. Lens on open
**None.** This is the one ledger that is genuinely about *your money*, not a map field — balance, burn, and runway have no per-body or per-tile spatial expression to arm. Opening Budget leaves `overlay_mode` as-is rather than forcing a lens. The nearest candidate is the **corporation** lens (identity-colour the map by owning corp), but that answers "who is where," not "how is my money" — it belongs to the Corporation dashboard, not here. Recommend: **none**, fixed.

## 4. Data sources
- Balance, starting capital, since-start delta, asset count → `corporation_component` (`cc.balance`, `cc.starting_capital`, `cc.assets`) — world state.
- Per-tick cashflow lines (income / expenditure / maintenance / wages / interest / net) → `report.budgets[corp].corp_budget`, populated by `apply_budget` (`FINANCE.md`). Mock mirror: **`cashflow.csv`** (Genom Systems row 30310: income 16.0007, exp 159.992, maint 20, wages 4.69, interest 5.15, net −173.83 — matches the negative-runway story).
- Profit series → `player_plot_history.income` / `.expenditure`, kept by the app per economy tick. Mock mirror: `player_timeseries.csv` (tick, balance, income, expenditure).
- Runway → a smoothed trailing net over `balance_history` plus `cc.balance`, read in the header.
- Laws → `world::laws` (`law::name`, `enacting_nation`, `rate`, `enacted`, `conditions`).

**Scope boundary — do NOT duplicate Economy's Cashflow:** the Cashflow tab proposed here and the Economy panel's Cashflow view read the same `corp_budget` feed, and at the player level are the *identical* table. **Balance owns per-corp money detail** — solvency, cashflow, runway; Economy's Cashflow view is the player-slice of that and defers to this surface.

**Mock-data notes:**
- **`starting_capital` is a placeholder in mock** (`corporations.csv` carries starting_capital=0, since_start=balance dup), so the "Since start" line reads as the full balance in mock data — live `corporation_component` has the real figure; don't tune off the CSV here.
- Interest is 0 for solvent corps and only populates in debt (row 30307 etc.) — the flow row hides it at 0, so a Cashflow table gains/loses a row across the solvency boundary. Correct behaviour, worth noting for the mockup so the row-count isn't treated as fixed.
- Assets on the live surface is a bare count plus two scalars; a by-type / by-body roll-up would join `buildings.csv` (building_id, type, output, active, exhausted).

## 5. Close / toggle semantics
Budget is a **standard rail-slot ledger**: the slot-2 icon toggles the ledger open/closed. Once tabbed (§2), re-clicking the **currently-active sub-view tab CLOSES the ledger** (not collapse-to-Treasury). There is no cross-cutting selector to exempt. Opening Budget closes whichever other ledger held the column (accordion, `close_all_panels`).

## Open questions for Ben
- **Does the Trend view earn a tab, or fold into Treasury?** The profit chart is the live trend read; `player_timeseries.csv` implies a fuller balance chart. Is that its own one-question view ("how did I get here"), or a sparkline under the Treasury balance? A full tab is cleaner per the one-question rule; a sparkline is cheaper.
- **How far does Assets go?** Bare building count plus income and cargo value, or a by-type/by-body roll-up joining `buildings.csv`? If it grows, does it risk duplicating the Corporation dashboard's asset view — and should it therefore stay a thin count here and live richly in Corporation?
- **Does the itemised cashflow table return here, or in a dedicated breakdown menu?** Ben's ruling removed it from this surface; the Cashflow tab above is the case for bringing it back as its own view.
- **Runway unit — quarters or years as the headline?** The header shows "~N quarters (X yr)". Which is the primary number for the at-a-glance read on the mockup?
