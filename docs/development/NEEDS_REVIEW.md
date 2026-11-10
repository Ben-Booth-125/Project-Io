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

*77 entries — 74 open, 3 resolved.*

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

### NR-1160 — Near-space design filled in on your behalf: slot counts, signal delay of one quarter, data home only through coverage
*decision taken on your behalf · raised 2026-11-10 · from space-mechanics session (worktree claude/space-mechanics-design), writing your form answers into SPACE_ASSETS.md*

Your answers set the shape; I chose the rest. Nodes: surface, low, high per body, plus L1/L2/L4/L5 per pair (L3 left out). Slots: low 8 + 4 per Earth mass, high 4 (planet) or 2, L4/L5 3, L1/L2 2; first come, first held. Launch buys spacecraft_components + propellant off the home shelf. An order outside comms coverage waits one quarter; research, observation and survey data are earned only in coverage. An edge module (Dock, Shipyard) enclosed by a grid upgrade goes idle until refitted. Upgrades: 5x3 -> 7x5 -> 9x7. Budgets stop the newest module first. Solar power falls with the square of orbital radius.

**Why it matters.** These are the numbers and rules a first playtest of v0.4.0 will feel.

- Keep them
- Name the ones to change

> **Recommendation:** Keep them until the spectator run and your playtest say otherwise.

*Files: `docs/economy/SPACE_ASSETS.md`*

### NR-1161 — Far-space design written whole on your behalf (SPACE_FLEETS.md and six sibling sections)
*decision taken on your behalf · raised 2026-11-10 · from space-mechanics session, designing v0.5.0 so its lanes can run while you are away*

New doc docs/military/SPACE_FLEETS.md: five hull classes (Hauler, Lander, Tug, Frigate, Cruiser; Cruiser only from a Shipyard); a dv budget that refuses an unaffordable move, cargo adding mass; refuel at a Dock+Fuel depot or over an owned Launchpad; a Dock berths two; space convoys are Haulers on the cheapest path, interdicted by hostile armed fleets on it; daily deterministic battles at a node; blockade = hostile armed fleet in low orbit with none of the holder's; bombardment from low orbit and an orbital defence battery. Also: debris as wear and cascade (SPACE_ASSETS), mapping-then-prospecting (DISCOVERY), bases as capitals that partition a body into provinces on first landing (NATIONS), war across every body (MILITARY), war-placed front pins (LAYOUT), seeded solar storms (EVENTS), firsts with a research grant, the propellant loop and the next age's tree (ERAS).

**Why it matters.** It is a large design taken without a form. Each piece is in its owning doc, so a change is an edit there.

- Keep and build as written
- Pause v0.5.0 lanes for a design form when you are back

> **Recommendation:** Keep and build; your v0.6.0 playtest is the place to reshape it.

*Files: `docs/military/SPACE_FLEETS.md`, `docs/economy/SPACE_ASSETS.md`, `docs/ui/DISCOVERY.md`, `docs/politics/NATIONS.md`, `docs/military/MILITARY.md`, `docs/ui/LAYOUT.md`, `docs/EVENTS.md`, `docs/economy/ERAS.md`*

### NR-1162 — Space work is on its own branch, ids from BL-1580 and sprints 65 and 66
*observation · raised 2026-11-10 · from space-mechanics session*

All space work lands on branch claude/space-mechanics-design (worktree .claude/worktrees/space-mechanics-design), cut from local main and merged with main as it moves. It is not merged into main: main's checkout is shared, and other sessions commit there. Backlog ids start at BL-1580 (a gap above BL-1567) and NR ids at NR-1160, so a parallel session minting on main does not collide; sprints are numbered 65 (near space) and 66 (far space).

**Why it matters.** Merging the branch into main, and whether 65/66 clash with a sprint another session opened, are yours to decide.

- Merge into main when you are back
- Keep it on the branch until v0.6.0

> **Recommendation:** Merge at each version cut, after your look.

*Files: `docs/development/sprints.json`, `docs/development/backlog.json`*

### NR-1163 — v0.3.0 (military) is not cut and the naval branch is unmerged, while v0.5.0 builds space war
*question · raised 2026-11-10 · from space-mechanics session, standing rule on milestone order*

ROADMAP says the numbers are naming order, not build order, and you asked for v0.4.0 and v0.5.0 now. But space battles, blockade and war-on-every-body build on the campaign military, and sprint 64's naval system sits on claude/naval-warfare-system-b0d978, unmerged. Space combat is built standalone on the orbital graph, not on the sea theatre.

**Why it matters.** If the naval branch merges later it may want to share interdiction rules with space convoys.

- Fine as is
- Merge naval first and reconcile interdiction

> **Recommendation:** Fine as is; reconcile at v0.6.0.

*Files: `docs/military/SPACE_FLEETS.md`*

### NR-1164 — Generated bodies other than the homeworld carry mass 1.0, so a moon's climb is cheap only because it has no air
*question · raised 2026-11-10 · from BL-1580 (orbital graph) lane report, confirmed by its harness WARN line on seed 20260911*

hard_coded_world.cpp sets mass_earths only on the homeworld; planetology's per-body masses are not copied onto body_component. On the generated world the moon climbs 8.0 dv against home's 12.16 (atmosphere does all the work), an asteroid climbs 8.0, and low-orbit slots read 12 for both instead of 8. Copying the masses fixes it but moves body_km_per_tile (logistics.cpp) and so very likely the digests.

**Why it matters.** Your item 7 wants smaller bodies cheaper to launch from; today the gap is half what it should be, and the asteroid is as heavy as the moon.

- Copy planetology masses in generation and re-bless the digests
- Keep body_km_per_tile on the old value and give the graph its own mass read
- Leave it until v0.6.0

> **Recommendation:** Copy the masses and re-bless at the v0.4.0 cut; you said continuity between releases is not a constraint.

*Files: `src/world/hard_coded_world.cpp`, `src/world/orbital_graph.cpp`, `src/world/logistics.cpp`*

### NR-1165 — Which Digitisation node unlocks each near-space thing, and the Early Space tree's first cut
*decision taken on your behalf · raised 2026-11-10 · from BL-1592 + BL-1607 lane report*

