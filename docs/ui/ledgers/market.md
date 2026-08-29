# Market — design Q&A

> **Working design doc** for the ledger-mockup pass (Power BI). Strawman answers — Ben revises.
> Menu slot: `nav rail slot 6 "Market Ledger"` · Source: `src/ui/market_ledger.cpp` · Mock table(s): `markets.csv`, `stockpiles.csv` · Owning items: `BL-453` (convoys ledger), `BL-037` (sell-order routing)
> Host: shell fold-out column, ~380px @1720 (derived — `shell_column_width(disp.x)`, 380–460 by resolution).

## 1. Top question — the one thing this answers at first glance
**"For the market I'm looking at, how is each good priced, and which way is it moving?"** The default lands on a single market's per-good board: one price sparkline per traded good (`base_price[r] > 0`), stacked in a scroll region under **Price over time**, each with its identity swatch and a `now X.XX` readout from `market_plot_history`. The secondary questions that earn the other views: **what am I offering here, and at what floor** (the player's standing sell orders), and **what is on its way to me** (in-flight convoys). Proposed beyond those: **where does trade concentrate** across a body's several markets (a turnover table), and a **supply/demand/net board** per good colour-keyed by scarcity. Each is a distinct question, so each is its own view rather than one stacked scroll.

## 2. Sub-levels — views & default

Three `nav_button` views on `s.market_ledger_view`, above which sit the two cross-cutting selectors.

| View | Answers (one question) | Content |
|---|---|---|
| **Prices** *(default)* | How has each good's price moved here? | `SeparatorText("Price over time")`, then per traded good: name in its resource colour, `now` price, a `draw_plot` price sparkline (44 px, blue) from `market_plot_history[market][r].price`, or *(no history yet)* before the first tick |
| **Sell Orders** | What am I offering here, and at what floor? | The player's standing sell orders on the selected body — `resource x qty >= floor` with a **Remove** press — then an add form (resource among the market's `base_price > 0` goods, quantity, floor). Every press issues a `corp_command` (`remove_sell_order` / `place_sell_order`) through `state.pending_order_commands`, never a direct write. Orders are honoured at clearing ahead of the anonymous auto-sell path (`MARKETS.md` § preferred-seller routing) |
| **Convoys** | What is on its way to me? | One row per in-flight convoy: origin → destination (resource), a progress bar whose overlay reads `held`, `N%  (stalled)`, or `N%  M qtr(s)`, and *Haul cost paid*; *Nothing in flight* when empty. A market question, so it lives beside the other two rather than in a ledger of its own |
| **Markets** *(proposed)* | Where does trade concentrate on this body? | One row per market on the selected body (Market / Supply / Demand / **Turnover** = Σ min(supply, demand)), selectable to re-point Prices |
| **Board** *(proposed)* | What's tight here right now? | Resource / Supply / Demand / Price / **Net** per traded good with a positive/negative/neutral colour wash; "Turnover this tick" footer |

**Default on open:** **Prices**.
**Cross-cutting selectors (NOT views, toggle-exempt):** the **Body** combo (bodies that have a market; defaults to `w.home_body`, else the first) and the **Market** combo (the selected body's markets by city name, defaulting to the first by ascending entity id; `pending_focus_market` lets a canvas press land the ledger on a given market). Both re-point every view.

## 3. Lens on open
Proposal: arm **`market`** — the per-body price wash keyed to the currently selected resource — **fixed** for Prices, Sell Orders and Convoys (all single-market reads the price lens reinforces). **Open option:** a **Markets** sub-view swaps the lens to **`scarcity`**, since that view is about *where clearing is tight across markets*, not price level. So: lens fixed `market` by default, following the sub-view only to the extent that Markets flips it to `scarcity`. Consistent with Ben's "opening a menu usually arms a lens". The ledger arms no lens on open; this is the proposal.

## 4. Data sources
Everything the ledger draws is in `w.markets` (`market_component`: `supply[]`, `demand[]`, `price[]`, `base_price[]`, `body`, `centre_tile`), `market_plot_history` for the sparklines, `w.sell_orders` for the player's orders, and the convoy list for Convoys — no new world plumbing for the surface itself.

`markets.csv` leads with `market_id, market_label, body_id, …`, so each market row is distinct and a Power BI join on body does not fan out. Read the fixture for its row and market counts rather than pasting a figure (see `_critic_notes.md` for why a pasted fixture figure is a liability). `stockpiles.csv` is near-empty by design — the economy drains pools to market each tick — so it is not a market-ledger input, just noted so it isn't mistaken for one.

## 5. Close / toggle semantics
Nav-rail slot-5 icon toggles the ledger open/closed. Re-clicking the **currently-active sub-view tab** (Prices / Sell Orders / Convoys) **closes the ledger** — `nav_button` takes `&open`; it does not collapse to an overview. Switching *between* tabs just changes view. The Body / Market selectors are cross-cutting and **never** trigger the close (toggle-exempt); Remove and the add form are in-view presses. Opening the ledger closes whichever other ledger held the column (accordion, `close_all_panels`).

## Open questions for Ben
- **Default market when none picked.** The ledger falls back to `home_body`'s **first market by ascending entity id**. On a multi-market body, is "first market" the right default landing, or should Prices open on the **highest-turnover** market (more informative first glance)?
- **Lens-follows-view vs fixed.** I have Markets flip the lens to `scarcity` and the other views hold `market`. Confirm you want the lens to *follow the sub-view* here rather than a single fixed lens for the whole ledger.
- **Sparklines or board as the Prices default?** Prices leads on movement (one sparkline per good); the proposed Board leads on per-good **Net** (supply − demand) and the proposed Markets on **Turnover**. Is movement the right first glance, or should the scarcity board come first with the sparklines behind it?
- **Body selector granularity.** Should the Body combo list only bodies with markets (its behaviour), or show all discovered bodies and grey the market-less ones for map-consistency?
