# Sprint 48's second re-bless — the shape, the causes, the pins

**AUTHORISED by Ben, 2026-10-04** ("re-bless now"), against the SHAPE below (DELIVERY.md § The
digest re-bless is one act per WAVE, rule 4), never against the hashes. Prepared the same day on
the re-bless lane's branch, on main 9171c2ef; re-checked after merging main b856c4d9, which moved
no `src/world` file and no generation script (`git diff --stat 9171c2ef HEAD -- src/world
scripts/*.lua` is empty).

**Follow-ups, filed or in flight:**
- **BL-1183 (the fair-price ceiling stalls construction)**, filed for sprint 49: Ben re-blessed
  first and the fix follows. It is the campaign-economy side of BL-1172 that this shape reads only
  through the settle digests and the spawn_solvency red.
- **acquisition_viability R1** (the seated corporation accumulates) is being re-windowed,
  harness-only. The red attributed to BL-1173 below is a gate, not a pin; re-windowing it moves no
  digest.
- **spawn_solvency R4 is GREEN on the merged tree.** Main's harness-only fix counts hires by
  provenance (seeded survivors + hires = rival units at close). Re-run here: 152 seeded, 152
  standing, 304 hired, ALL PASS. The "-732 hired" below was the old count's artefact, not a field
  that stopped hiring.

**Re-checked on the merged tree (main b856c4d9 merged in):** player_seed_sweep `--digest-check`
16/16 PASS on both arcs; world_determinism ALL PASS with the digests below unchanged;
spawn_solvency ALL PASS.

## Baseline

- **Before** is 179615b8, the merge of sprint 48's first re-bless, measured from a clean export of
  that tree. Its pins are the ones the first re-bless blessed (no pin row changed on main since).
  A shipped-arc `--digest-check --seeds 28,0` built from 179615b8 passes both rows.
- **After** is main 9171c2ef. Its `src/` and `scripts/` equal b427aed0's (BL-1169 at K = 1000);
  the three commits after it are docs.
- The combined before/after is ONE instrument run on both trees:
  `tools/verify/rebless_shape_probe`, unchanged since the first re-bless. Its before side
  reproduces the first draft's after column exactly (centres 13,437, markets 429, lane tiles
  12,975, haulage 14,433.9 / 11,916.5), which is the probe's calibration.
- **The causes were isolated by checkpoint**, not inferred. The same probe, the same
  `player_seed_sweep --digest-check` (both arcs) and the same haulage run on a clean export of
  every main commit that merged a mover, in merge order:

  | step | tree | what it adds |
  |---|---|---|
  | c0 | 179615b8 | before |
  | c2 | 314e972b | sea lanes merge into shared trunks (reuse 500); curved roads already in (2c9235a9) |
  | c2b | a339437f | BL-1169 merged OFF (K = 0) |
  | c3 | 5c18bf8c | bridges span at most two water tiles |
  | c4 | 86c8b25c | BL-1168 charter priced by trade reach |
  | c5 | aeba4413 | BL-1171 far trade, the meeting gate, trunks only with the current |
  | c6 | 4df6f469 | BL-1172 fair-price army + BL-1173 working capital |
  | after | 9171c2ef | BL-1169 at K = 1000 |

  Plus one knockout: **m1173** is 9171c2ef with `k_background_working_capital_of_stock` set to 0
  (the pre-BL-1173 opening cash), to separate BL-1173 from BL-1172.
- Haulage: `haulage_measure --far-trade --seeds 1 --first-seed S`, one run per seed, every tree.

## The shape, pooled over the 16 curated seeds

Per-seed median in brackets where the spread matters.

