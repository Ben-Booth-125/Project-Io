# Sprint 49 — household re-measure (water flowing, stock freed)

Measurement only, 2026-10-05. Seeds 0, 43, 10; 12 settle ticks + 400 play ticks.
Nothing here changes `src/` or `scripts/`.

## The three trees

| cfg | tree | what it adds |
|---|---|---|
| **a** | `main` @ dc9a5ab8 | the Well, lake cap, chain-feasible placement, build-only-what-runs, orders are price floors (BL-1201, save v33) |
| **b** | a + `worktree-agent-a9fdaa3472e00382d` @ 96328d42 | BL-1197 round 3: generation places Wells, a chain-refused good charters its missing raw's extractor first. Clean merge. |
| **c** | b + `worktree-agent-adc6cdf570362aedc` @ 40a4fb87 | BL-1179 shelf spoilage (k = 0, water durable), BL-1196 households consume off the shelf, BL-1163 per-centre (per-market) household growth gate |

Throwaway branches `meas49-b` (8a36a6d5) and `meas49-c` (merge + conflict fix), not pushed.

**Separability caveat.** c bundles two changes to the growth reading: households now *consume*
(BL-1196, the stock is freed) AND the growth gate moves from one body-level met ratio to a
per-centre one (BL-1163). a and b run the body gate. So a vs c heads are not a clean read of
"water flowing" — the gate itself changed. The household-good rows (hsp) are comparable.

**Harness versions differ.** a/b run main's `market_viability` (no shelf G5, no centre
counts) and main's `centre_decline_trace` (body-level met, no household fill/bid). c runs the
held stack's versions. `household_supply_probe` is the same source on all three.

### Merge conflicts (c)

1. `docs/development/save_version_reservations.jsonl` — main's v33 line (BL-1201) vs the
   stack's ledger (v32 BL-1196 already common). Kept main's side; no new ledger line.
2. `src/world/world_save.hpp` — `world_save_version` 33 (BL-1201, sell_order::empty_ticks) vs 32
   (BL-1196, market household_bid/household_fill). Kept both comment blocks; set **34**, the next
   safe number from `next_save_version.js --kind world`. **Not claimed in the ledger** — the real
   integration must claim it. `world_save.cpp` auto-merged with both record changes.
3. `tools/verify/market_viability.cpp` — two different "G5" rows: main's BL-1187 (idled
   processors come back) and the stack's BL-1179 (shelf at ceiling, household fill/bid, centres
   grew/fell). Kept both: struct fields unioned, both trackers, both per-seed and pooled prints.
   A follow-up compile error (`gr` declared twice in one pooled scope) fixed by splitting the
   two pooled blocks into their own scopes.

## 1. market_viability (400 play ticks)

| | a main | b +r3 | c +household stack |
|---|---|---|---|
| G1 running at handoff (pooled) | 41.0% | 40.1% | 33.6% |
| G2 play 26-50 / settle mean | 45.1% | 45.1% | 38.5% |
| G3 firms alive t400 / handoff | 55.2% (203/368) | 49.7% (191/384) | 59.9% (230/384) |
| G5 idled procs came back (BL-1187) | 38.0% (287/756) | 43.1% (333/773) | 61.8% (462/747) |

Per seed G1 / G2 ratio / G3:

| seed | a | b | c |
|---|---|---|---|
| 0 | 56.8 / 41.1 / 53.6 | 54.4 / 66.1 / 53.9 | 45.6 / 60.6 / 67.2 |
| 43 | 35.3 / 156.1 / 55.4 | 38.6 / 93.0 / 50.0 | 34.1 / 80.7 / 62.3 |
| 10 | 30.0 / 55.5 / 56.3 | 24.9 / 128.6 / 45.2 | 18.9 / 73.0 / 50.0 |

c only — G5 shelf (BL-1179), play ticks 20-50: at ceiling 54.6%, at ceiling against a stocked
shelf 1.3%, over reservation 87.1%, priced against stock 7.2%. Household fill / bid, ticks 20-50:

| good | s0 | s43 | s10 | pooled |
|---|---|---|---|---|
| agricultural produce | 75% | 91% | 91% | 86% |
| water | 79% | 80% | 73% | 77% |
| food rations | 56% | 72% | 43% | 58% |
| clean water | 37% | 0% | 10% | 20% |
| consumer goods | 38% | 1% | 0% | 18% |
| medical supplies | 67% | 11% | 22% | 42% |

