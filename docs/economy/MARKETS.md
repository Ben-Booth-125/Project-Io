# Project Io — Markets

> **Settles:** where a market centre is and what it covers · the shelf: who stocks it, who draws
> on it, and at what price · how a market clears · how a price resolves and what bounds it · where
> a demand want comes from · what a market does when it cannot clear.
> **Not here:** how goods move from one market to another (TRADE) · the money loop the proceeds
> land in (FINANCE) · the shipment in transit (SUPPLY) · what the road costs (LOGISTICS) · a priced
> promise between named parties (CONTRACTS).
> **Confused with:** TRADE.md, FINANCE.md, CONTRACTS.md, SUPPLY.md.

The market model: `src/world/market_clearing.cpp`, the market component in
`src/world/components.hpp`, and the seeding in `src/world/hard_coded_world.cpp`. Production's side
of the exchange is `docs/economy/PRODUCTION.md` § Output and the shelf; which resources trade
at all is `docs/economy/RESOURCES.md` § What trades. Contracts — the alternative to the market,
priced and paced between named parties — are `docs/economy/CONTRACTS.md`.

A market is **anonymous, instant and price-only**. Every seller meets the market, not a buyer; a
trade clears in the tick it is listed; and the only term is the price. Everything else about an
exchange — a counterparty, a lead time, a refusal — is a contract, not a market.

## The shelf economy

**Corporations hold no stockpiles; every good is on a market's shelf (Ben, 2026-10-10).** The
market owns its shelf. This replaces the corporation pool — the per-(corporation, market) store
production used to land in, list from, and draw on — and with it everything that read a pool.

- **Production lands on the shelf, and landing is selling.** What a building makes goes onto the
  shelf of its own market the tick it is made, and its owner is paid the quantity at that tick's
  clearing price — the market is the counterparty, whether or not anyone bids (Ben, 2026-10-10).
  A glut drives that price to the floor: that is the signal, and the maker feels it.
- **The order book retires (Ben, 2026-10-10).** No standing buy or sell orders: everyone buys at
  the posted price, under the fair-price ceiling. Procurement contracts are not the order book and
  stay (`CONTRACTS.md`); they deliver from and to shelves.
- **Everyone buys from the shelf.** A processor's inputs, a construction site's materials,
  building upkeep, procurement, the space programme and a launch's propellant are all bought on
  the shelf at the posted price, under the fair-price ceiling. A corporation buys even its own
  output back: vertical integration is a location, not a free transfer.
- **Goods leave a market only by trade** (`TRADE.md`), which buys on one shelf and lands on
  another.
- **The opening stock is on the shelves.** Generation's opening stockpile — the same total it
  seeds — is placed on the shelves of the markets the corporations sit in, each corporation's
  share on its own markets (`../generation/CORPORATION_GENERATION.md` § Pass 4b).

**What retires with the pool.** Auto-surplus and the processor reservation; the order book (standing buy
and sell orders, and the sell order's floor); the pad's propellant reserve; opening stock held until a bid; the
corporation convoy and the market export (`TRADE.md` § What trade replaces); the workforce
dial's stock-fed draws (`../ai/AI_OPPONENT.md` § 11 — with no pool, a plant's whole want is
posted demand).

---

## Market centres and seeding

A market is a `market_component`: a body, an optional `centre_tile` anchor, and four
resource-indexed per-tick arrays — `supply`, `demand`, `price`, `base_price` — plus a persistent
`inventory` (§ Real market inventory). Generation seeds markets **only on the home body**; every
other body's markets emerge at runtime (§ Spontaneous market emergence).

Seeding is population-anchored but **resource-carved** (BL-096, market resource generation):
markets anchor to population-centre tiles, and how finely a nation's territory fractures into
markets follows its tradeable-resource concentration — a resource-rich nation admits smaller
centres (more markets), a barren one folds into its neighbour's. One pass at world-gen,
deterministic, with a seeded jitter on the borderline. If no centre qualifies, one unanchored
fallback market is seeded. The home body carries many markets, and *no single one of them stands
for the body*.

**A living polity's capital carries a market, priced at a premium (Ben, 2026-09-23).** The Era −1
history marks a market at every living polity's capital at the 1200 close (`CIVILISATION.md`), and
those markets stand on the home body beside the carve's. Each is priced like a carve market — the
template base price for every good, the endemic field where it applies — and then lifted by a
**capital premium** over all of it: a capital is where a realm's court, treasury and demand
concentrate, so goods cost more there. An unpriced market is not a neutral default: a good with no
base price is never listed, so stock in its catchment could never be sold. Owner: BL-1066 (the
player cannot build). A capital's market can still die by the causes below: one standing inside a
larger market's reach folds into it like any other market, and the absorber keeps its own prices.

