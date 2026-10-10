# Sprint 48's re-bless movements — traced (BL-1165)

Prepared 2026-10-10 on the sprint 50 branch. It traces the movements the two sprint 48 re-bless
drafts left unattributed or attributed without a mechanism: `sprint-48-rebless.md` (the first,
v0.1.26 e50ab81c -> a899bb2a) and `sprint-48-rebless-2.md` (the second, 179615b8 -> 9171c2ef).
Nothing here re-blesses or re-pins anything. Each verdict says whether the movement is the
intended effect of the commit's ruling; where it is not, a follow-up is proposed, not filed.

## Method

- **One clean export per commit** (`git archive`), every instrument built in it with
  `build_lua_harness.sh`. No working tree was checked out to an old commit.
- **The shape** is `rebless_shape_probe` as committed with the first re-bless (0b5eec63), plus
  read-only fields added for this trace (not committed): nation-owned unit GROUPS (garrisons),
  and the nations' treasury sum. It compiles on every sprint 48 commit.
- **The class splits** (§ 2, § 4) are a scratch copy of each tree's own `haulage_measure` with
  one tally added: (a) by the dispatching corporation's class at dispatch. Not committed; every
  total it prints equals the unpatched harness's.
- **The treasury distributions** (§ 4) are a scratch copy of each tree's `exploration_sweep`
  printing the sorted capital treasuries per seed. Not committed.
- **Population and units are bisected on seeds 28 and 0.** Both seeds move the same way as the
  16-seed pool (population halves on each; units fall 60-67%). Every number below is the two-seed
  sum unless marked pooled. The two endpoints reproduce the drafts' rows for those seeds.
- **Haulage is pooled over all 16 curated seeds** at every checkpoint: `haulage_measure
  --far-trade --seeds 1 --first-seed S`, each tree's own harness. The tip checkpoint (75c1a4e5)
  reproduces the first draft's after column exactly (14,433.9 / 11,916.5); seed 41 at a899bb2a
  reproduces 1,073.2 / 796.2.
- **The second re-bless** reuses its own checkpoints (c3 5c18bf8c, c4 86c8b25c, c5 aeba4413,
  c6 4df6f469, after 9171c2ef). The haulage class split at c3/c4 reproduces that draft's pooled
  c3 (14,640.2 / 11,892.2) and c4 (9,429.1 / 8,485.8) exactly. The seed library fingerprint is
  bisected inside BL-1171's branch with `exploration_sweep --seeds 0,9`.

## 1. World population 12.19 B -> 6.27 B — BL-1132 (settle spacing 3)

| commit | item | population (28 + 0) | regions |
|---|---|---:|---:|
| e50ab81c | v0.1.26 | 1,097.1 M | 3,767 |
| 85fc03a1 | BL-1130 centres consolidate | 1,124.1 M | 3,810 |
| 5cab560b, 7238f8ef | BL-1133, BL-1120, BL-1136, BL-1140, BL-1098 | 1,124.1 M | 3,810 |
| **89d3bb4b** | **BL-1132 settle spacing 3** | **521.4 M (-53.6%)** | **1,187 (-69%)** |
| 3392406d | BL-1132 review fix | 521.4 M | 1,187 |
| b591d1e9 | BL-1141 + BL-1137 (first cut) | 954.5 M | 1,188 |
| 8cbdbc9d | BL-1137 reverted | 521.4 M | 1,187 |
| 9ec287f0 | BL-1150 | 525.2 M | 1,208 |
| d0db9927 | BL-1125 (after the BL-1137 re-merge) | 546.2 M | 1,234 |
| a899bb2a | sprint 48 tip | 545.4 M | 1,232 |

Per seed at the step: 28 352.4 M -> 159.8 M; 0 771.7 M -> 361.6 M. The only other mover was the
first cut of BL-1137, which nearly restored it and was reverted the same day.

**Mechanism.** A region's carrying capacity is its anchor's farm quality only:
`region_carrying_capacity(farm_q) = 5000 + 500 x farm_q` (settlement.cpp:1814 at 89d3bb4b). It
does not read the ground the region commands. BL-1132 refuses a Settle site within three tiles of
any standing anchor (`find_settle_site`, history_sim.cpp:2855; the scorer's room gate,
history_sim.cpp:5942; `generation_settle_spacing_tiles = 3`, era_minus_one.hpp:114). Two thirds of
the regions are never founded, so two thirds of the capacities never exist. Population per region
rises only from 295 k to 439 k.

