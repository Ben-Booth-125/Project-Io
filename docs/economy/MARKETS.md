# Project Io — Markets

The market model: `src/world/market_clearing.cpp`, the market/order components in
`src/world/components.hpp`, and the seeding in `src/world/hard_coded_world.cpp`. Production's side
of the exchange is `docs/economy/PRODUCTION.md` § Stockpile and output flow; which resources trade
at all is `docs/economy/RESOURCES.md` § What trades. Contracts — the alternative to the market,
priced and paced between named parties — are `docs/economy/CONTRACTS.md`.

A market is **anonymous, instant and price-only**. Every seller meets the market, not a buyer; a
trade clears in the tick it is listed; and the only term is the price. Everything else about an
exchange — a counterparty, a lead time, a refusal — is a contract, not a market.

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
fallback market is seeded. On seed 0 the home body carries nine carved markets, and *no single one
of them stands for the body*.

**Catchment routing:** a tile clears against the market whose `centre_tile` is nearest
(`market_for_tile`); a corp's body-aggregate clearing routes via its lowest-id building's tile
(`market_for_corp_on_body`).

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
seeding is separate. A market **never disappears** — nothing in the engine removes an entry from
`world::markets`; an outpost whose last building is decommissioned goes dormant (clears nothing,
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
*price*; it moves no goods. Physical movement is `dispatch_convoys`' independent job.

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
rejected because those writes are themselves demand-sensitive (auto-surplus yields to standing
orders), which would turn a pass whose ordering is already load-bearing into a two-pass
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

`clear_markets` runs once per economy tick, after production (`run_economy_step`). In order:

1. **Reset** — every market's `supply` and `demand` arrays zero. Both are per-tick flows. A
   snapshot of every market's supply is taken *immediately before* the zeroing
   (`snapshot_market_supply`), because the inter-body pull needs a supply that this pass has not
   yet computed — see § Spontaneous market emergence.
2. **Background-firm production** — real corporations (`corporation_component.is_background =
   true`), generated at world-gen and running the same corp_ai scored-utility layer as rivals,
   produce, sell, and buy through the ORDINARY steps of this tick (auto-surplus, standing orders,
   auto-demand — steps 4, 5, 6 below) exactly like any other corp. There is no separate injection
   step for background supply: saturation and the live opportunity margin are **emergent** from
   real generated firms, not asserted by a function. See § Background corporations below.
3. **Demand injection** — two pure demand-side pulls, both after the reset so they are not
   erased the tick they land:
   - `inject_population_demand` — each population centre pulls a price-elastic, multi-resource
     DEMAND from its catchment market (food rations, agricultural produce, water, clean water,
     consumer goods, medical supplies): `pcc.scale × demand_scale × basket[r] ×
     elasticity(price)`. Population is a pure **consumer** — no supply term. Tunables in
     `scripts/economy.lua` § `population_demand`.
   - `inject_background_demand` — the offstage economy's own pull on the mid-chain processing
     goods (silicon, refined copper, REE alloy, machinery, alloys, electronics — **not**
     `spacecraft_components`, which stays procurement-only so the militia's contracts remain its
     only buyer), world-scale rather than per-centre, because real background firms alone would
     under-consume these before enough of them exist. Per-body population scale is gathered in a
     `std::map` so accumulation order is deterministic. Tunables in `scripts/economy.lua` §
     `background_demand`.
4. **Auto-surplus** — each `(corp, body)` pool lists everything above its **processor
   reservation** (the inputs its own processors need for a full run next tick) for sale. A
   resource under a standing sell order is exempted — the manual order governs.
5. **Standing sell orders** — read from `world::sell_orders` (the book is world state, placed by
   the player and by rival corps through the same `place_sell_order` verb), quantity capped by the
   pool, entered into both market supply and the explicit sell book with their `floor_price`.
   Multiple orders against one `(corp, body, resource)` share a **running remainder**: total
   listed quantity never exceeds the pool, each order's matched/auto-cleared quantity is tracked
   per order, and pool debits clamp at zero.
6. **Auto-demand** — two registers, read separately (§ Want and fill below). `report.wants` —
   what processors and construction sites set out to buy this tick, whether or not they got it —
   enters **market demand**, and is the only thing that does. `report.purchases` — what was
   actually drawn — enters the **billing** pass instead. That billing is not a fresh grant: the
   real transaction already happened, capped by real inventory, in the PRIOR phase of the same
   tick (production and construction both run before `clear_markets`; see § Real market
   inventory).
7. **Standing buy orders** — read from `world::buy_orders`, entered into demand and the
   explicit buy book (`max_price`, optional `preferred_seller`).
8. **Reference prices** — computed once from the accumulated supply/demand (below), so every
   flow this tick uses the same price.
9. **Auto clearing** — auto-surplus sells at the reference price (**perfect counterparty**: the
   sell side is unconditional — see § Real market inventory); auto-demand billed at the
   reference price for whatever was already drawn in step 6.
10. **Order-book matching** (BL-037, preferential purchasing) — explicit sells vs explicit buys
    by price-time priority: cheapest ask first, highest bid first, corp id as the deterministic
    tiebreak. A buyer's `preferred_seller` is served first, tolerated up to **1.10×** the cheapest
    compatible ask. Trades clear at the **seller's ask**.
11. **Buyer of last resort** — unmatched standing-order quantity auto-clears at the
    **reference price**, and only if the order's floor allows it: an order whose `floor_price`
    exceeds the reference price clears **nothing** and its stock stays in the pool. The floor is
    a reservation price — "hold rather than sell below this" — never a price the market is made
    to pay. A rule that let a seller name the price a perfect counterparty pays would be
    unbounded income, not a simplification of a market but the absence of one (BL-386, floor is a
    reservation price). The auto-surplus path (step 9) clears at the market's own resolved price,
    which is the defensible prototype simplification.
12. **Price update** — where explicit trades occurred, the price eases toward their VWAP;
    otherwise it takes the reference price.

Cash flows accrue per corp and are applied to balances by `apply_budget`
(`src/world/budget_system.cpp`).

## Demand channels — where a want comes from

A price is resolved against demand, and demand is never ambient. **Every unit of it is injected by
a named pass**, and this section is the register of those passes. It exists because the roster grew
a supply side faster than a demand side and the gap was invisible from inside: a resource can
satisfy the admission rule by naming a consumer nobody ever built.

**The rule, and it is the whole section in one line: a consumer is a MECHANISM, not a noun.**
"Sold to the market" is not a consumer. "Mercantile demand" is not a consumer. A good is wanted
when some pass adds to a market's `demand` for it, or draws it from a pool — and where no pass
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
**pool draw never reaches a market's `demand`**: wanting tools never raises their price, so no
rival ever scores a Toolmaker and the supply is never induced.

So a channel has to do one of two things — bid on the market so its want becomes a price signal,
or draw from a pool for a good the world already makes. `run_construction` and `run_processing`
bid; `run_unit_upkeep` does not, and it has the same latent defect. **A sink that cannot call forth
its own supply is a slow way to shut the economy down**, and the cost of learning that is one
harness run rather than a shipped world nobody can play.

### Settled: a short pool BUYS, up to a reservation ceiling

Ben's ruling, 2026-08-26 (BL-654): *"Buy on the market, but at a threshold, buying is not allowed.
This goes hand in hand with maximum and minimum prices for goods."* And **one rule for every goods
draw** — unit upkeep takes the same shape, not a second one.

- **Short pool → buy the shortfall on the market**, spending credits. The draw becomes a real
  participant, so the want lands in `demand`, the price moves, and a rival scoring the building
  that supplies it finally has a reason to. That is the half BL-641 was missing.
- **Above a reservation ceiling, it does not buy.** The draw goes unmet and the shortfall rule
  applies — the building weakens, exactly as an unsupplied unit does. Going without is an outcome
  the design already knows how to express.

**This is the exact mirror of a rule the market already has.** Step 11's `floor_price` is a
seller's reservation — *"hold rather than sell below this"*, never a price the market is made to
pay (BL-386). The ceiling is the buyer's — **"go without rather than buy above this"**, never a
price the market is made to accept. Both sides may now decline a trade, and neither may dictate
one, which is what stops a starving building bidding a good to its cap or spending itself to death
chasing a shortfall it cannot fix.

The ceiling belongs to the **price band's** authored family (`floor_mult` / `ceil_mult`,
§ Price resolution) rather than to upkeep, because it is a statement about what a good is worth
paying, not about who is buying. Its value is measured, not guessed.

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
  stratum ladder POPULATION.md § Population demand already calls for and leaves unquantified. It is
  the sink for terminal artisan goods — the ancient roster's ceramics, cloth, leather, dressed
  stone — which is what those goods were authored to be.
