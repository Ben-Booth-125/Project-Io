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

*49 entries — 16 open, 33 resolved.*

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

### NR-1129 — Grievance never holds: no pair's Trust reaches -25
*question · raised 2026-11-03 · from BL-1479 (grievance and Alarm floor), war_census 16 seeds*

Grievance now holds at Trust -25 as ruled. Over 16 seeds the lowest Trust any pair reached was about -11.8, so grievance held on 0 of 652,094 live pair-ticks and carried no declaration.

**Why it matters.** A declared casus belli that nothing meets is a dead rule. The conduct weights and the nine-tick decay cap Trust well above the edge.

> **Recommendation:** Either raise the conduct weights (or slow the decay) so a sustained offence reaches -25, or move the edge toward the observed floor (about -10). Measure with war_census either way.

*Files: `src/world/war.hpp`, `src/world/sentiment.cpp`, `docs/politics/RELATIONS.md`*

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

---

## Resolved

Kept, not pruned: the reasoning is the point. Prune only in a deliberate sweep, once the
answer has landed in an authority doc.

### NR-1084 — Four combat calls taken in writing the land model
*decision taken on your behalf · raised 2026-10-26 · from sprint 58 military design*

Writing Ben's land rulings into MILITARY.md § The campaign force model took four calls he did not make: (1) defence and breakthrough are separate stats (defending vs attacking); (2) no piercing-versus-armour check, hardness alone; (3) readiness takes the larger share of landed damage, so most fights end in retreat; (4) the seven classes - motorised infantry replaces trucks, one main battle tank line replaces light/medium/heavy, light armour is low-hardness reconnaissance, and air defence is added.

**Why it matters.** Each shapes BL-1421 (soft hard resolver) and BL-1422 (land classes and kit) before they are built.

> **Recommendation:** Keep all four; the realism case is in research/MILITARY_1960_PRESENT.md § 4 and § 10.

> **RESOLVED.** Ben, 2026-11-03 (sprint 61 calls form): ratified as taken.

*Files: `docs/military/MILITARY.md`, `docs/research/MILITARY_1960_PRESENT.md`*

### NR-1085 — Five calls taken in retiring the Era catastrophe
*decision taken on your behalf · raised 2026-10-26 · from era catastrophe retirement design pass*

Writing Ben's retirement (ERAS.md § The point of an Era) took five calls he did not make: (1) each nation crosses into the next Era on its own when it meets the gate, not the world at once; (2) Alarm is a third sentiment dimension beside Access and Trust (RELATIONS.md § 6), directed per pair, decaying, under the informs-never-declares invariant; (3) three Digitisation nodes that read the retired Ceiling were rewritten in the store: Inspection Regimes 'Alarm raised by your visible capability -10%', Arms Control Verification's second effect 'every nation's Alarm toward every other decays 10% faster', Quiet Borders' deed 'no nation's Alarm toward you rises for 8 quarters'; Planetary Accords keys 'shared limits on climate Load'; (4) the tempo trap retired with the countdown; (5) climate is Era 2's continuous pressure with no test.

**Why it matters.** Each shapes what a nation chases and what Alarm costs it before any of it is built.

> **Recommendation:** Keep all five. (1) makes the race to orbit the success Ben asked for; (2) reuses a tested substrate rather than adding a new store.

> **RESOLVED.** Ben, 2026-11-03 (sprint 61 calls form): ratified as taken.

*Files: `docs/economy/ERAS.md`, `docs/politics/RELATIONS.md`, `docs/economy/tech/digitisation_tree.json`, `docs/CLIMATE.md`*

### NR-1087 — Six calls taken building the land combat core
*decision taken on your behalf · raised 2026-10-27 · from BL-1421 (soft hard resolver)*

(1) HP is the unit's count of men against a stored establishment; equipment joins HP with BL-1423 (refit and retreat). (2) Terrain DIVIDES the attacker's hits (a mountain halves them) rather than raising the defender's absorb: an absorb raise mattered only where hits already exceeded defence, so infantry-on-infantry ignored mountains. MILITARY.md says so now. (3) The calibration, measured by campaign_battle_harness C4 (infantry on plains, 200 seeds): breaking a defender takes about 2:1 - decisive 0% at 1:1, 15% at 1.5:1, 64% at 1.75:1, 89% at 2:1, 99% from 2.5:1; fights last about two ticks. The old curve was 50% at 1:1 because the old round had no attacker penalty. (4) A side of spent units is overrun: it deals nothing, the enemy's hits land on it in full, and it is broken. (5) A retreat moves one tile: land, never onto an enemy of the fight, preferring outside the province then own ground, ties to the lowest tile id; a unit with nowhere to go stays put, out of the fight. (6) Morale: +50 to a battle's winners, -100 to its losers and to every unit that retreats; out of contact readiness regains 100 a tick scaled by supply, never above morale, and morale moves 10 a tick toward supply. Found by the cold review and left for you: (7) a static nation garrison that retreats has no order to return, so it stays off its post; (8) the rival scorer's unit_strength still reads men x quality x supply and ignores class, readiness and morale, so the AI's picture of strength has drifted from the resolver (BL-1427, fronts, is where the AI reads force); (9) a v57 save loads its units with no establishment, so a unit wounded before the load reads at full HP.

**Why it matters.** Each sets how war feels before fronts and the AI are built on it. And the re-bless: player_seed_sweep digests fail 16/16 with the v58 stream, owed to the sprint's re-bless.

> **Recommendation:** Keep all six; (3) is the one to feel in play.

> **RESOLVED.** Ben, 2026-11-03 (sprint 61 calls form): ratified as taken.

