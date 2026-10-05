# Sprint 49 — shelf supply k re-sweep on the household integration branch

Measurement, 2026-10-05. Ben's NR-972 ruling: k = 0 until water is supplied, then re-sweep.
Water is now supplied (BL-1197: generation places Wells). This is that re-sweep.
**k is not changed on the branch** — it ships `shelf_supply_ticks = 0`; Ben ratifies.

## The tree

Integration commit `22062e4b` on `worktree-agent-a836856240126a55f`:
`main` @ a9823674 (BL-1197 merged: Wells at generation, derived demand in the charter walk)
+ `worktree-agent-adc6cdf570362aedc` @ 40a4fb87 (BL-1179 shelf spoilage on BL-1196 households
consume and BL-1163 the per-market household growth gate; k = 0, water durable).

Conflicts, resolved as the household re-measure's measurement merge did:

1. `docs/development/save_version_reservations.jsonl` — kept main's v33 line (BL-1201) and
   claimed **v35** through `next_save_version.js --kind world --claim` (ledger line added).
2. `src/world/world_save.hpp` — both comment blocks kept; `world_save_version = 35`. The stream
   carries BL-1196's market tail (household_bid / household_fill) and BL-1201's
   `sell_order::empty_ticks`: a layout neither 32 nor 33 describes. 34 is held by the throwaway
   `meas49-c` branch (never claimed); the tool named 35 as the next safe number.
   `world_save.cpp` auto-merged with both record changes.
3. `tools/verify/market_viability.cpp` — both G5 rows kept, separate pooled scopes. Taken from
   `meas49-c`'s resolution, then three-way-merged with main's later BL-1197 additions (clean).

Integrity on the integration commit: `world_determinism` ALL PASS (0 failures);
`save_roundtrip` SAVE ROUND TRIP OK (70 PASS, 0 FAIL) at v35. Regression rows:
`population_mvp` ALL PASS (27), `market_inventory_harness` ALL PASS,
`shelf_spoilage_loader_check` ALL PASS, `order_book_harness` ALL PASS (86/0).

## The instrument

`market_viability --seeds 0,43,10 --ticks 400 --k {0,1,2,4}`, the four runs in parallel.
k = 0 is the shipped value (`scripts/economy.lua` `shelf_supply_ticks = 0`), so the k = 0 row
is the integration branch's baseline. The k override applies from world build, so the 12-tick
settle runs at the swept k too (handoff heads differ by < 1M).

This sweep adds a **W row** to `market_viability` (a pure reader): water in two windows, early
(play ticks 20-50) and late (the last 50), against the G5 shelf snapshot — surplus markets,
dry markets, their prices, and the water convoys dispatched each tick. It also adds the late
window's household fill per good. `steel_chain_probe` gains `--k` for the refusal breakdown.

## The sweep

### Pooled (seeds 0, 43, 10; 400 play ticks)

| k | G1 run@handoff | G2 play26-50/settle | G3 firms alive t400 | G5 idled came back | G5 at ceiling (t20-50) | at ceiling + stocked | heads handoff -> t400 (M) | centres grew / fell |
|---|---|---|---|---|---|---|---|---|
| 0 | 42.8% | 45.1% | 72.1% (277/384) | 46.2% | 54.8% | 1.4% | 174.2 -> 288.9 | 718 / 1635 |
| 1 | 41.3% | 48.1% | 60.4% (232/384) | 56.9% | 49.6% | 0.0% | 173.6 -> 328.7 | 911 / 1442 |
| 2 | 41.9% | 51.9% | 57.3% (220/384) | 63.5% | 49.1% | 0.0% | 173.0 -> 289.3 | 828 / 1331 |
| 4 | 43.3% | 47.1% | 52.1% (200/384) | 63.8% | 50.5% | 0.0% | 173.5 -> 230.8 | 688 / 1604 |

### Household fill / bid per good, pooled

| k | early (ticks 20-50) | late (ticks 351-400) |
|---|---|---|
| 0 | agricultural_produce 90% water 79% food_rations 68% clean_water 15% consumer_goods 10% medical_supplies 18% | agricultural_produce 89% water 80% food_rations 70% clean_water 25% consumer_goods 48% medical_supplies 52% |
| 1 | agricultural_produce 92% water 79% food_rations 78% clean_water 18% consumer_goods 18% medical_supplies 41% | agricultural_produce 89% water 89% food_rations 66% clean_water 29% consumer_goods 49% medical_supplies 52% |
| 2 | agricultural_produce 92% water 78% food_rations 75% clean_water 19% consumer_goods 22% medical_supplies 41% | agricultural_produce 89% water 90% food_rations 63% clean_water 26% consumer_goods 39% medical_supplies 48% |
| 4 | agricultural_produce 92% water 71% food_rations 73% clean_water 13% consumer_goods 19% medical_supplies 30% | agricultural_produce 87% water 87% food_rations 46% clean_water 12% consumer_goods 24% medical_supplies 28% |

