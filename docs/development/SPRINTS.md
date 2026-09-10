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

**Sprint-number ceiling advanced to 35 (Ben, 2026-09-08: “We will open sprint 35 looking at the startup budget”).** The 2026-08-24 ceiling stands in spirit — author no sprint past the one Ben opens — with 35 now the horizon. One sprint is open, well inside the three-sprint cap. This is the first sprint authored after the same day's board clear, and it is deliberately four items: the board was emptied because too many items hung off sprint plans that should have been better planned, so the corrective is fewer items with their preconditions stated, not the same list under a new number.

**The startup-budget chain is dropped outright (Ben, 2026-09-08): “drop it — a watched wait needs no budget”.** Sprint 35 was authored that morning around the 72–73 s warm start and re-authored the same day around generation visibility. BL-813/814/815 are deleted under the 2026-08-24 unstarted-plans policy. The ruling is not that startup time stopped mattering — it is that a wait the player is WATCHING is content rather than cost, so the obligation moves from a stopwatch to whether the thing on screen is worth looking at.

## Open now

### Sprint 37 — how humanity spreads before it fights
*Open · opened 2026-09-09*

**Goal.** Ben, 2026-09-09: focus on the initial colonisation and spreading of humanity - how populations grow and develop agriculture in appropriate locations, how cultures grow out of that, and what philosophies emerge as A PRECURSOR TO MILITARY DOCTRINE. The ordering is the point: doctrine should be downstream of how a people came to live where they live, not an independent axis bolted on beside it.

The evidence that this is a separate phase and not a mood: over 4,000 years seed 0 goes from 533 to 1,372 regions with 839 foundings and 9 battles. B384a - every world sees at least one region change hands by war - FAILS because several seeds fight zero times. The 258-battle pathology sprint 36 removed was war logic operating on ground colonisation had produced and had no answer for. One loop with one parameter set is being asked to serve both, which is why a dial like defence_levy_q has to satisfy empty frontiers and contested borders at once.

**Planned.**
- BL-846 (colonisation span) - the diffusion loop, the stated boundary, the year-cost walk over terrain alone.
- BL-847 (domestication package) - affinity and breadth; the settlement gate; crossing.
- BL-848 (culture by route) - inheritance by arrival, superseding nearest-cradle.
- BL-849 (colonisation seeds partition) - settled cells as a hard input to the province partition.
- BL-850 (predation caps pressure) - wildlife danger, logarithmic decay in population, re-wilding on a sack.
- BL-851 (sweep separates cradle outcomes) - four outcomes currently read as one.
- BL-852 (fragmentation from contact) - the tribal marches retire; fragmentation from interpenetration.
- BL-817 (history playback record) + BL-829 (time-lapse view) + BL-830 (top-sixteen scoreboard) - the wizard round 4 surface, which is the INSTRUMENT the cultures are judged on, not decoration.

**Done when.** Round 4 of the wizard plays a 4,000-year time-lapse on a 2D grid-map in the built app, on a live click and not a capture; the arc STARTUP.md sets as the criterion is legible from the map alone - origin, communication, conquest or diplomatic union, a stable dark age; and the cultures it produces are ones Ben judges worth having. Region count is NOT trimmed toward a target: regions become provinces and the country count is the empire phase's to reduce.

**Risk.** THE SCALE QUESTION IS OPEN AND ITS COST IS SUPERLINEAR. Ben's brief says transform regions into provinces and do not aim for a small set. Regions are ~1,372 after 4,000 years; land provinces are ~22,153 - a factor of sixteen. BL-849 already rules that colonisation SEEDS the partition rather than drawing it, so they need not be one-for-one, but how far the region count grows is unsettled. Sprint 36 took the span from 6,923 to 1,288 ms and reach is STILL 41% of the run as a per-region Dijkstra, so sixteen times the regions is not sixteen times the cost. MEASURE REGION-COUNT SENSITIVITY BEFORE PICKING A NUMBER.

THE VIEW HAS A DEPENDENCY THAT IS NOT BUILT. BL-829 draws from the BL-817 playback record, and BL-817 is designed only. The wizard shell IS built (BL-816, complete, live-verified) - round 4 exists as an honest amber placeholder. So the first day is the record, not the page.