Every near-space unlock rides an existing Digitisation node as an added mechanic effect (no node added): e.g. Comms role on Communication Satellites, Navigation on Satellite Navigation, Observation on Thermal Imaging, Survey on Seismic Survey, astronaut training on Mass Higher Education, Dock on Gantry Cranes, Power on Photolithography, 7x5 grid on Numerical Control; the station core sits on Rocketry so a nation can found a station as soon as it can launch. The Early Space tree's first cut is three trees of one branch each (In-Situ Propellant, Interplanetary Convoys, Space Force; 54 nodes), keyed by the Digitisation quests named in keys_next_branch; the other six named branches are reserved. Kit item keys (two drives, two tanks, two armaments, two armours, a debris arm) were invented; quest deeds, advantage figures and Era 3 branch names are placeholders.

**Why it matters.** Where an unlock sits sets how soon a nation reaches each module, which is the opening of the space race.

- Keep
- Move named unlocks

> **Recommendation:** Keep until the spectator run shows how soon rivals reach each module.

*Files: `docs/economy/tech/DIGITISATION_TREE.md`, `docs/economy/tech/EARLY_SPACE_TREE.md`, `src/world/space_tech_keys.hpp`*

### NR-1166 — Novel work: per-tree research state so a nation can research the next age's tree
*novel-work · raised 2026-11-10 · from BL-1607 lane report*

The research queue has one held set and one queue, both sized to the Digitisation tree, and no Era 1 -> Era 2 gate query exists. Filed as BL-1612 (early space researchable): per-tree state with a save leg, the gate query, and branch keys by quest. All Digitisation quest deeds are declared unread, so no branch can be keyed today; the item reads them or lets the gate stand in.

**Why it matters.** It grows the research system from one age to two, which no doc had designed in code terms.

- Build it as filed
- Collapse far-space unlocks onto the Digitisation tree instead

> **Recommendation:** Build it as filed; it is the shape RESEARCH.md already describes.

*Files: `docs/economy/RESEARCH.md`, `src/world/nation_tech.cpp`*

### NR-1167 — Re-pin the player_seed_sweep digests at the v0.4.0 cut (the save format moved them, the world did not)
*question · raised 2026-11-10 · from BL-1583 lane report: save v80 -> v81*

Adding the station section bumped world_save_version 80 -> 81, and player_seed_sweep --digest-check fails seeds 46 and 31 on D_land, D_settle and D_seat (D_search unmoved). rebless_shape_probe on both trees: every ROW and MIX line identical, so the world is unchanged; only the stream moved, as with earlier version bumps. More space save legs land in sprint 65 (slots, roles, pending orders, per-tree research), each moving the stream again.

**Why it matters.** A failing digest gate hides any real world movement that lands beside it.

- Re-pin once at the v0.4.0 cut, with a shape probe either side
- Re-pin after each save leg

> **Recommendation:** Once at the v0.4.0 cut; I will run the shape probe and leave the re-pin for your go.

*Files: `tools/verify/player_seed_sweep.cpp`*

### NR-1168 — Station model calls: core costs, module numbers, which module stops, removal keeps crew
*decision taken on your behalf · raised 2026-11-10 · from BL-1583 + BL-1585 lane report*

Core launch 50,000 cr, upkeep 2,500; core gives power 4, crew room 2, life support 2, needs 1 crew. Module draw/crew: Research 3/1, Defence 2/1, Power 0/0 (+6), Dock 1/1, Habitation 1/0 (+4 room), Life support 2/0 (+4), Fuel depot 1/0, Shipyard 4/2, Factory 4/2, Observatory 2/1, Relay 1/0, Cargo 0/0, Market 1/1. On a shortfall the newest module drawing on the SHORT budget stops (a Power module is passed over in a crew shortfall). An upgrade grows the grid when ordered, its truss launched after. Removing an arrived module moves its crew onto the core; a lost core loses the whole station and its crew. Training: 2 astronauts per centre per quarter, pool cap 20 per running centre; the Astronaut Training Centre costs 300 cr, 240 days, 35 steel.

**Why it matters.** These set how fast a station grows and what a shortfall feels like.

- Keep
- Name the numbers to change

> **Recommendation:** Keep until the spectator run.

*Files: `src/world/stations.hpp`, `scripts/economy.lua`*

### NR-1169 — Early Space branches open on a stand-in until quest deeds are read: Rocketry alone keys Interplanetary Convoys
*decision taken on your behalf · raised 2026-11-10 · from BL-1612 lane report*

