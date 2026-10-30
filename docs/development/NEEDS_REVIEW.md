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

*18 entries — 14 open, 4 resolved.*

---

## Open

### NR-1082 — Two disclosure tensions the NR-1066 doc pass found
*question · raised 2026-10-26 · from NR-1066 doc pass*

(1) A rival's treasury is private (Ben, 2026-10-26), but DISCOVERY.md § The operational fog says an open book shows "flows, balance", and FINANCE.md § Surfaces gives the Profitability ledger one row per nation, so a rival's filed return could expose its treasury. Whether a rival's return is readable at all is unruled. (2) The Industry lens, kept, weights rival plants by output_quantity, against "production stays private" (DISCOVERY.md; TOOLTIP.md). Also: the seat screen lists every major's treasury before play.

**Why it matters.** Each decides what the player can see of a rival nation.

- (1) A: a rival's return is unreadable; B: readable without its balance line. (2) A: the Industry lens shows rival plant count or presence, not output; B: output is public through the lens.

> **Recommendation:** (1) B; (2) A. The seat screen is a pre-play pick, so it may show treasuries.

*Files: `docs/ui/DISCOVERY.md`, `docs/economy/FINANCE.md`, `docs/ui/LENSES.md`*

### NR-1084 — Four combat calls taken in writing the land model
*decision taken on your behalf · raised 2026-10-26 · from sprint 58 military design*

Writing Ben's land rulings into MILITARY.md § The campaign force model took four calls he did not make: (1) defence and breakthrough are separate stats (defending vs attacking); (2) no piercing-versus-armour check, hardness alone; (3) readiness takes the larger share of landed damage, so most fights end in retreat; (4) the seven classes - motorised infantry replaces trucks, one main battle tank line replaces light/medium/heavy, light armour is low-hardness reconnaissance, and air defence is added.

**Why it matters.** Each shapes BL-1421 (soft hard resolver) and BL-1422 (land classes and kit) before they are built.

> **Recommendation:** Keep all four; the realism case is in research/MILITARY_1960_PRESENT.md § 4 and § 10.

*Files: `docs/military/MILITARY.md`, `docs/research/MILITARY_1960_PRESENT.md`*

### NR-1085 — Five calls taken in retiring the Era catastrophe
*decision taken on your behalf · raised 2026-10-26 · from era catastrophe retirement design pass*

Writing Ben's retirement (ERAS.md § The point of an Era) took five calls he did not make: (1) each nation crosses into the next Era on its own when it meets the gate, not the world at once; (2) Alarm is a third sentiment dimension beside Access and Trust (RELATIONS.md § 6), directed per pair, decaying, under the informs-never-declares invariant; (3) three Digitisation nodes that read the retired Ceiling were rewritten in the store: Inspection Regimes 'Alarm raised by your visible capability -10%', Arms Control Verification's second effect 'every nation's Alarm toward every other decays 10% faster', Quiet Borders' deed 'no nation's Alarm toward you rises for 8 quarters'; Planetary Accords keys 'shared limits on climate Load'; (4) the tempo trap retired with the countdown; (5) climate is Era 2's continuous pressure with no test.

**Why it matters.** Each shapes what a nation chases and what Alarm costs it before any of it is built.

> **Recommendation:** Keep all five. (1) makes the race to orbit the success Ben asked for; (2) reuses a tested substrate rather than adding a new store.

*Files: `docs/economy/ERAS.md`, `docs/politics/RELATIONS.md`, `docs/economy/tech/digitisation_tree.json`, `docs/CLIMATE.md`*

### NR-1086 — Why are strategic nuclear weapons never fired, now there is no catastrophe?
*question · raised 2026-10-26 · from era catastrophe retirement design pass*

Ben ruled tactical nuclear use permitted and strategic never fired (MILITARY.md § Missiles), the same day the Era catastrophe retired. The old reason - 'firing one is the catastrophe' - is gone. The rule stands as written, with the arsenal acting through Alarm alone.

**Why it matters.** A rule with no stated cause reads as arbitrary to the player, and the AI needs to know what an arsenal is for.

- A: keep the rule; the arsenal is pure deterrence through Alarm
- B: allow strategic use with ruinous consequences (sentiment collapse with every nation, climate strain, the target razed)
- C: strategic use only in answer to a strategic strike (second-strike only)

> **Recommendation:** A for v0.3.0: cheapest and legible; revisit B once war goals and peace exist.

*Files: `docs/military/MILITARY.md`*

### NR-1087 — Six calls taken building the land combat core
*decision taken on your behalf · raised 2026-10-27 · from BL-1421 (soft hard resolver)*