- **Industry** (BL-641, building upkeep in goods). Today a building pays maintenance and wages in
  **credits only**, while a unit pays credits **and a goods vector** (`run_unit_upkeep`). Giving
  buildings the same shape turns every firm in the world into a consumer, and it is the single
  largest structural sink available: tools and planks keep an ancient workshop running, machinery
  and electronics an industrial one. The precedent, the shortfall rule and the pool-draw ordering
  all already exist — this is the unit-upkeep vector applied to the other kind of asset.
- **Construction** (BL-642, construction actually draws). Materials are authored per building
  (`resource_costs`) and charged at the build press — but generation *places* buildings rather than
  constructing them, so the draw never fires during the opening years, and stone and timber have a
  construction sink on paper with no pull in practice. Two halves: make the opening years build,
  and make **centres draw materials as they grow**, which is the half that does not decay after the
  warm start. An ancient economy's largest material sink is building.
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
| **Structural sink** | A good *has* an authored consumer — a recipe input, a basket weight, a pool draw. A property of the design. |
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
quantities are the two pure demand pulls in step 3. The design is BL-365 (real background
corporations).

World-gen runs a **second, later corporation-generation pass**
(`docs/generation/CORPORATION_GENERATION.md` § Pass 6) that places real background firms — real
buildings, on real tiles, with `corporation_component.is_background = true` — until the body's
real production meets a ~90% target fraction of real demand. The count is **calibrated**, not
authored: generation keeps adding firms/holdings until the measured production/demand ratio
crosses the target, so the figure stays correct as recipes, deposits, or population are retuned.