| | before (179615b8) | after (9171c2ef) |
|---|---|---|
| **Centres** | 13,437 (median 830) | 13,410 (median 845; 357-1,136) |
| scale mix village / town / city / metro | 4,536 / 6,202 / 2,696 / 3 | 4,526 / 6,139 / 2,742 / 3 |
| **Regions** (all living) | 15,436 | 15,372 |
| population on the settlement record | 6.27 B | 6.25 B |
| **Urban share** (per-seed median) | 25.7% | 25.6% |
| **Land provinces** | 13,007 | 12,980 |
| **Markets** | 429 (median 28; 17-33) | 436 (median 27.5; 18-35) |
| **Road tiles** | 79,152 | 78,662 |
| Track / Road / Highway tiles | 2,287 / 11,537 / 65,328 | 2,249 / 12,683 / 63,730 |
| **Sea lane tiles stamped** | 12,975 (median 568) | 8,645 (median 355) |
| **Corporations** | 3,134 | 2,102 |
| **Specialists = seat menu** | 1,215 (median 78.5; 29-114) | 182 (median 8; 1-29) |
| **Rivals armed** / rival units | 1,195 / 173,750 | 160 / 23,400 |
| firms | 1,919 | 1,920 |
| non-corporate units (nations' forces) | 167,110 | 160,946 |
| **Empires** battles / conquests / foundings | 249,612 / 210,925 / 7,196 | unchanged |
| **Exploration** battles / conquests / foundings | 20,034 / 17,146 / 3,652 | 20,357 / 17,567 / 3,561 |
| Exploration subjections / provinces bought | 105 / 82 | 128 / 87 |
| Exploration sea legs: campaign / purchase / tribute / trade | 893 / 82 / 2,453 / 637 | 936 / 87 / 2,427 / 1,700 |
| Exploration lanes opened | 79 | 119 |
| Exploration treaties across water / far across water | 464 / 0 | 839 / 378 |
| Exploration contacts met by sea | 0 | 234 |
| fleets opened, Exploration | 762 | 762 |
| **Industrialisation** battles / conquests / foundings | 8,073 / 6,751 / 1,256 | 7,621 / 6,597 / 1,283 |
| Industrialisation subjections / bought | 70 / 45 | 30 / 8 |
| Industrialisation sea legs: campaign / purchase / tribute / trade | 781 / 45 / 2,307 / 5,220 | 654 / 8 / 2,078 / 4,872 |
| Industrialisation lanes opened | 312 | 291 |
| Industrialisation treaties across water / far | 1,782 / 1,508 | 2,168 / 1,896 |
| Industrialisation contacts met by sea | 446 | 358 |
| industry points at the close (industry_concentration) | 997.96 M (at c6) | 705.55 M (-29.3%) |
| far rival >= 25% of the leader (industry_concentration) | 12 of 16 (at c6) | 14 of 16 |
| cultures with a good preference, 1660 / 1960 | 389 / 376 | 387 / 376 |
| **Haulage** (a) sold at destination, window E / Y | 14,433.9 / 11,916.5 u | 8,497.0 / 8,033.1 u (**-41.1% / -32.6%**) |

### Per seed, where it matters

- Seat menu (= specialists): 46 29 -> 3; 28 52 -> 12; 11 83 -> 1; 31 61 -> 3; 40 70 -> 22; 12 89
  -> 23; 37 78 -> 3; 13 76 -> 8; 41 79 -> 14; 43 86 -> 8; 32 89 -> 24; 10 86 -> 6; 25 60 -> 6; 38
  72 -> 29; 9 114 -> 14; 0 91 -> 6. Every seed falls; five seeds offer three or fewer.
- Rivals armed: 46 28 -> 2; 28 51 -> 10; **11 82 -> 0**; 31 61 -> 2; 40 69 -> 20; 12 88 -> 21; 37
  76 -> 2; 13 75 -> 6; 41 78 -> 13; 43 83 -> 7; 32 88 -> 23; 10 84 -> 5; 25 59 -> 4; 38 71 -> 28; 9
  113 -> 13; 0 89 -> 4. The first re-bless's "rivals armed" headline (2 -> 1,195) is mostly undone:
  seed 11 has no armed rival again.
- Sea lane tiles: 41 691 -> 83; 37 364 -> 171; 12 2,256 -> 1,467; 40 1,678 -> 1,186; 28 rises
  257 -> 324. 11, 43, 10 and 25 are within one tile.
- Exploration contacts met by sea (0 before on every seed): 0 79, 32 53, 12 25, 40 18, 13 16;
  11, 41, 43, 10, 25 stay at 0.
- Industrialisation subjections: 41 31 -> 3; 31 8 -> 1; 46 7 -> 5; 25 3 -> 1; 40 19 -> 18.
- Highway tiles flip whole networks per seed: 28 2,017 -> 691 and 38 3,688 -> 926 (to Road); 12
  1,071 -> 4,097 (Road to Highway). See UNATTRIBUTED.

### Haulage

Units of cargo sold at a destination market other than the source pool's (reading (a)), window E
the first play year, window Y the year after; every run 0 failures.

| seed | before E | after E | before Y | after Y |
|---:|---:|---:|---:|---:|
| 46 | 1006.3 | 685.2 | 770.2 | 746.8 |
| 28 | 376.4 | 284.4 | 251.4 | 282.8 |
| 11 | 734.9 | 582.6 | 625.8 | 674.1 |
| 31 | 881.2 | 564 | 877.6 | 371.8 |
| 40 | 964.8 | 692.3 | 796.9 | 624.9 |
| 12 | 365.8 | 269.7 | 286.4 | 301.7 |
| 37 | 917.6 | 979 | 704.2 | 819.7 |
| 13 | 1315.5 | 487.3 | 1009.1 | 318.1 |
| 41 | 1073.2 | 784.6 | 796.2 | 705.5 |
| 43 | 1832.8 | 788.2 | 1397.4 | 748.7 |
| 32 | 185 | 138.9 | 254.6 | 117.5 |
| 10 | 550.3 | 349.4 | 705.7 | 321.4 |
| 25 | 778.5 | 630.4 | 803.7 | 537.9 |
| 38 | 656.5 | 387.1 | 413.9 | 493.5 |
| 9 | 1519.9 | 438 | 1136.4 | 589.8 |
| 0 | 1275.2 | 435.9 | 1087 | 378.9 |
| **pooled** | **14433.9** | **8497.0** (-41.1%) | **11916.5** | **8033.1** (-32.6%) |

Pooled by checkpoint, window E / Y: c0 14,433.9 / 11,916.5; c2 14,593.0 / 11,975.1; c3 14,640.2 /
11,892.2; c4 9,429.1 / 8,485.8; c5 8,218.4 / 7,394.2; c6 7,941.9 / 7,360.6; after 8,497.0 /
8,033.1; m1173 7,001.3 / 7,785.1.

## The causes, each with its own record

Each cause carries two records: the item's own (its commit body, measured in its own lane) and
this lane's checkpoint step (the probe, the digests and haulage across that one merge). Steps sum
to the combined movement; item records do not.

**Curved roads (2c9235a9) — moves no digest, by construction.** It changes `src/ui/`,
`src/core/verify_api.cpp` and `scripts/verify/route_curves.lua` only; no `src/world/` file and no
generation script. Nothing the probe or a digest reads can move. Not separately run. The
icon_silhouettes goldens DID move (~2.2%, per the handoff): BL-1167's, not touched here.

**Sea lanes merge into shared trunks (314e972b, reuse 500).**
- Own record: distinct lane tiles a seed 811 -> 439 at 500; lane pairs at a shared port share
  47.5 tiles out from it.
- Step c0 -> c2: lane tiles 12,975 -> 7,027 (-46%); unchanged on 11, 43, 10, 25. Seat shortlist
  982 -> 978; on 32 and 38 the specialist count moves by 2 (sums equal). Haulage +1.1% / +0.5%.
- Digests: shipped 12 rows from D_search (the four unchanged-lane seeds hold); legacy 4 rows.

**BL-1169 merged off, K = 0 (a339437f).** Step c2 -> c2b: no probe field and no digest row moves,
on either arc. Its identity claim holds.

**Bridges span at most two water tiles (5c18bf8c / 193bafed).**
- Own record: kMaxCrossingTiles 3 -> 2 on every road writer; road_generation_harness R7.
- Step c2b -> c3: road tiles 79,152 -> 78,815 (-337; Highway -299, Road -36, Track -2); markets
  429 -> 430 (seed 11); one more firm and one more corporation; rival units +150. Haulage +0.3% /
  -0.7%.
- Digests: all 16 rows from D_search on both arcs.
- acquisition_viability R1 accumulating seeds 5 -> 4 of 8 (still a pass at this step).

**BL-1168 charter priced by trade reach (44 charters, the anchor's reach, works notes by reach).**
- Own record: at 44 charters the median library world affords 8 specialists (the no-budget
  anchor); "almost no heartland centre affords a seat"; the live tick reads x1.12 the legacy world.
- Step c3 -> c4: specialists = seat menu 1,215 -> 190 (median 78.5 -> 8, which is the ruled
  anchor, met); corporations 3,135 -> 2,110; rivals armed 1,195 -> 172; rival units 173,900 ->
  24,150; seat shortlist 980 -> 148; markets 430 -> 433. Road tiers flip on three seeds (28, 31, 37:
  Highway to Road; Road 11,501 -> 19,344, Highway 65,029 -> 57,198). **Haulage -35.6% / -28.6%**,
  the largest single step.
- Digests: shipped all 16 from D_search; **legacy none** (the reach price is the shipped
  stockpile's).

**BL-1171 far trade (fleet out-projection binds; goods between landmasses sail; meeting by sea
gated; conquest inherits contacts; a trunk is shared only with the current).**
- Own record (3791723d, all switches on): Exploration meetings by sea 1,125 -> 234 with the gate;
  Industrialisation subjections 16 -> 33 (all switches off: 70); cross-landmass trade by sea at 1960
  3,323 -> 6,414.
- Step c4 -> c5: Exploration contacts met by sea 0 -> 234 (matches the item); Industrialisation
  subjections 70 -> 33 (matches); Exploration trade legs 637 -> 1,700; Exploration far treaties
  across water 0 -> 378; Exploration lanes opened 79 -> 119; Exploration battles 20,034 -> 20,357;
  Industrialisation battles 8,073 -> 7,680, bought 45 -> 8, trade legs 5,220 -> 4,607, lanes 312 ->
  271; lane tiles 7,027 -> 8,024; regions 15,436 -> 15,366; non-corporate units 167,110 -> 164,258.
  Road tiers flip on four seeds (31, 37 back to Highway; 13, 41 to Road). Haulage -12.8% / -12.9%.
- Digests: shipped 12 rows from D_search; legacy 12 (eleven from D_search, one from D_land). This
  is the only step that moves any Exploration field, so it is the whole of the seed-library
  fingerprint movement (below).
- acquisition_viability R1 4 -> 6 of 8.

**BL-1172 fair-price army (posted price, the 2x ceiling on every draw, no bid over it, shelf k = 0)
and BL-1173 working capital (0.25 x opening stock).** Campaign-economy changes. **Which pins they
can move:** neither touches generation before the landscape search, so D_search and every
generation reading (centres, provinces, the spans, lanes, roads) are out of their reach. BL-1173
opens background firms with cash in the landscape stage, so it moves D_land and everything after.
BL-1172 runs in the economy tick, so it moves D_settle and D_seat (the validation settle's 12
ticks) and every warm-start reading. Neither moves world_determinism: the m1173 knockout reads
its digests byte-identical to the tip's, and BL-1172 lives only in the tick.
- Own records: firm_attrition_trace corps at tick 462 on seeds 0/10/28: base 25/22/5; BL-1172 alone
  62/29/21; BL-1173 alone 25/7/5; both 56/34/27. econ_harness A.R3 996.764 -> 994.000.
- Step c5 -> c6 (both): D_search on no row; D_land, D_settle, D_seat on all 16 rows of both arcs.
  Probe: centre_pop_k 911,774 -> 928,799 (+1.9%); rivals armed 174 -> 171; rival units 24,550 ->
  24,850; seat shortlist 137 -> 135. Haulage -3.4% / -0.5%.
- BL-1173 alone, by knockout on the tip (m1173 -> after): D_land/D_settle/D_seat on all 16
  rows of both arcs, D_search none. Centre_pop_k 907,070 -> 913,140; rival units 23,450 ->
  23,400; seat shortlist 128 -> 126. Haulage +21.4% / +3.2% (7,001.3 -> 8,497.0).
- **Two gates go red here** (they are gates, not pins; not re-scoped):
  - `spawn_solvency` R4 "the field still fields a standing force (rivals can still afford to
    HIRE)": PASS through c5, FAIL at c6, FAIL on m1173 too, so it is **BL-1172's**. Units hired in
    the settle 229 -> -732 (the field loses units rather than hiring).
  - `acquisition_viability` R1 "the seated corporation ACCUMULATES on a majority of seeds": 6/8 at
    c5 -> 3/8 at c6; m1173 reads 5/8 and the tip 3/8, so it is **BL-1173's** (accumulation rate
    10.23 -> -13.91 cr/qtr).

**BL-1169 crowding brake at K = 1000 (Industrialisation only).**
- Own record (b427aed0): far rival 12 -> 14 of 16; industry points -29%.
- Step c6 -> after: industry_concentration reproduces both (far rival >= 25% on 12 -> 14 of 16;
  industry points 997.96 M -> 705.55 M, -29.3%). Industrialisation battles 7,680 -> 7,621,
  subjections 33 -> 30, trade legs 4,607 -> 4,872, lanes opened 271 -> 291; lane tiles 8,024 ->
  8,645; centres 13,437 -> 13,410; specialists 192 -> 182; markets 430 -> 436; non-corporate units
  164,258 -> 160,946. Road tiers flip on four seeds (12 to Highway; 13, 41 back to Highway; 38 to
  Road). Haulage +7.0% / +9.1%.
- Digests: shipped all 16 from D_search; **legacy none** (the span is off there).

**UNATTRIBUTED, or attributed without a traced mechanism.**
- **Road tiers flip whole networks.** Highway <-> Road moves wholesale on one seed at a time, at
  three of the five world-moving steps, and partly reverses: seed 31 and 37 go to Road at BL-1168
  and back at BL-1171; 13 and 41 go to Road at BL-1171 and back at BL-1169. The steps are
  measured; the mechanism is inferred, not traced: `edge_tier` gates a Highway on the nation's
  qualification PERCENTILE >= 0.80 among nations (mid-rank on ties), so a small upstream change
  that re-ranks or un-ties one nation flips its whole tree. Net: Highway 65,328 -> 63,730, Road
  11,537 -> 12,683. A human should read this as a sensitivity, not a design movement.
- **Haulage falls 41% / 33%.** Every step is measured and they sum to it: BL-1168 -35.6% / -28.6%,
  BL-1171 -12.8% / -12.9%, BL-1172/1173 -3.4% / -0.5%, BL-1169 +7.0% / +9.1%, trunks and bridges
  within 1.1%. The mechanism of BL-1168's fall is inferred (a third fewer corporations, a sixth of
  the specialists), not traced. No item recorded haulage.