(1) HP is the unit's count of men against a stored establishment; equipment joins HP with BL-1423 (refit and retreat). (2) Terrain DIVIDES the attacker's hits (a mountain halves them) rather than raising the defender's absorb: an absorb raise mattered only where hits already exceeded defence, so infantry-on-infantry ignored mountains. MILITARY.md says so now. (3) The calibration, measured by campaign_battle_harness C4 (infantry on plains, 200 seeds): breaking a defender takes about 2:1 - decisive 0% at 1:1, 15% at 1.5:1, 64% at 1.75:1, 89% at 2:1, 99% from 2.5:1; fights last about two ticks. The old curve was 50% at 1:1 because the old round had no attacker penalty. (4) A side of spent units is overrun: it deals nothing, the enemy's hits land on it in full, and it is broken. (5) A retreat moves one tile: land, never onto an enemy of the fight, preferring outside the province then own ground, ties to the lowest tile id; a unit with nowhere to go stays put, out of the fight. (6) Morale: +50 to a battle's winners, -100 to its losers and to every unit that retreats; out of contact readiness regains 100 a tick scaled by supply, never above morale, and morale moves 10 a tick toward supply. Found by the cold review and left for you: (7) a static nation garrison that retreats has no order to return, so it stays off its post; (8) the rival scorer's unit_strength still reads men x quality x supply and ignores class, readiness and morale, so the AI's picture of strength has drifted from the resolver (BL-1427, fronts, is where the AI reads force); (9) a v57 save loads its units with no establishment, so a unit wounded before the load reads at full HP.

**Why it matters.** Each sets how war feels before fronts and the AI are built on it. And the re-bless: player_seed_sweep digests fail 16/16 with the v58 stream, owed to the sprint's re-bless.

> **Recommendation:** Keep all six; (3) is the one to feel in play.

*Files: `src/world/campaign_battle.cpp`, `src/world/battle_system.cpp`, `src/world/unit_roster.cpp`, `docs/military/MILITARY.md`*

### NR-1088 — Should a rival's unit counter show its HP and readiness?
*question · raised 2026-10-27 · from BL-1425 (unit counter render)*

The counter and the unit card show HP, readiness and morale for the player's own units only. A rival's counter shows its owner colour and unit count, and its card shows no condition bars. This follows BL-068 (a rival's internals stay private) as the battle card already applies it. Your brief said units show HP and readiness, without saying whose.

**Why it matters.** Seeing a rival's spent units is how a player picks where to push, and fronts (BL-1427) will lean on it; but it is a widening of what the player sees of another nation, which is yours to rule.

- A: own units only (as built)
- B: everyone's bars always
- C: a rival's bars only where the player has units in contact or a recon satellite overhead (the intelligence layer, BL-1431)

> **Recommendation:** C, with A until satellites exist: it makes intelligence a thing worth buying.

*Files: `src/ui/body_surface_canvas.cpp`, `src/ui/selection_panel.cpp`, `docs/ui/DISCOVERY.md`*

### NR-1089 — Refit: the calls taken on your behalf
*decision · raised 2026-10-27 · from sprint 58 refit lane (Ben, 2026-10-27: "add refitting a unit's type from selection to a newer compatible model")*

Compatible means the same land class and a newer row: a later band, or the same band with a higher power. Credits are the hire price difference (hire_cost_per_power x the power gap, floored at 0) plus the target row's goods in full. It takes the target's muster days. The unit must stand on its own nation's ground, out of battle, and cannot march while refitting. Men, readiness and morale are kept. A refit clears the unit's standing march order. Rival nations never refit.

**Why it matters.** Each is a balance or feel call. Charging the full goods makes kit upgrades cost as much as new kit; clearing the march order is a choice the brief did not make.

- A: keep all as built
- B: change one (say which)

> **Recommendation:** A. Revisit the price once kit and designs (BL-1434, BL-1436) give refits more to buy.

*Files: `docs/military/MILITARY.md`, `src/world/corp_command.cpp`*

### NR-1090 — "Refit" now names two things
*question · raised 2026-10-27 · from sprint 58 refit lane*

MILITARY.md § Land — equipment, refit and manpower already uses "refit" for refilling a wounded unit's HP. The new verb uses it for changing a unit's type. The doc says they are distinct, but GLOSSARY.md settles neither.

**Why it matters.** One word for two mechanics confuses the player and the action dictionary.

- A: "Refit" is the type change; the HP refill becomes "Replenish"
- B: keep "Refit" for HP; the type change becomes "Upgrade"
- C: leave both

> **Recommendation:** A. The button on the unit card already reads Refit, and replenish is the military term for refilling men and kit.

*Files: `docs/military/MILITARY.md`, `docs/GLOSSARY.md`*

### NR-1093 — Capital base: the calls taken on your behalf
*decision · raised 2026-10-27 · from BL-1439 (capital military base) lane*

The base goes on the legal own tile nearest the capital garrison, empty before stacked. 592 of 593 nations on the 16 library seeds get one; seed 25's Lainka owns one tile already full and gets none. The opening unit now musters at the capital base, so a nation has one base. A seat moved at Begin keeps its base and garrisons and loses only its opening unit. A garrison is told from a hired unit by entity-id order (no saved flag). The capital garrison records the base, so demolishing the base disbands it (the orphan rule, which never ran until today's fix).

**Why it matters.** The garrison-dies-with-its-base rule is new in effect; the id-order test is an implicit invariant.

- A: keep all
- B: garrisons are never orphaned by their base
- C: add a saved garrison flag instead of id order

> **Recommendation:** A, with C if garrisons ever get hired-in replacements.

