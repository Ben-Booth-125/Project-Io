# Sprint 49 — why households are a market nobody supplies (BL-1193 diagnosis)

**Diagnosis only.** Nothing in `src/` or `scripts/` moved. The probe is
`tools/verify/household_supply_probe.cpp`, a pure reader.

```
bash tools/verify/build_lua_harness.sh household_supply_probe
build_gen/verify/household_supply_probe.exe --seeds 0,43,10 --ticks 200 --samples 1,2,3,12,50,100,200
```

It steps the world as `centre_decline_trace` does: 12 spectated settle ticks, then play ticks.
**Tick T is the state after T economy steps**, so 12 is the seat handoff. Per sample and per
household good, it prints four things:

- **The household bid.** This is `inject_population_demand`'s formula, recomputed against the posted
  price the injection read.
- **The market.** Demand, listings, shelf, corporation pools and reservation, and the posted price.
  It also counts markets standing **over the fair-price ceiling**: their shelf is closed to every
  draw.
- **The exchanges, by side.** "corp→mkt" is a sale to the market. "mkt→corp" is a shelf draw.
- **The makers.** Placed and running, why each is starved, and every switch, idle or resume since
  the last sample.

## The short answer

**Households never take a unit off any shelf.** Their bid is a pricing pull only. The price it
makes is real money to the seller, but the market pays that money, not the household.

**That pull is also what kills the supply chain.** A good that households bid for and nobody lists
goes to the 10x ceiling and stays there. The household bid never falls to zero, so the price
never eases. Above 2x base, the fair-price ceiling shuts the shelf to every processor.

**Water is the keystone.** It is a household good and the input to clean water and medical
supplies. Opening water stock (about 8,000 u a seed) is auctioned onto the shelves at tick 1. From
tick 2 to 3 it is locked there for the whole run, priced at 10x by households alone. On seeds 0 and
10 no firm digs water at all.

**The makers then leave or starve:**

- **Clean-water plants switch recipe.** The scorer moves them to consumer goods, the dearest recipe
  in their group. It never asks whether that recipe's inputs can be drawn.
- **Consumer-goods plants starve.** Food rations are ceiling-locked in 19–27 markets of 18–27, and
  steel has collapsed (BL-1186 § F).
- **Starved plants churn.** The scorer resumes them on a price-only estimate, and the reflex idles
  them again.

## Counts per seed

"HH bid" is the household bid, summed over markets, in units per tick. "Over ceil" counts markets
whose posted price exceeds 2x base. "Placed / running" counts live makers of the good. "Markets w/
maker" counts the bidding markets that hold at least one maker.

