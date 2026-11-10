# Project Io — Needs Review

**Ben's review queue.** Readable mirror of [`NEEDS_REVIEW.json`](NEEDS_REVIEW.json),
which is canonical — the JSON wins on any disagreement.

> **Generated file.** Produced by `node tools/session/render_needs_review.js`.
> Edit the JSON, then re-run; hand edits here are overwritten.

Things here are waiting on **your judgement**, not on work. Three kinds:

| Kind | Meaning |
|---|---|
| **question** | An open call nobody has made. Not blocking — a blocking item is a backlog entry with `blocked_on` set. |
| **decision-taken** | A call made **on your behalf** so work could continue. Recorded so it can be *overturned* rather than quietly becoming precedent. |
| **observation** | Something noticed in passing, too small or too cross-cutting to file, that a human should still see. |

**How this differs from the neighbours.** [`review.json`](review.json) is a *blocker* list —
items blocked on a visual artifact only you can produce; work there cannot proceed at all.
[`backlog.json`](backlog.json) is *work*. Entries here are neither: they are questions and
reversible calls. If an answer creates work, file a backlog item and resolve the entry with
that item's id.

This queue is **transient**: resolved entries are pruned promptly rather than kept for
posterity — the reasoning lands in code, an authority doc, or a backlog item at the moment
the work happens, and that is the durable record. What stays here is what is still open.

*50 entries — 43 open, 7 resolved.*

---

## Open

### NR-1082 — Two disclosure tensions the NR-1066 doc pass found
*question · raised 2026-10-26 · from NR-1066 doc pass*

(1) A rival's treasury is private (Ben, 2026-10-26), but DISCOVERY.md § The operational fog says an open book shows "flows, balance", and FINANCE.md § Surfaces gives the Profitability ledger one row per nation, so a rival's filed return could expose its treasury. Whether a rival's return is readable at all is unruled. (2) The Industry lens, kept, weights rival plants by output_quantity, against "production stays private" (DISCOVERY.md; TOOLTIP.md). Also: the seat screen lists every major's treasury before play.

**Why it matters.** Each decides what the player can see of a rival nation.

- (1) A: a rival's return is unreadable; B: readable without its balance line. (2) A: the Industry lens shows rival plant count or presence, not output; B: output is public through the lens.

> **Recommendation:** (1) B; (2) A. The seat screen is a pre-play pick, so it may show treasuries.

*Files: `docs/ui/DISCOVERY.md`, `docs/economy/FINANCE.md`, `docs/ui/LENSES.md`*

### NR-1086 — Why are strategic nuclear weapons never fired, now there is no catastrophe?
*question · raised 2026-10-26 · from era catastrophe retirement design pass*

Ben ruled tactical nuclear use permitted and strategic never fired (MILITARY.md § Missiles), the same day the Era catastrophe retired. The old reason - 'firing one is the catastrophe' - is gone. The rule stands as written, with the arsenal acting through Alarm alone.

**Why it matters.** A rule with no stated cause reads as arbitrary to the player, and the AI needs to know what an arsenal is for.

- A: keep the rule; the arsenal is pure deterrence through Alarm
- B: allow strategic use with ruinous consequences (sentiment collapse with every nation, climate strain, the target razed)
- C: strategic use only in answer to a strategic strike (second-strike only)

> **Recommendation:** A for v0.3.0: cheapest and legible; revisit B once war goals and peace exist.

*Files: `docs/military/MILITARY.md`*

### NR-1088 — Should a rival's unit counter show its HP and readiness?
*question · raised 2026-10-27 · from BL-1425 (unit counter render)*

