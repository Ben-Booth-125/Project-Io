# Project Io — Sprints

> **Generated file.** Produced by `node tools/session/render_sprints.js` from
> [`sprints.json`](sprints.json) (canonical, open sprints) and `archive/sprints-*.json`
> (completed sprints, cold). Edit the JSON, then re-run; hand edits here are overwritten.

A lightweight rhythm layered over the backlog/Delivery system: a **goal** stated at the start, a **retro** at the close comparing what landed against it. This is feedback *for Ben* — pacing and priority signal, not a new authority (backlog.json/REFINED.md/DEVLOG stay the source of truth for what’s actually true about an item).

Entries are one per **sprint** — a sprint is a themed span of work, not a fixed calendar week; it closes when its goal is settled (landed or deliberately descoped), not on a clock. A gap with no entry means no sprint goal was set — that’s fine, skip it rather than backfilling.

**Fully in JSON since 2026-08-24** (Ben: move sprints into JSON; archive them once complete). Drained from prose 2026-08-19; the last hand-kept surface — this file’s status table — retired 2026-08-24 after NR-598 caught it drifting. `sprints.json` holds the open/proposed/gated sprints; every completed sprint lives whole in `archive/sprints-*.json`, moved by `tools/session/archive_sprints.js`.

## Format

```
## Sprint N — theme (opened YYYY-MM-DD)

**Goal.** 1-2 sentences: the outcome this sprint is aiming for, referencing backlog item ids
and/or a version goal (v0.1.1 etc.).

**Planned.** BL-ids targeted, one line each.

**Retro** (filled in at close).
- Landed: ...
- Slipped: ... (+ why — scope grew, blocked on a dependency, deprioritized)
- Feedback: anything worth Ben knowing about how the sprint actually went — a pattern, a
  misjudged estimate, a design call that needed more/less discussion than expected.
```

(The old **Runtime** line — total session time vs. items delivered — is dropped from the template. It went uncollected for seven-plus consecutive entries; see Sprint 19’s retro in the archive for the reasoning. If the pacing signal is wanted again later, derive it from commit timestamps rather than reviving an unstarted timer.)

---

**Active-sprint cap (Ben, 2026-08-23).** No more than 3 sprints open/proposed at once — enough that a session doesn’t have to weigh a long tail of undocumented parallel work.

**Sprint-number ceiling (Ben, 2026-08-24): keep sprints 18 and below.** Nothing is planned or authored past sprint 18 — narrower than the count cap above, this bounds *how far ahead* the sprint horizon is allowed to reach at all.

**Unstarted plans are DELETED, not archived (Ben, 2026-08-24).** A sprint that was only ever open/proposed and never really executed is a stale reference once dropped — closing it and keeping its prose around just to reopen the same number later isn’t worth the upkeep. On adopting the cap, 27 open/proposed sprints (plus Sprint 32, authored and dropped same-day on the ceiling above) were removed outright from `sprints.json`, freeing their numbers rather than retiring them: 20, 21, 22, 22-24 preamble, 23, 24, 25, 26-33, 26a, 26b, 27, 28, 29, 30, 31, 32, B1, B2, B3, C1, C2, C3, D2, D3, D4, N3, N4, ST1, W1. This is narrower than it looks: a sprint that actually **landed work** — even a partial or unsatisfying result (Sprint 19’s goal NOT met, P1’s rendering debt) — is a real historical fact and stays in the status table as closed. Only the never-executed ones were deleted. A freed number picked back up is authored fresh against the current docs, never resurrected under its old prose.

**Also deleted 2026-08-24, same basis:** six orphaned backlog items (BL-579–BL-584) a concurrent review-queue-purge session had filed with no sprint attached, plus Sprint 32’s own three items (BL-595–BL-597, Logistic Points / Throughput lens) — all unstarted, all removed from `backlog.json` outright rather than archived.