Every Digitisation quest deed is declared unread, so no quest can be held and no Early Space branch could ever open. Stand-in (one place, branch_keyed_in): a branch is keyed if its quest is held, or if the quest's deed is unread and its major prerequisite is held. Consequence: any nation that meets the Era 2 gate already holds Rocketry, so Interplanetary Convoys (hulls, landers, tugs, drives) opens with the gate; In-Situ Propellant needs DG-PR-EN-C, Space Force needs Proven Readiness's major. One research pool and one queue: Digitisation entries are paid first. New dial space_gate_min_propellant = 1.0 (one launch's draw), since ERAS.md names a Lua threshold that did not exist.

**Why it matters.** It decides how soon far space opens after the first launch.

- Keep the stand-in until deeds are read
- Read the space quest deeds now (first convoy to another body etc.)

> **Recommendation:** Keep; the firsts item (BL-1606, space firsts) reads the same deeds and can retire the stand-in.

*Files: `src/world/nation_tech.cpp`, `docs/economy/RESEARCH.md`*

### NR-1170 — A new near-space command seam (space_command.*) beside the nation command path
*novel-work · raised 2026-11-10 · from BL-1588 lane report*

Space verbs (launch, move, fit, retire, set role and so on; 11 gameplay.space_* entries in ACTIONS.json) go through a new space_command seam. It holds a pending-order queue for signal delay: an order outside comms coverage waits one quarter, then applies or is dropped if it is no longer legal. The seam validates as an untrusted boundary, but no --serve/MCP transport carries it yet.

**Why it matters.** It is a second command path; the AI and the wire must both use it.

- Keep it separate (space orders carry delay; ground orders do not)
- Fold it into the nation command path later

> **Recommendation:** Keep; file the --serve/MCP transport as v0.5.0 work.

*Files: `src/world/space_command.hpp`, `docs/ai/ACTIONS.json`*

### NR-1171 — Launch cost formula and slot use: base + 250 per dv + goods value; slots only for satellites and cores
*decision taken on your behalf · raised 2026-11-10 · from BL-1581/BL-1582 lane report*

Launch cost = base + 250 credits per unit of delta-v climbed + the market value of the goods bought off the home shelf. Only satellites and station cores take an orbital slot. A station core may launch to any orbital node, including L-points. Launch and move debits are paid but are not yet booked as a line in the nation return.

**Why it matters.** It sets how expensive space is, and the return under-reports the spend.

- Keep these numbers for v0.4.0 and tune after the spectator run
- Book the debits as a return line now

> **Recommendation:** Keep the numbers; book the return line with the Space ledger item, BL-1591 (space ledger and designer).

*Files: `src/world/space_command.cpp`, `docs/economy/SPACE_ASSETS.md`*

### NR-1172 — Station and satellite effect dials and stand-ins taken by the effects lane
*decision taken on your behalf · raised 2026-11-10 · from BL-1584/BL-1586 lane report*

Life support keeps no memory: a station either buys its rations this quarter or starves. Electronics stands in as the factory module's output good. An Observation satellite lifts only the vision fog, not the activity fog. Survey satellites start the mapping, at a stand-in rate until prospecting lands (v0.5.0). Dials: navigation factor 0.9, research module 50 RP a quarter; the rest are in space_effects.hpp.

**Why it matters.** These are first numbers; the spectator run will show whether they matter.

- Accept as first cut
- Revisit any dial

> **Recommendation:** Accept; revisit after the spectator observation.

*Files: `src/world/space_effects.cpp`, `docs/economy/SPACE_ASSETS.md`*

### NR-1173 — Space units: an authored hull and item stat table, the rocket equation, and a depot that buys propellant on its own
*novel-work · raised 2026-11-10 · from BL-1594/BL-1595 lane report*

Delta-v is the rocket equation over propellant aboard (never a stored delta-v). Five hulls (Hauler 10 mass/20 hp, Lander 8/15, Tug 8/20, Frigate 14/40, Cruiser 40/120 and yard-only) and eleven items (chemical drive ve 10, nuclear thermal ve 20, tanks 20/45, kinetic and missile armament, Whipple and composite armour, cargo and passenger bays, capture arm) are a new numeric table no doc owns. A working Fuel depot fills itself each quarter off the owner home shelf (200 a depot). Raising draws goods market-direct and credits no seller. Docking is own-nation only. Dials: launch_propellant_per_dv 0.1, repair 1/day, quarters_to_loss 4, berths_per_dock 2. Space-unit verbs, UI and cargo loading are owed (BL-1596 onward).

**Why it matters.** The table sets how far and how cheaply fleets move; the depot is a new automatic buyer.

- Accept the table as a first cut and tune at the v0.6.0 playtest
- Move the table into SPACE_FLEETS.md as authority now

> **Recommendation:** Accept; the numbers live on recipe_registry::space_units() and can move to Lua later.

*Files: `src/world/space_units.cpp`, `docs/military/SPACE_FLEETS.md`*

### NR-1174 — Pinned panel: pins travel in the UI save (save_game v28); the lens key and the panel share the column
*decision taken on your behalf · raised 2026-11-10 · from BL-1603 lane report*

Player pins are saved in the ui_state slice (save_game_version 28; v27 opens with no pins). Front pins are derived each frame and never saved: one per (war, body) where both sides hold land. The panel stops one margin above a grown lens key (a new shell_metrics hand-off). The pin toggle sits left of the band header go-to button, or ends the tile heading row. New verify read verify.pinned_panel() reports only what the panel draws; it reads no tile data, so no grant was taken. Fronts off the home body wait on BL-1602 (interplanetary war).

**Why it matters.** A save-format change and a chrome hand-off grew past a UI-only brief; a live click is still owed.

- Accept
- Keep pins out of the save

> **Recommendation:** Accept.

*Files: `src/ui/pinned_panel.cpp`, `src/core/save_game.cpp`, `docs/ui/LAYOUT.md`*

### NR-1175 — Circumplanetary assets: L5 clamped above the band, first-cut mark sizes, a new space_asset selection kind
*decision taken on your behalf · raised 2026-11-10 · from BL-1590 lane report*

At 1280x720 the bottom band (~280 px) covers the moon outer orbit, so its L5 diamond is clamped above the band and floats off its orbit when zoomed. The alternative is to lift the rung centre so the whole system clears the band. Satellite plate radius 5.5 px; station 8.5 px plus 2 per grid ring; the player dark magenta reads dim on the satellite plate. A new selection_kind::space_asset (a station selects by its core) and its band card were added; SELECTION.md carries the row. New verify calls near_space_fixture (staging) and circum_assets report only what the canvas draws and touch no tile data, so no grant was taken. The transit segment needs an origin node and departure tick on a moving asset (a save change): filed as work.

**Why it matters.** A visual call, and whether verify staging calls need a dated grant when they carry no tile data.

- Keep the clamp
- Lift the rung centre above the band

> **Recommendation:** Lift the rung centre; a clamped L-point lies about where it is.

*Files: `src/ui/circum_nodes.cpp`, `src/ui/circum_assets.cpp`, `docs/ui/CIRCUMPLANETARY.md`*

### NR-1176 — Spectator finding: rivals reach Rocketry (first 1976, 14 of 41 by 2000) but launch nothing, because no Launchpad ever exists
*decision taken on your behalf · raised 2026-11-10 · from BL-1589 lane report (seed 0 spectate to 2000)*

Rocketry holders: 2 in 1977, 3 in 1978, 7 in 1988, 14 by 1994. Launches 0, space spend 0: every holder is blocked no_launchpad. Nothing builds a Launchpad (not generation, not the nation build scorer), and placement_rules.cpp allows ONE Launchpad per body world-wide, so on a shared home world only one nation could ever launch. SPACE_ASSETS.md says a staffed Launchpad on the nation home body, i.e. one per nation. Training centres are never built either, so every astronaut pool is 0. Treasuries: richest about 35.6k in 1975, about 108k-123k by 2000; a comms satellite quotes about 23.5k, a core about 60k. DECISION TAKEN: the doc wins, the cap becomes one Launchpad per nation per body; rivals build a Launchpad and an Astronaut Training Centre through the build scorer on their own ground. I read this as inside the 2026-10-23 build grant (what to build, own ground) and the 2026-11-10 space grant (which names astronaut training), not a new widening; filed as BL-1615 (rival space groundwork).

**Why it matters.** This is the first hurdle Ben asked about: without it no rival goes to space at all.

- Accept: per-nation pads, rivals build pads and centres
- Generation opens a pad for each major instead
- Keep one pad per body and make it a contested asset

> **Recommendation:** Accept; it matches the doc and the existing grants.

*Files: `src/world/placement_rules.cpp`, `src/world/nation_ai.cpp`, `docs/economy/SPACE_ASSETS.md`, `docs/ai/AI_OPPONENT.md`*

### NR-1177 — Rival space scorer dials: research relief 0.25 for near-space keys, half-treasury reserve, a fixed 14-module target
*decision taken on your behalf · raised 2026-11-10 · from BL-1589 lane report*

The research picker weighed only Rocketry among mechanic keys; every other near-space key now earns research_space_key_relief 0.25 (half of research_unlock_relief) x the space emphasis. That moves rival research picks after the handover in every campaign, so post-handover campaign digests are expected to move (not measured, nothing re-blessed). Reserve: one action spends at most half the treasury and keeps 8 quarters of space upkeep. A comms satellite first, then a core at the cheapest free home node, then modules toward a fixed order (power, research, habitation, life support, power, research, observatory, comms relay, power, habitation, life support, research, fuel depot, cargo hold). Home body only; no upgrades, retasks or crewed missions. Never the player nation, even under spectate.

**Why it matters.** These set how fast and how far rivals climb.

- Accept as first cut
- Tune after the groundwork rerun

> **Recommendation:** Accept; re-read after BL-1615 (rival space groundwork) lets them launch.

*Files: `src/world/nation_ai.cpp`*

### NR-1178 — Space battle dials and stand-ins: pooled armour can stall a battle, double-count across two battles at one node
*decision taken on your behalf · raised 2026-11-10 · from BL-1597 lane report*

Defence module firepower 12, armour 6, hull points 40, mass 6; core mass 20; 16 closed reports kept. Armour is spent once per target per day, so a side whose whole fire is at or below the front target armour lands nothing and the battle can stall with no forced end. Defence damage lives in the battle record (no repair rule). One battle per (node, nation pair): a nation hostile to two others at one node fires its whole firepower into both. Crew loss may be counted in both the battle and the station step reports. Withdrawn fleets can meet again where they withdraw to.

**Why it matters.** A stalled battle never ends; the double-count favours a side fighting two enemies.

- Accept and add a stall rule later
- Add a daily minimum damage now
- Split a nation fire across its battles

> **Recommendation:** Add a stall rule (a battle with no damage for N days ends as a standoff) with BL-1598 (debris).

*Files: `src/world/space_battle.cpp`, `docs/military/SPACE_FLEETS.md`*

### NR-1179 — Mapping then prospecting: the old probe and the two-step model overlap; deposits still show on mapped ground
*question · raised 2026-11-10 · from BL-1600 lane report*

Other bodies do have tile grids (Cinder 180x84, Selene 90x42, asteroids 30x14), and survey regions are the existing 8-tile partition. Knowledge is now per (nation, body); the home world counts as surveyed for all. A dispatched probe still reveals tiles and deposits the old way, and a body being mapped from orbit refuses a probe. The Planetary canvas gates deposits on survey_tile_visible only, so mapped ground still shows deposits before prospecting (UI work, noted on BL-1610 (base and prospect UI)). corp_ai reads the player mirror body.survey for fog. Novel: per-nation geographic fog state. Dials: satellite 4 regions/quarter, Observatory 2, prospect 1/quarter; region 0 stands in for the landing site; prospecting needs no comms coverage.

**Why it matters.** Two routes to the same knowledge confuse the player; the AI reads the player view.

- Retire the probe in favour of mapping then prospecting
- Keep the probe as a second, cheaper route

> **Recommendation:** Retire the probe; one route reads cleaner.

*Files: `src/world/survey_system.cpp`, `docs/ui/DISCOVERY.md`*

### NR-1180 — Space convoys: the straight-line lane kept for nations with no Hauler design; a chemical Hauler cannot round-trip home to moon
*decision taken on your behalf · raised 2026-11-10 · from BL-1596 lane report*

A nation with a live Hauler design ships inter-body trades as Haulers; one without still uses the old straight-line space lane (kept so worlds without designs do not move; the backlog says Haulers replace it). Unescorted Haulers meeting an armed hostile are lost with cargo; escorted ones wait (battles now exist, so the wait should become a battle). Unloading uses the lowest-id staffed pad of ANY nation on the body. The cargo cap reserves fuel for the empty trip home, with no refuelling counted, so the default chemical Hauler cannot round-trip home-moon; interplanetary Hauler trade is effectively closed until refuelling at the destination is counted. Each hop takes at least a day. Novel: the first cross-store save check (each outbound haul matches its live convoy).

**Why it matters.** As built, Hauler trade rarely pays and the old lane still carries most space trade.

- Count destination refuelling in the cap
- Retire the old lane now

> **Recommendation:** Count destination refuelling where the owner has a pad there; retire the old lane once rivals hold Hauler designs (BL-1608 (rival far-space AI)).

*Files: `src/world/space_convoy.cpp`, `src/world/supply_system.cpp`, `docs/military/SPACE_FLEETS.md`*

### NR-1181 — Solar storms are the first campaign event built ahead of the event system: hard-coded, derived from the seed, purely negative
*novel-work · raised 2026-11-10 · from BL-1605 lane report*

A storm is a pure function of the world seed and econ tick: one seeded draw per 12-quarter solar cycle (750 in 1000 carry a storm), so about three a decade and nothing saved. Every L-point and every asteroid orbit is exposed; planet and moon orbits are sheltered (binary; the doc names no strength or shielding). A struck satellite is dark for every role read; a struck station stops all modules for the quarter; nothing is worn or lost; astronauts untouched. A working Observatory warns its owner a quarter ahead; only the player row posts, on the Field channel. It is C++, not a Lua event on condition_set / modifier_set, because the general event system (BL-548) is not built; EVENTS.md says no event may be purely negative, and this one is. The canvas does not yet mark a dark satellite or stormed station.

**Why it matters.** It sets a precedent for events built before the event system.

- Accept as a hard-coded stand-in, migrate when the event system lands
- Give storms an upside now (e.g. research from an observed storm)

> **Recommendation:** Accept; add the upside and the canvas marker with BL-1609 (fleet UI).

*Files: `src/world/solar_storm.hpp`, `docs/EVENTS.md`*

### NR-1182 — Space ledger: rail slot 9 renumbers History and the developer tail; a verify fixture writes the player tech, buildings and treasury
*decision taken on your behalf · raised 2026-11-10 · from BL-1591 lane report*

Space sits at rail slot 9 after Diplomacy, under MENU.md rule that curated systems go above the developer tail; History and the tail move down and the rail has 13 slots (nav_rail_fit passes at 1280x720 and 1920x1080). verify.space_ledger_fixture() stages the player nation with every Digitisation node, a staffed pad and training centre, 40 astronauts and 5M credits; it reveals no tile data. Gaps against ledgers/space.md: applying a template does not quote its total first; satellite Move has no control; the Assets list does not show per-module rows. A live click in the built app is still owed (the scripted run passes 39 checks).

**Why it matters.** Rail order is a layout call; verify staging calls are listed by date in the standing rules.

- Accept slot 9 and the fixture
- Move Space elsewhere on the rail

> **Recommendation:** Accept; record the staging fixtures (this and near_space_fixture) as a dated verify grant if you want them listed.

*Files: `src/ui/space_ledger.cpp`, `src/ui/nav_pane.cpp`, `docs/ui/MENU.md`, `src/core/verify_api.cpp`*

### NR-1183 — Debris and the battle stall: a standoff after 10 quiet days, settled in SPACE_FLEETS.md by the lane
*novel-work · raised 2026-11-10 · from BL-1598 lane report*

Debris per node grows from every orbital loss (battle, upkeep, wear), wears assets (0.004 a quarter per unit of debris) and damages units daily (0.002 hp per debris), decays by drag in low orbit only (0.08 a quarter), and an idle Tug with a capture arm clears 6 a quarter at its node (no new verb: moving the Tug there is the order). The cascade is emergent (wear reaches 1, the wreck raises the level), with no separate threshold. The battle stall answers NR-1178: a battle with no damage landed for stall_days = 10 ends as a standoff, and that pair does not re-engage at the node until one side could land fire. No doc owned the stall, so the lane wrote it into SPACE_FLEETS.md § Battles as settled design. No UI reads debris yet.

**Why it matters.** A rule written into an authority doc without a ruling.

- Confirm the stall rule
- Change the window or the re-engage condition

> **Recommendation:** Confirm.

*Files: `src/world/debris.cpp`, `src/world/space_battle.cpp`, `docs/military/SPACE_FLEETS.md`, `docs/economy/SPACE_ASSETS.md`*

### NR-1195 — Blockade and bombardment built with four calls taken on your behalf
*decision taken on your behalf · raised 2026-11-10 · from BL-1599 lane report*

(1) A bombardment strike charges the existing force_used Alarm path; there is no separate missile Alarm code. (2) The orbital defence battery is a new building type, number 15, opened by its own Early Space node; it fires at hostile units in its body's low orbit. (3) Strikes spend no munitions; the cost is the unit's upkeep and its exposure to the battery. (4) A unit the battery kills does not raise debris, unlike a battle loss. Save v89 carries the standing bombard orders; blockade is derived and adds no bytes.

**Why it matters.** Calls (1), (3) and (4) shape how costly orbital war is.

- Accept all four
- Give strikes a munitions cost
- Feed battery kills into debris

> **Recommendation:** Accept for v0.5.0; feeding battery kills into debris is a one-line follow-up if wanted.

*Files: `src/world/space_bombard.cpp`, `docs/military/SPACE_FLEETS.md`*

### NR-1196 — Rivals cannot bombard or blockade yet: no AI uses the new verbs
*novel-work · raised 2026-11-10 · from BL-1599 lane report*

The bombard order and the battery are player-and-script only. Rival use belongs to BL-1608 (rival far-space AI), which is open; the 2026-11-10 grant covers it.

**Why it matters.** The spectator check will show rivals never striking until BL-1608 lands.

- Leave it to BL-1608 (rival far-space AI)

> **Recommendation:** Leave it to BL-1608.

*Files: `src/world/corp_ai.cpp`*

### NR-1197 — Grant reading: rivals building their own Launchpad and Training Centre
*decision taken on your behalf · raised 2026-11-10 · from BL-1615 lane report*

The lane read the 2026-11-10 space grant (which names astronaut training) together with the 2026-10-23 own-economy build grant as covering a rival building its own pad and centre, on its own ground, through the build verb, paid only with twice the capex uncommitted. Never for the player nation. Pads are placed nearest the capital.

**Why it matters.** A grant read as covering a new subject is the pattern AI_OPPONENT.md § 11 asks to be raised.

- Confirm the reading
- Write it into § 11 as its own dated line

> **Recommendation:** Confirm, and add one line to § 11 so the reading is on record.

*Files: `src/world/corp_ai.cpp`, `docs/ai/AI_OPPONENT.md`*

### NR-1198 — Branch ids collide with main: BL-1580..1618 and NR-1181..1183, save v83..v88
*question · raised 2026-11-10 · from main session, merge prep*

This branch minted BL-1580..1615 and NR-1181..1183; main now holds BL ids up to 1618 and NR-1181..1194 (the naval merge), plus world save v83 (BL-1562) and v88 (naval) and envelope v29. New ids on this branch start at BL-1630 and NR-1195. Merging the branch into main needs a renumber pass of ids and save versions.

**Why it matters.** The merge is yours (NR-1162); this is its cost.

- Renumber this branch at the merge
- Renumber main's side

> **Recommendation:** Renumber this branch at the merge; its versions stack on top of main's.

*Files: `docs/development/backlog.json`, `src/world/world_save.hpp`*

### NR-1199 — Grant reading: the rival research picker also picks Early Space nodes
*decision taken on your behalf · raised 2026-11-10 · from BL-1613 lane report*

The research grant covers one frontier node at a time over the nation's own tree. Once a rival meets the gate into space, the picker also scores Early Space nodes it can queue alone (quests excluded), by the same rule; a tie keeps the Digitisation node. The lane read the second tree as the same verb on the same queue.

**Why it matters.** A second tree may be a new subject under AI_OPPONENT.md § 11.

- Confirm: same verb, same queue
- Treat it as a new grant and write it into § 11

> **Recommendation:** Confirm, with one dated line in § 11.

*Files: `src/world/nation_ai.cpp`, `docs/ai/AI_OPPONENT.md`*

### NR-1200 — Research age tabs: re-clicking the active tab closes the panel
*decision taken on your behalf · raised 2026-11-10 · from BL-1613 lane report*

The research canvas has Digitisation and Early Space tabs. Per the toggle rule, re-clicking the active tab closes the research panel. If the tabs count as a cross-cutting selector (exempt), that press should do nothing instead. The Early Space tree draws as three branch columns, not a ring (the lane's layout call).

**Why it matters.** A small UX call, and a layout you have not seen.

- Keep: toggle closes
- Exempt the tabs

> **Recommendation:** Keep; it matches every other sub-view tab.

*Files: `src/ui/tech_tree_panel.cpp`*

### NR-1201 — Off-world bases: colonists, the base kit and life support are stand-ins no doc defined
*novel-work · raised 2026-11-10 · from BL-1601 lane report*

Colonists are counted in thousands, one Lander passenger berth per thousand; a base needs 2. The base kit is 20 machinery carried as cargo on a landed Lander. Life support is 1 water and 1 food ration per thousand per quarter, scaled by the habitability the tile lacks (oxygen folded into water). An unfed quarter costs max(1, ceil(25%)) of the people; the base is razed at 0. Colonists come from the most populous home centre that keeps at least a thousand. All are dials in offworld_base_params. Founding lays one Track to a dry neighbour so the base can build at all. Save v90.

**Why it matters.** These numbers set how hard a base is to keep alive.

- Accept as first cut
- Retune any of them

> **Recommendation:** Accept for v0.5.0; tune in the v0.6.0 playtest.

*Files: `src/world/offworld_base.cpp`, `docs/politics/NATIONS.md`*

### NR-1202 — A fed base reads its province at habitability 1 (the building ceiling, labour and the hub source)
*decision taken on your behalf · raised 2026-11-10 · from BL-1601 lane report*

Five home-body assumptions were widened to a base: labour and workforce read the base habitability (1 when fed); the logistics chain anchor and the capital free source fall back to the base capital on that body; and measure_province_sustain, which capped an airless province at one building, reads a fed base province at habitability 1. The last is a design reading, not a bug fix.

**Why it matters.** It decides how much a base can grow.

- Accept: a fed base is habitable ground
- Keep the airless cap and let bases stay small

> **Recommendation:** Accept.

*Files: `src/world/province.cpp`, `src/world/economy_system.cpp`, `src/world/logistic_points.cpp`*

### NR-1203 — Launch goods sit on the wrong shelves: their trade routes rank too low to move (measured)
*question · raised 2026-11-10 · from BL-1630 lane report*

With a staffed pad bidding for its launch lot, plants appear (seed 0: 5 components, 10 propellant by 2000), but the goods pile on different markets of the one body: 2015, 47582 holds 1,468 components and 21 propellant; 47599 and 47665 hold 1,542 and 788 propellant. 0 launches through 2015. MEASURED (experiment lane, seed 0): the ceiling does silence some wants, but auto trade already sizes to them (BL-1547, dispatch_absorbable), and every route exists overland. What blocks is RANKING: auto trade ranks by margin per trade point, and launch goods move few units per point (components 0.13, propellant 0.6), so their routes rank #68-#141 of ~250 at the source and the points run out first (272 "no points left" against 3 sends in the sample). Logistic Points by value also bite: a component draws 77.9 LP, a propellant unit 16.1. EXPERIMENT (branch worktree-agent-a9f2b8a287ef3a8e6, 8aa5f49b, NOT merged): routes into a market with a silenced want rank first. Result: first rival launch 1992, 3 by 2005, 4 by 2015 (vs none). Cost: pads completed by 2000 fall 7 to 4; treasuries 2015 947k to 629k; market_viability quick G1 at tick 50 86.0 PASS to 80.8 FAIL (target 84). It moves every good from ~1970. OPTION D MEASURED (branch worktree-agent-a83d4ca4b411f9d7d, bbcb17b5, NOT merged; switch launch_draws_body_wide, off byte-identical): a good the home shelf cannot cover may come whole off another market on the home body where the nation owns ground, ascending market id, at that market's price, no haul charge. Seed 0 to 2010: first rival launch 1993; 4 launches by 2000 (Teishe: a comms satellite, then a station core; Thuamuas and Tososhei: comms satellites); none added 2000-2010; 7 of 11 padded nations still refused at the shelf, cause unprobed. Pads unchanged (7 by 2000, 11 by 2010). Treasury sum 2000: -87k. market_viability quick: identical (it ends ~1972, before Rocketry, so it cannot see D). RULING (Ben, 2026-11-10): D is rejected; no global pools for goods (written into SPACE_ASSETS.md). Measure B next. Ben also raised a fourth shape: a Launchpad produces boost, a good on its market, so the economic draw happens over time; a launch consumes boost by the distance in space.

**Why it matters.** The wall between rivals and space is a trade-ranking rule. The one lever tried works for space and hurts the wider economy.

- A) Ship the experiment (silenced-want routes first) and accept the G1t50 cost
- B) Give dense goods more trade capacity per point (the capacity table, TRADE.md)
- C) Relax the by-value Logistic Point draw at starved anchors
- D) Let a launch draw its lot from every market on its home body (a space rule, not a trade rule; SPACE_ASSETS.md says the home market)