Background firms are not a cheaper stand-in for the player's rivals. They run the **full corp_ai
scored-utility layer** — build, demolish, survey, road, hire, and trade decisions, identical to
the handful of named rival corps (Ben, 2026-08-11, overriding a reduced-model recommendation) —
against the same `corp_command` seam and the same market this section documents. A background
firm's sell orders, auto-surplus, and auto-demand are ordinary rows in steps 4–7 above; there is
no separate code path for them.

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
both `std::map` keyed by `(corp, body)`:

- **`wants`** — the full-run input need, computed **before** any coverage decision, so it is the
  same number whether the draw then succeeds, runs short, or fails outright. Registered by
  `run_processing` before its starved early return, and by `run_construction` **before** its
  `rate <= 0` pause check, so a build stalled for want of steel says so. This is the sole input
  to `mc.demand`.
- **`purchases`** — what was actually delivered. Feeds `auto_buys`, the VWAP accumulator, and
  the expenditure a corp is charged. **Never pay against `wants`**: that would credit deliveries
  nobody made.

**The want is net of the corp's own pool** — the full-run need less what it already holds — because
`mc.demand` is compared against `mc.supply`, an *offer to the market*, so demand must be the *bid to
the market* for the ratio to mean anything. A corp feeding its smelter from its own mine is not
bidding for the input and must not push its price. (Delegated call: NR-281.)

**Determinism.** Both registers are `std::map`, so accumulation runs over a **sorted** key set —
the same seam where an `unordered_map` float accumulation is a latent nondeterminism.

## Real market inventory

`market_component.inventory` is **real, persistent stock** — not reset each tick, unlike
`supply`/`demand`, which are per-tick FLOW figures for pricing and reporting. It is the substrate
outpost markets need (a market clearing against remote demand has stock in transit and stock on
hand, which a derived-from-recent-supply figure cannot represent), and it is what makes the
market a finite counterparty on the **buy** side. The design is BL-130 (real market inventory).

