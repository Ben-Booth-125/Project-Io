# Market — design Q&A

> **Working design doc** for the ledger-mockup pass (Power BI). Strawman answers — Ben revises.
> Menu slot: `nav rail slot 6 "Market Ledger"` · Source: `src/ui/market_ledger.cpp` · Mock table(s): `markets.csv`, `stockpiles.csv` · Owning items: `BL-453` (convoys ledger), `BL-037` (sell-order routing)
> Host: shell fold-out column — **380 px at 1280, 384 px at 1920** (`shell_column_width` is `0.20 * disp_x` clamped to [380, 460], so it is effectively fixed across the common range; what resolution changes is the column's HEIGHT, and all of it).

## 1. Top question — the one thing this answers at first glance
**"What is each good worth here, and which way is it moving?"** The default lands on the **Goods**
table: one ROW per traded good, with its price, how that price stands against the body and against
its own base, and a mini six-month graph inline in the row.

**The stacked-sparkline layout it replaces is retired** (Ben, 2026-08-29). Each good took a name
line plus a 44 px chart — about 72 px of column — so the view showed 3.5 goods at 1280x720 and 9 at
1920x1080 against a roster of ~42. Three things were wrong with it and only one was density:

- **No shared scale.** Every sparkline was drawn to its own range, so Iron Ore's decay and Coal's
  flat line looked alike. The one thing a price board is for — comparing goods — was the one thing
  it could not do.
- **No direction.** `now 0.63` says nothing about which way it is going without reading a curve
  that has no axis.
- **Nothing had ever seen past the fourth good.** The scroll hook aimed at the outer window while
  the sparklines sat in an inner child scroller, so the "foot" capture was byte-identical to the
  head and goods 4–42 had never been looked at by any capture, golden or human (NR-719).

## 2. Sub-levels — views & default

Two `nav_button` views, and **Convoys leaves entirely** for a ledger of its own
([convoys.md](convoys.md)) — it was never a market question; a convoy is cargo in transit and
belongs to `SUPPLY.md`.

| View | Answers (one question) | Content |
|---|---|---|
| **Goods** *(default)* | What is each good worth here, and which way is it moving? | A table, one row per traded good: **item glyph · name · price · body average price · price vs base · six-month graph**. The graph is flattened INTO the row rather than stacked under it. `body_average_price` is the first column to drop if the row will not fit — the pressure at 384 px is on the graph's width, not the column count, so dropping it buys little and it should be tried before it is cut |
| **Trades** | What positions do I hold, what else is standing here, and what could I be doing? | Three reads, kept visibly distinct (`MARKETS.md` § Trades): **my standing trades**; **the market's standing trades** in markets the player operates in; and **potential trades** with their margins — a derivation, not a record |

**Above the tabs, below the selectors: the nation presence row.** A wrapped row of one chip per
nation operating in the selected market, wrapping to two or three rows if it must. **These are
colour chips with initials, not flags** — `nation_colour` is all that exists; there is no
per-nation emblem artwork and inventing one is a generated-identity feature, not a row
(`ICONS.md` § 2b).

**Default on open:** **Goods**.
**Cross-cutting selectors (NOT views, toggle-exempt):** the **Body** combo and the **Market**
combo, above the presence row. Both re-point every view.

**Row height is the open measurement.** Ben, 2026-08-29: *"since we are rendering rows, let's
compare this at 8 rows, 10 rows, and 12 rows."* Build all three and look at them at **1920x1080**,
which is the screen being reviewed — a density judgement taken at 720p is taken against half the
content height (NR-719's sibling finding).

## 3. Lens on open
Arm **`market`** — the per-body price wash keyed to the selected resource — **fixed** across both views. Goods and Trades are both single-market reads that the price wash reinforces, so there is nothing for the lens to follow. The Convoys view that would have wanted `supply_routes` has left for its own ledger, which is where that pairing now lives ([convoys.md](convoys.md) § 3) — one of the quieter arguments for the split, since a tab strip cannot arm two lenses and the old third tab always got the wrong one.

## 4. Data sources
Most of it is in `w.markets` (`market_component`: `supply[]`, `demand[]`, `price[]`, `base_price[]`, `body`, `centre_tile`) and `market_plot_history` for the six-month series. `price vs base` is `price[r] / base_price[r]`, already to hand. **`body_average_price` is the one new derivation** — mean `price[r]` across the markets on the selected body, which is a walk the surface can do itself.

**Trades needs more than the surface can derive, and the doc says which half.** `w.sell_orders` and `w.buy_orders` are world state and carry both the player's positions and everyone else's, so reads 1 and 2 exist. Potential trades (read 3) derive from prices, reach and haulage. What does **not** exist is a REALISED trade: clearing is an aggregate that resolves a price and moves quantity without pairing a buyer to a seller, so no per-exchange profit is recorded anywhere. See `MARKETS.md` § Trades — positions and potentials are answerable now, history is not, and a realised margin must not be invented to fill the column.

The nation presence row reads `nation_colour(entity_id)` and the set of nations with activity in the market. No convoy data — that left with the view.

`markets.csv` leads with `market_id, market_label, body_id, …`, so each market row is distinct and a Power BI join on body does not fan out. Read the fixture for its row and market counts rather than pasting a figure (see `_critic_notes.md` for why a pasted fixture figure is a liability). `stockpiles.csv` is near-empty by design — the economy drains pools to market each tick — so it is not a market-ledger input, just noted so it isn't mistaken for one.

## 5. Close / toggle semantics
Nav-rail slot-6 icon toggles the ledger open/closed. Re-clicking the **currently-active sub-view tab** (Goods / Trades) **closes the ledger** — `nav_button` takes `&open`; it does not collapse to an overview. Switching *between* tabs just changes view. The Body / Market selectors are cross-cutting and **never** trigger the close (toggle-exempt); Remove and the add form are in-view presses. Opening the ledger closes whichever other ledger held the column (accordion, `close_all_panels`).

## Open questions for Ben
- **Default market when none picked.** The ledger falls back to `home_body`'s **first market by ascending entity id**. On a multi-market body, is "first market" the right default landing, or should Prices open on the **highest-turnover** market (more informative first glance)?
- **Lens-follows-view vs fixed.** I have Markets flip the lens to `scarcity` and the other views hold `market`. Confirm you want the lens to *follow the sub-view* here rather than a single fixed lens for the whole ledger.
- **Sparklines or board as the Prices default?** Prices leads on movement (one sparkline per good); the proposed Board leads on per-good **Net** (supply − demand) and the proposed Markets on **Turnover**. Is movement the right first glance, or should the scarcity board come first with the sparklines behind it?
- **Body selector granularity.** Should the Body combo list only bodies with markets (its behaviour), or show all discovered bodies and grey the market-less ones for map-consistency?