*Files: `src/world/campaign_battle.cpp`, `src/world/battle_system.cpp`, `src/world/unit_roster.cpp`, `docs/military/MILITARY.md`*

### NR-1089 — Refit: the calls taken on your behalf
*decision · raised 2026-10-27 · from sprint 58 refit lane (Ben, 2026-10-27: "add refitting a unit's type from selection to a newer compatible model")*

Compatible means the same land class and a newer row: a later band, or the same band with a higher power. Credits are the hire price difference (hire_cost_per_power x the power gap, floored at 0) plus the target row's goods in full. It takes the target's muster days. The unit must stand on its own nation's ground, out of battle, and cannot march while refitting. Men, readiness and morale are kept. A refit clears the unit's standing march order. Rival nations never refit.

**Why it matters.** Each is a balance or feel call. Charging the full goods makes kit upgrades cost as much as new kit; clearing the march order is a choice the brief did not make.

- A: keep all as built
- B: change one (say which)

> **Recommendation:** A. Revisit the price once kit and designs (BL-1434, BL-1436) give refits more to buy.

> **RESOLVED.** Ben, 2026-11-03 (sprint 61 calls form): ratified as taken.

*Files: `docs/military/MILITARY.md`, `src/world/corp_command.cpp`*

### NR-1093 — Capital base: the calls taken on your behalf
*decision · raised 2026-10-27 · from BL-1439 (capital military base) lane*

The base goes on the legal own tile nearest the capital garrison, empty before stacked. 592 of 593 nations on the 16 library seeds get one; seed 25's Lainka owns one tile already full and gets none. The opening unit now musters at the capital base, so a nation has one base. A seat moved at Begin keeps its base and garrisons and loses only its opening unit. A garrison is told from a hired unit by entity-id order (no saved flag). The capital garrison records the base, so demolishing the base disbands it (the orphan rule, which never ran until today's fix).

**Why it matters.** The garrison-dies-with-its-base rule is new in effect; the id-order test is an implicit invariant.

- A: keep all
- B: garrisons are never orphaned by their base
- C: add a saved garrison flag instead of id order

> **Recommendation:** A, with C if garrisons ever get hired-in replacements.

> **RESOLVED.** Ben, 2026-11-03 (sprint 61 calls form): ratified as taken.

*Files: `docs/military/MILITARY.md`, `src/world/corporation_generation.cpp`, `src/world/economy_system.cpp`*

### NR-1095 — A PNG decoder in src/ui/png_decode.cpp
*novel-work · raised 2026-10-28 · from BL-1446 (unit designer) lane*

The designer draws a class image, and the app links no image library: SDL 3.2 loads BMP only and ImGui bundles no stb_image. The lane wrote an in-tree decoder (~340 lines): a full zlib inflate plus 8-bit grey, grey+alpha, RGB, RGBA and palette PNGs, non-interlaced; anything else is refused with a reason.

**Why it matters.** No doc owns image loading. Every later authored image (unit art, emblems, icons as files) will reach for this, and an in-tree decoder is code to maintain against untrusted-looking input.

- A: keep it, and name it in TECH_FOUNDATIONS.md as the one image path
- B: replace it with a vendored stb_image
- C: keep it designer-only until real art lands

> **Recommendation:** A. It is small, pure and refuses what it cannot read; a vendored library is a bigger surface for a handful of files.

> **RESOLVED.** Ben, 2026-10-28: A - keep it as the one image path. Written into TECH_FOUNDATIONS.md § Framework - SDL3. The output cap is BL-1447 (kit goods priced)'s tail.

*Files: `src/ui/png_decode.cpp`, `src/ui/png_decode.hpp`, `docs/tech/TECH_FOUNDATIONS.md`*

### NR-1096 — Designer: a pick saves at once; the fitted item is disabled rather than a toggle
*question · raised 2026-10-28 · from BL-1446 (unit designer) lane*

Two UI calls the lane made. (1) Each pick in a slot list sends set_unit_design at once: there is no draft, confirm or revert, so a misclick changes the live design for every new hire. (2) The fitted item in a slot list is disabled ("Fitted now"), not a toggle: pressing it again does not return the slot to base kit.

**Why it matters.** The toggle rule says a control whose active state is visible undoes on a second press. The fitted badge is a visible active state, so (2) reads as an exception the rule does not list. (1) follows military.md § The designer ("Saving is the pick") but has no undo.

- A: keep both as built
- B: make the fitted item a toggle back to base kit
- C: add a draft with a Save button
- D: B and C

> **Recommendation:** A. The slot is a selector of one-of-n, like the exempt combos; base kit is one press away in the same list.

> **RESOLVED.** Ben, 2026-10-28: C - a pick drafts and Save commits; the fitted item stays disabled. Written into docs/ui/ledgers/military.md § The designer; the work is BL-1448 (designer draft and Save).

*Files: `src/ui/unit_designer.cpp`, `docs/ui/ledgers/military.md`*

### NR-1097 — Land battle form: three readings taken in writing the carried-supplies and recon rulings
*decision · raised 2026-10-29 · from main session (Ben land battle form 2026-10-29)*

(1) The quarterly supply share arrives as a DAILY resupply of share x daily need, not one quarterly top-up - a quarterly refill dips below full the day after, so the flat hunger penalty would apply almost always. (2) The round reads the three carried stocks; supply_factor_permille no longer scales hits directly, it only sets the resupply rate. (3) Air superiority adds recon in proportion to its share rather than making revealed units fully visible, so superiority alone does not settle every battle. Also: the stat grid replaces the unit card's Strength and Roster pages.