**Fills from real corp sales only** — auto-surplus and standing sell orders, tallied separately
from `mc.supply[r]`. Every seller filling `mc.inventory` is a real corp, background or not.

**Credited where stock leaves a pool, not where it is listed (BL-422, inventory conservation).**
An order whose floor exceeds the resolved price **holds**, and its stock never leaves the seller;
crediting it at listing time would put goods on the shelf that no seller had parted with — and
`inventory` is not a display figure, so a processor would buy that phantom stock, decrement it,
and no counterparty would be paid. The credit sits in the same statement as each of the three
pool debits — auto-surplus clearing, matched explicit trades, and the auto-clear pass — so
**inventory gains exactly what pools lose**, and the two cannot drift apart in a later edit. It
is a conservation law, not a tally. All three sites iterate ordered containers (`std::map`
pools; sorted market and resource keys), so the credit is deterministic as well as correct.

**Drains during production and construction — both of which run BEFORE `clear_markets` in the
same tick** — against whatever stock survived from **prior ticks'** sales:

- **`run_processing`** computes coverage as `(pool + market inventory) / need`, per input,
  exactly as the no-market two-threshold model does — full batch at/above `t_full`, scaled
  between `t_idle` and `t_full`, idle below `t_idle`. A market's stock is real and finite, so it
  earns its place in the same coverage calculation rather than bypassing it. Draws pool-first,
  then the market's real inventory for the remainder, decrementing it directly.
- **`run_construction`** (the pacing rate of BL-095, construction pacing) reads
  `market_component.inventory` directly and **drains** it as the build draws.

Because both consumers draw from the SAME live inventory value in a fixed, deterministic order
(construction first, then production — the tick order), and a processor's `run` fraction is
bounded by the coverage-min across every input, the total drawn from a market in one tick can
never exceed what was actually on hand — no double-spend, no negative inventory, no ordering
dependence beyond the tick's own fixed pass order.

**The sell side has no volume cap.** `market_component.supply` is a derived per-tick flow for
pricing, and the market absorbs any quantity a seller is willing to release at the resolved price.
What is conditional is the *price*: an order whose floor exceeds the resolved price holds rather
than selling, so a listed quantity is a ceiling on what may clear, not a guarantee that it will.

**Held stock stays visible to the price signal — deliberately, and for a mechanical reason.**
Hiding held quantity from `mc.supply[r]` on the argument that stock explicitly not for sale below
its floor is not supply is not implementable as stated: whether an order holds is decided by
comparing its floor to the *resolved* price, and the resolved price is computed **from** `supply`.
Removing held stock raises the price, which can un-hold the order that was removed — a fixed
point, reached only by iterating, at a cost in complexity and determinism risk that a pricing
nicety does not repay. So the two arrays are deliberately asymmetric: `supply` is the **offer**
(listed stock is a real offer at a price, and the market may price against knowing it exists),
`inventory` is the **delivery** (only what actually changed hands). Revisit only if a real defect
is measured, not on the argument alone.

Verified by `tools/verify/market_inventory_harness.cpp` and `order_book_harness` § R7 (the
conservation rows).

## Where the order book lives

`world::sell_orders` and `world::buy_orders` — **world state**, not UI state (BL-293, order book
as world state). Ben's ruling, 2026-08-07: *"Order book needs to be a background process, the AI
must be able to trade as a player does."*

A book held on `ui_state` and handed to `clear_markets` as an argument would cost three things at
once: clearing would be something the **UI drove** rather than something the simulation does, so
a headless tick would sell nothing standing; a `corp_command` mutates `world&`, so there would be
**nothing for a trade verb to mutate** and no text-driven player could trade; and nothing on the
serialisation seam would reference an order, so a player's standing orders would not **survive a
save**. One misplacement, three symptoms.

What follows from the book being world state:

- **One implementation of what a press means.** `place_sell_order` / `remove_sell_order` are
  `corp_verb`s (`corp_command.hpp`). The Market Ledger's buttons queue commands that
  `app::render` applies through `apply_corp_command` — the same call the rival-corp scorer
  makes. A player and an AI cannot diverge, because there is nothing to diverge.