**Sprint 25b deleted 2026-08-24, same policy** (the Sprint 18 design form verdict): never opened, gated on a sequence (25a → 21 → 23 → 25b) whose middle numbers the purge had already freed. Its undelivered half — interception narration, the out_cuts sink — is unowned until re-filed (NR-599); the interdiction core itself landed 2026-08-21 and is unaffected.

**Sprint-number ceiling advanced to 19 (Ben, 2026-08-25: 'Open sprint 19 with these items').** The 2026-08-24 ceiling stands in spirit - author no sprint past the one Ben opens - with 19 now the horizon.

**Sprint-number ceiling advanced to 20 (Ben, 2026-08-26: 'Write this up as sprint 20').** The 2026-08-24 ceiling stands in spirit - author no sprint past the one Ben opens - with 20 now the horizon.

**Sprint-number ceiling advanced to 21 (Ben, 2026-08-26: 'A, and open it as sprint 21').** The 2026-08-24 ceiling stands in spirit - author no sprint past the one Ben opens - with 21 now the horizon. Two sprints are open (20, 21), inside the three-sprint cap.

**Sprint-number ceiling advanced to 22 (Ben, 2026-08-27, corrected 2026-08-28 00:16).** The 2026-08-24 ceiling stands in spirit - author no sprint past the one Ben opens - with 22 now the horizon. Two sprints open (21 demand, 22 UI visibility), inside the three-sprint cap. Ben has named 23 for the new UI elements he will describe once Sprint 22's improvements land, so 23 is spoken for but NOT yet authored.

**A sprint was renumbered and un-renumbered across the 2026-08-27/28 midnight; both moves are recorded because neither lost anything.** The demand sprint was moved 21 -> 22 to free the number for a UI visibility pass, then moved back at 00:16 on 2026-08-28 when Ben clarified that 'sprint 21' means the demand sprint to him. What made the round trip safe is that the demand sprint had LANDED WAVE 0 and was therefore moved WHOLE both times, never deleted under the 2026-08-24 unstarted-plans rule. The lesson worth keeping: a number is cheap to move and expensive to be wrong about, so confirm which sprint a bare number refers to before renumbering anything - the pre-move records (the DEVLOG heading, the NEEDS_REVIEW sources, the `sprint-21-wave-0` requirements batch key) all said 21, and they were right.

**The active-sprint cap is deliberately exceeded, as PROPOSED not open (Ben, 2026-08-28: "Let's make a sprint for each batch").** The 3-sprint cap counts sprints being worked; 23-27 are a decomposition of one agreed body of work, authored together so the batch boundaries are settled once rather than re-argued five times. Only one is worked at a time. If that reads as cap-breaking later, close them back down to a single sprint with a batch list — the content is the value, not the numbering.

## Open now

### Sprint 31 — Long-term market viability - every recipe pays at base price
*Open · opened 2026-09-02*

**Goal.** A robust market in which every player - human or rival - can make a steady profit. Ben, 2026-09-02: "the simplest way to do this is to ensure that all recipes (at base price) make a greater profit than marginal costs" - the obvious part the demand work walked past. Stated as two halves (PRODUCTION.md § The recipe margin anchor): M1, at base price every recipe's margin is at least k x its marginal cost (inputs at base + wage per batch), k authored at 1.0 = profit at least equal to marginal cost; M2, at the price FLOOR a building at typical staffing still covers maintenance, goods upkeep and wages (BL-740's form, for the whole roster). Both are checked at AUTHORING TIME against the three tables, never against live prices, so every other lever - demand composition, the band, the scorer - sits on a roster that can pay. Then MEASURE the field again: the anchor is necessary, not sufficient; the 2026-09-01 ledger's structural -755/qtr and the year-30 debt spiral (interest 96% of net loss) are separate levers and stay named.

