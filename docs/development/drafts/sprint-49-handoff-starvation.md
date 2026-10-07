# Sprint 49 — what is starved at the handoff (BL-1207 diagnosis)

**Diagnosis only.** Nothing in `src/` or `scripts/` moved. The probe is `tools/verify/handoff_starvation.cpp`.
It is a pure reader. Only input_reach's logistics path caches warm, after the handoff, and no tick follows.

```
bash tools/verify/build_lua_harness.sh handoff_starvation
build_gen/verify/handoff_starvation.exe [--seeds a,b] [--top N] [--list]
```

**The world** is market_viability's: `build_app_start_world`, the 12-tick settle, then the seat.
The state classification is market_viability's G1 `classify`, term for term. On seed 0 the totals match
market_viability's handoff row exactly: built 359, run 205, input 66, unsupplied 10, decom 77, build 180.
Two runs of the 16 seeds gave identical tables.

**Two reading points** on the settle's last tick:
- **PRE-DRAW**: after the convoys lap, before `run_economy_step`. Shelves, posted prices and (corp, market)
  pools as the production pass first sees them.
- **HANDOFF**: after the tick and the seat. Report rows, buildings, and input_reach in both forms.

**History.** Every settle tick's pre-draw posted prices are kept, plus each processor's ticks run.
So each row says whether a state is chronic or a one-tick snapshot.

**Which processors are analysed.** Every processor input-starved at the handoff (961). Also every
processor decommissioned whose last reported row was input-starved (1,471). Total: 2,432.

## The short answer

1. **G1 on current main is 41.8%, pooled over the 16 curated seeds.** Seed 0 alone reads 57.1%.
   Reaching 70% needs about 1,395 more processors running.
2. **Nothing is missing.** In no seed is a limiting input absent from the body, held only by idled
   makers, or made only by idle makers. Every starved processor's input is made on its body that tick.
3. **The biggest cause is the price ceiling cycle.** 1,081 processors (44%) had their input on the shelf
   but priced over the fair-price ceiling. Most of the 707 "drawn away" are the same cycle, caught on its
   open tick. Together: about 1,588 (65%).
4. **The rest is under-supply and undelivered supply.** In-market makers too small: 322.
   Makers in another market, nothing landed: 322.
5. **Starvation is chronic, not a snapshot.** 1,330 of the 2,432 never produced once in 12 settle ticks.
   1,278 of the 1,471 idled plants never ran before the loss reflex idled them.
6. **Provenance barely matters.** Generation-placed plants are 49% starved. Plants the scorer built and
   finished are 46% starved. But 3,499 scorer-built processors are still under construction at the handoff.
   That is 41% of all processors, outside the G1 denominator.

## T1 — state at the handoff, by provenance (16 seeds pooled)

"Gen" = present when the world is handed to the settle. "Scorer" = appeared during the settle.
"Decom-after-input" = decommissioned with an input-starved last row (a subset of decom).

| prov | run | input | nolab | unsup | decom | other | build | built | run % | decom-after-input |
|---|---|---|---|---|---|---|---|---|---|---|
| gen | 1,899 | 821 | 0 | 235 | 1,663 | 16 | 0 | 4,634 | 41.0% | 1,467 |
| scorer | 169 | 140 | 0 | 0 | 4 | 0 | 3,499 | 313 | 54.0% | 4 |
| **all** | **2,068** | **961** | 0 | **235** | **1,667** | 16 | **3,499** | **4,947** | **41.8%** | **1,471** |

Starved share (input + decom-after-input over built): gen 2,288 / 4,634 = **49.4%**; scorer 144 / 313 = **46.0%**.

Per seed, running / built at the handoff:

| seed | 46 | 28 | 11 | 31 | 40 | 12 | 37 | 13 | 41 | 43 | 32 | 10 | 25 | 38 | 9 | 0 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| built | 292 | 308 | 303 | 292 | 406 | 300 | 299 | 320 | 285 | 286 | 305 | 292 | 297 | 308 | 295 | 359 |
| run % | 36.0 | 53.9 | 24.1 | 31.2 | 44.1 | 48.7 | 43.8 | 33.8 | 41.1 | 50.3 | 52.8 | 38.4 | 33.7 | 43.8 | 32.2 | 57.1 |
| analysed | 163 | 127 | 194 | 167 | 211 | 126 | 143 | 174 | 132 | 120 | 120 | 146 | 166 | 152 | 157 | 134 |

## T2 — the limiting input's status

For a processor at market C on body B with limiting input r, `floor` = t_idle × its full-run need.
The **primary** bucket is the first that holds, in this order:

| bucket | meaning |
|---|---|
| over_ceiling | pre-draw pool + shelf ≥ floor, but the shelf's price > reservation_mult × base (2.0): stock there, priced out |
| drawn_away | pre-draw pool + admitted shelf ≥ floor, yet it starved: earlier draws in the same tick took it |
| never_chart | no building on B in any state makes r |
| not_standing | makers of r on B, none standing |
| makers_idle | standing makers on B, none produced r this tick |
| own_mkt | a maker in C produced r this tick; none of it reached this processor |
| reach_play | a producing market P ≠ C is within reach in input_reach's PLAY form; nothing landed |
| reach_gen | within reach only in the GENERATION form (the lane placement assumed) |
| no_reach | r produced on B; no producing market within reach of C in either form |

| bucket | primary | gen | scorer | starved now | idled | sole short | ticks run (mean of 12) | never ran | r over ceiling 0 / 1–3 / 4–8 / 9–12 settle ticks |
|---|---|---|---|---|---|---|---|---|---|
| over_ceiling | **1,081** | 1,000 | 81 | 510 | 571 | 770 | 2.5 | 459 | 0 / 58 / 524 / 499 |
| drawn_away | **707** | 666 | 41 | 261 | 446 | — ¹ | 2.2 | 433 | 200 / 208 / 288 / 11 |
| never_chart | 0 | | | | | | | | |
| not_standing | 0 | | | | | | | | |
| makers_idle | 0 | | | | | | | | |
| own_mkt | **322** | 300 | 22 | 137 | 185 | 291 | 2.0 | 181 | 28 / 50 / 119 / 125 |
| reach_play | **190** | 190 | 0 | 31 | 159 | 143 | 0.9 | 147 | 0 / 4 / 51 / 135 |
| reach_gen | **118** | 118 | 0 | 12 | 106 | 118 | 0.4 | 106 | 0 / 6 / 30 / 82 |
| no_reach | 14 | 14 | 0 | 10 | 4 | 10 | 3.6 | 4 | 0 / 0 / 10 / 4 |

¹ drawn_away has r covered by definition. 685 of the 707 had **no** input short at pre-draw (261 starved now, 424 idled).
They would have run had they drawn first.

Other readings:
- **over_ceiling prices**: price/base deciles 10/25/50/75/90% = 2.27 / 2.77 / 4.10 / 6.94 / 9.34. Mean 4.98.
  The shelf holds a median 14.7 ticks of the processor's own need.