| seed | tick | good | HH bid | listed | sold to mkt @ price | shelf | px/base | over ceil | placed / running | mkts w/ maker |
|---|---|---|---|---|---|---|---|---|---|---|
| 0 | 1 | water | 478.5 | 8,067.7 | 8,067.7 @ 1.03 | 8,067.7 | 2.19 | 15/24 | 0/0 | 0/24 |
| 0 | 1 | clean_water | 558.2 | 136.0 | 136.0 @ 10.33 | 136.0 | 3.47 | 20/24 | 16/15 | 5/24 |
| 0 | 1 | medical_supplies | 239.2 | 96.0 | 96.0 @ 17.17 | 96.0 | 2.89 | 18/24 | 4/4 | 2/24 |
| 0 | 1 | consumer_goods | 398.8 | 144.0 | 144.0 @ 75.76 | 144.0 | 3.24 | 19/24 | 32/28 | 6/24 |
| 0 | 1 | food_rations | 957.0 | 8,914.6 | 8,914.6 @ 9.60 | 8,914.6 | 2.20 | 15/24 | 22/21 | 7/24 |
| 0 | 12 | water | 143.6 | 0 | — | 7,936.7 | 9.99 | 24/24 | 0/0 | 0/24 |
| 0 | 12 | clean_water | 167.5 | 0 | — | 257.6 | 9.99 | 24/24 | 0/0 | 0/24 |
| 0 | 12 | medical_supplies | 71.8 | 0 | — | 126.4 | 9.99 | 24/24 | 0/0 | 0/24 |
| 0 | 12 | consumer_goods | 119.6 | 38.6 | 38.6 @ 298 | 767.9 | 9.04 | 24/24 | 30/8 | 6/24 |
| 0 | 12 | food_rations | 435.9 | 192.8 | 192.8 @ 27.28 | 10,703 | 3.54 | 21/24 | 26/26 | 7/24 |
| 0 | 50 | consumer_goods | 77.4 | 11.3 | 11.3 @ 335 | 2,102 | 8.93 | 24/24 | 17/0 | 5/24 |
| 0 | 50 | food_rations | 310.4 | 174.8 | 174.8 @ 24.88 | 16,550 | 3.34 | 19/24 | 24/24 | 7/24 |
| 0 | 100 | consumer_goods | 77.4 | 74.4 | 74.4 @ 370 | 2,814 | 8.25 | 24/24 | 25/11 | 7/24 |
| 0 | 100 | food_rations | 329.1 | 184.9 | 184.9 @ 24.67 | 22,593 | 3.09 | 19/24 | 26/26 | 7/24 |
| 0 | 200 | consumer_goods | 77.4 | 0 | — | 3,477 | 10.00 | 24/24 | 12/0 | 4/24 |
| 0 | 200 | food_rations | 286.0 | 67.7 | 67.7 @ 23.89 | 33,550 | 4.40 | 22/24 | 20/20 | 7/24 |
| 43 | 1 | water | 324.3 | 8,341.1 | 8,341 @ 0.96 | 8,341 | 2.38 | 11/18 | 2/2 | 1/18 |
| 43 | 1 | clean_water | 378.3 | 128.0 | 128.0 @ 7.72 | 128.0 | 4.01 | 15/18 | 16/16 | 3/18 |
| 43 | 1 | medical_supplies | 162.1 | 128.0 | 128.0 @ 11.24 | 128.0 | 4.02 | 16/18 | 16/16 | 2/18 |
| 43 | 1 | consumer_goods | 270.2 | 128.0 | 128.0 @ 56.54 | 128.0 | 3.98 | 15/18 | 21/16 | 4/18 |
| 43 | 12 | water | 120.7 | 68.2 | 68.2 @ 3.08 | 8,738 | 6.49 | 17/18 | 2/2 | 1/18 |
| 43 | 12 | clean_water | 113.5 | 0 | — | 249.6 | 9.99 | 18/18 | 2/0 | 1/18 |
| 43 | 12 | medical_supplies | 56.1 | 3.9 | 3.9 @ 43.69 | 322.3 | 7.83 | 17/18 | 8/2 | 2/18 |
| 43 | 12 | consumer_goods | 81.1 | 0.6 | 0.6 @ 416 | 1,061 | 9.62 | 18/18 | 28/0 | 3/18 |
| 43 | 12 | food_rations | 315.3 | 210.7 | 210.7 @ 16.16 | 10,694 | 4.39 | 16/18 | 39/22 | 2/18 |
| 43 | 100 | consumer_goods | 81.1 | 0 | — | 1,803 | 10.00 | 18/18 | 8/0 | 2/18 |
| 43 | 200 | water | 122.6 | 21.9 | 21.9 @ 1.82 | 14,381 | 7.18 | 17/18 | 2/2 | 1/18 |
| 43 | 200 | food_rations | 253.0 | 104.8 | 104.8 @ 34.16 | 25,210 | 4.86 | 17/18 | 17/9 | 3/18 |
| 10 | 1 | water | 492.0 | 8,107.5 | 8,108 @ 0.94 | 8,108 | 2.95 | 20/27 | 0/0 | 0/27 |
| 10 | 1 | clean_water | 574.0 | 144.0 | 144.0 @ 8.92 | 144.0 | 3.47 | 23/27 | 16/16 | 4/27 |
| 10 | 1 | medical_supplies | 246.0 | 128.0 | 128.0 @ 13.60 | 128.0 | 3.22 | 22/27 | 12/12 | 4/27 |
| 10 | 1 | consumer_goods | 410.0 | 112.0 | 112.0 @ 70.30 | 112.0 | 3.32 | 22/27 | 24/20 | 5/27 |
| 10 | 12 | water / clean_water / medical | 147.6 / 172.2 / 73.8 | 0 / 0 / 0 | — | 8,030 / 266 / 219 | 9.99 | 27/27 | 0/0, 0/0, 7/0 | — |
| 10 | 12 | consumer_goods | 123.0 | 0 | — | 883 | 9.10 | 27/27 | 65/0 | 4/27 |
| 10 | 100 | consumer_goods | 133.1 | 10.6 | 10.6 @ 296 | 2,462 | 7.37 | 27/27 | 49/4 | 4/27 |
| 10 | 200 | food_rations | 467.7 | 189.7 | 189.7 @ 18.30 | 48,946 | 4.31 | 25/27 | 46/42 | 5/27 |