The counter and the unit card show HP, readiness and morale for the player's own units only. A rival's counter shows its owner colour and unit count, and its card shows no condition bars. This follows BL-068 (a rival's internals stay private) as the battle card already applies it. Your brief said units show HP and readiness, without saying whose.

**Why it matters.** Seeing a rival's spent units is how a player picks where to push, and fronts (BL-1427) will lean on it; but it is a widening of what the player sees of another nation, which is yours to rule.

- A: own units only (as built)
- B: everyone's bars always
- C: a rival's bars only where the player has units in contact or a recon satellite overhead (the intelligence layer, BL-1431)

> **Recommendation:** C, with A until satellites exist: it makes intelligence a thing worth buying.

*Files: `src/ui/body_surface_canvas.cpp`, `src/ui/selection_panel.cpp`, `docs/ui/DISCOVERY.md`*

### NR-1090 — "Refit" now names two things
*question · raised 2026-10-27 · from sprint 58 refit lane*

MILITARY.md § Land — equipment, refit and manpower already uses "refit" for refilling a wounded unit's HP. The new verb uses it for changing a unit's type. The doc says they are distinct, but GLOSSARY.md settles neither.

**Why it matters.** One word for two mechanics confuses the player and the action dictionary.

- A: "Refit" is the type change; the HP refill becomes "Replenish"
- B: keep "Refit" for HP; the type change becomes "Upgrade"
- C: leave both

> **Recommendation:** A. The button on the unit card already reads Refit, and replenish is the military term for refilling men and kit.

*Files: `docs/military/MILITARY.md`, `docs/GLOSSARY.md`*

### NR-1094 — A retired recipe slot is a new pattern
*novel-work · raised 2026-10-27 · from BL-1438 (retire construction capacity) lane*

Recipe ids are list positions saved in every building, so the five capacity-yard recipes cannot be deleted. They stay in recipes.lua with retired = true and no inputs or outputs; the loader refuses a retired recipe that names any; a building running one idles but still pays wages. The capacity resource keeps its enum slot, price and spoilage so old stock decays.

**Why it matters.** No doc owns retiring a recipe; the next retirement will copy this.

- A: write the pattern into PRODUCTION.md as the rule
- B: also demolish or refund retired-recipe buildings on load

> **Recommendation:** A now; B only if old saves matter before v1.0.

*Files: `scripts/recipes.lua`, `docs/economy/PRODUCTION.md`*

### NR-1107 — A front key may now sit across the border
*novel-work · raised 2026-10-31 · from Lane F report, sprint 61 (BL-1441 front edge cases), 2026-10-31*

A span whose every tile an earlier span already claimed used to be dropped, leaving a front unnamed. It is now keyed by the lowest unclaimed tile on the neighbour's side. The lane wrote the rule into MILITARY.md § Fronts. Rival front assignment also now skips a front no march can reach (front_reachable); a player-assigned unit with a refused march is still re-issued each tick.

**Why it matters.** An authority-doc rule changed inside a defect fix. Digests did not move.

> **Recommendation:** Accept; the key is an identity, not a place.

*Files: `docs/military/MILITARY.md`, `src/world/fronts.cpp`*

### NR-1112 — State port siting has no doc home
*novel-work · raised 2026-10-31 · from Lane G report, sprint 61 (BL-1420 lane and port defects), 2026-10-31*

A coastal capital's state port sits on the coastal tile nearest the capital, a rule stated only in a code comment. On seed 40 that tile lay in a sea pocket joined to open water only across a hex diagonal, which the four-step lane walk cannot cross, so no lane left it. The lane added a preference for open sea (the largest four-connected piece of the six-connected sea) in code only.

**Why it matters.** A generation rule with no authority doc. It moves D_land on every seed with such a pocket.

> **Recommendation:** Write the siting rule, open sea included, into LOGISTICS.md § 4b beside the harbour rules.

*Files: `src/world/state_industry.cpp`*

### NR-1113 — Alarm's new vocabulary: five factors, three modifier subjects
*novel-work · raised 2026-10-31 · from Lane S report, sprint 61 (BL-1458 sentiment emitters, BL-1432 Alarm as sentiment), 2026-10-31*

To make Alarm's RELATIONS § 6 causes emit, the lane added five sentiment factors (technology_visible, facility_visible, border_force, trade_severed, unrest_visible authored zero), three modifier subjects on the meta layer (alarm_toward multiply, alarm_tell add in points, alarm_decay multiply) read by nation_tech.cpp, and widened the tech-store audit grammar (a points subject; "N% faster" reads as +N). A large facility is a state asset costing at least 450 (launchpad, airbase; not the university). 14 Digitisation effect rows now write Alarm; six stay unread (no Nuclear Plant, no accord-partner store, "spike" and "drift" ambiguous, no capability-only scale, no world-wide decay scale).

**Why it matters.** New META_LAYER vocabulary and five enumerators no doc named. The emitter module sentiment_emitters.cpp has no owning doc section beyond RELATIONS § Who emits.

> **Recommendation:** Accept; write the five factors into RELATIONS § Who emits and the three subjects into META_LAYER at merge.

*Files: `src/world/sentiment_emitters.cpp`, `src/world/modifier_set.hpp`, `tools/session/tech_tree_store.js`*

### NR-1126 — Major opening kit: most processor slots fail Pass 3, and majors run near break-even
*question · raised 2026-11-03 · from BL-1473 lane, branch worktree-agent-a1fb67f8f6d2e0152 at 14aefef5; quick-tier census 2026-11-03*

Quick tier (seeds 0, 28, 46, 12; 20:4; 20 quarters). Parity improves: treasury spread median 21x -> 2.0x (Standard) / 1.7x (Rich-core); buildings 13x -> 2.3x. Workability falls: Standard median net +1 per quarter, 8 of 16 majors losing money (baseline median +910, about a quarter losing); Rich-core median +255, 6 of 16 losing, none broke. Cause: kit processors fail Pass 3. Steel, refined fuel and power fail the want bound, which is body-wide, so the minors' walk already covers them; food rations, refined copper and machinery fail spare input, since one extractor per raw does not cover a plant at t_idle. Standard places a median of about 2-3 of its 6 processors. Also unbuilt: the logistics slots, so majors keep 2-47 nodes and their bills.

**Why it matters.** The doc says each kit processor runs on day one; on these seeds it mostly does not stand. The kit is not merged to main until this is decided.

- Exempt kit processors from the want bound (keep chain-feasible and sized)
- Read want at the major's own markets rather than the body
- Size kit extraction to its processors (enough feed for each slot)
- Build the logistics slot cap as the doc says

> **Recommendation:** The first and third together, then the fourth; re-run the census on both kits.

*Files: `docs/generation/CORPORATION_GENERATION.md`, `src/world/opening_kit.cpp`*

### NR-1127 — Novel seam: generation helpers exposed through corporation_generation_detail.hpp
*novel-work · raised 2026-11-03 · from BL-1473 lane*

The kit reuses Pass 3 helpers, so they moved from an anonymous namespace into namespace corpgen behind a new detail header. No doc owns that seam.

**Why it matters.** It lets other TUs call generation internals directly.

> **Recommendation:** Accept as the kit's seam; note it in CORPORATION_GENERATION.md if kept.

*Files: `src/world/corporation_generation_detail.hpp`*

### NR-1128 — Kit logistics cap: hubs stacked at the capital stay outside the two hub slots
*decision taken on your behalf · raised 2026-11-03 · from BL-1473 cold review, finding 6*

The cap counts every node on the capital tile as the capital's source, outside the slots, and stack_logistics_hubs may still stack hubs at a capped major's capital. Kept that way: stacked capital hubs are read as the source's capacity (the doc's "4 + the source"), so the cap bounds the network's reach, not the capital's throughput.