**A market also emerges where trade CONCENTRATED, not from population alone** (Ben, 2026-09-03,
the eight-phase reorder point 4: *"markets should begin to emerge towards the end of this
phase"*). The carve above is a **nation-grain** judgement — this nation's geology, and how many
corporations already compete in it — and it says nothing about *where inside that territory*
exchange actually happened. The Era −1 history does: it records every corridor it supplied an
army or a founding party along (`LOGISTICS.md` § The ancient network), and a region several of
them **meet** at is a junction. A centre standing at a junction is gated as a rich nation's
centres are, whatever its own nation's concentration says.

Three properties keep that from becoming a second, competing carve:

1. **It only ever LOWERS the gate**, so it adds markets and removes none. Raising the gate at a
   quiet region would delete a market the economy is already built on, and this is an emergence
   rather than a cull.
2. **The floor is the existing fracture gate.** A village never carries a market however many
   roads meet on it — the ladder's own bottom rung does not move.
3. **A junction is a graph property**, not a percentile: the count of distinct corridors
   incident on a region. So it is a plain integer over a sorted record rather than a threshold
   argued from a distribution.

What it produces is the **entrepôt on poor ground** — a barren nation that would otherwise fold
into its neighbour keeps a market where the routes cross. BL-768 (roads and markets from
history) owns the design.

**A market can die, so the world a player is handed carries tens of markets, not hundreds (Ben,
2026-09-25; BL-1125, markets can die).** Emergence alone left most of a world's markets adjacent
to another, idle and unbuilt. Three in-world causes remove them, and none is a count cap:

1. **Twins fold.** A market centred on the same tile as another folds into the lower-id one. Two
   centres whose best site is the same tile have one market place; a twin could never win a tile
   anyway, since catchment ties go to the lowest id.
2. **Gravity fold.** A market inside a larger market's reach folds into it: trade goes where it
   concentrates. "Larger" is catchment population; "reach" is a traversal cost, calibrated so a
   1960 world carries roughly **20–40 markets**, each serving a region of cities rather than one
   apiece (Ben, 2026-10-03: the per-city phrasing dropped; capitals fold like any market). That aim
   sets the constant, measured on the curated seeds; the rule never enforces a count. The walk is
   largest first, on catchments read once before any fold: each market still standing absorbs every
   smaller one whose centre lies within reach of it (the cost of travel toward the larger), and a
   market already absorbed absorbs nothing. **A fold is one a convoy could make (Ben, 2026-10-03):**
   the reach crosses water only between two centres whose regions hold a port, as a sea leg needs a
   port at both ends; otherwise it is measured over land.
3. **Conquest consolidates.** When a realm conquers the region its rival's market stands on, the
   conquered market is destroyed to consolidate the conqueror's strength (Ben, 2026-09-25). The
   history's markets are the survivors' markets, not a monument to every realm that ever stood.

A folded market's catchment and inventory pass to the market that absorbs it. The junction
rule above still only lowers the gate; the folds run after it, on the whole set.

**On a world whose firms come from the charter budget, the competitors the carve counts are the
budget's planned corporations (Ben, 2026-09-26; BL-1086), its specialists as well as its firms
(Ben, 2026-09-27, NR-957)** — every corporation each nation's centres can afford, under the same
ceiling the charter walk honours — so no roster is laid only to be discarded, and the landscape
search, which scores the carve's markets, still runs after the carve.

**Catchment routing:** a tile clears against the market whose `centre_tile` is nearest
(`market_for_tile`), measured on the cylinder (the column distance wraps). A folded market's
centre still counts: a tile nearest to it clears against the market that absorbed it, so a
catchment passes to its absorber whole, never to a third market that happens to stand nearer. **A building clears in its own tile's market** (Ben, 2026-09-15; Ben, 2026-10-10): its
output lands on that market's shelf and its inputs and upkeep are bought there
(`PRODUCTION.md` § Output and the shelf), and goods a trade delivers land and sell at the market
they were delivered to (`TRADE.md` § A trade).

## Spontaneous market emergence

Off-world (any body other than `world::home_body`), a market is **runtime state, not a
generation artifact**: it comes into existence the tick a body's first building *completes*
(`maybe_spawn_market`, called from both `construct_building`'s instant-completion path and
`run_construction`'s pacing loop, `economy_system.cpp`) — investment, not presence. A survey
completing is explicitly **not** the trigger, keeping the geographic and commercial fogs
independent (`docs/ui/DISCOVERY.md`). **Any** corporation can cause one; the player does not learn
of a rival-created market for free — it enters at the activity fog's Unknown tier like any other
undiscovered activity. Exactly **one market per body** off-world (population-anchored carving does
not apply — an outpost has no population to anchor to); the home body's own carved multi-market
seeding is separate. **In play** a market never disappears — nothing in the tick removes an entry
from `world::markets` (the folds of § Market centres run in generation and history only); an
outpost whose last building is decommissioned goes dormant (clears nothing,
per the ordinary zero-supply/zero-demand case), which the activity fog's Stale tier models. The
design is BL-263 (spontaneous market emergence).

**Opening prices** come from the home body's own `base_price`, marked up by distance
(`market_emergence_params.price_distance_gain`, `economy.market_emergence` in Lua, 0.08 per AU)
rather than from `world_gen`'s flat `base_price` table or from EMA smoothing, which cannot run with
no price history. A resource untradeable at home (`base_price` 0) stays untradeable at the outpost.

**What clears there.** An outpost has real supply (whatever it produces) and essentially no local
demand, so clearing only against local population would collapse its prices to the floor the
instant it started producing. Instead, `inject_interbody_demand` pulls a distance-discounted slice
(`pull_fraction`, 0.50) of a **home-body counterparty's unmet demand** onto every outpost market's
demand each tick, additive after `inject_population_demand` and `inject_background_demand` —
"nobody builds a mine on a moon to sell to the moon." This only shapes the outpost's local
*price*; it moves no goods. Goods move only by trade (`TRADE.md`).

**Whose demand — the counterpart market.** Per resource, the outpost reads its **counterpart**:
the home-body market carrying the **greatest demand for that resource**, with the lowest market id
breaking ties. Not an aggregate over the body, and emphatically not "the home market". The
counterpart rule is a total order (strictly-greater demand, then smaller id), so every standard
library names the same market — `world::markets` is unordered, and a rule that read the lowest-id
market would be a price input decided by container layout. `interbody_pull_harness` asserts the
order by re-deriving the relation over a reversed traversal.

Every home-body market sits at the same distance from a given outpost, so the relation is
**many-to-one keyed on the resource**; the outpost dimension is carried entirely by the distance
falloff. Per-(outpost, resource) counterparts would only start to mean something if markets
acquired a per-market haul cost.

**Against what — the netting.** `shortfall = counterpart.demand − counterpart.supply_last_tick`,
and a non-positive shortfall pulls nothing: a counterparty already meeting its own appetite does
not reach out for an outpost's goods. The subtrahend is the **previous tick's end-of-tick supply**,
captured by `snapshot_market_supply` before the reset in step 1 below.

The one-tick lag is deliberate. The reset zeroes every market's supply immediately before this
injection and the supply writes land after it, so a live read would be identically zero and every
outpost would be pulled by **gross** home demand. Moving the injection after the supply writes is
rejected because it would make the counterpart's netting read the supply this same pass is
writing, which would turn a pass whose ordering is already load-bearing into a two-pass
dependency. The snapshot is local to `clear_markets`; nothing is persisted.

**Ordering is a requirement, not a convenience.** The injection must run *after* the population
and background demand injections, because the counterpart is chosen by this tick's demand and those
two are what deposit it.

`pull_fraction` was chosen against an earlier, smaller source of demand; its re-tuning is part of
BL-440's repricing pass (NR-277).

## What trades

Only resources with a non-zero `base_price` on the market participate; everything else is
skipped by every clearing path and `resolve_price` leaves its price untouched (at 0). The
tradeable set is catalogued in `docs/economy/RESOURCES.md` § What trades.

## The clearing tick

`clear_markets` runs once per economy tick, after production, upkeep and the trade pass
(`run_economy_step`, then `run_trades`). Everything a consumer bought off a shelf this tick was
drawn in those phases, capped by what the shelf held; everything that landed this tick — a
building's output, a trade's arriving cargo, a captured cargo, a procurement delivery — waits in
the tick's landing register (`world::landed_this_tick`). The clear lists the one, bills the other,
and sells the landings. In order:

1. **Reset** — every market's `supply` and `demand` arrays zero. Both are per-tick flows. A
   snapshot of every market's supply is taken *immediately before* the zeroing
   (`snapshot_market_supply`), because the inter-body pull needs a supply that this pass has not
   yet computed — see § Spontaneous market emergence.
2. **Background firms** — real corporations (`corporation_component.is_background = true`),
   generated at world-gen and running the same corp_ai scored-utility layer as rivals, land, sell
   and buy through the ORDINARY steps of this tick exactly like any other corp. There is no
   separate injection step for background supply: saturation and the live opportunity margin are
   **emergent** from real generated firms, not asserted by a function. See § Background
   corporations below.
3. **Demand injection** — the pure demand-side pulls, after the reset so they are not erased the
   tick they land. The endemic pull (§ Demand channels) and the inter-body pull (§ Spontaneous
   market emergence) ride here too; the two that shape every market are:
   - `inject_population_demand` — each population centre pulls a price-elastic, multi-resource
     DEMAND from its catchment market, over the cumulative rungs its stratum reaches:
     `heads / heads_per_demand_unit × basket[r] × elasticity(price)`, with rungs 4–5 scaled by the
     nation's qualification and every rung weighted by the catchment's culture
     (`POPULATION.md` § The stratum ladder). Population is a pure **consumer** — no supply term.
     The bid is also kept on the market's own household register (`household_bid`), because
     the households draw it in step 9. Tunables in `scripts/economy.lua` § `population_demand`.
   - `inject_background_demand` — **a labelled STOPGAP**: the offstage economy's own pull on the
     mid-chain processing goods (silicon, refined copper, REE alloy, machinery, alloys, electronics
     — **not** `spacecraft_components`, which stays procurement-only so the militia's contracts
     remain its only buyer), because real background firms alone would under-consume these before
     enough of them exist. **It retires good by good as a real channel claims each one (Ben,
     2026-09-15):** electronics leaves it when the metropolis rung of the household ladder takes it
     (`POPULATION.md` § The stratum ladder), and not before — until then this pull is its only
     final buyer; the intermediates stay until the Industry channel's building upkeep buys them.
     **A body's pull is SPLIT across its markets in proportion to their catchment population**
     (each centre's scale, credited to the market `market_for_tile` gives it, the household
     channel's own attribution), never granted whole to each — a body carved into nine markets does
     not want nine times as much. Per-market scale is gathered in a `std::map` in ascending centre
     id, so accumulation order is deterministic. **The pull consumes what it buys (Ben, 2026-10-07; BL-1217, inputs reach
     processors):** after the households' draw, the background basket draws from the
     market's shelf what it bid, or the whole shelf if it holds less, markets and resources
     ascending (step 10). No money moves, on the households' rule: the market paid the maker when the
     stock landed. A bid that never took goods held a stocked shelf over the fair-price ceiling while
     the processors beside it were silenced. Measured on the sixteen curated seeds with the pull
     split by catchment: input-starved processors per reading fell 47.0 → 35.0 and the share starved
     beside a stocked shelf priced over the ceiling 51% → 25%; processors running at handoff moved
     54.7% → 55.4%, run-rate income 54.4% → 52.3%, firm survival 88.9% → 88.7%. **The cost,
     accepted:** a shelf the pull drains is a shelf a real processor cannot draw next tick.
     **Re-ruled — the pull draws after the processors (Ben, 2026-10-09; BL-1217, G1b):** the
     background basket leaves on the shelf one tick of the market's processor want for that good,
     and draws only what is left above it. The pull still consumes; it no longer takes the input a
     standing processor came for. **That includes want the fair-price ceiling silenced (Ben,
     2026-10-09):** a processor priced out this tick still came for the input, so its silenced
     want is left on the shelf too — the processors' part only, never construction's. The ceiling
     still keeps that want out of the price (`FINANCE.md`); it only keeps the background off the
     goods. Measured before the ruling: the pull took 871 of 943 units a tick
     on the markets where 63 processors starved beside it, and turning the pull off whole moved the
     input-starved share 11.5% → 9.6%. Tunables in `scripts/economy.lua` § `background_demand`
     (`consumes`).
4. **Landings listed** — every landing in the register enters its market's `supply` as this tick's
   listings (§ The shelf economy). It is listed *before* the prices resolve, so a landing moves the
   price it is then paid at: a glut floors its own sale, and the maker feels it.
5. **Wants → demand** — two registers, read separately (§ Want and fill below). `report.wants` —
   what processors, construction sites, upkeep and trades set out to buy this tick, whether or not
   they got it — enters **market demand**, and is the only thing that does. `report.purchases` —
   what was actually drawn — enters the **billing** pass instead (step 8). That billing is not a
   fresh grant: the real transaction already happened, capped by real inventory, in the phases
   before `clear_markets` (§ Real market inventory).
6. **Reference prices** — computed once from the accumulated demand and the supply the price law
   reads — this tick's listings plus the shelf's share, at most k ticks of demand off the stock
   standing after the tick's draws (k is `price_band.shelf_supply_ticks`; § Price resolution,
   below) — so every sale this tick uses the same price.
7. **Landings sold** — each landing's owner is paid its quantity at the reference price, and the
   goods move onto the shelf in the same statement, so the shelf gains exactly what was paid for.
   The market is the counterparty, **bid or no bid** (Ben, 2026-10-10): the sell side is
   unconditional, with no floor and no hold. A landing whose owner is not a corporation shelves
   unpaid. A landing on a market that no longer stands has no shelf to land on and is dropped. The
   register then empties.
8. **Shelf draws billed** — every draw taken off a shelf this tick is billed at the **posted
   price** — the price standing on the shelf when it was drawn, the one the draw checked against
   its reservation ceiling — never the reference price its own want helped resolve
   ([FINANCE.md](FINANCE.md) § Standing-force upkeep, Ben 2026-10-03). The market is the seller:
   whoever stocked the shelf was paid when the goods landed.
9. **Household draw** — the households clearing at each market take their pooled bid off its
   shelf: `household_fill[r] = min(household_bid[r], inventory[r])`, and that much leaves
   `inventory` for good. It runs after every landing this tick has stocked the shelf, so a unit
   made this tick can feed a household this tick. **Households come before the nation (Ben,
   2026-10-05):** processors and construction drew before the clear, households draw here, and
   the nation's own claims on the shelf — network upkeep and the space programme — draw later in
   the tick from what households left. People eat before the roads are mended. **No money moves** — the
   market paid the maker when the stock landed (step 7), so the draw moves goods, not
   credits. **The fair-price ceiling does not apply**: it is a processor's reservation price,
   and a household's reservation is already in its elastic bid. Several centres on one market
   bid one pooled quantity, so a short shelf fills each of them in the same share. Markets
   ascending, resources ascending. The fill is what the growth gate reads
   (`POPULATION.md` § Growth, decline and razing).
10. **Background draw** — the background pull draws next, on the households' terms, from what
    households left, less one tick of the market's processor want:
    `min(background bid, max(0, inventory − processor want))` (step 3, *the pull draws after the
    processors*).
11. **Shelf spoilage** — every good left on every shelf loses its spoilage rate,
    `inventory[r] −= inventory[r] × rate[r]` (§ Price resolution, *The shelf spoils*). After the
    households' draw, so the households' draw is not taxed by its own spoilage (the nation's later
    claims take what spoilage left); before the next
    tick's draws and reference prices read the shelf. No money moves. Markets ascending,
    resources ascending.
12. **Price update** — the reference price stands. Every exchange is with the market, at the price
    the supply and demand state set, so there is no second signal for the price to ease toward.

Cash flows accrue per corp and are applied to balances by `apply_budget`
(`src/world/budget_system.cpp`).

**Production lands at the clear, not when it is made.** A processor cannot draw a sibling's output
from the same tick: the output is still in the landing register while the processors draw, and
reaches the shelf at step 7, for the next tick's draws.

## Demand channels — where a want comes from

A price is resolved against demand, and demand is never ambient. **Every unit of it is injected by
a named pass**, and this section is the register of those passes. It exists because the roster grew
a supply side faster than a demand side and the gap was invisible from inside: a resource can
satisfy the admission rule by naming a consumer nobody ever built.

**The rule, and it is the whole section in one line: a consumer is a MECHANISM, not a noun.**
"Sold to the market" is not a consumer. "Mercantile demand" is not a consumer. A good is wanted
when some pass adds to a market's `demand` for it, or draws it from a shelf — and where no pass
does, the good is dead however plausible its name reads. Design: BL-648 (the admission rule names
an injector).

### The eight channels

Each is owned by the doc that owns its actor; this table is the index, not the design.

| Channel | Who wants it | Scales with | Owner |
|---|---|---|---|
| **Household** | population centres, by stratum | centre scale × era | [`POPULATION.md`](POPULATION.md) |
| **Industry** | every building, as operating upkeep | building count | [`FINANCE.md`](FINANCE.md) |
| **Construction** | anything being built; centres as they grow | build rate, population | [`PRODUCTION.md`](PRODUCTION.md) |
| **Infrastructure** | roads, ports and hubs, kept standing | network size | [`LOGISTICS.md`](LOGISTICS.md) |
| **State** | nations, through budget lines | treasury × weight | [`../politics/NATIONS.md`](../politics/NATIONS.md) |
| **Research** | the tech ladder | research rate | [`RESEARCH.md`](RESEARCH.md) |
| **Conflict** | battles, burning what they fire | war | [`../military/MILITARY.md`](../military/MILITARY.md) |
| **Endemic trade** | a nation's acquired taste | wealth × character | [`RESOURCES.md`](RESOURCES.md) |

### Three properties the set has to hold

**1. Demand must SCALE with the economy, or it decays into a fixed basket.** Household, Industry,
Construction and Infrastructure all grow as the world grows — more people, more buildings, more
road. That is what stops the next roster widening re-opening this hole: a good consumed by
*industry* is wanted in proportion to how much industry exists, without anyone re-authoring a
weight. A channel whose size is a constant is a stopgap, and should be labelled one.

**2. Demand is ERA-BANDED, exactly as recipes are.** This is the single largest cause of the
original gap. Recipes carry an `era` field (BL-433) and are masked by band; the demand baskets did
not, so they were authored in industrial goods and an ancient campaign inherited a basket naming
things nothing in that band can make. **An ancient household wants ceramics, cloth, leather and
dressed stone; an industrial one wants clean water, consumer goods and medical supplies.** Same
mechanism, banded input — no new concept, and the ancient chain's terminal goods stop being dead
ends the moment the basket knows which era it is in.

**3. A channel that CONSUMES without PRICING cannot bootstrap its own supply.** Measured, not
reasoned: BL-641 turned building upkeep on and operating firms collapsed **227 → 19**. Not a
magnitude problem — the goods it drew (tools, planks) are *produced 0.0* in that band, so every
draw went unmet, the supply factor decayed, output followed, and the reflex tier idled the firm.
Halving the rate only delays it. And the loop cannot close from the other end either, because a
**draw that does not bid never reaches a market's `demand`**: wanting tools never raises their
price, so no rival ever scores a Toolmaker and the supply is never induced.

So a channel bids on the market, and its want becomes a price signal. With every good on a shelf
(§ The shelf economy) there is no second road — no corporation's own store to draw on instead — so
a draw that takes goods without posting its want is the defect this property names, wherever it
sits. **A sink that cannot call forth its own supply is a slow way to shut the economy down**, and
the cost of learning that is one harness run rather than a shipped world nobody can play.

**4. Every resource must have a path to a TERMINAL sink** (Ben, 2026-08-31). A terminal sink
consumes a good and produces nothing that must itself be sold: a household basket, an upkeep draw,
a construction cost. **Processing is not one** — it is a pass-through, and a chain that ends in a
processor ends nowhere.

Measured 2026-08-31 with `demand_census`, and **the two bands fail differently** — which is worth
keeping straight, because the fix is not the same:

*Ancient band.* The endpoints largely work. BL-640's era-banded basket does what property 2 asked:
ceramics, dressed stone, planks, leather, cloth and charcoal all carry real household demand. What
is broken is the **middle**. `fibre` is produced 27613.5 against demand 89.7 and sits near its price
floor, while `leather` — which fibre's sibling chain should feed — is produced **6.8** against demand
87.1 and prices at **9.97× base, ceiled in 13 of 14 markets**. The chain is not converting: raw
inputs glut, finished goods starve. Fifteen resources still have no market sink at all, and the
census separates them usefully — four *produced in-band with no sink* (ordnance, rigging, tools,
trade_goods_misc) and eleven *extractable with no sink* (coal, coffee, copper ore, iron-nickel ore,
petroleum, platinum-group metals, rare-earth ore, regolith, silica, spices, tobacco).

*Industrial band.* The endpoints barely exist. Household reaches 6 resources, construction reaches
**0**, building upkeep **0**, and the largest single source of demand is the background-industrial
**stopgap** at 3660.8 of 5268 total — 69%. Strip it and roughly **1.8% of what the world produces
has a genuine buyer**. `iron_ore` is produced 42991.6 against demand **0.000**.

So the ancient band needs its chain to convert; the industrial band needs endpoints to exist at all.

The rule is not "every resource needs its own channel". It is that **intermediates earn DERIVED
demand through the chain** — a market with people wants cloth, and should never need to want fibre
directly; fibre's demand is the cloth-maker bidding for it. That is why property 3 above is
load-bearing rather than fastidious: derived demand only propagates backwards through links that
**bid**. Sever the chain at one draw that does not bid and everything upstream of it becomes an orphan, however
carefully its recipe was authored.

This gives the admission rule its shape. A resource is legitimate in a band when a path exists from
it to a terminal sink **in that band**, and the census can assert it per band rather than per
opinion.

**5. Terminal demand is UNIVERSAL; supply is not — and the asymmetry is generation's to PRODUCE,
not to guarantee** (Ben, 2026-08-31).

Terminal demand follows **population**, and population is everywhere. So every chain should
*terminate* in every market: wherever there are people, there is a buyer for the consumer end of
every chain the band supports. That half is a rule, and it is what stops a good being an orphan in
one market and a staple in the next for no reason a player could read.

Supply is regional, because deposits are. Trade therefore arises from **supply asymmetry**, not
from demand asymmetry — which is the right way round, and the reason universal demand does not
flatten the map.

**But local self-sufficiency is NOT forbidden.** An earlier draft of this section proposed the
stronger rule that no chain should be completable within any single market. Ben overturned it the
day it was written: *"there is no reason that every start needs to be equally good and fair… we
should be making rules that encourage a level of asymmetry."* A region that can close a chain by
itself is a strong start, and a strong start is a legitimate outcome — the same way a poor one is.

So the second half is **suppositional**: self-sufficiency is expected to be possible, uncommon, and
unevenly distributed. What generation owes is the **spread**, not the floor. See
`docs/generation/GENERATION_STRATEGY.md` § Asymmetry is the deliverable.

**AMENDED (Ben, 2026-09-15): demand keeps universal PRESENCE and takes a cultural WEIGHT.** The
first half above stands — every market keeps a buyer for every terminal good its band supports.
What changes is the volume: household demand for a good is weighted by the population-weighted
preference of the cultures in the market's catchment. Trade therefore arises from **both**
asymmetries — supply, because deposits are regional, and demand, because peoples are. The
preference is derived in the history (`docs/generation/EXPLORATION.md` § A good acquires a cultural
preference) and seeded at the epoch (`docs/generation/INDUSTRIALISATION.md` § 1. A dense corporate web,
and markets that stock what their people want).

**6. Two channels are settled by the power and construction design** (Ben, 2026-08-31), and both
are worth naming here because they change what the register measures.

**Construction stops being episodic.** It becomes a *sector* with a throughput that draws its
method's goods as upkeep every tick, rather than a per-project lump that fires only while something
is building — which is why the channel currently measures 0.000 in the industrial band. Seeded
capacity gives it a non-zero reading from tick 0. `docs/economy/PRODUCTION.md` § Construction as a
rate owns it.

**Power is a BOUGHT good, and it is the Industry channel's first viable entry** (Ben, 2026-08-31:
*"it has to be a bought good when it is taken as upkeep. Therefore corporations can buy power from
each other, and background companies can produce power with a profit"*).

Every building that needs power bids for it, so **both links of the fuel chain bid**: the generator
buys fuel as a processing input, and every building buys power as upkeep. Both bid, so
neither severs the chain — property 3 satisfied twice, and property 4's derived demand propagating
through links that bid, working as designed. `coal` and `petroleum` gain their endpoint and power
gains its own.

**It is the first entry in the Industry basket the world will actually make.** That basket ships at
zero because tools and planks are *produced 0.0* in band; power is produced because producing it is
profitable, which is why turning Industry on for power is a different proposition from turning it on
for tools, and the order to do it in.

Power is also the first good whose **movement and market are separate questions**: it has a price but
no trade, and a buyer can only match a seller its road network reaches, which keeps the price
regional. `docs/economy/PRODUCTION.md` § Power and `docs/economy/LOGISTICS.md` § 3a own it.

### Settled: every draw BUYS, up to a reservation ceiling

Ben's ruling, 2026-08-26 (BL-654): *"Buy on the market, but at a threshold, buying is not allowed.
This goes hand in hand with maximum and minimum prices for goods."* And **one rule for every goods
draw** — unit upkeep takes the same shape, not a second one.

- **A draw buys on the market**, spending credits. The draw is a real participant, so the want
  lands in `demand`, the price moves, and a rival scoring the building that supplies it finally
  has a reason to. That is the half BL-641 was missing. With no corporation stock (§ The shelf
  economy), the whole need is bought, never only a shortfall.
- **Above a reservation ceiling, it does not buy.** The draw goes unmet and the shortfall rule
  applies — the building weakens, exactly as an unsupplied unit does. Going without is an outcome
  the design already knows how to express.
- **It pays the posted price, and so does every other goods draw (Ben, 2026-10-03).** The ceiling
  is read against the price standing on the shelf, and that price is what the draw is billed.
  Processor inputs, construction materials and a trade's purchase obey the same ceiling: a
  processor idles, a site pauses and a trade ships less, rather than buy above it.
  [FINANCE.md](FINANCE.md) § Standing-force upkeep owns the rule.

**The ceiling is the buyer's reservation** — **"go without rather than buy above this"**, never a
price the market is made to accept. It is what stops a starving building bidding a good to its cap
or spending itself to death chasing a shortfall it cannot fix. **The seller has no mirror of it:**
a landing is sold at the clearing price, bid or no bid (§ The shelf economy), and the glut that
floors that price is the signal a maker answers, not a price it may refuse.

The ceiling belongs to the **price band's** authored family (`floor_mult` / `ceil_mult`,
§ Price resolution) rather than to upkeep, because it is a statement about what a good is worth
paying, not about who is buying. Its value (2.0) is a first cut, set by ruling rather than
derived; `firm_attrition_trace` and `demand_census` are the instruments that move it
(`scripts/economy.lua` carries the reasoning).

Every remaining channel inherits this question and is checked against it **before** it is built:
BL-643 (infrastructure), BL-644 (state), BL-645 (research), BL-646 (conflict).

**4. Every channel is a lever on what the player chases.** Demand is not bookkeeping; it is the
design's statement about what the game is *about*. A good with no buyer is a good the player has no
reason to build toward, and a good with a *state* buyer plays differently from one with a
*household* buyer — the first is lumpy, political and worth lobbying for; the second is steady,
broad and worth scaling into. Choosing which channel wants a good is choosing what kind of
gameplay that good produces.

### What each channel adds, and what it already has

- **Household** (BL-640, era-banded household basket). The basket gains an era band and the
  stratum ladder (`POPULATION.md` § The stratum ladder, Ben 2026-09-15): volume by headcount,
  cumulative rungs up to electronics at a metropolis, upper rungs scaled by qualification. It is
  the sink for terminal artisan goods — the ancient roster's ceramics, cloth, leather, dressed
  stone — and, in the industrial band, the first genuine buyer for electronics.
- **Industry** (BL-641, building upkeep in goods). Today a building pays maintenance and wages in
  **credits only**, while a unit pays credits **and a goods vector** (`run_unit_upkeep`). Giving
  buildings the same shape turns every firm in the world into a consumer, and it is the single
  largest structural sink available: tools and planks keep an ancient workshop running, machinery
  and electronics an industrial one. The precedent, the shortfall rule and the shelf-draw ordering
  all already exist — this is the unit-upkeep vector applied to the other kind of asset.
- **Construction** (BL-642, construction actually draws). Materials are authored per building
  (`resource_costs`) and charged at the build press — but generation *places* buildings rather than
  constructing them, so the draw never fires during the opening years, and stone and timber have a
  construction sink on paper with no pull in practice. Two halves: make the opening years build,
  and make **centres draw materials as they grow**, which is the half that does not decay after the
  pre-game settle. An ancient economy's largest material sink is building.
- **Infrastructure** (BL-643, network upkeep draws materials). The `logistics_maintenance` budget
  line already exists and names exactly this. A road network that consumes stone and timber to stay
  standing is a permanent sink scaled by geography rather than by population — and it gives the
  network a running cost that makes reach a decision rather than a ratchet.
- **State** (BL-644, the space programme line). Nations already hold a weighted budget over nine
  priority lines, and one of them — `strategic_reserve`, "buying goods to hold" — is a goods-buying
  channel **already designed and not yet claimed on**. A tenth line, a *space programme*, buys
  `spacecraft_components` and `propellant` for government satellite launches: the first buyer for
  space goods that is not a militia contract, and thematically the gate into space. Lumpy,
  political, worth lobbying for — see property 3.
- **Research** (BL-645, research consumes goods). `academic_research` and `military_research` are
  budget lines that spend credits; research that also consumes **goods** makes the tech ladder an
  economic decision rather than a free accumulator, and gives the top of the chain a buyer. Folds
  into the RESEARCH.md design session (BL-619) rather than pre-empting it.
- **Conflict** (BL-646, battles burn ordnance). Ordnance is priced at the top of the ancient roster
  and its only draw is a per-head upkeep rate small enough to be nil. A battle that consumes what
  it fires makes war a demand shock — the sink that couples the game's two pillars, so that
  *Conflict* moves *Trade*.
- **Endemic trade** (BL-647, endemic luxury demand). Tobacco, spices, coffee and furs are on the
  roster, extractable, priced, and wanted by nothing at all. The natural buyer is a household one
  that scales with **wealth** rather than headcount, flavoured by national character — so different
  nations crave different luxuries and the trade route is asymmetric by construction. This is the
  most *Trade*-shaped channel of the eight: extract where it grows, sell where the money is.

  **`trade_goods_misc` joins this basket as its fifth member (Ben, 2026-09-06).** It is a *produced*
  endemic-class good — the Potter & Weaver's and the Glassworks' output — where the other four are
  extracted, and until now it was produced, priced and wanted by nothing, which under this document's
  admission rule (a consumer is a mechanism, not a noun) makes it a name rather than a resource.
  Putting it in the wealth-scaled basket gives it the one thing it lacked: a terminal sink in its
  own band.

  **What it costs, and it is worth stating rather than discovering.** The four extracted luxuries
  carry *geography* — each grows in one lat/sector and nowhere else, which is what makes the trade
  route asymmetric and the price a function of distance. `trade_goods_misc` has no endemic geography;
  it is made wherever clay and a workshop are. So its row **dilutes the directional asymmetry** that
  is the whole point of the channel — a fifth craving that every nation can satisfy locally. It is
  admitted anyway because a sink in the right band beats no sink at all, and because the dilution is
  bounded by the row's own weight. If the channel's asymmetry measurably weakens, the honest next
  step is to give the good a geography (an endemic *recipe* input, or a named luxury replacing the
  placeholder) rather than to widen the basket further.

### Measuring it

A demand model is only as good as the census that checks it, and the failure it must catch is a
good that reads plausible and has no buyer. BL-649 (demand census) is the instrument: per resource,
per era band, the total modelled demand and the passes that inject it — so a dead good is a row in
a report rather than a discovery made three sprints later. It is also what makes a *tuning* pass
possible at all, since the question "did that change help" needs a before.

**The census's vocabulary is canon** (ratified on Ben's ruling, 2026-08-26 — NR-676), because every
tuning pass will quote it and harness-local jargon three passes deep is unreadable:

| Term | Means |
|---|---|
| **Structural sink** | A good *has* an authored consumer — a recipe input, a basket weight, an upkeep draw. A property of the design. |
| **Observed demand** | What was actually bid for this tick. A property of the run. |
| **`REC` / `DEP`** | Producibility: made by a recipe in this band, or dug from a deposit. A good can be neither, which is the most interesting row in the report. |
| **`px`** | Priced on some market. **An unpriced good is invisible to both basket injectors, which skip it silently** — so this column is where a missing script shows up (BL-652). |

The distinction that does the work is the first one: **a structural sink with zero observed demand
is the signature of a dead chain**, and it is invisible to any check that looks at only one of them.
A good with no sink at all is easy to notice; a good with a sink nobody exercises is what this
economy actually had.

## Background corporations

**The market saturates because real firms produce and consume, not because a substrate pass
injects supply and demand.** Nothing in the engine injects fictional supply; the only injected
quantities are the demand pulls in step 3. The design is BL-365 (real background
corporations).

**The background economy is the landscape phase 6 selected** — not a separate injection pass bolted
on after generation. The settle that hands play its opening position is phase 6's single
validation run of the winner — twelve quarterly econ ticks, `k_campaign_settle_ticks` (`src/world/campaign_settle.hpp`), a length
measured on the per-tick convoy dispatch count rather than chosen (`../economy/ERAS.md` § The
opening position; BL-978, warm start retired, owns the work). There is no other pre-game tick
loop: the prices play opens on are the ones those twelve clearings leave. Generation scores candidate corporate landscapes
— rosters, placements and road tiers — statically against the finished world, and the winning
candidate's firms *are* the background economy: real buildings, on real tiles, with
`corporation_component.is_background = true`. Placement mechanics are
`docs/generation/CORPORATION_GENERATION.md` § Pass 6; what is placed is decided by the search
(`docs/generation/GENERATION_STRATEGY.md` § The eight phases; BL-770, Era 0 candidate search).