### Per seed

| seed | k | G1 | G2 | G3 | heads t12 -> t400 (M) | grew / fell / held | late fill (water, clean water, food rations, consumer goods, medical) |
|---|---|---|---|---|---|---|---|
| 0 | 0 | 46.7% | 58.9% | 69.5% (89/128) | 79.4 -> 138.3 | 166 / 648 / 0 | 78 / 26 / 65 / 58 / 64 |
| 0 | 1 | 35.6% | 43.9% | 63.3% (81/128) | 79.2 -> 142.1 | 166 / 648 / 0 | 83 / 30 / 64 / 63 / 62 |
| 0 | 2 | 38.1% | 47.0% | 57.0% (73/128) | 79.3 -> 143.2 | 166 / 537 / 111 | 88 / 31 / 62 / 42 / 61 |
| 0 | 4 | 41.0% | 46.9% | 53.1% (68/128) | 79.3 -> 137.1 | 227 / 557 / 30 | 86 / 13 / 39 / 29 / 26 |
| 43 | 0 | 50.2% | 37.0% | 70.8% (92/130) | 41.9 -> 87.8 | 313 / 345 / 0 | 88 / 36 / 79 / 65 / 63 |
| 43 | 1 | 49.8% | 59.8% | 60.8% (79/130) | 41.7 -> 90.6 | 313 / 345 / 0 | 92 / 32 / 69 / 52 / 39 |
| 43 | 2 | 47.7% | 69.8% | 58.5% (76/130) | 41.4 -> 71.7 | 313 / 345 / 0 | 92 / 33 / 69 / 56 / 49 |
| 43 | 4 | 46.7% | 52.5% | 52.3% (68/130) | 41.9 -> 50.1 | 216 / 442 / 0 | 88 / 0 / 51 / 16 / 8 |
| 10 | 0 | 30.1% | 34.8% | 76.2% (96/126) | 52.9 -> 62.7 | 239 / 642 / 0 | 73 / 10 / 67 / 0 / 22 |
| 10 | 1 | 41.1% | 40.9% | 57.1% (72/126) | 52.7 -> 96.0 | 432 / 449 / 0 | 90 / 27 / 65 / 29 / 48 |
| 10 | 2 | 41.8% | 39.0% | 56.3% (71/126) | 52.3 -> 74.4 | 349 / 449 / 83 | 90 / 14 / 58 / 14 / 30 |
| 10 | 4 | 43.2% | 41.3% | 50.8% (64/126) | 52.3 -> 43.6 | 245 / 605 / 31 | 86 / 20 / 48 / 22 / 41 |

### Water (per tick means; consuming = a market whose households bid water)

Surplus = shelf held the whole household bid; dry = shelf < 1 unit (G5 snapshot, before the clear). p/base = mean price over base in that class. Shipped = convoys dispatched that tick carrying water (corporate and shelf-export); "to dry" = bound for a market dry that tick.

