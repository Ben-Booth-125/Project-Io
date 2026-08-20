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
broken. Instead, `inject_interbody_demand` pulls a distance-discounted slice of a **home-body
counterparty's unmet demand** onto every outpost market's demand each tick, additive after
`inject_population_demand` and `inject_background_demand` — "nobody builds a mine on a moon to
sell to the moon." This only shapes the outpost's local *price*; it moves no goods. Physical
movement is `dispatch_convoys`' independent job.

**Whose demand — the counterpart market.** Per resource, the outpost reads its **counterpart**:
the home-body market carrying the **greatest demand for that resource**, with the lowest market id
breaking ties. Not an aggregate over the body, and emphatically not "the home market" — the home
body carries nine carved markets on seed 0 (BL-096), and *no single one of them stands for the
body*.

That is the correction BL-406 made, and it is worth stating what it replaced. The pull used to
read `market_for_body(w, w.home_body)` — the **lowest-id** market on the body — and treat its
demand as the body's. Measured, that market held **5% of the body's demand**; and because
`world::markets` is unordered, a g++ build of the same seed selected a *different* market
entirely. Same-seed reproducibility within a binary always held, so this was never a determinism
violation — it was a price input decided by an implementation accident. The counterpart rule is a
total order (strictly-greater demand, then smaller id), so every standard library names the same
market. `interbody_pull_harness` asserts that by re-deriving the relation over a reversed
traversal.

Under today's model every home-body market sits at the same distance from a given outpost, so the
relation is **many-to-one keyed on the resource**; the outpost dimension is carried entirely by the
distance falloff. Per-(outpost, resource) counterparts would only start to mean something if
markets acquired a per-market haul cost.

**Against what — the netting.** `shortfall = counterpart.demand − counterpart.supply_last_tick`,
and a non-positive shortfall pulls nothing: a counterparty already meeting its own appetite does
not reach out for an outpost's goods. The subtrahend is the **previous tick's end-of-tick supply**,
captured by `snapshot_market_supply` before the reset in step 1 below.

The one-tick lag is deliberate (BL-404). Reading supply live is what made this a no-op for as long
as it existed: the reset zeroes every market's supply immediately before this injection and the
supply writes land after it, so the subtrahend was identically zero and every outpost was pulled by
**gross** home demand. The alternative — moving the injection after the supply writes — was
rejected because those writes are themselves demand-sensitive (auto-surplus yields to standing
orders), which would make a pass whose ordering is already load-bearing into a two-pass dependency.
The snapshot is local to `clear_markets`; nothing is persisted and the save format is untouched.

**Ordering is now a requirement, not a convenience.** The injection must run *after* the population
and background demand injections, because the counterpart is chosen by this tick's demand and those
two are what deposit it.

> **Magnitude moved, and was not re-tuned (2026-08-17).** Ben's ruling cleared outpost prices to
> move. Measured on seed 0: two of three pulled resources now pull at **2.8×** their previous size
> (the counterpart wants far more than the lowest-id market did) and one is **silenced outright**
> by the now-real netting — its counterpart holds 260.7 supply against 5.5 demand.
> `pull_fraction` is still its authored 0.50, a number chosen against a source that no longer
> exists; re-tuning it is folded into BL-440's repricing pass (NR-277).

## What trades

Only resources with a non-zero `base_price` on the market participate; everything else is
skipped by every clearing path and `resolve_price` leaves its price untouched (at 0). The
tradeable set — seven authored prices plus the distance-priced endemic goods (BL-191) — and the
minable-but-unsellable asymmetry of the BL-040 raws are catalogued in
`docs/economy/RESOURCES.md` § What actually trades.

## The clearing tick

`clear_markets` runs once per economy tick, after production (`run_economy_step`). In order:

1. **Reset** — every market's `supply` and `demand` arrays zero. There is no stored inventory;
   both are per-tick flows. A snapshot of every market's supply is taken *immediately before* the
   zeroing (`snapshot_market_supply`), because the inter-body pull needs a supply that this pass
   has not yet computed — see § Spontaneous market emergence.
