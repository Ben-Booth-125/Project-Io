# Market — design Q&A

> **Working design doc** for the ledger-mockup pass (Power BI). Strawman answers — Ben revises.
> Menu slot: `nav rail slot 5` · Source: `src/ui/market_ledger.cpp` · Mock table(s): `markets.csv`, `stockpiles.csv` · Related: `BL-063` (trends), `BL-122` (fold-out host), `BL-078/BL-079` (pool→market drain)
> Host: shell fold-out column (BL-122), ~480px @1720.

## 1. Top question — the one thing this answers at first glance
**"For the market I'm looking at, what's each resource's supply/demand/price right now, and where's it tight?"** The default lands on a single market's per-resource board (the `##market_detail` table today: Resource / Supply / Demand / Price / Net, colour-keyed by scarcity). The secondary questions that earn the other views: **where does trade concentrate** across a body's multiple markets (Kepler has 5 — the `##market_dash` turnover table), and **which way is a resource's price/supply/demand moving** over the last ticks (the `draw_plot` trends block, BL-063). Each is a distinct question, so each is its own view rather than the current single stacked scroll.

## 2. Sub-levels — views & default

> **As built the surface has TWO tabs, not three** (`market_ledger.cpp:260-262`): **Prices** and
> **Sell Orders**. The Markets and Trends views below were designed and not built; Sell Orders was
> built and never designed here. Corrected 2026-08-04.

| View | Status | Answers (one question) | Content |
|---|---|---|---|
| **Prices** *(default)* | **built** | What are supply/demand/price/net per resource here, and what's tight? | The `##market_detail` board for the selected market: one row per traded resource (`base_price[r] > 0`), Supply/Demand/Price/**Net** with the positive/negative/neutral colour wash; "Turnover this tick" footer. |
| **Sell Orders** | **built** | What am I offering here, and at what floor? | The player's standing sell orders for the selected market — per-(body, resource) quantity with a floor price, honoured at clearing ahead of the anonymous auto-sell path (BL-037 routing). Undocumented here until now; it arrived from the Construction ledger's own open question about where sell orders belong. |
| **Markets** | *designed, not built* | Where does trade concentrate on this body? | The `##market_dash` list — one row per market on the selected body (Market / Supply / Demand / **Turnover** = Σ min(supply,demand)), selectable to re-point Prices. |
| **Trends** | *designed, not built* | Which way is a chosen resource moving here? | The BL-063 `draw_plot` trio (price / supply / demand sparklines) for the resource picked in the trend combo, read from `market_plot_history`. |

**Default on open:** **Prices** (single-market per-resource board — the first-glance question).
**Cross-cutting selectors (NOT views, toggle-exempt):** the **Body** combo, the **Market** row-selection (from Markets, or falls back to `home_body`'s first market), and the **Resource** combo inside Trends. Selecting a market on the Markets view re-points both Prices and Trends.

## 3. Lens on open
Arms **`market`** — the per-body price wash keyed to the currently selected resource — **fixed** for the Prices and Trends views (both are single-market, single-resource-focused, so the price lens reinforces them). **Open option:** the **Markets** sub-view swaps the lens to **`scarcity`**, since that view is about *where clearing is tight across markets*, not price level. So: lens is fixed `market` by default, but **follows the sub-view** to the extent that Markets flips it to `scarcity`. Consistent with Ben's "opening a menu usually arms a lens".

## 4. Data — live vs plumbing gaps
Everything the ledger draws is **already live in `w.markets`** (`market_component`: `supply[]`, `demand[]`, `price[]`, `base_price[]`, `body`, `centre_tile`) and `market_plot_history` for trends — no new world plumbing needed for the surface itself.

**This gap is closed (2026-08-04).** `markets.csv` now leads with `market_id,market_label,body_id,…`,
so each market row is distinct and the join no longer inflates. The fixture currently holds four
markets over 68 rows; do not paste that count anywhere — read the file (see `_critic_notes.md` for
why a pasted fixture figure is a liability). *The original text: the exporter emitted one row per
`market_component` with no `market_id`/region column, so all Kepler markets shared `body_id` and a
Power BI join on it fanned out.* `stockpiles.csv` is near-empty by design (the economy drains pools to market each tick, BL-078/079) — not a market-ledger input, just noted so it isn't mistaken for one.

## 5. Close / toggle semantics
Nav-rail slot-5 icon toggles the ledger open/closed. Re-clicking the **currently-active sub-view tab** (Prices / Markets / Trends) **closes the ledger** — it does not collapse to an overview. Switching *between* tabs just changes view. The Body / Market / Resource selectors are cross-cutting and **never** trigger the close (toggle-exempt). Opening the ledger takes the fold-out column from the Selection element (mutually exclusive, per the settled decision); closing it returns the column to the persisted Selection if one exists.

## Open questions for Ben
- **Default market when none picked.** Code falls back to `home_body`'s **first market by ascending entity id**. On a 5-market body, is "first market" the right default landing, or should Prices open on the **highest-turnover** market (more informative first glance)?
- **Lens-follows-view vs fixed.** I have Markets flip the lens to `scarcity` and Prices/Trends hold `market`. Confirm you want the lens to *follow the sub-view* here rather than a single fixed lens for the whole ledger.
- **Net vs Turnover as the headline number.** Prices leads on per-resource **Net** (supply−demand); Markets leads on **Turnover** (cleared volume). Is that the right split, or should Prices also surface a price-change delta (needs the trend history the Trends view already loads)?
- **Body selector granularity.** Should the Body combo list only bodies with markets (current behaviour), or should it show all discovered bodies and disable/grey the market-less ones for map-consistency?
