# Balance — design Q&A

> **Working design doc** for the ledger-mockup pass (Power BI). Strawman answers — Ben revises.
> Menu slot: `rail slot 2 "Budget"` · Source: `src/ui/balance_ledger.cpp` · Mock table(s): `cashflow.csv`, `player_timeseries.csv` · Related: `BL-072` (cashflow/runway), `BL-118` (Treasury/Cashflow/Assets split), `BL-122` (fold-out host)
> Host: shell fold-out column (BL-122), ~380px @1720 (derived — `shell_column_width(disp.x)`, 380–460 by resolution).

## 1. Top question — the one thing this answers at first glance
**"Am I solvent, and how long do I have?"** The first-glance read is the balance figure (colour-coded pos/neg) and the projected **runway** in quarters at current burn. Everything else on the surface is the supporting decomposition: *what moved the balance last tick* (the signed cashflow lines — income, inputs, maintenance, wages, interest, net) and *what I own* (building count). Secondary questions that justify sub-views: **why** is my net what it is (line-item cashflow), and **how did I get here** (the balance/income/expenditure trend over ticks, which the code does not yet render but `player_timeseries.csv` exists to feed).

## 2. Sub-levels — views & default

> **Three corrections against code, 2026-08-04.** (1) There are **two** `SeparatorText` sections,
> not three — *Assets* and *Top buildings by profit*. (2) **Runway is not on this surface** —
> `grep runway balance_ledger.cpp` returns nothing, so § 1's "the first-glance read is the
> projected runway" is aspirational; the runway readout lives in the header (BL-177). (3) **There
> is no Corporation combo and no `selected_corp`** — BL-142 pinned the surface to the player, which
> also settles the open question in § 6: Budget is player-only.

Code today is **one flat panel** with `SeparatorText` sections stacked in a single scroll — BL-118's "split" is a visual grouping, **not** nav_button tabs. Proposed one-question-per-view tabs for the fold-out:

| View | Answers (one question) | Content |
|---|---|---|
| **Treasury** *(default)* | Am I solvent and how long do I have? | Balance (colour-coded), debt flag, projected runway in quarters/years (smoothed burn for player), Starting capital + Since-start delta |
| **Cashflow** | Why is my net what it is? | The signed per-tick flow table: Income (sales), Inputs bought, Maintenance, Wages, Interest (hidden when 0), Net/tick |
| **Trend** *(proposed — not in code)* | How did my balance get here? | Line chart of balance over ticks + income vs expenditure bands; from `player_timeseries.csv`. This is the mock table with no live renderer yet. |
| **Assets** | What do I own that generates this? | Buildings-owned count today; proposed to grow into a by-type / by-body asset roll-up |

**Default view on open:** Treasury (the solvency read is the reason to open Budget).
**Cross-cutting selectors (NOT views, exempt from toggle rule):** **none.** *(This described a Corporation combo on `selected_corp`; neither exists — BL-142 pinned the surface to the player. Corrected 2026-08-04.)*

## 3. Lens on open
**None.** This is the one ledger that is genuinely about *your money*, not a map field — balance, burn, and runway have no per-body or per-tile spatial expression to arm. Opening Budget should leave `overlay_mode` as-is rather than force a lens. The nearest candidate is the **corporation** lens (identity-colour the map by owning corp), but that answers "who is where," not "how is my money" — it belongs to the Corporation dashboard, not here. If Ben wants the "opening a menu arms a lens" default honoured, **corporation** is the only defensible pick, and only when the Corporation combo is off the player (i.e. you're inspecting a rival's money and want to see their footprint). Recommend: **none**, fixed.

## 4. Data — live vs plumbing gaps
**Live / wired today:**
- Balance, starting capital, since-start delta, asset count → `corporation_component` (`cc.balance`, `cc.starting_capital`, `cc.assets`) — real world state.
- Per-tick cashflow lines (income / expenditure / maintenance / wages / interest / net) → `report.budgets[corp].corp_budget` — real, populated by `apply_budget` (BL-072). Mock mirror: **`cashflow.csv`** (Genom Systems row 30310: income 16.0007, exp 159.992, maint 20, wages 4.69, interest 5.15, net -173.83 — matches the negative-runway story).
- Runway → derived from `balance_history` (smoothed 4-tick trailing net for the player) + `cc.balance`.

**Scope boundary — do NOT duplicate Economy's Cashflow:**
- Both this doc's **Cashflow** tab and the Economy panel's **Cashflow** view read the same `cashflow.csv` / `corp_budget` source, and at the **player level** they are the *identical* income/expenditure/maintenance/wages/interest/net table. That is a duplication hazard: two docs must not both ship a player income/expenditure table off the same feed. The split is **Balance owns per-corp money detail** — solvency, cashflow, runway — as the money ledger; Economy's Cashflow view is the *player-slice* of that and should either defer to this table or be dropped from Economy in favour of a link here. **Flag for the Economy doc so the two don't both render it.**

**Plumbing gaps:**
- **Trend view has no renderer.** `player_timeseries.csv` (tick,balance,income,expenditure over 16 ticks) exists as the intended feed but nothing in `balance_ledger.cpp` draws a time series — only the scalar `smoothed_net` reads `balance_history`. A trend sub-view needs a plot widget wired to the balance history buffer. **Flag: mock table with no live surface.**
- **Assets is a bare count.** `cc.assets.size()` only. A by-type/by-body roll-up would need to join `buildings.csv` (building_id,type,output,active,exhausted) — that data exists in mock but isn't surfaced here.
- **`starting_capital` is a placeholder in mock** (`corporations.csv` notes starting_capital=0, since_start=balance dup), so the "Since start" line reads as the full balance in mock data — live `corporation_component` has the real figure; don't tune off the CSV here.
- Interest is 0 for solvent corps and only populates in debt (row 30307 etc.) — the flow_row hides it at 0, so the Cashflow table silently gains/loses a row across the solvency boundary. Correct behaviour, worth noting for the mockup so the row-count isn't treated as fixed.

## 5. Close / toggle semantics
Budget is a **standard rail-slot ledger**: the slot-2 icon toggles the ledger open/closed. Once tabbed (§2), re-clicking the **currently-active sub-view tab CLOSES the ledger** (not collapse-to-Treasury). The **Corporation combo is a cross-cutting selector, exempt** — changing corp re-populates in place, never toggles. Opening Budget takes the fold-out column from any active Selection (mutual exclusion, BL-122/BL-123); the Selection persists behind it and reappears on close.

## Open questions for Ben
- **Is Budget player-only, or per-corp?** The code keeps a full Corporation combo defaulting to the player. If Economy=aggregate and Corporation is the rival drill-down, does Budget need the combo at all — or is "your money, full stop" the cleaner one-question framing (drop the combo, always player)? This also decides whether the corporation-lens-on-rival idea in §3 is even reachable.
- **Does the Trend view earn a tab, or fold into Treasury?** `player_timeseries.csv` implies a wanted chart. Is it its own one-question view ("how did I get here"), or a sparkline under the Treasury balance? A full tab is cleaner per the one-question rule; a sparkline is cheaper.
- **How far does Assets go?** Bare building count (today), or a by-type/by-body roll-up joining `buildings.csv`? If it grows, does it risk duplicating the Corporation dashboard's asset view — and should it therefore stay a thin count here and live richly in Corporation?
- **Runway unit — quarters or years as the headline?** Code shows "~N quarters (X yr)". Which is the primary number for the at-a-glance read on the mockup?
