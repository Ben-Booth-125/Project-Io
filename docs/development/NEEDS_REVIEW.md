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

*118 entries — 61 open, 57 resolved.*

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

### NR-1215 — Missiles: a magazine assembled from kit goods, or a market good of its own?
*decision taken on your behalf · raised 2026-11-11 · from BL-1431 (missiles and satellites) lane*

MILITARY.md § Missiles says a missile is a good fired by a launcher and consumed. The lane built it instead as a per-nation magazine (up to 64 of each kind), stocked by buying kit goods off the nation's own shelves at posted prices. That avoids widening the resource arrays, an envelope bump and moved digests, but a missile is then never traded, priced or shipped. It also took two first-cut calls: tactical warheads need the Uranium Enrichment node, and the rival scorer prices nuclear Alarm at 100 credits a point.

**Why it matters.** A market good makes missiles a trade lever (embargo, scarcity, sale to a rival). A magazine keeps them a pure force cost. The doc now says both and must say one.

- Keep the magazine; reword the settled line
- Make missiles market goods (resource arrays widen; envelope bump; digests move)

> **Recommendation:** Keep the magazine for v0.3.0: the force cost is what the design leans on, and goods trade can follow. Uranium Enrichment and the Alarm price are in-band first cuts.

*Files: `docs/military/MILITARY.md`, `src/world/missiles.cpp`, `src/world/missile_params.hpp`*

### NR-1216 — UI scale: should the canvases shrink with the UI, or keep their size?
*decision taken on your behalf · raised 2026-11-11 · from BL-1495 (UI scale down) lane*

The 2026-11-05 mechanism drew the whole frame at 0.875, canvases and hit-testing included. Your 2026-11-10 ruling says the default is a 12 px font and everything in the chrome scales with it. The main session's brief added "canvas ground does not scale", which you did not say, and the lane removed the 0.875 frame scale on that line. So the map now draws at 1.0: about 14% larger on screen than before, while the chrome is 0.75 of its authored size and the canvas gains the room the chrome gave up.

**Why it matters.** Tile, unit and marker sizes on the map read about 14% larger than at your last playtest. That is a look call only you can make.

- Keep the canvas at 1.0 (the map gains room and keeps tile size)
- Scale the canvas with the UI factor too (the map shrinks with the chrome)
- Restore a separate canvas factor (e.g. 0.875) on its own dial

> **Recommendation:** Keep the canvas at 1.0: the map has its own zoom, so a smaller tile is a zoom-out away. Decide at the live look.

*Files: `docs/ui/LAYOUT.md`, `src/ui/ui_scale.cpp`, `src/core/app.cpp`*

### NR-1163 — v0.3.0 (military) is not cut and the naval branch is unmerged, while v0.5.0 builds space war
*question · raised 2026-11-10 · from space-mechanics session, standing rule on milestone order*

ROADMAP says the numbers are naming order, not build order, and you asked for v0.4.0 and v0.5.0 now. But space battles, blockade and war-on-every-body build on the campaign military, and sprint 64's naval system sits on claude/naval-warfare-system-b0d978, unmerged. Space combat is built standalone on the orbital graph, not on the sea theatre.

**Why it matters.** If the naval branch merges later it may want to share interdiction rules with space convoys.

- Fine as is
- Merge naval first and reconcile interdiction

> **Recommendation:** Fine as is; reconcile at v0.6.0.

*Files: `docs/military/SPACE_FLEETS.md`*

### NR-1203 — Launch goods sit on the wrong shelves: their trade routes rank too low to move (measured)
*question · raised 2026-11-10 · from BL-1630 lane report*

With a staffed pad bidding for its launch lot, plants appear (seed 0: 5 components, 10 propellant by 2000), but the goods pile on different markets of the one body: 2015, 47582 holds 1,468 components and 21 propellant; 47599 and 47665 hold 1,542 and 788 propellant. 0 launches through 2015. MEASURED (experiment lane, seed 0): the ceiling does silence some wants, but auto trade already sizes to them (BL-1547, dispatch_absorbable), and every route exists overland. What blocks is RANKING: auto trade ranks by margin per trade point, and launch goods move few units per point (components 0.13, propellant 0.6), so their routes rank #68-#141 of ~250 at the source and the points run out first (272 "no points left" against 3 sends in the sample). Logistic Points by value also bite: a component draws 77.9 LP, a propellant unit 16.1. EXPERIMENT (branch worktree-agent-a9f2b8a287ef3a8e6, 8aa5f49b, NOT merged): routes into a market with a silenced want rank first. Result: first rival launch 1992, 3 by 2005, 4 by 2015 (vs none). Cost: pads completed by 2000 fall 7 to 4; treasuries 2015 947k to 629k; market_viability quick G1 at tick 50 86.0 PASS to 80.8 FAIL (target 84). It moves every good from ~1970. OPTION D MEASURED (branch worktree-agent-a83d4ca4b411f9d7d, bbcb17b5, NOT merged; switch launch_draws_body_wide, off byte-identical): a good the home shelf cannot cover may come whole off another market on the home body where the nation owns ground, ascending market id, at that market's price, no haul charge. Seed 0 to 2010: first rival launch 1993; 4 launches by 2000 (Teishe: a comms satellite, then a station core; Thuamuas and Tososhei: comms satellites); none added 2000-2010; 7 of 11 padded nations still refused at the shelf, cause unprobed. Pads unchanged (7 by 2000, 11 by 2010). Treasury sum 2000: -87k. market_viability quick: identical (it ends ~1972, before Rocketry, so it cannot see D). RULING (Ben, 2026-11-10): D is rejected; no global pools for goods (written into SPACE_ASSETS.md). Measure B next. Ben also raised a fourth shape: a Launchpad produces boost, a good on its market, so the economic draw happens over time; a launch consumes boost by the distance in space. FORM (Ben, 2026-11-10): build boost now alongside the B measurement: a pad burns propellant and components into boost, which stays on the pad's market (SPACE_ASSETS.md § Boost). Work: BL-1637 (launch boost). B MEASURED 2026-11-16 (main 59663e5a, launch boost merged; space_launch_census --stage-space, seeds 0, 46, 7, 13 to 2030; data-only worktrees, not merged): spacecraft components and propellant raised from the table's ~40-credits-a-point line to 2.4x it (0.31 / 1.5, steel's precedent) and to 10x (1.3 / 6). Rival launches by 2030: seed 0 6 -> 8 -> 16 (first 2000 -> 1996 -> 1994); seeds 46, 7, 13 stay 0 at every dose. The probe says why: on 46, 7 and 13 no component plant is ever built through 2010 (0 plants, 0 units on any shelf), and on seed 0 plants appear only from 1985 (9 by 2010) and every unit sits on the one market that makes it. Capacity moves what is made; the wall on most worlds is that nothing makes components. Components carry no background demand (recipes.lua id 13, BL-365); a pad's intake want is their only buyer, and a plant needs alloys + electronics or machinery + steel on its own market.

**Why it matters.** The wall between rivals and space is a trade-ranking rule. The one lever tried works for space and hurts the wider economy.

- A) Ship the experiment (silenced-want routes first) and accept the G1t50 cost
- B) Give dense goods more trade capacity per point (the capacity table, TRADE.md)
- C) Relax the by-value Logistic Point draw at starved anchors
- D) Let a launch draw its lot from every market on its home body (a space rule, not a trade rule; SPACE_ASSETS.md says the home market)