- **BL-1172 alone** is the remainder of c5 -> c6 after the m1173 knockout; the knockout is taken
  on the tip, not at c6, so the two are not separated exactly.
- Non-corporate units fall 167,110 -> 160,946: BL-1171 -2,852 and BL-1169 -3,312, measured; no
  item recorded it.

## The headline claims and the check that asserts each

Every gate below was re-run by this lane on the after tree, and every one is ALL PASS:
sea_lane_stamp_harness (55), road_generation_harness (audit OK), ocean_currents_harness (107),
unit_upkeep (123), building_upkeep (90), fair_price_ceiling (101), nation_budget_harness (79),
working_capital (8), stockpile_budget_check (38), industry_concentration, player_seed_sweep.

| claim | committed check | gate or reading |
|---|---|---|
| lanes merge into trunks near shared ports | sea_lane_stamp_harness T1-T3 | gate |
| a trunk is shared only with the current | sea_lane_stamp_harness T4 | gate |
| bridges span at most two water tiles, every writer | road_generation_harness R7 | gate |
| curved roads move no world | none needed: no world file changed | by construction |
| a charter is priced by its anchor's trade reach | stockpile_budget_check 1c, player_seed_sweep stockpile_price_failure, industry_concentration (labels) | gate |
| the seat menu sits at the no-budget anchor (median 8) | rebless_shape_probe | **reading only** |
| far realms bind only where a fleet out-projects | ocean_currents_harness F6-F8, F11, F12 | gate |
| meeting by sea is gated the same way | ocean_currents_harness F13 | gate |
| goods between landmasses sail, every span | ocean_currents_harness F9, F10, F14 | gate |
| every draw obeys the 2x ceiling at the posted price | unit_upkeep U9-U13, building_upkeep R10, fair_price_ceiling, nation_budget R7n/R7o/R9m/R9n | gate |
| every background firm opens with working capital | working_capital W1-W3 | gate |
| rivals still hire in the settle | spawn_solvency R4 | gate, **RED since BL-1172** |
| the seated corporation accumulates | acquisition_viability R1 | gate, **RED since BL-1173** |
| a crowded heartland yields less at K = 1000 | **none: no harness reads `industry_points_crowding_k` or the brake** | **GAP** |
| the far rival holds 25% on 14 of 16 | industry_concentration | **reading only** |
| haulage | haulage_measure --far-trade | **reading only** (goldens cannot see trade) |
| the shape table itself | rebless_shape_probe | probe |