2. **Background-firm production** (BL-365, 2026-08-11) — real corporations
   (`corporation_component.is_background = true`), generated at world-gen and running the same
   corp_ai scored-utility layer as rivals, produce, sell, and buy through the ORDINARY steps of
   this tick (auto-surplus, standing orders, auto-demand — steps 4, 5, 6 below) exactly like any
   other corp. There is no separate injection step for background supply any more: saturation and
   the live opportunity margin are now **emergent** from real generated firms, not asserted by a
   function. This replaces `inject_substrate_demand` (BL-078), which wrote an abstract
   per-(nation,body) `nation_substrate` capacity/demand pair straight into the market arrays
   without any building behind it; that function, `nation.resource_abundance`, and the substrate
   capacity arrays are deleted. See § Background corporations below.
3. **Population demand injection** (`inject_population_demand`, BL-190 ordering fix + BL-368
   basket generalisation) — each population centre pulls a price-elastic, multi-resource DEMAND
   from its catchment market (food rations, agricultural produce, water, clean water, consumer
   goods, medical supplies): `pcc.scale × demand_scale × basket[r] × elasticity(price)`. Population
   is a pure **consumer** — no supply term; the corresponding supply now comes from real background
   firms (step 2), not from an elastic substrate. Tunables in `scripts/economy.lua` §
   `population_demand`. Runs after the reset for the same reason step 2 must: written before the
   reset, it would be erased the same tick it landed (the bug BL-190 fixed, 2026-07-31).
4. **Auto-surplus** — each `(corp, body)` pool lists everything above its **processor
   reservation** (the inputs its own processors need for a full run next tick) for sale. A
   resource under a standing sell order is exempted — the manual order governs.
5. **Standing sell orders** — read from `world::sell_orders` (BL-293: the book is world state,
   placed by the player and by rival corps through the same `place_sell_order` verb), quantity
   capped by the pool, entered into both market supply and the explicit sell book with their
   `floor_price`. Multiple orders against one `(corp, body, resource)` share a **running
   remainder**: total listed quantity never exceeds the pool, each order's matched/auto-cleared
   quantity is tracked per order, and pool debits clamp at zero.
6. **Auto-demand** — two registers, read separately (BL-441; see § Want and fill below).
   `report.wants` — what processors and construction sites set out to buy this tick, whether or
   not they got it — enters **market demand**, and is the only thing that does.
   `report.purchases` — what was actually drawn — enters the **billing** pass instead. BL-130:
   that billing is not a fresh grant; the real transaction already happened, capped by real
   inventory, in the PRIOR phase of the same tick (production and construction both run before
   `clear_markets`; see § Real market inventory below).
7. **Standing buy orders** — read from `world::buy_orders`, entered into demand and the
   explicit buy book (`max_price`, optional `preferred_seller`).
8. **Reference prices** — computed once from the accumulated supply/demand (below), so every
   flow this tick uses the same price.
9. **Auto clearing** — auto-surplus sells at the reference price (**perfect counterparty**: the
   sell side is still unconditional — see § Real market inventory); auto-demand billed at the
   reference price for whatever was already drawn in step 6.
10. **Order-book matching** (BL-037, preferential purchasing — shipped) — explicit sells vs
    explicit buys by price-time priority: cheapest ask first, highest bid first, corp id as the
    deterministic tiebreak. A buyer's `preferred_seller` is served first, tolerated up to
    **1.10×** the cheapest compatible ask. Trades clear at the **seller's ask**.
11. **Buyer of last resort** — unmatched standing-order quantity auto-clears at the
    **reference price**, and only if the order's floor allows it: an order whose
    `floor_price` exceeds the reference price clears **nothing** and its stock stays in the
    pool. The floor is a reservation price — "hold rather than sell below this" — never a
    price the market is made to pay. *(Corrected 2026-08-14, BL-386: the previous rule,
    `max(reference_price, floor_price)` with no counterparty debited, let a seller name the
    price a perfect counterparty pays — unbounded income, found by AI play. The auto-surplus
    path (step 9) is unchanged: clearing at the market's own resolved price is the defensible
    prototype simplification; paying a seller-named price was not a simplification of a
    market but the absence of one.)*
12. **Price update** — where explicit trades occurred, the price eases toward their VWAP;
    otherwise it takes the reference price.

Cash flows accrue per corp and are applied to balances by `apply_budget`
(`src/world/budget_system.cpp`).