| scope | k | window | consuming | surplus mkts | units on surplus shelves | surplus p/base | dry mkts | dry unmet u | dry p/base | convoys | units shipped | of which shelf export | to a dry mkt |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| pooled | 0 | early | 20.7 | 5.3 | 2589 | 5.86 | 15.0 | 41 | 8.38 | 3.9 | 14 | 2 | 8 |
| pooled | 1 | early | 20.7 | 2.8 | 1395 | 1.42 | 16.9 | 50 | 7.70 | 4.1 | 18 | 3 | 13 |
| pooled | 2 | early | 20.7 | 2.2 | 1012 | 1.45 | 17.2 | 56 | 7.45 | 4.6 | 15 | 3 | 10 |
| pooled | 4 | early | 20.7 | 1.7 | 784 | 1.50 | 18.0 | 65 | 7.79 | 3.8 | 22 | 3 | 15 |
| pooled | 0 | late | 20.7 | 2.8 | 3957 | 2.59 | 17.5 | 48 | 8.25 | 4.3 | 9 | 2 | 7 |
| pooled | 1 | late | 20.7 | 2.7 | 5587 | 0.93 | 16.9 | 35 | 7.63 | 5.0 | 9 | 1 | 7 |
| pooled | 2 | late | 20.7 | 2.7 | 2552 | 1.09 | 16.3 | 36 | 7.49 | 4.9 | 11 | 4 | 9 |
| pooled | 4 | late | 20.7 | 1.5 | 246 | 0.81 | 17.7 | 39 | 7.24 | 5.3 | 8 | 1 | 8 |
| s0 | 0 | early | 25.0 | 5.5 | 2445 | 6.86 | 18.8 | 50 | 8.24 | 5.0 | 17 | 1 | 14 |
| s0 | 1 | early | 25.0 | 4.1 | 2478 | 1.44 | 19.7 | 48 | 7.97 | 4.8 | 12 | 5 | 9 |
| s0 | 2 | early | 25.0 | 3.0 | 1941 | 1.71 | 20.2 | 55 | 7.14 | 7.3 | 16 | 6 | 12 |
| s0 | 4 | early | 25.0 | 2.5 | 1633 | 1.86 | 21.5 | 64 | 7.96 | 4.8 | 19 | 6 | 14 |
| s0 | 0 | late | 25.0 | 2.7 | 3639 | 3.00 | 22.0 | 54 | 8.15 | 6.5 | 11 | 3 | 10 |
| s0 | 1 | late | 25.0 | 1.5 | 1201 | 1.95 | 22.3 | 43 | 7.26 | 5.8 | 8 | 3 | 6 |
| s0 | 2 | late | 25.0 | 3.4 | 1737 | 1.65 | 19.5 | 43 | 7.41 | 6.3 | 18 | 9 | 12 |
| s0 | 4 | late | 25.0 | 1.6 | 295 | 0.87 | 21.5 | 44 | 7.88 | 5.5 | 7 | 1 | 6 |
| s43 | 0 | early | 14.0 | 5.4 | 2710 | 5.81 | 8.5 | 23 | 8.17 | 3.3 | 15 | 1 | 4 |
| s43 | 1 | early | 14.0 | 2.0 | 1172 | 0.93 | 11.3 | 45 | 7.02 | 5.3 | 37 | 3 | 25 |
| s43 | 2 | early | 14.0 | 1.8 | 843 | 0.69 | 11.4 | 43 | 7.20 | 5.1 | 23 | 1 | 14 |
| s43 | 4 | early | 14.0 | 1.2 | 608 | 0.88 | 11.9 | 50 | 7.23 | 5.4 | 43 | 5 | 29 |
| s43 | 0 | late | 14.0 | 3.0 | 6199 | 0.88 | 11.0 | 31 | 7.66 | 3.3 | 10 | 0 | 5 |
| s43 | 1 | late | 14.0 | 3.4 | 6748 | 0.67 | 10.4 | 24 | 8.24 | 1.6 | 4 | 0 | 3 |
| s43 | 2 | late | 14.0 | 2.6 | 5478 | 0.60 | 10.3 | 27 | 7.46 | 2.4 | 4 | 0 | 4 |
| s43 | 4 | late | 14.0 | 1.0 | 198 | 0.61 | 11.8 | 33 | 6.68 | 2.7 | 5 | 0 | 5 |
| s10 | 0 | early | 23.0 | 5.1 | 2611 | 4.83 | 17.6 | 50 | 8.62 | 3.4 | 9 | 4 | 7 |
| s10 | 1 | early | 23.0 | 2.4 | 534 | 1.81 | 19.7 | 58 | 7.83 | 2.2 | 4 | 1 | 4 |
| s10 | 2 | early | 23.0 | 1.9 | 251 | 1.75 | 20.1 | 69 | 7.89 | 1.6 | 5 | 1 | 3 |
| s10 | 4 | early | 23.0 | 1.5 | 110 | 1.40 | 20.6 | 80 | 7.94 | 1.4 | 3 | 0 | 3 |
| s10 | 0 | late | 23.0 | 2.8 | 2033 | 4.02 | 19.6 | 57 | 8.69 | 2.9 | 7 | 4 | 6 |
| s10 | 1 | late | 23.0 | 3.2 | 8813 | 0.74 | 18.0 | 37 | 7.73 | 7.5 | 14 | 0 | 12 |
| s10 | 2 | late | 23.0 | 2.2 | 440 | 0.82 | 19.1 | 37 | 7.60 | 6.0 | 12 | 2 | 10 |
| s10 | 4 | late | 23.0 | 1.8 | 244 | 0.88 | 19.9 | 40 | 6.87 | 7.9 | 13 | 2 | 12 |

### Why water does not move: the dispatcher's refusals (seed 0)

`steel_chain_probe --seeds 0 --ticks 60 --good water --quiet --k {0,2}`, the BL-1186
breakdown at each tick's decision point, summed over ticks 12-59. **Caveat:** this probe runs
the spectated continuation, not the seated game, and "would send" is before the passive-LP cap.
It explains the mechanism; the sweep's shipped column is the measured volume.

