# Sprint 49's one re-bless — the shape, the causes, the pins

**PREPARED 2026-10-07 for Ben's authorisation** against the SHAPE below (DELIVERY.md § The digest
re-bless is one act per WAVE, rule 4), never against the hashes. Ben authorised running it on
2026-10-07; the result is his to authorise before it merges.

## Baseline and method

- **Before** is 1a44b6df, sprint 48's second re-bless. Its `src/` and `scripts/` are byte-identical
  to the parent of the first sprint-49 mover (df6ccfe4^1). Its pins were the ones in force: the
  before side's probe reproduces sprint 48's after column exactly (centres 13,410, markets 436,
  lane tiles 8,645, corporations 2,102, seat menu 182, Empires battles 249,612), which is the
  probe's calibration.
- **After** is main c77f2d5a (sprint 49 closed). Its `src/` and `scripts/` equal dafaa58a's
  (BL-1183 C1, the last mover).
- **One instrument on both trees and every step between:** `tools/verify/rebless_shape_probe`,
  built on a clean checkout of each tree. This lane extended it with the treasuries and the
  building mix by recipe (a second `MIX` line per seed); every added field compiles on both sides.
- **The causes are isolated by checkpoint, not inferred.** The probe ran on a clean checkout of
  every main merge that touched `src/world`, `src/core` or the generation scripts, in merge order
  (17 checkpoints). Two merges were folded into the next: BL-1202 (a UI notice) into BL-1199, and
  the sprint-49 housekeeping merge (stated "no behaviour change") into BL-1203.
- Haulage: `haulage_measure --far-trade --seeds 1 --first-seed S`, every seed, on both trees and
  every checkpoint.

## The headline, in one paragraph

**Two causes reshape the world, and only one of them touches generation.** The lake size cap
(BL-1200) is the sprint's only generation mover: every Empires, Exploration and Industrialisation
counter, every region, centre, province, lane and road length moves at that one merge and nowhere
else. The campaign-start economy then roughly doubles in plant: the density ceiling per good
served (BL-1204) takes every world from 120 to 172 firms, and gap firms digging (BL-1197) plus
chain-feasible placement (BL-1185) broaden the processor mix from a few staples to the whole
chain (alloys, machinery, clean water, propellant, construction). **Trade quadruples**: goods
sold at a destination market rise from 8,497 to 34,984 units in the first play year, the largest
steps being BL-1204 (+51%), BL-1197 (+36%), BL-1187 (+23%) and BL-1209 (+22%). Treasuries rise
everywhere except where shelf k = 1 (BL-1209) bites: it is the one step that takes rival cash down
sharply and leaves 13 rivals negative at the seat.

## The shape, pooled over the 16 curated seeds

Per-seed median in brackets where the spread matters. "Seeds" = how many of 16 moved.