## Background corporations (BL-365, 2026-08-11)

**The market saturates because real firms produce and consume, not because a substrate pass
injects supply and demand.** Previously `inject_substrate_demand` (BL-078) wrote an abstract
per-(nation,body) `nation_substrate` — a capacity figure with no building behind it — straight
into the market's `supply`/`demand` arrays each tick, clearing demand to a fixed
`clearing_fraction` (first cut 0.90) by construction. That function, `nation.resource_abundance`,
and the substrate capacity arrays are deleted; nothing in the engine injects fictional supply or
demand any more.

In its place, world-gen runs a **second, later corporation-generation pass**
(`docs/generation/CORPORATION_GENERATION.md` § Pass 6) that places real background firms — real
buildings, on real tiles, with `corporation_component.is_background = true` — until the body's
real production meets the same ~90% target fraction of real demand. The count is **calibrated**,
not authored: generation keeps adding firms/holdings until the measured production/demand ratio
crosses the target, so the figure stays correct as recipes, deposits, or population are retuned.

Background firms are not a cheaper stand-in for the player's rivals. They run the **full corp_ai
scored-utility layer** — build, demolish, survey, road, hire, and trade decisions, identical to
the handful of named rival corps (Ben, 2026-08-11, overriding a reduced-model recommendation) —
against the same `corp_command` seam and the same market this section documents. A background
firm's sell orders, auto-surplus, and auto-demand are ordinary rows in steps 4–7 above; there is
no separate code path for them. This also inherits BL-340's background-demand step for silicon,
refined_copper, ree_alloy, machinery, alloys, and electronics — **not** `spacecraft_components`,
which stays player/AI-procurement-only so the militia's contracts remain its only buyer
(`docs/economy/MARKETS.md` § Procurement above).

**Data-model hook, not a UI change.** `is_background` exists on `corporation_component` today so
generation and `export_corp_blackboard` (BL-206) can distinguish "background noise" from a named
rival. Aggregating ~80 background firms into one entry on the Corporation lens and dashboard is
future UI work, out of this item's scope — today every corp, background or not, is still listed
individually on any surface that enumerates corporations.

**BL-130 (real market inventory, below) is a hard prerequisite, not a neighbour.** Real background
firms selling into a market that still conjured any buy-side shortfall would undercut the entire
point of modelling real producers, which is why this item required BL-130 to land first.

## Want and fill — the demand register records the bid (BL-441, 2026-08-17)

**A want is a bid; a fill is a receipt.** This is the buy-side twin of the sell-side distinction
BL-422 landed below: `supply` is an **offer** and `inventory` is a **delivery**. Demand needed the
same split, and did not have it.

`mc.demand` used to accrue from `report.purchases` — what a corp **actually bought**. But hunger is
precisely the state in which no purchase completes, so the register was silent exactly when it
mattered most. A processor that needed 16 units of an input and could draw only 2 told the market
it wanted 2; one that could draw nothing told it nothing at all. The resource then read to
`resolve_price` as one almost nobody wants, its price never rose, and scarcity was **invisible to
the price signal**. `resolve_price` was never wrong about this — its own branch for zero supply
against real demand takes the price to the top of the band — it was being starved of input.

**Two registers, one of them priced.** `economy_report` now carries `wants` alongside `purchases`,
both `std::map` keyed by `(corp, body)`:

- **`wants`** — the full-run input need, computed **before** any coverage decision, so it is the
  same number whether the draw then succeeds, runs short, or fails outright. Registered by
  `run_processing` before its starved early return, and by `run_construction` **before** its
  `rate <= 0` pause check, so a build stalled for want of steel finally says so. This is the sole
  input to `mc.demand`.
- **`purchases`** — what was actually delivered. Still feeds `auto_buys`, the VWAP accumulator, and
  the expenditure a corp is charged. **Never pay against `wants`**: that would credit deliveries
  nobody made, which is BL-422's defect pointed the other way.

**The want is net of the corp's own pool** — the full-run need less what it already holds — because
`mc.demand` is compared against `mc.supply`, an *offer to the market*, so demand must be the *bid to
the market* for the ratio to mean anything. A corp feeding its smelter from its own mine is not
bidding for the input and must not push its price. (Recorded as a delegated call: NR-281.)

