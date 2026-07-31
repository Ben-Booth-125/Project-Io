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
   resource under a standing player sell order is exempted — the manual order governs.
4. **Player sell orders** — quantity capped by the pool, entered into both market supply and
   the explicit sell book with their `floor_price`.
5. **Auto-demand** — processor input shortfalls and construction material draws
   (`report.purchases`) enter market demand.
6. **Player buy orders** — entered into demand and the explicit buy book (`max_price`,
   optional `preferred_seller`).
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
(demand − supply per market) and hauls from the cheapest reachable corp pool; an arriving
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
  it, but the live app passes only `ui_state.sell_orders` — no UI path submits a buy order yet,
  so preferred-seller routing is dormant in play. *Decided 2026-07-31:* the wiring waits for
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