**Why it matters.** A major can carry more than two hubs' bill if its capital stacks. Counting them against the slots would bound the bill but could starve the capital's Logistic Points.

> **Recommendation:** Keep; revisit if the census shows capital stacks driving a major's bill.

*Files: `src/world/opening_kit.cpp`, `src/world/logistic_points.cpp`*

### NR-1130 — Rival embargo bars and the offer cooldown: calls taken in the relations lane
*decision taken on your behalf · raised 2026-11-03 · from BL-1480 (war loose ends), BL-1481 (rival embargo)*

(1) Embargo enacts above utility 0.20 and repeals below 0.10 - no range was ruled; at 0.30 no rival ever embargoed (0.10: 713 pair-ticks, 0.20: 38, 0.30: 4). Census: 3 enacted, 3 repealed, none on the player. (2) The 4-tick peace-offer cooldown sits in offer_peace itself, so it binds the player too; the ledger disables Offer peace and names the quarters left. (3) War dispatches post to Field with no speaker.

**Why it matters.** An embargo can prop itself up: it cuts the target's cargo, which removes the author's trade-exposure cost and raises its own Alarm through trade_severed, until decay lifts it. All three census embargoes still lifted.

> **Recommendation:** Keep all three; revisit the bars if embargoes stop lifting once Trust can fall further (NR-1129).

*Files: `src/world/corp_ai.cpp`, `src/world/war.cpp`, `src/core/session_history.cpp`*

### NR-1131 — Rivals fortify any border-province tile their units stand on
*decision taken on your behalf · raised 2026-11-03 · from BL-1478 (bunker retune)*

'Widen to border posts' was read as any of the rival's own tiles with its units on them, in a province that holds a tile of a hostile front - so a garrison posted inside a border province is fortified where it stands. Written into AI_OPPONENT.md § 11's bunker entry as the ruling's widening. Rival bunkers over 16 seeds: 1 -> 15 (13 completed); none for the player.

**Why it matters.** Narrower reading (garrison post tiles only) would build fewer. The roughly 15x bunker spend moved seed 46's G1t50 85.7 -> 83.1.

> **Recommendation:** Keep.

*Files: `src/world/corp_ai.cpp`, `docs/ai/AI_OPPONENT.md`*

### NR-1132 — Kit: a major's kept foundings no longer refund into the flat treasury
*decision taken on your behalf · raised 2026-11-03 · from BL-1473 fix round, release_kit_foundings*

The opening reserve refunded founding payments (state plants, ports) a major made from its history chest. With the flat treasury replacing that chest, every founding of a major is zeroed, kept or removed, so the reserve refunds nothing and every major enters the settle at exactly the kit treasury (seed 0: 1,442 credits of refunds gone on Rich-core). The brief named only removed foundings.

**Why it matters.** Without it a major that founded a kept port opens above the flat figure.

> **Recommendation:** Keep.

*Files: `src/world/opening_kit.cpp`*

### NR-1133 — Kit: refined fuel, power and ordnance slots are never wanted, so they substitute
*question · raised 2026-11-03 · from BL-1473 calibration, quick tier*

On every major the refined fuel, power and (Rich-core) ordnance slots fail the want bound and fall down the ladder to another processor. Nothing generation reads draws refined fuel; power want is per grid and grids read as fed; units' ordnance draw is not in the want reading. The doc says each kit processor runs on day one; these three never stand as named.

**Why it matters.** The kit's named energy and military-supply lines are absent from every major's opening; the slots become other plants.

- Count those real draws in the want reading (building fuel upkeep, unit ordnance, grid need)
- Replace the three slots in the kit with goods that are wanted
- Accept the substitution

> **Recommendation:** Count the real draws: the goods are wanted in play, and generation is blind to it.

*Files: `docs/generation/CORPORATION_GENERATION.md`, `src/world/opening_kit.cpp`*

### NR-1134 — Build times anchored at x2/3, so a mine takes 120 days
*decision taken on your behalf · raised 2026-11-05 · from BUILD_DAYS_SCALED (BL-1499)*

Ben chose "scale all down, keep the differences" without naming the anchor. Taken: extraction 180 to 120, the rest x2/3: bunker 60, processing 180, base 240, university 300, launchpad 360.

**Why it matters.** A processor at 120 instead would mean x4/9 and much faster builds overall.

### NR-1135 — Occupation defaults
*decision taken on your behalf · raised 2026-11-05 · from OCCUPATION_MOVES_FRONT (BL-1502)*

Taken: an empty tile is occupied on entry, with no stay needed; an occupied tile's output is denied to its owner and not gained by the occupier; occupation needs a war, in Ben's words "when at war", while push stays open under one-way hostility and without a war still stops at the first tile. Rivals' pushes take ground under the same rule with no new grant, since push is an existing verb.

**Why it matters.** Sets how fast wars move the map. Hostility without war taking no ground is the reading to confirm; the rival reading is the one to check against the AI register.

### NR-1136 — A convoy with no way round
*decision taken on your behalf · raised 2026-11-05 · from CONVOY_REROUTES_FRONTS (BL-1505)*

