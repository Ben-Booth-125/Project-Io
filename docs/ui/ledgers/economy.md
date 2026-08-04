# Economy (aggregate) — design Q&A

> **Working design doc** for the ledger-mockup pass (Power BI). Strawman answers — Ben revises.
> Menu slot: `NOT IN RAIL (currently orphaned — see §5 / open questions)` · Source: `src/ui/economy_panel.cpp` · Mock table(s): `player_timeseries.csv, cashflow.csv, workforce.csv, buildings.csv, corporations.csv` · Related: `BL-117, BL-122, BL-123, BL-078/079`
> Host: shell fold-out column (BL-122), ~380px @1720 (derived — `shell_column_width(disp.x)`, 380–460 by resolution).

> **Lead decision — does Economy survive as a distinct surface?** Before the four-tab shape below is taken as the target, note the overlap this pass surfaced: as currently drafted, Economy **substantially re-implements Balance + Corporation**. Its **Cashflow** view duplicates the Balance ledger's Cashflow tab *exactly* (same `cashflow.csv`, same six lines, same player corp `30310`); its **Standing** corp-rank duplicates Corporation's **Standings**; and §2 already drops Holdings→Corporation and Markets→Market. Strip those and what is genuinely *aggregate-only* — neither a single corp's ledger nor a single market's board — is thin: the **Sector composition bar** and **Workforce**. That thinness is exactly why open-Q (B) *"fold Economy into Corporation"* is live. **Read this doc as the "does Economy earn its own rail slot" decision first, and the four-tab layout as the strawman that mostly loses to Balance + Corporation second.**

## 1. Top question — the one thing this answers at first glance
**"Is my whole enterprise winning or losing, and where is the money going?"** At a glance: my net-worth/balance rank against the rival corps, and my income-vs-expenditure trend line (the `player_timeseries.csv` curve that today bleeds from +1963 to −262 over 16 ticks — the single most alarming fact in the mock, and the thing an aggregate view exists to catch early). Secondary questions that *would* justify sub-views: *how is my cash split between earning and spending* (cashflow composition — wages/maintenance/interest), *how balanced is my sector footprint* (extraction vs processing buildings), and *is labour throttling me anywhere* (workforce staffing across holdings).

**But note the ownership overlap up front:** "how is my cash split" is already answered by the **Balance** ledger's Cashflow tab, and "how do I rank against rival corps" is already answered by **Corporation**'s Standings. Neither is aggregate-only. Once those are handed back to their owners, the only cross-cutting roll-ups left are the **sector footprint** and **workforce contention** — genuinely all-corps/all-bodies, but a thin pair to hang a whole surface on. Per-corp drill-down is the Corporation ledger; per-market is the Market ledger.

## 2. Sub-levels — views & default

| View | Answers (one question) | Content | Aggregate-only? |
|---|---|---|---|
| **Standing** | "Am I winning?" | Player balance trend plot (`draw_balance_trend`, already built) + a corp balance *ranking*. | **No — collides with Corporation.** The rank ladder duplicates Corporation's **Standings** (same per-corp `balance`). Corporation owns "corp balance rank"; Economy must not re-answer it. The bare player *trend* plot is the only non-duplicative fragment. |
| **Cashflow** | "Where does the money go?" | Income vs expenditure split from `cashflow.csv` — maintenance / wages / interest / net. | **No — collides with Balance.** This is the Balance ledger's Cashflow tab *exactly*: same `cashflow.csv`, same six lines, same player corp `30310`. Balance owns "my cashflow"; Economy must not re-answer it. **Cut.** |
| **Sector** | "Is my footprint balanced?" | Extraction-vs-processing tally across all my holdings, from `buildings.csv` (type/output/active/exhausted). A composition bar, not a building list. New. | **Yes.** A cross-corp footprint composition is neither a single corp's ledger nor a single market's board. This is the surface's real reason to exist. |
| **Workforce** | "Is labour throttling me?" | `draw_workforce` — staffing contention across (corp × body). Already built. | **Yes.** Contention across the whole holding set is genuinely aggregate. Thin in the mock (see §4), but aggregate-only. |

**Aggregate-only taxonomy note.** A view earns a place on this surface only if it answers a question that is *neither* a single corp's ledger *nor* a single market's board. By that test **four collisions** fall out, not two:
- **Holdings → Corporation ledger** (`draw_pools`).
- **Markets → Market ledger** (`draw_markets`).
- **Cashflow → Balance ledger** (duplicates its Cashflow tab exactly). **New — was missed in the prior draft.**
- **Standing's corp-rank → Corporation's Standings** (duplicates the balance ranking). **New — was missed in the prior draft.**

After removing all four, the genuinely aggregate-only residue is **Sector** + **Workforce** — and a bare player-balance *trend* sparkline that has no other home. That residue is thin enough that folding it into Corporation (open-Q B) is the live alternative to keeping a fourth rail slot.