**Determinism.** Both registers are `std::map`, so accumulation runs over a **sorted** key set. This
is the same seam where BL-422 found a latent `unordered_map` float-accumulation nondeterminism; the
container choice here is that lesson, not an accident.

**What it moved.** Scarce inputs stopped being free of price pressure, so they got dearer:
measured over `tier_margin`'s 3-seed run, processing input cost rose 10.86 → 11.59/tick and
processing net fell −9.52 → −10.42/tick, while extraction net eased 7.27 → 7.11. Several resources
left the price floor for the first time (resource 1 at 0.95× base → 1.83×, resource 9 at 1.00× →
1.28×). Refining paying worse than mining is BL-436's open calibration, not a regression this
introduced — the failing assertion set is identical on both sides of the change.

## Real market inventory (BL-130, 2026-08-11)

`market_component.inventory` is **real, persistent stock** — not reset each tick, unlike
`supply`/`demand`, which stay per-tick FLOW figures for pricing and reporting. It is the
substrate BL-263's outpost markets need (a market clearing against remote demand has stock in
transit and stock on hand, which a derived-from-recent-supply figure cannot represent) and the
end of the market's old role as an unconditional, infinite counterparty on the **buy** side.

**Fills from real corp sales only** — auto-surplus and standing sell orders, tallied separately
from `mc.supply[r]`. This is a residual note from before BL-365 removed the old
nation-substrate's abstract supply (a pricing fiction, never actually sold by anyone); today
every seller filling `mc.inventory` is a real corp, background or not.

**Credited where stock leaves a pool, not where it is listed (BL-422, 2026-08-16).** The fill
used to happen at listing time, on every listed quantity, which was true while everything listed
also sold. BL-386's reservation rule ended that: an order whose floor exceeds the resolved price
**holds**, and its stock never leaves the seller. Crediting it anyway put goods on the shelf that
no seller had parted with — and `inventory` is not a display figure, so a processor bought that
phantom stock, decremented it, and no counterparty was ever paid. The credit now sits in the same
statement as each of the three pool debits — auto-surplus clearing, matched explicit trades, and
the auto-clear pass — so **inventory gains exactly what pools lose**, and the two cannot drift
apart in a later edit. It is a conservation law, not a tally.

A side effect worth naming: the old fill iterated the sell books in `unordered_map` order, so a
float sum accumulated in an unspecified sequence. All three new sites are ordered (`std::map`
pools; sorted market and resource keys), so the credit is deterministic as well as correct.

**Drains during production and construction — both of which run BEFORE `clear_markets` in the
same tick** — against whatever stock survived from **prior ticks'** sales:

- **`run_processing`** no longer runs an unconditional full batch just because a market exists on
  the body. Coverage is now `(pool + market inventory) / need`, computed per input exactly as the
  no-market two-threshold model always did — full batch at/above `t_full`, scaled between
  `t_idle` and `t_full`, idle below `t_idle`. The old "a market body always runs full batch,
  auto-buying any shortfall" special case is retired: a market's stock is real and finite now, so
  it earns its place in the same coverage calculation rather than bypassing it. Draws pool-first,
  then the market's real inventory for the remainder, decrementing it directly.