Ben chose re-route. Taken for the case with no route: dispatch refused with a surfaced reason; in-flight cargo returns to origin. A neutral third nation is not bound by the front.

**Why it matters.** Returning cargo is not captured, so war does not seize goods; capture was the other option on the form.

### NR-1137 — "Same amount" read as same at the handoff
*decision taken on your behalf · raised 2026-11-05 · from FLAT_HANDOFF_TREASURY (BL-1498)*

Today majors share 12,000 before the settle and reach 16,994-36,214 when play starts. Taken: one figure set at the handoff, after the settle.

**Why it matters.** Setting a value after the settle could read as a clamp; it is written as the kit's grant.

### NR-1138 — Small defaults on the UI list
*decision taken on your behalf · raised 2026-11-05 · from Sprint 63 playtest list*

Taken, stated to Ben before the form: scrub stays round; rail Track reads Railroad; drag-select is own units with shift to add; trades gate on the viewed market only; alerts are not dismissable; the side-by-side nation scan goes.

**Why it matters.** Each is a one-line reversal if he disagrees.

### NR-1139 — Schooling and University are not in the Build door
*question · raised 2026-11-05 · from CONSTRUCTION_FOLDS (BL-1513)*

Read from selection_panel.cpp:4764: the door lists Port, Marketplace, Launchpad, Hub and Station under Infrastructure, Base, Airbase, Bunker and Research Institute under Military, and no Schooling or University. The form did not ask.

**Why it matters.** If unintended, players cannot build them, and Research Institute under Military reads oddly. Fold them into the construction pass, or leave?

### NR-1140 — What 96 px does beyond the top rung
*decision taken on your behalf · raised 2026-11-05 · from TILE_RES_96 (BL-1523)*

Ben ruled the master at 96 px and the top rung lowered to about 96 px. Written beyond that: the lower zoom rungs halve from about 96; and a 4K-height window now magnifies the top rung about 2x (computed), where the 2026-10-10 ruling accepted about 1.7x. TECH_FOUNDATIONS.md keeps a benchmark measured at 128 px, labelled as such.

**Why it matters.** Halving lowers every zoom level, not only the closest, and 4K play looks softer than the accepted figure.

### NR-1145 — Novelty: research is priced as a share of the chest, so a smaller chest cuts research
*novel-work · raised 2026-11-07 · from Opening lane stage 1*

Research takes up to 14.3% of the chest per quarter, and points follow the square root of credits. A much smaller flat chest therefore also slows research, a coupling no doc states as an opening lever. The opening lane reads research per quarter on each chest rung.

**Why it matters.** The opening chest ruling may move the tech pace as well as construction.

### NR-1146 — Wind Power, Upkeep, University: the calls the small lane took
*decision · raised 2026-11-07 · from Sprint 63 small lane (WIND_TURBINE, RESEARCH_IN_RETURN, SCHOOLING_DOOR)*

WIND: a major at ring 2, cost 12, needing Fuel Economy III and V, the first node of each arm. Grammar says two minors, so this is a new shape; the alternative is a real cluster with two new minors you author. Rivals first differ at quarter 132 on seed 46, none within 40 quarters on seeds 0 and 46. UPKEEP: the Balance segment "Force" became "Upkeep", because one figure holds garrison and space; a separate line needs a new saved field. LAUNCH COST stays outside the return, as construction does. UNIVERSITY: 100 RP a quarter against an institute's 50, at 400 cr against 300, on a constant still marked unmeasured, and it also raises qualification, so it is better on both counts. No tree node gates Schooling or University, so both open on day one subject only to the stratum gate. On the verify world the player holds no City, so University mostly refuses. OLD SAVES: a queued Wind Power with Fuel Economy V not held waits forever and reads "not in 120 y"; a shift-click off and on recovers it, and a rival in that state stops picking. The state needs an old save.

**Why it matters.** University is now reachable on an unmeasured number, and wind is a new shape in the tree's grammar.

> **RESOLVED.** Ben, 2026-11-09: WIND keeps the new shape. The Upkeep, launch cost and University calls were not asked.

### NR-1148 — Rival march supply: the calibrations and readings inside your grant
*decision · raised 2026-11-07 · from BL-1540 (rival march supply)*

You allowed fall back on 2026-11-07. Taken inside the grant: plain ground is worth 500 and a goal 1000, so plain ground stops being worth a march at about share 590. A front stops being a reason once all of a nation's goals in that war are taken; a nation with no goals keeps its reason. A force with goals left but refused the push holds on hungry ground, because a fall back would re-assign straight back with no stored state. Liberation of own occupied ground is weighed as plain ground. The pool is not read, and 10 units starved on pool shortage at 20 quarters.

**Why it matters.** These set when a rival goes hungry for a prize, and at 20 quarters 6.2% of opening units still starve.

### NR-1149 — Novelty: the settle prices buildings separately from play
*novel-work · raised 2026-11-07 · from BL-1497 (opening cost curve)*

Foundings and every settle tick use the authored bills; play uses the raised curve, through a registry swap. That holds every kit identical, and it means 31 sites on 23 majors still building at handoff draw the live bill for their remaining days.

**Why it matters.** Two price tables for one building are a new seam, and first-quarter bills are not perfectly equal between majors.

### NR-1150 — Rival march supply: three calls after merge
*question · raised 2026-11-07 · from BL-1540 (rival march supply)*