**So the roster is a consequence of the objective, not of a calibrated stopping rule.** The earlier
design added firms until measured production crossed a ~90% target fraction of measured demand.
That is superseded, and for a reason worth stating rather than dropping: a target fraction pushes
every market toward the same coverage, and an even map is exactly what
`docs/generation/GENERATION_STRATEGY.md` § Asymmetry is the deliverable exists to prevent. The
search's objective is **viable-but-uneven** — chain completeness, the supply-to-demand ratio, and
the *spread* of both across markets, with unevenness scored for rather than tolerated. What the
calibrated rule got right is kept: nothing is authored as a fixed count, so the roster stays
correct as recipes, deposits or population are retuned.

Background firms are not a cheaper stand-in for the player's rivals. They run the **full corp_ai
scored-utility layer** — build, demolish, survey, road, hire, and trade decisions, identical to
the handful of named rival corps (Ben, 2026-08-11, overriding a reduced-model recommendation) —
against the same `corp_command` seam and the same market this section documents. A background
firm's landings, shelf draws and trades are ordinary rows in steps 4–8 above; there is no separate
code path for them.

**Data-model hook, not a UI change.** `is_background` exists on `corporation_component` so
generation and `export_corp_blackboard` (BL-206) can distinguish "background noise" from a named
rival. Every corp, background or not, is listed individually on any surface that enumerates
corporations; aggregating the ~80 background firms into one entry on the Corporation lens and
dashboard is separate UI work.