**Planned.**
- BL-744 (recipe margin anchor) - FIRST. The instrument half is built: tools/verify/recipe_margin prices every processing recipe and every extraction target in both bands, prints the table, asserts the two halves with a differential red-proof. The retune half is the sprint's real work: turn R1-R4 green without simply lifting every output price. Order of preference, from NR-776: (1) processing base_rate and the wage/maintenance side - wage 12 over 8 batches is 1.50 per batch and is often the WHOLE margin, maintenance 10/tick against 4 batches at W=0.5 wants 2.50 per batch at base and 10 per batch at the floor; (2) input quantities where a recipe is authored at zero or negative value-add (steel_from_blooms 22 in for 8 out, consumer_goods 14 in for 12 out, hydroponics_bay 6.25 in for 3 out); (3) base prices last, up the chain in order, with ceil_mult re-derived after.
- BL-740 (maintenance floor anchor) - the M2 half, now measured: recipe_margin R2/R4 are its rows. Retune the maintenance constants where they breach it, in the same pass as BL-744 so one table clears both.
- BL-738 (industry rates go live) - re-measure the stage-2 rates against the anchored tables. Goods upkeep is 0.49-0.78/tick in band today, a small term beside maintenance; confirm it stays one after the retune.
- BL-725 (sweep price levers) - re-run on the anchored tables. It carries the live finding that ceil_mult must be re-derived (haulage_measure demands > 14.07 against the authored 10.0), and a retune that moves the cheapest base price moves that derivation.
- BL-726 (sweep debt dynamics) - the year-30 lapse baseline says interest is ~96% of the field's net loss. Once the roster can pay at base, the debt spiral is the next drag to measure, and the insolvency exit (BL-743) is its counterpart.
- campaign_lapse (BL-723) on the industrial 1960 band is the done-when instrument: mean operating net, operating-positive corps, valued production, debtor count - the same four the sweeps already log.

**Done when.** recipe_margin R1-R4 green in both bands (registered with ctest once they are), AND campaign_lapse on the industrial 1960 band shows mean operating net positive with a majority of corps operating-positive at year 30 and valued production growing rather than culled - the same instrument, the same four metrics, the baseline of 2026-09-01 beside it.

**Risk.** Anchoring by raising output prices alone compounds up the chain: every tier's inputs rise with the tier below, the ladder widens past the 56x the space premise rests on, and ceil_mult re-derives upward. Retune from rates and costs first, prices last. Two bands share one price table, so a retune that fixes one can break the other - the harness runs both and both must clear. And the anchor is necessary, not sufficient: a roster that pays at base still loses collectively if the money entering the field is less than what leaves, so the lapse measurement stays the verdict and the anchor is the precondition for it meaning anything.