One claim has no committed check: BL-1169's brake. DELIVERY.md asks for one BEFORE the re-bless.
The haulage fall and the seat menu's collapse to single digits are the two readings a human should
weigh before authorising.

## Pins and goldens, old -> new

- **player_seed_sweep `--digest-check`, shipped and legacy**: all 16 rows of both tables moved
  (attribution above). Tables below. Post-pin: 16/16 PASS on both arcs.
- **world_determinism** (not pinned; byte-identity only): ALL PASS on both trees. Digests seedA/on
  42A1B0FD437D89DD -> 0CDD52D79FDA6086; seedB/on 7E48A5383CE087DA -> A32FBF7D3844B434; seedA/off
  68E2A3663570FB3E unchanged. Part of the move is the digest's own definition: `deep_digest` now
  folds BL-1168's reach prices.
- **exploration_sim_harness R3b / R3d.1**: unmoved; ALL PASS on both trees.
- **Seed library fingerprints**: 11 of 16 moved (46, 11, 43, 10, 25 unchanged), all through
  BL-1171. `seed_library_sweep.json` re-swept here (committed); **`--bless` was not run** (this
  lane's permission was refused); the main session runs `node tools/session/seed_library.js
  --bless` after authorisation. The moves: 28 expl_battles 382 -> 350, flows 27 -> 29, treasury
  median 216 -> 241; 31 3,388 -> 3,504, flows 99 -> 109, treasury 130 -> 226, living 121 -> 116; 40
  840 -> 933, flows 57 -> 59, treasury 83 -> 221, living 52 -> 53; 12 208 -> 140, flows 24 -> 22,
  treasury 173 -> 136; 37 1,374 -> 1,507, living 73 -> 75; 13 988 -> 985; 41 281 -> 207, flows 57
  -> 53, treasury 170 -> 159; 32 1,924 -> 1,918, flows 37 -> 35, treasury 169 -> 411, living 65 ->
  62; 38 flows 33 -> 29; 9 836 -> 1,094, flows 39 -> 46, treasury 153 -> 233; 0 993 -> 899, flows
  77 -> 72, treasury 8,664 -> 162, living 75 -> 70.
- **Visual goldens** (icon_silhouettes): BL-1167's. The curved roads moved them; not touched.
- **New reds, not pins** (above): spawn_solvency R4 (BL-1172) and acquisition_viability R1
  (BL-1173). Both PASS on the before tree.
- **Unchanged reds, identical on both trees**: history_sim_harness R3a2/R3a3;
  history_conquest_gap R3; industrialisation_sim_harness's BL-1056 self-check (BL-1166).
- **Unchanged greens**: ownership_class, province_partition_harness.

### Checked-in sweep tables, regenerated on the after tree

- `exploration_sweep.json` (seeds 0-15, `exploration_sweep 16`): regenerated. It was stale since
  BL-1044, so its diff carries earlier waves' movement too.
- `seed_library_sweep.json`: regenerated (the bless input above).
- `exploration_sweep_1960.json` (`--seeds <library> --through 1960 --cost`): regenerated with nothing
  else of this lane's running; its cost fields are wall-clock and vary run to run.
- `history_sweep.json` (Empires only): regenerated. **This wave moves none of it**: the before and
  after trees write byte-identical tables bar the `ms` timings. Its diff is earlier waves'
  movement (stale since 2026-09-17) plus timings. Kept in its own commit so it can be dropped.
- `charter_cost_sweep.json` / `charter_cost_sweep_rerun46.json`: NOT regenerated. They are
  BL-1033/BL-1043 experiment records with folded named runs, not a table any harness rewrites.

#### player_seed_sweep, shipped arc (D_search / D_land / D_settle / D_seat)

| seed | old | new |
|---:|---|---|
| 46 | 74BE5522F38FEBCE C2A75EC94251BAFF 7CA5CDC0DA7A6A29 B6EA9782801343AD | 34F2560905FFFCEB 6AAA5EDFAEF1CC4F CC78E91673F77299 C490CA6D45088DBF |
| 28 | D6AB1602D748AA6F AF71059DB82123C6 221DB67A043DE691 69168B884DABA0FC | B946CCD77067FCB0 B931B36F6BDAADA2 79730152B5DF08F0 65DD23B5CA186BBE |
| 11 | AF32371FEF8B3606 81BB20A025B48785 A491A5B1636517D5 E33B34D787C369B2 | 8B214357D752EF9F 35B95760389162D3 C88655EFAC6F804C 8C5F017A4E3FB44E |
| 31 | 73BBC02EB2C9C120 259973ACDA424BCA E3E49E0F91AB6D89 6C83C0389D6F1E6C | 48C2F80E652AD0BE 2FEEE52FA7550BDC A62B4A5CF1E58857 C1E3C295572D03EE |
| 40 | AB43E1396EA45C4A 6AFAF7354159C616 44CB33648B0F1BF1 439EC02481F29ECF | 9F518F9C3A3A676A A25F56E6A4BB684C 1B190E2C86263750 19870527DBBBCA93 |
| 12 | F1A17A78CAC966E6 AEF427CFAF6C36F8 5281AA30120BAF8E 274963F6379BA4C4 | 0B8C7AE7A903A3D6 7DB6282732DF636E 962C9E070F7C34D9 6C60488E069619AA |
| 37 | E8D9ABA85A646583 F4A84A65DE8755E8 F3052D47407C1983 D003A2DD03A1EE48 | 06B39FF3A0CE6D12 CCBBEF58479254E7 CAE1EC9A0FC0AA44 F88425B03E8111B0 |
| 13 | 260196CE3FE02645 48A7A9252B20B2BD 428EE6DE84AC42BC 4C26DC9E51E01F29 | 94802500676E33AA F90535822F236D6E 0EC6446D1598005C 648C1746F6B241F4 |
| 41 | C72D63F71EB8AEAB 4FEAFD09295BE43B 6A0295A6B1F6B199 6941EEA85538A607 | 543F6539820F88A3 E8F9BCED09684F38 7F4706920E2DA870 A837AFFC36BB1C09 |
| 43 | 852D7BFB0B8CBDE4 7BA36C709B48BF23 FF111A58441C8A49 7079608617EC9485 | FAEFD54CDAF75337 A710A1CBB06665BD 25FA4DB2E0216EE9 44C32FA17F03C937 |
| 32 | 47B5645C92164CE6 E5153D380117DAD5 169E3C63D379983A 9AD497B2641B1268 | 2077BFB3B3A00BFA 465E25806F47DB4F B72623095032DE33 5D1E2671DC5BE1C1 |
| 10 | 7087A2B4F2C53AE6 3B9036177D81ABCB 766762F955C40B04 536FA6D5DA41A688 | D6CFA0CF18873E35 0EFAA3C393BA487B 7DFEEE88B178FA69 27A1C0BB0F550BAD |
| 25 | F50E0E1138A7CACD 8448477D72617A04 89B7C1B34832F3CD E69C7622F0B73EB3 | 46486AD6E44C5DCB EA42931E03F76B84 77E31977258700FB 8269E6EFFADB2395 |
| 38 | EDACF79591AC3D4E D0EE0733E5A69C6B E5F4469D1F65C695 1ACFC1D599ED5097 | 4F6FC306DFCF2145 33F9B0D74CD39432 2A72FB3D95483D17 4C0A374EA083875B |
| 9 | 03880872A7549AAE 0DF01729AAFAFD97 500158A1F99F5A4C C2DC76F4FA79B98A | A4F8A159EABD95EE 3FA66D0EA2086E24 6F3EAEED7E474880 3BA449CA80DB1775 |
| 0 | 64F157945F9219E0 F014965E981977C4 5DD2709164B25F75 7346D517854A28F9 | C161F5F9B300B5C7 812A4D7C4E67EC04 6CADE285258EF691 1C62BD83A29D77B6 |

#### player_seed_sweep, legacy arc (D_search / D_land / D_settle / D_seat)

| seed | old | new |
|---:|---|---|
| 46 | C4F1975FD79F7FF0 81726A7A8DD18CEF 3E78BB77321511EB E542BB6C27BB5CAC | A49E43848C16DA35 32AD7CD27862AA16 88C9472BCF8BD3C9 5670AEFEE01B241F |
| 28 | 2DBD979F692CAAF2 3270D3EEA2D1896C E08CEDC067BA5EB9 73844338F800F5B7 | D6DC11ABCD55681B A7BE5CB2AF82D398 CE0F9D914EB74996 38C8B9737EEB016B |
| 11 | BDA2F620C1A19B83 7FFEF1057CB4FB1E 90024714A1E34B97 92272E9B87A1E1CB | 25BFEBEA08464A81 D09AF6EAD0DF3305 D1A2D13238F2C66E 7D02922FBC7FFD6D |
| 31 | 9083FB07D36E90C2 80A09F3731984D14 C393850AC3B4671A AA6D93562F7CA134 | 3D78CAD4E0A3FA0B 179E2C9AA3966B32 83F5C08BA8F63E39 39EC4E4B7B244C33 |
| 40 | 7078D86D3FC1E1A2 310FFABAAB2A046C 1B77C68ECDFE9422 BA1D855B042977BA | 57762D5165544B1C C4D959FD27DFEC99 BAF66EB09EFBC7AA 4E45801DCD3FE479 |
| 12 | 87556FF4FAB1F552 23EDAFA8AF9F67BD 09733A4728B1A5B9 C0D93F4E1899FEFA | 699921965CFBBF2C C6C65AAE1836FEAF 4BEA2A9E46C92208 418E63514703BEB7 |
| 37 | 4A103A2F7B353759 E4D9E90E977B97AC B03B1918385F7C06 46F8995C56F586F4 | CCE7CB2BBC26F19C 8A9F1C32D72A96E7 A4DD3D3C92BFEE55 7AE5C12D387A4B6F |
| 13 | 13078D4571844958 AFAEEA79198FC6FB 90C562285ED73155 3B8451C70BACC87C | B8569827D2340A15 3BA624D1C6384463 DB3992E1F0A31000 00E6CFBD3E149FA2 |
| 41 | 676D62DB4C3F623D EF8CC28E8C157561 55A1011E9CE5FD8D B9EEE4E515FD5ED0 | B60ACAB506456D8D 09FDCF066B4A4CC2 3558491A99B7B118 0C4A3A06C04D6A37 |
| 43 | 040B19E462A90C09 EDAFAB2B229BD14B 1B9614B54C95D664 11FAB5454F90B848 | E94AEE911DFCEE24 60B2A8437C3FF979 DD2E9F27042EC7E0 4C788BC5DC842A19 |
| 32 | 433F9A93F4B6E102 44C375BAAE762B7F 1DBD6157A07661AE B492817140D176C0 | 8B3A4FB246FB9538 451313D3995C4BD6 918559433436DA0E CED19B7DFE3E7AE1 |
| 10 | 5B2983E76540CA75 62D3F3E0A4510F0D C5FC257141F03DAD 632DC032DFE54305 | 00E6B5139CA88598 793EE259BA588ECC F9C272F3211233FD 011132288BF69287 |
| 25 | 2B4F4CB745F3F8D1 EC2056A676985C6C EB09A5BFF2A6411F BAE3083C405D58FA | 8425A84A9F0EF5C7 4480190D102EF70F D2FD5DFBA0F78744 AB8F1B80E0CC767C |
| 38 | 49AA47746AB1568C AA779F1CD016FACF 120B7EB8ED66B569 2343D365E150D939 | 7CF462189775B8AD DB85D77BA6BE755C 9DE0DB4271E3CA21 5017794B129A0E87 |
| 9 | 1BE49E73EFD0B4A8 6EB8F41B54773E10 E0984466B957383F ABCD77E68ADF8C3A | 8759B4A4B9836475 2A75D2CA9DF68502 CEF48A28907D1FD9 7DEEE7636002F0BD |
| 0 | 3F38C26E04995BEF D903B49CDFF55AE4 7C9575BF6CF612B3 1BDC81725DAC4A58 | 802AF6CE63047E13 D64AF3A739B65EC8 BBF9407242D8F2B7 0E6A8E93293437DA |