Real market inventory (below) is a hard prerequisite of this model, not a neighbour: real
background firms selling into a market that conjured any buy-side shortfall would undercut the
entire point of modelling real producers.

## Want and fill — the demand register records the bid

**A want is a bid; a fill is a receipt.** This is the buy-side twin of the sell-side distinction
below: `supply` is an **offer** and `inventory` is a **delivery**. The design is BL-441 (demand
records the want).

Hunger is precisely the state in which no purchase completes. A demand register that accrued from
what a corp **actually bought** would be silent exactly when it mattered most: a processor that
needed 16 units of an input and could draw only 2 would tell the market it wanted 2, and one that
could draw nothing would tell it nothing at all. The resource would read to `resolve_price` as one
almost nobody wants, and scarcity would be **invisible to the price signal**. `resolve_price`'s own
branch for zero supply against real demand takes the price to the top of the band — it needs the
input.

**Two registers, one of them priced.** `economy_report` carries `wants` alongside `purchases`,
both `std::map` keyed by `(corp, market)`:

- **`wants`** — the full-run input need, computed **before** any coverage decision, so it is the
  same number whether the draw then succeeds, runs short, or fails outright. Registered by
  `run_processing` before its starved early return, and by `run_construction` **before** its
  `rate <= 0` pause check, so a build stalled for want of steel says so; upkeep and a trade's
  purchase register theirs the same way. This is the sole input to `mc.demand`.
- **`purchases`** — what was actually delivered off the shelf. Feeds the billing pass (step 8) and
  the expenditure a corp is charged. **Never pay against `wants`**: that would credit deliveries
  nobody made.

**The want is the whole need (Ben, 2026-10-10).** A corporation holds no stock of its own
(§ The shelf economy), so nothing covers any part of a processor's input but the shelf, and its
full-run need is its bid to the market. A corporation feeding its smelter from its own mine bids
for the ore like anyone else, and pushes its price like anyone else: the mine's output was sold to
the market when it landed. A need the fair-price ceiling silences does not bid; it goes to the
silenced-want register instead (§ Price resolution, *The shelf's share reads the want the ceiling
silenced*).

**Determinism.** Both registers are `std::map`, so accumulation runs over a **sorted** key set —
the same seam where an `unordered_map` float accumulation is a latent nondeterminism.

**The household channel keeps the same pair, on the market.** `household_bid` is the population
channel's want — its share of `demand`, summed over the centres clearing there. `household_fill`
is its receipt — what those households drew off the shelf at the end of the clear (§ The clearing
tick, step 9). A centre's met ratio is `fill / bid`, never `supply / demand`: a shelf standing
full beside a tick with no listings still feeds the people. Both arrays are saved with the market,
because the growth pass reads them before the next clear rewrites them.