*Files: `docs/military/MILITARY.md`, `src/world/corporation_generation.cpp`, `src/world/economy_system.cpp`*

### NR-1094 — A retired recipe slot is a new pattern
*novel-work · raised 2026-10-27 · from BL-1438 (retire construction capacity) lane*

Recipe ids are list positions saved in every building, so the five capacity-yard recipes cannot be deleted. They stay in recipes.lua with retired = true and no inputs or outputs; the loader refuses a retired recipe that names any; a building running one idles but still pays wages. The capacity resource keeps its enum slot, price and spoilage so old stock decays.

**Why it matters.** No doc owns retiring a recipe; the next retirement will copy this.

- A: write the pattern into PRODUCTION.md as the rule
- B: also demolish or refund retired-recipe buildings on load

> **Recommendation:** A now; B only if old saves matter before v1.0.

*Files: `scripts/recipes.lua`, `docs/economy/PRODUCTION.md`*

### NR-1097 — Land battle form: three readings taken in writing the carried-supplies and recon rulings
*decision · raised 2026-10-29 · from main session (Ben land battle form 2026-10-29)*

(1) The quarterly supply share arrives as a DAILY resupply of share x daily need, not one quarterly top-up - a quarterly refill dips below full the day after, so the flat hunger penalty would apply almost always. (2) The round reads the three carried stocks; supply_factor_permille no longer scales hits directly, it only sets the resupply rate. (3) Air superiority adds recon in proportion to its share rather than making revealed units fully visible, so superiority alone does not settle every battle. Also: the stat grid replaces the unit card's Strength and Roster pages.

**Why it matters.** Each changes how strong a supplied, a half-supplied and an air-covered army is. Written into MILITARY.md § Carried supplies, § Recon and § Air, ledgers/military.md § The stat grid, SELECTION.md. Work: BL-1452 (carried supplies), BL-1453 (land recon), BL-1454 (unit stat grid).

### NR-1098 — Terrain and the classes: four readings the build took
*decision · raised 2026-10-29 · from BL-1455 (terrain class modifiers) lane, cold review 2026-10-29*

(a) Wear is x1.5 equipment drain - the ruling named the cells but no rate. (b) "Blocked unless roaded" means any road edge on the tile, so armour may enter a mountain tile from a roadless side if some road touches it. (c) The move rows change a march's days but not its route: the path stays the shared terrain-weighted one, so medium armour may be routed through forest at x1.5 when a faster route for it exists. (d) Pre-modern rows (no land class) read none of the rows, not even the open-ground stealth x0.75 the doc gives every unit. Also: carriers dismount even on a mountain road, as the doc reads.

**Why it matters.** (b) and (c) decide whether terrain shapes WHERE armour goes or only how slowly; (c) in particular lets an armoured column walk into a forest debuff it could have avoided. Written into MILITARY.md § Terrain and the classes. Work: BL-1455 (terrain class modifiers).

### NR-1099 — Carried supplies: a hub on every capital, and one purse gate caps surveys too
*decision · raised 2026-10-30 · from BL-1452 (carried supplies) lane, cold-review round 2026-10-30*

(a) The free source is the lowest-id running inland logistics hub on the capital. Seeds 10, 11, 12, 25 and 31 had a railway station or port standing as the capital's source and no hub, so nothing ran free and the opening cut removed their capitals' buildings (dark at the handoff). The seeding's step 0 now stands a hub on EVERY capital, beside any station or port already there. (b) purse_room_of now carries the settle cap for every spend - builds, retools, hires, refits, roads and survey dispatch alike; surveys in the settle were not separately ruled.

**Why it matters.** (a) changes world shape: every capital opens with one more building (the shape probe was not re-run). The alternative is to let a station or port on the capital be the free source instead. (b) a poor nation may send fewer surveys in the settle. Merged at b0a9e9b5, G1t50 full 84.6. Work: BL-1452 (carried supplies).

### NR-1102 — A ceded capital, and a nation ceded away
*question · raised 2026-10-30 · from NR-1101 ruling (capitals may be goals), 2026-10-30*

With capitals open as goals, two cases now exist that nothing owned. (a) A nation that cedes its capital province takes as its new capital the remaining province with the largest population centre, ties to the lower id. The capital is where the treasury sits and where the free logistics hub stands (NR-1099), so the hub must move too. (b) Three goals can take every province of a small nation. Strawman: it leaves the map, its wars close, and its sentiment rows are erased. The alternative is a floor: the last province can never be a goal or ceded.

**Why it matters.** (b) is elimination, and nothing else in the campaign removes a nation, so every consumer of the nation list meets it for the first time (fronts, markets, the scorer, saves, spectating). Both rules are strawman in RELATIONS.md § 7. Work: BL-1460 (cession moves territory).

- A: both strawman rules as written
- B: the capital moves as written; a nation's last province can never be ceded
- C: a capital can never be ceded (a goal, but not cedable)

> **Recommendation:** B. Elimination is a whole subject. The floor keeps every nation on the map until it is designed on purpose.

---

## Resolved

Kept, not pruned: the reasoning is the point. Prune only in a deliberate sweep, once the
answer has landed in an authority doc.

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