c only — centres handoff -> play tick 400: s0 814 centres, 79.5M -> 131.1M heads, grew 166
fell 648; s43 658, 42.1M -> 71.9M, grew 216 fell 442; s10 881, 52.8M -> 29.9M, grew 58
fell 823. Pooled 174.5M -> 233.0M, grew 440 fell 1913. No centre razed.

## 2. centre_decline_trace (t = economy steps; t12 = handoff, t400 = play tick 388)

Heads (M) and centres on a negative streak ("declining"):

| seed | cfg | t12 | t100 | t200 | t400 | declining t400 | met at t400 |
|---|---|---|---|---|---|---|---|
| 0 | a | 79.4 | 111.6 | 162.1 | 344.4 | 0 | body 0.69 |
| 0 | b | 79.4 | 111.6 | 162.1 | 344.4 | 0 | body 0.72 |
| 0 | c | 79.5 | 80.3 | 86.5 | 131.1 | 463 / 814 | per-centre median 0.22 (648 below 0.50) |
| 43 | a | 41.1 | 48.2 | 69.2 | 144.0 | 0 | body 0.68 |
| 43 | b | 41.1 | 57.9 | 83.2 | 173.1 | 0 | body 0.74 |
| 43 | c | 42.1 | 40.4 | 45.9 | 68.0 | 442 / 658 | median 0.34 (442 below) |
| 10 | a | 52.2 | 73.6 | 105.6 | 97.3 | 881 / 881 | body 0.48 |
| 10 | b | 52.2 | 61.3 | 87.9 | 182.1 | 0 | body 0.80 |
| 10 | c | 52.8 | 44.8 | 40.7 | 31.6 | 881 / 881 | median 0.28 (881 below; max 0.48) |

c median per-centre met ratio over time: s0 0.12 / 0.16 / 0.19 / 0.22 (t12/100/200/400);
s43 0.49 / 0.43 / 0.45 / 0.34; s10 0.43 / 0.32 / 0.34 / 0.28.

c only — household fill / bid (body sums) at t400, shelf left after the draw:

| good | s0 | s43 | s10 |
|---|---|---|---|
| water | 302/343 (88%), shelf 10561 | 222/261 (85%), shelf 15645 | 162/227 (71%), shelf 1327 |
| clean water | 47/136 (35%), shelf 4 | 2/98 (2%), 0 | 1/118 (1%), 0 |
| medical supplies | 42/98 (43%), 0 | 0/42 (0%), 0 | 0/58 (0%), 0 |
| consumer goods | 32/133 (24%), 0 | 17/72 (24%), 0 | 0/84 (0%), 0 |
| food rations | 217/361 (60%), 34 | 251/337 (74%), 70 | 109/334 (33%), 0 |
| agricultural produce | 125/173 (72%), 379 | 154/165 (93%), 510 | 164/179 (92%), 202 |

## 3. household_supply_probe (t = economy steps; t412 = play tick 400)

Cell: `listed/HH bid  sh<shelf>  r<running>/<placed makers>  m<bidding markets holding a maker>`
(25 / 14 / 23 bidding markets on seeds 0 / 43 / 10). On a and b the HH bid is the probe's
formula (nothing consumes); on c it is the live household channel.