**The sampled ticks repeat the same figures.** From tick 12 to 200, clean water and medical
supplies list 0 on all three seeds. Water lists 0 on seeds 0 and 10. On seed 43, two water
extractors in one market list 22–68 u, but water stays over the ceiling in 17 of 18 markets.
**Households draw 0 units at every tick on every seed.** No `mkt→corp` row is ever a household,
because there is no household draw (§ 2).

Full per-tick output, including the steel rows and the churn lines, is reproducible with the
command above. Seed 0 takes about 3 minutes for 200 ticks.

## 1. Where household-good output goes

The output goes into the maker's pool, then onto a market shelf. It never reaches a household.

1. **Pool.** `run_processing` credits the output to the corporation's (corp, market) pool.
2. **Auto-surplus listing.** Everything above the processor reservation is listed
   (`market_clearing.cpp:1222-1259`).
   - **Nothing reserves clean water, medical supplies or consumer goods.** No recipe the maker runs
     takes them as an input. The probe reads pools 0.0 and reserved 0.0 at every sample, so 100%
     of output lists on the tick it is made.
3. **Sold to the market.** The market is the buyer of last resort. The corporation is paid
   `qty × ref_price` (`market_clearing.cpp:1414-1432`, income at `:1427`), and the units are
   credited to `mc.inventory`, the shelf (`:1425`). The exchange row's buyer is `null_entity`.
4. **The shelf, for good.** Shelf stock leaves by three routes only:
   - a processor or construction draw (`economy_system.cpp:446`, `:824`);
   - an upkeep draw (`:2533`, `network_upkeep.cpp:388`, `space_programme.cpp:314`);
   - a market export (`supply_system.cpp:1105`).

   Each draw is gated by `shelf_admits`. **No household path decrements `inventory` anywhere.**
   So, as the § Counts table shows:
   - shelves of goods only households want only grow — clean water 257.6 u, medical 126–373 u,
     consumer goods 1.8–3.5k u;
   - food rations reach 25–49k u by tick 200;
   - water stays at about 8,000 u.

**The tick-1 sale is opening stock.** At tick 1 the corporations' opening pools are auctioned at
home. That puts about 8,000 water, 8,700–9,000 food rations and 11–12k agricultural produce on the
shelves. Opening stock comes from `generate_starting_stockpile`, `corporation_generation.cpp:769-800`.
This is the same move as BL-1186 § F for steel.

## 2. The household bid is a pricing pull, not a buyer

**`inject_population_demand` only adds to `mc.demand`** (`market_clearing.cpp:516-586`; the add is
at `:585`). It is called after the demand reset (`:1139`).

- It never reads `inventory`.
- It never debits anyone.
- No exchange row ever has a household side.

The BL-1163 lane's reading is right: **the clear never physically fills households.**

**Nobody pays a firm for a unit a household takes, because households take none.** A firm is paid
for every unit it lists, by the market (§ 1, step 3). It is paid at the reference price, which the
household bid drives. That price is `base × sqrt(D/S)`, or `base × 10` when nothing is listed, eased
50% a tick (`market_clearing.cpp:30-40`, `:56-66`).

**Growth reads the same listings.** The growth pass's met ratio is `min(1, supply/demand)`, where
`supply` is listings (`economy_system.cpp:1872-1886`). So a centre counts as fed only by what was
listed **this tick**. The 10–49k of food rations standing on its own market's shelf count for
nothing.

**Doc divergence.** `POPULATION.md` § Population demand says a centre "consumes a basket of goods
drawn from the local market", and that a deficit is "met by imports". `MARKETS.md` § Demand
channels names Household as a **terminal sink**: "consumes a good and produces nothing". The code
consumes nothing. Either the code or those two passages are wrong.

## 3. Can a household-good maker earn, and from what?

**Yes, and handsomely, while it can run.** All of its income is the market's buyer-of-last-resort
payment (§ 1, step 3). The price is set by the household bid against its own listings:

- clean water: 7.72–12.64 a unit at ticks 1–2 (1.0–1.6x base);
- medical supplies: 11–44 a unit;
- consumer goods: 56–76 at tick 1, then **298–416 a unit** (4.9–6.8x base) once listings thin.

That money is **minted**: no balance is debited for it. A maker with output therefore has an
excellent market.

**The constraint is entirely on the input side.** Clean water, medical supplies and consumer goods
are not short of buyers. They are short of inputs a processor is allowed to draw.

## 4. Why the makers go: the lock, the switch, the churn

### 4a. The household bid locks the input shelves (the keystone)

Four rules combine:

1. **A bid with nothing listed aims at 10x base** (`market_clearing.cpp:35-36`; `ceil_mult` 10,
   `economy.lua:1391`).
2. **The shelf is not supply.** At `shelf_supply_ticks = 0` (`economy.lua:1400`;
   `market_clearing.hpp:265-272`), 8,000 u of water on the shelf reads to the price law as
   nothing.
3. **The household bid never reaches zero.** Elasticity is floored at `elasticity_min = 0.30`
   (`economy.lua:1258`; the clamp is at `market_clearing.cpp:581-584`). At 10x, households still
   bid 30% of their basket.
4. **Over 2x base, the shelf is shut to every draw** (`shelf_admits`, `components.hpp:1070-1077`;
   `reservation_mult` 2.0, `economy.lua:1392`). A processor neither counts it nor bids for it
   (`economy_system.cpp:361-365`, `:439-446`).

**Water's whole demand is the household bid.** Seed 0, tick 12: demand 143.6 against a household
bid of 143.6. Processors stop bidding the moment the ceiling shuts them out (rule 4).

**This is NR-968's cycle with the release valve welded shut.** In the steel cycle the price eases
after the bid drops out. Here the bid never drops out (rule 3), so the price sits at 10.00x
permanently. It does so from tick 2–3 on every seed, in 24 of 24, 18 of 18 and 27 of 27 markets.
The 8,000 u water shelf is never drawn again.

**Households, by bidding for water, price the input of the clean water and medical supplies they
also bid for beyond reach.** The same lock holds food rations over 2x in 16–25 markets. Households
bid more food than is listed, so food is ceiling-locked to the consumer-goods makers (§ 4c).

### 4b. Clean-water plants switch away; they do not starve first

On seed 0 the plants exit like this:

- **Tick 1:** 16 placed, 15 running.
- **Tick 2:** 14 placed, 14 running.
- **Tick 3:** 0 placed. 14 switched away, 12 to consumer goods and 2 to medical supplies.

Seed 10 does the same by tick 12, and seed 43 has 2 left (both under construction). The switch is
the scorer's **recipe margin chase** (`corp_ai.cpp:1553-1630`), the legal `set_recipe` verb.

- **It is not the reflex rescue.** That rescue fires only on a floored output price
  (`economy_system.cpp:1992-2014`), and clean water stood at 3.5–7x.
- **The chase compares `recipe_margin` within a group** (`corp_ai.cpp:166-179`), priced at local
  price for inputs and outputs alike. All three recipes are in group "Welfare Goods"
  (`recipes.lua:247, :258, :271`).
- **At base price, consumer goods win every time:**

  | recipe | per-batch margin at base |
  |---|---|
  | clean water | 7.7 − 2 × 1.5 = **4.7** |
  | medical supplies | 14.0 − 1.5 − 3.0 = **9.5** |
  | consumer goods | 61 − 13.6 − 16.1 = **31.3** |

  Clean water is dominated about 6.7x, so a clean-water plant is a consumer-goods plant waiting
  for a margin gate to clear.
- **The chase checks no input access.** It does not ask whether a recipe's inputs can be drawn.
  The build candidate does ask (`corp_ai.cpp:1150-1172`, `shelf_admits` plus `t_idle`), but the
  chase does not. So it moves working plants onto a recipe whose inputs are locked or empty.

### 4c. Consumer goods: starved on both inputs, then churned

Consumer goods need food rations and steel (`recipes.lua:256-261`).

- **Food rations.** Ceiling-locked in 19–22 of 24 markets on seed 0. Seed 0, tick 3: 15 makers
  starved on `food_rations:ceiling-locked`.
- **Steel.** Its makers collapse to 2–5 by tick 12 (BL-1186 § F). Its shelf is either thin in the
  maker's market (`steel:shelf-admits`: the price admits it but the stock is under `t_idle`) or
  over the ceiling.
  - Seed 0, tick 12: 13 starved on steel.
  - Seed 43, tick 12: 22 starved on steel.

