# Project Io — Markets

The market model as **shipped** (written 2026-07-31 from `src/world/market_clearing.cpp`, the
market/order components in `src/world/components.hpp`, and the seeding in
`src/world/hard_coded_world.cpp`). This doc documents fact; the market rework family that would
change it is listed at the end as owed follow-ons only. Production's side of the exchange is
`docs/economy/PRODUCTION.md` § Stockpile and output flow; which resources trade at all is
`docs/economy/RESOURCES.md` § What actually trades.

---

## Market centres and seeding

A market is a `market_component`: a body, an optional `centre_tile` anchor, and four
resource-indexed arrays — `supply`, `demand`, `price`, `base_price`. Markets exist **only on the
home body (Kepler)** today; Cinder, Selene, and Pallas carry none.

Seeding is population-anchored but **resource-carved** (BL-096, market resource generation):
markets anchor to population-centre tiles, and how finely a nation's territory fractures into
markets follows its tradeable-resource concentration — a resource-rich nation admits smaller
centres (more markets), a barren one folds into its neighbour's. One pass at world-gen,
deterministic, with a seeded jitter on the borderline. If no centre qualifies, one unanchored
fallback market is seeded.

**Catchment routing:** a tile clears against the market whose `centre_tile` is nearest
(`market_for_tile`); a corp's body-aggregate clearing routes via its lowest-id building's tile
(`market_for_corp_on_body`). Markets are **fixed at world-gen** — nothing creates or destroys
one at runtime.

## What trades

Only resources with a non-zero `base_price` on the market participate; everything else is
skipped by every clearing path and `resolve_price` leaves its price untouched (at 0). The
tradeable set — seven authored prices plus the distance-priced endemic goods (BL-191) — and the
minable-but-unsellable asymmetry of the BL-040 raws are catalogued in
`docs/economy/RESOURCES.md` § What actually trades.

## The clearing tick

`clear_markets` runs once per economy tick, after production (`run_economy_step`). In order:

1. **Reset** — every market's `supply` and `demand` arrays zero. There is no stored inventory;
   both are per-tick flows.
2. **Substrate injection** (`inject_substrate_demand`, BL-078) — the nation background economy:
   a price-elastic per-capita basket demand and an abstract capacity-capped supply that clears
   demand only to `clearing_fraction`, leaving the live margin (the saturation cushion / the
   opportunity gap the player fills). Tunables in `scripts/economy.lua` § `substrate`.
3. **Auto-surplus** — each `(corp, body)` pool lists everything above its **processor
   reservation** (the inputs its own processors need for a full run next tick) for sale. A
   resource under a standing sell order is exempted — the manual order governs.
4. **Standing sell orders** — read from `world::sell_orders` (BL-293: the book is world state,
   placed by the player and by rival corps through the same `place_sell_order` verb), quantity
   capped by the pool, entered into both market supply and the explicit sell book with their
   `floor_price`. Multiple orders against one `(corp, body, resource)` share a **running
   remainder**: total listed quantity never exceeds the pool, each order's matched/auto-cleared
   quantity is tracked per order, and pool debits clamp at zero.
5. **Auto-demand** — processor input shortfalls and construction material draws
   (`report.purchases`) enter market demand.
6. **Standing buy orders** — read from `world::buy_orders`, entered into demand and the
   explicit buy book (`max_price`, optional `preferred_seller`).
7. **Reference prices** — computed once from the accumulated supply/demand (below), so every
   flow this tick uses the same price.
8. **Auto clearing** — auto-surplus sells, and auto-demand buys, at the reference price. The
   market is a **perfect counterparty**: these always clear in full.
9. **Order-book matching** (BL-037, preferential purchasing — shipped) — explicit sells vs
   explicit buys by price-time priority: cheapest ask first, highest bid first, corp id as the
   deterministic tiebreak. A buyer's `preferred_seller` is served first, tolerated up to
   **1.10×** the cheapest compatible ask. Trades clear at the **seller's ask**.
10. **Buyer of last resort** — unmatched player sell quantity auto-clears at
    `max(reference_price, floor_price)`, so listed supply always clears (prototype invariant).
11. **Price update** — where explicit trades occurred, the price eases toward their VWAP;
    otherwise it takes the reference price.

Cash flows accrue per corp and are applied to balances by `apply_budget`
(`src/world/budget_system.cpp`).

## Where the order book lives

`world::sell_orders` and `world::buy_orders` — **world state**, not UI state, since BL-293
(2026-08-07). Ben's ruling: *"Order book needs to be a background process, the AI must be able
to trade as a player does."*

It used to sit on `ui_state` and arrive at `clear_markets` as a caller-supplied argument, which
cost three things at once. Clearing was something the **UI drove** rather than something the
simulation does, so a headless tick sold nothing standing. A `corp_command` mutates `world&`, so
there was **nothing for a trade verb to mutate** and no text-driven player could trade. And
nothing on the serialisation seam referenced an order, so a player's standing orders would not
have **survived a save**. One misplacement, three symptoms.