**Why it matters.** Each changes how strong a supplied, a half-supplied and an air-covered army is. Written into MILITARY.md § Carried supplies, § Recon and § Air, ledgers/military.md § The stat grid, SELECTION.md. Work: BL-1452 (carried supplies), BL-1453 (land recon), BL-1454 (unit stat grid).

> **RESOLVED.** Ben, 2026-11-03 (sprint 61 calls form): ratified as taken.

### NR-1098 — Terrain and the classes: four readings the build took
*decision · raised 2026-10-29 · from BL-1455 (terrain class modifiers) lane, cold review 2026-10-29*

(a) Wear is x1.5 equipment drain - the ruling named the cells but no rate. (b) "Blocked unless roaded" means any road edge on the tile, so armour may enter a mountain tile from a roadless side if some road touches it. (c) The move rows change a march's days but not its route: the path stays the shared terrain-weighted one, so medium armour may be routed through forest at x1.5 when a faster route for it exists. (d) Pre-modern rows (no land class) read none of the rows, not even the open-ground stealth x0.75 the doc gives every unit. Also: carriers dismount even on a mountain road, as the doc reads.

**Why it matters.** (b) and (c) decide whether terrain shapes WHERE armour goes or only how slowly; (c) in particular lets an armoured column walk into a forest debuff it could have avoided. Written into MILITARY.md § Terrain and the classes. Work: BL-1455 (terrain class modifiers).

> **RESOLVED.** Ben, 2026-11-03 (sprint 61 calls form): ratified as taken.

### NR-1099 — Carried supplies: a hub on every capital, and one purse gate caps surveys too
*decision · raised 2026-10-30 · from BL-1452 (carried supplies) lane, cold-review round 2026-10-30*

(a) The free source is the lowest-id running inland logistics hub on the capital. Seeds 10, 11, 12, 25 and 31 had a railway station or port standing as the capital's source and no hub, so nothing ran free and the opening cut removed their capitals' buildings (dark at the handoff). The seeding's step 0 now stands a hub on EVERY capital, beside any station or port already there. (b) purse_room_of now carries the settle cap for every spend - builds, retools, hires, refits, roads and survey dispatch alike; surveys in the settle were not separately ruled.

**Why it matters.** (a) changes world shape: every capital opens with one more building (the shape probe was not re-run). The alternative is to let a station or port on the capital be the free source instead. (b) a poor nation may send fewer surveys in the settle. Merged at b0a9e9b5, G1t50 full 84.6. Work: BL-1452 (carried supplies).

> **RESOLVED.** Ben, 2026-11-03 (sprint 61 calls form): ratified as taken.

### NR-1100 — Does the stance grant cover a rival answering, offering and accepting peace, and so ceding ground?
*question · raised 2026-10-30 · from Sprint 61 design (diplomacy and war), 2026-10-30*

Ben ruled the 2026-08-18 stance grant covers a rival DECLARING war. A whole war also needs the rival to answer a declaration (naming goals), offer peace, accept or refuse one, and impose at 100. Accepting a cession is a rival choosing to give up ground, which is a new subject (territory), not a stance. Recommended: yes, same terms (pure, seeded, deterministic, legal verbs, never a planner), recorded as a dated entry in AI_OPPONENT.md § 11. Alternative: rivals answer and accept white peace only; cession happens only when imposed at 100.

**Why it matters.** Without it, a war against a rival can only end by imposition, and a rival never ends a war it started. Blocks the peace half of BL-1461 (rivals declare war). RELATIONS.md § 1 and § 7 cite this entry.

> **RESOLVED.** Ben, 2026-10-30: yes - full peace including cession, on the same grant terms, dated in AI_OPPONENT.md § 11 ("Rival nations wage war whole"). Written into RELATIONS.md § 1 and § 7.

### NR-1101 — The war strawman: goals, score weights, imposing at 100, truce, the free answer
*question · raised 2026-10-30 · from Sprint 61 design (diplomacy and war), 2026-10-30*

RELATIONS.md § 7 sets shapes I proposed, not ones you ruled: (a) 1-3 goals, each a defender province bordering the declarer or coastal, never a capital; (b) score = occupation + battles, strawman 60/40; (c) a side at 100 may impose; (d) a truce after peace, strawman 8 ticks (two years); (e) the defender's answer is free and names its own goals; (f) a war ends only in peace (return_to_neutral refused between parties); (g) a justification cuts the third-party price, strawman by half. Numbers inside these shapes are delegated calibration; the shapes are not.

**Why it matters.** Each shape is a rule a player learns. Overturning one after BL-1459 and BL-1428 build is a save-format change. Answer before those lanes start; numbers can follow.

> **RESOLVED.** Ben, 2026-10-30: (a) any of the defender's provinces may be a goal, capitals included, no reach rule; (b) occupation and battles carry half each; (c) a side at 100 imposes; (d) truce sixteen ticks (four years); (e) the answer is free and names its own goals; (f) only peace ends a war; (g) a justification halves the price. The goal count was not addressed; one to three stands. Written into RELATIONS.md § 7 and ledgers/diplomacy.md. Allowing capitals raised NR-1102.

### NR-1102 — A ceded capital, and a nation ceded away
*question · raised 2026-10-30 · from NR-1101 ruling (capitals may be goals), 2026-10-30*

With capitals open as goals, two cases now exist that nothing owned. (a) A nation that cedes its capital province takes as its new capital the remaining province with the largest population centre, ties to the lower id. The capital is where the treasury sits and where the free logistics hub stands (NR-1099), so the hub must move too. (b) Three goals can take every province of a small nation. Strawman: it leaves the map, its wars close, and its sentiment rows are erased. The alternative is a floor: the last province can never be a goal or ceded.

