# Market — design Q&A

> **Working design doc** for the ledger-mockup pass (Power BI). Strawman answers — Ben revises.
> Menu slot: `nav rail slot 6 "Market Ledger"` · Source: `src/ui/market_ledger.cpp` · Mock table(s): `markets.csv`, `stockpiles.csv` · Owning items: `BL-453` (convoys ledger), `BL-037` (sell-order routing)
> Host: shell fold-out column — **380 px at 1280, 384 px at 1920** (`shell_column_width` is `0.20 * disp_x` clamped to [380, 460], so it is effectively fixed across the common range; what resolution changes is the column's HEIGHT, and all of it).

## 1. Top question — the one thing this answers at first glance
**"What is each good worth here, and which way is it moving?"** The default lands on the **Goods**
table: one ROW per traded good, with its price, how that price stands against the body and against
its own base, and a mini 8-quarter graph inline in the row.

**The stacked-sparkline layout it replaces is retired** (Ben, 2026-08-29). Each good took a name
line plus a 44 px chart — about 72 px of column — so the view showed 3.5 goods at 1280x720 and 9 at
1920x1080 against a roster of 45 (measured). Three things were wrong with it and only one was density:

- **No shared scale.** Every sparkline was drawn to its own range, so Iron Ore's decay and Coal's
  flat line looked alike. The one thing a price board is for — comparing goods — was the one thing
  it could not do.
- **No direction.** `now 0.63` says nothing about which way it is going without reading a curve
  that has no axis.
- **Nothing had ever seen past the fourth good.** The scroll hook aimed at the outer window while
  the sparklines sat in an inner child scroller, so the "foot" capture was byte-identical to the
  head and goods 4–45 had never been looked at by any capture, golden or human (NR-719).

## 2. Sub-levels — views & default

Two `nav_button` views, and **Convoys leaves entirely** for a ledger of its own
([convoys.md](convoys.md)) — it was never a market question; a convoy is cargo in transit and
belongs to `SUPPLY.md`.

| View | Answers (one question) | Content |
|---|---|---|
| **Goods** *(default)* | What is each good worth here, and which way is it moving? | A table, one row per traded good: **item glyph · name · price · price vs base · 8-quarter graph**, the graph flattened INTO the row rather than stacked under it. Every row is drawn against a **shared `price / base_price` axis with a 1.0 baseline**, which is what makes goods comparable; direction is carried by the line's colour. **`body_average_price` does not fit and is dropped** — see below |
| **Trades** | What positions do I hold, what else is standing here, and what could I be doing? | **Four headed sections**, each bounded and scrolling inside itself. Three reads, kept visibly distinct (`MARKETS.md` § Trades) — **My trades** (with the `place_sell_order` form folded under it and an `x` per row); **All trades here**, gated on the player owning a building on the body; **Potential trades**, ranked by margin — plus the history half, **Recent trades**, over the exchange record. See § 2b |

**Above the tabs, below the selectors: the nation presence row.** A wrapped row of one chip per
nation operating in the selected market, wrapping to two or three rows if it must. **These are colour chips with initials, not flags**, and for now **placeholder chips at that**
(Ben, 2026-08-29 — flags and glyphs are a later sprint; reserve the real width, do not size the
column to the stand-in). — `nation_colour` is all that exists; there is no
per-nation emblem artwork and inventing one is a generated-identity feature, not a row
(`ICONS.md` § 2b).

**Default on open:** **Goods**.
**Cross-cutting selectors (NOT views, toggle-exempt):** the **Body** combo and the **Market**
combo, above the presence row. Both re-point every view.

**Row height is the open measurement.** Ben, 2026-08-29: *"since we are rendering rows, let's
compare this at 8 rows, 10 rows, and 12 rows."* Build all three and look at them at **1920x1080**,
which is the screen being reviewed — a density judgement taken at 720p is taken against half the
content height (NR-719's sibling finding).

**`body_average_price` does not survive, and the expectation was wrong.** The column was
nominated first-to-drop *if* the row would not fit, on the reasoning that the pressure at 384 px
would be on the graph's width rather than the column count. Measured at 1920x1080 over the real
45-good roster (`scripts/verify/goods_table.lua` prints both layouts on every run):

| Layout | Name column | Names that fit |
|---|---|---|
| **with** body average | 42 px | **13 of 45** |
| **without** body average | 98 px | **36 of 45** |

So the column count was the pressure after all — the graph is only ~3.4 em, and dropping one
text column more than doubles the name width. At 42 px `Iron Ore` and `Iron-Nickel Ore` both
elide to `Iron ...`, which defeats the one thing a price board is for. It is kept as a one-line
dial (`ui_state::market_goods_show_body`) rather than deleted, so the call is cheap to revisit.

## 2b. Trades — four sections, and why they are four

**The tab is called Trades** — Ben, 2026-08-29, naming it: *"we should call it Trades"*.
Not Sell Orders and not Orders. The word carries the widening: a sell order is one direction and
one actor; a trade is a position either way round, held by anyone in the market, and — since the
clearing tick retains a per-exchange record — one that has already happened.

| Section | Read | Columns |
|---|---|---|
| **My trades** | The acting corp's standing orders on the selected market's **body** (`sell_orders` *and* `buy_orders`) | Good · Qty · Limit · `x` |
| **All trades here** | Every standing order on that body, whoever holds it | Good · Holder · Qty · Limit |
| **Potential trades** | A derivation: buy here, sell there, less haulage. Per unit, ranked by margin | Good · To · Margin |
| **Recent trades** | The exchange record filtered to this market, newest first | qtr · Good · With · **Revenue** |

**Direction is the limit's operator, not a column.** `>= 0.6` is a sell's floor and `<= 0.6` a
buy's ceiling — the shorthand the pre-rename row already used, and at ~380 px a fourth text
column costs more than it says.

**The gate on the second read is: the player owns a building on that body** (Ben, 2026-08-29,
over "an order here", "either", and "any discovered market"). Shut, the section says so in words —
"You hold no building on *X*" — because a shut gate and an empty book are different answers.

**Potential trades price the real leg.** The haulage term is `price_convoy_leg`'s own cost for a
one-unit leg, so the figure the player acts on is the figure the dispatcher would bill; there is
no second haulage model to disagree with the one that charges. A lane that will not price
produces **no row** — an unreachable market is not a trade at a worse margin, it is not a trade.
The read is cached against the econ tick and the player's asset count, because pricing runs an
A\* on a cache miss and doing that per frame over every market is the shape of the stall that
narrowed `invalidate_logistics_caches`.

**The history column is REVENUE and the limit is structural.** `stockpile_component` is
`quantities[]` and nothing else, so no cost basis exists anywhere in the model and a margin is
not derivable from a sale. An absent counterparty is **the market**, not "unknown"; the cell
names the side that varies (`to Corix`, `from Marur`) and the hover labels both.

**No item glyph on these tables, unlike the Goods board — measured.** With the glyph reserved,
five columns left the Good and Holder names ~77 px and ~64 px, drawing `Petrol...` and `Far...`:
two goods or two firms sharing a prefix become one string, which is the defect the Goods table's
name column was rebuilt to fix. The glyph is a deliberate placeholder (`ICONS.md` § 2b), and
reserving width for artwork that does not exist yet at the cost of names that do is the wrong
trade at this width. The good keeps its identity **colour** on its name.

**Each long section is bounded and scrolls inside itself.** Measured on the shipped fixture: the
book runs to 24 rows and the exchange read to 120, so laid out end to end the first fills the
column and the other three reads are below the fold on open — and a tab whose headline question
is *what could I be doing?* cannot open on a list of rival orders with the answer three screens
down. Caps are 5 / 5 / 7 / 6 rows against a ~595 px content height, which totals a little over,
so the outer scroller still has somewhere to go.

**Ranking is permitted here**, and this is the one surface where that has been ruled on
explicitly — `CONCEPT.md` § Player identity and Ben's qualification of it the same day.

## 3. Lens on open
Arm **`market`** — the per-body price wash keyed to the selected resource — **fixed** across both views. Goods and Trades are both single-market reads that the price wash reinforces, so there is nothing for the lens to follow. The Convoys view that would have wanted `supply_routes` has left for its own ledger, which is where that pairing now lives ([convoys.md](convoys.md) § 3) — one of the quieter arguments for the split, since a tab strip cannot arm two lenses and the old third tab always got the wrong one.

## 4. Data sources
Most of it is in `w.markets` (`market_component`: `supply[]`, `demand[]`, `price[]`, `base_price[]`, `body`, `centre_tile`) and `market_plot_history` for the 8-quarter series (`record_histories` samples once per econ tick and a tick is a calendar quarter, so a "six-month" window would be two points; Ben chose 8 of the 64 retained). `price vs base` is `price[r] / base_price[r]`, already to hand. **`body_average_price` is the one new derivation** — mean `price[r]` across the markets on the selected body, which is a walk the surface can do itself.

**Trades reads four stores and derives one column.** `w.sell_orders` and `w.buy_orders` are world state and carry both the player's positions and everyone else's, so reads 1 and 2 are filters over them. Read 3 derives from the two markets' `price[]` plus `price_convoy_leg` — prices, reach and haulage. The history half reads `w.exchanges`, and **through `oldest_first(i)`**: the raw ring's vector order stops being chronological the moment it wraps at 8192 rows, so a reader walking `entries` directly shows history shuffled at the wrap point. What the record does **not** carry is a realised margin — clearing resolves a price and moves quantity without any cost basis to net it against — so the column is **revenue** (`MARKETS.md` § The exchange record), and a profit must not be invented to fill it.

The nation presence row reads `nation_colour(entity_id)` and the set of nations with activity in the market. No convoy data — that left with the view.

`markets.csv` leads with `market_id, market_label, body_id, …`, so each market row is distinct and a Power BI join on body does not fan out. Read the fixture for its row and market counts rather than pasting a figure (see `_critic_notes.md` for why a pasted fixture figure is a liability). `stockpiles.csv` is near-empty by design — the economy drains pools to market each tick — so it is not a market-ledger input, just noted so it isn't mistaken for one.

## 5. Close / toggle semantics
Nav-rail slot-6 icon toggles the ledger open/closed. Re-clicking the **currently-active sub-view tab** (Goods / Trades) **closes the ledger** — `nav_button` takes `&open`; it does not collapse to an overview. Switching *between* tabs just changes view. The Body / Market selectors are cross-cutting and **never** trigger the close (toggle-exempt); Remove and the add form are in-view presses. Opening the ledger closes whichever other ledger held the column (accordion, `close_all_panels`).

## Open questions for Ben
- **Default market when none picked.** The ledger falls back to `home_body`'s **first market by ascending entity id**. On a multi-market body, is "first market" the right default landing, or should Prices open on the **highest-turnover** market (more informative first glance)?
- **Lens-follows-view vs fixed.** I have Markets flip the lens to `scarcity` and the other views hold `market`. Confirm you want the lens to *follow the sub-view* here rather than a single fixed lens for the whole ledger.
- **Sparklines or board as the Prices default?** Prices leads on movement (one sparkline per good); the proposed Board leads on per-good **Net** (supply − demand) and the proposed Markets on **Turnover**. Is movement the right first glance, or should the scarcity board come first with the sparklines behind it?
- **Body selector granularity.** Should the Body combo list only bodies with markets (its behaviour), or show all discovered bodies and grey the market-less ones for map-consistency?