THE OPENING TABLE (recipe_margin, 2026-09-02, k=1.0, W=0.5, floor 0.25). Ancient: 19 priced recipes, 18 fail M1 (glass alone passes), 9 have any positive margin, 0 clear 2x; 18 fail M2 (ordnance_from_blooms alone passes). Industrial: 25 priced (propellant's two routes exempt as unpriced), 23 fail M1 (medical_supplies and spacecraft_components_heavy pass), 16 positive, 0 clear 2x; 23 fail M2. Extraction, both bands: M1 passes everywhere but regolith (0.60 against a 0.40 wage per unit); M2 fails on 16 of 18 - at the floor a mine at W=0.5 earns 2.5 x price per tick against 9 of wages and maintenance, so only rare_earth_ore (6.0) and platinum_group_metals (40.0) cover it. The shape of the failure: the mid-chain is authored at zero or negative value-add BEFORE wages - steel 8 from 7.0 of inputs, refined_copper 7.5 from 6.0, silicon 5 from 4, clean_water 3 from 3, food_rations 6 from 6 - and the 1.50 wage per batch eats what is left. The power plants (oil 5.08 from 3.50, coal 4.35 from 3.00) clear inputs and not the wage. NR-775 records the two delegated constants; NR-776 asks the retune direction.

STAGE 2 RESULT (2026-09-02). The anchor holds: 25 goods re-priced (spacecraft_components 280, ladder 112x), base prices era-banded (ancient steel 113), both producer base_rates doubled, fixed costs cut (extraction 2/4, processing maintenance 2), unit wages x2.776 with hire costs so the military value anchor holds, ceil_mult bound 14.07 -> 8.84. Harness fallout absorbed: value_anchor R6 re-aimed, upkeep_harness U1 reads authored prices, spawn_solvency and upkeep_harness CMake targets gained the config TU they always needed. Two reds left standing for Ben (NR-781): tier_margin R2 (mining out-earns refining per building-tick, 28.2 vs 24.9) and spawn_solvency R3 (the seated corp earns 3.1x the field per holding - the subsidy detector tripped by a real change). Pre-existing: chain_depth's deliberate named-list guard; battle_engagement_harness no longer compiles and blocks `nmake all` (BL-731).

THE FIELD (campaign_lapse, industrial, unwarmed 40 ticks): active buildings 237 -> 263 through tick 12, valued production 13-23k/tick; expenditure 1.5-2.5x income from tick 4 (inputs at the ceiling), interest to 1k/tick, exits from tick 20, survivors idle from tick 25 (43 -> 21 active of ~390 holdings), valued production ~0 for the whole measured window of the warm-started run. The ancient band takes the same over-spend and exit wave and recovers (66 active, +1.9k/tick, one debtor at tick 40). The anchor is necessary and was never sufficient; the next lever is BL-745.

RULINGS (2026-09-02, review form): NR-778 overturned - no banded prices; the ancient chain to steel is depth one (Bloomery Furnace: ore + timber -> steel), the Smithy the deeper alternate, one table re-derived (steel 16.1, ladder ~124x). NR-779/780 ratified. NR-781: tier_margin R2 compares net per UNIT of output; spawn_solvency R3 holds the seated corp to the best rival per holding rather than 3x the mean.

## Where things stand

| Sprint | Theme | State |
|---|---|---|
| 1 | Procedural generation v1 | Closed — goal met (food cluster landed 2026-08-02, see amendment) |
| 3 | Corp AI stage B + skill harness | Closed — BL-203, BL-204 both landed |
| 4 | Communication surface (BL-205 chat log) | Mostly landed — slice 1 complete 2026-07-26/28; only the C-route remainder (§7 Stage C) stays open, unstaffed |
| 2a | Close out the v0.1.0 cut set | Closed — all four planned items landed |
| 2b | BL-210 oral-history pivot (nations/corps rewrite) | Closed — all four rungs built (BL-217, BL-208, BL-218, BL-219) |
| 5 | Era −1 history sim, 0–2000 CE (BL-271–275) | Closed 2026-08-10 — four of five landed; BL-274 and BL-317 carried to v0.3.0 |
| 6 | The release sprint | Closed — five versions tagged; every cut minor now carries a done-definition |
| 7 | The stub minors become releases | Closed — v0.1.3 and v0.1.4 cut; post-v0.1.0 swept |
| 8 | Who the player is (design only) | Closed 2026-08-10 — BL-094 rewritten as the militia, BL-350 filed, v0.3.0 roster reconciled |
| 9 | The militia takes the field | Closed 2026-08-10 — v0.1.5 cut; BL-325/BL-331 landed, BL-332 designed and re-versioned |
| 10 | The living world | Closed 2026-08-11 — the living world: real firms produce and consume; market saturation made real, not injected |
| 11 | Procurement, and the goods it is about | Closed 2026-08-11 — all three landed in build order, each verified before the next; v0.1.14 cut |
| 15 | The 0 CE refocus | Closed 2026-08-12 — epoch 0, 3× map, Era −1 sim wired in, mercenary seam designed |
| 16 | The mercenary vertical slice | Closed 2026-08-24 — all ten (BL-569..BL-578) landed in six dependency waves; the loop live-clicked end to end; v0.1.15 cut |
| 12 | v0.1.11 reconciled | Superseded 2026-08-12 by the 0 CE refocus (NR-177), never opened |
| 13 | Generation visibility, and the owed timelapse | Superseded 2026-08-12 by the 0 CE refocus (NR-177), never opened |
| 14 | v0.2.0 (the AI opponent) | Superseded 2026-08-12 by the 0 CE refocus (NR-177), never opened |
| 17 | The ancient roster becomes a ladder | Closed 2026-08-24 — all ten items (BL-585..BL-594) landed. Cut v0.1.17, done-definition written at the cut |
| 18 | The military engagement surface | Superseded — the military-surface remainder subsumed into the re-planned Sprint 16 (2026-08-23) |
| 19 | The economy tells the truth | Closed 2026-08-17 — goal NOT met. The blame moved three times and landed on supply; goldens left red and unblessed |
| 18 retro | The growth gate, and four things measurement overturned | Retro-recorded 2026-08-16 — BL-428 complete; four plausible stories overturned by measurement |
| 18b | Roster invariants | Closed 2026-08-16 — BL-432 landed; BL-435 paused 4/6; BL-436 filed |
| 25a | The draw (upkeep, ordnance, convoy seam) | Closed 2026-08-18 — 6/6; goldens left red and attributed |
| 27 | The run is retained, and its failure is falsifiable | Closed 2026-08-20 — Lane A landed: the run retained, “the sim conquers nothing” a red assertion |
| D1 | A tech can express a buff | Closed 2026-08-19 — BL-479 complete same-day, 35/35; BL-443 rider deliberately not taken (NR-296 is Ben's) |
| P1 | The province becomes a thing you can see, and then a thing worth seeing | Closed 2026-08-21 — done_when met, then overshot by five items; owed: nothing was rendered |
| N1 | The two spines, landed inert | Closed 2026-08-23 — all three landed inert; two of the three were UNSOUND and were fixed in the closing pass (NR-546, NR-547) |
| N2 | The spines move | Closed — three lanes merged; one lane’s interpretation withdrawn after adversarial verification (NR-554) |
| 17b | The shell stops fighting the map | Closed 2026-08-24 — goal met. All nine items; 93 scripts / 4 failures, all four pre-existing (NR-606). Four doc fictions found, one fixed |
| 18 | Logistic Points land with their consumers | Closed 2026-08-25 — all eight landed (BL-596..BL-603, BL-606..BL-608); Sprint 17b merged alongside; v0.1.18 uncut pending Ben |
| 26 | Re-baseline (the gate; nothing else may open first) | Subsumed — split at execution into 26a/26b, themselves deleted in the 2026-08-24 purge |
| 19 | The world reads lived-in - population foundations | Closed 2026-08-25 - thirteen of fourteen items landed (BL-610..BL-618, BL-620, BL-621, BL-623, BL-624); v0.1.19 recut after the NR-640 verdict wave |
| 20 | The books open, and the start earns its way | Closed 2026-08-26 - GOAL MET AND PROVED. Fifteen items landed; BL-634 measured Ben's own criterion and it holds. Cuts v0.1.21. |
| 21 | The other half of the economy - demand | CLOSED 2026-08-31, wave 0 only (BL-648 guard, BL-649 census). Its remaining waves are RESUMED AS SPRINT 27 on Ben's call - "unpause demand. Let's work on this for sprint 27" - re-planned against sprint 26's census measurement rather than its own original ordering. |
| 22 | UI visibility - batch 1: lenses | Closed 2026-08-28 - the lens batch reviewed and reworked; the remaining element classes become their own sprints (23-27) |
| 23 | UI visibility - batch 2: selection & hover | Closed 2026-08-28 - goal met. Twelve items across three waves; every planned item landed |
| 25 | UI visibility - batch 5: canvases & the zoom ladder | Proposed 2026-08-28 as sprint 26; RENUMBERED to 25 on 2026-08-30 when Ben retired the shell-chrome and startup batches - "Sprint 25 and 27 don't need a revisit, UI items for these are working great." SUPERSEDED 2026-09-02 on Ben's call (archive all prior sprints): never opened. The canvas review it proposed ran instead as sprints 29-30, the baked ground. |
| 24a | UI visibility - batch 3: ledgers | Closed 2026-08-29 - batch 3, the ledgers; goal met and exceeded, and four designed mechanisms found never to have run |
| 24b | UI visibility - batch 3b: the ledgers not yet read | Closed 2026-08-30 - the six unread ledgers reviewed, three rebuilt, and the batch's review queue worked through rather than filed forward. |
| 26 | The world that brakes a leader - a rival worth watching | CLOSED 2026-08-31 AT WAVE 1 on Ben's call, GOAL MET. Spectator mode works and the feed reads; the measurement that wave 1 produced then made wave 2 not worth running, and demand takes priority as sprint 27. |
| 28 | The AI that holds back - the brake, once standing means something | PROPOSED 2026-08-31 on Ben's call, taking the three sprint-26 items whose tuning is blocked on demand. NOT open: it starts when sprint 27 has given standing a meaning. SUPERSEDED 2026-09-02 on Ben's call (archive all prior sprints): never opened. BL-697 landed under sprint 26's close; BL-698 and BL-699 were cancelled with it. |
| 27 | The other half of the economy - demand, resumed | CLOSED 2026-09-02 on Ben's call (archive all prior sprints, sprint 31 opens). Both buyers and the whole operating-loss block landed; what it measured is the reason sprint 31 exists. |
| 29 | The world gets a face - detailed canvas rendering | CLOSED 2026-09-02 on Ben's call (archive all prior sprints). The ground has a mechanism: baked painterly chunks in the C-F direction, stepped zoom with bake tiers, a threaded bake, muted borders. |
| 30 | Canvas texture update | CLOSED 2026-09-02 on Ben's call (archive all prior sprints) with wave 1 landed and merged to main the same day: edges back on the ground, the land tilts at the top rungs. |
| 31 | Long-term market viability - every recipe pays at base price | OPEN. 2026-09-02: BL-744 stage 2 landed and ruled on - one price table (NR-778 overturned: the ancient chain reaches steel at depth 1 instead), the three constants and the anchor-route rule ratified, the two design-red harness rows re-expressed. recipe_margin ALL PASS in both bands. The field re-measure says the industrial band still collapses on inputs bought at the ceiling; BL-745 (processor input bid cap) is the next lever. |

**Next up.** SPRINT NUMBERING, reset by Ben on 2026-08-30: the proposed shell-chrome (old 25) and startup (old 27) batches are DELETED - "Sprint 25 and 27 don't need a revisit, UI items for these are working great" - and the canvases batch renumbered from 26 to 25. THE NEXT NEW SPRINT IS 26. Two of the six UI review batches from 2026-08-28 therefore never run, and that is a judgement that their surfaces are good enough rather than a deferral.

OPEN AND PAUSED: sprint 24b (the ledgers not yet read) is the live one; sprint 21 (demand) stays PAUSED with wave 0 landed - its guard (BL-648) is deliberately red and its census (BL-649) is the before/after instrument for when it resumes. Sprint 20 also stays open and owns BL-634 (acquisition viability), which cannot honestly pass until sprint 21's demand channels land.

NEXT UP: sprint 25 (canvases & the zoom ladder), carrying BL-694 (top-bar tracker) out of the retired batch 4.

**The standing debt out of P1**, worth repeating here because it spans four items: nothing built in that sprint was ever *rendered*. The session ran in a container that cannot build the GUI, so every UI half is compile-clean and arithmetically checked and visually unseen, and no golden was blessed. For a sprint whose own method note is *build it, look at it, then rule*, that is the thing to fix first.

*44 sprints archived cold; 1 open/gated in the hot store.*