**Why it matters.** (b) is elimination, and nothing else in the campaign removes a nation, so every consumer of the nation list meets it for the first time (fronts, markets, the scorer, saves, spectating). Both rules are strawman in RELATIONS.md § 7. Work: BL-1460 (cession moves territory).

- A: both strawman rules as written
- B: the capital moves as written; a nation's last province can never be ceded
- C: a capital can never be ceded (a goal, but not cedable)

> **Recommendation:** B. Elimination is a whole subject. The floor keeps every nation on the map until it is designed on purpose.

> **RESOLVED.** Ben, 2026-10-30: B - the capital moves to the largest remaining population centre (with its free hub); a nation's last province can never be ceded, so no nation leaves the map by war. Written into RELATIONS.md § 7 and ledgers/diplomacy.md.

### NR-1103 — An answered war can never reach a score of 100
*question · raised 2026-10-31 · from Lane W report, sprint 61 (BL-1459 war record, BL-1428 war goals and peace), 2026-10-31*

The occupation half divides held goals by ALL goals named. Once the defender answers with its own goals, neither side can reach +-100: holding every enemy goal while losing none reads 83 in the harness (P1e), and the defender tops out lower still (P1f: -67). So the defender can never impose, and the declarer can impose only while the war is unanswered. A cold review (2026-10-31) adds: the defender's score is bounded above -100 in EVERY war, since the declarer always names a goal, so the defender can never impose at all; and the battles half needs the imposer to lose almost nothing.

**Why it matters.** Impose at 100 (NR-1101 C) silently becomes declarer-only, and answering a war makes it unwinnable by force. Work: BL-1428 (war goals and peace).

- A: normalise each side by its own goals: (my held share of your goals) less (your held share of mine), so holding all of theirs and losing none is 100
- B: keep as built; imposing is only for an unanswered war
- C: keep the formula, lower the impose threshold

> **Recommendation:** A. It keeps half-occupation, half-battles and makes 100 reachable for either side.

> **RESOLVED.** Ben, 2026-10-31: A - each side is normalised by its own goals; holding every goal you named and losing none of your own is the full occupation half, for either side. Written into RELATIONS.md § 7 War score; code in BL-1428 (war goals and peace).

*Files: `src/world/war.cpp`, `docs/politics/RELATIONS.md`*

### NR-1104 — Grievance can almost never hold
*question · raised 2026-10-31 · from Lane W report, sprint 61 (BL-1459 war record, BL-1428 war goals and peace), 2026-10-31*

The grievance justification reads the adverse band, Trust <= -50 toward the target. With the nine-tick decay and the authored weights, conduct between two nations rarely pushes Trust that far, so grievance will almost never halve a price. The other three (embargo, lost ground, a friend attacked) hold as designed.

**Why it matters.** A justification that never holds is a rule nobody meets. The threshold is RELATIONS § 7 design, not calibration inside a ruled range.

- A: grievance holds at the hostile-leaning band (Trust <= -25)
- B: keep -50 and let war price grievance only after long conduct
- C: grievance counts the seeded grudge too, so an inherited war justifies the next one

> **Recommendation:** A, measured once rivals declare (BL-1461 rivals declare war) so the census shows how often it holds.

> **RESOLVED.** Ben, 2026-11-03 (sprint 61 calls form): A - grievance holds at the hostile-leaning band, Trust <= -25. RELATIONS.md carries it.

*Files: `src/world/war.cpp`*

### NR-1105 — Is hostility without a war retired?
*question · raised 2026-10-31 · from Lane W report, sprint 61 (BL-1459 war record, BL-1428 war goals and peace), 2026-10-31*

declare_war now writes the hostility row, and a bare declare_hostile between war parties is refused, so the defender must answer instead. Between nations NOT at war, declare_hostile still writes a hostility row with no war record: no goals, no score, no peace, and return_to_neutral ends it.

**Why it matters.** Two ways to be hostile with different exits is the opposite of legible. RELATIONS § 1 predates § 7 here.

- A: retire bare hostility; declare_hostile becomes declare_war (the right-click path already is)
- B: keep both; hostility is a cold stance, war is the hot one

> **Recommendation:** A. Every hostility then has goals, a score and a peace.

> **RESOLVED.** Ben, 2026-11-03 (sprint 61 calls form): B - keep both: hostility is the cold stance, war the hot one. declare_hostile stays between nations not at war.

*Files: `src/world/stance.cpp`, `docs/politics/RELATIONS.md`*

### NR-1106 — Four war calls the lane took
*decision taken on your behalf · raised 2026-10-31 · from Lane W report, sprint 61 (BL-1459 war record, BL-1428 war goals and peace), 2026-10-31*

(1) A province belongs, for goal validity and cession, to the plurality of tile_to_nation over its tiles, ties to the lower id; province_holder is only the military holder. (2) A right-click attack on a stack standing off the defender's ground takes the defender's capital province as the first goal. (3) Justifications are pair-measure rows in war.cpp in the condition grammar (measure, comparator, operand), not condition_subject entries, which measure one actor; promoting them with a counterparty qualifier would change the save layout. (4) Exhaustion's field days are counted once a quarter from where units stand then. Cold review (2026-10-31): (4) deviates from RELATIONS § 7 ("each day in the field") and is being fixed to a daily accumulation, so it drops out of this list.

**Why it matters.** (2) means a declaration can name a capital the player never pointed at; (3) leaves a second predicate grammar beside META_LAYER.md.

> **Recommendation:** Accept all four; promote (3) into META_LAYER only if another pair rule needs it.

> **RESOLVED.** Ben, 2026-11-03 (sprint 61 calls form): ratified as taken.

*Files: `src/world/war.cpp`, `src/core/app.cpp`*