CLOCK: rivals score fronts once a year, while a unit with no supply dies near day 210. Scoring fronts every quarter (and nothing else) takes men lost from 6.7% to 4.4% at 20 quarters, but the march-off-and-return loop count goes from 9 to 31. Not shipped. HOME LEAK: 5 of 21 fall backs at 20 quarters land in the capital province and are then posted home for good; telling them from the deliberate home garrison needs a new saved field. RANGE: ground within 8 straight hex steps of a goal is worth the goal; past it a hungry march holds, 7 refusals in 20 quarters across 16 seeds, all with a goal left.

**Why it matters.** Each sets how often rivals lose men to hunger against how steadily they hold a war.

> **RESOLVED.** Ben, 2026-11-09: CLOCK: score fronts quarterly. HOME LEAK and RANGE were not asked.

### NR-1151 — Opening climb: where it lands, and the calls taken inside it
*decision · raised 2026-11-08 · from BL-1497 (opening cost curve) and BL-1545..1549 (steel trade rulings)*

Measured on the merged lane, 16 seeds, 64 majors, 80 quarters: the reference mine costs 5.2 months of net at the opening and 3.9 at year 20; 10 of 64 majors reach 1 month; median 4 buildings completed by year 20; 7 majors never build; 1 below zero by q80. The target of about 1 month at year 20 is not met. Calls taken: the Logistic Point constant 3.98 was calibrated on the summed q20 draw (the median major draws 0); the dead corporation-era Marketplace estimate in corp_ai.cpp was converted to points, not deleted; settle-day construction arrears bill at the authored row. Late trade fell 17% and late stranded coal rose 8.0 -> 14.3 after the review fixes, cause untraced. The kit Marketplace laid a new building for only 4 majors.

**Why it matters.** The climb is the sprint's top item and lands at about a third of the target pace.

> **RESOLVED.** Ben, 2026-11-09: playtest the opening first, then decide.

### NR-1152 — Doc drift: two claims the code cannot settle
*question · raised 2026-11-08 · from BL-1525 (playtest doc drift)*

WATER: SELECTION.md and resolve_unit_order ignore every water tile on a right-click, but march_check accepts a land unit onto its own coastal water, naval types exist, and MILITARY.md says the owned-coast rule stands for marches. So a right-click can never order a march the verb allows. COMPETITION: MENU.md says competitors' buildings appear in the Market Ledger; the ledger only uses buildings to list which nations are present (nations_in_market), and ledgers/market.md has no such surface.

**Why it matters.** Each is either a doc to correct or code owed; the docs are the authority, so neither can be settled by reading the code.

> **RESOLVED.** Ben, 2026-11-09: WATER: right-click stays land only; the march verb and doc narrow to match. COMPETITION was not asked.

### NR-1153 — Elimination defaults: treasury to the conqueror, debt written off, a lost campaign spectates
*decision taken on your behalf · raised 2026-11-09 · from main session, designing elimination on Ben's form answer*

Ben asked to design elimination of a landless nation. Taken: wars close as white peace with no truce; relation rows clear; units disband; a positive treasury passes to the conqueror (conserves money); a debt is written off; the nation stays a flagged record; the player's end is a lost campaign, then spectate.

**Why it matters.** Each default moves what conquest pays and what losing means.

- Keep the defaults
- Treasury destroyed instead of passed
- Debt inherited by the conqueror
- Player's end returns to the menu instead of spectating

> **Recommendation:** Keep the defaults.

*Files: `docs/politics/NATIONS.md`, `docs/politics/RELATIONS.md`*

### NR-1154 — Elimination as built: five calls, and rivals may now end the player
*decision taken on your behalf · raised 2026-11-10 · from war lane building BL-1562 (war both hostile, elimination), commit 0eba4369*

Taken while building: (1) elimination fires on no LAND province; coastal-water provinces pass to the conqueror with the cession; (2) every truce naming the ended nation is erased, including the one just written with the conqueror; (3) sentiment rows to and from it are deleted, generation grudge seeds kept as history; (4) its live capital is cleared, its generation capital kept as identity; (5) a debt written off sets the treasury to 0, which creates money outside the money books. Consequence of the legal verb under the existing AI grant: a rival imposing peace at 100 can now cede the last province and end any nation, the player included. Rival defenders push from the declaration rather than after answering.

**Why it matters.** Call 5 breaks money conservation; the AI consequence means a campaign can be lost to a rival peace term.

- Keep all as built
- Debt passes to the conqueror (conserves money)
- Rivals may not impose a last-province cession on the player

> **Recommendation:** Keep 1-4. Ben to choose on 5 and on the rival ending the player.

*Files: `src/world/war.cpp`, `docs/politics/NATIONS.md`*

### NR-1155 — Population growth as built: 1.18%/yr pooled, and centres demote on their first bad quarter
*question · raised 2026-11-10 · from population lane building POPULATION_GROWTH_RATE (growth as a rate), commit 317c6bfe*

16 seeds, 40 quarters: pooled growth -0.22 -> 1.18%/yr (target about 2%); a third of centre-ticks still fail a floor and shed 1.6%/yr. Seeds 92/17/27/13 grow 2.0-2.3%; seed 64 is flat. Handoff shape: centre heads +3.5%; villages 245 -> 305, towns 274 -> 249, cities 248 -> 212. Cause of the shift: generation seeds every centre at exactly its rung headcount, and the first failing quarter now sheds 0.4% and demotes at once (the old step waited 10 quarters). No nation reaches qualification 0.8 on any library seed.

**Why it matters.** The opening map loses a seventh of its cities to a rounding edge, and growth runs below the anchor.

