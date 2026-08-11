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
(`market_for_corp_on_body`). This section describes the **home body's** own market set, fixed at
world-gen. Off-world, markets are no longer fixed — see § Spontaneous market emergence below.

## Spontaneous market emergence (BL-263, 2026-08-11)

Off-world (any body other than `world::home_body`), a market is **runtime state, not a
generation artifact**: it comes into existence the tick a body's first building *completes*
(`maybe_spawn_market`, called from both `construct_building`'s instant-completion path and
`run_construction`'s pacing loop, `economy_system.cpp`) — investment, not presence; a survey
(BL-067) completing is explicitly **not** the trigger, keeping the geographic and commercial fogs
independent (`docs/ui/DISCOVERY.md`). **Any** corporation can cause one; the player does not learn
of a rival-created market for free — it enters at the activity fog's Unknown tier
(`docs/ui/DISCOVERY.md`) like any other undiscovered activity. Exactly **one market per body**
off-world (BL-096's population-anchored carving does not apply — an outpost has no population to
anchor to); the home body's own carved multi-market seeding is untouched. A market **never
disappears** — nothing in the engine ever removes an entry from `world::markets`; an outpost whose
last building is decommissioned simply goes dormant (stops clearing anything, per the ordinary
zero-supply/zero-demand case), which the activity fog's existing Stale tier already models.

**Opening prices** come from the home body's own `base_price`, marked up by distance
(`market_emergence_params.price_distance_gain`, `economy.market_emergence` in Lua) rather than
from `world_gen`'s flat `base_price` table or from EMA smoothing, which cannot run with no price
history. A resource untradeable at home (`base_price` 0) stays untradeable at the outpost.

**What clears there.** An outpost has real supply (whatever it produces) and essentially no local
demand, so clearing only against local population would collapse its prices to the floor the
instant it started producing — the failure mode BL-263's design flagged as most likely to read as
broken. Instead, `inject_interbody_demand` pulls a distance-discounted slice of the **home body's
own unmet demand** (its `demand - supply`, when positive) onto every outpost market's demand each
tick, additive alongside `inject_population_demand` — "nobody builds a mine on a moon to sell to
the moon." This is deliberately simpler than modelling every possible source/destination pair: the
home body is the one market every outpost is presumed to ultimately feed, directly or through
`dispatch_convoys`' own existing physical routing (which independently moves the corp's stockpiled
surplus between bodies — this mechanism only shapes the outpost's local *price*, it moves no
goods itself).

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
3. **Population demand injection** (`inject_population_demand`, BL-190 ordering fix + BL-368
   basket generalisation) — each population centre pulls a price-elastic, multi-resource DEMAND
   from its catchment market (food rations, agricultural produce, water, clean water, consumer
   goods, medical supplies): `pcc.scale × demand_scale × basket[r] × elasticity(price)`. Same
   elastic shape as the substrate injection above, but population is a pure **consumer** — no
   supply term. Tunables in `scripts/economy.lua` § `population_demand`. Runs after the reset (and
   after substrate injection) for the same reason substrate does: written before the reset, it
   would be erased the same tick it landed (the bug BL-190 fixed, 2026-07-31).
4. **Auto-surplus** — each `(corp, body)` pool lists everything above its **processor
   reservation** (the inputs its own processors need for a full run next tick) for sale. A
   resource under a standing sell order is exempted — the manual order governs.
5. **Standing sell orders** — read from `world::sell_orders` (BL-293: the book is world state,
   placed by the player and by rival corps through the same `place_sell_order` verb), quantity
   capped by the pool, entered into both market supply and the explicit sell book with their
   `floor_price`. Multiple orders against one `(corp, body, resource)` share a **running
   remainder**: total listed quantity never exceeds the pool, each order's matched/auto-cleared
   quantity is tracked per order, and pool debits clamp at zero.
6. **Auto-demand** — processor input shortfalls and construction material draws
   (`report.purchases`) enter market demand.
7. **Standing buy orders** — read from `world::buy_orders`, entered into demand and the
   explicit buy book (`max_price`, optional `preferred_seller`).
8. **Reference prices** — computed once from the accumulated supply/demand (below), so every
   flow this tick uses the same price.
9. **Auto clearing** — auto-surplus sells, and auto-demand buys, at the reference price. The
   market is a **perfect counterparty**: these always clear in full.