> **Recommendation:** D first: it opens space without moving any other good, and its cost is one rule in SPACE_ASSETS.md. It stalls at 4 launches, so expect B (dense goods' trade capacity) to be wanted too; A fails a gate as it stands.

*Files: `src/world/trade.cpp`, `docs/economy/TRADE.md`, `docs/economy/SPACE_ASSETS.md`*

### NR-1204 — The BL-644 space-programme lumps are retired by data (authored at zero)
*decision taken on your behalf · raised 2026-11-10 · from BL-1630 lane report*

The lumps bought a whole launch lot off any shelf and consumed it with nothing launched; seed 0 1983-2000 bought 80 lumps and starved every launch. The real launch now buys the goods, so components_lump and propellant_lump are 0 in economy.lua and NATIONS.md says so. A staffed pad instead posts a standing bid (standing_lots = 1) on its home market, including the player's pad: a demand on the market, nothing bought or built for you. The state hash moves after 1980; no digest re-pinned.

**Why it matters.** Post-1960 pins will move at the next re-bless.

- Accept
- Limit the standing bid to rivals

> **Recommendation:** Accept.

*Files: `scripts/economy.lua`, `src/world/economy_system.cpp`, `docs/politics/NATIONS.md`*

### NR-1205 — Experiment lane went past its brief: changed route ranking, not sizing
*novel-work · raised 2026-11-10 · from NR-1203 experiment lane*

Briefed to make auto trade size off the silenced want, the lane found that already built (BL-1547) and instead ranked silenced-want routes first, behind a tunable. It is on its own branch, unmerged.

**Why it matters.** Recorded so the branch is not mistaken for a delivery.

- Keep the branch for reference
- Delete it

> **Recommendation:** Keep until NR-1203 is ruled.

*Files: `src/world/trade.cpp`*

### NR-1206 — Rival units at a moon base are not kept home: a scorer change, raised
*question · raised 2026-11-10 · from BL-1602 lane report*

corp_ai.cpp keeps the capital province's units at home, reading only the home capital. A rival's units at an off-world base would be sent to fronts. Treating a base province as a home to keep is a scorer change to land behaviour. The 2026-11-10 grant names space verbs; it is silent on land units at a base. Also taken by the lane: a cross-body front assignment is refused; the fallback war goal prefers the defender's base on that body; a ceded capital stays on its own body while land remains there (MILITARY.md § Space).

**Why it matters.** A rival base would be stripped of its garrison.

- Grant it: a base province is a home the scorer keeps
- Leave it for BL-1608 (rival far-space AI) to cover

> **Recommendation:** Grant it inside BL-1608; it is one line in the keep-home rule.

*Files: `src/world/corp_ai.cpp`*

### NR-1207 — Space firsts: five calls taken, and the player cannot see a first yet
*decision taken on your behalf · raised 2026-11-10 · from BL-1606 lane report*

(1) Ties go to the lowest nation id; firsts are judged by the day. (2) A flat 400 RP per first (economy.research.space_first_grant_rp), not scaled with the research price. (3) The convoy first counts on delivery to another body's market, not dispatch. (4) Crew in orbit includes crew aboard a station core, so a station core launch can take both firsts. (5) One public history entry per first, tagged with the taker. The player's dispatch shows only war entries, so no surface shows a first yet. Calls 1 and 2 are written into ERAS.md § Firsts. Save v91.

**Why it matters.** The grant size sets how much racing to space pays.

- Accept all
- Scale the grant with the research price
- Count the convoy at dispatch

> **Recommendation:** Accept; tune the 400 in playtest.

*Files: `src/world/space_firsts.cpp`, `docs/economy/ERAS.md`*

### NR-1208 — Fleet UI: nine calls taken, and seven new verify reads to grant
*decision taken on your behalf · raised 2026-11-10 · from BL-1609 / BL-1614 lane report*

Calls: (1) unit orders are not signal-delayed; (2) orders sit on the Selection card, not a right-click; (3) bombard targets whole provinces, listed where a building can be struck; (4) one fleet mark per (node, owner), a press selects the lowest-id lead, "Whole fleet" moves them all; (5) an asset's transit share is quarters gone plus day/90, a unit's from path days; (6) the card has two columns, destinations cheapest first; (7) the space designer expands rows in place; Recruit has no Space tab; (8) theatre_tabs became a template for four Design tabs; (9) a digest from a world with assets in transit may move (the origin is hashed). The space_command seam gained eight unit verbs (13-20) from a UI item. New verify reads, each reporting what the screen draws (screen positions included): verify.circum_transits, circum_fleets, solar_fleets, circum_fleet_hovered, circum_debris, fleet_card, space_designs; STAGING verify.space_fleet_fixture writes a Frigate design, four units and debris 25 (returns counts and the player id); circum_assets rows gain a storm flag. A solar storm cannot be staged (derived from the seed), so no capture shows the storm mark.

**Why it matters.** The verify reads touch the no-tile-data rule (io-standing-rules § Determinism & data model); each needs your grant like the ten before it.

- Grant the reads as an eleventh scoped exception
- Narrow them

> **Recommendation:** Grant: they report only what the canvas draws.

*Files: `src/core/verify_api.cpp`, `src/world/space_command.hpp`, `.claude/rules/io-standing-rules.md`*

### NR-1209 — Rival far-space AI: five calls taken; rivals design but raise nothing (the NR-1203 wall)
*decision taken on your behalf · raised 2026-11-10 · from BL-1608 lane report*

One far-space verb per rival per quarter, first wanted and legal of: design (best researched item per slot, redesign when a better one is researched), war (bombard from low orbit, move the fleet whole only when every unit there is armed, raise Frigates up to 2 per hostile, max 6), base (embark, load kit, move, found on the most-deposit prospected tile; max 1 base). Calls: (1) a base trip a full Lander cannot make with the 20-unit kit is refused (14.6 dv loaded vs 21.7 empty), so a heavy moon never gets a base; (2) once a rival holds a Hauler design the existing trade code ships its inter-body trade by Hauler, which moves the world from ~2020 (BL-1596's design); (3) Tugs and Cruisers get designs but are never raised; no shipyard or docking used; (4) the pick pays at the fair-price ceiling; (5) the pick's priority order is written only in nation_ai.hpp, not AI_OPPONENT.md. NR-1206 keep-home is wired behind far_space_keep_base_home = 0 (OFF). Spectate seed 0 to 2030: 9 designs held by three rivals by 2030, no unit raised (Teishe at war meets shelf_short for its Frigate), 1 launch all game.

**Why it matters.** The far-space AI exists and is correct in harness, but the world never lets it act: everything waits on NR-1203.

- Accept the calls
- Write the priority order into AI_OPPONENT.md

> **Recommendation:** Accept; rule NR-1203 first, then re-spectate (BL-1611).

*Files: `src/world/nation_ai.cpp`, `src/world/corp_ai.cpp`, `docs/ai/AI_OPPONENT.md`*

### NR-1210 — Propellant loop: the Electrolysis Plant keeps refined fuel as an input, against "water and power"
*decision taken on your behalf · raised 2026-11-10 · from BL-1604 lane report*

No new building types: the Ice Extractor is the extraction site on an ice deposit, the Electrolysis Plant the existing propellant_electrolysis route (water x3 + refined fuel x1). ERAS.md's ruling says water and power; the lane kept the recipe and wrote that the fuel half is shipped in or made locally. Also: the Ice Extractor key applies only off the builder's home body; the electrolysis key is read on the tile owner (so the propellant chain stays open to every nation at home); a Fuel depot over a base fills from the base shelf, else home; neither fill path checks blockade.

**Why it matters.** With refined fuel as an input a base still depends on home for half its propellant.

- Keep refined fuel (a base must import)
- Make it water and power only (a base is self-sufficient)

> **Recommendation:** Keep: it gives Haulers a reason to run; rule otherwise if bases should close the loop alone.

*Files: `scripts/recipes.lua`, `docs/economy/ERAS.md`*

### NR-1211 — Base and prospect UI: five calls taken, and four new verify reads to grant
*decision taken on your behalf · raised 2026-11-10 · from BL-1610 / BL-1633 lane report*

Calls: (1) the player picks the base site from a list, one row per province with mapped unclaimed land, at its most habitable tile, because a Lander lands at a surface node and no tile is behind it; (2) ground a dispatched probe is scanning keeps showing its deposits, and a fully surveyed body shows everything (DISCOVERY.md leaves a probed body to its probe); (3) the unprospected wash is 30% slate (palette::unprospected), subtle over grey regolith; (4) two deposit reads were left raw: the construction-suitability wash and the extraction candidates on the tile card, which could show bands on an unprospected base province; (5) Embark defaults to every free berth. BL-1633: Assign to front lists only fronts on the units' own body. New verify reads: verify.base_card() (the base orders as drawn, including each site row's label, grid col/row, chosen and prospected flags), verify.prospect_mask() (washed tile count, lens key goods, the title's mapped/prospected/total counts); STAGING verify.base_prospect_fixture() (grants the base_founding key, maps a moon, lands two player Landers with colonists and kit, idles an empty Lander in home low orbit; returns the moon name and three unit ids) and verify.stage_prospect(n) (prospects up to n regions by the real rule; returns the count); fleet_card().presses gains embark/confirm_embark/found_base/confirm_found.

**Why it matters.** The reads touch the no-tile-data rule; base_card returns site tile positions the card already prints. Call (4) is a small leak of deposit information on unprospected ground.

- Grant the reads; fix call (4) as a follow-up
- Narrow base_card to labels only

> **Recommendation:** Grant, since they report what the card draws; route the two raw reads through prospect_view.hpp.

*Files: `src/core/verify_api.cpp`, `src/ui/prospect_view.hpp`, `.claude/rules/io-standing-rules.md`*

### NR-1212 — Novelty: a base-site picker list, and a shared prospect-visibility test
*novel-work · raised 2026-11-10 · from BL-1610 lane report*

The design note expected a Lander "landed on a mapped tile"; a Lander lands at a surface node, so the lane built a site list the note did not anticipate. src/ui/prospect_view.hpp is a new shared piece every deposit-drawing surface now asks. Scope stayed small.

**Why it matters.** Novelty should be chosen, not accreted (standing rules).

- Accept the list as the base-site idiom
- Replace it with a press on the Planetary canvas

> **Recommendation:** Accept for v0.5.0; revisit in the v0.6.0 polish pass if it reads poorly in play.

*Files: `src/ui/circum_units.cpp`, `docs/ui/SELECTION.md`*

### NR-1213 — Garrison chase fix moves the campaign march; two harness re-aims taken
*decision taken on your behalf · raised 2026-11-10 · from BL-1634 / BL-1632 lane reports*

BL-1634 (garrison chase loop): units on a defend order now step last in the daily march, and re-aim at where the day's steps left the hostile stacks (economy_system.cpp run_unit_march; MILITARY.md § Defend province). Chosen over a swap/cross rule because the chase never swaps or crosses tiles. Side effect: a stack entering a province draws its defenders the same day, not the next. Any world where a defender steps beside other units moves, so play-path digests and sweeps may move; none of the harnesses run pins one. BL-1632 (stale tail harnesses): one helper, world_save_tail_bytes_after, replaces the hand sums; a static_assert breaks the build when the save version passes 92 until the new section joins its table. Calls: war_harness RF1 asserted ver == 76 and now asserts ver == world_save_version (as R4j and R9b do); tile_contact T13e and quarterly_return R5 passed only by corrupting the wrong section and are re-aimed at the bytes they name.

**Why it matters.** The march change is a world-mover folded into the pending digest re-pin (NR-1167).

- Accept the step rule
- Prefer a same-tile meeting rule instead

> **Recommendation:** Accept: one sentence, deterministic, reuses the existing targeting.

*Files: `src/world/economy_system.cpp`, `docs/military/MILITARY.md`, `src/world/world_save.cpp`*

### NR-1214 — Research pacing has drifted out of its pin (first research comes sooner and less evenly): re-pin, or treat it as a fold defect
*question · raised 2026-11-10 · from BL-1635 lane report*

nation_tech_harness R0h pins median quarters-to-first-research in [16,24]; R0i pins p10 >= 8. Pooled over the 16 library seeds: 10-15 db77bdde 20/11; 10-23 17/9; 10-24 17/7 (R0i first fails, the nations-own-everything wave); 10-25 15/5 (R0h first fails); 11-01 19/6; 11-03 15/5; HEAD 15/6. The largest single step on a two-seed probe (46/28) is f1d05dfc BL-1402 (fold to ground): 15/15 to 12/12, then small drops at the road-join, site-days and port-links commits. The seed re-curation is not the cause. Not re-pinned. Also from the same lane: well_gate shields its fixture from the rival dial with ai_cooldown = 8 (seating the player conflicts with the harness's seat move); railway R7/D now compares against a twin world with the track erased, since a nation may briefly run a negative treasury (BL-1482).

**Why it matters.** Research is reached faster and more unevenly than the pacing band intends; either the band or the fold is wrong.

- Re-pin the band to the post-fold reading (median 15, p10 6)
- Treat it as a fold defect and file the work

> **Recommendation:** Your call: the drift is the cost of nations owning their ground, and it is not obviously wrong.

*Files: `tools/verify/nation_tech_harness.cpp`, `src/world/state_industry.cpp`*

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