### NR-1108 — Trade-flow room: nothing computes "no room" today
*question · raised 2026-10-31 · from Lane U report, sprint 61 (UI carries), 2026-10-31, BL-1414 (trade-flow room)*

BL-1414 asks the Trade-flow lens to compute a lane's room without rivals' cargo, so a no-room marker never reveals a rival's shipments. But the dispatcher (src/world/trade.cpp) only ever records trade_refusal::sent, so the lens never draws a refusal marker and there is no room figure to change. The item was skipped.

**Why it matters.** Building a room figure is new lens-side economics, not polish. The brief and the row also disagreed on which way rivals' cargo counts.

- A: bring the refusal classes back in the dispatcher, room computed without rivals' cargo (private stays private)
- B: cancel BL-1414; the lens shows flows only
- C: park it until trade capacity is next touched

> **Recommendation:** B unless a refusal marker answers a question the player is asking now; the lens question is "where do my goods go".

> **RESOLVED.** Ben, 2026-11-03 (sprint 61 calls form): C - park BL-1414 (trade-flow room) until trade capacity is next touched.

*Files: `src/world/trade.cpp`, `docs/economy/TRADE.md`*

### NR-1109 — The "always-on player outline" draws only under a lens
*question · raised 2026-10-31 · from Lane U report, sprint 61 (UI carries), 2026-10-31, BL-1416 (identity wash)*

The 2026-10-26 ruling says "keep the always-on player outline". Since the 2026-10-24 ruling the outline draws only under a lens, and PLANETARY.md and LENSES.md both say the plain canvas has no outline. The lane removed the stale wash wording and did not bring the outline back.

**Why it matters.** Two dated rulings disagree; the code and two docs follow the earlier one.

- A: the plain canvas has no outline (as built and documented)
- B: the player outline is always on, plain canvas included

> **Recommendation:** A: read "always-on" as "under every lens", and amend the 10-26 wording.

> **RESOLVED.** Ben, 2026-11-03 (sprint 61 calls form): A - the player outline draws under every lens and not on the plain canvas; the 2026-10-26 'always-on' reads 'under every lens'.

*Files: `src/ui/body_surface_canvas.cpp`, `docs/ui/canvases/PLANETARY.md`*

### NR-1110 — Five UI calls the lane took
*decision taken on your behalf · raised 2026-10-31 · from Lane U report, sprint 61 (UI carries), 2026-10-31*

(1) BL-1417: with the emblem gone, the profile card marks the nation by a thin rule in its colour beside the name. (2) BL-1454: new colours palette::unit_food, unit_ammo, unit_equipment. (3) A rival unit's design stats count as "seen" only while it is in a battle against the player or air-revealed to the player; nothing remembers a past sighting, since that needs a save field. (4) New verify reads verify.unit_stat_grid(id) and verify.unit_designer().grid return only the cells the screen draws (unit stats, no tile data). (5) The unit card's Strength/Roster pager is gone; verify.unit_card_page is kept as a no-op. Cold review (2026-10-31): air_revealed is rebuilt every air pass, so (3) blanks a rival's stats the day after any battle; ledgers/military.md § The stat grid says "only once seen", which reads as remembered.

**Why it matters.** (3) means a rival's stats blank again the day a battle ends, which may read as forgetting. The rest are look-and-feel; each is in the live click list.

> **Recommendation:** Accept (1), (2), (4), (5). For (3), a saved per-nation set of seen rival designs matches the doc's "once seen"; it is a save field, so it would ride the next save bump (the war or embargo lane).

> **RESOLVED.** Ben, 2026-11-03 (sprint 61 calls form): ratified as taken.

*Files: `src/ui/unit_stat_grid.cpp`, `src/ui/profile_panel.cpp`*

### NR-1111 — A harbour group's far ends get their own harbour
*decision taken on your behalf · raised 2026-10-31 · from Lane G report, sprint 61 (BL-1420 lane and port defects), 2026-10-31*

The ruling that colonial ends within two tiles share one harbour port and LOGISTICS.md § 4b's chaining of harbour groups could not both hold: a chain carried an end three or four steps from its port (seed 13). The lane kept the two-tile reach: harbour land is now chosen so its sea mouths lie within two steps of every end in the group, and ends still beyond two steps get a harbour of their own. Written into § 4b as a delegated reading.

**Why it matters.** More, smaller harbours on chained coasts (seed 10 gained two ports). The alternative accepts longer dead spurs and loosens the check.

> **Recommendation:** Accept.

> **RESOLVED.** Ben, 2026-11-03 (sprint 61 calls form): ratified as taken.

*Files: `src/world/road_generation.cpp`, `docs/economy/LOGISTICS.md`*

### NR-1114 — Which pair trade counts for, and when trade is severed
*decision taken on your behalf · raised 2026-10-31 · from Lane S report, sprint 61 (BL-1458 sentiment emitters, BL-1432 Alarm as sentiment), 2026-10-31*

(1) trade_conducted counts the cargo owner's nation and the destination market's nation (the import-tariff foreign-goods rule), foreign landings only, one event each way per tick. (2) trade_severed is raised by the receiver toward the shipper (+2 Alarm, once) when a pair that traded at the last fold lands nothing now; a pair whose trade flickers raises repeated spikes. (3) embargo_imposed has no emitter yet: no embargo law is enacted anywhere; BL-1418 (embargo bars goods) is where it gets one. (4) BL-1432's rival scorer reading Alarm and its player surface are left to BL-1461 (rivals declare war) and BL-1462 (diplomacy ledger).

**Why it matters.** (2) may want a streak (several ticks of trade, then several of none) before it counts; flicker is common on thin lanes.

