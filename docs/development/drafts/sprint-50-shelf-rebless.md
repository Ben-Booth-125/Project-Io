# Sprint 50's one re-bless, with the shelf economy and trade — the shape, the causes, the pins

**PREPARED 2026-10-10 for Ben's authorisation** against the SHAPE below (DELIVERY.md § The digest
re-bless is one act per WAVE, rule 4), never against the hashes. Ben gave the go to run it on
2026-10-10; the result is his to authorise before it merges into `worktree-sprint-50`.

## Baseline and method

- **Pins in force** were taken at sprint 49's re-bless (main c77f2d5a). Sprint 50 never re-blessed,
  so this one act carries all of sprint 50: its own items (merged up to d0c74dea) and the shelf
  economy and trade (BL-1265..BL-1270, the 2026-10-10 rulings, the rival Marketplace scored as any
  build), tip c8752b8a.
- **One instrument on every tree:** `tools/verify/rebless_shape_probe` (the tip's source, built on a
  clean checkout of each tree), the sixteen library seeds, the app's start world after the
  validation settle and the seat.
- **Checkpoints:** c77f2d5a (sprint 49 re-bless), d0c74dea (sprint 50 before the shelf), then six
  points along the shelf branch: a1cdf914 (shelf + trade core), 20a40a7a (AI trades, generation
  trade, UI lanes merged), e5173b81 (cold-review fixes), 41adbfd5 (trade buildings burn coal),
  a1c69671 (upkeep cut), 307b145e (the 2026-10-10 rulings), and the tip.
- **Sprint 50's own items are attributed only as a block** (c77f2d5a -> d0c74dea): they were
  measured item by item by the sprint 50 session's gate, not by this probe.
- **Haulage:** `haulage_measure --far-trade` (its default three seeds, two windows) on d0c74dea and
  the tip.

## The headline

**History is untouched; the 1960 economy contracts.** Every Empires, Exploration and
Industrialisation counter, every region, centre, province, market and lane is identical across all
three trees. The only generation movement is road length (-3%), from sprint 50's no-parallel-roads
item. Everything else is the campaign start — the economy the landscape search and the validation
settle build:

- **Sprint 50's own items** (supply-judged plants) cut the plant to what its inputs can feed:
  firms 2,752 -> 1,833, processors 7,443 -> 2,273, rival cash 16.2 M -> 7.0 M.
- **The shelf economy and trade core** (one checkpoint, a1cdf914) moves almost everything that
  follows: households are fed less (cities demote), the steel and machinery trades shrink, rivals
  muster less, rival cash falls again, and goods moving between markets fall by about 87%.
- **Later shelf steps** move little: burning coal lifts cities back by ~120; the rulings and the
  rival Marketplace build add construction in flight.

## The shape, pooled over the sixteen library seeds

| | s49 c77f2d5a | s50 d0c74dea | shelf core a1cdf914 | coal 41adbfd5 | rulings 307b145e | **tip c8752b8a** |
|---|---|---|---|---|---|---|
| Empires battles | 239,403 | 239,403 | 239,403 | 239,403 | 239,403 | 239,403 |
| regions / markets | 14,097 / 444 | 14,097 / 445 | = | = | = | 14,097 / 445 |
| villages | 4,815 | 4,918 | 8,998 | 8,624 | 8,795 | **8,780** |
| towns | 5,710 | 5,703 | 3,495 | 3,753 | 3,668 | **3,677** |
| cities | 3,030 | 2,934 | 1,064 | 1,180 | 1,094 | **1,100** |
| centre population (k) | 1,021,771 | 1,016,241 | 984,489 | 986,878 | 985,664 | **985,766** |
| firms | 2,752 | 1,833 | 1,831 | 1,835 | 1,835 | **1,835** |
| rivals armed | 280 | 279 | 195 | 193 | 193 | **193** |
| rival units | 33,250 | 32,100 | 27,250 | 26,950 | 26,900 | **26,900** |
| player balance | 55,025 | 37,404 | 21,769 | 27,498 | 22,477 | **22,567** |
| rival cash | 16.22 M | 6.98 M | 4.97 M | 5.08 M | 4.89 M | **4.96 M** |
| rivals negative | 13 | 0 | 15 | 14 | 12 | **12** |
| extraction sites | 6,256 | 4,728 | 4,441 | 4,445 | 4,436 | **4,445** |
| processors | 7,443 | 2,273 | 1,882 | 1,864 | 1,839 | **1,856** |
| military bases | 1,268 | 954 | 280 | 279 | 286 | **307** |
| under construction | 6,383 | 1,574 | 488 | 473 | 1,797 | **1,083** |
| decommissioned | 1,382 | 43 | 76 | 92 | 89 | **94** |
| steel works | 314 | 330 | 132 | 140 | 121 | **136** |
| food works | 966 | 1,167 | 1,081 | 1,068 | 1,068 | **1,063** |
| machinery works | 464 | 11 | 0 | 0 | 0 | **0** |
| medical works | 374 | 51 | 19 | 18 | 20 | **21** |
| refined copper works | 615 | 157 | 127 | 124 | 119 | **116** |