| good | cfg | s0 t12 | s0 t100 | s0 t200 | s0 t412 | s43 t12 | s43 t100 | s43 t200 | s43 t412 | s10 t12 | s10 t100 | s10 t200 | s10 t412 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| water | a | 50/196 sh8792 r2/4 m2 | 544/286 sh23496 r17/21 m4 | 520/350 sh67497 r19/22 m5 | 504/366 sh169114 r20/22 m5 | 22/112 sh9944 r1/8 m2 | 154/195 sh14217 r10/12 m3 | 366/219 sh30440 r13/13 m3 | 541/267 sh83488 r20/20 m4 | 104/204 sh8269 r9/10 m2 | 298/204 sh8014 r14/18 m3 | 239/220 sh10991 r15/20 m3 | 226/229 sh23300 r13/18 m3 |
| water | b | 254/258 sh10891 r18/18 m3 | 1408/346 sh30032 r49/59 m7 | 1517/455 sh142358 r56/63 m8 | 1513/537 sh453819 r56/63 m8 | 123/187 sh10767 r10/10 m2 | 490/284 sh28252 r25/27 m4 | 725/297 sh77176 r34/34 m4 | 515/341 sh166181 r28/28 m4 | 250/252 sh8757 r9/11 m4 | 239/289 sh22123 r17/19 m4 | 374/305 sh51296 r17/19 m4 | 446/348 sh137127 r17/17 m4 |
| water | c | 206/218 sh6532 r16/18 m3 | 541/261 sh5939 r22/22 m3 | 508/266 sh9540 r22/25 m5 | 588/316 sh10158 r27/29 m6 | 123/177 sh6738 r10/10 m2 | 262/183 sh3423 r14/16 m4 | 447/220 sh6373 r16/23 m4 | 534/262 sh15602 r22/22 m4 | 248/240 sh4108 r9/11 m4 | 224/206 sh1507 r15/16 m4 | 201/212 sh1070 r12/14 m4 | 218/223 sh1332 r12/14 m4 |
| clean water | a | 0/181 sh0 r0/0 m0 | 103/254 sh7054 r5/5 m1 | 63/232 sh12062 r1/1 m1 | 66/275 sh20237 r5/5 m1 | 0/128 sh219 r0/0 m0 | 0/128 sh219 r0/0 m0 | 0/128 sh3554 r0/0 m0 | 0/128 sh10988 r0/0 m0 | 0/169 sh204 r0/0 m0 | 92/228 sh6450 r5/10 m2 | 66/238 sh13272 r4/6 m2 | 87/206 sh24528 r9/10 m2 |
| clean water | b | 35/188 sh459 r4/4 m1 | 130/288 sh14650 r11/11 m1 | 56/270 sh28445 r5/5 m1 | 86/262 sh36679 r4/4 m1 | 0/128 sh357 r2/2 m1 | 60/187 sh1069 r8/8 m3 | 29/153 sh7951 r3/3 m1 | 0/150 sh19354 r0/0 m0 | 0/183 sh381 r2/6 m1 | 14/200 sh2380 r4/4 m2 | 14/185 sh4452 r3/3 m1 | 2/169 sh5989 r2/2 m1 |
| clean water | c | 47/186 sh0 r4/4 m1 | 0/142 sh0 r4/5 m1 | 15/164 sh0 r1/1 m1 | 78/204 sh0 r4/4 m1 | 0/116 sh0 r2/2 m1 | 52/136 sh0 r5/5 m2 | 28/141 sh0 r3/3 m2 | 2/105 sh0 r1/1 m1 | 36/195 sh0 r4/6 m3 | 12/119 sh0 r3/7 m3 | 28/158 sh0 r3/5 m2 | 0/118 sh0 r1/2 m1 |
| medical | a | 133/78 sh133 r17/17 m1 | 70/124 sh9965 r5/7 m1 | 22/99 sh13316 r3/3 m1 | 18/102 sh19246 r1/1 m1 | 0/55 sh188 r0/8 m2 | 47/82 sh487 r6/6 m2 | 89/79 sh4246 r8/8 m2 | 53/76 sh12286 r3/3 m1 | 36/99 sh567 r4/11 m2 | 137/111 sh11150 r8/10 m2 | 98/117 sh18536 r13/13 m2 | 33/115 sh30542 r5/5 m2 |
| medical | b | 2/78 sh289 r1/6 m2 | 180/147 sh18991 r12/12 m2 | 48/128 sh31733 r4/4 m2 | 0/88 sh36717 r3/3 m2 | 10/79 sh345 r3/8 m3 | 31/66 sh1434 r0/0 m0 | 75/100 sh11096 r7/8 m2 | 3/79 sh23398 r2/2 m1 | 46/106 sh789 r8/10 m4 | 24/94 sh3257 r4/4 m2 | 8/86 sh5535 r1/1 m1 | 0/72 sh7470 r1/2 m2 |
| medical | c | 0/67 sh0 r1/6 m2 | 80/106 sh23 r3/8 m2 | 33/90 sh0 r3/3 m1 | 32/88 sh0 r0/0 m0 | 27/52 sh5 r3/8 m3 | 4/52 sh0 r1/2 m2 | 97/99 sh50 r7/7 m3 | 0/42 sh0 r0/0 m0 | 30/100 sh0 r5/8 m4 | 9/64 sh0 r2/2 m2 | 0/52 sh0 r1/1 m1 | 19/69 sh0 r3/3 m2 |
| consumer goods | a | 227/130 sh1218 r26/46 m2 | 103/183 sh7566 r6/17 m3 | 79/177 sh14461 r4/8 m2 | 59/204 sh30320 r4/5 m2 | 0/95 sh1055 r2/69 m5 | 17/96 sh3344 r1/25 m5 | 86/150 sh10184 r8/10 m4 | 30/101 sh17211 r7/10 m4 | 133/138 sh1039 r16/64 m5 | 38/143 sh3419 r8/40 m6 | 19/138 sh4481 r1/11 m4 | 0/120 sh4694 r1/3 m2 |
| consumer goods | b | 429/170 sh2436 r53/69 m4 | 88/185 sh9282 r5/15 m6 | 93/179 sh16735 r8/29 m7 | 42/191 sh31364 r2/5 m4 | 64/106 sh1028 r8/65 m4 | 275/130 sh3768 r22/41 m4 | 44/108 sh13173 r4/5 m2 | 42/107 sh23469 r3/14 m4 | 16/123 sh1125 r2/59 m5 | 21/120 sh3035 r10/33 m2 | 18/134 sh5078 r1/8 m4 | 13/132 sh9479 r2/9 m4 |
| consumer goods | c | 292/152 sh714 r40/70 m5 | 67/161 sh0 r5/30 m5 | 68/153 sh0 r5/17 m4 | 67/104 sh32 r8/11 m4 | 36/91 sh106 r4/65 m4 | 3/65 sh0 r1/10 m2 | 38/96 sh0 r3/22 m3 | 3/77 sh0 r1/20 m3 | 16/117 sh32 r0/55 m5 | 0/79 sh0 r0/26 m5 | 1/84 sh0 r1/15 m3 | 0/84 sh0 r0/5 m3 |
| food rations | a | 620/521 sh13006 r47/56 m7 | 481/468 sh38629 r40/45 m3 | 503/511 sh68773 r40/44 m4 | 404/556 sh133536 r38/41 m4 | 356/471 sh11807 r39/39 m6 | 298/393 sh27490 r34/34 m6 | 267/412 sh24138 r33/33 m6 | 338/436 sh48704 r29/29 m5 | 614/581 sh12518 r35/45 m7 | 382/538 sh34744 r39/43 m6 | 357/560 sh59918 r43/43 m6 | 192/494 sh90204 r35/37 m6 |
| food rations | b | 531/466 sh12288 r39/44 m6 | 348/427 sh22661 r26/37 m5 | 332/447 sh29166 r28/39 m4 | 396/527 sh55747 r26/34 m4 | 391/489 sh12051 r30/36 m6 | 268/391 sh38230 r30/31 m6 | 238/380 sh34433 r26/28 m5 | 206/442 sh38552 r28/30 m5 | 338/552 sh12084 r38/46 m7 | 194/525 sh29270 r34/44 m7 | 312/520 sh47564 r40/40 m7 | 260/529 sh72832 r30/33 m7 |
| food rations | c | 323/386 sh98 r28/44 m6 | 262/343 sh66 r24/36 m6 | 277/352 sh66 r29/37 m6 | 259/344 sh25 r25/32 m6 | 304/416 sh10 r24/36 m6 | 297/362 sh62 r28/29 m6 | 264/312 sh68 r24/28 m5 | 278/344 sh67 r25/34 m4 | 229/489 sh40 r25/45 m7 | 156/344 sh1 r27/37 m7 | 200/367 sh0 r28/32 m7 | 96/346 sh0 r24/29 m6 |
| agricultural produce | a | 706/177 sh10506 r24/24 m3 | 713/179 sh9149 r23/23 m2 | 701/204 sh14709 r22/23 m2 | 589/209 sh25435 r22/23 m2 | 602/189 sh13748 r17/18 m6 | 764/206 sh29059 r18/18 m6 | 720/194 sh52834 r15/16 m6 | 777/201 sh94566 r18/19 m7 | 1201/288 sh17385 r29/29 m8 | 862/286 sh53812 r30/30 m8 | 574/245 sh67605 r27/27 m8 | 377/222 sh74070 r16/16 m5 |
| agricultural produce | b | 681/206 sh10892 r25/26 m6 | 672/221 sh13719 r23/23 m3 | 548/198 sh22112 r21/25 m4 | 635/232 sh35593 r21/21 m3 | 675/192 sh13035 r17/18 m6 | 652/192 sh27977 r17/17 m5 | 518/174 sh30842 r16/16 m5 | 454/180 sh41202 r14/14 m5 | 549/221 sh11394 r15/15 m7 | 434/245 sh17212 r16/16 m7 | 576/232 sh28961 r15/15 m7 | 504/230 sh44399 r14/14 m6 |
| agricultural produce | c | 681/185 sh565 r25/26 m6 | 681/177 sh463 r23/23 m3 | 617/176 sh418 r23/23 m3 | 583/176 sh373 r21/21 m2 | 671/180 sh562 r17/18 m6 | 720/142 sh461 r17/17 m5 | 842/165 sh527 r17/17 m5 | 823/163 sh507 r17/17 m5 | 551/218 sh371 r15/15 m7 | 460/179 sh264 r15/16 m7 | 462/187 sh304 r16/16 m7 | 371/182 sh193 r15/15 m6 |