> **Recommendation:** Accept (1), (3), (4); for (2), require two ticks traded and two ticks silent - in-band, delegated unless you object. Cold review agreed the flicker is real (alternating landings net about +7 Alarm, worse than not trading); the two-and-two window is being built.

> **RESOLVED.** Ben, 2026-11-03 (sprint 61 calls form): ratified as taken.

*Files: `src/world/sentiment_emitters.cpp`*

### NR-1115 — Can Alarm go below zero?
*question · raised 2026-10-31 · from Cold review of Lane S, sprint 61 (BL-1432 Alarm as sentiment), 2026-10-31*

Relief - live trade, an accepted friendship, a peace - lowers Alarm below zero, and the floor is the substrate's -100. A pair landing 500 credits a tick sits near -13.5, so a later war declaration (+3) or a modest border force still reads as reassured on balance. RELATIONS § 6 says positive is alarmed and lists relief as decay toward neutral and live trade; it does not say relief can bank a reserve.

**Why it matters.** A banked reserve means trade buys immunity to Alarm; a floor at 0 means relief only ever cancels Alarm already raised. The rival war scorer (BL-1461 rivals declare war) will read this number.

- A: Alarm's floor is 0 - relief cancels raised Alarm and never goes past neutral (Alarm is a one-sided dimension)
- B: Alarm is signed like Access and Trust; negative is reassured, and a reserve is intended

> **Recommendation:** A. Reassurance is already what Trust measures; a signed Alarm double-counts it.

> **RESOLVED.** Ben, 2026-11-03 (sprint 61 calls form): A - Alarm's floor is 0: relief cancels raised Alarm and never goes past neutral. RELATIONS.md § 6 carries it.

*Files: `src/world/sentiment.cpp`, `docs/politics/RELATIONS.md`*

### NR-1116 — An embargo is a verb and a pair, not a law record
*question · raised 2026-10-31 · from Lane E report, sprint 61 (BL-1418 embargo bars goods), 2026-10-31*

RELATIONS § 3 says an embargo's author is a law (the relationship family). No law verb could carry it, so the lane added one verb, set_embargo (author, target, enact or repeal), stored as an (author, embargoed) pair set and saved at v70. Nothing is a law record with a law_effect_kind. Refused cargo is destroyed at the border (it was bought and hauled at dispatch), and dispatch itself now refuses an embargoed shipment so a standing trade does not buy goods to burn each tick.

**Why it matters.** Either the doc sentence changes to match, or the embargo is re-housed as a law (and then the Diplomacy ledger's Embargo press becomes a law press). The ledger is being built against set_embargo.

- A: keep the verb; RELATIONS § 3 says the author is the nation, enacted by set_embargo
- B: re-house as a law record with a law_effect_kind, enacted through the law verb

> **Recommendation:** A. A pair set is the legible form; nothing else in the law family reads it.

> **RESOLVED.** Ben, 2026-11-03 (sprint 61 calls form): A - keep the verb: set_embargo, stored as an (author, embargoed) pair set. RELATIONS.md § 3 says the author is the nation, enacted by set_embargo.

*Files: `src/world/embargo.cpp`, `docs/politics/RELATIONS.md`*

### NR-1117 — May rival nations embargo?
*question · raised 2026-10-31 · from Lane E report, sprint 61 (BL-1418 embargo bars goods), 2026-10-31*

set_embargo exists and the player can press it; no scorer issues it, so rivals never embargo. Letting them would be a new subject under the AI-behaviour prohibition (a relation, not an action on own ground), so it is raised, not assumed.

**Why it matters.** Without it, embargo_imposed only ever names the player as author, and the embargo justification for war only ever fires against the player.

- A: grant - a rival may enact or repeal an embargo against a nation, scored over its own sentiment, Alarm and trade exposure, on the standing terms
- B: not now; embargo stays the player's

> **Recommendation:** A, as a dated grant in AI_OPPONENT § 11, built with BL-1461 (rivals declare war) since it shares the scorer.

> **RESOLVED.** Ben, 2026-11-03 (sprint 61 calls form): A - grant: a rival may enact or repeal an embargo against a nation, scored over its own sentiment, Alarm and trade exposure, on the standing terms. Dated grant in AI_OPPONENT.md § 11.

*Files: `src/world/corp_ai.cpp`, `docs/ai/AI_OPPONENT.md`*

### NR-1118 — Five cession calls RELATIONS § 7 does not settle
*decision taken on your behalf · raised 2026-10-31 · from Lane C and its cold review, sprint 61 (BL-1460 cession moves territory), 2026-10-31*

(1) Any peace returns every occupied, uncedeed goal of that war to its owner and withdraws the occupier, so no province stays held at peace by a former enemy. (2) A loser's air wing whose airbase passed rebases to its nearest own airbase, or disbands if it has none (the existing gone-base rule). (3) Units withdraw from ceded ground by the retreat rule, aimed at their own nation's nearest ground; a unit with no legal step stays. (4) A ceded site under construction keeps what the loser paid; if the gainer cancels it, the refund goes to the gainer. (5) A moved capital must not re-roll national cravings: the preference hash will read a saved generation-time capital (save v71).

**Why it matters.** (1) and (2) change what a peace does on the map; (4) moves money between nations.

> **Recommendation:** Accept all five; (4) is the ground passing with what stands on it.

> **RESOLVED.** Ben, 2026-11-03 (sprint 61 calls form): ratified as taken.

*Files: `src/world/war.cpp`, `src/world/market_clearing.cpp`*

### NR-1119 — May a rival impose peace on the player at 100?
*question · raised 2026-10-31 · from Lane R report, sprint 61 (BL-1461 rivals declare war), 2026-10-31*

The war grant's Not-admitted clause says the player's wars "only the player answers and ends". The lane read that as forbidding a rival to IMPOSE on the player at 100: at 100 against the player a rival offers instead, and may accept or refuse the player's offers. Imposing on a rival works as designed. Also taken: rivals declare only across a shared front; front strength counts visible standing units on the front's provinces only; answering is always taken; the war pass runs just before the fronts pass.

**Why it matters.** If imposing is the player's alone to suffer too, the player can never lose ground to a rival by force - only by accepting. That makes war against the player toothless at 100.

- A: a rival may impose on the player at 100 ("answers and ends" means the rival never answers or offers FOR the player, not that the player is immune)
- B: as built - the player can only lose ground by accepting

> **Recommendation:** A. Imposing at 100 is the war's one forcing outcome; immunity removes the threat the ledger exists to read.

> **RESOLVED.** Ben, 2026-11-03 (sprint 61 calls form): A - a rival may impose peace on the player at 100. 'Only the player answers and ends' means a rival never answers or offers FOR the player; the player is not immune.

*Files: `src/world/corp_ai.cpp`, `docs/ai/AI_OPPONENT.md`*

### NR-1120 — Three war loose ends: spectate, offer churn, the public record
*question · raised 2026-10-31 · from Cold review of Lane R, sprint 61 (BL-1461 rivals declare war), 2026-10-31*

(1) In spectate mode the player's nation has no one in its seat and the war pass never scores it, so a rival's war against it is never answered, its offers never answered, and it never ends; other strategic tiers score the player's nation while spectating (BL-409). (2) A rival that offers white peace and is refused re-offers on its next evaluation with no cooldown - endless offer and refuse, one verb a war per evaluation. (3) No war verb writes to the history log or the AI decision ring; only a harness-only trace records why a rival declared, though every declaration is public.

**Why it matters.** (1) leaves permanent wars in every spectated run; (3) means the player learns of a war on them only from the ledger, never from a dispatch or the log.

- (1) A: while spectating, the war pass scores the player's nation like any rival (BL-409's rule) / B: leave it
- (2) A: a refused offer may not be repeated for 4 ticks (needs a saved refusal tick) / B: accept the churn
- (3) A: declarations and peaces write a history entry and raise a chat dispatch to the player when it is party / B: the ledger is enough