- **Rival corps trade.** The scored-utility layer reaches the verb like any other
  (`io-standing-rules.md` § rival-corp exception). Its rule is deliberately conservative —
  surplus past a hold threshold, floored at the rarity price — and lives in `corp_ai_params`.
- **Orders carry a stable `id`.** Removal names the id, never an index, so withdrawing one order
  cannot renumber another.
- **Insertion order is semantic.** Matching is price-**time** priority, so the book's sequence is
  state: it is never re-sorted, it is serialised in place, and `world::state_hash` folds it as
  stored rather than sorted. `order_book_harness` asserts that two books holding the same orders
  in a different sequence hash differently — the tripwire for a determinism leak.
- **Persistence.** `order_book.{hpp,cpp}`, a flat-binary stream: magic `IOOB` + version, and a
  bad stream is refused rather than reinterpreted.

**The buy side is engine-only.** `clear_markets` implements it and the harnesses exercise it, but
no press and no `corp_verb` submits a buy order, so preferred-seller routing is dormant in play:
the book is one-sided, and the AI can release stock but cannot bid for it. The intended emitter is
BL-160 (auto-exchange policy), whose buy band is the player buy surface and whose
`derive_exchange_orders` is the first live writer — there is no interim manual buy tab (decided
2026-07-31). Whether the dormant side should instead be removed is BL-383 (remove dormant buy
side).

## Procurement — a layer over the market, not a second market

> **[`CONTRACTS.md`](CONTRACTS.md) is the authority for contracting** — both the buy side
> (procurement, BL-350) and the sell side (the mercenary contract, BL-377). This section is the
> **market-facing** account: how procurement sits against the market rather than replacing it. The
> counterparty model, the terminal states, the reputation axis and the whole sell side live there.