- **input_obtainable** (the scorer's own question) answers "obtainable" for 1,137 (generation form)
  and 1,162 (play form) of the 2,432. Mostly drawn_away (580) and over_ceiling (455).
  So for about half the idled plants, the resume gate's input test is not what keeps them idle.
- **Independent flags** (they overlap): own_mkt 1,735; reach_play 1,459; reach_gen 721; no_reach 41.
  Most starved processors have their input made in their own market AND reachable from another.
- **Seat**: 7 of the analysed processors belong to the seated corp.

## T3 — top limiting inputs

| input | n | sole | scorer | over_ceiling | drawn_away | own_mkt | reach_play | reach_gen | no_reach |
|---|---|---|---|---|---|---|---|---|---|
| steel | 361 | 182 | 72 | **346** | 13 | 0 | 1 | 1 | 0 |
| refined_copper | 283 | 263 | 1 | 154 | 14 | 40 | 47 | 28 | 0 |
| copper_ore | 265 | 125 | 37 | 27 | **140** | 59 | 15 | 24 | 0 |
| ree_alloy | 260 | 254 | 0 | 136 | 0 | 48 | 34 | 42 | 0 |
| rare_earth_ore | 254 | 80 | 5 | 20 | **174** | 33 | 10 | 17 | 0 |
| coal | 228 | 100 | 10 | 46 | 128 | 18 | 26 | 0 | 10 |
| silicon | 198 | 0 | 0 | 126 | 8 | 18 | 42 | 0 | 4 |
| silica | 177 | 46 | 4 | 32 | 131 | 2 | 6 | 6 | 0 |
| food_rations | 131 | 91 | 14 | 8 | 24 | **95** | 4 | 0 | 0 |
| iron_ore | 90 | 80 | 0 | 88 | 2 | 0 | 0 | 0 | 0 |
| agricultural_produce | 90 | 21 | 1 | 12 | 68 | 9 | 1 | 0 | 0 |
| water | 82 | 82 | 0 | 78 | 0 | 0 | 4 | 0 | 0 |

By recipe (T4): consumer_goods 483 (steel 352, food_rations 131), steel 316 (coal 228, iron_ore 88),
machinery 278 (refined_copper 273), refined_copper 265, alloys 261, ree_alloy 254, electronics 208,
silicon 177, medical_supplies 71 (water 50), food_rations 69, clean_water 32.

Units on over-ceiling shelves, by input: steel 74,730; iron_ore 29,668; water 26,747; silicon 2,206;
refined_copper 1,656. The rest are under 1,000 each.

## T5 — the holes

673 distinct (seed, market, input) holes. 539 of them hold 2+ processors, covering 2,298 of the 2,432.
**Every analysed processor sits on its home body.**

Pooled per leading bucket. "Draw" = the nominal draw of every standing consumer of r in C.

| holes led by | holes | procs | pre-draw shelf | C output this tick | standing draw | (shelf + output) / draw |
|---|---|---|---|---|---|---|
| over_ceiling | 252 | 1,079 | 136,449 | 3,466 | 7,394 | 1,892% |
| drawn_away | 219 | 712 | 20,257 | 9,710 | 18,355 | 163% |
| own_mkt | 82 | 325 | 37 | 1,144 | 2,417 | 49% |
| reach_play | 73 | 184 | 12 | 0 | 470 | 2% |
| reach_gen | 41 | 118 | 5 | 0 | 199 | 2% |
| no_reach | 6 | 14 | 0 | 0 | 160 | 0% |

The ten largest holes:

| seed | market | input | procs | bucket | pre-draw shelf | p/base | C output | standing draw | post-clear demand / supply |
|---|---|---|---|---|---|---|---|---|---|
| 40 | 46731 | steel | 55 | over_ceiling | 653 | 2.51 | 288 | 600 | 0 / 163 |
| 13 | 47919 | copper_ore | 25 | own_mkt 20, over_ceiling 5 | 3 | 2.01 | 3 | 400 | 0 / 0 |
| 13 | 47919 | steel | 22 | over_ceiling | 900 | 4.22 | 90 | 208 | 0 / 30 |
| 25 | 48093 | steel | 20 | over_ceiling | 2,557 | 3.11 | 8 | 176 | 0 / 2 |
| 40 | 46731 | copper_ore | 19 | drawn_away | 166 | 1.48 | 236 | 486 | 453 / 221 |
| 40 | 47503 | food_rations | 19 | own_mkt | 0 | 1.63 | 40 | 152 | 166 / 28 |
| 46 | 47888 | steel | 18 | over_ceiling | 4,397 | 2.80 | 161 | 192 | 0 / 39 |
| 0 | 47629 | coal | 15 | drawn_away | 59 | 1.59 | 132 | 290 | 183 / 76 |
| 0 | 47629 | rare_earth_ore | 15 | drawn_away | 37 | 1.18 | 78 | 301 | 201 / 6 |
| 11 | 48152 | steel | 15 | over_ceiling | 1,777 | 2.20 | 20 | 120 | 0 / 8 |

## The mechanisms

**A. The ceiling cycle (about 1,588 processors).** Over-ceiling holes hold 18× their standing draw, and
post-clear demand there is **zero**. The price is not set by scarcity; it is the four-tick cycle BL-1186 (shipment diagnosis) found.
- While the price is over 2× base, processors neither draw nor bid (BL-1172, the fair-price ceiling). Demand reads zero.
- With k = 0 (NR-972) the shelf is not supply, so the price decays by EMA toward 0.25×.
- On the tick it dips under 2×, every processor bids its whole want against listings-only supply.
  The target jumps toward 10×, and the price is over the ceiling again.

The history agrees. 1,023 of the 1,081 had r over the ceiling on 4+ of 12 settle ticks; they ran 2.5 of 12.
Running one tick in four loses money, so the reflex idles them (571 of the 1,081 are idled).
Of the 707 drawn_away, 507 had r over the ceiling on 1+ settle ticks. The handoff caught their hole on
its open tick, and first-come draw order (corp id, then asset order) emptied the shelf. Only 200
drawn_away processors sit in holes that never crossed the ceiling.

**B. Shallow shelf, first-come draw (about 200).** Drawn_away holes that never crossed the ceiling.
The shelf plus this tick's output covers the standing draw (163% pooled), but not all of it at once.

**C. In-market under-capacity (322).** Own_mkt holes: C's makers produce 49% of C's standing draw.
The shelf is near empty (37 units pooled). Consumer goods on food rations is the largest group (95).
Households eat the same food rations.

**D. Made in reach, nothing landed (322).** Reach_play 190, reach_gen 118, no_reach 14. The input at C
was over the ceiling on 9–12 settle ticks for most of them. A dry market priced at the ceiling registers
no processor want, so a hauler sees no room. This is BL-1203's mechanism, at the processor grain.

**E. Not analysed here.** Unsupplied 235: the BL-641 (building supply) upkeep scalar is zero, not an input.
Also 196 idled with a non-input last row, and 16 "other".

## Levers, ranked

Estimates are **upper bounds** and **overlap**: one processor can sit under several levers.
"Now" = input-starved at the handoff, so freeing its input lets it run on the next tick.
"Idled" = also needs a resume, which today gates on obtainable inputs, profit and the strategic cooldown.
The G1 figure assumes nothing else moves: (2,068 + freed) / 4,947.

| # | lever | touches | frees (now + idled) | G1 if only "now" | G1 with idled resumed |
|---|---|---|---|---|---|
| 1 | **Break the ceiling cycle.** A processor's draw stops spiking the price against a stocked shelf. Options: (a) a want counts only past what the admitted shelf already covers; (b) the shelf counts as supply for intermediate goods (k > 0 for them); (c) judge the ceiling on a price the draw itself did not move. | MARKETS.md § Price resolution (the k = 0 ruling, NR-972); FINANCE.md § the fair-price ceiling (BL-1172) | sole over_ceiling 770 (369 + 401); cycle-phase drawn_away with no input short 486 (115 + 371). **Up to 1,256.** | 51.6% | 67.2% |
| 2 | **Resume idled plants whose inputs are at hand.** 424 idled processors had every input at pre-draw. | AI_OPPONENT.md § 11 (the resume reflex; BL-1187 (build only what runs) gate); economy_system's loss reflex | 424 idled (mostly inside lever 1's 371) | — | +8.6 pts on its own |
| 3 | **Proportional draw instead of first-come.** Each draw takes a pro-rata share of a short shelf, so all run at ≥ t_idle where the shelf allows. | PRODUCTION.md § stockpile flow / the two-threshold model | shelf ≥ t_idle × combined draw, no other input short: 345 (137 + 208) | 44.6% | 48.8% |
| 4 | **Hauler room for suppressed want** (BL-1203 (water reaches dry markets), already ruled, option B). | SUPPLY.md § Dispatch trigger | reach_play sole 143 (31 + 112). Reach_gen 118 needs lever 1 first. | 42.4% | 44.7% |
| 5 | **Size upstream to downstream within a market at placement.** | CORPORATION_GENERATION.md / BL-1185 (chain-feasible placement) | own_mkt sole 291 (136 + 155) | 44.6% | 47.7% |
| 6 | **Upkeep supply** (the BL-641 scalar). | FINANCE.md § Upkeep is credits AND goods | 235 unsupplied (not analysed here) | up to 46.6% | — |