10. **Order-book matching** (BL-037, preferential purchasing — shipped) — explicit sells vs
    explicit buys by price-time priority: cheapest ask first, highest bid first, corp id as the
    deterministic tiebreak. A buyer's `preferred_seller` is served first, tolerated up to
    **1.10×** the cheapest compatible ask. Trades clear at the **seller's ask**.
11. **Buyer of last resort** — unmatched player sell quantity auto-clears at
    `max(reference_price, floor_price)`, so listed supply always clears (prototype invariant).
12. **Price update** — where explicit trades occurred, the price eases toward their VWAP;
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

## Procurement — a layer over the market, not a second market (BL-350, 2026-08-11)

A procurement contract is **a build order placed with someone else**: BL-095's own
commit-on-affordability, draw-materials-per-tick, pay-across-the-build shape, with the materials
drawn against the **supplier's** market and the output delivered to the **buyer's** pool. The
counterparty is a NAMED corp with a price, a lead time, and a possible refusal — not a purchase
order against an unlimited market, and not an order-book entry (the book is price-time priority
over anonymous asks; it has no representation for a named counterparty or a lead time). It joins
the same `corp_command` seam the order book does, for the same reason: the player's press and the
AI's command are one implementation.

- **Three verbs** (`corp_verb::request_quote` / `accept_quote` / `cancel_contract`, `world.hpp`
  §11–14, append-only after `set_workforce_auto`). `request_quote` evaluates four decline
  conditions in order — no capacity (the supplier holds no completed building that produces the
  good), no input access (the supplier's local market cannot supply its recipe's inputs), embargo
  (`world::corp_embargo_conditions`, a `condition_set` per supplier — BL-342's generic predicate
  machinery reaching procurement for free), reputation floor (`world::corp_reputation`, one scalar
  per (buyer, supplier) pair) — and returns a distinguishable `corp_command_result` for each.
- **Split payment.** A deposit (`economy.procurement.deposit_fraction`, first cut 0.25) debits at
  `accept_quote`; the remainder is drawn evenly across the quote's `lead_time_ticks`
  (`economy_system.cpp`'s contract-pacing pass, right after the capability-points pass). A known
  simplification against BL-095's own model: the pace is fixed rather than market-gated
  (stretch/pause on the supplier's live throughput) — a follow-on refinement, not built here.
- **Lead time is derived, not authored**: `base_lead_ticks x ceil(quantity / supplier_throughput)`
  — a bigger order takes longer, a capable supplier is faster, and the quote is incidentally an
  intelligence channel (legitimate under BL-068: the supplier volunteers its own throughput in the
  price it quotes).
- **Reputation moves only on completion (+) or cancellation (−)** — narrow by design: it shifts
  price/tie-breaking, never gates access beyond the decline floor above (that is BL-087's job).
- **Persistence.** `procurement.{hpp,cpp}`, the fourth flat-binary stream in `world/*`: magic
  `IOPC` + version, `world::procurement_quotes` / `procurement_contracts` / `corp_reputation`
  round-trip in stored order, and a bad stream is refused rather than reinterpreted.
- **The militia's own demand.** `spacecraft_components` (BL-340) carries no background demand —
  a militia's contracts are its only buyer, which is what makes the BL-340/BL-350 coupling real
  rather than thematic.

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
- **Population demand's undersupply effects are unwired.** `inject_population_demand` (§ The
  clearing tick, above) pulls real, price-elastic, multi-resource demand into the market every
  tick — the BL-190 ordering bug and the BL-368 single-good stub are both resolved (2026-08-11).
  What is *not* built: a shortfall against that demand does nothing to habitability, workforce
  efficiency, or growth (RESOURCES.md § Habitability goods names these as the intended effects).
  The demand signal moves prices; it does not yet feed back into the population/workforce model.
- **Markets are static.** Seeded once at world-gen; no runtime creation, destruction, or
  migration.

## Owed follow-ons

The market rework family — all **open**, so named here without design import:

- **BL-130 (real market inventory)** — design-owed
- **BL-131 (player market destruction)** — design-owed
- **BL-132 (market cogeneration)** — designed
- **BL-160 (auto-exchange policy)** — designed
- **BL-161 (counterparty allow/deny)** — designed
