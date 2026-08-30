# Convoys — design Q&A

> **Working design doc** for the ledger pass. Ben, 2026-08-29: *"I think we can make convoys into
> a ledger of its own."* · Source: currently `src/ui/market_ledger.cpp` (the Convoys view) ·
> Menu slot: `rail slot 7 "Convoys"`, directly after Market (Ben, 2026-08-29) ·
> Host: shell fold-out column, ~380 px at 1280 and 384 px at 1920 (`shell_column_width`).
> Authority for the mechanism: [`SUPPLY.md`](../../economy/SUPPLY.md). This doc is the surface.

## 0. Why it leaves the Market ledger

It was never a market question. `MARKETS.md` owns clearing, price resolution and the order book;
a convoy is **cargo in transit** and belongs to `SUPPLY.md` — *"Logistics is the road, Supply is
the traffic."* Hosting it as the Market ledger's third tab put a logistics read behind a market
door, and the tab strip was the only thing relating them.

The flattening is what forced the question. Ben's Goods table and Trades rework each want the
column's full height; a third tab answering a different system was the piece with somewhere else
to be.

## 1. Top question
**"What is on its way, and what is it costing me?"** One row per convoy in flight: origin →
destination, the resource and quantity, a progress bar with its ETA in **qtr**, and the haul cost
already paid. Held convoys and stalled convoys say so on the bar rather than in a separate list.

## 2. Sub-levels — views & default
**One flat view.** There is no second question here yet. Candidates if it earns one later:
*Routes* (the lanes the player runs, aggregated, with cost per unit delivered) and *History*
(what arrived, when, and what haulage ate) — both wanted, neither backed by a store today.

**Cross-cutting selector (NOT a view, toggle-exempt):** none at first. The Market ledger's Body
and Market combos do **not** come with it: a convoy is a route between two places and is not
scoped by the market you are looking at, which is part of why the tab sat oddly there.

## 3. Lens on open
Arm **`supply_routes`** — the lane overlay is the literal map twin of this list, and the ledger
and the canvas then answer the same question in two registers. This is the pairing the Market
ledger could never offer from a tab, since opening it armed the price wash instead.

## 4. Data sources
The convoy list (`SUPPLY.md`: cargo, dispatch, cost, arrival), `price_convoy_leg` /
`commit_convoy` for the cost figures, and `LOGISTICS.md` traversal cost for the route. All world
state; no new plumbing for the surface itself.

## 5. Close / toggle semantics
Standard rail-slot ledger: the icon toggles it open/closed, and with one view the icon is the
whole toggle. It draws `icons::convoy` — the marker the canvas already uses for cargo in transit,
so slot and canvas share a silhouette (`ICONS.md`). Opening it closes whichever other ledger held
the column.

## Known defects it inherits, both visible in the 2026-08-29 capture
- **Destinations clip.** `Huhaidar -> Kua Sua…` and `Huhaidar -> Sus Kha…` overrun the column
  edge. Same family as NR-709, which has now bitten four surfaces. The route takes its own line
  and **wraps** — the fold-out column is `LAYOUT.md` container 1, whose policy is wrap, so a
  destination name is readable in full rather than elided.
- **`Agricultural Produce x0` is a FORMATTING defect, not a dispatch one — measured, not
  argued** (`tools/verify/convoy_cargo_census.cpp`). It was worth measuring, and the measurement
  overturned the premise this list was written with.

  Neither dispatch path can commit an empty convoy, and both forbid it by construction: the auto
  path (`dispatch_convoys`) skips a source whose surplus is `<= 0` and a destination whose
  shortfall is `<= 0`, then takes `min(surplus, shortfall)`, strictly positive; the directed verb
  (`corp_verb::dispatch_convoy`) rejects the whole command on `!(quantity > 0)`, finiteness first.
  Over the real generated world the census found **1669 dispatches and zero with an empty hold**.

  What was wrong was the row. `cargo_qty` is a **float**, and the row printed it with `"x%.0f"` —
  which renders every cargo below 0.5 as `x0`. **4.6% of real convoys are below 0.5** (measured
  minimum 0.0097 against a median of 5.62), so a 0.3-unit convoy and an empty one drew the same
  six pixels. The display was not "reporting what it was given"; it was rounding it away. The
  quantity is now formatted in tiers against that measured distribution, with an explicit
  `<0.01` floor, so a convoy carrying something can never read as carrying nothing.

## Open questions for Ben
- ~~Does it earn a rail slot, and where?~~ **Answered** (Ben, 2026-08-29): *"give convoys a slot,
  put it after market."* Slot **7**, directly after Market, taking the rail to fourteen and
  shifting Corp. Strategy down to 8. That placement keeps the commercial run of the rail together
  — Acquisitions, Market, Convoys — while giving the logistics read its own door and its own
  lens. See `MENU.md` § Scope-widening names.
- ~~**Is `x0` a defect?**~~ **Answered by measurement** (see above): it is a formatting defect in
  the row, not a supply-layer bug. Dispatch cannot commit an empty convoy — 1669 dispatches, zero
  empty holds — and `"%.0f"` was rounding the 4.6% of convoys carrying less than half a unit down
  to `x0`. The invariant is now asserted by `convoy_cargo_census`, so a real empty dispatch would
  fail a check rather than reappear as a display puzzle.
- **Do Routes and History earn views**, and does History need the per-arrival record that
  `MARKETS.md` § Trades notes is also missing for realised trades? The two gaps are the same
  shape — an aggregate loop that retains no per-event row — and might be worth closing together.