20a40a7a and e5173b81 read identically to a1cdf914 on every row above save rival cash (±2 k);
a1c69671 reads within a few units of 41adbfd5.

## What each movement is

- **Cities demote, villages double (shelf core).** A centre's tier falls when its population drops
  below its rung, and its population falls on every tick its households are under-fed. Under the
  shelf economy households buy only what landed on their market's shelf. Pooled household fill over
  the gate's sixteen seeds, before -> after: water 88% -> 74%, food rations 83% -> 79%, medical
  supplies 16% -> 9%, produce 92% -> 89%. Centre population falls only 3%, but enough centres sit
  just above a rung that 1,834 cities and 2,026 towns step down. Coal upkeep for trade buildings
  (41adbfd5) lifts ~120 cities back.
- **Steel, machinery, medical and copper shrink (shelf core).** No corporation holds stock, so a
  plant runs only on what its market's shelf carries this tick, and goods no longer travel unless a
  trade carries them. Machinery was already near zero after sprint 50's supply-judged plants
  (464 -> 11) and goes to none.
- **Rivals muster less (shelf core).** Military bases 954 -> 280 and armed rivals 279 -> 195: a
  base's upkeep and a unit's hire are bought off the shelf at the posted price.
- **Cash falls (shelf core).** Rival cash 6.98 M -> 4.97 M, the player's 37 k -> 22 k, 15 rivals
  negative at the seat. Every good is bought at the posted price; nothing is drawn free from a
  corporation's own pool.
- **Construction in flight (rulings, tip).** 488 -> 1,797 at the rulings (the flat-score rival
  Marketplace build) -> 1,083 at the tip (the Marketplace scored as any build).
- **Trade falls ~87% (shelf core and capacity).** `haulage_measure --far-trade`, window E / Y:

  | | d0c74dea | tip |
  |---|---|---|
  | convoys delivered | 1,462 / 1,446 | 239 / 188 |
  | units delivered | 7,020.8 / 6,481.3 | 900.3 / 862.1 |
  | sold at destination (a) | 6,993.5 / 6,457.1 | 595.2 / 624.2 |
  | (b) not to the seller's nearest market | 85.7% / 85.6% | 85.8% / 83.8% |

  Before, every corporation dispatched its own surplus from its pool. Now goods move only on trade
  points: the 50-tick gate reads ~350 points made per seed-tick and ~17 spent, most refused by the
  Logistic Point cap or with no spare to carry. Trade still reaches beyond the nearest market at
  the same share; there is just much less of it.

## The gate, for context (16 seeds, 50 ticks)

| | G1 | G1 t50 | G1b | G2 | G3 |
|---|---|---|---|---|---|
| s50 d0c74dea | 96.7 | 87.3 | 1.7 | 95.6 | 100.0 |
| tip c8752b8a | 93.3 | 87.5 | 5.7 (fail; accepted by Ben 2026-10-10) | 113.4 | 99.8 |

## Re-pinned

- `player_seed_sweep` `k_shipped_digest_pins` and `k_world_digest_pins` (legacy arc): all sixteen
  rows of both, from D_search on; every row seated exactly one player. Old rows are one commit back
  in git history; after: `--digest-check` 16/16 PASS on the shipped arc and 16/16 PASS on the legacy arc.
- `seed_library.js --check`: 0 moved, 16 unchanged (the fingerprints are span counters, and no span
  moved). Nothing to bless.

## Gaps

- Sprint 50's own items are attributed as one block, not per merge.
- Trade's collapse is measured on three seeds by `haulage_measure` and on sixteen by the gate's
  trade readout; no gate asserts a floor on inter-market flow.
- The haulage tool's WHY text still describes corporation pools; it reads correctly but explains
  the retired model.