## Real market inventory

`market_component.inventory` is **real, persistent stock** — the shelf — not reset each tick,
unlike `supply`/`demand`, which are per-tick FLOW figures for pricing and reporting. It is the
substrate outpost markets need (a market clearing against remote demand has stock in transit and
stock on hand, which a derived-from-recent-supply figure cannot represent), and it is what makes
the market a finite counterparty on the **buy** side. The design is BL-130 (real market
inventory).

**Fills from landings only.** A building's output, a trade's arriving cargo, a captured cargo and
a procurement delivery each land on a market and are sold to it (§ The clearing tick, step 7); the
opening stock generation places is the one other inflow (§ The shelf economy). Nothing else puts a
unit on a shelf.

**Credited in the statement that pays for it (BL-422, inventory conservation).** The shelf gains a
landing in the same statement that pays its owner, so **inventory gains exactly what the landing
register held**, and the two cannot drift apart in a later edit. It is a conservation law, not a
tally. The walk is over an ordered container (`std::map`, sorted market and resource keys), so the
credit is deterministic as well as correct.

**Drains before `clear_markets`, in the same tick** — against whatever stock stood on the shelf
from **prior ticks'** landings:

- **`run_construction`** (the pacing rate of BL-095, construction pacing) reads
  `market_component.inventory` directly and **drains** it as the build draws.
- **`run_processing`** computes coverage as `market inventory / need`, per input, on the
  two-threshold model — full batch at/above `t_full`, scaled between `t_idle` and `t_full`, idle
  below `t_idle` — and draws the shelf for what it runs, decrementing it directly. A short shelf is
  shared pro-rata (§ Price resolution).
- **Upkeep** — building and unit upkeep in goods, bought off the shelf at the posted price under
  the ceiling (`FINANCE.md` § Standing-force upkeep).
- **Trade** — a trade buys its cargo off its source market's shelf, and on a space leg the
  launch's propellant with it (`TRADE.md` § A trade). It is the only way stock leaves for another
  market.

Because every consumer draws from the SAME live inventory value in a fixed, deterministic order
(the tick order), and a processor's `run` fraction is bounded by the coverage-min across every
input, the total drawn from a market in one tick can never exceed what was actually on hand — no
double-spend, no negative inventory, no ordering dependence beyond the tick's own fixed pass
order.

**After the clear, three more drains.** The households take their bid off the shelf (§ The
clearing tick, step 9): the Household channel's terminal sink, consumed and never returned. The
background pull takes what it bid above one tick of processor want (step 10). Then every good left
on the shelf spoils by its authored rate (step 11; § Price resolution, *The shelf spoils*). None of
the three moves money: the market paid the maker when the goods landed. The nation's own claims —
network upkeep and the space programme — draw later in the tick from what is left, buying off the
shelf (`../politics/NATIONS.md`).

**The sell side has no volume cap and no condition.** `market_component.supply` is a derived
per-tick flow for pricing, and the market absorbs any quantity that lands, at the clearing price.
There is no floor a seller can hold behind and no stock held back from the price signal: whatever
landed is listed, sold and shelved in the one tick.

Verified by `tools/verify/market_inventory_harness.cpp`.

## Trades on the ledger — positions and history

Ben, 2026-08-29, redesigning the Market ledger: *"Sell orders are complicated, and we should
rework this into persistent trades."* What he asked to see was a list of trades and their profits,
and potential trades and theirs.

A **trade** is `TRADE.md`'s standing route — a good, a source market, a destination market, the
trade points behind it, and an owner — and `TRADE.md` owns what it is and who may set it. This
section owns the two things a market reading needs from it.

**Three reads, and they are not equally cheap. Say which is which rather than presenting them as
one table.**

1. **My trades.** Every trade the player's corporation holds, manual and auto, with its route.
   Trades are world state (`world::trades`), written by the player and by rivals through the same
   verbs, so the population this reads exists.
2. **The market's trades.** Every trade touching a market the player **operates in**, whoever owns
   it. *Operates in* is the gate, and it must be a real predicate rather than "every market": a
   player reads the trades of markets they trade at, not of the whole system.
3. **Potential trades.** Not a record at all — a **derivation**: for each good the player can
   reach, the destination's price less the source's, less the haul the route would cost
   (`LOGISTICS.md` traversal cost), times the good's trade capacity — exactly the ranking auto
   trade runs (`TRADE.md` § Auto and reserved trade).

**What a trade realised is recorded, exchange by exchange.** A trade's purchase is a shelf draw at
the source and its cargo is a landing at the destination, and both are rows of the exchange record
below. Reporting a realised profit the clearing loop never computed would be inventing a number,
and the honest-placeholder idiom (NR-249) is not licence to do it on a figure a player would act
on; the record is what makes the history real.

### The exchange record

One row per exchange, appended by the clearing tick, ring-capped the way the plot histories are:

| Field | Why |
|---|---|
| `tick` | The econ tick it cleared on. A tick is a quarter, so this is the date. |
| `market` | Which board. The surface is per-market. |
| `resource` | What moved. |
| `quantity` | How much. |
| `unit_price` | The price the exchange was made at. A landing is sold to the market at the **clearing** price of its tick; a draw off the shelf is bought at the **posted** price it was decided and billed at ([FINANCE.md](FINANCE.md) § Standing-force upkeep). |
| `seller`, `buyer` | The corporation on one side. The other side is **absent**, which means the market itself and not an unknown party (see below). |

**There are two kinds of exchange, and the market is on one side of every one.** A **landing** —
a building's output, a trade's cargo, a captured cargo, a procurement delivery — is *sold to the
market*: the owner is the seller and the buyer is empty. A **shelf draw** — a processor's input, a
site's material, an upkeep draw, a trade's purchase — is *bought from the market*: the seller is
empty and the buyer is the corporation. Both are real — goods moved, cash moved — so both are
recorded. A surface renders the empty side as the market; treating it as missing data would hide
the whole history the record exists to keep. The households', the background's and spoilage's
takes move no money and are not exchanges.

**It records REVENUE, not profit, and that limit is structural rather than an omission.** There
is **no cost basis anywhere in the model**: a unit on a shelf does not know what it cost to extract
or to carry there, so the margin on selling it cannot be derived from the sale.
`quantity * unit_price` is honest; `profit` is not available at this grain and must not be printed
as though it were.

Margin is answered where it is defined instead:

- **At the building**, where `building_profit.hpp` nets revenue against input cost, maintenance
  and wages — a *per-building* answer.
- **At the trade**, whose margin is defined by `TRADE.md` § A trade: the destination's price on
  landing, less the source's price, less the haul — each a figure the record and the shipment
  carry.

So the surface reads: **what moved, at what price, between whom** — and a column built from the
record alone must be labelled **revenue**.

**Serialisation.** The record is world state, so it is in the save envelope and
`save_game_version` moves. NR-708 records that **no envelope field has round-trip coverage** and
that `read_save_game` refuses the whole file on a version mismatch — so this is the change that
should finally bring an envelope round-trip assertion with it, rather than adding one more
untested field to a seam that has never been tested.

**Ranking is permitted here**, and this is the one surface where that has been ruled on
explicitly. `CONCEPT.md` § Player identity holds the rule and its qualification: a surface may
rank where the top row is one input among several. Ben, same day: *"Market prices is a vital
pillar of gameplay, but the strategy 'just build the most profitable' is a red herring."* A
potential trade sorted by margin is information the player must still weigh against reach, stock,
competition and what the price does next — so ordering it does not decide the game. Ordering
*tiles to build on* by margin does, and is refused.

## A market's listed value

**A market's cap is its LISTED VALUE (Ben, 2026-09-15): the summed valuation of the firms
headquartered in its catchment.** It answers *how much capital sits here*, which is the reading a
player means by a market's size. Each firm is valued by the formula a whole-firm buyout already
prices (`FINANCE.md` § Whole-firm acquisition), so the sum introduces no second valuation.

**It is a sum over firms, not a share price.** The corporation ledger's ruling that a firm has no
market cap stands; nothing here gives a corporation equity, a share count or a stake. A firm counts
toward the market whose catchment holds its headquarters, once.

**It is derived, never stored**, and it is seeded at the epoch only in the sense that the firms it
sums are (`docs/generation/INDUSTRIALISATION.md` § 5. Wealth inequality, market cap and GDP).

## Procurement is not the market

A procurement contract is a **named counterparty** with a lead time and a refusal. The market is
anonymous, instant and price-only, and it has no representation for any of those. The form itself
— its verbs, its terms, its pricing and its reputation axis — is [`CONTRACTS.md`](CONTRACTS.md).