| k | pool | sources | surplus u | pairs | price <= gate | unroutable | (no port from origin / none reaching dest) | no room | would send (pairs, u) |
|---|---|---|---|---|---|---|---|---|---|
| 0 | market shelf | 6.0 | 3558 | 149 | **74.6** | 53.3 | 28.1 / 25.1 | 16.5 | 4.5, 15.0 |
| 2 | market shelf | 4.3 | 2841 | 106 | **13.4** | 70.6 | 47.6 / 22.9 | 12.6 | 8.6, 34.7 |
| 0 | corporate pools | 7.0 | 217 | 174 | 7.5 | 107.1 | 51.0 / 56.1 | 40.9 | 19.0, 69.9 |
| 2 | corporate pools | 9.6 | 296 | 240 | 14.4 | 145.4 | 51.0 / 94.4 | 43.2 | 33.7, 136.9 |

At tick 59, seed 0, k = 0: the body's big water shelf (market 47896, 2066 u) already prices at
0.59 x base, because it lists. It is not price-gated: every one of its pairs is unroutable —
"cheapest path crosses water, no port reached overland from the origin". The shelves the price
gate does hold at k = 0 are small ones (10-71 u) parked at the ceiling (x7.6-x10 base).
At k = 2 those small shelves are priced down and drain; only the big unroutable one is left.

## The plain answer

**The hypothesis is half right.** At k = 0 a stocked shelf does sit at the ceiling: the pooled
surplus-market water price is 5.9 x base early, against 8.4 x for dry markets. Any k > 0
collapses it to 1.4-1.5 x, and the dispatcher's price-gate refusals on shelf water fall from 75
to 13 a tick. The gate opens.

**But opening it barely moves water.** Water shipped between markets stays at 8-22 u/tick at
every k, against 2.5-6k units parked on surplus shelves and 15-18 dry consuming markets.
The binding brakes are not price:

1. **Unroutable** — the surplus sits where no port is reachable overland (the largest single
   refusal at every k, and the whole of the big shelf's).
2. **No room** — a dry market's household bid is elastic and collapses at the ceiling price
   (d.demand 0.1-1.4 u at the sampled dry markets), so the room the dispatcher may fill is tiny.

**k against G3.** G3 (firms alive) falls monotonically with k, on every seed:
pooled 72.1% / 60.4% / 57.3% / 52.1% at k = 0 / 1 / 2 / 4 (seed 0: 69.5 -> 53.1; seed 43:
70.8 -> 52.3; seed 10: 76.2 -> 50.8). **No k > 0 keeps G3 at or above k = 0's.** k = 0 is the
only swept value that passes the 70% target.

**The trade, plainly.** k = 1 serves households best:

| | k = 0 | k = 1 | move |
|---|---|---|---|
| heads t400 (pooled) | 288.9M | 328.7M | +39.8M (+14%) |
| centres grew / fell | 718 / 1635 | 911 / 1442 | +193 grew |
| late water fill | 80% | 89% | +9 pt |
| late clean water / medical / consumer goods | 25 / 52 / 48% | 29 / 52 / 49% | flat |
| late food rations | 70% | 66% | -4 pt |
| G3 firms alive | 72.1% | 60.4% | **-11.7 pt (below the 70% target)** |
| G2 | 45.1% | 48.1% | +3.0 pt |

k = 2 gives the same water fill (90%) with heads flat to k = 0 (289.3M) and G3 lower still
(57.3%). k = 4 is worse for both: heads fall to 230.8M, food rations late fill 46%, G3 52.1%.

So: **k = 1 buys ~14% more heads and +9 pt water fill for -11.7 pt of firms alive.** Most of
k = 1's head gain is seed 10 (52.7M -> 96.0M against 62.7M at k = 0); seeds 0 and 43 gain
about 3% each. And it does not buy it through water moving between markets — shipped water is
flat. Where the gain does come from (lower prices at the stocked markets raising their own
households' elastic bid, or a different firm mix) this sweep does not separate.

**What would actually move the water** (an observation for Ben's call, not built): the stocked
water is stranded behind the land/sea split (BL-1186's port legs need a port reachable
overland at both ends), and the dry markets' bid self-suppresses at the ceiling. Neither is a
k question.

## Reproduce

```bash
bash tools/verify/build_lua_harness.sh market_viability
build_gen/verify/market_viability.exe --seeds 0,43,10 --ticks 400 --k 1
bash tools/verify/build_lua_harness.sh steel_chain_probe
build_gen/verify/steel_chain_probe.exe --seeds 0 --ticks 60 --good water --quiet --detail-ticks 59 --k 2
```

Run time: ~5 min per k (three seeds), four k in parallel. The PC's keep-awake hold could not
be taken from the worktree agent; the run finished without a sleep (per-seed times 48-124 s).

## Cold review of the household stack — finding 1, held for Ben

The households are not the shelf's last claimant in a tick. They draw at the end of
`clear_markets`; national upkeep and the space programme draw from the same shelf later in
the tick. So the households are served before those state draws, not after them. Which
claimant should come last is a priority call for Ben.
The review fix round left the code order unchanged. Findings 2-5 are fixed on this branch.