A procurement contract is **a build order placed with someone else**: the commit-on-affordability,
draw-materials-per-tick, pay-across-the-build shape of construction pacing, with the materials
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
  (`world::corp_embargo_conditions`, a `condition_set` per supplier — the generic predicate
  machinery of BL-342 reaching procurement for free), reputation floor
  (`world::procurement_reputation`, a **view** of the relational substrate — see below) — and
  returns a distinguishable `corp_command_result` for each.

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
- **Split payment.** A deposit (`economy.procurement.deposit_fraction`, 0.25) debits at
  `accept_quote`; the remainder is drawn evenly across the quote's `lead_time_ticks`
  (`economy_system.cpp`'s contract-pacing pass, right after the capability-points pass). The pace
  is fixed rather than market-gated (stretch/pause on the supplier's live throughput) — a known
  simplification against construction pacing's own model.
- **Lead time is derived, not authored**: `base_lead_ticks × ceil(quantity / supplier_throughput)`
  — a bigger order takes longer, a capable supplier is faster, and the quote is incidentally an
  intelligence channel (legitimate under BL-068, competitor visibility: the supplier volunteers
  its own throughput in the price it quotes).
- **Reputation moves only on completion (+) or cancellation (−)** — narrow by design: it shifts
  price/tie-breaking, never gates access beyond the decline floor above.
- **Reputation is NOT procurement's store.** It is a **view** of `world::sentiment` — the
  relational substrate, `src/world/sentiment.{hpp,cpp}` — on its **Trust** dimension at (buyer,
  supplier) grain (BL-546, reputation as a sentiment view). The two moves above are one
  occurrence each of authored conduct (`contract_completed`, `contract_cancelled`) folded into
  that table at weights seeded from `economy.procurement.reputation_on_*`. Two consequences the
  market side cares about: the floor is **not permanent** (the row decays toward neutral, so a
  refusal is a condition of today rather than a verdict), and there is **no second table** the
  axis can disagree with. [`../politics/RELATIONS.md`](../politics/RELATIONS.md) § 2 is the
  authority.
- **Persistence.** `procurement.{hpp,cpp}`, magic `IOPC` + version 3: `world::procurement_quotes`
  and `procurement_contracts` round-trip in stored order, and no relational value crosses this
  stream at all. The substrate carries its own leg of the seam (`IOSN`, `write_sentiment` /
  `read_sentiment`), and the whole-world snapshot (`IOSV`, BL-536) carries the per-pair record.
  A bad stream is refused rather than reinterpreted, and strict version equality is what keeps an
  older stream's trailing block from being misread as quote records. Every stream carries the
  header BL-107 (save-format header) specifies.
- **The militia's own demand.** `spacecraft_components` carries no background demand — a
  militia's contracts are its only buyer, which is what makes the coupling between the
  processing roster and procurement real rather than thematic.

### What a contract is actually worth

The seam is only half the deal; the other half is the terms, and three of them are load-bearing
(BL-392, contract terms).

1. **Goods land on the BUYER's body.** A contract carries a **`delivery_body`** — the buyer's own
   body, taken as the body of the lowest-id building they own (lowest id, not first-in-`assets`,
   because a demolish permutes that list and the quote must be reproducible). It degrades to the
   supplier's fulfilment body only when the buyer owns nothing anywhere. Delivering to the
   supplier's body instead would land goods on a body where the buyer holds no processor
   reservation, and the auto-surplus path would liquidate the whole delivery the tick it arrived.
2. **A commitment buys a discount.** The quote is spot less a **volume discount**, asymptotic in
   the order size — `volume_discount_max × q / (q + volume_discount_half_quantity)`, authored in
   `scripts/economy.lua` under `economy.procurement` — so no order however large drives the price
   to zero, and the terms improve monotonically with the size of the commitment. A quote at the
   live spot price would settle at break-even minus friction, strictly worse than buying on the
   market.
3. **Lead time reads the SUPPLIER.** The throughput divisor is that supplier's real per-tick
   output of that good — extraction sites targeting it at their own rate, processing facilities
   at their recipe's yield of it times theirs, summed in ascending building id (a float sum needs
   a fixed order). Floored at 1 tick: a contract completing in zero ticks is a spot purchase
   wearing a contract's name.

**Freight is the price of the distance.** Delivering across bodies costs
`offbody_freight_fraction` of the order's pre-discount goods value, carried on the contract as
`freight_cost` and included in the total the deposit and the instalments are computed from. It is
set **below** `volume_discount_max` on purpose, so a genuine volume order still beats spot after
carriage; a same-body delivery pays nothing.

**Every credit this seam moves is a TRANSFER.** The supplier is credited exactly what the buyer is
debited, in the same statement, deposit, instalments and freight alike — the supplier arranges the
carriage, so paying them for it keeps the flow closed. On completion the goods are **drawn from
the supplier's pool** at the fulfilment body as far as their stock goes, with any shortfall built
to order (which is what a build order placed with someone else means).

Verified by `tools/verify/money_conservation.cpp`.

## Tariffs — the first flow that pays a nation

> **The nation half of this lives in [`docs/politics/NATIONS.md`](../politics/NATIONS.md)** — what a
> treasury is, who may author a law, and how the treasury is spent. This section owns the
> **clearing-tick half**: how the duty is charged when a trade matches.

A market resolves to a jurisdiction: `market_component::centre_tile` through
`world::tile_to_nation`. A sale whose buyer is domiciled outside that jurisdiction is a
**cross-border** sale, and that is what a tariff reads.

**The rule, in one line:** a matched trade whose buyer is domiciled outside the market's own
nation pays that nation's enacted import duty, and the duty is credited to that nation's treasury.

- **The rate is set by law, and only by law.** `law_effect_kind::import_tariff` is ad-valorem
  (`law::rate` is a fraction of the trade's value, not a per-unit charge) and has a second party.
  A `law` carries an **`author_nation`**: the author is both the jurisdiction the duty applies in
  and the treasury it is paid into, so a tariff with a null author is inert by construction rather
  than by a special case. Rates from several enacted laws stack additively, clamped to `[0, 1]` —
  a stack of laws cannot charge a buyer more than the goods are worth.
- **`nation_component` carries a `treasury`**, zero at generation — a treasury that started full
  would be a balance change smuggled in as a field. Its spend side is the national budget
  (`docs/politics/NATIONS.md`, BL-537).
- **A same-nation sale is charged nothing.** A tariff that taxed domestic trade would be a sales
  tax wearing the wrong name.
- **Only matched explicit trades are charged**, and that is a principled limit rather than an
  oversight: a matched trade is the only clearing path with a real counterparty on both sides. The
  auto-surplus and buyer-of-last-resort paths trade against the market itself, and taxing an
  import from nobody would invent the second party the flow does not have.
- **It is a transfer.** The buyer's expenditure rises by exactly what the treasury rises by, in
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

Target and result are clamped to the band **[0.25×, 10×] of base**. Prices are therefore
*anchored*: no scarcity can push a good past 10× its authored base, and no glut below a quarter
of it. Untradeable resources (`base_price ≤ 0`) keep their prior price. A resource pegged at the
ceiling is a generation-calibration signal (background production absent or under-target — see
`docs/generation/CORPORATION_GENERATION.md` § Pass 6), not a legitimate "lucrative fillable gap".

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

**Why a smaller ceiling is not enough.** A ceiling of 4.0 clears the *median* neighbour pair
(which needs 2.16) and only just clears the p90 (3.79); the tail — the worst-connected market pair
carrying the cheapest good — is permanently unservable at any scarcity. 10.0 covers **every**
nearest-neighbour pair measured, for **every** priced good.

**A second, independent reading agrees.** The requirement's other half is that a scarce cheap
good must be able to outprice an abundant dear one. Read *within a tier* — which is the only
coherent reading, since RESOURCES.md promises margin widens *between* tiers — the ordinary raw
tier spans 0.60 to 6.00 (`rare_earth_ore`), demanding `ceil > 10`. The two derivations land on
the same number, which is the reason to trust it. Read *across* tiers it would demand 233
(0.60 against `spacecraft_components` at 140), which would delete the tier model; that reading
is rejected and recorded (NR-291).

**The floor is 0.25×.** The requirement derives a ceiling and says nothing about a floor; lowering
it would widen the arbitrage margin only by cutting what an abundant producer receives (NR-290).

**Re-derive rather than trust.** Re-run `haulage_measure` whenever the logistics cost table
(`logistics.base_cost_per_unit_distance`), the map scale (`body_km_per_tile`), or the
`base_price` table changes — all three move the number this ceiling is computed from.

**The band does not create inter-market trade; the convoy does.** Measured over five seeds, 1,677
of 2,146 dispatched convoys are intra-body market-to-market hauls. **Inter-body** trade is gated
separately: the space lane is refused by the launchpad and propellant gates in `dispatch_convoys`
before any price is consulted. `trade_routes` (BL-088, persistent trade routes) is a body-level
record, so it is structurally blind to intra-body trade (NR-289).

## Inter-body linkage

There is no abstract price coupling between bodies — **the convoy is the coupling**
(`docs/economy/SUPPLY.md`). `dispatch_convoys` reads last tick's cleared shortfalls
(demand − supply per market) and hauls from the cheapest reachable corp pool — distances and
haul prices read **tick-pure orbital angles** (`orbital_angle_at_tick`, BL-354), so dispatch is
identical at any frame rate; the smoothly-advancing render angles are display-only. An arriving
convoy credits the destination pool **only** — the cargo reaches the market's supply through the
ordinary auto-surplus path off that pool at the next clear. A direct supply write on arrival would
be dead: `credit_arrived_convoys` runs *after* `clear_markets` in the tick, so the write would be
zeroed before pricing read it, while the pre-clearing AI scorer *would* read it — a private signal
nothing priced agrees with (BL-382, dead market writes). A marketless body's processors fall back
to the two-threshold pool model (PRODUCTION.md § Processing), and goods extracted there enter the
economy only by convoy to a market.

## Adjacent design

- **Population demand's undersupply effects.** `inject_population_demand` pulls real,
  price-elastic, multi-resource demand into the market every tick, and the signal moves prices.
  What a shortfall against that demand does to habitability, workforce efficiency and growth is
  the habitability feedback model in `docs/economy/POPULATION.md`; RESOURCES.md § Habitability
  goods names the intended effect per good.
- **Exchange policy.** BL-160 (auto-exchange policy) is the player's standing buy/sell band and
  the buy book's first live writer; BL-161 (counterparty allow/deny) is the per-counterparty gate
  over it.