- Add a demotion margin (demote below e.g. 90% of the rung)
- Seed centres above their rung at generation
- Calibrate growth: staples floor, decline rate or staple supply
- Accept as built

> **Recommendation:** A demotion margin first, then re-measure growth before touching the dials.

*Files: `src/world/economy_system.cpp`, `src/world/population_generation.cpp`, `docs/economy/POPULATION.md`*

### NR-1141 — Naval design details I filled in behind your form
*decision taken on your behalf · raised 2026-11-05 · from SEA_THEATRE (BL-1430), MILITARY.md § Sea*

Your form settled the shape; I wrote these to make it whole. (1) A blockade closes the port for everyone, neutrals included, with no Alarm cost to the blockader. (2) Prize cargo goes to the raider's nearest own port in range, or is sunk. (3) An escort covers its own nation's cargo, not a friend's. (4) Submarines count for nothing in sea control. (5) Mission reach is a per-class range over water from any own port. (6) A landing is refused on a neutral's coast. (7) Ship hulls are one new good, made at the Arms Works. (8) Sea fights keep no battle record: a damaged ship breaks off home. (9) A ship's attack never reaches the shore, since you did not pick bombardment.

**Why it matters.** Items 1, 3 and 6 decide how third parties feel a sea war; item 8 means sea fights read differently from land battles.

### NR-1181 — Seven sea-day details taken inside the sea theatre core
*decision taken on your behalf · raised 2026-11-06 · from SEA_THEATRE (BL-1430)*

Taken so the core could land; each is one line to change. (1) A ship working a zone spreads its presence 1/N over the zone's provinces, as a wing's day does. (2) A ship fires when its side found a target even with no mission; a broken-off ship does not fire. (3) Refit at the home port costs goods only, no recruits. (4) A hire spends sea recruits and refunds them only if its port is lost before it stands. (5) A ship working a zone that holds its own mouth does not mend there; only a ship broken off or recalled does. (6) A ship that loses its home re-homes to the nearest own port BY WATER, and disbands if it can reach none, even with a port on another sea. (7) A blockade order is legal on any in-range water; its target and effect arrive with BL-1530 (sea control and blockade).

**Why it matters.** (1) makes a larger zone thinner, which is the trade-off the zone choice is meant to carry. (2) means a ship in transit can start a fight.

### NR-1182 — Ship refit: four calls taken inside the ship kit
*decision taken on your behalf · raised 2026-11-06 · from SHIP_KIT (BL-1527)*

Taken so the item could land; each is small to change. (1) A refitting ship is in dock: it neither fights nor can be hit. A land refit fights on its old kit, so this differs, and a ship can dodge fire by refitting, at the price of goods and its muster days. The alternative is hittable in dock. (2) In dock it still draws quarterly supply and does not mend. (3) A broken-off ship must be recalled to no mission before it can refit, as MILITARY.md says. (4) Base kit draws nothing beyond the hire goods, mirroring airframes, so ship hulls have no demand until the naval branch (BL-1528) opens later kit.

**Why it matters.** Call (1) sets how a blockaded fleet can behave once BL-1530 (sea control) counts presence: a docked ship would not count, and could not be sunk at its mouth.

### NR-1183 — Raid: calls taken inside BL-1529 (patrol escort raid)
*decision taken on your behalf · raised 2026-11-06 · from SHIP_KIT (BL-1527)*

Taken so the item could land; each is small to change. (1) A refitting ship is in dock: it neither fights nor can be hit. A land refit fights on its old kit, so this differs, and a ship can dodge fire by refitting, at the price of goods and its muster days. The alternative is hittable in dock. (2) In dock it still draws quarterly supply and does not mend. (3) A broken-off ship must be recalled to no mission before it can refit, as MILITARY.md says. (4) Base kit draws nothing beyond the hire goods, mirroring airframes, so ship hulls have no demand until the naval branch (BL-1528) opens later kit.

**Why it matters.** Call (1) sets how a blockaded fleet can behave once BL-1530 (sea control) counts presence: a docked ship would not count, and could not be sunk at its mouth.

### NR-1184 — Blockade: calls taken inside BL-1530 (sea control and blockade)
*decision taken on your behalf · raised 2026-11-06 · from SHIP_KIT (BL-1527)*

Taken so the item could land; each is small to change. (1) A refitting ship is in dock: it neither fights nor can be hit. A land refit fights on its old kit, so this differs, and a ship can dodge fire by refitting, at the price of goods and its muster days. The alternative is hittable in dock. (2) In dock it still draws quarterly supply and does not mend. (3) A broken-off ship must be recalled to no mission before it can refit, as MILITARY.md says. (4) Base kit draws nothing beyond the hire goods, mirroring airframes, so ship hulls have no demand until the naval branch (BL-1528) opens later kit.

**Why it matters.** Call (1) sets how a blockaded fleet can behave once BL-1530 (sea control) counts presence: a docked ship would not count, and could not be sunk at its mouth.

### NR-1185 — Landings: calls taken inside BL-1531 (troop landings)
*decision taken on your behalf · raised 2026-11-06 · from SHIP_KIT (BL-1527)*

Taken so the item could land; each is small to change. (1) A refitting ship is in dock: it neither fights nor can be hit. A land refit fights on its old kit, so this differs, and a ship can dodge fire by refitting, at the price of goods and its muster days. The alternative is hittable in dock. (2) In dock it still draws quarterly supply and does not mend. (3) A broken-off ship must be recalled to no mission before it can refit, as MILITARY.md says. (4) Base kit draws nothing beyond the hire goods, mirroring airframes, so ship hulls have no demand until the naval branch (BL-1528) opens later kit.

