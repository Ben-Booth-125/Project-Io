# Economy (aggregate) — design Q&A

> **Working design doc** for the ledger-mockup pass (Power BI). Strawman answers — Ben revises.
> Menu slot: `rail slot 3 "Workforce"` — a provisional host: the slot keeps its real subject's name and glyph, and carries the Economy panel because that panel would otherwise have no door (`MENU.md` § Structure) · Source: `src/ui/economy_panel.cpp` · Mock table(s): `player_timeseries.csv, cashflow.csv, workforce.csv, buildings.csv, corporations.csv` · Owning items: `BL-482` (economy panel pools leak)
> Host: shell fold-out column, ~380px @1720 (derived — `shell_column_width(disp.x)`, 380–460 by resolution).

> **Lead decision — does Economy survive as a distinct surface?** Before the tab shape below is taken as the target, note the overlap this pass surfaced: Economy **substantially re-implements Balance + Corporation**. A Cashflow view duplicates the Balance ledger's Cashflow tab *exactly* (same `cashflow.csv`, same six lines, same player corp `30310`); the Corps view's balance list duplicates Corporation's **Standings**; and §2 already hands Holdings→Corporation and Markets→Market. Strip those and what is genuinely *aggregate-only* — neither a single corp's ledger nor a single market's board — is thin: the **Sector composition bar** and **Workforce**. That thinness is exactly why open-Q (B) *"fold Economy into Corporation"* is live. **Read this doc as the "does Economy earn its own rail slot" decision first, and the tab layout as the strawman that mostly loses to Balance + Corporation second.**

## 1. Top question — the one thing this answers at first glance
**"Is my whole enterprise winning or losing, and where is the money going?"** At a glance: my balance trend (the `player_timeseries.csv` curve that bleeds from +1963 to −262 over 16 ticks in the mock — the single most alarming fact in it, and the thing an aggregate view exists to catch early), and where I stand among the corps. Secondary questions that *would* justify sub-views: *how is my cash split between earning and spending* (cashflow composition — wages/maintenance/interest), *how balanced is my sector footprint* (extraction vs processing buildings), and *is labour throttling me anywhere* (workforce staffing across holdings).

**But note the ownership overlap up front:** "how is my cash split" is answered by the **Balance** ledger, and "how do I rank against rival corps" by **Corporation**'s Standings. Neither is aggregate-only. Once those are handed back to their owners, the only cross-cutting roll-ups left are the **sector footprint** and **workforce contention** — genuinely all-corps/all-bodies, but a thin pair to hang a whole surface on. Per-corp drill-down is the Corporation ledger; per-market is the Market ledger.

## 2. Sub-levels — views & default

The surface has three `nav_button` views (`ui.economy_view`), each drawing exclusively so the panel reads in the narrow column:

| View | Answers (one question) | Content | Aggregate-only? |
|---|---|---|---|
| **Corps** *(default)* | "How are the corporations doing?" | `draw_balance_trend` (player balance plot, then income and spend side by side from `player_plot_history`), `draw_balances` (one row per corp, name and balance, player row pinned, negative red), `draw_workforce` (labour contention per corp × body from `economy_report.workforce_contention`; "All buildings fully staffed" when none) | **Partly.** The trend plot and the contention table are aggregate; the balance list **collides with Corporation** — and prints exact rival balances, which the competitor-visibility rule forbids and Corporation's banded table already answers. Corporation owns "corp rank"; Economy must not re-answer it. |
| **Holdings** | "What do I hold, and where?" | `draw_pools` — stockpile pools per (corp × body), quantities per resource | **No — collides with Corporation / Selection**, and lists *every* corp's pools: `BL-482` (economy panel pools leak) owns redacting rival rows to the player-only / god-view standard the Selection and hover surfaces meet. |
| **Markets** | "What is the market doing?" | `draw_markets` — supply / demand per market | **No — collides with Market.** |

**Proposed aggregate-only residue**, were the surface to keep a slot:

| View | Answers (one question) | Content |
|---|---|---|
| **Sector** *(proposed default)* | "Is my footprint balanced?" | Extraction-vs-processing tally across all my holdings, from `w.buildings` (type, active, exhausted). A composition bar, not a building list. |
| **Workforce** | "Is labour throttling me?" | `draw_workforce` — staffing contention across (corp × body). |

**Aggregate-only taxonomy note.** A view earns a place on this surface only if it answers a question that is *neither* a single corp's ledger *nor* a single market's board. By that test **four collisions** fall out:
- **Holdings → Corporation ledger / Selection** (`draw_pools`).
- **Markets → Market ledger** (`draw_markets`).
- **Cashflow → Balance ledger** (would duplicate its Cashflow tab exactly).
- **Corps' balance list → Corporation's Standings** (duplicates the ranking, and un-banded).

