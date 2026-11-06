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

*28 entries — 26 open, 2 resolved.*

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

### NR-1141 — Naval design details I filled in behind your form
*decision taken on your behalf · raised 2026-11-05 · from SEA_THEATRE (BL-1430), MILITARY.md § Sea*

Your form settled the shape; I wrote these to make it whole. (1) A blockade closes the port for everyone, neutrals included, with no Alarm cost to the blockader. (2) Prize cargo goes to the raider's nearest own port in range, or is sunk. (3) An escort covers its own nation's cargo, not a friend's. (4) Submarines count for nothing in sea control. (5) Mission reach is a per-class range over water from any own port. (6) A landing is refused on a neutral's coast. (7) Ship hulls are one new good, made at the Arms Works. (8) Sea fights keep no battle record: a damaged ship breaks off home. (9) A ship's attack never reaches the shore, since you did not pick bombardment.

**Why it matters.** Items 1, 3 and 6 decide how third parties feel a sea war; item 8 means sea fights read differently from land battles.

### NR-1144 — Seven sea-day details taken inside the sea theatre core
*decision taken on your behalf · raised 2026-11-06 · from SEA_THEATRE (BL-1430)*

Taken so the core could land; each is one line to change. (1) A ship working a zone spreads its presence 1/N over the zone's provinces, as a wing's day does. (2) A ship fires when its side found a target even with no mission; a broken-off ship does not fire. (3) Refit at the home port costs goods only, no recruits. (4) A hire spends sea recruits and refunds them only if its port is lost before it stands. (5) A ship working a zone that holds its own mouth does not mend there; only a ship broken off or recalled does. (6) A ship that loses its home re-homes to the nearest own port BY WATER, and disbands if it can reach none, even with a port on another sea. (7) A blockade order is legal on any in-range water; its target and effect arrive with BL-1530 (sea control and blockade).

**Why it matters.** (1) makes a larger zone thinner, which is the trade-off the zone choice is meant to carry. (2) means a ship in transit can start a fight.

### NR-1145 — Ship refit: four calls taken inside the ship kit
*decision taken on your behalf · raised 2026-11-06 · from SHIP_KIT (BL-1527)*

Taken so the item could land; each is small to change. (1) A refitting ship is in dock: it neither fights nor can be hit. A land refit fights on its old kit, so this differs, and a ship can dodge fire by refitting, at the price of goods and its muster days. The alternative is hittable in dock. (2) In dock it still draws quarterly supply and does not mend. (3) A broken-off ship must be recalled to no mission before it can refit, as MILITARY.md says. (4) Base kit draws nothing beyond the hire goods, mirroring airframes, so ship hulls have no demand until the naval branch (BL-1528) opens later kit.

**Why it matters.** Call (1) sets how a blockaded fleet can behave once BL-1530 (sea control) counts presence: a docked ship would not count, and could not be sunk at its mouth.

### NR-1146 — Raid: calls taken inside BL-1529 (patrol escort raid)
*decision taken on your behalf · raised 2026-11-06 · from SHIP_KIT (BL-1527)*

Taken so the item could land; each is small to change. (1) A refitting ship is in dock: it neither fights nor can be hit. A land refit fights on its old kit, so this differs, and a ship can dodge fire by refitting, at the price of goods and its muster days. The alternative is hittable in dock. (2) In dock it still draws quarterly supply and does not mend. (3) A broken-off ship must be recalled to no mission before it can refit, as MILITARY.md says. (4) Base kit draws nothing beyond the hire goods, mirroring airframes, so ship hulls have no demand until the naval branch (BL-1528) opens later kit.

**Why it matters.** Call (1) sets how a blockaded fleet can behave once BL-1530 (sea control) counts presence: a docked ship would not count, and could not be sunk at its mouth.

---

## Resolved

Kept, not pruned: the reasoning is the point. Prune only in a deliberate sweep, once the
answer has landed in an authority doc.

### NR-1143 — Research spends outside net: should it count, and how does it share with construction?
*question · raised 2026-11-05 · from BUILD_PAYBACK_CENSUS (BL-1496)*

The research line takes a weighted share of the treasury, up to what the queue owes, as a direct expenditure the quarterly return does not carry. Mean 1,644 credits a major-quarter over 20 years, against net of 1,000-2,500. So the header NET and the Earnings chart overstate free cash, and "a mine costs six months of net" means different things before and after research. Recommended: book research in the return, so NET is the money left; the mine target then reads net after research.

**Why it matters.** A player with a research queue may have little left for construction under the new prices, and a share of treasury shrinks as the opening treasury falls. This sets how research and construction compete at the opening.

> **RESOLVED.** Ben: "Yes, count research." Research counts inside net; written into FINANCE § The quarterly return; the work is BL-1539 (research in return).

### NR-1142 — Does offshore drilling join sprint 64?
*question · raised 2026-11-05 · from OFFSHORE_DRILLING (BL-1522)*

You ruled offshore drilling lands with the naval system, and it is parked on that. It is a separate build: new sea petroleum moves world generation, TILES.md must change, and shelf-only against open ocean is unruled. I left it out of sprint 64's plan.

**Why it matters.** Adding it enlarges an already five-difficulty sprint and adds a second world-mover to its re-bless.

> **RESOLVED.** Ben, 2026-11-05: keep it out of sprint 64. BL-1522 (offshore drilling) stays parked.

