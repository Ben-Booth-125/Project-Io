# Sprint 48's one re-bless — the shape, the causes, the pins

Prepared 2026-10-03 on the re-bless lane's branch, on main a899bb2a. **Not authorised.** Ben
authorises against the SHAPE below (DELIVERY.md § The digest re-bless is one act per WAVE, rule 4),
never against the hashes. Nothing merges until he has.

## Baseline

- **Before** is the v0.1.26 cut, e50ab81c (2026-09-25), measured from a clean export of that tree.
- The pins themselves were blessed at **d6558647** (sprint 47's final tree, exe 10:22 2026-09-25).
  e50ab81c differs from it in `src/` only in the settle's lap clocks and UI (BL-1085, write-only);
  a shipped-arc `--digest-check --seeds 28,0` built from e50ab81c passes both rows against the old
  pins (seed 28 is the row BL-1117 later moved), so the pins are the cut's world.
- **After** is main a899bb2a, the last commit before this lane.
- The combined before/after is ONE instrument run on both trees: `tools/verify/rebless_shape_probe`
  (committed with this re-bless; it compiles against both trees, reads only what both carry, and
  prints `-1` where a tree does not record a field). It builds the app's world: generation with the
  app's config and works, the landscape search's winner, the validation settle, then the seat.
  Its before-side centres (164,982) and markets (4,984) reproduce BL-1130's recorded baseline
  exactly, which is the probe's own calibration.
- Haulage: `haulage_measure --far-trade --seeds 1 --first-seed S`, one run per seed, each tree.

## The shape, pooled over the 16 curated seeds

Per-seed median in brackets where the spread matters.

| | before (e50ab81c) | after (a899bb2a) |
|---|---|---|
| **Centres** | 164,982 (median 10,330 a seed) | 13,437 (median 773; 352-1,175) |
| scale mix village / town / city / metro / mega | 160,669 / 3,351 / 761 / 171 / 30 | 4,536 / 6,202 / 2,696 / 3 / 0 |
| province-anchor centres | 7,561 | 1,448 |
| **Regions** (all living) | 42,695 | 15,436 |
| population on the settlement record | 12.19 B | 6.27 B |
| **Urban share** (per-seed median) | 14.4% (13.5-15.7%) | 25.6% (22.7-28.3%) |
| **Land provinces** | 69,137 | 13,007 |
| province size p50 / p90 / max (tiles, per-seed median) | 3 / 6 / 15 | 8 / 29 / 594 (largest 2,541) |
| **Markets** | 4,984 (median 273) | 429 (median 28; 17-33) |
| **Road tiles** | 175,173 | 79,152 |
| Track / Road / Highway tiles | 0 / 42,070 / 133,103 | 2,287 / 11,537 / 65,328 |
| sea lane tiles stamped | not recorded (no stamp) | 12,975 (4-2,256 a seed) |
| **Empires** battles / conquests / foundings | 158,361 / 128,638 / 19,509 | 249,612 / 210,925 / 7,196 |
| **Exploration** battles / conquests / foundings | 10,433 / 8,063 / 13,577 | 20,034 / 17,146 / 3,652 |
| Exploration subjections / provinces bought | 107 / 47 | 105 / 82 |
| **Industrialisation** battles / conquests / foundings | 3,792 / 2,607 / 6,277 | 8,073 / 6,751 / 1,256 |
| Industrialisation subjections / bought | 62 / 56 | 70 / 45 |
| sea legs by writer, Exploration: campaign / purchase / tribute / trade | 650 / 47 / 4,976 / n.r. | 893 / 82 / 2,453 / 637 |
| sea legs by writer, Industrialisation | 264 / 56 / 3,881 / n.r. | 781 / 45 / 2,307 / 5,220 |
| sea lanes opened, Exploration / Industrialisation | 85 / 20 | 79 / 312 |
| treaties across water / far, Industrialisation | n.r. | 1,782 / 1,508 |
| fleets opened, Exploration | n.r. | 762 (24-97 a seed) |
| crossings fought (sea-leg battles), Exploration / Industrialisation | 650 / 264 | 893 / 781 |
| **Rivals armed** / rival units | 2 / 100 | 1,195 / 173,750 |
| corporations / firms / specialists chartered | 1,297 / 979 / 318 | 3,134 / 1,919 / 1,215 |
| **Seat menu** (specialists offered) | 318 (median 8; 4-69) | 1,215 (median 78; 29-114) |
| cultures with a good preference, 1660 / 1960 | 169 / 170 | 389 / 376 |
| preference entries, 1660 | 254 | 816 |
| **Haulage** (a) sold at destination, window E / Y | 16327.0 / 14728.9 u | 14433.9 / 11916.5 u (-11.6% / -19.1%) |

n.r. = not recorded by that tree (the field did not exist).

### Per seed, where it matters

- Centres: 46 15,240 -> 998; 28 6,176 -> 352; 11 13,094 -> 1,031; 31 13,621 -> 1,175; 40 7,421 ->
  710; 12 10,330 -> 676; 37 10,606 -> 953; 13 11,469 -> 1,042; 41 8,709 -> 952; 43 7,470 -> 625;
  32 10,164 -> 720; 10 13,771 -> 886; 25 10,354 -> 1,136; 38 6,353 -> 680; 9 10,977 -> 728;
  0 9,227 -> 773. Every seed loses 89-95%; only 28 falls under the retired ~500 aim.
- Markets: every seed lands in 17-33 (43 is the one under 20). Before: 148-538.
- Rivals armed: 0 or 1 before on every seed; 28-113 after (46 is lowest).
- Exploration battles fall on one seed only (38: 176 -> 153); 12 goes 12 -> 208.
- Sea lane tiles: seed 11 stamps 4 tiles and 25 stamps 57; 12 stamps 2,256.

### Haulage

Units of cargo sold at a destination market other than the source pool's (reading (a)), window E the first play year, window Y the year after; every run 0 failures.

| seed | before E | after E | before Y | after Y |
|---:|---:|---:|---:|---:|
| 46 | 134 | 1006.3 | 82.3 | 770.2 |
| 28 | 361 | 376.4 | 263.5 | 251.4 |
| 11 | 844.6 | 734.9 | 780 | 625.8 |
| 31 | 2390.7 | 881.2 | 2096.2 | 877.6 |
| 40 | 1407.1 | 964.8 | 1266.9 | 796.9 |
| 12 | 751.9 | 365.8 | 541.2 | 286.4 |
| 37 | 745.3 | 917.6 | 662.5 | 704.2 |
| 13 | 709.9 | 1315.5 | 658 | 1009.1 |
| 41 | 3023.9 | 1073.2 | 2722.5 | 796.2 |
| 43 | 1564.3 | 1832.8 | 1368 | 1397.4 |
| 32 | 694.6 | 185 | 607.3 | 254.6 |
| 10 | 616.3 | 550.3 | 584.4 | 705.7 |
| 25 | 432.3 | 778.5 | 337.5 | 803.7 |
| 38 | 817 | 656.5 | 927.2 | 413.9 |
| 9 | 1376.2 | 1519.9 | 1299.8 | 1136.4 |
| 0 | 457.9 | 1275.2 | 531.6 | 1087 |
| **pooled** | **16327.0** | **14433.9** (-11.6%) | **14728.9** | **11916.5** (-19.1%) |

## The causes, with each item's own before/after

Every number below is the item's own record (its requirement group, its commit body, or REFINED.md),
measured in its own worktree on the main of its day; they do not sum to the combined movement.

**Density chain.**
- BL-1130 (centres consolidate): centres 164,982 -> 66,034, land under centres 70.4% -> 28.2%,
  road tiles 168,582 -> 82,543, markets 4,984 -> 5,180; water regions carry no centre (spills
  127,226 -> 0). Battles moved (synthetic 7 -> 8 after the P/R rulings).
- BL-1133 (a province is its centre's ground): provinces 45,199 -> 30,894, anchors 20,505 -> 6,856.
- BL-1132 (settle spacing 3): regions 43,130 -> 15,194, centres 52,552 -> 42,251, land provinces
  31,029 -> 33,749. **The war rise is mostly this item**: Empires battles 155,226 -> 247,547,
  conquests +66%, foundings -65%; Exploration and Industrialisation battles about double.
- BL-1141 (one centre a region): centres 42,251 -> 22,521, scales 37,755/3,487/795/178/36 ->
  15,624/6,749/148/0/0; the history identical.
- BL-1150 (the fill crosses the settled line): land provinces 21,972 -> 13,232, anchors 10,381 ->
  1,430, lock islands 9,295 -> 0.
- BL-1156 (rivers divide banks): crossing/plain 1.01 -> 1.52; banks in one province 76.1% -> 51.1%.

**Works and urbanisation.**
- BL-1137 (urbanisation, rebuilt), BL-1149 (works credit at x4), BL-1155 (every centre a work
  candidate): at x4 urban 25.6%, centres 13,392, 2,704 cities, heads moved 962.8M. BL-1149 alone:
  points per world 107.7M -> 57.5M. BL-1155: Empires battles 247,547 -> 237,568 (every span),
  points 49.0M -> 38.0M. The towns and cities of the scale mix are these items.

**Markets.**
- BL-1125 (markets die) with BL-1127 (the column wrap): twins fold, gravity fold at reach 16 with
  the port gate (median 31 a world, 15 of 16 seeds in 20-40), conquest destroys markets.
  BL-1127's own movement was not measured apart (committed separately "to be taken or dropped").

**Firms and the seat.**
- BL-1086 (the carve's planned firms): markets 5,784 -> 5,803; every seed plans exactly 120 firms
  (the body ceiling) — the firms column's 120 a seed.
- BL-1146 (the scaled firm cap): province_cap refusals 6.0% -> 3.1% of the budget; budget firms
  1,911 -> 1,914 of 1,920.
- The seat menu (318 -> 1,215) is not one item's: the carve plans every specialist the budget
  affords (BL-1086) on far richer, larger provinces (the density chain). Not attributed per item.

**Arms.**
- BL-1154 (rivals armed, the legacy arc too): 1,113 rivals armed, 1,553 units hired in the settle;
  rivals solvent 88.5% -> 71.5%.

**The sea chain.**
- BL-1120 (currents): the field, weight 500; lanes barely move alone (tribute writes ~90%).
- BL-1140 / BL-1098 (lanes from trade, the lane tier stamped): lanes earned 468 -> 493, laid 352 ->
  371 (after the four-way fix).
- BL-1142 (far pairs across water): cross-water pairs bound 53 -> ~538 at 1960, trade lanes 27 ->
  297.
- BL-1147 (naval points carry over): fleets opened from points; weights 1 / 280 / 9,600.
- BL-1152 (fleets project power; 20 / 10 / 50 in the Exploration age): no recorded count delta.
- BL-1153 (lane ports): laid 327 -> 382 of 446; lane tiles 10,657 -> 11,902.
- Read together they are the Industrialisation trade legs (n.r. -> 5,220) and its lanes (20 -> 312).

**Roads.**
- BL-1119 (the roads tree): border links on a bare street 106 -> 0 (seed 46); road tiles moved
  little alone (streets are centres).
- BL-1138 (market roads): markets off the backbone 128 -> 19.
- BL-1159 (the Highway read from the polity crossing): 209 qualifying links, all Highway, on 15 of
  16 seeds; haulage +1.0% / -1.4%.

**Smaller readers.**
- BL-1145 (garrison posts): 0 garrisons off post; nation-AI threat term within 2% of main.
- BL-1107 (culture ground profile): cultures with a preference 224 -> 389 at 1660; haulage
  +13.6% / +15.5%.
- BL-1117 (settle tick one): D_settle/D_seat moved on seed 28 only (the flood fields no longer
  built); world_determinism unmoved.
- BL-1136 (two-thread search): byte-identical by its proof; no world movement.

**UNATTRIBUTED.** Traced 2026-10-10 (BL-1165) in `sprint-48-rebless-traced.md`: population is
BL-1132; the haulage fall is BL-1154 (-30%) and BL-1125 (-47%), after the density chain had
doubled it; nation units are the
province grain (garrison count) plus BL-1132 (garrison size).
- The settlement record's population halves (12.19 B -> 6.27 B). The per-item records begin at
  6.08 B (BL-1141's base), so the halving lies upstream, in BL-1130/BL-1132's regions (42,695 ->
  15,436). No item recorded population; this is the likeliest cause, not a measured one.
- Haulage falls pooled (-11.6% / -19.1%) and splits by seed: in window E it rises on 8 (46 x7.5,
  0 x2.8) and falls on 8 (32 -73%, 41 -65%, 31 -63%). Every item that read it recorded a rise or a wash; the market
  folds (BL-1125, ~10x fewer markets) are the likeliest cause, and BL-1125's own gate accepted
  haulage "pending a shipped re-read". Not isolated.
- Non-corporate units (nations' forces) fall 414,404 -> 167,110; no item recorded it. Likely the
  smaller provinces' garrison readers on far fewer provinces (BL-1145 / the density chain).

## The headline claims and the check that asserts each

| claim | committed check | gate or reading |
|---|---|---|
| one centre a region, no region over its ground | centre_census (the cap row) | gate |
| every land province holds a centre | centre_census C7 `unanchored` | gate |
| settle spacing holds | centre_census C10 | gate |
| the stream conserves | centre_census C15 | gate |
| rivers divide banks | centre_census C13 C2a (C2b slope red on 46, BL-1158) | gate |
| garrisons on post | centre_census C16b, garrison_border_probe | gate |
| twins fold, catchments pass whole | market_census R1/R2, market_fold_fixture | gate |
| markets ~20-40 a world | market_census, market_gravity_ladder | **reading only** (the aim is not enforced, by ruling) |
| rivals armed, the seat unarmed | rival_military_seeding_harness R0/R0b, spawn_solvency | gate |
| fleets bound their army; out-projected crossings never sail | ocean_currents_harness P0-P10 | gate |
| lanes from coastal ports | sea_lane_stamp_harness R1-R3 (16-seed seed 28 lays no lane, known) | gate |
| far pairs bind across water | ocean_currents_harness W9, C7-C10, F1-F5 | gate |
| Highways on the shipped worlds | road_generation_harness R2s0/R2s-a/R2s-b | gate |
| market centres on the backbone | road_generation_harness R6a-g, market_census I1 | gate |
| culture profile inherited, read | culture_preference_fixture P0-P6, culture_preference_census | gate |
| urban share ~25% | centre_census | **reading only** |
| the war roughly doubles | exploration_sweep, centre_census C9 | **reading only** |
| haulage | haulage_measure --far-trade | **reading only** (goldens cannot see trade) |
| the shape table itself | rebless_shape_probe | **probe, committed here** |

Three headlines rest on readings alone: the market count, the urban share and the war's size. Each
is a ruled consequence, not a target, so no gate is owed by the docs; the haulage fall is the one a
human should read before authorising.

## Pins and goldens, old -> new

- **player_seed_sweep `--digest-check`, shipped and legacy**: all 16 rows of both tables moved from
  D_search on. Tables below. Post-pin: 16/16 PASS on both arcs.
- **exploration_sim_harness R3b** (`w_want_q` 0 regression pin): battles / conquests / foundings
  30 / 27 / 816 -> 239 / 150 / 134; subjections 3 -> 0; freed 0 -> 0; tribute 228,427,744 -> 0;
  treaties 354 -> 496; broken 1 -> 4; owner changes 2,061 -> 864.
- **exploration_sim_harness R3d.1** (fork-off control): the same old values -> the same new values;
  on the new fixture no realm holds a subject, so fork-off equals R3b's fork-on. Post-pin ALL PASS.
- **Seed library fingerprints** (`seed_library_sweep.json`, re-swept by exploration_sweep over the
  16 library seeds, then `--bless`): 16 of 16 moved. Empires battles per seed (old -> new): 46
  14,541 -> 19,841; 28 3,418 -> 5,114; 11 19,204 -> 26,050; 31 17,842 -> 34,221; 40 5,820 ->
  10,184; 12 4,905 -> 4,983; 37 12,181 -> 20,112; 13 10,250 -> 18,772; 41 7,329 -> 10,367; 43 4,168
  -> 7,361; 32 8,389 -> 14,867; 10 11,148 -> 20,944; 25 18,776 -> 25,795; 38 4,453 -> 4,822; 9 5,834
  -> 9,467; 0 10,103 -> 16,712. Exploration battles, flows, treasury median and living polities
  moved with them (git diff of `docs/generation/seed_library.json`). Post-bless `--check`: 16
  unchanged.
- **Visual goldens** (`scripts/verify/golden/icon_silhouettes_*`): NOT refreshed here. They draw
  the home surface's world content (a corp name, a tile, a province capacity, a nation count),
  contrary to the policy text that calls them world-independent, and were last blessed 2026-06-17.
  A worktree lane cannot build the app (no cmd). The main session owes `--verify-all --bless`
  after the merge, or a call on whether the pair stays curated.
- **Not pins, left alone**: history_sim_harness R3a2/R3a3 (real known failures). The checked-in
  `exploration_sweep.json` (seeds 0-15) is a reading table, stale since BL-1044, not refreshed.
- **Observed, not a pin**: industrialisation_sim_harness exits at its BL-1056 self-check (2) —
  "past the heads domain -> NOT REFUSED" — before reaching its library-fingerprint control. The
  apportion no longer refuses a 2^31-head region; NR-964 (treasury by employed heads) changed what
  it reads. A stale self-check, not this re-bless's to re-scope.

#### player_seed_sweep, shipped arc (D_search / D_land / D_settle / D_seat)

| seed | old | new |
|---:|---|---|
| 46 | 9163D7627D2D707D CF296FEBDA8A5CF6 0C18C7D28527C748 AFE6EDBBF94E7A39 | 74BE5522F38FEBCE C2A75EC94251BAFF 7CA5CDC0DA7A6A29 B6EA9782801343AD |
| 28 | E3EE2CA3DBDCACF1 88DCF45E0F51F723 14DA34AB7D5E9719 BC353A3566E70D46 | D6AB1602D748AA6F AF71059DB82123C6 221DB67A043DE691 69168B884DABA0FC |
| 11 | 4B6DBCCC2EF92437 68943644E32B85E5 41A932A61B505B1C 1ADFC4BB030D719B | AF32371FEF8B3606 81BB20A025B48785 A491A5B1636517D5 E33B34D787C369B2 |
| 31 | 913EF0928777CA9C 374B79FA1FBDA488 27C96798E750F5E9 6A114436D0FD4B54 | 73BBC02EB2C9C120 259973ACDA424BCA E3E49E0F91AB6D89 6C83C0389D6F1E6C |
| 40 | 805FEFE1957B607E A83B418C29D1C916 E135C9BB98CF3AAD 7590195E7273E2E7 | AB43E1396EA45C4A 6AFAF7354159C616 44CB33648B0F1BF1 439EC02481F29ECF |
| 12 | 136EED72EDF61794 3E4417AAF25A1C50 7145C4FEF61EEBCC 5E9AF445AF7AE251 | F1A17A78CAC966E6 AEF427CFAF6C36F8 5281AA30120BAF8E 274963F6379BA4C4 |
| 37 | 00375FA217E03123 F1068B129FF0A982 D07FFE966B18DAE3 79FE5F32C70F3C8F | E8D9ABA85A646583 F4A84A65DE8755E8 F3052D47407C1983 D003A2DD03A1EE48 |
| 13 | BE4F1C03BBD7C761 2BA02C74CAB7BCCA 76F0E3015717C22A 51ECD04465AC030B | 260196CE3FE02645 48A7A9252B20B2BD 428EE6DE84AC42BC 4C26DC9E51E01F29 |
| 41 | 9DFBAD10E7B26C86 3A3D7CC57633AA05 106A11B2857FB197 789EAACB3BA1DD28 | C72D63F71EB8AEAB 4FEAFD09295BE43B 6A0295A6B1F6B199 6941EEA85538A607 |
| 43 | 057CA694B1A2930C 4076E9787D238074 519028D648B0C6FB 0AE0D0491F073614 | 852D7BFB0B8CBDE4 7BA36C709B48BF23 FF111A58441C8A49 7079608617EC9485 |
| 32 | B1336A041598EDDE 8666E7FB97AFA045 92978C54EA9B06DE CAE16F6F852699EF | 47B5645C92164CE6 E5153D380117DAD5 169E3C63D379983A 9AD497B2641B1268 |
| 10 | EE9F4CEBDD5209D7 0940315A86A33D66 09E81548A473ADED 0CE1C0DE18B708E5 | 7087A2B4F2C53AE6 3B9036177D81ABCB 766762F955C40B04 536FA6D5DA41A688 |
| 25 | 79B43A9C38921727 BFCA1924FE329582 5A45D5CA513848D5 F2B07B6F9FE57771 | F50E0E1138A7CACD 8448477D72617A04 89B7C1B34832F3CD E69C7622F0B73EB3 |
| 38 | 364B4435C0856DF1 C87E83A20E93B463 413D4F82DD6E449C 815166FF9B09673D | EDACF79591AC3D4E D0EE0733E5A69C6B E5F4469D1F65C695 1ACFC1D599ED5097 |
| 9 | C99EA3CD68B5A279 7B16B1823F122F54 663D59BBCD98A8C7 E9C081E25B7EB933 | 03880872A7549AAE 0DF01729AAFAFD97 500158A1F99F5A4C C2DC76F4FA79B98A |
| 0 | 74D765F3BAD8759B 854427AF3B6BB2B1 6816A20C4269D530 23A9FC8EF3C5EFD7 | 64F157945F9219E0 F014965E981977C4 5DD2709164B25F75 7346D517854A28F9 |

#### player_seed_sweep, legacy arc (D_search / D_land / D_settle / D_seat)

| seed | old | new |
|---:|---|---|
| 46 | B01320655D4BE99B CE0759A1021D42DE 943B500E6BACA53E 2255E0FA29F84B37 | C4F1975FD79F7FF0 81726A7A8DD18CEF 3E78BB77321511EB E542BB6C27BB5CAC |
| 28 | 6147552AE45BC384 6945C8FD0630F62B 6421FAE0D54B08C8 34207AB36AACBE19 | 2DBD979F692CAAF2 3270D3EEA2D1896C E08CEDC067BA5EB9 73844338F800F5B7 |
| 11 | 347D1FC2F3414944 49E952EEB82638B6 97AF74843097D0B0 B1F03569405B3499 | BDA2F620C1A19B83 7FFEF1057CB4FB1E 90024714A1E34B97 92272E9B87A1E1CB |
| 31 | 8222E1FC2E699B75 30E354906C6683E6 6B1D3466230B69BD E0403E96D32375F4 | 9083FB07D36E90C2 80A09F3731984D14 C393850AC3B4671A AA6D93562F7CA134 |
| 40 | 675BE71426C41855 E6DA3DE7391A91A5 5A41B0F860559779 8AFE71BAD882F586 | 7078D86D3FC1E1A2 310FFABAAB2A046C 1B77C68ECDFE9422 BA1D855B042977BA |
| 12 | 9797ADFB249251D2 35DA779C3A2D0377 99DBA6C406C1EA4C AD04BD8F4D730186 | 87556FF4FAB1F552 23EDAFA8AF9F67BD 09733A4728B1A5B9 C0D93F4E1899FEFA |
| 37 | EFB311383F01645A 5DAA228B371EFB8A 453EFADF8D1E6B3D E08894B43B9A2B85 | 4A103A2F7B353759 E4D9E90E977B97AC B03B1918385F7C06 46F8995C56F586F4 |
| 13 | C7B777CA0FD159E6 71C82015587C12C6 ABC4B0335C819C6E D1D38669D2CE5948 | 13078D4571844958 AFAEEA79198FC6FB 90C562285ED73155 3B8451C70BACC87C |
| 41 | A05441E37A179C0F DAFBFBF5FF1FDFDB 07D808D0285565F3 7CA3B2D323F52729 | 676D62DB4C3F623D EF8CC28E8C157561 55A1011E9CE5FD8D B9EEE4E515FD5ED0 |
| 43 | 326144637C766315 001B77C98DC69050 CF71E1AF1CCEC50C 8A0598264EF33472 | 040B19E462A90C09 EDAFAB2B229BD14B 1B9614B54C95D664 11FAB5454F90B848 |
| 32 | 808D9D6939315793 0FEC8B3A1FD72AFC 16079F89B73132E6 E4979C16408E5B1E | 433F9A93F4B6E102 44C375BAAE762B7F 1DBD6157A07661AE B492817140D176C0 |
| 10 | 6AC29D77FA59D8DF 374F4DF634E78FA0 E06752EA5BC0A215 04BC1F2DF67C4F44 | 5B2983E76540CA75 62D3F3E0A4510F0D C5FC257141F03DAD 632DC032DFE54305 |
| 25 | 0DBABB3FACE37ABC 93B92BFFC30698BE 9E81C9E7EB037F05 67BB0397644A428B | 2B4F4CB745F3F8D1 EC2056A676985C6C EB09A5BFF2A6411F BAE3083C405D58FA |
| 38 | 1B69ED64D13CB22B E338A5503F445F73 174215F4B3B99E0F 15B9D1123D7C00B8 | 49AA47746AB1568C AA779F1CD016FACF 120B7EB8ED66B569 2343D365E150D939 |
| 9 | 465FB80B0F56317B 367C311E788EB6A0 F399AF5750EB1CF8 64D0529BD6BB47A5 | 1BE49E73EFD0B4A8 6EB8F41B54773E10 E0984466B957383F ABCD77E68ADF8C3A |
| 0 | A69C27D61A1F3F83 DFFE435C03F75E29 9551A80942BA0761 3690DB05EA78806F | 3F38C26E04995BEF D903B49CDFF55AE4 7C9575BF6CF612B3 1BDC81725DAC4A58 |