**Intended?** The ruling is intended: cells wider than one tile. The halving is **not** part of
it. BL-1132's record measured regions and centres and never population. The land feeds fewer
people because a capacity belongs to a city state, not to its ground. **A consequence, unruled;
a call for Ben.**

## 2. Shipped haulage (--far-trade) 16,327 / 14,729 -> 14,434 / 11,917 — BL-1154 and BL-1125, on a rise

Pooled over 16 seeds, window E / Y (reading (a), units sold at a destination market):

| commit | item(s) in the step | (a) E / Y | markets | convoys E |
|---|---|---:|---:|---:|
| e50ab81c | v0.1.26 (the first draft's column) | 16,327.0 / 14,728.9 | 4,984 | n.r. |
| 89d3bb4b | density chain to BL-1132 | 25,148.0 / 23,859.7 | 5,784 | 12,725 |
| 9ec287f0 | to BL-1150 | 34,753.5 / 32,320.4 | 6,921 | 20,864 |
| 856d6235 | BL-1146 | 34,878.6 / 32,880.3 | 6,921 | 20,418 |
| 106ba090 | BL-1153 | 34,637.7 / 32,636.4 | 6,921 | 20,452 |
| 555d164f | BL-1156 | 35,045.0 / 32,505.0 | 6,921 | 20,628 |
| **cf2dc85c** | **BL-1154 rivals start armed** | **24,356.8 / 20,326.0 (-30% / -37%)** | 6,921 | **13,658** |
| 14babd92 | BL-1152 (+ NR-959) | 26,170.4 / 22,419.9 | 6,931 | 14,489 |
| a83e6188 | BL-1137 / 1149 / 1155 | 23,065.9 / 19,314.7 | 8,344 | 13,954 |
| **d0db9927** | **BL-1125 markets die, BL-1127** | **12,207.2 / 11,047.6 (-47% / -43%)** | **492** | **3,451** |
| 205a1afa | BL-1159 | 12,020.2 / 10,495.0 | 493 | 3,345 |
| 175ed49e | BL-1145, BL-1139 p1, BL-1107 | 13,534.9 / 11,431.5 | 501 | 3,801 |
| 75c1a4e5 | BL-1138 (= a899bb2a's src) | 14,433.9 / 11,916.5 | 429 | 3,970 |

**The net fall is not one fall.** The density chain first raises haulage to 2.1x the cut
(markets 4,984 -> 6,921, convoys rising). **BL-1154 takes back 30% / 37%** (convoys in window E
20,628 -> 13,658), and BL-1137/1149/1155 a further 12%. **BL-1125 then halves it**: markets
8,344 -> 492 (-94%), convoys dispatched in window E 13,954 -> 3,451 (-75%), volume 23,342 ->
12,399 u (-47%). It falls on 14 of 16 seeds; 41 and 25 rise. The step holding
BL-1107 (+12.6%, beside BL-1145 and BL-1139 part 1; BL-1107's own record was +13.6%) and BL-1138
(+6.6%) recover part.

**BL-1154's mechanism: armed specialists stop hauling.** The same per-dispatcher class split as
§ 4, at 555d164f -> cf2dc85c, pooled over 16 seeds:

| | before E | after E | before Y | after Y |
|---|---:|---:|---:|---:|
| specialists | 20,467.5 (12,216 convoys) | 8,202.4 (4,306) | 18,234.8 | 4,462.6 |
| background firms | 14,392.5 | 15,912.0 | 14,028.0 | 15,573.5 |
| specialist corporations at the close | 1,266 | 859 | | |

Every specialist now opens with a muster base and a 50-head unit (`arm_rivals`,
corporation_generation.cpp at cf2dc85c). Specialists' haulage falls 60% / 76%, and a third of
them are gone by the window's close. Background firms take back a tenth. **Traced** to the
specialist class and its exits. **Inferred:** the exits are the force's upkeep against the
charter's cash, with the solvency gate refusing a haul a corporation cannot pay
(supply_system.cpp:753 at cf2dc85c). BL-1154's own record agrees: rivals solvent 88.5% -> 71.5%.
**Intended?** Arming the rivals is the ruling. A third of them failing and 30% of trade with
them was never measured. **A consequence, unruled; a call for Ben** (follow-up 2).

**BL-1125's mechanism.** Reading (a) counts only cargo sold at a market *other than* the source
pool's. A fold makes twin and gravity-near markets one catchment (BL-1125's fold, reach 16, the port gate),
so a short haul between two of them becomes trade inside one market and leaves the reading. The
average cargo per convoy doubles (1.67 -> 3.59 u): the hauls that survive are the long ones.

**BL-1125 intended?** Yes, as the ruling's direct effect: ~30 markets a world is the ruled
shape, and a haul inside a catchment is not inter-market trade. BL-1125's gate accepted haulage
"pending a shipped re-read"; this is that re-read. **No defect.**

What no item recorded is that the density chain had first doubled the reading. So "every item
read a rise or a wash" was true of the items that read it, and the net was still a fall. The two
falls are BL-1154's and BL-1125's, and the first draft lists no haulage reading for either.

## 3. Nation units 414,404 -> 167,110 — the province grain (BL-1130/1133/1141/1150) and BL-1132

Every non-corporate unit is a nation's garrison (`seed_nation_garrisons`,
nation_generation.cpp:1533 at a899bb2a). A nation garrisons its capital province and every
province bordering its highest-grudge neighbour (:1638), each with
`20 + 0.05 x treasury` men, clamped 20-200 (:1564; nation_generation.hpp:244, 247).
So units = garrisons x size, and the steps split cleanly between the two terms.

| commit | item | units (28 + 0) | garrisons | per garrison | land provinces |
|---|---|---:|---:|---:|---:|
| e50ab81c | v0.1.26 | 48,819 | 972 | 50.2 | 6,873 |
| 85fc03a1 | BL-1130 | 43,289 (-11%) | 882 | 49.1 | 4,555 |
| 5cab560b | BL-1133 (+ BL-1120, 1136, 1140) | 38,221 (-12%) | 735 | 52.0 | 2,874 |
| **89d3bb4b** | **BL-1132** | **24,769 (-35%)** | 728 | **34.0** | 2,924 |
| 8cbdbc9d | BL-1141 (+ BL-1086), BL-1137 reverted | 19,063 (-23%) | 567 | 33.6 | 1,917 |
| 572a0afb | BL-1086 fixes, BL-1142, BL-1147 | 20,492 (+7%) | 607 | 33.8 | 1,910 |
| **9ec287f0** | **BL-1150** | **14,283 (-30%)** | 406 | 35.2 | 1,076 |
| d0db9927 | BL-1146 .. BL-1125 | 15,407 (+8%) | 405 | 38.0 | 1,102 |
| 5e44897d | BL-1145 (garrison posts) | 15,407 (0) | 405 | 38.0 | 1,102 |
| a899bb2a | tip | 15,886 | 418 | 38.0 | 1,089 |

- **The count term** falls with the province count: 972 -> 418 garrisons as land provinces fall
  6,873 -> 1,089. BL-1130, BL-1133, BL-1141 and BL-1150 are each a step of it. One garrison per
  bordering province is the rule; there are a sixth as many provinces.
- **The size term** is BL-1132's alone: men per garrison 52.0 -> 34.0 with the garrison count
  flat. Nation treasuries (Pass 7 credits each nation its folded polities' closing chest) fall
  45,625 -> 27,987 (-39%) across that one commit, nations 71 -> 72. Fewer regions, smaller chests.
- BL-1145 moves posts, not counts: identical totals.

**Intended?** Each step is its ruling applied: bigger provinces, sparser regions. But the
garrison rule was written against the old grain, and nothing re-read it. A nation now guards a
border with a sixth of the garrisons, each smaller. **A consequence, unruled; a call for Ben.**

## 4. The second re-bless's untraced moves

### Seed 0's Exploration fingerprint — BL-1171's far binding, then goods by sea

Inside BL-1171's branch (`exploration_sweep --seeds 0,9`), seed 0:

| commit | step | battles | flows | treasury median | living | cross-landmass volume |
|---|---|---:|---:|---:|---:|---:|
| 57c20071, f9de3283 | base; the switches added OFF | 993 | 77 | 8,664 | 75 | 1,331 |
| **ab60a34a** | far realms across water meet by sea and bind where a fleet out-projects | 827 | 75 | **329** | **70** | 1,198 |
| **c1c05c87** | goods between landmasses go by sea (`trade_road_joins_one_landmass`) | 886 | 74 | **162** | 70 | **32** |
| 2a7c26ab, 9ee2422e | road-rule reads | 886 | 74 | 162 | 70 | 32 |
| **3791723d** | meeting by sea gated by fleet out-projection | **899** | **72** | 162 | 70 | 32 |
| aeba4413 | the merge (c5) | 899 | 72 | 162 | 70 | 32 |

**The treasury median is a statistic sitting on a cliff, not a collapse.** Seed 0's 75 capital
treasuries run from 0 to 177.8 M with a gap in the middle: `... 435, 1,056 | 8,664, 11,893,
13,724 ...`. The 38th of 75 was 8,664, the first value above the gap. ab60a34a's far meetings
raise neighbour wars 5 -> 34 and broken treaties 5 -> 66; five fewer polities are alive at 1660
(75 -> 70). The median of 70 is the 36th value, just below the gap (329). The polities around it
barely moved: 1,056, 11,893 and 13,724 are in both lists, and the maximum moves under 0.5%.

The rest is c1c05c87: cross-landmass volume by road 1,198 -> 32, the trade that ran overland
between landmasses. Flows 77 -> 72 and battles 993 -> 899 are the sum of the three steps.

**Intended?** Yes, all three are BL-1171's rulings (Ben, 2026-10-03), and c1c05c87 deletes trade
that crossed water by road. The -98% headline is the fingerprint's statistic. **No defect in the
world; the median is a fragile fingerprint field** (follow-up 4).

### Seed 9 moving the other way — the meeting gate (3791723d)

Seed 9: ab60a34a takes battles 836 -> 819 and treasury median 153 -> 45. 3791723d then takes
battles 819 -> 1,094, conquests 764 -> 1,001, skirmishes 762 -> 1,033, median 45 -> 233. Traced
to that commit. Fields that move with it: treaties standing 85 -> 80, treaty-blocked campaigns
206,764 -> 203,084, navy spend 353,400 -> 263,400, sea campaigns 54 -> 89. **Mechanism inferred,
not traced:** fewer far meetings leave fewer treaties to block campaigns, and less is spent on
navies. The median again sits in the low cluster of a split distribution. Intended (Ben's ruling).

### Road tiers flipping whole networks — the landscape search's road-tier axis

**Traced, and the drafted mechanism was wrong.** It is not `edge_tier`'s percentile gate. It is
the landscape search's ROAD TIER axis (landscape_search.hpp:84, :102): one world-wide floor that
lifts every road tile to the winner's tier (landscape_search.cpp:175-177, 283-285 at 86c8b25c).
Winner 3 makes every road a Highway; winner 2 makes the Tracks Roads and leaves native Highways.

| seed | c3 | c4 (BL-1168) | c5 (BL-1171) | c6 | after (BL-1169) |
|---:|---:|---:|---:|---:|---:|
| 28 | 3 (2,021 Hwy) | **2** (698 Hwy) | **1** | 1 | 1 |
| 31 | 3 | **2** | **3** | 3 | 3 |
| 37 | 3 | **2** | **3** | 3 | 3 |
| 13 | 3 | 3 | **2** | 2 | **3** |
| 41 | 3 | 3 | **2** | 2 | **3** |
| 12 | 1 | 1 | 1 | 1 | **3** |
| 38 | 3 | 3 | 3 | 3 | **1** |

Every flip in the second draft is a change of winner, one to one. Seeds 28, 31 and 37 at BL-1168
move Highway 14,708 -> 6,880 and Road 0 -> 7,829: the draft's -7,831 / +7,843 to within 12 tiles.
**Why it flips:** one tier step is worth +0.02% to +0.2% of the composite (+1.66% once). The
walk has six rounds; each proposes one axis, one step. Whether a world reaches tier 3 depends on
how many rounds the walk spends on the tier axis. When an upstream item changes a placement
proposal's score, the walk takes a different path, and the tier comes out different. **A
sensitivity, not a design movement. A defect in the instrument that decides it** (follow-up 3).

### BL-1168's -35.6% haulage — specialists stop hauling

Class split at c3 -> c4 (haulage_measure with a per-dispatcher tally; the class is the convoy's
corporation at dispatch), pooled over 16 seeds:

| | c3 E | c4 E | c3 Y | c4 Y |
|---|---:|---:|---:|---:|
| specialists (rival, not background) | 4,172.9 (1,171 convoys) | 129.4 (54) | 2,390.2 | 72.9 |
| background firms | 10,266.5 (2,871) | 9,247.6 (2,774) | 9,396.1 | 8,382.7 |
| player / gone | 200.9 | 52.1 | 105.9 | 30.3 |
| **total** | **14,640.2** | **9,429.1** | **11,892.2** | **8,485.8** |
| specialist corporations at the close | 717 | 137 | | |

The specialists' haulage goes from 4,173 u to 129 u: **78% of the step's fall in E, 68% in Y.**
At c4 specialists ship nothing on 9 of 16 seeds and under 25 u on 15. Background firms fall 10% in E with
their count flat (1,671 -> 1,640).

**Traced** to BL-1168 removing most specialists (the seat menu re-anchored at a median of 8).
**Inferred, not traced:** that the survivors ship 4x less each (0.39 convoys each, from 1.63)
because a cheap charter is one with a short trade reach; and why background firms fall 10%.
**Intended?** The specialist count is the ruling. That specialists carried 28% of all
inter-market trade, and that removing them takes it, was never measured. **A consequence,
unruled; a call for Ben** (follow-up 2).

## 5. haulage_measure re-pointed at the router

`haulage_measure`'s default mode priced the nearest-market haul with its own copy of the
pre-BL-1186 arithmetic: the whole unconfined A* path at the sea rate if it touched water, no
node discount, no handling. It now asks `price_market_export_leg` at one unit, the router the
dispatcher and placement ask. A pair the router refuses is no neighbour.

Default run (5 seeds, 20 ticks), before -> after:

| | before | after |
|---|---:|---:|
| nearest-neighbour samples (of 21 markets) | 21 | 17 |
| p10 / median / p90 / max, credits per unit | 0.094 / 0.237 / 0.376 / 0.448 | 0.230 / 0.327 / 0.619 / 0.691 |
| all same-body pairs: samples, median, max | 96, 0.373, 1.865 | 68, 0.669, 1.545 |
| binding ceiling (cheapest good, max haul) | > 1.45 | > 1.69 |

Four markets have no neighbour the router can reach (no overland path and no port pair). The
hauls rise because the router pays handling at each port and routes a dearer overland leg where
the cheap sea path has no ports. The trade section of the run is byte-identical. The ceiling
(10.0) covers the new bound; MARKETS.md § Where the ceiling comes from now carries these
numbers, and its within-tier reading (`ceil > 6`) is the binding one.

## Proposed follow-ups (not filed)

1. **Region capacity reads its ground** (population, BL-1132). A region's carrying capacity is
   its anchor's farm quality only, so spacing regions deletes people rather than spreading them.
   Either capacity scales with the cell a region commands, or 6.27 B is ruled the intended
   world. Ben's call first.
2. **Specialists carry the inter-market trade, and two rulings cut them** (haulage, BL-1154 and
   BL-1168). Arming them took specialists' haulage down 60% and a third of them out; the charter
   re-anchor took the rest (28% of (a) at c3 -> 1%). Background firms do not take the routes
   over. Decide whether that is wanted; if not, the work is on the specialists' solvency or on
   the firms' dispatch, not on either ruling. A reading, then a call.
3. **The landscape road-tier axis decides a whole world's tier on noise** (road flips). Options:
   retire the axis and let `edge_tier` decide alone; demand a minimum composite margin for a tier
   step; or give the axis its own rounds. Defect, priority by how much play reads road tier.
4. **The seed library fingerprint's treasury median sits on a cliff.** Replace it with a robust
   field (the count above a fixed treasury, or the log-median) so a fingerprint move means a
   world move.
5. **The garrison rule against the new province grain** (units). One garrison per bordering
   province at 20-200 men, on a sixth as many provinces. Re-read the rule's scale, or rule the
   167 k forces intended.

## Ben's rulings on the traced movements (2026-10-10)

All five follow-ups were ruled as they stand — none filed:

- **Population 12.19B -> 6.27B (BL-1132 settle spacing): intended.**
- **Specialists stop carrying trade (BL-1154 armed rivals, BL-1168 seat re-anchor): intended.**
  The shelf economy and trade redesign (BL-1265..BL-1270) changes how goods move in any case.
- **Road-tier search axis deciding on noise: leave it.**
- **Garrisons 414k -> 167k (rule unchanged over the new province grain): intended.**
- **Fingerprint treasury median (fragile across a split distribution): leave it.**