**Why it matters.** Call (1) sets how a blockaded fleet can behave once BL-1530 (sea control) counts presence: a docked ship would not count, and could not be sunk at its mouth.

### NR-1186 — Sea UI: calls taken inside BL-1534 (sea UI), and its live click is owed
*decision taken on your behalf · raised 2026-11-06 · from SHIP_KIT (BL-1527)*

Taken so the item could land; each is small to change. (1) A refitting ship is in dock: it neither fights nor can be hit. A land refit fights on its old kit, so this differs, and a ship can dodge fire by refitting, at the price of goods and its muster days. The alternative is hittable in dock. (2) In dock it still draws quarterly supply and does not mend. (3) A broken-off ship must be recalled to no mission before it can refit, as MILITARY.md says. (4) Base kit draws nothing beyond the hire goods, mirroring airframes, so ship hulls have no demand until the naval branch (BL-1528) opens later kit.

**Why it matters.** Call (1) sets how a blockaded fleet can behave once BL-1530 (sea control) counts presence: a docked ship would not count, and could not be sunk at its mouth.

### NR-1187 — Opening fleets: calls taken inside BL-1532 (opening fleets)
*decision taken on your behalf · raised 2026-11-07 · from OPENING_FLEETS (BL-1532)*

Taken inside your form answers so the item could build; each is one line of scripts/world_gen.lua or one rule to change. (1) Minors open with no ship: "fixed kit fleet" read as no history fleet at all, and the kit is a major's. Minors will raise fleets only through BL-1533 (rival fleets). (2) The fixed shares are two destroyers, one capital ship, one submarine, one transport: five ships, list value 7,200 credits at hire prices. (3) A port berths three opening ships; the rest spill to the next-nearest sea port. With the kit capped at one port, most majors berth all five at one port. (4) "Nearest the capital" is straight-line hex distance, tile id breaking ties. (5) The sea pool opens on 100 recruits, enough to raise one capital ship or three destroyers. (6) "Prefer the sea": the kit cap's LAST port slot admits only a sea-mouthed port while one stands on the major's ground and none is kept; kit cover still picks which. A lake port is still legitimate logistics (lanes cross lakes), so generation still places them. (7) Opening ships lie docked at their mouths with no mission, and a docked ship counts as present in its mouth's province, so two majors hostile at the opening that share a coastal sea province would fight from day one. None did in the 16 seeds' settle (no ship lost HP), but play may. Alternative: docked ships with no mission are not engaged until a war is declared in play.

**Why it matters.** Call (7) decides whether a fleet can be sunk in harbour before its owner has given an order. Measured: 16 seeds, 425 ships, 85 of 94 majors with a fleet, cover unchanged by the cap rule (76 -> 76).

### NR-1190 — Arms Works open unfed where steel or machinery is out of reach
*decision taken on your behalf · raised 2026-11-08 · from FORCE_EQUIPMENT (BL-1551)*

Most majors cannot feed an Arms Works at generation: their machinery slot itself fails for want of refined copper. The kit then substituted the arms slots into more steel, silicon or rations, so no major held a works. To honour "majors open with Arms Works" the arms recipes are exempt from the want and feed gates: the works stands idle until its inputs arrive, and pays its upkeep meanwhile.

### NR-1191 — Force sizing: income read as operating surplus; the landing hire sits outside the target
*decision taken on your behalf · raised 2026-11-08 · from FORCE_SIZING (BL-1552)*

Two calls. Income is the mean of the last four returns of income plus taxes less expenditure, maintenance and wages, floored at zero, so a nation is not sized on gross takings it has already spent. The landing force hired for a ready transport (Ben, 2026-11-08) stays outside the target, since it is a mission need rather than a standing force.

### NR-1192 — Minor income: floor applies to every nation; garrisons use the 0.50 ceiling; digests moved
*decision taken on your behalf · raised 2026-11-09 · from MINOR_PLANT_FLOOR (BL-1616), GARRISON_INCOME (BL-1617)*

Three calls. The plant floor visits every nation, because majors are not known until after the walk; the kit then replaces a major's floor firm. Garrisons are paid at the 0.50 force ceiling, not the threat-scaled share, so a minor is not stripped by its first reading. The opening chest cut now reserves only the 20-man token's pay. Measured on four seeds: 22 of 112 minors still open plantless, and 15 of the 23 broke rivals at tick 40 are among those. World digests moved and wait on the re-bless.

---

## Resolved

Kept, not pruned: the reasoning is the point. Prune only in a deliberate sweep, once the
answer has landed in an authority doc.

### NR-1143 — Research spends outside net: should it count, and how does it share with construction?
*question · raised 2026-11-05 · from BUILD_PAYBACK_CENSUS (BL-1496)*

The research line takes a weighted share of the treasury, up to what the queue owes, as a direct expenditure the quarterly return does not carry. Mean 1,644 credits a major-quarter over 20 years, against net of 1,000-2,500. So the header NET and the Earnings chart overstate free cash, and "a mine costs six months of net" means different things before and after research. Recommended: book research in the return, so NET is the money left; the mine target then reads net after research.

**Why it matters.** A player with a research queue may have little left for construction under the new prices, and a share of treasury shrinks as the opening treasury falls. This sets how research and construction compete at the opening.