**Default view on open (if the surface survives):** **Sector** — the one load-bearing aggregate-only question. (The prior draft defaulted to Standing; Standing is now mostly a Corporation duplicate.)
**NOT views (cross-cutting selectors):** none needed — this surface is all-corps/all-bodies aggregate by definition. A "me only vs all corps" toggle would be a filter, not a tab.

## 3. Lens on open
**None on open — the prior "Industry, fixed" answer was wrong.** `overlay_mode::industry` does **not** paint the player's extraction/processing sector balance. It is the **per-tile nation-substrate throughput field** (occupation × terrain richness — the AI-owned *broad* economy; see LENSES.md § Industry lens / `ui_state.hpp`). Arming it for the **Sector** view would light up the *nation substrate*, not the corp building mix — the opposite of what the view is asking. So the "on-canvas twin of sector-balance" rationale does not hold.

Two honest options:
- **Arm none.** Sector-balance is a composition of *player holdings*, and no single lens paints exactly that. Leaving the lens untouched is the truthful default.
- **Arm Corporation.** If a lens should follow the surface, **Corporation** is the one that actually paints corp holdings on the canvas — the nearest map echo of "where my sector footprint sits". This is my recommendation over "none" if Ben wants a lens armed at all.

**Do not arm Industry.** Beyond painting the wrong thing, Industry is **keyboard-cycle-only off the visible minimap lens bar (BL-093)** — arming it from a ledger would surface a lens the player can't reach from the strip, which is inconsistent chrome. (Note it belongs to the AI's broad-economy substrate read, not the player's holdings, so it would never be the right "my sector" echo regardless of bar placement.)

## 4. Data — live vs plumbing gaps
- **Live today:** balance trend (`player_plot_history`, `draw_balance_trend`), corp balances (`w.corporations[].balance`), workforce contention (`economy_report.workforce_contention`). These are real world state already rendered.
- **Mock-backed, needs wiring:** cashflow split (`cashflow.csv` has income/expenditure/maintenance/wages/interest/net per corp) — but this is the Balance ledger's data, not Economy's to plumb (see §2). Sector tally derivable from `buildings.csv` / `w.buildings` (type + active/exhausted) but no aggregation helper exists yet.
- **Gaps to flag:**
  - **Sector aggregation helper doesn't exist** — a composition tally over `w.buildings` (extraction vs processing, active vs exhausted) needs a new accessor. This is the *one* new plumbing the surface's genuinely-aggregate view requires.
  - **Workforce is uniform 85.47% in the mock** for all corps — the view will look inert until real contention varies; verify against live sim, not the CSV.
  - **Net-worth ≠ balance.** `corporations.csv` has `starting_capital=0 placeholder` and `since_start=balance dup` — there is no real net-worth (assets + cash) figure yet. Any "who's ahead" ranking would have to run on raw `balance` — which is precisely the figure Corporation's Standings already owns, reinforcing that Economy should not re-answer it.

## 5. Close / toggle semantics
Once (if) wired to a rail slot: the nav-rail Economy icon toggles the ledger open/closed. Re-clicking the **currently-active tab** **closes the ledger entirely** (not collapse-to-overview). Opening Economy while the Selection owns the column evicts the Selection (mutual exclusion, BL-122); Selection reappears on close. **Wrinkle:** this panel has **no nav-rail slot today** — it's reachable only via the verify harness / app default, so the toggle rule currently has nothing to fire it. That is not just an unfinished detail — combined with the §2 overlap finding, it is the crux of the open question: allocate a rail slot, or fold the residue into Corporation and retire the panel.

## Open questions for Ben
- **(Lead) Does Economy survive as a distinct menu at all?** It has no rail slot today, and this pass found it re-answers Balance's Cashflow and Corporation's Standings outright. Options: (A) give it its own rail icon (a 4th ledger) hosting only the genuinely-aggregate residue — **Sector** + **Workforce** + a bare player-balance trend sparkline; or (B) fold that residue in as tabs under the Corporation dashboard and retire the Economy panel. Given how thin the residue is, **(B) is now my lean** — the prior draft leaned (A), but Cashflow and Standing turned out to be duplicates, not distinct value.
- **If it survives — is this really one view, not four?** Cashflow and Standing's rank are cut as duplicates. That leaves **Sector** (new aggregation, the real reason to exist) + **Workforce** (built, but uniform in mock) + an optional player-trend sparkline. Is that enough surface to justify a rail slot, or is it a single "Aggregate" tab bolted onto Corporation?
- **Lens: none, or Corporation-follows?** The prior "Industry fixed" answer was wrong (Industry paints the nation substrate, not corp holdings, and is keyboard-only off the bar). Default to **none**, or arm **Corporation** so the map echoes the player's footprint when Sector is shown?
- **Net-worth vs balance is moot here.** True net worth needs asset valuation that doesn't exist (`starting_capital` is a 0 placeholder) — but since any balance-rank duplicates Corporation's Standings, this surface shouldn't carry the ranking at all. Confirm the ranking stays in Corporation and Economy drops it.