**BL-1189 (opening stock sized) is not a G1 lever on this reading.** The over-ceiling shelves hold
136k units and are priced out, not short. A smaller opening stock shrinks them, but the cycle stays.
It may lower G1 unless lever 1 lands first. Re-measure G1 after it, beside G2.

**The only path to 70% on this reading runs through lever 1 plus resumes.** Lever 1 alone, with no
resumes, reaches about 52%. Levers 3–6 add a few points each and overlap with lever 1 and with each other.

## Open questions for the main session

- **Why are the 424 idled plants with inputs at hand not resumed?** Cooldown, the profit estimate,
  or the obtainable test on post-clear prices. The probe does not separate these. A one-tick check
  of `corp_ai`'s resume candidate list at the handoff would settle it.
- **The construction backlog.** 3,499 scorer-built processors are still under construction at the
  handoff, about 219 per seed. They join the G1 denominator as they finish. Of the 313 that finished,
  46% are already starved. G1 at tick 50 will read them; this probe does not.
- **The brief's ~49% G1.** Current main reads 41.8% pooled over the 16 curated seeds.
  The earlier figure may come from a 5-seed reading.

## Assumptions

- PRE-DRAW is after the convoys lap; arrivals have landed, production has not drawn.
- An idled processor's limiting input and labour come from its last reported row.
- `need` is the full-run need: inputs × base_rate × effective labour × workforce target × supply scalar.
- Reach and obtainable are asked on the handoff world, post-clear, so their prices are the next tick's.
- "Sole short" and "no input short" read pool + admitted shelf at PRE-DRAW, before any same-tick draw.
- Lever 3's feasibility counts idled processors' need in the combined draw.