**A contract still delivers from and to shelves.** At fulfilment the supplier buys what its home
market's shelf holds of the good, at the posted price under the ceiling, and makes the rest to
order; the delivery then lands on the buyer's home market on the delivery body and is sold there
like any landing (§ The clearing tick, step 7). The contract moves money between the two parties;
the goods meet the market at both ends.

## Tariffs — the first flow that pays a nation

> **The nation half of this lives in [`docs/politics/NATIONS.md`](../politics/NATIONS.md)** — what a
> treasury is, who may author a law, and how the treasury is spent. This section owns the
> **arrival half**: how the duty is charged when a shipment crosses a border.

A market resolves to a jurisdiction: `market_component::centre_tile` through
`world::tile_to_nation`. Goods that arrive in one jurisdiction from another have crossed a border,
and that is what a tariff reads.

**The rule, in one line (Ben, 2026-09-15):** a convoy arriving at a market whose nation differs
from its source market's nation pays the destination nation's enacted import duty on the cargo, at
the destination's price, charged to the convoy's owner and credited to that nation's treasury.

**Why the border and not the sale.** An import duty taxes goods coming in, and a trade's shipment
is the only object that carries goods across a line — so it is the only place the duty has a real
payer without inventing one. A sale has no foreign party to charge: every exchange is with the
market (§ The exchange record). Charging the shipper at arrival reaches every import, auto or
manual, and it is **the one** point of charge, so no good pays twice.

- **The base is the cargo's value at the destination** — quantity × the destination market's last
  resolved price at the arrival tick. The goods are priced where they will be sold.
- **A market with no nation charges nothing.** An off-world market emerges outside any
  jurisdiction, so a space convoy pays no duty until law reaches the sky.