> **Recommendation:** 1A, 2A, 3A. (3) answers diplomacy.md open question 2 as well.

> **RESOLVED.** Ben, 2026-11-03 (sprint 61 calls form): 1A, 2A, 3A - while spectating the war pass scores the player's nation like any rival; a refused offer may not be repeated for 4 ticks (saved refusal tick); declarations and peaces write a history entry and raise a chat dispatch to the player when it is party.

*Files: `src/world/corp_ai.cpp`, `src/world/war.cpp`*

### NR-1121 — Wars cost the economy gate 1.6 points: G1 at tick 50 now 83.5
*decision · raised 2026-10-31 · from Sprint 61 wave 2 gate on main 37b8ac7c (rivals declare war merged), 2026-10-31*

War census, 16 library seeds to tick 40: 34 wars on 9 seeds (seed 25 has 13, with 11 of 43 nations at war at once); seeds 12, 28, 31, 32, 37, 38, 40 see none; every peace by tick 40 is white; one rival declares on the player (seed 10). market_viability full with wars: G1 93.2, G1b 2.5, G2 190.5, G3 100 all pass; G1 at tick 50 falls from 85.1 to 83.5 against its 85 floor (seed 25 92.1 -> 84.6, seed 0 87.5 -> 83.1). Hostile cargo is intercepted, so a war idles the processors that fed on it - the designed consequence, measured.

**Why it matters.** G1t50 is already the marginal gate BL-1442 (post-capacity starvation) carries; wars push it under. The war band, and this cost, are what the wave re-bless will pin.

- A: accept - wars are meant to cost trade; BL-1442 carries G1t50 as before
- B: calm the rivals (raise the declaration bar) until G1t50 clears 85 with wars on

> **Recommendation:** A. A war that costs nothing would be the defect; damping rivals to protect a gate is a forced outcome.

> **RESOLVED.** Ben, 2026-11-03 (sprint 61 calls form): A - accept: wars are meant to cost trade. BL-1442 (post-capacity starvation) carries G1t50 as before.

*Files: `src/world/corp_ai.cpp`, `tools/verify/war_census.cpp`*

### NR-1122 — Two rulings collide: every capital has a base, and bases are cut like any building
*question · raised 2026-10-31 · from Lane D report, sprint 61 (BL-1464 capital base missing), 2026-10-31*

capital_military_base fails because size_opening_state_to_chest (finish_campaign_world, after the capital bases are laid) demolishes military bases on purpose: seed 46 cuts 15 (all 15 baseless nations are in the cut list), seed 28 cuts 9, seed 11 cuts 26. MILITARY.md § Nation garrisons (Ben, 2026-10-27) says every capital is generated with a military base; NATION_GENERATION.md (Ben, 2026-10-29) says military bases are cut like any building and a cut garrison stands with no recorded base.

**Why it matters.** A third to two-thirds of nations open with no base, so they cannot recruit or refill (the recruit design of 2026-10-30 hires at bases) until they rebuild one.

- A: the newer ruling stands - a chest can cut the capital base; the harness accepts and names it; MILITARY.md states the exception
- B: the capital base is exempt from the cut, as the capital's free Logistic Point source is

> **Recommendation:** B. Recruits now come from bases, so a baseless opening nation cannot raise or refill an army; the capital base is infrastructure the state needs, like the free hub.