## 4. Integrity on c

- `world_determinism`: ALL PASS (18 PASS rows, 0 failures).
- `save_roundtrip`: SAVE ROUND TRIP OK (70 PASS rows, 0 FAIL) at world_save_version 34.

## 5. The plain answer

**Do centres grow anywhere? Yes — but a minority, and seed 10 not at all in aggregate.** With
households consuming and the per-centre gate, 440 of 2353 centres grew over 400 play ticks
(s0 166, s43 216, s10 58); 1913 fell. Pooled heads still rose 174.5M -> 233.0M, carried by the
centres that grew on seeds 0 and 43 (+65%, +71%). Seed 10 lost 43% of its heads; at t400 every
one of its 881 centres sits below the 0.50 gate (max 0.48). Median per-centre met ratio
sits at 0.22 / 0.34 / 0.28.

**Water is no longer the binding short — on the body.** Water production is enough: body-wide
listed water exceeds the household bid on all three seeds, and fill runs 71-88% at t400.
The short that remains is *placement*: water makers sit in 3-6 of 14-25 bidding markets, and
seeds 0 and 43 pile 10-16k units of durable water on shelves while other markets go dry.

**The short goods, worst first (c, t400):**
1. **Clean water** — 1-35% filled (pooled 20% in ticks 20-50). One to four running plants per
   body; listed 0-78 against a 98-204 bid. A production shortage, not a shelf one.