> **RESOLVED.** Ben: "Yes, count research." Research counts inside net; written into FINANCE § The quarterly return; the work is BL-1539 (research in return).

### NR-1144 — Sprint 63 defaults taken without a form question: overturn any you dislike
*decision · raised 2026-11-07 · from Sprint 63 lanes*

MARKET: pip rather than glyph on the Build door; name column 72 px. CANVAS: rival convoy card; Total force reads a unit count; no hover on a convoy marker. SHELL: UI scale 0.875; exit warnings; a single quicksave behind Load. WAR: stranded cargo rule; retreats occupy; muster allowed on occupied ground; no territory tint for occupation. ART: default framing off the ladder (zoom 4/3); top rung 2x at 4K; scrub round, forest fringe pointed. CONSTRUCTION: 0-cover rows never warn; a rising shelf gets no credit; a 7-good basket may scroll. GENERATION: seed 25 loses one station.

**Why it matters.** Each was small enough to take as a default, and each is visible in play.

> **RESOLVED.** Ben, 2026-11-09: defaults accepted, with one addition: fronts fit land tiles only, not coast.

### NR-1147 — Novelty: a major with no cluster of its own enters the tree grammar
*novel-work · raised 2026-11-07 · from Sprint 63 small lane*

docs/economy/RESEARCH.md gains a sentence for a major that needs one node of each arm rather than two minors. The wording is the lane's, not yours.

**Why it matters.** A grammar exception is how a tree loses its grammar.

> **RESOLVED.** Ben, 2026-11-09: keep the new shape (one node of each arm).

### NR-1142 — Does offshore drilling join sprint 64?
*question · raised 2026-11-05 · from OFFSHORE_DRILLING (BL-1522)*

You ruled offshore drilling lands with the naval system, and it is parked on that. It is a separate build: new sea petroleum moves world generation, TILES.md must change, and shelf-only against open ocean is unruled. I left it out of sprint 64's plan.

**Why it matters.** Adding it enlarges an already five-difficulty sprint and adds a second world-mover to its re-bless.

> **RESOLVED.** Ben, 2026-11-05: keep it out of sprint 64. BL-1522 (offshore drilling) stays parked.

### NR-1188 — Opening fleets: the transport can berth where no land unit can reach it
*question · raised 2026-11-08 · from OPENING_FLEETS (BL-1532) live click*

Live click, Quick Start seat Keitoheim Meido, 1960 Jan 26. The kit lists the transport fifth, so with three ships to a berth it spills to the second sea port. Here that is Dutum, a small island port with no military base and no land unit. Boarding needs a land unit on or beside the transport's own port, a transport takes no order to sail to another port, and land units are raised only at a base. So this seat's transport can never be boarded, and the landing can never be clicked. Tukornom, the capital port and the only base, holds the three warships.

> **Recommendation:** A, cheapest and always legible; C is worth a backlog item on its own merits.

> **RESOLVED.** Ben, 2026-11-08: A, transport first. Both kits list the transport first, so it berths at the capital port; written into MILITARY.md § Sea.

### NR-1189 — Rival nations' land hire: MILITARY.md says the build scorer hires, the code hires only for corporations
*question · raised 2026-11-08 · from RIVAL_FLEETS (BL-1533) landing force*

MILITARY.md § The muster interface says rival nations hire land units through the build scorer, carried to the nation unchanged (Ben, 2026-10-23), under a soft cap of three. The scorer in src/world/corp_ai.cpp scores a hire only when the actor is not a nation (a BL-1401 comment: a corporation's only), and corporations are retired. So today no rival nation hires a land unit in the build pass. The landing force (Ben, 2026-11-08) now hires one for a ready transport, inside the sea pass; that is the only rival land hire running.

> **Recommendation:** A reads truest to the 2026-10-23 ruling, but it is a world-mover; B is a doc edit today. Your call.

> **RESOLVED.** Ben, 2026-11-08, by the force-sizing form: rival nations hire land units again, up to the income target (MILITARY.md § Force sizing). The work is BL-1552 (force sizing).

### NR-1156 — A blockade moves prices both ways: what it re-ships falls, what it imports rises
*question · raised 2026-11-10 · from NAVAL_WAR_READING (BL-1535)*

MILITARY.md § Blockade says the cost of a closed port lands on the blockaded market's prices. On the three library seeds where a blockade closed a port, it did not. Blockaded price against its own price the tick before closing, netted of the other markets' move over the same span: seed 0 0.998 vs 1.040, seed 31 1.038 vs 1.038, seed 60 1.055 vs 1.026 (110, 864, 880 readings). The blockaded markets read 14-24% below other markets, but they were cheap before closing. Likely cause, unconfirmed: each market has other ports or land routes, so one closed port reroutes trade without shortage. CORRECTED 2026-11-10 (Ben chose C, "Make closure bite"; the reading below came after, and changes the premise). The all-goods mean hid two opposite moves. Each closure shut 1 of its market's 5-7 ports. On seed 31 it cut landings 67% (sea 1041 -> 108 a tick) but cut the market's own buying faster: steel demand 73 -> 21 against supply 27 -> 11, all of it in the construction/upkeep/trade channel (no household, background or processor steel), so steel fell 20% against +8% elsewhere and silica's shelf grew 14 -> 34. Likely cause, unproven: the closed port's trade points went, so the owner's outbound trades stopped buying there. Pure imports did rise: seed 60 tobacco +88% against +53% elsewhere. Open oddity: seed 31 machinery fell 473 -> 370 with supply 1.7 -> 0.1 and demand flat; not yet explained.