> **RESOLVED.** Ben, 2026-11-03 (sprint 61 calls form): A - the newer ruling stands: the chest cut may remove a capital base like any building, and a cut nation rebuilds one to recruit. MILITARY.md § Nation garrisons states the exception; capital_military_base accepts and names a cut base. BL-1464 (capital base missing) becomes that harness and doc change.

*Files: `src/world/state_industry.cpp`, `docs/military/MILITARY.md`, `docs/generation/NATION_GENERATION.md`*

### NR-1123 — A land unit may cross a bridge over another nation's water
*decision taken on your behalf · raised 2026-10-31 · from Lane D, sprint 61 (BL-1466 march path domain), 2026-10-31*

MILITARY.md § Domains and traversal lets land classes onto coastal water only where owned. A roaded water tile (a bridge) now counts as land for a land class, as a road already lifts the class terrain wall - so a land unit may cross a bridge over another nation's strait. Naval rows still see the water under it.

**Why it matters.** A bridge over a rival's strait becomes an invasion route; without the rule a bridge would be a road nobody may walk.

> **Recommendation:** Accept; a bridge is ground. Write it into § Domains and traversal.

> **RESOLVED.** Ben, 2026-11-03 (sprint 61 calls form): ratified as taken.

*Files: `src/world/battle_system.cpp`, `docs/military/MILITARY.md`*

### NR-1124 — Bunkers: four first-cut calls (hardness as a soft-fire screen, strength, rival uptake, numbers)
*decision · raised 2026-11-02 · from Sprint 61 kit lane, BL-1426 (bunkers) merged at 18b6b22e, 2026-11-02*

(1) HARDNESS: MILITARY.md says concrete makes infantry hard to soft attack. Taken as plain hardness, armour's hard attack (140 > its soft 100) would land 1.8x its open-ground hits and take a bunker faster (day 13 vs day 23). Built instead as a soft-fire SCREEN: 40% of each on-tile defender's soft share screened, hard fire unchanged; MILITARY.md § Fortifications says so. (2) STRENGTH: +50% defence plus the screen, both scaled by condition. campaign_battle C8: first break 1.5:1 open vs 1.75:1 full bunker; at 2:1 the defender breaks day 12 open vs day 118 bunkered. Through the daily pass a supplied 2:1 attack takes open ground day 13 and never takes the bunker (held at day 400) - it falls inside the 1.25 dig-in band; breaking it needs artillery wear first (12 condition a day per battery) or more than about 2.1:1. (3) RIVAL UPTAKE: 1 rival bunker in 16 seeds to tick 40 (seed 25). A candidate is an own-ground front tile toward a hostile neighbour where the nation's own units stand, with a road; garrisons mostly stand on posts, not fronts. (4) NUMBERS: 120 credits + stone 20 + steel 8, 90 days, upkeep 5; shelling 12/day per battery; rival cap 4 built or building, only while uncommitted treasury holds 2x capex. Bunkers count against the province building ceiling and tile stack cap.

**Why it matters.** (2) decides whether a supplied front can be broken without artillery; (3) decides whether rivals visibly fortify at all; (1) is a reading of a doc sentence.

- (1) A keep the screen / B plain hardness
- (2) A keep / B lower defence or screen so 2:1 eventually breaks a supplied bunker
- (3) A keep / B widen candidates to tiles beside the front and posts on a hostile border
- (4) A keep the first-cut numbers / B retune named ones

> **Recommendation:** (1) A; (2) B - drop defence to +30% so a supplied 2:1 breaks it slowly and artillery shortens it; (3) B, posts on a hostile border count; (4) A.

> **RESOLVED.** Ben, 2026-11-03 (sprint 61 calls form): (1) A - keep the soft-fire screen. (2) B - defence drops to +30% so a supplied 2:1 breaks a bunker slowly and artillery shortens it. (3) B - candidates widen to posts on a hostile border. (4) A - keep the first-cut numbers. MILITARY.md § Fortifications carries it; the build is the bunker follow-up lane.

*Files: `src/world/battle_system.cpp`, `src/world/campaign_battle.cpp`, `src/world/corp_ai.cpp`, `scripts/economy.lua`, `docs/military/MILITARY.md`*

### NR-1125 — Major opening kit: five calls taken while writing the ruling
*decision taken on your behalf · raised 2026-11-02 · from Sprint 62 design session, Ben's form 2026-11-02 (fixed kit, substitute, Standard, flat treasury, count only)*

(1) KIT MIX: the proposed construction-materials and Arms Works slots are replaced by refined copper and machinery. Arms Works output is wanted only once kit is researched (base kit costs no goods) and the industrial construction recipe is retired, so both would idle on day one against Pass 3's want rule. Steel, copper and machinery make one chain toward ordnance. (2) TAKE-UP: what the walk placed on a major's ground is taken up once majors are known, after the search scored it, and the kit laid in its place; a minor's processor whose feed stood on that ground may lose it. (3) LOGISTICS: history and fold nodes fill the kit's four logistics slots in seeding order; nodes beyond the count are taken up, roads and track stay. (4) ARMY POSTS: of the eight units, four at the capital (2 infantry, artillery, light armour) and four one each at border posts in grudge order, wrapping; land pool opens at 200 recruits. (5) LADDER END: a slot nothing on the substitution ladder places stays unbuilt and is reported, not paid in credits.

**Why it matters.** Each is a reading of the form Ben answered, not a choice he made. (2) and (3) can change what a minor opens with, and every seed digest moves with them.

> **Recommendation:** Confirm all five; (2) and (3) are the ones most likely to move a minor's opening.

> **RESOLVED.** Ben, 2026-11-03 (sprint 61 calls form): ratified as taken.

*Files: `docs/generation/CORPORATION_GENERATION.md`, `docs/military/MILITARY.md`, `docs/generation/NATION_GENERATION.md`*