A starved maker is idled. The scorer idles at `corp_ai.cpp:1503`, and the reflex idles persistent
losers at `economy_system.cpp:2043`.

**It is then resumed on a price-only estimate.** `estimate_prospective_profit`
(`building_profit.cpp:113ff`, price lambda `:140`) prices inputs and outputs at the posted price.
It has no coverage or ceiling test. With consumer goods at 8–10x, a resume always looks
profitable (`corp_ai.cpp:1413-1470`). The build/resume and idle counts per window:

| seed | window | consumer goods built or resumed | consumer goods idled | medical built or resumed | medical idled |
|---|---|---|---|---|---|
| 0 | 50→100 | 55 | 47 | 16 | 24 |
| 0 | 100→200 | 73 | 85 | 34 | 32 |
| 43 | 12→50 | 10 | 20 | 48 | 32 |

So the "supply ≈ 0 by tick 100" is not a steady exit. It is a population of makers flickering
between resumed-and-starved and idled, listing a few units on the odd tick:

- seed 0: 74.4 at tick 100, 0 at tick 200;
- seed 10: 10.6 at tick 100, 2.0 at tick 200.

### 4d. Food rations: capacity, not lock

Food processors **run**: 20–46 running, 0 input-starved. Their input, agricultural produce, mostly
sits under 2x. Their output, 68–280 u a tick, is 25–60% of the household bid of 250–470. The
shortfall has two parts:

- **Too few makers, too concentrated.** They sit in 7 of 24 markets on seed 0, 2–3 of 18 on
  seed 43 and 5 of 27 on seed 10.
- **Growth reads listings only.** The 10–49k already on the shelves never counts (§ 2).

## 5. Are makers placed in proportion to household demand?

**No, and water most of all.**

- **Water: 0 extractors on seeds 0 and 10.** Seed 43 has 2, in 1 of 18 markets. Every market
  bids, at 324–492 u a tick at tick 1.
  - **Cause, in generation.** `generate_background_firms` picks the gap resource by absolute
    shortfall and sets `focus = extraction` (`corporation_generation.cpp:2543-2558`).
  - But `place_starting_assets` anchors on any tile that can host an extraction site for its
    **richest** extractable (`:548`). `author_building` then targets that richest deposit, not
    the gap resource (`:476`).
  - The firm is still counted against the gap: `++firms_by_resource[gap_r]` (`:2667`). So water's
    per-resource cap fills with firms that dig something else.
  - It is called from `landscape_search.cpp:214`, the shipped path.
- **The other goods, at tick 1** (the markets holding a maker, of the markets that bid):

  | good | seed 0 | seed 43 | seed 10 |
  |---|---|---|---|
  | clean water | 5 of 24 | 3 of 18 | 4 of 27 |
  | medical supplies | 2 of 24 | 2 of 18 | 4 of 27 |
  | consumer goods | 6 of 24 | 4 of 18 | 5 of 27 |
  | food rations | 7 of 24 | 3 of 18 | 4 of 27 |

  - **Clean-water plants are placed with no water source.** Their placement is not constrained
    to input reach, which is BL-1185 (chain-feasible placement).
  - **Household goods only reach other markets by export,** and export fails for the BL-1186
    reasons.
  - **Placement volume roughly matches the bid at tick 1.** Clean-water makers produce 120–128
    u against a bid of 378–574.

## Candidate fixes

None is taken here. Every one moves the world.