| | before (1a44b6df) | after (c77f2d5a) | seeds |
|---|---|---|---|
| **Empires** battles / conquests / foundings | 249,612 / 210,925 / 7,196 | 239,403 / 204,746 / 6,817 | 16 / 16 / 14 |
| **Exploration** battles / conquests / foundings | 20,357 / 17,567 / 3,561 | 16,268 / 12,384 / 3,094 | 15 |
| Exploration subjections / bought | 128 / 87 | 169 / 103 | 10 |
| Exploration naval battles / sea-leg campaigns / lanes opened / fleets | 874 / 936 / 119 / 762 | 1,400 / 1,796 / 210 / 1,193 | 15 |
| Exploration treaties / across water / far across water | 16,955 / 839 / 378 | 18,270 / 1,285 / 466 | 15 |
| **Industrialisation** battles / conquests / foundings | 7,621 / 6,597 / 1,283 | 7,069 / 5,774 / 854 | 16 |
| Industrialisation sea-leg campaigns / trade legs / lanes opened | 654 / 4,872 / 291 | 1,581 / 7,075 / 464 | 15 |
| Industrialisation treaties across water / far | 2,168 / 1,896 | 3,701 / 3,235 | 16 / 15 |
| cultures with a good preference, 1660 / 1960 | 387 / 376 | 376 / 362 | 13 / 14 |
| **Regions** (all living) | 15,372 | 14,097 (-8.3%) | 15 |
| population on the settlement record | 6.25 B | 6.10 B (-2.4%) | 16 |
| population in centres (centre_pop_k) | 913.1 M | 1,021.8 M (+11.9%) | 16 |
| urban share, per-seed median | 25.6% | 27.4% | 15 |
| **Centres** | 13,410 (median 845) | 13,567 (median 848) | 16 |
| village / town / city / metro | 4,526 / 6,139 / 2,742 / 3 | 4,815 / 5,710 / 3,030 / 12 | 16 |
| **Land provinces** | 12,980 | 13,090 | 15 |
| **Markets** | 436 (median 27.5) | 444 (median 28) | 13 |
| **Road tiles** | 78,662 | 79,987 | 16 |
| Track / Road / Highway | 2,249 / 12,683 / 63,730 | 2,278 / 22,312 / 55,397 | 5 / 10 / 16 |
| **Sea lane tiles** | 8,645 (median 355) | 9,916 (median 434) | 16 |
| **Corporations** | 2,102 | 2,967 | 16 |
| **Firms** | 1,920 (120 every seed) | 2,752 (172 every seed) | 16 |
| **Specialists = seat menu** | 182 (median 8) | 215 (median 9) | 15 |
| seat shortlist | 126 | 179 | 14 |
| **Rivals armed** / rival units | 160 / 23,400 | 280 / 33,250 | 16 |
| non-corporate units (nations' forces) | 160,946 | 173,734 | 16 |
| **Buildings** extraction / processing / military / port | 4,748 / 3,352 / 1,415 / 62 | 6,256 / 7,443 / 1,268 / 53 | 16 |
| under construction at the seat / decommissioned | 6,032 / 2,589 | 6,383 / 1,382 | 16 |
| **Player balance** at the seat (sum) | 39,131 | 55,025 | 16 |
| rival balance, sum / per-seed median of medians | 11.85 M / 4,759 | 16.22 M / 4,882 | 16 |
| rivals with a negative balance | 1 | 13 | 8 |
| **Nation treasuries**, sum / median nation | 257,983 / 83.5 | 347,658 / 136 | 16 |
| **Haulage** (a) sold at destination, window E / Y | 8,497.0 / 8,033.1 u | 34,984.3 / 33,419.1 u (**x4.1 / x4.2**) | 16 |

### The plant: extraction by good, processors by recipe

Extraction sites by target good, pooled: iron ore 851 -> 1,301; petroleum 1,312 -> 850; copper
810 -> 568; fibre 778 -> 1,448; agricultural produce 443 -> 667; **water 20 -> 390** (the Well);
timber 190 -> 383; coal 143 -> 201; silica 56 -> 135; stone 24 -> 67; sand/clay/peat 35 -> 84;
tobacco/spices/coffee/furs 26 -> 80; hides 46 -> 55.

Processors by recipe, pooled. The chain fills in:

| recipe | before | after | | recipe | before | after |
|---|---:|---:|---|---|---:|---:|
| consumer_goods | 756 | 1,091 | | machinery | 18 | 464 |
| food_rations | 550 | 966 | | reinforced_concrete_construction | 31 | 388 |
| iron_frame_construction | 174 | 769 | | medical_supplies | 31 | 374 |
| silicon | 485 | 694 | | alloys | 3 | 327 |
| refined_copper | 289 | 615 | | steel | 94 | 314 |
| ree_alloy | 669 | 468 | | clean_water | 4 | 295 |
| oil_power_plant | 149 | 250 | | propellant_atmospheric | 2 | 236 |
| ordnance | 0 | 69 | | refined_fuel | 5 | 42 |
| steel_frame_construction | 78 | 38 | | electronics / spacecraft_heavy | 0 / 0 | 18 / 12 |

### Per seed, where it matters

- **Firms: 120 -> 172 on every seed.** The density ceiling binds identically everywhere, before
  and after. 172 = floor(7.5 x 23), so every library world serves the same 23 goods in G.
- **Rivals armed:** 46 2 -> 5; 28 10 -> 9; **11 0 -> 14**; 31 2 -> 11; 40 20 -> 25; 12 21 -> 25;
  37 2 -> 12; 13 6 -> 28; 41 13 -> 27; 43 7 -> 9; 32 23 -> 30; 10 5 -> 13; 25 4 -> 15; 38 28 ->
  33; 9 13 -> 17; 0 4 -> 7. Seed 11 has armed rivals again; every seed but 28 rises.
- **Seat menu:** 46 3 -> 6; 28 12 -> 5; 11 1 -> 8; 31 3 -> 12; 40 22 -> 28; 12 23 -> 22; 37 3 ->
  4; 13 8 -> 15; 41 14 -> 14; 43 8 -> 10; 32 24 -> 28; 10 6 -> 5; 25 6 -> 8; 38 29 -> 34; 9 14 ->
  8; 0 6 -> 8. Median 8 -> 9. Four seeds offered three or fewer (46, 11, 31, 37); none does now
  (37's 4 is the floor). Three seeds fall: 28 12 -> 5, 9 14 -> 8, 10 6 -> 5.
- **Empires battles move by up to a half per seed** on a 4% pooled fall: 9 9,467 -> 5,944; 31
  34,221 -> 26,985; 38 4,822 -> 7,443; 43 7,361 -> 8,951; 10 20,944 -> 17,314. **Seed 12 is
  untouched by the lake cap** (4,983 -> 4,982; Exploration 140 -> 140; regions 762 -> 762).
- **Exploration battles:** 41 207 -> 47; 9 1,094 -> 303; 25 2,371 -> 1,087; 32 1,918 -> 1,127;
  38 153 -> 514; 0 899 -> 1,333.
- **Sea lane tiles:** 41 83 -> 720; 13 385 -> 883; 11 4 -> 208; 40 1,186 -> 721; 38 794 -> 492;
  43 135 -> 32.
- **Highway tiles flip whole networks**: 46 6,038 -> 2,530; 41 5,307 -> 1,382; 9 4,315 -> 1,149;
  13 5,911 -> 2,573 (to Road); 38 926 -> 3,749; 28 691 -> 1,965 (to Highway). See UNATTRIBUTED.
- **Metropolises 3 -> 12:** 46 0 -> 4; 13 0 -> 5; 0 0 -> 1; 11 1 -> 0.
- **Rivals negative at the seat:** 28 2, 10 2, 13 2, 38 2, 31/40/37/32/12 1 each (was 12 only).

### Haulage

Units of cargo sold at a destination market other than the source pool's (reading (a)), window E
the first play year, window Y the year after; every run 0 failures. The before column reproduces
sprint 48's after column exactly (8,497.0 / 8,033.1). **Trade is four times what it was.**

| seed | before E | after E | before Y | after Y | convoys E |
|---:|---:|---:|---:|---:|---:|
| 46 | 685.2 | 2,209.2 | 746.8 | 2,133.0 | 220 -> 708 |
| 28 | 284.4 | 1,423.5 | 282.8 | 1,081.2 | 94 -> 418 |
| 11 | 582.6 | 2,218.3 | 674.1 | 2,327.9 | 177 -> 666 |
| 31 | 564.0 | 2,276.8 | 371.8 | 2,212.2 | 151 -> 597 |
| 40 | 692.3 | 1,807.9 | 624.9 | 1,639.2 | 173 -> 625 |
| 12 | 269.7 | 1,990.1 | 301.7 | 1,955.9 | 118 -> 537 |
| 37 | 979.0 | 2,085.6 | 819.7 | 2,215.0 | 254 -> 579 |
| 13 | 487.3 | 2,002.1 | 318.1 | 1,745.7 | 187 -> 575 |
| 41 | 784.6 | 2,149.7 | 705.5 | 1,703.8 | 211 -> 578 |
| 43 | 788.2 | 1,412.9 | 748.7 | 1,499.8 | 161 -> 284 |
| 32 | 138.9 | 2,247.1 | 117.5 | 2,219.3 | 68 -> 594 |
| 10 | 349.4 | 2,943.6 | 321.4 | 2,655.1 | 182 -> 819 |
| 25 | 630.4 | 2,401.5 | 537.9 | 2,350.4 | 239 -> 869 |
| 38 | 387.1 | 2,782.7 | 493.5 | 2,529.6 | 117 -> 657 |
| 9 | 438.0 | 2,372.6 | 589.8 | 2,291.8 | 120 -> 623 |
| 0 | 435.9 | 2,660.7 | 378.9 | 2,859.2 | 114 -> 593 |
| **pooled** | **8,497.0** | **34,984.3** (+311.7%) | **8,033.1** | **33,419.1** (+316.0%) | 2,586 -> 9,722 |

Pooled by checkpoint, window E / Y (step change in E): before 8,497.0 / 8,033.1; BL-1186 9,241.1 /
8,230.9 (+8.8%); BL-1187 11,396.5 / 10,346.6 (+23.3%); BL-1185/1188 12,488.2 / 11,015.6 (+9.6%);
BL-1198 identical to the decimal on all 16 seeds; BL-1200 14,073.1 / 12,934.9 (+12.7%); BL-1201
16,255.4 / 15,095.5 (+15.5%); **BL-1197 22,155.4 / 21,294.1 (+36.3%)**; households 21,775.3 /
19,988.5 (-1.7%); **BL-1204 32,813.0 / 31,276.6 (+50.7%)**; BL-1191 +0.1%; BL-1194 +0.4% (4
seeds); BL-1183 C4 +0.1%; BL-1199, BL-1208 identical; **BL-1203 (with housekeeping) 28,773.9 /
27,951.6 (-12.8%, while convoys rise 6,604 -> 8,250)**; BL-1206 -0.8%; **BL-1209 34,859.7 /
33,546.0 (+22.1%)**; BL-1183 C1 34,984.3 / 33,419.1 (+0.4%).

## The causes, step by step

Each row is one main merge, measured by the probe against the merge before it. "=" means no
pooled field moved on any seed. Steps sum to the combined movement.

| step | generation (spans, regions, centres, lanes) | firms / corps | processors | extraction | other notable |
|---|---|---|---|---|---|
| BL-1186 goods cross markets (df6ccfe4) | = | = | +15 | -35 | rival cash -69 k |
| BL-1187 build only what runs (737d5d28) | = | = | +355 | +67 | decommissioned -315, military -105; rival cash +719 k |
| BL-1188 seat kit + **BL-1185 chain-feasible** (02da0166) | = | -89 firms (7 seeds) | +246 | -188 (iron +128, petroleum -139, copper -164) | decommissioned -464; highway flips on 3 seeds; rivals armed +8 |
| BL-1198 the Well (c39d2989) | = | = | +20 | +1 (water +33) | decommissioned -22 |
| **BL-1200 lake cap** (8277706d) | **all of it**: Empires -10,209 battles (16 seeds), Exploration -4,089 (15), Industrialisation -552 (16); regions -1,275; centres +157; cities +474; metros +9; provinces +110; markets +9; lane tiles +1,271; road tiles +1,333; settlement pop -147 M; centre pop +103 M | corps +29, specialists +35 | +136 | +90 | rivals armed +28, rival units +4,800, nations' units +12,788 |
| BL-1201 orders are floors (7ab91754) | = | = | -92 | +111 | under construction -153 |
| **BL-1197 gap firms dig** (a9823674) | = | +95 firms | +898 | +515 (water +96, fibre +218, produce +83, timber +79) | decommissioned -575; nation treasuries +33 k; highway flips on 2 seeds (-7,384) |
| BL-1163/1196/1179 **households consume** (90640c82) | cities -425 (16 seeds) | = | -83 | +16 | rival cash -269 k |
| **BL-1204 density per good** (+ BL-1205, decisions byte-identical) (7aae16de) | cities +127 | **+832 firms (all 16)** | **+1,902** | **+1,900** | military +580; under construction +2,335; rival cash +5.12 M; nation treasuries +45 k; highway flips on 5 seeds |
| BL-1191 upkeep x 0.25 (d54ccf20) | = | = | +30 | -26 | player cash +6.8 k |
| BL-1194 sea cheaper (53a1a740) | = | = | -5 (2 seeds) | -9 (2 seeds) | negligible at the 12-tick settle |
| BL-1183 C4 idle floor (85ec3660) | = | = | +5 | -4 | rival cash +282 k |
| BL-1202 + **BL-1199 wharf yields** (ea697134) | = | = | = | = | **moves nothing** |
| **BL-1208 wharves in generation** (fe25017c) | = | = | = | = | **moves nothing** (its own record: no change on shipped seeds) |
| housekeeping + BL-1203 water reaches dry markets (1cfbca9e) | cities -66 | = | -25 | +1 | rivals armed +19 (3 seeds) |
| **BL-1206 seat clean slate** (bf643960) | = | = | -56 | +3 | player cash +3.7 k; food_rations +171, steel -207 |
| **BL-1209 shelf k = 1** (d7490620) | cities +221 | = | +686 (alloys +255, machinery +308, clean water +147) | -158 | **rival cash -1.96 M; rivals negative +13**; rivals armed +59; under construction -546; military -158 |
| **BL-1183 C1 no unobtainable sites** (dafaa58a) | = | = | +59 | **-776 (petroleum -744, copper -454, water +144)** | under construction -1,226; military -564 |

**The digest pins, by step** (the items' own records, measured on seeds 0/43/10 in their lanes, not
re-measured here per merge): BL-1200 moves D_search on; every other mover from D_settle or D_land
on. The probe's generation fields agree: nothing but BL-1200 moves a field read before the
landscape search.

## UNATTRIBUTED, or attributed without a traced mechanism

- **Road tiers flip whole networks.** Highway <-> Road moves wholesale on single seeds at four
  steps: BL-1185 (3 seeds), BL-1197 (2), BL-1204 (5) and BL-1200. The first three change road
  length by 8 tiles or none, so the flip is a tier choice, not new road. Net
  Highway 63,730 -> 55,397, Road 12,683 -> 22,312. Same sensitivity sprint 48 recorded: a small
  upstream change re-ranks the landscape search's road-tier choice or a nation's qualification
  percentile and flips one tree. Inferred, not traced.
- **Centre population rises 11.9% while the settlement record falls 2.4%**, both at BL-1200.
  The step is measured; why re-classifying lakes as sea concentrates people into centres is not
  traced.
- **Empires battles swing by up to +/-50% per seed** on a 4% pooled move, all at BL-1200. A
  400-year sim re-run on a different water map is chaotic; the per-seed swings are the expected
  sensitivity, not a design movement. Seed 12 is the control: its water map barely moved.
- **Cities move at four campaign steps** (households -425, BL-1204 +127, BL-1203 -66, BL-1209
  +221). The settle's growth gate reads per-market met ratios (BL-1163), so any change to supply
  moves centre scale; inferred from BL-1163's design, not traced per step.
- **BL-1203 cuts goods sold at destination by 12.8% while dispatching 25% more convoys** (6,604
  -> 8,250 in window E). Its design sends water to dry markets at the landed price; why more
  convoys sell less is not traced. The housekeeping merge is folded into this step but is stated
  behaviour-free.
- **BL-1198 (the Well) moves the probe but not haulage**: haulage is identical to the decimal on
  all 16 seeds across its merge, while the probe reads water sites +33. Not traced.
- **BL-1209's rival cash fall (-1.96 M) and 13 negative rivals.** Measured at the step; the
  mechanism (the shelf counting as supply cuts what rivals sell in the settle) is inferred.
- **The seed library's "why" lines no longer describe some seeds** (fingerprints below): 43 "the
  small rich world" now has a median chest of 105 (was 21.8 M); 41 "colonial ties without the
  capital" has 9,519 (was 159); 38 "insular rich" fights 54% more. All BL-1200. A call for Ben,
  not this lane.

## The headline claims and the check that asserts each

| claim | committed check | gate or reading |
|---|---|---|
| enclosed water of 150+ tiles is sea (BL-1200) | lake_census (reading); the cap's value is validated on load | **GAP: no gate asserts the classification** |
| the density ceiling is 7.5 firms a good served (BL-1204) | density_census (reading); player_seed_sweep's charter-rule check compares the ceiling on its cost rows | **partial: no gate on the shipped spend** |
| build only what runs (BL-1187) | build_only_what_runs | gate |
| the Well / gap firms dig (BL-1198/1197) | well_gate | gate |
| a processor lands only where its inputs reach (BL-1185) | charter_refusal_probe, player_seed_sweep chain column | reading + row audit |
| shelf k = 1, pro-rata short shelves (BL-1209) | shelf_share | gate |
| the seat's clean slate (BL-1206) | seat_clean_slate | gate |
| a site under construction pays the idle floor (BL-1183 C4) | site_idle_floor | gate |
| the shape table | rebless_shape_probe | probe |
| haulage | haulage_measure --far-trade | reading only |

Two headline claims have no committed gate: **the lake cap** (the only generation mover) and **the
density ceiling's per-good rate** (the largest economy mover). DELIVERY.md asks for one BEFORE the
re-bless. Both are flagged; neither is written by this lane.

## Pins, old -> new

- **player_seed_sweep `--digest-check`, shipped and legacy**: all 16 rows of both tables moved,
  every digest from D_search on. Tables below. Post-pin: 16/16 PASS on both arcs.
- **exploration_sim_harness R3b and R3d.1**: 239/150/134, subjections 0, freed 0, tribute 0,
  treaties 496, broken 4, owner changes 864 -> R3b 86/83/112, 2, 0, 2,481,129, 359, 1, 778 and
  R3d.1 the same with tribute 747,063. **One cause, measured**: the harness passes on the old pins
  built at c39d2989 and reads exactly the new values built at 8277706d (BL-1200). Post-pin: ALL
  PASS.
- **world_determinism** (byte-identity, not pinned): ALL PASS on both trees. Digests seedA/on
  0CDD52D79FDA6086 -> 6244EC4CAD3871C2; seedB/on A32FBF7D3844B434 -> 6C49522501D8F8B2; seedA/off
  68E2A3663570FB3E -> AC7AF48B9FD2EA83 (the prehistory-off world moves too: the lake cap is
  terrain).
- **Seed library fingerprints: all 16 moved**, all through BL-1200. `seed_library_sweep.json`
  re-swept here (committed). **`--bless` was refused to this lane** (the permission classifier);
  the main session runs `node tools/session/seed_library.js --bless` after authorisation. The
  moves (Empires battles, Exploration battles, flows, treasury median, living polities):
  46 19,841 -> 17,962, 1,790 -> 1,057, 44 -> 56, 8.35 M -> 3.67 M, 78 -> 85;
  28 5,114 -> 6,455, 350 -> 291, 29 -> 33, 241 -> 156, 32 -> 41;
  11 26,050 -> 28,371, 2,126 -> 2,302, 98 -> 97, 274 -> 197, 110 -> 115;
  31 34,221 -> 26,985, 3,504 -> 2,791, 109 -> 99, 226 -> 197, 116 -> 105;
  40 10,184 -> 10,805, 933 -> 1,123, 59 -> 43, 221 -> 479, 53 -> 52;
  12 4,983 -> 4,982, unchanged, unchanged, 136 -> 120, 42 -> 43;
  37 20,112 -> 20,144, 1,507 -> 1,237, 72 -> 77, 102 -> 137, 75 -> 77;
  13 18,772 -> 19,428, 985 -> 1,307, 73 -> 87, 336 -> 213, 73 -> 81;
  41 10,367 -> 8,657, 207 -> 47, 53 -> 57, 159 -> 9,519, unchanged;
  43 7,361 -> 8,951, 581 -> 328, 49 -> 57, 21.8 M -> 105, 31 -> 46;
  32 14,867 -> 16,469, 1,918 -> 1,127, 35 -> 55, 411 -> 182, 62 -> 77;
  10 20,944 -> 17,314, 1,799 -> 1,281, 60 -> 65, 106 -> 276, unchanged;
  25 25,795 -> 24,258, 2,371 -> 1,087, 84 -> 99, 303 -> 4,607, 82 -> 84;
  38 4,822 -> 7,443, 153 -> 514, 29 -> 34, 196 -> 303, 29 -> 47;
  9 9,467 -> 5,944, 1,094 -> 303, 46 -> 40, 233 -> 157, 52 -> 50;
  0 16,712 -> 15,235, 899 -> 1,333, 72 -> 65, 162 -> 432, 70 -> 68.
- **Unchanged reds, identical verdicts on both trees**: history_sim_harness R3a2/R3a3.
- **Not refreshed** (not pins; stale since BL-1200 moved every span): `history_sweep.json`,
  `exploration_sweep.json`, `exploration_sweep_1960.json`. Visual goldens untouched (curated set
  only; this lane builds no app).

#### player_seed_sweep, shipped arc (D_search / D_land / D_settle / D_seat)

| seed | old | new |
|---:|---|---|
| 46 | 34F2560905FFFCEB 6AAA5EDFAEF1CC4F CC78E91673F77299 C490CA6D45088DBF | 447A1E86FC2FA441 A063F01DCEE5CCBD 9B67089BEC4FEB9A 70F09094E9DACEC0 |
| 28 | B946CCD77067FCB0 B931B36F6BDAADA2 79730152B5DF08F0 65DD23B5CA186BBE | 876D419405ED40C3 7B114F30EF9442C8 3FE193B19D45CD41 2873CC6AB7E59ECD |
| 11 | 8B214357D752EF9F 35B95760389162D3 C88655EFAC6F804C 8C5F017A4E3FB44E | 00297EE71A9C4AD8 B57B6FE05F4CA85E 696F162B519F8476 59D0BB1BCD925157 |
| 31 | 48C2F80E652AD0BE 2FEEE52FA7550BDC A62B4A5CF1E58857 C1E3C295572D03EE | 76BE58A2272C98FC E5F5BC29D381DA57 D2C0B66B1D8006EF 7EA4EFD9D12B6251 |
| 40 | 9F518F9C3A3A676A A25F56E6A4BB684C 1B190E2C86263750 19870527DBBBCA93 | 9208A632B3CAD766 A0D72CB637CB7FDD 7619E371E5B53B51 70471A466ECA279B |
| 12 | 0B8C7AE7A903A3D6 7DB6282732DF636E 962C9E070F7C34D9 6C60488E069619AA | 18A5741AB8504230 4DE1C6A2594D33EF 8C4623D450F1F536 9664B20733E9359F |
| 37 | 06B39FF3A0CE6D12 CCBBEF58479254E7 CAE1EC9A0FC0AA44 F88425B03E8111B0 | 117C377EA9E31DE2 DC58DE35F15B8E3B D2D4D5B6185B3192 55A94AE4DC9AB84B |
| 13 | 94802500676E33AA F90535822F236D6E 0EC6446D1598005C 648C1746F6B241F4 | 320F15EFA010E9B4 FAB4CD5842319A91 0928BC574ED6D3E0 24CBFB265549FD88 |
| 41 | 543F6539820F88A3 E8F9BCED09684F38 7F4706920E2DA870 A837AFFC36BB1C09 | D14DF4409D806F07 EA417F7927CE8CA1 0E47B3CA66F3383F 900DE56D25D1D461 |
| 43 | FAEFD54CDAF75337 A710A1CBB06665BD 25FA4DB2E0216EE9 44C32FA17F03C937 | 11411BC159733D0B 9EF416685AAE20A0 8894484311E63B66 C813327BF358EFC9 |
| 32 | 2077BFB3B3A00BFA 465E25806F47DB4F B72623095032DE33 5D1E2671DC5BE1C1 | C536B6E1F9BDA013 D8719F504BC32D86 4963D79FC29B5AF9 C6FEBE72FA2FA51A |
| 10 | D6CFA0CF18873E35 0EFAA3C393BA487B 7DFEEE88B178FA69 27A1C0BB0F550BAD | 0F4407C70B4DF54B 89ECFE6D59DD0EF4 2CFE162D6B8B1C62 4570671D135F16B6 |
| 25 | 46486AD6E44C5DCB EA42931E03F76B84 77E31977258700FB 8269E6EFFADB2395 | 33A1A21D99F399B2 E9F8530ECAA59E79 BED51C01DA829370 25E74197CD2F134A |
| 38 | 4F6FC306DFCF2145 33F9B0D74CD39432 2A72FB3D95483D17 4C0A374EA083875B | 0842AD7CB1C46C88 02D84FBB1960C63F 56FA72573A7AD9F5 D0FE83DA18F683C2 |
| 9 | A4F8A159EABD95EE 3FA66D0EA2086E24 6F3EAEED7E474880 3BA449CA80DB1775 | C7FC8A2410FE181F 298489BC06ACC751 34B00A511A13DAAD D9F4A6171224D823 |
| 0 | C161F5F9B300B5C7 812A4D7C4E67EC04 6CADE285258EF691 1C62BD83A29D77B6 | C8964E73F6BAC81B E22562E3C4EAAD26 8A6F5A96D6A3795D 383572BE156A317B |

#### player_seed_sweep, legacy arc (D_search / D_land / D_settle / D_seat)

| seed | old | new |
|---:|---|---|
| 46 | A49E43848C16DA35 32AD7CD27862AA16 88C9472BCF8BD3C9 5670AEFEE01B241F | 99B20B3C6471638E 9F23835060D6CB06 455C25076561A790 8FBAB8D732E218D5 |
| 28 | D6DC11ABCD55681B A7BE5CB2AF82D398 CE0F9D914EB74996 38C8B9737EEB016B | 1EFCD95FE3895DBF E71300727201D677 43C621C42EE3271B 39C2B4893B51E82A |
| 11 | 25BFEBEA08464A81 D09AF6EAD0DF3305 D1A2D13238F2C66E 7D02922FBC7FFD6D | FB6424C90C5972DD DBB8076A6F5BC69A E71133211D684A04 C39D7005E176B578 |
| 31 | 3D78CAD4E0A3FA0B 179E2C9AA3966B32 83F5C08BA8F63E39 39EC4E4B7B244C33 | 74FE029BE89D146E 1A08D918D489053A 95251B6FE8704D80 4FE7B921C2A3858F |
| 40 | 57762D5165544B1C C4D959FD27DFEC99 BAF66EB09EFBC7AA 4E45801DCD3FE479 | 5452676FAF2AC979 98363380358CC5F2 2C94EDA4C86E7F80 DE01AAB5D43805C5 |
| 12 | 699921965CFBBF2C C6C65AAE1836FEAF 4BEA2A9E46C92208 418E63514703BEB7 | 7D3A9131228C5447 EE91397B8FAF2691 7E30725B21569028 91B50799C205725C |
| 37 | CCE7CB2BBC26F19C 8A9F1C32D72A96E7 A4DD3D3C92BFEE55 7AE5C12D387A4B6F | 6B74B416CF11B2E7 CC3D13E5C3351F2B 0E2591ED42220ACD D86655D212C2010C |
| 13 | B8569827D2340A15 3BA624D1C6384463 DB3992E1F0A31000 00E6CFBD3E149FA2 | 62EA922DB948C1D1 E914EF28C7A04EB8 2B30AE5ECCE5F546 A691D8C22B98D898 |
| 41 | B60ACAB506456D8D 09FDCF066B4A4CC2 3558491A99B7B118 0C4A3A06C04D6A37 | 741C2B7AC7620075 ECFE4BE5E812C8C9 0E7DFD7D7A7E9003 0A4867E97095DB6F |
| 43 | E94AEE911DFCEE24 60B2A8437C3FF979 DD2E9F27042EC7E0 4C788BC5DC842A19 | D485E514436FC786 D0D730522E621FB3 961E98C0296C0E1D 4AEA2C6DFBE01185 |
| 32 | 8B3A4FB246FB9538 451313D3995C4BD6 918559433436DA0E CED19B7DFE3E7AE1 | 0895DA418FB7B07D 4633FAD27EBD92D4 BCA3389F93E8F4B1 FEC52B768A0FFD10 |
| 10 | 00E6B5139CA88598 793EE259BA588ECC F9C272F3211233FD 011132288BF69287 | C7B8CFBCEDBC9B4E 08B86D9A796ADC90 AB86E45167836328 BF3C45C13116AA76 |
| 25 | 8425A84A9F0EF5C7 4480190D102EF70F D2FD5DFBA0F78744 AB8F1B80E0CC767C | 6E0CCB13CA9EF879 9D05CA13D1ADF2AB 50C3A24ECD65009C 84E1F25F72674237 |
| 38 | 7CF462189775B8AD DB85D77BA6BE755C 9DE0DB4271E3CA21 5017794B129A0E87 | DF4B4578E1B02ADF 2D9F257DC7C7C793 A86EBE57A22D309D 3E05C240744C9B70 |
| 9 | 8759B4A4B9836475 2A75D2CA9DF68502 CEF48A28907D1FD9 7DEEE7636002F0BD | 5E71BFBFE5B35706 AD76B7051D97C4E6 CC3A31D59FFC6430 0284969236D60376 |
| 0 | 802AF6CE63047E13 D64AF3A739B65EC8 BBF9407242D8F2B7 0E6A8E93293437DA | C43566CEFAAF8F39 58940249CE0D9132 0F672ECBEFF3B6C0 5935E03F97BC4B46 |