B384a MAY WANT RETIRING RATHER THAN FIXING. It asserts every world sees a region change hands by war and fails because several seeds fight zero times. This sprint's design says that is correct behaviour. Raise it; do not quietly delete it.

CODE COMMENTS DESCRIBE SUPERSEDED RULES AND ARE CORRECT UNTIL THEIR ITEMS LAND - nearest-cradle inheritance (BL-848) and welding (BL-852). Fix each WITH its change.

AND THE STANDING TRAP APPLIES DOUBLE HERE: creeds, philosophies and cultures are exactly where a real proper noun is most tempting. Real history transfers as MECHANISM only.

OPENED 2026-09-09 out of sprint 36's redirect. Sprint 36 was closed rather than renamed so its landed military work stays attached to the sprint that did it.

THE FOUR EMPIRE-PHASE ITEMS ARE IN THE POOL, unbuilt and untouched: BL-837 (ancient logistics and roads), BL-838 (fear of being next), BL-823 (anti-hegemon levers), BL-839 (turbulence lean). Each carries a note to re-scope against whatever boundary this sprint settles. BL-845 (the quiet worlds' 1:1 battle/conquest ratio) may well be answered by this sprint rather than by any work of its own.

DESIGN PASS CLOSED 2026-09-09. docs/generation/COLONISATION.md is new and is the authority for the span - sibling to MILITARY_HISTORY.md, one doc per concern. Ben's five design calls: DIFFUSION with no actor (so no AI-behaviour grant is needed, and it must not become precedent for one); a STATED boundary year, not a derived one; a DOMESTICATION PACKAGE that spreads and gates settlement; philosophy is the EXISTING pantheon with no new axis; and colonisation SEEDS the province partition rather than drawing it. Two later rulings: predation caps a penned people's pressure and decays LOGARITHMICALLY IN POPULATION, and fragmentation is re-derived FROM CONTACT with the creeds' tribal marches retired (NR-808). Seven items minted, BL-846..BL-852. Four questions stay open in the doc and none is a design call - three magnitudes for the sweep and one derivation inside BL-852.

### Sprint 38 — how city states become empires
*Open · opened 2026-09-09*

**Goal.** Decompose CIVILISATION.md's Empires design (settled 2026-09-09) into delivery and build it: sparse settlements with seats and hinterlands, roads that gate reach, materials spent on action, culture relations as the engine of conquest, civilisations formed by mixing, and centres that grow only where the road network can supply and govern them.

**Planned.**
- BL-873 (culture coining year) - retains when a culture split off; feeds kinship.
- BL-871 (empire span 400 BCE to 1200 CE) - splits Culture and Empires into genuinely separate wizard rounds.
- BL-866 (settlements are sparse) - seats and hinterland pointers; conquest carries a whole hinterland at once.
- BL-837 (ancient roads / reach gate) - a heavily-walked edge gets cheaper; campaigns beyond sustainable reach are denied outright.
- BL-867 (materials spent on action) - labour splits subsistence/industry/muster; campaigns cost materials; a captured seat carries its stock.
- BL-870 (culture relations) - kinship and opposition drive conquest likelihood; opposition PERMITS conquest, it does not forbid it.
- BL-869 (civilisations from mixing) - named records with an ethic, gated by opposition, outliving the polities that formed them.
- BL-872 (centres from supply/governance) - a population centre grows only on ground the road network can still feed and rule.
- BL-868 (creeds raise armies) - a culture's aggression_q leans the Campaign score; NOT YET DEMONSTRATED, see notes.

**Done when.** BL-868's harness cases (BL868a/BL868b) pass on main against a fixture that actually produces conquests under current BL-837/BL-872 mechanics, with no confound between aggression and how fast a war happens to resolve.

**Risk.** BL-868's wiring (w_aggr_q, a proportional lean around neutral 500, same idiom as w_cult/w_dist) has looked structurally sound across all seven attempts. What has never once landed is a TEST that cleanly shows the effect without a confound: first a vacuous trace_battles-off metric, then separation calibration putting polities outside default neighbour_radius, then a real-but-inverted 6-seed sweep with a symmetric-arms-race confound (both sides escalating together lets a war end EARLY, which logs FEWER campaign_chosen events for the MORE warlike world). The sixth pass fixed that by holding the defender at neutral aggression and measuring time-to-first-conquest -- sound design, but on current main (post BL-837/BL-872) the two_polity_world fixture now produces ZERO conquests within 1000 years for either attacker, so the lean never gets exercised at all. BL-837's reach gate and BL-872's supply floor are both tuned against a different fixture and have made the old BL-868 fixture too hard to fight over. The three items are coupled: BL-868 cannot be verified without either a fixture rebuilt for the post-BL-837/872 world (shorter reach requirement, explicit road/supply seeding) or a longer stop_year giving reach and supply time to build up first.

OPENED implicitly 2026-09-09 alongside sprint 37's design pass (CIVILISATION.md), given its own sprints.json entry 2026-09-10 during sprint-close housekeeping -- the items had been carrying sprint:38 in backlog.json and NEXT_SESSION.md's ordering since the design closed, but no sprint object existed until now.

BL-867's backlog record was found unmarked during this housekeeping pass despite its code having landed on main in b25db602 (2026-09-10 02:14) -- the delivery commit happened, the bookkeeping commit did not. Fixed same session.

BL-887 (reach-as-centre-chains) was filed out of this sprint's work, priority B, no sprint -- Ben's own call to defer the reach model rework (chains of population centres, Logistic Points) to a later sprint once tech progression is wired into generation. Too few small polities survive Round 4 as-is; BL-887 is the eventual fix, deliberately not now.

BL-861 (no-conquest measurement, sprint 37) and BL-823 (anti-hegemon levers, no sprint) both came up when checking what else might belong to this close. Neither is sprint 38's, and both need a re-scoping pass before implementation -- their backlog prose predates how much this sprint changed the mechanics underneath them.

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
| 31 | Long-term market viability - every recipe pays at base price | CLOSED 2026-09-02 on Ben's call, stage one a SUCCESS: the field ends the standard thirty-year lapse with a majority of corps operating-positive (46 of 61, 31 of 55) where it began with four and none, debtors a tenth of the field, median balances climbing, buildings running. The growth half - valued production still declines over the run - is sprint 33. |
| 33 | Long-term market viability, the growth half - the field that keeps producing | OPENED 2026-09-02 on Ben's call as sprint 31's second half. SUPERSEDED 2026-09-08, never worked: "remove sprint 33. Let's approach the next task with a fresh mindset." Its five planned items (BL-746, BL-745, BL-738, BL-726, BL-725) were cancelled into the backlog archive the day before, when the live backlog was cleared to zero. The baseline in `notes` is the measured state the field was in when it closed, and is the one thing here still worth reading. |
| 34 | Long-term market viability, the growth half - the field that keeps producing | OPENED 2026-09-02 as sprint 33; renumbered to 34 on 2026-09-07. SUPERSEDED 2026-09-08, never worked: Ben, "remove sprint 33. Let's approach the next task with a fresh mindset" - naming this sprint under its old number. Its five planned items (BL-746, BL-745, BL-738, BL-726, BL-725) were cancelled into the backlog archive the same day. The baseline in `notes` is the measured state the field was in on 2026-09-02, and is the one thing here still worth reading. |
| 32a | Gamified generation, 32a - the arc runs, and the instruments that measure it are honest | CLOSED 2026-09-06 at a natural boundary. Five items delivered - the two-span sim, the sweep that measures the real run, the continent time axis, the tick-length audit, and the saturation measure promoted where generation can call it. The remaining 29 carry to 32b, led by the water model. |
| 32b | Gamified generation, 32b - the water model, and the reorder that 32a built the instruments for | CLOSED 2026-09-06. ELEVEN ITEMS DELIVERED in three waves - the market batch (BL-774, BL-759, BL-760, BL-770 slices 1-2), wave 1 (BL-776, BL-766, BL-767, BL-748, BL-762, BL-764 slice 1) and wave 2 (BL-777, BL-783, BL-765, BL-750, BL-769, BL-768, BL-754 partial). The world CHANGED, on purpose, and the digests are moved and DELIBERATELY UNBLESSED - BL-780 owns that and now carries four attributed causes rather than one. The remaining 28 carry to 32c. |
| 32c | Gamified generation, 32c - the water model finishes, and phase 6 gets its search | CLOSED 2026-09-07. GOAL MET ON BOTH CHAINS. The water model is complete and blessed ONCE against a stated shape description; phase 6 has a real search WITH a generation caller. Nine items delivered. The remaining 28 were cut to 12 on Ben's call - sprint 29, 33 and ownerless items archived unstarted, to be revisited with a narrower focus. |
| 32c | Gamified generation, 32c - the water model finishes, and phase 6 gets its search | OPENED 2026-09-06 as the 32b continuation, carrying 28 items. Much of its water-model chain landed (BL-776 through BL-780, BL-783, BL-785, BL-786) and the phase 6 chain reached its search. SUPERSEDED 2026-09-08 on Ben's board-clearing call: the carried remainder was cancelled into the backlog archive rather than re-promoted. What landed under it is recorded in the devlog, not here. |
| 33 | Context economy - the corpus stops charging every session for what one session needs | OPENED 2026-09-07 and ran hard: THREE blocks, 16 items delivered, retro recorded below. CLOSED 2026-09-08 on Ben's call clearing the board - "let's approach the next task with a fresh mindset" - NOT because it failed. Its unfinished remainder (BL-807 the corpus citations, BL-808 the reach defect, BL-809 the red province assertions, plus BL-810 and BL-811 filed on the way past) was cancelled into the backlog archive the same day. The retro is the record of what this sprint actually delivered and stands unchanged. |
| 35 | Watching the world be made | CLOSED 2026-09-09 with its placeholder elements standing. The visibility scaffold landed and was verified live; the sim work it would show turned out to be much larger than the sprint assumed, and moves to 36. Three measurements refuted three assumptions, which is the sprint real output. |
| 36 | a revised look at ancient history | CLOSED 2026-09-09, REDIRECTED RATHER THAN FINISHED. Three of six items landed and are verified: the ancient pass is 5.4x/2.9x faster over 4,000 years and the dead-region ping-pong is gone at its cause. The four unbuilt items were empire-phase and went back to the pool when Ben observed that colonisation and empire want different rules. Sprint 37 takes that up. |
| 40 | three trees, one grammar | CLOSED 2026-09-10, the same day it opened, as a DESIGN sprint: the three trees, the grammar, the lint and the Empire scorer are written and linted; the six build items BL-881..BL-886 go to the pool. Ben: "I'm reluctant to push further when Empire and Industry have not landed yet" - the build waits on sprints 38 and 39. |
| 37 | how humanity spreads before it fights | REOPENED 2026-09-09 for a design addendum, having been closed the same day. Ben: 'we have more work to do for sprint 37 ... we need to be sure we retrieve all necessary data to facilitate this.' The Empires phase needs city states growing into empires, ancient logistics expanding, and natural resources abstracted for conquest -- and the Culture phase must HAND THAT FORWARD. docs/generation/CIVILISATION.md is the new authority. The gap found: the migration builds a family tree of peoples (BL-856) and DISCARDS it, so kinship -- the natural substrate for culture similarity -- is unrecoverable by the empire phase. BL-865 closes it and is sprint 37's; BL-866..BL-870 are sprint 38's. |
| 38 | how city states become empires | 8 of 9 items landed and verified on main (BL-873, BL-871, BL-866, BL-837, BL-867, BL-870, BL-869, BL-872). BL-868 (creeds raise armies) is the one holdout, blocked seven attempts running -- not a wiring defect, see notes. |

**Next up.** SPRINT 38 IS OPEN (opened 2026-09-09, given its own entry 2026-09-10) - how city states become empires. 8 of 9 items are landed and verified on main; BL-868 (creeds raise armies) is the sole holdout after seven verification attempts, now understood to be a fixture problem entangled with BL-837/BL-872's reach and supply mechanics rather than a wiring defect. Next session's whole focus here is BL-868: redesign its test fixture for the post-BL-837/872 world, or give it a longer stop_year, before attempting an eighth pass. Sprint 37 remains open pending its reopened design addendum (Ben, 2026-09-09: 'we have more work to do for sprint 37').

**The standing debt out of P1**, worth repeating here because it spans four items: nothing built in that sprint was ever *rendered*. The session ran in a container that cannot build the GUI, so every UI half is compile-clean and arithmetically checked and visually unseen, and no golden was blessed. For a sprint whose own method note is *build it, look at it, then rule*, that is the thing to fix first.

*55 sprints archived cold; 2 open/gated in the hot store.*