What follows from the move:

- **One implementation of what a press means.** `place_sell_order` / `remove_sell_order` are
  `corp_verb`s (`corp_command.hpp`). The Market Ledger's buttons queue commands that
  `app::render` applies through `apply_corp_command` — the same call the rival-corp scorer
  makes. A player and an AI cannot diverge, because there is nothing to diverge.
- **Rival corps trade.** The scored-utility layer reaches the verb like any other
  (`io-standing-rules.md` § rival-corp exception, widened for this). Its first cut is
  deliberately conservative — surplus past a hold threshold, floored at the rarity price — and
  lives in `corp_ai_params`.
- **Orders carry a stable `id`.** Removal names the id, never an index, so withdrawing one order
  cannot renumber another.
- **Insertion order is semantic.** Matching is price-**time** priority, so the book's sequence is
  state: it is never re-sorted, it is serialised in place, and `world::state_hash` folds it as
  stored rather than sorted. `order_book_harness` asserts that two books holding the same orders
  in a different sequence hash differently — the tripwire for a determinism leak.
- **Persistence.** `order_book.{hpp,cpp}`, the second flat-binary stream in `world/*` after
  `history_log`: magic `IOOB` + version, and a bad stream is refused rather than reinterpreted.

## Price resolution

`resolve_price`, per (market, resource):

```
target = base_price × √(demand / supply)     — damped elasticity
         base_price                           when neither side has a signal
         base_price × 4.0                     demand with zero supply
price  = prior + 0.5 × (target − prior)       — EMA smoothing
```

Target and result are clamped to the band **[0.25×, 4×] of base**. Prices are therefore
*anchored*: no scarcity can push a good past 4× its authored base, and no glut below a quarter
of it. Untradeable resources (`base_price ≤ 0`) keep their prior price.

## Inter-body linkage

There is no abstract price coupling between bodies — **the convoy is the coupling**
(`docs/economy/SUPPLY.md`). `dispatch_convoys` reads last tick's cleared shortfalls
(demand − supply per market) and hauls from the cheapest reachable corp pool — distances and
haul prices read **tick-pure orbital angles** (`orbital_angle_at_tick`, BL-354), so dispatch is
identical at any frame rate; the smoothly-advancing render angles are display-only. An arriving
convoy credits the destination pool **and** injects its cargo into the destination market's
supply, repricing it at the next clear. Since only Kepler has markets, a marketless body's
processors fall back to the two-threshold pool model (PRODUCTION.md § Processing), and goods
extracted there enter the economy only by convoy to a Kepler market.

## Known limitations (honest list)

- **No stored inventory.** `market_component.supply` is a derived per-tick flow, not a stock —
  the market absorbs any quantity and conjures any shortfall at the resolved price. Revisiting
  this is BL-130 (real market inventory).
- **Anchored prices.** The [0.25×, 4×] band caps every scarcity signal at 4× base. This is the
  band the BL-078 (product-market inertness) diagnosis found products pegged against — resolved
  in *shape* by the elastic substrate (prices now discover within the band), but the ceiling
  itself is retained deliberately, to be retuned once real ranges are visible.
- **Most of the enum cannot trade.** Base price 0 across the non-subset resources makes the
  BL-040 raws minable but unsellable (RESOURCES.md § What actually trades) — the standing
  market-inertness residue.
- **The buy-order book is engine-only.** `clear_markets` implements it, and harnesses exercise
  it, but nothing writes `world::buy_orders` — no press and no `corp_verb` submits a buy order
  yet, so preferred-seller routing is dormant in play. BL-293 gave the buy side world state and
  a save format alongside the sell side, but not a verb: the book is one-sided, so the AI can
  release stock and cannot bid for it. *Decided 2026-07-31:* the wiring waits for
  BL-160 (auto-exchange policy, v0.2.0), whose buy band is the intended player buy surface and
  whose `derive_exchange_orders` is the first live emitter — no interim manual buy tab. See the
  dated note in that item's design for the open preferred-seller call.
- **The population agri demand stub never reaches clearing.** `run_economy_step`'s
  population-centre pass writes `agricultural_produce` demand directly into markets, but
  `clear_markets` zero-resets demand before accumulating — the write is erased the same tick.
  The real population pull is the substrate basket. Flagged as a code wrinkle, not fixed here.
- **Markets are static.** Seeded once at world-gen; no runtime creation, destruction, or
  migration.

## Owed follow-ons

The market rework family — all **open**, so named here without design import:

- **BL-130 (real market inventory)** — design-owed
- **BL-131 (player market destruction)** — design-owed
- **BL-132 (market cogeneration)** — designed
- **BL-160 (auto-exchange policy)** — designed
- **BL-161 (counterparty allow/deny)** — designed