After removing all four, the genuinely aggregate-only residue is **Sector** + **Workforce** — and a bare player-balance *trend* plot that has no other home. That residue is thin enough that folding it into Corporation (open-Q B) is the live alternative to keeping a rail slot.

**Default view on open (if the surface survives):** **Sector** — the one load-bearing aggregate-only question.
**NOT views (cross-cutting selectors):** none needed — this surface is all-corps/all-bodies aggregate by definition. A "me only vs all corps" toggle would be a filter, not a tab.

## 3. Lens on open
**None.** `overlay_mode::industry` does **not** paint the player's extraction/processing sector balance. It is the **per-tile nation-substrate throughput field** (occupation × terrain richness — the AI-owned *broad* economy; see `LENSES.md` § Industry lens / `ui_state.hpp`). Arming it for the **Sector** view would light up the *nation substrate*, not the corp building mix — the opposite of what the view is asking.

Two honest options:
- **Arm none.** Sector-balance is a composition of *player holdings*, and no single lens paints exactly that. Leaving the lens untouched is the truthful default.
- **Arm Corporation.** If a lens should follow the surface, **Corporation** is the one that actually paints corp holdings on the canvas — the nearest map echo of "where my sector footprint sits". This is my recommendation over "none" if Ben wants a lens armed at all.

**Do not arm Industry.** Beyond painting the wrong thing, Industry is **keyboard-cycle-only, off the visible minimap lens bar** — arming it from a ledger would surface a lens the player can't reach from the strip, which is inconsistent chrome.

## 4. Data sources
- **World state:** balance trend (`player_plot_history`, `draw_balance_trend`), corp balances (`w.corporations[].balance`), workforce contention (`economy_report.workforce_contention`), pools (`w.stockpiles`), markets (`w.markets`).
- **Mock-backed:** cashflow split (`cashflow.csv` has income/expenditure/maintenance/wages/interest/net per corp) — but this is the Balance ledger's data, not Economy's to plumb (see §2). A Sector tally is derivable from `buildings.csv` / `w.buildings` (type + active/exhausted); it needs a composition accessor over `w.buildings` (extraction vs processing, active vs exhausted) — the *one* new plumbing the surface's genuinely-aggregate view requires.
- **Mock-data notes:**
  - **Workforce is uniform 85.47% in the mock** for all corps — the view looks inert until real contention varies; verify against the live sim, not the CSV.
  - **Net-worth ≠ balance.** `corporations.csv` has `starting_capital=0 placeholder` and `since_start=balance dup` — there is no net-worth (assets + cash) figure. Any "who's ahead" ranking would run on raw `balance` — precisely the figure Corporation's Standings owns, banded, reinforcing that Economy should not re-answer it.

## 5. Close / toggle semantics
The nav-rail slot-3 icon toggles the ledger open/closed (`ui.show_economy_panel`). Re-clicking the **currently-active tab** **closes the ledger entirely** (not collapse-to-overview) — `nav_button` takes `&ui.show_economy_panel`. Opening Economy closes whichever other ledger held the column (accordion, `close_all_panels`). The slot is provisional: it is the Workforce slot's door until a Workforce surface exists, which is the crux of the lead question — keep the slot for the aggregate residue, or fold that residue into Corporation and retire the panel.

## Open questions for Ben
- **(Lead) Does Economy survive as a distinct menu at all?** It borrows the Workforce slot, and this pass found it re-answers Balance's Cashflow and Corporation's Standings outright. Options: (A) keep a rail icon hosting only the genuinely-aggregate residue — **Sector** + **Workforce** + a bare player-balance trend plot; or (B) fold that residue in as tabs under the Corporation dashboard and retire the Economy panel. Given how thin the residue is, **(B) is my lean**.
- **If it survives — is this really one view, not three?** Holdings and Markets are cut as duplicates; the Corps balance list goes to Corporation. That leaves **Sector** (new aggregation, the real reason to exist) + **Workforce** (uniform in mock) + an optional player-trend plot. Is that enough surface to justify a rail slot, or is it a single "Aggregate" tab bolted onto Corporation?
- **Lens: none, or Corporation-follows?** Industry paints the nation substrate, not corp holdings, and is keyboard-only off the bar. Default to **none**, or arm **Corporation** so the map echoes the player's footprint when Sector is shown?
- **Net-worth vs balance is moot here.** True net worth needs asset valuation (`BL-527`, corp valuation) — but since any balance-rank duplicates Corporation's Standings, this surface shouldn't carry the ranking at all. Confirm the ranking stays in Corporation and Economy drops it.