- **`run_construction`** (BL-095's pacing rate) reads `market_component.inventory` directly in
  place of the old "last tick's cleared supply" proxy, and now actually **drains** it as the
  build draws — the rate calculation is unchanged in shape, only what it reads and consumes.

Because both consumers draw from the SAME live inventory value in a fixed, deterministic order
(construction first, then production — the existing tick order), and a processor's `run` fraction
is bounded by the coverage-min across every input, the total drawn from a market in one tick can
never exceed what was actually on hand — no double-spend, no negative inventory, no ordering
dependence beyond the tick's own fixed pass order.

**The sell side was unchanged by BL-130** — auto-surplus and standing sell orders cleared in full
unconditionally, so a seller saw no difference; what BL-130 changed was only whether a *buyer* (a
processor, a build) could get what it wanted when the market's real shelf was bare. **That is no
longer true**: BL-386 made the floor a reservation price, so a standing order can now hold, and
BL-422 above is what stops a held order stocking the shelf regardless.

**Held stock stays visible to the price signal — deliberately, and for a mechanical reason.**
BL-422's design defaulted to hiding held quantity from `mc.supply[r]` as well, on the argument
that stock which is explicitly not for sale below its floor is not supply. That default is not
implementable as written: whether an order holds is decided by comparing its floor to the
*resolved* price, and the resolved price is computed **from** `supply`. Removing held stock raises
the price, which can un-hold the order that was removed — a fixed point, reached only by iterating,
at a cost in complexity and determinism risk that a pricing nicety does not repay. So the two
arrays are deliberately asymmetric: `supply` is the **offer** (listed stock is a real offer at a
price, and the market may price against knowing it exists), `inventory` is the **delivery** (only
what actually changed hands). Revisit only if a real defect is measured, not on the argument alone.

Verified by `tools/verify/market_inventory_harness.cpp` and `order_book_harness` § R7 (BL-422's
conservation rows).

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

  > **Authoring a `market` condition: it measures the WORLD, not a market (BL-342, NR-114).**
  > `condition_subject::market` resolves to the **mean resolved price across every market in the
  > world**, summed in ascending entity-id order for determinism — not the price in the local
  > market, and not the corp's own markets. There is no market qualifier on `condition`, because
  > a law or tech asking "is this good expensive yet?" is asking a world-level question, and a
  > mean is harder to game than a max. The consequence to author around: **a corp trading in one
  > expensive market cannot satisfy a market condition on its own.** If a per-market predicate is
  > ever wanted, add a qualifier to `condition` rather than changing what this subject means.
  > `evaluate` also takes a **subject corp** (`condition_set::evaluate(set, world, subject_corp)`),
  > since every consumer — a levy charged to a corp, a tech earned per corp — is per-corporation;
  > pass `null_entity` for a genuinely world-level predicate and the corp-scoped subjects measure
  > zero.
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
- **Persistence.** `procurement.{hpp,cpp}`, the **third** flat-binary stream in `world/*` —
  `history_log` (`IOHL`) and `order_book` (`IOOB`) are the other two, and there is no fourth
  *(corrected 2026-08-12: this line read "the fourth"; `grep magic src/` finds exactly three)*.
  Magic `IOPC` + version, `world::procurement_quotes` / `procurement_contracts` /
  `corp_reputation` round-trip in stored order, and a bad stream is refused rather than
  reinterpreted. All three carry the header BL-107 specifies; what BL-107 still owes is the
  **world-snapshot** header, which no stream here provides.
- **The militia's own demand.** `spacecraft_components` (BL-340) carries no background demand —
  a militia's contracts are its only buyer, which is what makes the BL-340/BL-350 coupling real
  rather than thematic.

### What a contract is actually worth (BL-392, 2026-08-20)

Everything above described the seam correctly and the **deal** wrongly. A player ran twenty
contracts, took delivery of 200 units of iron ore, and could not detect a single delivery. Three
faults compounded, and all three are now fixed:

1. **Goods land on the BUYER's body.** Delivery credited `pool_for(buyer, contract.body)` — and
   `contract.body` is where the **supplier** fulfils from. On a body the buyer holds no processor
   reservation on, the auto-surplus path liquidated the whole delivery in the tick it landed, so
   every completed contract was invisible by construction. A contract now carries a
   **`delivery_body`** — the buyer's own body, taken as the body of the lowest-id building they
   own (lowest id, not first-in-`assets`, because a demolish permutes that list and the quote must
   be reproducible). It degrades to the supplier's body only when the buyer owns nothing anywhere.
2. **A commitment buys a discount.** `unit_price` was the supplier's live spot price and the
   liquidation happened at that same spot price, so a measured 20-unit iron round trip settled at
   exactly **−0.14 credits**: break-even minus friction, by construction, and strictly worse than
   simply buying on the market. The quote is now spot less a **volume discount**, asymptotic in
   the order size — `volume_discount_max × q / (q + volume_discount_half_quantity)`, authored in
   `scripts/economy.lua` under `economy.procurement` — so no order however large drives the price
   to zero, and the terms improve monotonically with the size of the commitment.
3. **Lead time reads the SUPPLIER.** The divisor was `processing_facility.base_rate`, a **global
   constant**: a supplier with ten facilities quoted the same term as one with a single stalled
   site. It is now that supplier's real per-tick throughput of that good — extraction sites
   targeting it at their own rate, processing facilities at their recipe's yield of it times
   theirs, summed in ascending building id (a float sum needs a fixed order). Floored at 1 tick:
   a contract completing in zero ticks is a spot purchase wearing a contract's name.

**Freight is the price of the distance.** Delivering across bodies costs
`offbody_freight_fraction` of the order's pre-discount goods value, carried on the contract as
`freight_cost` and included in the total the deposit and the instalments are computed from. It is
set **below** `volume_discount_max` on purpose, so a genuine volume order still beats spot after
carriage; a same-body delivery pays nothing.

**Every credit this seam moves is a TRANSFER.** This is the fix nobody asked for and the one that
mattered most: the deposit and every paced instalment used to leave the buyer's balance and
**arrive nowhere**. Every contract in flight was quietly burning credits out of the economy. The
supplier is now credited exactly what the buyer is debited, in the same statement, deposit,
instalments and freight alike — the supplier arranges the carriage, so paying them for it keeps
the flow closed. On completion the goods are **drawn from the supplier's pool** at the fulfilment
body as far as their stock goes, with any shortfall built to order (which is what a build order
placed with someone else means).

Verified by `tools/verify/money_conservation.cpp`.

## Price resolution

`resolve_price`, per (market, resource):

```
target = base_price × √(demand / supply)     — damped elasticity
         base_price                           when neither side has a signal
         base_price × ceil_mult               demand with zero supply
price  = prior + 0.5 × (target − prior)       — EMA smoothing
```

Target and result are clamped to the band **[0.25×, 10×] of base**. Prices are therefore
*anchored*: no scarcity can push a good past 10× its authored base, and no glut below a quarter
of it. Untradeable resources (`base_price ≤ 0`) keep their prior price.

### Where the band lives (BL-442, 2026-08-17)

The band is **authored once, in data**: `scripts/economy.lua` → `economy.price_band`
(`floor_mult` / `ceil_mult`), reaching C++ as `price_band_params` through
`recipe_registry::price_band()` — the same route every other economy tunable takes.

It is read by **two** call sites, and that is the point of naming it here:

| Site | Function | What it clamps |
|---|---|---|
| `src/world/market_clearing.cpp` | `resolve_price` | Every market price, every tick — the real clearing band. |
| `src/world/economy_system.cpp` | `wf_target_price` | The BL-181 workforce auto-solver's **forward** price estimate, so it prices a candidate target against the band the market will actually clear it in. |

Until BL-442 those were two hand-synchronised `constexpr` copies, the second commented
"mirror market_clearing.cpp price band" — with nothing enforcing the mirror. Editing one and
not the other would have left the solver optimising against a band the market does not use: a
silent divergence with no error and no visible symptom. `tools/verify/price_band_harness.cpp`
is now the guard. Its rows are **differential** — each sets a non-default band and asserts the
site *moves* — because a guard that only exercised one site would have passed before the change
and would pass again the day someone reintroduces a local copy.

The move was deliberately **behaviour-identical**: the authored values were the constants they
replaced, and the struct defaults reproduce them exactly so a harness that hand-builds a
registry is unchanged.

### Where the ceiling comes from (BL-442 step 2, 2026-08-17)

`ceil_mult` is **derived, not authored**. Ben's requirement (2026-08-17) is that scarcity must
price high enough to **cross the margin for a nearby market**, so inter-market trading is a
day-1 fact rather than a late-game unlock. That makes the ceiling a function of the haulage the
supply layer already charges:

```
ceil_mult × base_price  >  base_price + haulage_per_unit
ceil_mult               >  1 + haulage_per_unit / base_price
```

**The haulage is measured, not assumed.** `tools/verify/haulage_measure.cpp` walks every market
in the real generated world, finds its nearest market neighbour by the terrain-weighted A* cost,
and reports `logistics_cost(mode) × path.cost` — *exactly* the per-unit figure `dispatch_convoys`
debits (`supply_system.cpp`). Over 5 seeds, 39 markets on 5 multi-market bodies:

| Market → nearest market neighbour | credits per unit |
|---|---|
| p10 | 0.12 |
| median | 0.70 |
| p90 | 1.67 |
| max | 4.83 |

The denominator is the **cheapest good carrying a base price: 0.60**. The binding case is
therefore the worst haul against the cheapest good:

```
ceil > 1 + 4.83 / 0.60 = 9.06   ->   10.0
```

**Why 4.0 was not enough.** It cleared the *median* neighbour pair (which needs 2.16) and only
just cleared the p90 (3.79). The tail — the worst-connected market pair carrying the cheapest
good — was permanently unservable at any scarcity. 10.0 covers **every** nearest-neighbour pair
measured, for **every** priced good.

**A second, independent reading agrees.** The requirement's other half is that a scarce cheap
good must be able to outprice an abundant dear one. Read *within a tier* — which is the only
coherent reading, since RESOURCES.md promises margin widens *between* tiers — the ordinary raw
tier spans 0.60 to 6.00 (`rare_earth_ore`), demanding `ceil > 10`. The two derivations land on
the same number, which is the reason to trust it. Read *across* tiers it would demand 233
(0.60 against `spacecraft_components` at 140), which would delete the tier model; that reading
was rejected and recorded (NR-291).

**The floor is unchanged at 0.25×.** The requirement derives a ceiling and says nothing about a
floor; lowering it would widen the arbitrage margin only by cutting what an abundant producer
receives (NR-290).

**Re-derive rather than trust.** Re-run `haulage_measure` whenever the logistics cost table
(`logistics.base_cost_per_unit_distance`), the map scale (`body_km_per_tile`), or the
`base_price` table changes — all three move the number this ceiling is computed from.

**What the widening did NOT do.** It did not create inter-market trade. Measured at the *old*
band, 1,677 of 2,146 dispatched convoys were already intra-body market-to-market hauls. What is
zero — at both bands — is **inter-body** trade: 0 space-lane convoys and 0 persistent
`trade_routes` across all five seeds, because that lane is refused by the launchpad and
propellant gates in `dispatch_convoys` before any price is consulted. `trade_routes` is a
body-level record, so BL-088's route store is structurally blind to the intra-body trade that
*was* happening, which is very likely why it was believed to be absent (NR-289).

## Inter-body linkage

There is no abstract price coupling between bodies — **the convoy is the coupling**
(`docs/economy/SUPPLY.md`). `dispatch_convoys` reads last tick's cleared shortfalls
(demand − supply per market) and hauls from the cheapest reachable corp pool — distances and
haul prices read **tick-pure orbital angles** (`orbital_angle_at_tick`, BL-354), so dispatch is
identical at any frame rate; the smoothly-advancing render angles are display-only. An arriving
convoy credits the destination pool **only** — the cargo reaches the market's supply through the
ordinary auto-surplus path off that pool at the next clear. (BL-382, the dead market writes,
removed a direct supply write on arrival: `credit_arrived_convoys` runs *after* `clear_markets`
in the tick, so the write was zeroed before pricing ever read it — while the pre-clearing AI
scorer *did* read it, a private signal nothing priced agreed with.) Since only Kepler has
markets, a marketless body's
processors fall back to the two-threshold pool model (PRODUCTION.md § Processing), and goods
extracted there enter the economy only by convoy to a Kepler market.

## Known limitations (honest list)

- **The SELL side has no *volume* cap.** `market_component.supply` stays a derived per-tick flow
  for pricing purposes, and the market absorbs any quantity a seller is willing to release at the
  resolved price — BL-130 made the BUY side real and finite; sell volume remains unconditional.
  What is no longer unconditional is the *price*: since BL-386 an order whose floor exceeds the
  resolved price holds rather than selling (§ Real market inventory), so a listed quantity is a
  ceiling on what may clear, not a guarantee that it will.
- **Anchored prices.** The [0.25×, 4×] band caps every scarcity signal at 4× base. This is the
  band the BL-078 (product-market inertness) diagnosis found products pegged against — resolved
  in *shape* first by the elastic substrate and now by real background-firm supply (BL-365,
  prices discover within the band against real production), but the ceiling itself is retained
  deliberately, to be retuned once real ranges are visible. A resource pegged at exactly 4× base
  is now a generation-calibration bug (background production absent or under-target), not a
  legitimate "lucrative fillable gap" — see `docs/generation/CORPORATION_GENERATION.md` § Pass 6.
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