- **The rate is set by law, and only by law.** `law_effect_kind::import_tariff` is ad-valorem
  (`law::rate` is a fraction of the trade's value, not a per-unit charge) and has a second party.
  A `law` carries an **`author_nation`**: the author is both the jurisdiction the duty applies in
  and the treasury it is paid into, so a tariff with a null author is inert by construction rather
  than by a special case. Rates from several enacted laws stack additively, clamped to `[0, 1]` —
  a stack of laws cannot charge a buyer more than the goods are worth.
- **The posture a nation holds at world setup is derived by its history, not set by a dial
  (Ben, 2026-09-24).** The Industrialisation span derives each nation's protection
  (`polity::protection_q`) at its close from three readings the span already carries — its
  scarcity signals, its trade flows and its cultural preference — and the handoff enacts that
  posture as the nation's one blanket `import_tariff` law, authored by the nation itself
  (`derive_national_protection` → `seed_national_tariffs`; `docs/generation/INDUSTRIALISATION.md`
  § The boundary owns the derivation, `docs/politics/NATIONS.md` § The import tariff the
  enactment). A nation whose derived protection sits below the floor enacts none, and no other
  path writes a tariff at setup.
- **`nation_component` carries a `treasury`**, zero at generation — a treasury that started full
  would be a balance change smuggled in as a field. Its spend side is the national budget
  (`docs/politics/NATIONS.md`, BL-537).
- **A same-nation haul is charged nothing.** A tariff that taxed domestic trade would be a sales
  tax wearing the wrong name.
- **The payer is the shipper, and the shipper is always real.** A landing sold to the market has
  no importer to tax; a shipment does: every convoy has an owner who paid to move it.
- **It is a transfer.** The shipper's expenditure rises by exactly what the treasury rises by, in
  the same statement. `apply_budget` charges expenditure unconditionally (a balance may go
  negative), so the two sides cannot drift apart on a solvency edge.
- **Off by default, and provably so.** The whole pass is gated on `any_import_tariff_enacted`; with
  no tariff enacted, not one line of tariff arithmetic runs — asserted by `money_conservation.cpp`
  as a state hash against a control world carrying no law record at all.

**This is a rate, not a planner.** The standing grant for nation behaviour
(`.claude/rules/io-standing-rules.md` § Determinism & data model, 2026-08-18) admits a nation
holding a treasury and setting a tariff rate by law, and excludes a nation planner. Nothing here
chooses, scores or schedules: `nation_tariff_rate` walks the authored law list and returns a
number.

## Price resolution

`resolve_price`, per (market, resource):

```
target = base_price × √(demand / supply)     — damped elasticity
         base_price                           when neither side has a signal
         base_price × ceil_mult               demand with zero supply
price  = prior + 0.5 × (target − prior)       — EMA smoothing
```

**The shelf is supply (Ben, 2026-10-03):** `supply` is this tick's listings plus the shelf's share
of the stock standing on the market's shelf (`inventory`) — at most k ticks of demand, k set by
measurement (below). A market fed only by deliveries is not a market with
nothing to sell, so at k > 0 its own buyers cannot drive its price to the ceiling against a full
shelf.
**The shelf counts only as far as it can sell (Ben, 2026-10-03):** the shelf's share of `supply` is
at most what the market's demand would take off it within k ticks, `min(inventory, k × demand)`.
Counted whole, a market that buys every landing grows a glut that
floors its own prices; measured on seeds 0/10/28 the field fell to 1/1/11 firms. **k is 0 until
the shelf spoils (Ben, 2026-10-03):** measured at k = 1 to 16, every k above 0 left fewer firms
than listings-only supply (seeds 0/10/28: 43/48/32 at 0, 16/5/21 at 1, 5/2/19 at 4), because a
glut the market cannot shed floors prices. **k is set above 0, by measurement, once the shelf
drains (Ben, 2026-10-05, NR-968):** a shelf now has two drains — households eat off it
(`POPULATION.md` § Population demand) and it spoils (below) — so a glut no longer only grows, and k
is the smallest value that keeps a consuming market's price off the ceiling against a stocked
shelf, read on the market viability gate. **By that measurement k = 0 (Ben, 2026-10-05, NR-972):**
swept at k = 0/1/2/4/8 on seeds 0/43/10 with spoilage on and households eating, spoilage alone
took the share of consuming prices at the ceiling against a stocked shelf from 5.2% to 0.1%, every
k above 0 left fewer firms alive (38% at 0, 21–23% above), and the ceiling that remains is empty
shelves no k can reach. It is re-swept once water is supplied (BL-1198, the Well).
**The shelf's share reads the want the ceiling silenced (Ben, 2026-10-07; BL-1209, shelf sees silenced want).** With the
share at `min(inventory, k × demand)`, a buyer silenced by the fair-price ceiling contributes no
demand, so a full shelf priced over the ceiling counts as no supply and stays priced over it — a
cycle that starved about 65% of the processors starved at handoff (BL-1207, handoff starvation).
So the share is `min(inventory, k × (demand + suppressed want))`, the suppressed want being the
processor inputs and construction materials that went unbought over the ceiling (the same register
auto trade reads to size a shipment, `TRADE.md` § Auto and reserved trade). The want **still never bids** — it only lets a
stocked shelf count as what it is, so the price can fall to where the silenced buyers return. The
ceiling ruling stands unchanged. **Under this rule k = 1 (Ben, 2026-10-07):** re-swept at
k = 0/1/2/4 on seeds 0/43/10/28/38, k = 1 is the smallest k that takes a consuming market's price
at the ceiling against a stocked shelf to 0% (2.5% at k = 0); it passes run-rate income (50.8%
against 47.2% at k = 0) and firm survival (83.5%, against 88.9% at k = 0), and lifts medical
supply at ticks 20-50 from 31% to 51% of the household bid.
**A short shelf is shared pro-rata (Ben, 2026-10-07; BL-1209, shelf sees silenced want).** When a shelf cannot meet every
draw admitted against it in a tick, each draw receives the same share of its need, not first-come
by building id: a dip under the ceiling no longer lets the lowest-numbered plants empty the shelf
while the rest starve. **The sharing is a floor, then the remainder.** A shelf is *contended* when
the want admitted against it (under the ceiling) exceeds what it holds as the phase opens; an
uncontended shelf is drawn as it always was. On a contended shelf:
- a draw that cannot run even at its best case — every good it needs met as far as its shelf
  holds it, so an empty co-input shelf is a share of 0 — is *hopeless* and reserves nothing;
- every other draw has the same share of its want reserved, `floor = want × shelf / total want`;
- in visit order, each draw may take its **full** need from what the shelf holds beyond the floors
  still reserved for the draws after it. A draw that takes less than its floor — short on another
  good, or running part of a batch — releases the rest down the order, so an
  equal share too small to run anybody passes on until it runs someone;
- the phase then **tops up**: a draw left short takes, in visit order, what the shelves still hold,
  sweeping again while a sweep moves.

The invariant this keeps: **no contended shelf ends a phase holding stock while an admitted draw
left short could have used it.** An uncontended shelf is drawn first-come as it always was, with no
top-up; there every draw found enough at its turn. **It is shared within each phase, not across the tick (NR-977):** the
construction phase rations its sites first, then the production phase rations its processors
from what construction left; upkeep draws and the trade pass, later in the tick, stay first-come
after both.
Pooling every draw of the tick would need the tick reordered — construction runs before labour is
solved, and a processor's need depends on its labour.

**The shelf spoils (settled 2026-10-05, on Ben's delegation; BL-1179).** Every good standing on a
market's shelf loses a fixed share of itself each tick, its **spoilage rate**, authored per good in
data by class: **perishable** (food rations, agricultural produce, clean water, medical supplies)
fastest, **consumable** (consumer goods, fuels, chemicals, power-adjacent stocks) slower,
**durable** (ores, metals, alloys, materials, components, and raw water — stored water does not rot
like food, NR-972) slowest but never zero. The rates are
first cuts, then measured. Three rules hold whatever the numbers:

- **Only the shelf spoils.** Cargo in transit does not: it is a trader's, and answered for. The
  shelf is the market's, bought from every maker on landing, and nobody tends it.
- **Nobody is charged.** The market already paid the maker when the stock landed, so a spoiled
  unit simply leaves — goods leave, no credits move, exactly as when a household eats it.
- **Spoilage is a drain, not a price.** It takes stock off the shelf after the tick's draws and
  before the shelf's share of supply is read; it never sets a price by itself.

The k cap stays: spoilage is what makes k > 0 safe, not a replacement for it.

Target and result are clamped to the band **[0.25×, 10×] of base**. Prices are therefore
*anchored*: no scarcity can push a good past 10× its authored base, and no glut below a quarter
of it. Untradeable resources (`base_price ≤ 0`) keep their prior price. A resource pegged at the
ceiling is a generation-calibration signal (background production absent or under-target — see
`docs/generation/CORPORATION_GENERATION.md` § Pass 6), not a legitimate "lucrative fillable gap".

> **Authoring a `market` condition: it measures the WORLD, not a market (NR-114).**
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

### Where the band lives

The band is **authored once, in data**: `scripts/economy.lua` → `economy.price_band`
(`floor_mult` / `ceil_mult`), reaching C++ as `price_band_params` through
`recipe_registry::price_band()` — the same route every other economy tunable takes (BL-442,
price band in data).

It is read by **two** call sites, and that is the point of naming it here:

| Site | Function | What it clamps |
|---|---|---|
| `src/world/market_clearing.cpp` | `resolve_price` | Every market price, every tick — the real clearing band. |
| `src/world/economy_system.cpp` | `wf_target_price` | The workforce auto-solver's **forward** price estimate (BL-181), so it prices a candidate target against the band the market will actually clear it in. |

Two hand-synchronised copies would leave the solver optimising against a band the market does
not use — a silent divergence with no error and no visible symptom.
`tools/verify/price_band_harness.cpp` is the guard. Its rows are **differential** — each sets a
non-default band and asserts the site *moves* — because a guard that only exercised one site
would pass again the day someone reintroduces a local copy.

### Where the ceiling comes from

`ceil_mult` is **derived, not authored**. Ben's requirement (2026-08-17) is that scarcity must
price high enough to **cross the margin for a nearby market**, so inter-market trading is a
day-1 fact rather than a late-game unlock. That makes the ceiling a function of the haulage the
supply layer charges:

```
ceil_mult × base_price  >  base_price + haulage_per_unit
ceil_mult               >  1 + haulage_per_unit / base_price
```

**The haulage is measured, not assumed.** `tools/verify/haulage_measure.cpp` walks every market
in the real generated world, finds its nearest market neighbour by the terrain-weighted A* cost,
and reports `logistics_cost(mode) × path.cost` — *exactly* the per-unit haul a trade's shipment
pays (`supply_system.cpp`). Over 5 seeds, 39 markets on 5 multi-market bodies:

| Market → nearest market neighbour | credits per unit |
|---|---|
| p10 | 0.08 |
| median | 0.76 |
| p90 | 5.65 |
| max | 7.84 |

The denominator is the **cheapest good carrying a base price: 1.00** (regolith, sitting at the
stone/sand bulk floor — `RESOURCES.md` § What trades). The binding case is therefore the worst
haul against the cheapest good:

```
ceil > 1 + 7.84 / 1.00 = 8.84   ->   10.0
```

**Why a smaller ceiling is not enough.** A ceiling of 4.0 clears the *median* neighbour pair
(which needs 1.76) but not the p90 (6.65); the tail — the worst-connected market pair carrying
the cheapest good — is permanently unservable at any scarcity. 10.0 covers **every**
nearest-neighbour pair measured, for **every** priced good, with headroom below it.

**A second, independent reading agrees on the shape.** The requirement's other half is that a
scarce cheap good must be able to outprice an abundant dear one. Read *within a tier* — which is
the only coherent reading, since RESOURCES.md promises margin widens *between* tiers — the
ordinary raw tier spans 1.00 to 6.00 (`rare_earth_ore`), demanding `ceil > 6`, inside the
haulage bound. Read *across* tiers it would demand 280 (1.00 against `spacecraft_components`
at 280), which would delete the tier model; that reading is rejected and recorded (NR-291).

**The floor is 0.25×.** The requirement derives a ceiling and says nothing about a floor; lowering
it would widen the arbitrage margin only by cutting what an abundant producer receives (NR-290).

**The nearest-neighbour reading is no longer the whole requirement (Ben, 2026-09-15).** Trade
chases price over distance (`TRADE.md` § Auto and reserved trade), and a sea leg is the cheapest
per distance (`SUPPLY.md` § Logistical cost), so the haul that matters is not only to the nearest
neighbour but to wherever a gap is. The ceiling still covers every nearest pair; beside it sits a
reading of the far pairs a seller actually reaches, taken after play has run, since a gap play
erases in a year was never a working international market.

**The far trade reading** (BL-1006, far trade reading) is `haulage_measure --far-trade`. It follows
every convoy from dispatch to its first clear after arrival, on the world the app builds, in two
windows of four quarterly dispatches — the first year of play, and the year after N years — and
reads three things:

- **(a)** volume delivered **and sold** at its destination, where that is not the source market.
  A dispatch count is not this figure.
- **(b)** the share of (a) whose destination is not the source's **nearest** market — the market
  other than the source the trade's own leg pricing reaches cheapest, so *nearest* means nearest
  reachable.
- **(c)** per good, the destination-minus-source price gap at dispatch and again at arrival, beside
  the haul paid per unit, so a gap that play closes shows as closing.

A sale is read from the exchange record: a cargo is sold on landing, so its sale is its own landing
row. Sea legs and auto trade's ranking are each judged against this reading.

**Re-derive rather than trust.** Re-run `haulage_measure` whenever the logistics cost table
(`logistics.base_cost_per_unit_distance`), the map scale (`body_km_per_tile`), or the
`base_price` table changes — all three move the number this ceiling is computed from.

**The band does not create inter-market trade; a trade does** (`TRADE.md`). A dispatch count is
not trade: the figure that means what it says is delivered volume sold at the destination.
**Inter-body** trade is gated separately: a trade between bodies needs the trader's Launchpad on
the source body and the launch's propellant on the source shelf before any price is consulted.
`trade_routes` (BL-088, persistent trade routes) is a body-level record, so it is structurally
blind to intra-body trade (NR-289).

## Inter-body linkage

There is no abstract price coupling between bodies — **trade is the coupling** (`TRADE.md`), and
its shipment rides the space lane as a convoy (`docs/economy/SUPPLY.md`). Distances and haul prices
read **tick-pure orbital angles** (`orbital_angle_at_tick`, BL-354), so a shipment is identical at
any frame rate; the smoothly-advancing render angles are display-only. An arriving cargo **lands**
on the destination market and is sold at that tick's clear (§ The clearing tick, steps 4 and 7).
Arrivals are credited at the head of the tick, before production and before `clear_markets`, so
the landing is this tick's supply and every reader of the market — the price law and the AI scorer
alike — sees the same goods (BL-382, dead market writes, is why it is never a direct `supply`
write). **A body with no market makes nothing:** a producer with no market under it has no shelf
to land on (`PRODUCTION.md` § Output and the shelf).

## Adjacent design

- **Population demand's undersupply effects.** `inject_population_demand` pulls real,
  price-elastic, multi-resource demand into the market every tick, and the signal moves prices.
  What a shortfall against that demand does to habitability, workforce efficiency and growth is
  the habitability feedback model in `docs/economy/POPULATION.md`; RESOURCES.md § Habitability
  goods names the intended effect per good.
- **Trade.** How goods move between markets — the Planetary Marketplace, trade points, capacity,
  auto and reserved trade — is `TRADE.md`.