> **Recommendation:** D first: it opens space without moving any other good, and its cost is one rule in SPACE_ASSETS.md. It stalls at 4 launches, so expect B (dense goods' trade capacity) to be wanted too; A fails a gate as it stands.

*Files: `src/world/trade.cpp`, `docs/economy/TRADE.md`, `docs/economy/SPACE_ASSETS.md`*

### NR-1205 — Experiment lane went past its brief: changed route ranking, not sizing
*novel-work · raised 2026-11-10 · from NR-1203 experiment lane*

Briefed to make auto trade size off the silenced want, the lane found that already built (BL-1547) and instead ranked silenced-want routes first, behind a tunable. It is on its own branch, unmerged. Ben, 2026-11-10: delete the D branch; keep A until B reads.

**Why it matters.** Recorded so the branch is not mistaken for a delivery.

- Keep the branch for reference
- Delete it

> **Recommendation:** Keep until NR-1203 is ruled.

*Files: `src/world/trade.cpp`*

### NR-1220 — Generated body mass: bands and four calls taken; the inner planet's air now varies by seed
*decision taken on your behalf · raised 2026-11-10 · from BL-1638 lane report*

Bands (PLANETOLOGY.md): planet R 0.30-0.50 R_E, moon 0.20-0.34, asteroid 0.010-0.075, core fragment 0.015-0.045; density from core mass fraction and mantle density for every kind. Seed 0: Cinder 0.059 M_E, Selene 0.0050, Pallas 0.0000054; seed 46: Cinder 0.113. Climbs on seed 20260911: home 12.16, Cinder 2.10, moon 1.89, Pallas 0.29 (all read 8.0 before). Calls: (1) bands live in a C++ table beside the chain's other ranges (no data/params file exists; Lua would split the wizard preview from the build); (2) one density rule for every kind, the planet included (its band reaches 0.12 M_E, where self-compression is a few percent); (3) logistics keeps its scale by reading 1.0 for a non-home body on a world with a home body, not by a saved field; (4) Selene's parent mass stays the authored 1.0 for tidal heating, not the homeworld's corridor mass (outside the brief). Moved: Cinder's atmosphere class now follows its rolled mass (seed 46 none to thin; seed 20260911 thin to none); world_determinism's printed digests move; the homeworld's tiles are byte-identical on three seeds. Low-orbit slots stay 8 on every non-home body (all under 0.25 M_E).

**Why it matters.** Call (4) leaves a moon's tidal heating keyed to a fixed parent mass while the homeworld's mass varies 0.66-1.48.

- Accept
- Feed Selene the homeworld's rolled mass as its parent

> **Recommendation:** Accept, and file (4) as a small follow-up.

*Files: `src/world/planetology.cpp`, `docs/generation/PLANETOLOGY.md`, `src/world/logistics.cpp`*

### NR-1221 — Space merge: five space-line save versions can no longer be read
*decision taken on your behalf · raised 2026-11-11 · from space merge (claude/space-mechanics-design onto main)*

Both lines wrote world saves v81..v89 with different layouts. The merge mints v93 (main's v89 sections, then every space section in the space line's order) and envelope v30 (main's v29 plus v28's pins). Still read: every main version, the space line's v84..v87 and v90..v92 (told apart by number), and envelopes v22..v30 including the space line's v28. NOT readable: space-line v81, v82, v83, v88 and v89 - each number is main's, so such a file is read as main's layout and refused or misread. The space harnesses' five pre-section writers for those shapes (pre_early_space, pre_space_nodes, pre_space_units, pre_space_bombard, pre_offworld_bases) now write main's v89, and their checks compare against v89 plus every space section.

**Why it matters.** A space-branch save made at one of those five versions will not load. Only saves made on the unmerged branch are affected.

- Accept: branch saves are disposable (seed is the save)
- Add a distinguishing marker and read them too

> **Recommendation:** Accept - the seed library is the save, and the readable space versions cover the branch's later saves.

*Files: `src/world/world_save.hpp`, `src/world/world_save.cpp`, `src/core/save_game.hpp`, `src/core/save_game.cpp`*

### NR-1222 — Space merge: main's naval quest Blue Water keys a new reserved Early Space branch, Orbital Fleet
*decision taken on your behalf · raised 2026-11-11 · from space merge (claude/space-mechanics-design onto main)*

The Early Space store's check requires every Digitisation quest to key an Early Space branch. Main added the naval quest DG-ST-NV-Q (Blue Water); the space line had no branch for it. The merge adds a seventh reserved branch, Orbital Fleet, keyed by Blue Water, so the check passes.

**Why it matters.** Which space branch a sea-power quest opens is a design call; the merge picked a placeholder.

- Keep a reserved Orbital Fleet branch
- Have Blue Water key the existing Space Force branch instead

> **Recommendation:** Keep the reserved branch until the Early Space tree is designed past its reserved rows.

*Files: `docs/economy/tech/early_space_tree.json`, `docs/economy/tech/EARLY_SPACE_TREE.md`*

### NR-1223 — Monthly tick: market price smoothing kept at 0.5 a tick, so prices settle three times faster in calendar time
*decision taken on your behalf · raised 2026-11-12 · from BL-1642 (monthly tick) lane and cold review*

k_price_smoothing was re-derived to 0.2063 a month (the quarter rate spread over three ticks). At that rate processors sat unbid over the fair-price ceiling, because monthly supply arrives in lumps. The lane kept 0.5 per tick: prices now close half the gap to target in a month, not a quarter. Measured: market_viability quick G1 86.8 / G1b 3.2 merged, against main 83.6 / 5.2.

**Why it matters.** Price speed is what the player reads as market volatility; the ruling said every per-tick flow becomes a third, and this one does not.

- Keep 0.5 a tick (faster calendar settling, measured better)
- Restore the calendar rate (0.2063) and fix the lumpy-supply bidding another way

> **Recommendation:** Keep 0.5 a tick; MARKETS.md states it once with the reason.

*Files: `src/world/market_clearing.hpp`, `docs/economy/MARKETS.md`*

### NR-1224 — Monthly tick: migration runs once a quarter; one recruits check uses fixture rates
*decision taken on your behalf · raised 2026-11-12 · from BL-1642 (monthly tick) lane and cold review*

Migration moves three months of its rate on the first month of each quarter, so its integer floor does not silence centres of 100-299k. recruits_harness mechanic rows run a fixture registry (50/20/20 a tick) while R0 checks the shipped monthly rates.

**Why it matters.** Migration arrives in quarterly lumps under a monthly tick.

- Keep the quarterly migration cadence
- Carry a fractional remainder monthly instead

> **Recommendation:** Keep it; heads are conserved and the calendar rate holds.

*Files: `src/world/economy_system.cpp`, `tools/verify/recruits_harness.cpp`*

### NR-1230 — A rival's new plant needs supply that arrives at its own market
*decision taken on your behalf · raised 2026-11-15 · from BL-1651 (majors hoard treasury) round 2, market_viability seed 0 trace*

With no build limit, the settle stacked steel mills on one market's coal: coal demand there rose from 27 to 257 a month, the shelf drained, the price sat at 1.4-2.7x base, and 57 mills on seed 0 starved at the handoff (input-starved pooled 4.9% -> 8.9%). The input gate (BL-1234) reads spare coal anywhere a leg reaches (3,469 on the body); the haul never brought it. I added one test for a NATION's new processor: its own market's last month of listed supply of each input must cover every plant's draw there (standing and building) plus its own. A first cut on price (under 1.25x base) lagged the mills still building and moved nothing. Written into NATIONS.md § 3. Measured: seed 46 input-starved 20.6% -> 0.6%; pooled quick G1 87.6, G1t50 84.2, G1b 3.9, G2 143.4, G3 100 - all five targets met. The same test also re-asks each later pick in one evaluation. Seed 0 still reads 11.2% (coal stocked but priced over the mills' ceiling).

**Why it matters.** It narrows what a rich rival may build, in the direction of build only what runs, on public market supply. The rule is new.

- Keep the arrivals test
- Read shortage from price instead (lags the plants still building)
- Fix the cause in the supply clause: count only spare the haul has actually delivered

> **Recommendation:** Keep it for now; the third option is the deeper fix and is backlog work if the price test proves too blunt.

### NR-1226 — Launch boost: a satellite move still buys propellant off the home shelf; a lane now needs a STAFFED pad
*decision taken on your behalf · raised 2026-11-14 · from BL-1637 (launch boost) build*

Two calls taken while building launch boost. (1) A satellite changing orbit climbs through no pad, so it spends no boost: it keeps buying the move's propellant off the home shelf (rejected_shelf now means only that). Written into SPACE_ASSETS.md § Boost. (2) The straight-line space lane used to open on any Launchpad on the source body; it now needs a staffed one holding a launch's boost, because only a staffed pad banks boost and a launch spends it. (3) A launch asset's recorded launch_cost, and what the rival AI's reserve check and cheapest-kind pick read, is now the credits alone: the goods were bought by the intake beforehand, so they are a sunk cost at launch (cold review). The launch_spend census and verify_api launch_cost under-read a launch by its goods value accordingly.

**Why it matters.** Call (1) leaves one lump shelf buy in space, outside the pad intake that § Boost names the only goods buyer. Call (2) can close a lane an unstaffed pad used to open.

- Keep both as built
- A satellite move spends boost from the home pad instead (one rule for every burn from the ground's goods)
- A satellite move burns nothing extra: its launch quote already covers its first node

> **Recommendation:** Keep both: a move is in space, and § Boost says a unit in space burns what it carries; a satellite carries no tank, so the shelf stands in for it.

*Files: `src/world/space_assets.cpp`, `src/world/supply_system.cpp`, `docs/economy/SPACE_ASSETS.md`*

### NR-1227 — Launch boost dials: 2.5 boost a day (75 a month), cap 1,500, one third components
*decision taken on your behalf · raised 2026-11-14 · from BL-1637 (launch boost) build*

boost_per_day 2.5 makes a pad bank one home-orbit satellite (25 components + 50 propellant = 75 boost) each monthly tick. boost_cap 1,500 is twenty satellites, above the largest single launch the classes and paths quote. boost_components_share 1/3 is the satellite lot's own ratio, so a satellite costs exactly the goods it did. All three are C++ defaults in space_asset_params, not yet authored in Lua.

**Why it matters.** The rate paces every nation's space programme and is a steady draw on two scarce goods at every pad market; too high drains a thin shelf, too low starves rivals of launches.

- Keep as built and read the spectated run
- Author them in scripts/economy.lua now
- Tie the rate to the pad's staffing

> **Recommendation:** Keep as built; re-read after the spectated run shows rival launch counts.

*Files: `src/world/space_assets.hpp`*

### NR-1228 — Does propellant carried up through a pad - a raise's tank, a refuel - spend boost?
*question · raised 2026-11-14 · from BL-1637 (launch boost) cold review*

Your 2026-11-14 ruling: every climb through a pad spends boost. As built, the climb of the HULL and of CARGO spends boost; the propellant a unit carries up (its tank at a raise, a refuel over the pad) is still bought off the shelf as goods. refuel_space_unit's own comment says that propellant "climbs through the pad", so the ruling arguably reaches it.

**Why it matters.** If the tank spends boost, a Hauler trade draws much more boost per trip and the cap and rate want re-reading; if not, the shelf still sees a lump propellant buy at every raise and refuel.

- Keep as built: the tank and a refuel are goods bought off the shelf; only the climb of hull and cargo is boost
- The tank and a refuel spend boost too (their propellant is the cargo of the climb)

> **Recommendation:** Keep as built: the tank is the unit's own fuel, bought as goods like its kit, and boost stays the launch's cost.

*Files: `src/world/space_units.cpp`, `src/world/space_convoy.cpp`, `docs/economy/SPACE_ASSETS.md`*

### NR-1229 — Launch boost is built but no rival launches: no nation reaches Rocketry by 2015, and staged, no pad market ever holds a spacecraft component
*question · raised 2026-11-14 · from BL-1637 (launch boost) spectated census (tools/verify/space_launch_census.cpp)*

MEASURED (space_launch_census, the app's own monthly tick). SHIPPED: seeds 0, 28, 11 to 2015 - no nation holds Rocketry; the leading nation holds 22, 19, 21 of 212 Digitisation nodes; no pad is ever built, so boost never engages. STAGED (--stage-space: every rival handed the near-space keys and their chains at the handover; replay deterministic on all three): pads are built and staffed (seed 0: 2 in 1965, 14 in 1980, 23 in 2000; seed 28: 10 by 2010; seed 11: 5 by 2010, 19 still building), but every pad banks 0 boost and no rival launches by 2010. Seed 0 probe: propellant reaches the pad markets (22 of 22 pads stocked, 7,826 units in 1995), but spacecraft_components stand at 0 on every pad market in every reading, and only 8 of 22 pads see components under the fair-price ceiling - so 14 pads' components want goes to the hauler-only register and is never bid. The intake draws in the recipe ratio, so no components means no boost at all. RE-MEASURED 2026-11-16 on main 763e603e (boost merged with BL-1651's uncapped rivals and monthly AI), seeds 0, 46, 7, 13 to 2030. SHIPPED: no nation holds Rocketry by 2030 on any seed; the leader holds 31, 28, 30, 29 of 212 nodes; no pad is built. STAGED: rival pads are built and staffed (seed 0: 21, 46: 27, 13: 23, 7: 7 by 2030) and propellant stands on nearly every pad market (2.8k-16k units), but spacecraft components stand on almost none (0 on seeds 46, 13 and 7 in every five-year read; 1-6 pads on seed 0). Seed 0 alone launches: first in 2000, 6 by 2030, none after 2005. Mean boost 0 on the other three. Two walls stand: research reaches Rocketry too slowly to see in 70 years, and components never reach a pad (NR-1203's ranking). TRACED 2026-11-16 (corp_ai.cpp's write-only component trace, read by space_launch_census --stage-space, seeds 0, 46, 7, 13, 1960-2000): every rival nation asks for a Spacecraft Assembly Plant on each of its own tiles every month - 257k to 575k method visits per seed - and on 46, 7 and 13 not one passes. No method is tech-locked or barred by the air. Of the visits: 51-64% die at the SUPPLY clause (no spare input anywhere a leg reaches), 7-12% at the fair-price ceiling on the plant's own shelf, 6-10% at the supply-that-arrives test (BL-1651), 22-32% on a tile already standing a processor. The short input is the chain beneath: alloys first (43-48% of first failures), then steel (29-44%), then machinery (8-28%). On seed 0, 33 methods reached the pick and 32 became candidates, all from 1980. So the wall is the chain beneath: no world holds spare alloys, steel or machinery for a plant to draw, and the chain start (BL-1227's refused-draw bid) does not raise them. CHAIN TRACED 2026-11-16 (the trace generalised to every product, armed only by the harness; staged, seeds 0, 46, 7, 13, 1960-1990). Plants by 1990, seeds 0/46/7/13: alloys 1/0/0/0, machinery 12/0/0/1, electronics 0 everywhere, REE alloy 10/1/3/1, refined copper 75/9/14/7, steel 98/68/11/47. Steel is built but drawn to the bone: the world's spare steel is 117/72/24/10 units a month on 2-5 markets, and alloys and machinery die on steel first (steel is the first failed input in 44-85% of their visits, REE alloy or copper the rest). One level down, REE alloy dies at the supply clause on rare-earth ore (28-63% of visits), at the supply-that-arrives test (BL-1651; 20-40%) and at the glut veto, no buyer for its output (1-20%); refined copper likewise on copper ore (supply 43-74%, arrive 8-32%). Steel itself dies on iron ore and coal not spare. READING: every level of the chain is starved of spare input, and demand does not travel down it - a pad's want reaches components only; alloys, machinery and REE alloy have no buyer until the plant above them stands, and that plant needs their spare already. BL-1227's chain start bridges one level, not four. NOT YET TRACED: why extraction never runs the raws (iron ore, coal, rare-earth ore, copper ore) spare.

**Why it matters.** The item's done-line (a spectated run shows rival launches without pooling, same goods per launch) is not met, and boost cannot meet it alone: two walls stand upstream - the research pace never reaches Rocketry inside a campaign, and spacecraft components are never made or moved to a pad.

- A) Close launch boost as built (its mechanism verified by harness) and file the two walls as their own items: research pace to Rocketry, and a spacecraft-components supply
- B) Bank each good separately, so a pad holds propellant boost and components boost and a launch spends both - shelves fill with propellant meanwhile, but no launch still happens while components are absent
- C) Keep launch boost open until a spectated run launches, and work the components supply under it

> **Recommendation:** A: the boost mechanism does what SPACE_ASSETS.md says; the walls are research pacing and components supply, each a design question of its own. Start with why spacecraft_components is never made: the over-ceiling price at zero stock silences its want at most pads.

*Files: `tools/verify/space_launch_census.cpp`, `src/world/economy_system.cpp`, `docs/economy/SPACE_ASSETS.md`*

### NR-1231 — Industry startups open space to the rivals but cost the tick-50 market gate 1.6 points: turn the switch on?
*question · raised 2026-11-16 · from BL-1673 (industry startups), measured behind its switch*

Built as ruled (Ben, 2026-11-16: start new industries though not the most profitable; waive supply and arrival; behind a switch). Off, the scorer is byte-identical: the 16 digests match the merge gate's exactly, and world_determinism passes. ON, staged (rivals handed Rocketry), space_launch_census to 2030: rival launches seed 0 6 -> 170 (first 2000 -> 1972), 46 0 -> 57 (1976), 13 0 -> 19 (1977), 7 0 -> 2 (1978); Spacecraft Assembly Plants by 2030 21 / 35 / 18 / 0 standing, through the heavy method (machinery + steel; alloys and electronics stay near 0). Shipped worlds still launch nothing: no nation reaches Rocketry by 2030 (the research wall, NR-1229). market_viability quick, off -> on: G1 87.6 -> 91.1, G1t50 84.2 -> 82.6 (FAILS its 84), G1b 3.9 -> 1.5, G2 143.4 -> 140.0, G3 100 -> 100. Seed 0 staged launches 157 comms satellites by 2030, which may be more than the design wants. TUNED (Ben, 2026-11-17: 'Tune it first'): a startup now needs a BUILDING's want for its product at its market - a plant drawing it (standing or building, starved or not), a ceiling-silenced plant bid, or a held off-book want (a pad's lump, upkeep); a household bid alone no longer admits one. Staged census to 2030, untuned -> tuned: seed 0 170 -> 71 launches (comms class 157 -> 57; first 1972 -> 1985), 46 57 -> 125 (1976 -> 1974), 13 19 -> 4 (1977 -> 1975), 7 2 -> 4 (1978 -> 1995); component plants 21/35/18/0 -> 23/33/6/2. market_viability quick (the quick tier is FOUR seeds, 0/28/46/12 - not sixteen, as first reported), off / untuned / tuned: G1 87.6 / 91.1 / 88.8, G1t50 84.2 / 82.6 / 82.9 (still FAILS 84), G1b 3.9 / 1.5 / 3.2, G2 143.4 / 140.0 / 136.3, G3 100. Per seed, G1t50 with the switch OFF already spans 76.9 to 89.6, so a 1.3-point pooled move on four seeds is inside the seed spread: the full tier would say whether the miss is real. FULL TIER (16 curated seeds, 2026-11-17, at b128d2ef), off -> tuned on: G1 82.2 -> 83.4 (both FAIL 85), G1t50 80.2 -> 77.5 (both FAIL 84), G1b 3.3 -> 3.2, G2 129.9 -> 133.3, G3 100 -> 100. Absolute: running at handoff 2371/2886 -> 2506/3004; running at tick 50 2543/3172 -> 2606/3361. So the miss is real and larger than four seeds showed (-2.7), and its shape is the design's: startups add 189 plants by tick 50 and 63 more running - a startup is built before its input exists, so it waits, and a ratio counts it as a failure while it does. NEW: with the switch OFF, main already fails both G1 gates at the full tier (82.2 and 80.2) - the gate BL-1442 (post-capacity starvation) closed has reopened since, independent of startups.

**Why it matters.** It is the first lever that opens space to the rivals at all; it fails one market gate by 1.6 points, and a failing gate is not weakened.

- Turn it on: more plants run in absolute terms, and the ratio drop is startups waiting for input as designed; carry the G1 gates as their own item, since main fails them already
- Count a startup outside G1 until its first input arrives (a gate definition change - Ben's call, never a quiet weakening)
- Tune further (one startup a year) and re-measure
- Keep it off; first answer why main fails G1 at the full tier with the switch off

> **Recommendation:** D then A: main fails G1 and G1t50 at the full tier without startups, so the reopened gate is the first question; startups only deepen a miss already there.

### NR-1234 — Shader seam: --verify captures stay on the software renderer, so they never show a shader pass
*decision taken on your behalf · raised 2026-11-14 · from BL-1655 (shader seam), sprint 68*

--verify forces the software renderer, and every shader pass (lens wash, border band, water glint) has a no-shader path that is what captures draw. The shader passes were checked by a live look in the GPU app, not a capture. IO_NO_GPU_EFFECTS=1 turns every pass off in the GPU app, for an A/B.

**Why it matters.** A green visual run proves the no-shader path, not the shader. A shader regression would pass every capture.

- Keep it: captures prove the fallback, live looks prove the shader
- File a GPU capture mode (read back the GPU target) as its own item

> **Recommendation:** Keep it for this sprint; file the GPU capture mode if a shader pass regresses unseen.

*Files: `src/core/gpu_effects.cpp`, `src/core/app.cpp`*

### NR-1235 — Borders: the shader measures the hex side analytically, with no distance field stored
*decision taken on your behalf · raised 2026-11-14 · from BL-1657 (SDF national borders), sprint 68*

The item named a signed distance field. The pass instead reads a one-texel-per-tile owner table and, per pixel, measures the distance to each of the six sides whose neighbour has a different owner. That is exact for a hex grid and needs no baked field or extra memory. The rule's width is set in screen pixels, so it holds across zoom.

**Why it matters.** It is cheaper and exact on hexes, but it cannot draw a border off the hex edges (a smoothed or river-following line would need a real field).

- Keep the analytic pass
- Bake a true distance field later if borders should leave the hex edges

> **Recommendation:** Keep it; the border rule follows hex edges by design (PLANETARY.md).

*Files: `src/shaders/border_band.frag.hlsl`, `docs/ui/PLANETARY.md`*

### NR-1238 — The G1 market gates fail on main because arms works stand unstaffed: no buyer posts a bid for arms
*question · raised 2026-11-18 · from NR-1231 (industry startups switch) full-tier reading; traced by bisect*

market_viability full tier, startups off: G1 82.2 (FAILS 85), G1t50 80.2 (FAILS 84); last green 94.5 / 84.7 at b2b2502b (BL-1442, post-capacity starvation, 2026-11-04). BISECT (seeds 17/27/31/0, handoff labour-short count, 6 then 5 steps): first bad on main is the naval merge 860c24d6; inside it, b8336a0d - BL-1551 (force equipment, 2026-11-08), which puts three Arms Works in each major's opening kit, exempt from the kit's want and feed gates ('opens, idle, and runs when they arrive'). On main at the handoff, 16 seeds: 374 arms works, 21 running (312 short of labour, 34 decommissioned); every other processor 2350 / 2512 running = 93.6%, back at the last green reading. MECHANISM: the workforce dial prices a works' output against posted demand (dial_bid = demand only, Ben 2026-10-09), and a refill buys arms off the shelves without posting a bid; the capital opens on an arms reserve besides (seed_arms_reserve), so the dial sees supply and no buyer and staffs the works at zero. OPEN: seed 0 holds exactly 3 per major (15 for 5), but seed 27 holds 68 for 11 majors and seed 17 30 for 6 - something besides the kit builds them, likely the rival scorer in the settle; not traced. The tick-50 gate is assumed to share the cause (not split by recipe yet). TRACED (2026-11-18, the extras): every Arms Works past the kit's three a major is a kit slot for ANOTHER good that fell to rung 3 of the substitution ladder (opening_kit.cpp place_processor, 'the feasible processor nearest its feed'). Rung 3 offers every processor recipe; BL-1551 (force equipment) exempts any arms recipe from the want gate and lifts it from chain_tier_none to chain_tier_none - 1 when unfed, so once no wanted, fed processor fits, Small Arms always wins. The kit report (print_opening_kit, in every market_viability log) accounts for every works exactly: seed 27, 11 majors x 3 + 35 rung-3 substitutions (steel 8, silicon 8, food rations 7, ordnance 6, power 3, machinery, refined copper, refined fuel) = 68; seed 17, 6 x 3 + 12 = 30; seed 0, 5 x 3 + 0 = 15. The BL-1551 comment names the exemption for 'the kit's arms slots', so a non-arms slot taking it is a defect: those slots should walk on to unbuilt (or the flex ladder), not stand as idle arms. FIXED (2026-11-18, Ben's go on option C): place_processor takes the arms exemption only for a slot whose good is an arms good; the leaked slots now stay unbuilt (seed 27: unbuilt 13 -> 48, arms works 68 -> 33). Full tier after: G1 82.2 -> 86.0 (PASSES 85); G1t50 80.2 -> 81.1 (FAILS 84); G1b 2.8; G2 132.3; G3 100. AT TICK 50 BY RECIPE: arms works 282 built, 6 running; every other processor 2511 / 2823 = 88.9%. So the tick-50 failure left is the kit's own three arms works a major, waiting on a buyer the dial cannot see. DIGESTS: the fix moves D_land, D_settle and D_seat on exactly the 12 seeds that leaked (0, 13, 29, 46 identical; D_search unmoved everywhere); main before the fix already failed those three on all 16 (the re-pin owed since the launch-boost merge). BEN RULED (2026-11-18): three changes replace options A/B - the shelf pricing cap to one month for every good (shelf_supply_ticks 3 -> 1, MARKETS.md), equipment wears 5% a month, rival forces at 1.5x the target share (base 0.30, ceiling 0.75). The 10x capital reserve first built from 'increase the stockpile' was a misreading and is reverted. ABLATION (16 seeds): cap alone G1 84.4 / G1t50 82.6 / G1b 2.2; wear + armies alone 86.6 / 84.5 / 8.1 (FAILS); all three 87.5 / 84.1 / 5.9 (FAILS G1b, steel short of coal the largest cause). Ben chose all three (2026-11-19) and then the stocks redesign, BL-1678 (stocks are bought), which carries the wear; its gate reading is NR-1239's.

**Why it matters.** Both G1 gates fail on main with nothing switched on, and NR-1231 (startups switch) cannot be read against a baseline that already fails. A failing gate is not weakened, so the cause needs a call.

- A: arms are wanted - a refill's arms draw is held as an unposted bid and the dial reads it for arms (widens the dial's buyer signal, Ben's 2026-10-09 narrowing; an AI-grant question)
- B: an Arms Works waiting for its first hire is a reserve, not a failure - G1 leaves it out until it has run once (a gate definition, never a quiet weakening)
- D: the kit opens arms works only where a hire will soon want them (undoes part of BL-1551 force equipment)

> **Recommendation:** C is done. Between A and B: A if the works should make arms ahead of war (the dial then staffs them on refill draw); B if BL-1551 meant them as a standing reserve that idles in peace. Either clears G1t50 by the reading above.

### NR-1239 — Stocks are bought (BL-1678): the calls taken inside the four rulings
*decision · raised 2026-11-19 · from BL-1678 (stocks are bought) build and its cold review*

Taken on Ben's behalf, each written into MILITARY.md § Carried supplies: (1) PAID IN ARREARS. A unit pays each month for what its stocks were DELIVERED - a month's camp drain plus a per-stock account of what a fight, march or refill was delivered beyond it. The cold review found the first build (buy what each stock lacks plus the peace drain) never billed a fight at full cover: the restock outpaced the spend, the stock stayed full, and the shells were free. The account fixes that without daily churn: a quiet unit at full never moves it. The bought share (met share of last month's bill) still caps the next month's refill rate, as ruled (lower of road and shelf). (2) FUEL CARRIERS are the energy-gated VEHICLE classes (motorised, mechanised, armour, light armour): a pre-modern row's energy gate stands in for saltpetre, so Line Infantry carries no tank. (3) THE FUEL GOOD is refined fuel only: a coal economy can hire armour (the energy axis takes coal) but cannot run it. (4) WEAR BUYS KIT GOODS ONLY (the class's equipment and the design's), not the hire's gated-axis costs (steel, rations, coal) - the ablation's steel/coal starvation came partly from wear buying those. (5) Calibration: ammunition fill 0.0035 ordnance a man (twelve months of the old training fire), fuel fill 0.1 refined fuel a man, fuel 2 a day in camp, +20 marching, +30 fighting. (6) NO SHELF IS AN OFF-WORLD EDGE ONLY. Every home-body tile belongs to a market (MARKETS.md: the off-road leg reaches every tile), so a home unit always has a shelf. An off-world body has no market until its first building completes; a unit standing there would buy nothing. No path sets a land unit down off-world today (units in transit ride aboard). (7) The rival march forecasts read min(road cover, food bought share) at the unit's tile - the same rule, read by the existing grant. (8) An empty tank holds its order and burns only camp fuel; a refit off the vehicles resets the tank.

**Why it matters.** Each is a design choice the rulings did not name. (3) makes supply harsher than before; (1) changes when war goods reach the market (a month late).

> **Recommendation:** Accept as built unless (3) or (6) reads wrong in play.

*Files: `src/world/carried_supplies.cpp`, `src/world/economy_system.cpp`, `docs/military/MILITARY.md`*

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

### NR-1154 — Elimination as built: five calls, and rivals may now end the player
*decision taken on your behalf · raised 2026-11-10 · from war lane building BL-1562 (war both hostile, elimination), commit 0eba4369*

Taken while building: (1) elimination fires on no LAND province; coastal-water provinces pass to the conqueror with the cession; (2) every truce naming the ended nation is erased, including the one just written with the conqueror; (3) sentiment rows to and from it are deleted, generation grudge seeds kept as history; (4) its live capital is cleared, its generation capital kept as identity; (5) a debt written off sets the treasury to 0, which creates money outside the money books. Consequence of the legal verb under the existing AI grant: a rival imposing peace at 100 can now cede the last province and end any nation, the player included. Rival defenders push from the declaration rather than after answering.

**Why it matters.** Call 5 breaks money conservation; the AI consequence means a campaign can be lost to a rival peace term.

- Keep all as built
- Debt passes to the conqueror (conserves money)
- Rivals may not impose a last-province cession on the player

> **Recommendation:** Keep 1-4. Ben to choose on 5 and on the rival ending the player.

> **RESOLVED.** Ben, 2026-11-10: keep as built. A debt is written off at elimination (money is created; accepted). A rival's imposed peace may end the player. The land-only trigger, erased truces, deleted sentiment and the cleared capital stand. Written into NATIONS.md § Elimination.

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

> **RESOLVED.** Ben, 2026-11-10: accept as built. Centres may demote on their first bad quarter; pooled growth 1.18%/yr stands; no demotion margin and no seeding headroom.

*Files: `src/world/economy_system.cpp`, `src/world/population_generation.cpp`, `docs/economy/POPULATION.md`*

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

### NR-1193 — Naval merge: an ended nation's fleet disbands with everyone aboard
*decision taken on your behalf · raised 2026-11-10 · from naval merge fix-up 2de15672 (elimination predates ships)*

When a nation ends (NATIONS.md Elimination), its ships are erased where they stand with everyone aboard, its sightings and every sighting of its ships go, and its ship designs clear. The save refuses an ended nation that still owns a ship, a sighting or a design.

**Why it matters.** A transport at sea when its nation ends loses its embarked army outright.

- Keep: the fleet ends with the nation
- Ships pass to the conqueror like other property
- Ships at sea return to the nearest friendly port first

> **Recommendation:** Keep; it matches units disbanding where they stand.

> **RESOLVED.** Ben, 2026-11-10: keep. An ended nation's ships disband where they stand, everyone aboard. Written into NATIONS.md § Elimination.

*Files: `src/world/war.cpp`, `docs/politics/NATIONS.md`*

### NR-1194 — Naval merge: MILITARY.md domain rows reworded where both lines changed them
*decision taken on your behalf · raised 2026-11-10 · from naval merge 62277745 (conflict in the MILITARY.md domain table)*

Main's land-only march ruling (Ben, 2026-11-09) and the naval branch's transport rows met in one table. Merged wording: coastal_water "No, bridges excepted (Ben, 2026-11-09); crossed only aboard a transport"; open_ocean "Only aboard a transport".

**Why it matters.** The table is the authority for where a land unit may stand.

- Keep the merged wording
- Reword

> **Recommendation:** Keep.

> **RESOLVED.** Ben, 2026-11-10: keep the merged MILITARY.md domain-row wording for coastal water and open ocean.

*Files: `docs/military/MILITARY.md`*

### NR-1160 — Near-space design filled in on your behalf: slot counts, signal delay of one quarter, data home only through coverage
*decision taken on your behalf · raised 2026-11-10 · from space-mechanics session (worktree claude/space-mechanics-design), writing your form answers into SPACE_ASSETS.md*

Your answers set the shape; I chose the rest. Nodes: surface, low, high per body, plus L1/L2/L4/L5 per pair (L3 left out). Slots: low 8 + 4 per Earth mass, high 4 (planet) or 2, L4/L5 3, L1/L2 2; first come, first held. Launch buys spacecraft_components + propellant off the home shelf. An order outside comms coverage waits one quarter; research, observation and survey data are earned only in coverage. An edge module (Dock, Shipyard) enclosed by a grid upgrade goes idle until refitted. Upgrades: 5x3 -> 7x5 -> 9x7. Budgets stop the newest module first. Solar power falls with the square of orbital radius.

**Why it matters.** These are the numbers and rules a first playtest of v0.4.0 will feel.

- Keep them
- Name the ones to change

> **Recommendation:** Keep them until the spectator run and your playtest say otherwise.

> **RESOLVED.** Ben, 2026-11-10: accepted with every call taken on your behalf in this stretch ("accept all, tune in playtest").

*Files: `docs/economy/SPACE_ASSETS.md`*

### NR-1161 — Far-space design written whole on your behalf (SPACE_FLEETS.md and six sibling sections)
*decision taken on your behalf · raised 2026-11-10 · from space-mechanics session, designing v0.5.0 so its lanes can run while you are away*

New doc docs/military/SPACE_FLEETS.md: five hull classes (Hauler, Lander, Tug, Frigate, Cruiser; Cruiser only from a Shipyard); a dv budget that refuses an unaffordable move, cargo adding mass; refuel at a Dock+Fuel depot or over an owned Launchpad; a Dock berths two; space convoys are Haulers on the cheapest path, interdicted by hostile armed fleets on it; daily deterministic battles at a node; blockade = hostile armed fleet in low orbit with none of the holder's; bombardment from low orbit and an orbital defence battery. Also: debris as wear and cascade (SPACE_ASSETS), mapping-then-prospecting (DISCOVERY), bases as capitals that partition a body into provinces on first landing (NATIONS), war across every body (MILITARY), war-placed front pins (LAYOUT), seeded solar storms (EVENTS), firsts with a research grant, the propellant loop and the next age's tree (ERAS).

**Why it matters.** It is a large design taken without a form. Each piece is in its owning doc, so a change is an edit there.

- Keep and build as written
- Pause v0.5.0 lanes for a design form when you are back

> **Recommendation:** Keep and build; your v0.6.0 playtest is the place to reshape it.

> **RESOLVED.** Ben, 2026-11-10: accepted with every call taken on your behalf in this stretch ("accept all, tune in playtest").

*Files: `docs/military/SPACE_FLEETS.md`, `docs/economy/SPACE_ASSETS.md`, `docs/ui/DISCOVERY.md`, `docs/politics/NATIONS.md`, `docs/military/MILITARY.md`, `docs/ui/LAYOUT.md`, `docs/EVENTS.md`, `docs/economy/ERAS.md`*

### NR-1162 — Space work is on its own branch, ids from BL-1580 and sprints 65 and 66
*observation · raised 2026-11-10 · from space-mechanics session*

All space work lands on branch claude/space-mechanics-design (worktree .claude/worktrees/space-mechanics-design), cut from local main and merged with main as it moves. It is not merged into main: main's checkout is shared, and other sessions commit there. Backlog ids start at BL-1580 (a gap above BL-1567) and NR ids at NR-1160, so a parallel session minting on main does not collide; sprints are numbered 65 (near space) and 66 (far space).

**Why it matters.** Merging the branch into main, and whether 65/66 clash with a sprint another session opened, are yours to decide.

- Merge into main when you are back
- Keep it on the branch until v0.6.0

> **Recommendation:** Merge at each version cut, after your look.

> **RESOLVED.** Ben, 2026-11-10: accepted with every call taken on your behalf in this stretch ("accept all, tune in playtest").

*Files: `docs/development/sprints.json`, `docs/development/backlog.json`*

### NR-1164 — Generated bodies other than the homeworld carry mass 1.0, so a moon's climb is cheap only because it has no air
*question · raised 2026-11-10 · from BL-1580 (orbital graph) lane report, confirmed by its harness WARN line on seed 20260911*

hard_coded_world.cpp sets mass_earths only on the homeworld; planetology's per-body masses are not copied onto body_component. On the generated world the moon climbs 8.0 dv against home's 12.16 (atmosphere does all the work), an asteroid climbs 8.0, and low-orbit slots read 12 for both instead of 8. Copying the masses fixes it but moves body_km_per_tile (logistics.cpp) and so very likely the digests.

**Why it matters.** Your item 7 wants smaller bodies cheaper to launch from; today the gap is half what it should be, and the asteroid is as heavy as the moon.

- Copy planetology masses in generation and re-bless the digests
- Keep body_km_per_tile on the old value and give the graph its own mass read
- Leave it until v0.6.0

> **Recommendation:** Copy the masses and re-bless at the v0.4.0 cut; you said continuity between releases is not a constraint.

> **RESOLVED.** Ben, 2026-11-10: generate each body's size and density, and derive its mass (PLANETOLOGY.md § The prototype body set); the orbital graph reads it. Work: BL-1638 (generated body mass).

*Files: `src/world/hard_coded_world.cpp`, `src/world/orbital_graph.cpp`, `src/world/logistics.cpp`*

### NR-1165 — Which Digitisation node unlocks each near-space thing, and the Early Space tree's first cut
*decision taken on your behalf · raised 2026-11-10 · from BL-1592 + BL-1607 lane report*

Every near-space unlock rides an existing Digitisation node as an added mechanic effect (no node added): e.g. Comms role on Communication Satellites, Navigation on Satellite Navigation, Observation on Thermal Imaging, Survey on Seismic Survey, astronaut training on Mass Higher Education, Dock on Gantry Cranes, Power on Photolithography, 7x5 grid on Numerical Control; the station core sits on Rocketry so a nation can found a station as soon as it can launch. The Early Space tree's first cut is three trees of one branch each (In-Situ Propellant, Interplanetary Convoys, Space Force; 54 nodes), keyed by the Digitisation quests named in keys_next_branch; the other six named branches are reserved. Kit item keys (two drives, two tanks, two armaments, two armours, a debris arm) were invented; quest deeds, advantage figures and Era 3 branch names are placeholders.

**Why it matters.** Where an unlock sits sets how soon a nation reaches each module, which is the opening of the space race.

- Keep
- Move named unlocks

> **Recommendation:** Keep until the spectator run shows how soon rivals reach each module.

> **RESOLVED.** Ben, 2026-11-10: accepted with every call taken on your behalf in this stretch ("accept all, tune in playtest").

*Files: `docs/economy/tech/DIGITISATION_TREE.md`, `docs/economy/tech/EARLY_SPACE_TREE.md`, `src/world/space_tech_keys.hpp`*

### NR-1166 — Novel work: per-tree research state so a nation can research the next age's tree
*novel-work · raised 2026-11-10 · from BL-1607 lane report*

The research queue has one held set and one queue, both sized to the Digitisation tree, and no Era 1 -> Era 2 gate query exists. Filed as BL-1612 (early space researchable): per-tree state with a save leg, the gate query, and branch keys by quest. All Digitisation quest deeds are declared unread, so no branch can be keyed today; the item reads them or lets the gate stand in.

**Why it matters.** It grows the research system from one age to two, which no doc had designed in code terms.

- Build it as filed
- Collapse far-space unlocks onto the Digitisation tree instead

> **Recommendation:** Build it as filed; it is the shape RESEARCH.md already describes.

> **RESOLVED.** Ben, 2026-11-10: accepted with every call taken on your behalf in this stretch ("accept all, tune in playtest").

*Files: `docs/economy/RESEARCH.md`, `src/world/nation_tech.cpp`*

### NR-1167 — Re-pin the player_seed_sweep digests at the v0.4.0 cut (the save format moved them, the world did not)
*question · raised 2026-11-10 · from BL-1583 lane report: save v80 -> v81*

Adding the station section bumped world_save_version 80 -> 81, and player_seed_sweep --digest-check fails seeds 46 and 31 on D_land, D_settle and D_seat (D_search unmoved). rebless_shape_probe on both trees: every ROW and MIX line identical, so the world is unchanged; only the stream moved, as with earlier version bumps. More space save legs land in sprint 65 (slots, roles, pending orders, per-tree research), each moving the stream again.

**Why it matters.** A failing digest gate hides any real world movement that lands beside it.

- Re-pin once at the v0.4.0 cut, with a shape probe either side
- Re-pin after each save leg

> **Recommendation:** Once at the v0.4.0 cut; I will run the shape probe and leave the re-pin for your go.

> **RESOLVED.** Ben, 2026-11-10: re-pin once, after the launch fix lands.

*Files: `tools/verify/player_seed_sweep.cpp`*

### NR-1168 — Station model calls: core costs, module numbers, which module stops, removal keeps crew
*decision taken on your behalf · raised 2026-11-10 · from BL-1583 + BL-1585 lane report*

Core launch 50,000 cr, upkeep 2,500; core gives power 4, crew room 2, life support 2, needs 1 crew. Module draw/crew: Research 3/1, Defence 2/1, Power 0/0 (+6), Dock 1/1, Habitation 1/0 (+4 room), Life support 2/0 (+4), Fuel depot 1/0, Shipyard 4/2, Factory 4/2, Observatory 2/1, Relay 1/0, Cargo 0/0, Market 1/1. On a shortfall the newest module drawing on the SHORT budget stops (a Power module is passed over in a crew shortfall). An upgrade grows the grid when ordered, its truss launched after. Removing an arrived module moves its crew onto the core; a lost core loses the whole station and its crew. Training: 2 astronauts per centre per quarter, pool cap 20 per running centre; the Astronaut Training Centre costs 300 cr, 240 days, 35 steel.

**Why it matters.** These set how fast a station grows and what a shortfall feels like.

- Keep
- Name the numbers to change

> **Recommendation:** Keep until the spectator run.

> **RESOLVED.** Ben, 2026-11-10: accepted with every call taken on your behalf in this stretch ("accept all, tune in playtest").

*Files: `src/world/stations.hpp`, `scripts/economy.lua`*

### NR-1169 — Early Space branches open on a stand-in until quest deeds are read: Rocketry alone keys Interplanetary Convoys
*decision taken on your behalf · raised 2026-11-10 · from BL-1612 lane report*

Every Digitisation quest deed is declared unread, so no quest can be held and no Early Space branch could ever open. Stand-in (one place, branch_keyed_in): a branch is keyed if its quest is held, or if the quest's deed is unread and its major prerequisite is held. Consequence: any nation that meets the Era 2 gate already holds Rocketry, so Interplanetary Convoys (hulls, landers, tugs, drives) opens with the gate; In-Situ Propellant needs DG-PR-EN-C, Space Force needs Proven Readiness's major. One research pool and one queue: Digitisation entries are paid first. New dial space_gate_min_propellant = 1.0 (one launch's draw), since ERAS.md names a Lua threshold that did not exist.

**Why it matters.** It decides how soon far space opens after the first launch.

- Keep the stand-in until deeds are read
- Read the space quest deeds now (first convoy to another body etc.)

> **Recommendation:** Keep; the firsts item (BL-1606, space firsts) reads the same deeds and can retire the stand-in.

> **RESOLVED.** Ben, 2026-11-10: accepted with every call taken on your behalf in this stretch ("accept all, tune in playtest").

*Files: `src/world/nation_tech.cpp`, `docs/economy/RESEARCH.md`*

### NR-1170 — A new near-space command seam (space_command.*) beside the nation command path
*novel-work · raised 2026-11-10 · from BL-1588 lane report*

Space verbs (launch, move, fit, retire, set role and so on; 11 gameplay.space_* entries in ACTIONS.json) go through a new space_command seam. It holds a pending-order queue for signal delay: an order outside comms coverage waits one quarter, then applies or is dropped if it is no longer legal. The seam validates as an untrusted boundary, but no --serve/MCP transport carries it yet.

**Why it matters.** It is a second command path; the AI and the wire must both use it.

- Keep it separate (space orders carry delay; ground orders do not)
- Fold it into the nation command path later

> **Recommendation:** Keep; file the --serve/MCP transport as v0.5.0 work.

> **RESOLVED.** Ben, 2026-11-10: accepted with every call taken on your behalf in this stretch ("accept all, tune in playtest").

*Files: `src/world/space_command.hpp`, `docs/ai/ACTIONS.json`*

### NR-1171 — Launch cost formula and slot use: base + 250 per dv + goods value; slots only for satellites and cores
*decision taken on your behalf · raised 2026-11-10 · from BL-1581/BL-1582 lane report*

Launch cost = base + 250 credits per unit of delta-v climbed + the market value of the goods bought off the home shelf. Only satellites and station cores take an orbital slot. A station core may launch to any orbital node, including L-points. Launch and move debits are paid but are not yet booked as a line in the nation return.

**Why it matters.** It sets how expensive space is, and the return under-reports the spend.

- Keep these numbers for v0.4.0 and tune after the spectator run
- Book the debits as a return line now

> **Recommendation:** Keep the numbers; book the return line with the Space ledger item, BL-1591 (space ledger and designer).

> **RESOLVED.** Ben, 2026-11-10: accepted with every call taken on your behalf in this stretch ("accept all, tune in playtest").

*Files: `src/world/space_command.cpp`, `docs/economy/SPACE_ASSETS.md`*

### NR-1172 — Station and satellite effect dials and stand-ins taken by the effects lane
*decision taken on your behalf · raised 2026-11-10 · from BL-1584/BL-1586 lane report*

Life support keeps no memory: a station either buys its rations this quarter or starves. Electronics stands in as the factory module's output good. An Observation satellite lifts only the vision fog, not the activity fog. Survey satellites start the mapping, at a stand-in rate until prospecting lands (v0.5.0). Dials: navigation factor 0.9, research module 50 RP a quarter; the rest are in space_effects.hpp.

**Why it matters.** These are first numbers; the spectator run will show whether they matter.

- Accept as first cut
- Revisit any dial

> **Recommendation:** Accept; revisit after the spectator observation.

> **RESOLVED.** Ben, 2026-11-10: accepted with every call taken on your behalf in this stretch ("accept all, tune in playtest").

*Files: `src/world/space_effects.cpp`, `docs/economy/SPACE_ASSETS.md`*

### NR-1173 — Space units: an authored hull and item stat table, the rocket equation, and a depot that buys propellant on its own
*novel-work · raised 2026-11-10 · from BL-1594/BL-1595 lane report*

Delta-v is the rocket equation over propellant aboard (never a stored delta-v). Five hulls (Hauler 10 mass/20 hp, Lander 8/15, Tug 8/20, Frigate 14/40, Cruiser 40/120 and yard-only) and eleven items (chemical drive ve 10, nuclear thermal ve 20, tanks 20/45, kinetic and missile armament, Whipple and composite armour, cargo and passenger bays, capture arm) are a new numeric table no doc owns. A working Fuel depot fills itself each quarter off the owner home shelf (200 a depot). Raising draws goods market-direct and credits no seller. Docking is own-nation only. Dials: launch_propellant_per_dv 0.1, repair 1/day, quarters_to_loss 4, berths_per_dock 2. Space-unit verbs, UI and cargo loading are owed (BL-1596 onward).

**Why it matters.** The table sets how far and how cheaply fleets move; the depot is a new automatic buyer.

- Accept the table as a first cut and tune at the v0.6.0 playtest
- Move the table into SPACE_FLEETS.md as authority now

> **Recommendation:** Accept; the numbers live on recipe_registry::space_units() and can move to Lua later.

> **RESOLVED.** Ben, 2026-11-10: accepted with every call taken on your behalf in this stretch ("accept all, tune in playtest").

*Files: `src/world/space_units.cpp`, `docs/military/SPACE_FLEETS.md`*

### NR-1174 — Pinned panel: pins travel in the UI save (save_game v28); the lens key and the panel share the column
*decision taken on your behalf · raised 2026-11-10 · from BL-1603 lane report*

Player pins are saved in the ui_state slice (save_game_version 28; v27 opens with no pins). Front pins are derived each frame and never saved: one per (war, body) where both sides hold land. The panel stops one margin above a grown lens key (a new shell_metrics hand-off). The pin toggle sits left of the band header go-to button, or ends the tile heading row. New verify read verify.pinned_panel() reports only what the panel draws; it reads no tile data, so no grant was taken. Fronts off the home body wait on BL-1602 (interplanetary war).

**Why it matters.** A save-format change and a chrome hand-off grew past a UI-only brief; a live click is still owed.

- Accept
- Keep pins out of the save

> **Recommendation:** Accept.

> **RESOLVED.** Ben, 2026-11-10: accepted with every call taken on your behalf in this stretch ("accept all, tune in playtest").

*Files: `src/ui/pinned_panel.cpp`, `src/core/save_game.cpp`, `docs/ui/LAYOUT.md`*

### NR-1175 — Circumplanetary assets: L5 clamped above the band, first-cut mark sizes, a new space_asset selection kind
*decision taken on your behalf · raised 2026-11-10 · from BL-1590 lane report*

At 1280x720 the bottom band (~280 px) covers the moon outer orbit, so its L5 diamond is clamped above the band and floats off its orbit when zoomed. The alternative is to lift the rung centre so the whole system clears the band. Satellite plate radius 5.5 px; station 8.5 px plus 2 per grid ring; the player dark magenta reads dim on the satellite plate. A new selection_kind::space_asset (a station selects by its core) and its band card were added; SELECTION.md carries the row. New verify calls near_space_fixture (staging) and circum_assets report only what the canvas draws and touch no tile data, so no grant was taken. The transit segment needs an origin node and departure tick on a moving asset (a save change): filed as work.

**Why it matters.** A visual call, and whether verify staging calls need a dated grant when they carry no tile data.

- Keep the clamp
- Lift the rung centre above the band

> **Recommendation:** Lift the rung centre; a clamped L-point lies about where it is.

> **RESOLVED.** Ben, 2026-11-10: accepted with every call taken on your behalf in this stretch ("accept all, tune in playtest").

*Files: `src/ui/circum_nodes.cpp`, `src/ui/circum_assets.cpp`, `docs/ui/CIRCUMPLANETARY.md`*

### NR-1176 — Spectator finding: rivals reach Rocketry (first 1976, 14 of 41 by 2000) but launch nothing, because no Launchpad ever exists
*decision taken on your behalf · raised 2026-11-10 · from BL-1589 lane report (seed 0 spectate to 2000)*

Rocketry holders: 2 in 1977, 3 in 1978, 7 in 1988, 14 by 1994. Launches 0, space spend 0: every holder is blocked no_launchpad. Nothing builds a Launchpad (not generation, not the nation build scorer), and placement_rules.cpp allows ONE Launchpad per body world-wide, so on a shared home world only one nation could ever launch. SPACE_ASSETS.md says a staffed Launchpad on the nation home body, i.e. one per nation. Training centres are never built either, so every astronaut pool is 0. Treasuries: richest about 35.6k in 1975, about 108k-123k by 2000; a comms satellite quotes about 23.5k, a core about 60k. DECISION TAKEN: the doc wins, the cap becomes one Launchpad per nation per body; rivals build a Launchpad and an Astronaut Training Centre through the build scorer on their own ground. I read this as inside the 2026-10-23 build grant (what to build, own ground) and the 2026-11-10 space grant (which names astronaut training), not a new widening; filed as BL-1615 (rival space groundwork).

**Why it matters.** This is the first hurdle Ben asked about: without it no rival goes to space at all.

- Accept: per-nation pads, rivals build pads and centres
- Generation opens a pad for each major instead
- Keep one pad per body and make it a contested asset

> **Recommendation:** Accept; it matches the doc and the existing grants.

> **RESOLVED.** Ben, 2026-11-10: accepted with every call taken on your behalf in this stretch ("accept all, tune in playtest").

*Files: `src/world/placement_rules.cpp`, `src/world/nation_ai.cpp`, `docs/economy/SPACE_ASSETS.md`, `docs/ai/AI_OPPONENT.md`*

### NR-1177 — Rival space scorer dials: research relief 0.25 for near-space keys, half-treasury reserve, a fixed 14-module target
*decision taken on your behalf · raised 2026-11-10 · from BL-1589 lane report*

The research picker weighed only Rocketry among mechanic keys; every other near-space key now earns research_space_key_relief 0.25 (half of research_unlock_relief) x the space emphasis. That moves rival research picks after the handover in every campaign, so post-handover campaign digests are expected to move (not measured, nothing re-blessed). Reserve: one action spends at most half the treasury and keeps 8 quarters of space upkeep. A comms satellite first, then a core at the cheapest free home node, then modules toward a fixed order (power, research, habitation, life support, power, research, observatory, comms relay, power, habitation, life support, research, fuel depot, cargo hold). Home body only; no upgrades, retasks or crewed missions. Never the player nation, even under spectate.

**Why it matters.** These set how fast and how far rivals climb.

- Accept as first cut
- Tune after the groundwork rerun

> **Recommendation:** Accept; re-read after BL-1615 (rival space groundwork) lets them launch.

> **RESOLVED.** Ben, 2026-11-10: accepted with every call taken on your behalf in this stretch ("accept all, tune in playtest").

*Files: `src/world/nation_ai.cpp`*

### NR-1178 — Space battle dials and stand-ins: pooled armour can stall a battle, double-count across two battles at one node
*decision taken on your behalf · raised 2026-11-10 · from BL-1597 lane report*

Defence module firepower 12, armour 6, hull points 40, mass 6; core mass 20; 16 closed reports kept. Armour is spent once per target per day, so a side whose whole fire is at or below the front target armour lands nothing and the battle can stall with no forced end. Defence damage lives in the battle record (no repair rule). One battle per (node, nation pair): a nation hostile to two others at one node fires its whole firepower into both. Crew loss may be counted in both the battle and the station step reports. Withdrawn fleets can meet again where they withdraw to.

**Why it matters.** A stalled battle never ends; the double-count favours a side fighting two enemies.

- Accept and add a stall rule later
- Add a daily minimum damage now
- Split a nation fire across its battles

> **Recommendation:** Add a stall rule (a battle with no damage for N days ends as a standoff) with BL-1598 (debris).

> **RESOLVED.** Ben, 2026-11-10: accepted with every call taken on your behalf in this stretch ("accept all, tune in playtest").

*Files: `src/world/space_battle.cpp`, `docs/military/SPACE_FLEETS.md`*

### NR-1179 — Mapping then prospecting: the old probe and the two-step model overlap; deposits still show on mapped ground
*question · raised 2026-11-10 · from BL-1600 lane report*

Other bodies do have tile grids (Cinder 180x84, Selene 90x42, asteroids 30x14), and survey regions are the existing 8-tile partition. Knowledge is now per (nation, body); the home world counts as surveyed for all. A dispatched probe still reveals tiles and deposits the old way, and a body being mapped from orbit refuses a probe. The Planetary canvas gates deposits on survey_tile_visible only, so mapped ground still shows deposits before prospecting (UI work, noted on BL-1610 (base and prospect UI)). corp_ai reads the player mirror body.survey for fog. Novel: per-nation geographic fog state. Dials: satellite 4 regions/quarter, Observatory 2, prospect 1/quarter; region 0 stands in for the landing site; prospecting needs no comms coverage.

**Why it matters.** Two routes to the same knowledge confuse the player; the AI reads the player view.

- Retire the probe in favour of mapping then prospecting
- Keep the probe as a second, cheaper route

> **Recommendation:** Retire the probe; one route reads cleaner.

> **RESOLVED.** Ben, 2026-11-10: retire the probe; mapping then prospecting is the one route. Work: BL-1639 (retire the survey probe).

*Files: `src/world/survey_system.cpp`, `docs/ui/DISCOVERY.md`*

### NR-1180 — Space convoys: the straight-line lane kept for nations with no Hauler design; a chemical Hauler cannot round-trip home to moon
*decision taken on your behalf · raised 2026-11-10 · from BL-1596 lane report*

A nation with a live Hauler design ships inter-body trades as Haulers; one without still uses the old straight-line space lane (kept so worlds without designs do not move; the backlog says Haulers replace it). Unescorted Haulers meeting an armed hostile are lost with cargo; escorted ones wait (battles now exist, so the wait should become a battle). Unloading uses the lowest-id staffed pad of ANY nation on the body. The cargo cap reserves fuel for the empty trip home, with no refuelling counted, so the default chemical Hauler cannot round-trip home-moon; interplanetary Hauler trade is effectively closed until refuelling at the destination is counted. Each hop takes at least a day. Novel: the first cross-store save check (each outbound haul matches its live convoy).

**Why it matters.** As built, Hauler trade rarely pays and the old lane still carries most space trade.

- Count destination refuelling in the cap
- Retire the old lane now

> **Recommendation:** Count destination refuelling where the owner has a pad there; retire the old lane once rivals hold Hauler designs (BL-1608 (rival far-space AI)).

> **RESOLVED.** Ben, 2026-11-10: accepted with every call taken on your behalf in this stretch ("accept all, tune in playtest").

*Files: `src/world/space_convoy.cpp`, `src/world/supply_system.cpp`, `docs/military/SPACE_FLEETS.md`*

### NR-1217 — Solar storms are the first campaign event built ahead of the event system: hard-coded, derived from the seed, purely negative
*novel-work · raised 2026-11-10 · from BL-1605 lane report*

A storm is a pure function of the world seed and econ tick: one seeded draw per 12-quarter solar cycle (750 in 1000 carry a storm), so about three a decade and nothing saved. Every L-point and every asteroid orbit is exposed; planet and moon orbits are sheltered (binary; the doc names no strength or shielding). A struck satellite is dark for every role read; a struck station stops all modules for the quarter; nothing is worn or lost; astronauts untouched. A working Observatory warns its owner a quarter ahead; only the player row posts, on the Field channel. It is C++, not a Lua event on condition_set / modifier_set, because the general event system (BL-548) is not built; EVENTS.md says no event may be purely negative, and this one is. The canvas does not yet mark a dark satellite or stormed station.

**Why it matters.** It sets a precedent for events built before the event system.

- Accept as a hard-coded stand-in, migrate when the event system lands
- Give storms an upside now (e.g. research from an observed storm)

> **Recommendation:** Accept; add the upside and the canvas marker with BL-1609 (fleet UI).

> **RESOLVED.** Ben, 2026-11-10: accepted with every call taken on your behalf in this stretch ("accept all, tune in playtest").

*Files: `src/world/solar_storm.hpp`, `docs/EVENTS.md`*

### NR-1218 — Space ledger: rail slot 9 renumbers History and the developer tail; a verify fixture writes the player tech, buildings and treasury
*decision taken on your behalf · raised 2026-11-10 · from BL-1591 lane report*

Space sits at rail slot 9 after Diplomacy, under MENU.md rule that curated systems go above the developer tail; History and the tail move down and the rail has 13 slots (nav_rail_fit passes at 1280x720 and 1920x1080). verify.space_ledger_fixture() stages the player nation with every Digitisation node, a staffed pad and training centre, 40 astronauts and 5M credits; it reveals no tile data. Gaps against ledgers/space.md: applying a template does not quote its total first; satellite Move has no control; the Assets list does not show per-module rows. A live click in the built app is still owed (the scripted run passes 39 checks).

**Why it matters.** Rail order is a layout call; verify staging calls are listed by date in the standing rules.

- Accept slot 9 and the fixture
- Move Space elsewhere on the rail

> **Recommendation:** Accept; record the staging fixtures (this and near_space_fixture) as a dated verify grant if you want them listed.

> **RESOLVED.** Ben, 2026-11-10: accepted with every call taken on your behalf in this stretch ("accept all, tune in playtest").

*Files: `src/ui/space_ledger.cpp`, `src/ui/nav_pane.cpp`, `docs/ui/MENU.md`, `src/core/verify_api.cpp`*

### NR-1219 — Debris and the battle stall: a standoff after 10 quiet days, settled in SPACE_FLEETS.md by the lane
*novel-work · raised 2026-11-10 · from BL-1598 lane report*

Debris per node grows from every orbital loss (battle, upkeep, wear), wears assets (0.004 a quarter per unit of debris) and damages units daily (0.002 hp per debris), decays by drag in low orbit only (0.08 a quarter), and an idle Tug with a capture arm clears 6 a quarter at its node (no new verb: moving the Tug there is the order). The cascade is emergent (wear reaches 1, the wreck raises the level), with no separate threshold. The battle stall answers NR-1178: a battle with no damage landed for stall_days = 10 ends as a standoff, and that pair does not re-engage at the node until one side could land fire. No doc owned the stall, so the lane wrote it into SPACE_FLEETS.md § Battles as settled design. No UI reads debris yet.

**Why it matters.** A rule written into an authority doc without a ruling.

- Confirm the stall rule
- Change the window or the re-engage condition

> **Recommendation:** Confirm.

> **RESOLVED.** Ben, 2026-11-10: accepted with every call taken on your behalf in this stretch ("accept all, tune in playtest").

*Files: `src/world/debris.cpp`, `src/world/space_battle.cpp`, `docs/military/SPACE_FLEETS.md`, `docs/economy/SPACE_ASSETS.md`*

### NR-1195 — Blockade and bombardment built with four calls taken on your behalf
*decision taken on your behalf · raised 2026-11-10 · from BL-1599 lane report*

(1) A bombardment strike charges the existing force_used Alarm path; there is no separate missile Alarm code. (2) The orbital defence battery is a new building type, number 15, opened by its own Early Space node; it fires at hostile units in its body's low orbit. (3) Strikes spend no munitions; the cost is the unit's upkeep and its exposure to the battery. (4) A unit the battery kills does not raise debris, unlike a battle loss. Save v89 carries the standing bombard orders; blockade is derived and adds no bytes.

**Why it matters.** Calls (1), (3) and (4) shape how costly orbital war is.

- Accept all four
- Give strikes a munitions cost
- Feed battery kills into debris

> **Recommendation:** Accept for v0.5.0; feeding battery kills into debris is a one-line follow-up if wanted.

> **RESOLVED.** Ben, 2026-11-10: accepted with every call taken on your behalf in this stretch ("accept all, tune in playtest").

*Files: `src/world/space_bombard.cpp`, `docs/military/SPACE_FLEETS.md`*

### NR-1196 — Rivals cannot bombard or blockade yet: no AI uses the new verbs
*novel-work · raised 2026-11-10 · from BL-1599 lane report*

The bombard order and the battery are player-and-script only. Rival use belongs to BL-1608 (rival far-space AI), which is open; the 2026-11-10 grant covers it.

**Why it matters.** The spectator check will show rivals never striking until BL-1608 lands.

- Leave it to BL-1608 (rival far-space AI)

> **Recommendation:** Leave it to BL-1608.

> **RESOLVED.** Resolved by BL-1608 (rival far-space AI), whose war pick bombards from low orbit. Ben, 2026-11-10: accepted.

*Files: `src/world/corp_ai.cpp`*

### NR-1197 — Grant reading: rivals building their own Launchpad and Training Centre
*decision taken on your behalf · raised 2026-11-10 · from BL-1615 lane report*

The lane read the 2026-11-10 space grant (which names astronaut training) together with the 2026-10-23 own-economy build grant as covering a rival building its own pad and centre, on its own ground, through the build verb, paid only with twice the capex uncommitted. Never for the player nation. Pads are placed nearest the capital.

**Why it matters.** A grant read as covering a new subject is the pattern AI_OPPONENT.md § 11 asks to be raised.

- Confirm the reading
- Write it into § 11 as its own dated line

> **Recommendation:** Confirm, and add one line to § 11 so the reading is on record.

> **RESOLVED.** Ben, 2026-11-10: accepted with every call taken on your behalf in this stretch ("accept all, tune in playtest").

*Files: `src/world/corp_ai.cpp`, `docs/ai/AI_OPPONENT.md`*

### NR-1198 — Branch ids collide with main: BL-1580..1618 and NR-1181..1183, save v83..v88
*question · raised 2026-11-10 · from main session, merge prep*

This branch minted BL-1580..1615 and NR-1181..1183; main now holds BL ids up to 1618 and NR-1181..1194 (the naval merge), plus world save v83 (BL-1562) and v88 (naval) and envelope v29. New ids on this branch start at BL-1630 and NR-1195. Merging the branch into main needs a renumber pass of ids and save versions.

**Why it matters.** The merge is yours (NR-1162); this is its cost.

- Renumber this branch at the merge
- Renumber main's side

> **Recommendation:** Renumber this branch at the merge; its versions stack on top of main's.

> **RESOLVED.** Ben, 2026-11-10: merge after the launch fix; renumber this branch onto main's ids and save versions then.

*Files: `docs/development/backlog.json`, `src/world/world_save.hpp`*

### NR-1199 — Grant reading: the rival research picker also picks Early Space nodes
*decision taken on your behalf · raised 2026-11-10 · from BL-1613 lane report*

The research grant covers one frontier node at a time over the nation's own tree. Once a rival meets the gate into space, the picker also scores Early Space nodes it can queue alone (quests excluded), by the same rule; a tie keeps the Digitisation node. The lane read the second tree as the same verb on the same queue.

**Why it matters.** A second tree may be a new subject under AI_OPPONENT.md § 11.

- Confirm: same verb, same queue
- Treat it as a new grant and write it into § 11

> **Recommendation:** Confirm, with one dated line in § 11.

> **RESOLVED.** Ben, 2026-11-10: accepted with every call taken on your behalf in this stretch ("accept all, tune in playtest").

*Files: `src/world/nation_ai.cpp`, `docs/ai/AI_OPPONENT.md`*

### NR-1200 — Research age tabs: re-clicking the active tab closes the panel
*decision taken on your behalf · raised 2026-11-10 · from BL-1613 lane report*

The research canvas has Digitisation and Early Space tabs. Per the toggle rule, re-clicking the active tab closes the research panel. If the tabs count as a cross-cutting selector (exempt), that press should do nothing instead. The Early Space tree draws as three branch columns, not a ring (the lane's layout call).

**Why it matters.** A small UX call, and a layout you have not seen.

- Keep: toggle closes
- Exempt the tabs

> **Recommendation:** Keep; it matches every other sub-view tab.

> **RESOLVED.** Ben, 2026-11-10: accepted with every call taken on your behalf in this stretch ("accept all, tune in playtest").

*Files: `src/ui/tech_tree_panel.cpp`*

### NR-1201 — Off-world bases: colonists, the base kit and life support are stand-ins no doc defined
*novel-work · raised 2026-11-10 · from BL-1601 lane report*

Colonists are counted in thousands, one Lander passenger berth per thousand; a base needs 2. The base kit is 20 machinery carried as cargo on a landed Lander. Life support is 1 water and 1 food ration per thousand per quarter, scaled by the habitability the tile lacks (oxygen folded into water). An unfed quarter costs max(1, ceil(25%)) of the people; the base is razed at 0. Colonists come from the most populous home centre that keeps at least a thousand. All are dials in offworld_base_params. Founding lays one Track to a dry neighbour so the base can build at all. Save v90.

**Why it matters.** These numbers set how hard a base is to keep alive.

- Accept as first cut
- Retune any of them

> **Recommendation:** Accept for v0.5.0; tune in the v0.6.0 playtest.

> **RESOLVED.** Ben, 2026-11-10: accepted with every call taken on your behalf in this stretch ("accept all, tune in playtest").

*Files: `src/world/offworld_base.cpp`, `docs/politics/NATIONS.md`*

### NR-1202 — A fed base reads its province at habitability 1 (the building ceiling, labour and the hub source)
*decision taken on your behalf · raised 2026-11-10 · from BL-1601 lane report*

Five home-body assumptions were widened to a base: labour and workforce read the base habitability (1 when fed); the logistics chain anchor and the capital free source fall back to the base capital on that body; and measure_province_sustain, which capped an airless province at one building, reads a fed base province at habitability 1. The last is a design reading, not a bug fix.

**Why it matters.** It decides how much a base can grow.

- Accept: a fed base is habitable ground
- Keep the airless cap and let bases stay small

> **Recommendation:** Accept.

> **RESOLVED.** Ben, 2026-11-10: accepted with every call taken on your behalf in this stretch ("accept all, tune in playtest").

*Files: `src/world/province.cpp`, `src/world/economy_system.cpp`, `src/world/logistic_points.cpp`*

### NR-1204 — The BL-644 space-programme lumps are retired by data (authored at zero)
*decision taken on your behalf · raised 2026-11-10 · from BL-1630 lane report*

The lumps bought a whole launch lot off any shelf and consumed it with nothing launched; seed 0 1983-2000 bought 80 lumps and starved every launch. The real launch now buys the goods, so components_lump and propellant_lump are 0 in economy.lua and NATIONS.md says so. A staffed pad instead posts a standing bid (standing_lots = 1) on its home market, including the player's pad: a demand on the market, nothing bought or built for you. The state hash moves after 1980; no digest re-pinned.

**Why it matters.** Post-1960 pins will move at the next re-bless.

- Accept
- Limit the standing bid to rivals

> **Recommendation:** Accept.

> **RESOLVED.** Ben, 2026-11-10: accepted with every call taken on your behalf in this stretch ("accept all, tune in playtest").

*Files: `scripts/economy.lua`, `src/world/economy_system.cpp`, `docs/politics/NATIONS.md`*

### NR-1206 — Rival units at a moon base are not kept home: a scorer change, raised
*question · raised 2026-11-10 · from BL-1602 lane report*

corp_ai.cpp keeps the capital province's units at home, reading only the home capital. A rival's units at an off-world base would be sent to fronts. Treating a base province as a home to keep is a scorer change to land behaviour. The 2026-11-10 grant names space verbs; it is silent on land units at a base. Also taken by the lane: a cross-body front assignment is refused; the fallback war goal prefers the defender's base on that body; a ceded capital stays on its own body while land remains there (MILITARY.md § Space).

**Why it matters.** A rival base would be stripped of its garrison.

- Grant it: a base province is a home the scorer keeps
- Leave it for BL-1608 (rival far-space AI) to cover

> **Recommendation:** Grant it inside BL-1608; it is one line in the keep-home rule.

> **RESOLVED.** Ben, 2026-11-10: granted; a base province is a home the scorer keeps (the dial is switched on).

*Files: `src/world/corp_ai.cpp`*

### NR-1207 — Space firsts: five calls taken, and the player cannot see a first yet
*decision taken on your behalf · raised 2026-11-10 · from BL-1606 lane report*

(1) Ties go to the lowest nation id; firsts are judged by the day. (2) A flat 400 RP per first (economy.research.space_first_grant_rp), not scaled with the research price. (3) The convoy first counts on delivery to another body's market, not dispatch. (4) Crew in orbit includes crew aboard a station core, so a station core launch can take both firsts. (5) One public history entry per first, tagged with the taker. The player's dispatch shows only war entries, so no surface shows a first yet. Calls 1 and 2 are written into ERAS.md § Firsts. Save v91.

**Why it matters.** The grant size sets how much racing to space pays.

- Accept all
- Scale the grant with the research price
- Count the convoy at dispatch

> **Recommendation:** Accept; tune the 400 in playtest.

> **RESOLVED.** Ben, 2026-11-10: accepted with every call taken on your behalf in this stretch ("accept all, tune in playtest").

*Files: `src/world/space_firsts.cpp`, `docs/economy/ERAS.md`*

### NR-1208 — Fleet UI: nine calls taken, and seven new verify reads to grant
*decision taken on your behalf · raised 2026-11-10 · from BL-1609 / BL-1614 lane report*

Calls: (1) unit orders are not signal-delayed; (2) orders sit on the Selection card, not a right-click; (3) bombard targets whole provinces, listed where a building can be struck; (4) one fleet mark per (node, owner), a press selects the lowest-id lead, "Whole fleet" moves them all; (5) an asset's transit share is quarters gone plus day/90, a unit's from path days; (6) the card has two columns, destinations cheapest first; (7) the space designer expands rows in place; Recruit has no Space tab; (8) theatre_tabs became a template for four Design tabs; (9) a digest from a world with assets in transit may move (the origin is hashed). The space_command seam gained eight unit verbs (13-20) from a UI item. New verify reads, each reporting what the screen draws (screen positions included): verify.circum_transits, circum_fleets, solar_fleets, circum_fleet_hovered, circum_debris, fleet_card, space_designs; STAGING verify.space_fleet_fixture writes a Frigate design, four units and debris 25 (returns counts and the player id); circum_assets rows gain a storm flag. A solar storm cannot be staged (derived from the seed), so no capture shows the storm mark.

**Why it matters.** The verify reads touch the no-tile-data rule (io-standing-rules § Determinism & data model); each needs your grant like the ten before it.

- Grant the reads as an eleventh scoped exception
- Narrow them

> **Recommendation:** Grant: they report only what the canvas draws.

> **RESOLVED.** Ben, 2026-11-10: granted as a scoped exception (io-standing-rules.md, eleventh).

*Files: `src/core/verify_api.cpp`, `src/world/space_command.hpp`, `.claude/rules/io-standing-rules.md`*

### NR-1209 — Rival far-space AI: five calls taken; rivals design but raise nothing (the NR-1203 wall)
*decision taken on your behalf · raised 2026-11-10 · from BL-1608 lane report*

One far-space verb per rival per quarter, first wanted and legal of: design (best researched item per slot, redesign when a better one is researched), war (bombard from low orbit, move the fleet whole only when every unit there is armed, raise Frigates up to 2 per hostile, max 6), base (embark, load kit, move, found on the most-deposit prospected tile; max 1 base). Calls: (1) a base trip a full Lander cannot make with the 20-unit kit is refused (14.6 dv loaded vs 21.7 empty), so a heavy moon never gets a base; (2) once a rival holds a Hauler design the existing trade code ships its inter-body trade by Hauler, which moves the world from ~2020 (BL-1596's design); (3) Tugs and Cruisers get designs but are never raised; no shipyard or docking used; (4) the pick pays at the fair-price ceiling; (5) the pick's priority order is written only in nation_ai.hpp, not AI_OPPONENT.md. NR-1206 keep-home is wired behind far_space_keep_base_home = 0 (OFF). Spectate seed 0 to 2030: 9 designs held by three rivals by 2030, no unit raised (Teishe at war meets shelf_short for its Frigate), 1 launch all game.

**Why it matters.** The far-space AI exists and is correct in harness, but the world never lets it act: everything waits on NR-1203.

- Accept the calls
- Write the priority order into AI_OPPONENT.md

> **Recommendation:** Accept; rule NR-1203 first, then re-spectate (BL-1611).

> **RESOLVED.** Ben, 2026-11-10: accepted with every call taken on your behalf in this stretch ("accept all, tune in playtest").

*Files: `src/world/nation_ai.cpp`, `src/world/corp_ai.cpp`, `docs/ai/AI_OPPONENT.md`*

### NR-1210 — Propellant loop: the Electrolysis Plant keeps refined fuel as an input, against "water and power"
*decision taken on your behalf · raised 2026-11-10 · from BL-1604 lane report*

No new building types: the Ice Extractor is the extraction site on an ice deposit, the Electrolysis Plant the existing propellant_electrolysis route (water x3 + refined fuel x1). ERAS.md's ruling says water and power; the lane kept the recipe and wrote that the fuel half is shipped in or made locally. Also: the Ice Extractor key applies only off the builder's home body; the electrolysis key is read on the tile owner (so the propellant chain stays open to every nation at home); a Fuel depot over a base fills from the base shelf, else home; neither fill path checks blockade.

**Why it matters.** With refined fuel as an input a base still depends on home for half its propellant.

- Keep refined fuel (a base must import)
- Make it water and power only (a base is self-sufficient)

> **Recommendation:** Keep: it gives Haulers a reason to run; rule otherwise if bases should close the loop alone.

> **RESOLVED.** Ben, 2026-11-10: keep refined fuel as an input; a base imports half its propellant.

*Files: `scripts/recipes.lua`, `docs/economy/ERAS.md`*

### NR-1211 — Base and prospect UI: five calls taken, and four new verify reads to grant
*decision taken on your behalf · raised 2026-11-10 · from BL-1610 / BL-1633 lane report*

Calls: (1) the player picks the base site from a list, one row per province with mapped unclaimed land, at its most habitable tile, because a Lander lands at a surface node and no tile is behind it; (2) ground a dispatched probe is scanning keeps showing its deposits, and a fully surveyed body shows everything (DISCOVERY.md leaves a probed body to its probe); (3) the unprospected wash is 30% slate (palette::unprospected), subtle over grey regolith; (4) two deposit reads were left raw: the construction-suitability wash and the extraction candidates on the tile card, which could show bands on an unprospected base province; (5) Embark defaults to every free berth. BL-1633: Assign to front lists only fronts on the units' own body. New verify reads: verify.base_card() (the base orders as drawn, including each site row's label, grid col/row, chosen and prospected flags), verify.prospect_mask() (washed tile count, lens key goods, the title's mapped/prospected/total counts); STAGING verify.base_prospect_fixture() (grants the base_founding key, maps a moon, lands two player Landers with colonists and kit, idles an empty Lander in home low orbit; returns the moon name and three unit ids) and verify.stage_prospect(n) (prospects up to n regions by the real rule; returns the count); fleet_card().presses gains embark/confirm_embark/found_base/confirm_found.

**Why it matters.** The reads touch the no-tile-data rule; base_card returns site tile positions the card already prints. Call (4) is a small leak of deposit information on unprospected ground.

- Grant the reads; fix call (4) as a follow-up
- Narrow base_card to labels only

> **Recommendation:** Grant, since they report what the card draws; route the two raw reads through prospect_view.hpp.

> **RESOLVED.** Ben, 2026-11-10: granted as a scoped exception (io-standing-rules.md, eleventh).

*Files: `src/core/verify_api.cpp`, `src/ui/prospect_view.hpp`, `.claude/rules/io-standing-rules.md`*

### NR-1212 — Novelty: a base-site picker list, and a shared prospect-visibility test
*novel-work · raised 2026-11-10 · from BL-1610 lane report*

The design note expected a Lander "landed on a mapped tile"; a Lander lands at a surface node, so the lane built a site list the note did not anticipate. src/ui/prospect_view.hpp is a new shared piece every deposit-drawing surface now asks. Scope stayed small.

**Why it matters.** Novelty should be chosen, not accreted (standing rules).

- Accept the list as the base-site idiom
- Replace it with a press on the Planetary canvas

> **Recommendation:** Accept for v0.5.0; revisit in the v0.6.0 polish pass if it reads poorly in play.

> **RESOLVED.** Ben, 2026-11-10: accepted with every call taken on your behalf in this stretch ("accept all, tune in playtest").

*Files: `src/ui/circum_units.cpp`, `docs/ui/SELECTION.md`*

### NR-1213 — Garrison chase fix moves the campaign march; two harness re-aims taken
*decision taken on your behalf · raised 2026-11-10 · from BL-1634 / BL-1632 lane reports*

BL-1634 (garrison chase loop): units on a defend order now step last in the daily march, and re-aim at where the day's steps left the hostile stacks (economy_system.cpp run_unit_march; MILITARY.md § Defend province). Chosen over a swap/cross rule because the chase never swaps or crosses tiles. Side effect: a stack entering a province draws its defenders the same day, not the next. Any world where a defender steps beside other units moves, so play-path digests and sweeps may move; none of the harnesses run pins one. BL-1632 (stale tail harnesses): one helper, world_save_tail_bytes_after, replaces the hand sums; a static_assert breaks the build when the save version passes 92 until the new section joins its table. Calls: war_harness RF1 asserted ver == 76 and now asserts ver == world_save_version (as R4j and R9b do); tile_contact T13e and quarterly_return R5 passed only by corrupting the wrong section and are re-aimed at the bytes they name.

**Why it matters.** The march change is a world-mover folded into the pending digest re-pin (NR-1167).

- Accept the step rule
- Prefer a same-tile meeting rule instead

> **Recommendation:** Accept: one sentence, deterministic, reuses the existing targeting.

> **RESOLVED.** Ben, 2026-11-10: accepted with every call taken on your behalf in this stretch ("accept all, tune in playtest").

*Files: `src/world/economy_system.cpp`, `docs/military/MILITARY.md`, `src/world/world_save.cpp`*

### NR-1214 — Research pacing has drifted out of its pin (first research comes sooner and less evenly): re-pin, or treat it as a fold defect
*question · raised 2026-11-10 · from BL-1635 lane report*

nation_tech_harness R0h pins median quarters-to-first-research in [16,24]; R0i pins p10 >= 8. Pooled over the 16 library seeds: 10-15 db77bdde 20/11; 10-23 17/9; 10-24 17/7 (R0i first fails, the nations-own-everything wave); 10-25 15/5 (R0h first fails); 11-01 19/6; 11-03 15/5; HEAD 15/6. The largest single step on a two-seed probe (46/28) is f1d05dfc BL-1402 (fold to ground): 15/15 to 12/12, then small drops at the road-join, site-days and port-links commits. The seed re-curation is not the cause. Not re-pinned. Also from the same lane: well_gate shields its fixture from the rival dial with ai_cooldown = 8 (seating the player conflicts with the harness's seat move); railway R7/D now compares against a twin world with the track erased, since a nation may briefly run a negative treasury (BL-1482).

**Why it matters.** Research is reached faster and more unevenly than the pacing band intends; either the band or the fold is wrong.

- Re-pin the band to the post-fold reading (median 15, p10 6)
- Treat it as a fold defect and file the work

> **Recommendation:** Your call: the drift is the cost of nations owning their ground, and it is not obviously wrong.

> **RESOLVED.** Ben, 2026-11-10: re-pin the pacing band to the post-fold reading.

*Files: `tools/verify/nation_tech_harness.cpp`, `src/world/state_industry.cpp`*

### NR-1225 — Search placement-only: re-pin the shipped digests; rounds = 2 keeps 61% of the gain
*decision taken on your behalf · raised 2026-11-13 · from BL-1646 (search placement only) lane and cold review*

The search now walks placement alone over 2 rounds (1 seed + 2 evaluations), term 5 kept. Every row of player_seed_sweep k_shipped_digest_pins moves (D_search, D_land, D_settle, D_seat, 16 of 16): the path, the evaluation count and the winners change. Rounds = 2 was picked as the smallest count keeping >= 60% of the 6-round composite gain pooled over the 16 curated seeds (61.2%; k=1 53%, k=3 72%). world_determinism is unmoved (generation never calls the search).

**Why it matters.** The digest gate fails 16/16 until re-pinned, and the pin rule needs your go. Two rounds is a cost-for-quality trade inside the 60 s world budget.

- Re-pin the digests at the merged tree (after BL-1645 lands)
- Hold the re-pin for the v0.6.0 re-bless
- Take rounds = 3 (72% of the gain, about 3 s more on seed 0 with the fast field: 3.0-3.2 s an evaluation)

> **Recommendation:** Re-pin once BL-1645 (world under 60 s) lands, at one re-bless for both; keep rounds = 2.

> **RESOLVED.** Ben, 2026-11-13: option C. Rounds = 3 (72% of the six-round gain); slowest curated world 35.1 s against the 60 s budget. Digests re-pinned the same day after BL-1645 landed; the shape summary is in the pin comment (player_seed_sweep.cpp).

*Files: `src/world/finish_campaign_world.cpp`, `tools/verify/player_seed_sweep.cpp`*

### NR-1232 — Live grade shader: a draw-time grade needs a per-pixel land weight the chunks no longer hold
*question · raised 2026-11-14 · from BL-1659 (live grade shader), sprint 68*

The baked grade lifts land and leaves water, so a draw-time grade must know, per pixel, how much of it is land. The opaque master chunks are held as RGB since BL-1311 (master chunk RAM), so alpha cannot carry that weight. The baked grade also runs before the unsharp pass and the shadows; a draw-time grade after them is close, not exact.

**Why it matters.** Without a land weight the live grade lifts the sea too. Each way of carrying the weight costs ground memory that BL-1311 just won back.

- A) Hold opaque chunks as RGBA again: +33% master memory, exact weight
- B) A half-resolution one-byte land mask per chunk, sampled with filtering: about +8% of master memory, one extra texture bind, a softer shore in the grade
- C) Keep the grade baked; the live lever stays a re-bake, and the item closes unbuilt

> **Recommendation:** B. It keeps most of BL-1311's saving (a full-resolution byte would cost the same as RGBA), and the mask is reusable (shore fade, water glint). Accept that the grade then sits after unsharp, a visible but small change, with one re-bless.

> **RESOLVED.** Ben, 2026-11-15: try D (a per-tile land table, no stored weight), fall back to B (a half-resolution land mask) if the hex-side shore shows. D built: RENDERING.md § Art direction and palette, 'The lift at draw time'. Only the land lift moved to draw time; the near-future grade stays baked.

*Files: `src/ui/body_surface_canvas.cpp`, `docs/ui/RENDERING.md`*

### NR-1233 — Compressed ground textures: the GPU path can draw them, but measuring needs a vendored encoder
*question · raised 2026-11-14 · from BL-1662 (compressed ground textures) spike, sprint 68*

SDL 3.4's GPU renderer wraps an external SDL_GPUTexture (SDL_PROP_TEXTURE_CREATE_GPU_TEXTURE_POINTER), so a BC1 or BC7 chunk can be drawn on the GPU path. The software renderer (every --verify capture) cannot, so captures would keep the RGB chunk. The open cost is encode time per 512 px chunk at bake, which needs an encoder in the tree.

**Why it matters.** BC1 is 6:1 against RGB (BC7 3:1), the largest memory lever left for the ground. It adds a third-party dependency and a second chunk format beside the capture path.

- A) Vendor a single-file BC1 encoder (stb_dxt, public domain) and measure encode ms per chunk
- B) Vendor a BC7 encoder (bc7enc, MIT) for quality, slower to encode
- C) Defer: close the item unbuilt until ground memory binds again

> **Recommendation:** A for the measurement only; decide the format from the numbers.

> **RESOLVED.** Ben, 2026-11-16: option A. Vendor stb_dxt and measure BC1 encode time per 512 px chunk; decide the format from the numbers.

*Files: `src/core/gpu_effects.cpp`, `docs/ui/RENDERING.md`*

### NR-1236 — The flat ground stays on the software path: --verify captures still draw the squashed 2D map
*question · raised 2026-11-16 · from BL-1671 (flat ground retired), sprint 68*

On the GPU renderer the canvas now draws only the 3D ground: the squash is off, marks are draped, markers stand upright and clicks ray-pick. The software renderer (every --verify capture, and any machine without a GPU device) cannot run the 3D pass, so it still draws the flat, squashed map. Retiring that path too means moving --verify onto the GPU renderer, which moves every canvas golden and capture at once.

**Why it matters.** Until it moves, a green visual run proves the 2D map, not the 3D one the player sees. Moving it is a bulk re-bless, which the golden policy reserves for you.

- Keep the software path as the --verify map for now; check the 3D canvas by live click
- Move --verify to the GPU renderer and re-bless the canvas captures once
- Delete the 2D path and refuse to run the canvas without a GPU device

> **Recommendation:** Keep it for now, then take the one re-bless when the 3D defaults settle.

> **RESOLVED.** Ben, 2026-11-17 ("Let's work on BL-1671"): options B and C. --verify runs the GPU renderer and bakes the 3D ground; the flat ground draw, the squash camera and the fixed-angle ground shader are deleted; a renderer with no GPU device draws no ground and says so. The two ground goldens were re-blessed once on the GPU renderer.

*Files: `src/ui/body_surface_canvas.cpp`, `src/core/ground_slice_3d.cpp`, `docs/ui/RENDERING.md`*

### NR-1237 — Compressed ground textures: BC1 measures cheap and close to lossless - build the GPU path, or close the spike?
*question · raised 2026-11-16 · from BL-1662 (compressed ground textures) spike, sprint 68*

The spike measured BC1 with stb_dxt on 18 chunks of 512 px: a median 3.32 ms a chunk on one core, PSNR 41.1 dB median (38.8 worst); by eye the loss is the finest grain noise. A 2.9 GB chain would hold about 0.5 GB. The 3D ground draws only on the GPU renderer, which can take a BC1 texture; the software path and every --verify capture would keep RGB.

**Why it matters.** It is the largest memory lever left for the ground: a finer master, or many more resident chunks, in today's budget. The cost is a second chunk format beside the capture path, and a lossy one.

- A) Build it: BC1 chunks on the GPU renderer, RGB kept for software and captures
- B) Close the spike unbuilt; reopen when ground memory binds again
- C) Measure BC7 first (bc7enc), for quality, before deciding

> **Recommendation:** B for now: nothing binds on memory today, and the 3D ground has moved where the budget goes. The numbers stay in the item for when it does.

> **RESOLVED.** Ben, 2026-11-17: option B. Close the spike unbuilt; the BC1 measurements stay on BL-1662 (compressed ground textures) for when ground memory binds again.

*Files: `src/core/ground_slice_3d.cpp`, `docs/ui/RENDERING.md`*