| # | Lever | What it changes | Ruling / doc it touches |
|---|---|---|---|
| H1 | **Households draw.** `inject_population_demand` also takes `min(bid, shelf)` off `mc.inventory`. The firm was already paid at listing, so no new payment is needed. | Shelves of household goods drain, so the 33–49k food piles stop. The fill becomes a measurable "household served". Growth can read the fill. Alone it does **not** unlock water: the price law still ignores the shelf (k = 0). Households would compete with processors for the water shelf, so draw order matters. | `MARKETS.md` § The clearing tick step 3 ("pure demand-side pulls") and § Real market inventory (a new drain). Makes the code match `POPULATION.md` § Population demand. The save seam is untouched (no new field). |
| H2 | **The shelf counts as supply**: `shelf_supply_ticks` k > 0 (`economy.lua:1400`). This is the shipment draft's A1. | The 8,000 u water shelf prices water down to the floor, so the shelf opens and clean-water and medical plants run. Food and consumer-goods shelves price down too. | **Ben ruled k = 0 until shelf spoilage (BL-1179).** BL-1172 measured k > 0 flooring gluts. With H1 the gluts drain, which may answer that objection. |
| H3 | **Households obey a ceiling too.** Above `reservation_mult × base`, the household want leaves the price, as a draw's does. | The price eases off 10x, so input shelves reopen for some ticks. It reproduces NR-968's cycle on household goods, and it silences the want exactly when households are unserved. | It reverses the spirit of BL-441 (demand records the want). It extends the BL-1172 ruling (FINANCE.md, Ben 2026-10-03) to a channel it did not name. **Ben's call.** |
| H4 | **Growth reads the fill, or listings plus shelf** (`economy_system.cpp:1872-1886`). | Centres stop starving beside full shelves. It pairs naturally with H1. | `POPULATION.md` § Growth, decline and razing. BL-1163 (play villages decline) and NR-967. |
| H5 | **Generation digs the gap.** An extraction firm opened for gap resource *g* anchors on a tile carrying *g* and targets it (`corporation_generation.cpp:476, :548, :2667`). | Water extractors appear on every seed that has water deposits, and the water gap is honestly measured. Landscape digests and placements move. | `CORPORATION_GENERATION.md` § Pass 6. **Re-bless owed**; the BL-1031 pins move. |
| H6 | **The scorer asks input access before a switch or resume.** It applies the build candidate's test (`corp_ai.cpp:1150-1172`) to the margin chase (`:1553-1630`), and the coverage test to `estimate_prospective_profit` (`building_profit.cpp:113ff`). | It stops clean water → consumer goods switches into starvation, and the build/idle churn in § 4c. It reads the existing rule, so it is not a new verb. | `AI_OPPONENT.md` § 11. It **refines** the scored-utility grant's existing `set_recipe`/`resume` scoring and adds no new actor or verb. I read it as inside the grant, but the main session should confirm rather than assume. |
| H7 | **Split "Welfare Goods"**, so clean water, medical supplies and consumer goods are separate facilities (`recipes.lua:247, :258, :271`). | The 6.7x margin dominance cannot strip clean-water plants. | `PRODUCTION.md` (recipe groups; BL-434's "a dial tunes within a group"). It is a content change. |

**What I would weigh first.** H1 + H2 together are the structural answer.

- **H1 makes households a real sink.** That is what MARKETS.md property 4 already claims.
- **H2 lets the price see the shelf.** That breaks the permanent lock.
- **H5 is independent and cheap to reason about.** Water has no producer on two seeds of three.
- **H6 removes the churn that hides the lock** in every "consumer goods ≈ 0" reading.

H3 is the fast patch that silences the symptom, and it is a ruling, not a fix.

## Relation to the other sprint-49 diagnoses

- **NR-968 (the price cycle).** Same four rules. The household bid's elasticity floor turns the
  four-tick cycle into a permanent 10x for any good households want and nobody lists. If NR-968 is
  ruled by an A2-style "want stays visible" fix, it **worsens** this lock: more goods would sit at
  10x with shut shelves.
- **BL-1186 (goods cross markets).** Explains the steel half of consumer goods (§ 4c), and why
  household goods cannot reach the markets without makers.
- **BL-1185 (chain-feasible placement).** Clean-water plants sit where no water is produced (§ 5).

## Notes and caveats

- **Novel.** Households as a physical sink (H1) has no owning code path today, and the docs
  disagree with the code (§ 2). Flag it as `novel-work` if H1 is taken.
- **A second divergence, not chased.** The code's basket is flat per scale point
  (`market_clearing.cpp:567-578`: `scale × basket[r]`). `POPULATION.md` § The stratum ladder wants
  headcount, cumulative rungs and a cultural weight.
- **The probe's "starved on" for a just-switched building names its OLD recipe's input.** The
  report row is written before the agency step switches it (seed 0 tick 3:
  `consumer_goods … water:ceiling-locked 14`).
- **"Built" in the churn lines counts resumes from idle as well as new builds.**
- **The household bid is recomputed, not read off the injector.** It uses the pre-tick posted
  price. It matches `demand` exactly where households are the only bidder (water and clean water
  at tick 12).
