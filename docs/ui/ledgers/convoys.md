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
  edge. Same family as NR-709, which has now bitten four surfaces.
- **A convoy reads `Agricultural Produce x0`.** A zero-quantity convoy in flight, with a haul cost
  paid and a progress bar running. Either a real defect in dispatch or a state nothing explains;
  it is not a display bug, because the display is reporting what it was given. Worth measuring
  before the surface is rebuilt around it.

## Open questions for Ben
- ~~Does it earn a rail slot, and where?~~ **Answered** (Ben, 2026-08-29): *"give convoys a slot,
  put it after market."* Slot **7**, directly after Market, taking the rail to fourteen and
  shifting Corp. Strategy down to 8. That placement keeps the commercial run of the rail together
  — Acquisitions, Market, Convoys — while giving the logistics read its own door and its own
  lens. See `MENU.md` § Scope-widening names.
- **Is `x0` a defect?** See above. If dispatch can legitimately commit an empty convoy, the row
  needs to say why; if it cannot, this is a supply-layer bug the ledger merely revealed.
- **Do Routes and History earn views**, and does History need the per-arrival record that
  `MARKETS.md` § Trades notes is also missing for realised trades? The two gaps are the same
  shape — an aggregate loop that retains no per-event row — and might be worth closing together.