2. **Medical supplies** — 0-43% (pooled 42% early, collapsing to 0% on seeds 43 and 10 by t400;
   makers fall to 0-3 running).
3. **Consumer goods** — 0-24% (pooled 18%). Makers placed 55-70 at handoff, running 0-8 by t400.
4. **Food rations** — 33-74% (pooled 58%). Makers mostly run (24-29 of 29-34), but
   output trails the bid, and seed 10 halves by t400.
5. Agricultural produce 72-93% and water 71-88% are the near-met goods.

**What "stock freed" did.** On a and b nothing consumes, so household goods pile on shelves
(b, seed 0, t412: water 454k, food rations 56k, consumer goods 31k). On c those shelves
are gone — food, clean water, medical, consumer goods all read 0-70. The stock was
real but finite; once drawn down, the shortfall is the flow. b's extra Wells raise water
output (s0 running water makers at t412: a 20, b 56); c runs fewer (27) and that water
still does not reach most markets.

**Separation.** BL-1197 r3 (a -> b) moves the household-good flows little except water. It
moves G3 down (55.2 -> 49.7) and leaves G1/G2 flat. The household stack (b -> c) lowers G1
(40.1 -> 33.6) and G2 (45.1 -> 38.5), raises G3 (49.7 -> 59.9) and G5 comebacks (43 -> 62%),
and turns body-wide growth into per-centre decline. That last move is the gate change
(BL-1163) as much as consumption; these three trees cannot separate them.
