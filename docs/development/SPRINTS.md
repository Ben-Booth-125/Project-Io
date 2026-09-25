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

### Sprint 47 — one history, told through the rounds
*Open · opened 2026-09-24 · Ben (2026-09-24); Claude (handoff from a six-round narrative survey with adversarial verification)*

**Goal.** The wizard reads as one history unfolding, not six screens: what a player meets in one round — a people, a polity, a creed, a place, a road, an industry — carries visibly into the next, and the corporation they choose at the end stands in the history that made it.

**Planned.**
- WAVE 0 — the seams (generation, world-movers land behind the re-bless): BL-1083 (one seed per span), BL-1085 (Begin retired into round six), BL-1101 (the band from history), BL-1096 (the purchase verb), BL-1097 (sea legs recorded), BL-1102 (tariff posture derived), BL-1108 (quit-during-build crash, Light, any time) — BL-1084 (the world built once and moved) CARRIED TO SPRINT 48 (Ben, 2026-09-25)
- WAVE 1 — identity (UI, on the seeds): BL-1087 (realm keeps its colour), BL-1088 (realm keeps its name), BL-1089 (realm becomes nation), BL-1090 (hard borders by people share), BL-1094 (marks that earn their place), BL-1106 (names and voice)
- WAVE 2 — each round's flair: BL-1091 (from Life to people), BL-1092 (routes and splits on the map), BL-1095 (fleets and ties), BL-1099 (works chartered events and the origin sentence), BL-1100 (the rung crossing narrated), BL-1104 (the Culture record saved)
- WAVE 3 — stretch, in the re-bless only if their predecessors land with time: BL-1086 (the search inside generation), BL-1098 (the sea-lane tier stamped), BL-1107 (the culture ground profile) — CARRIED TO SPRINT 48 (Ben, 2026-09-25)
- CARRIED: BL-1068, BL-1072, BL-1073, BL-1076, BL-1080 owe Ben's live click; BL-1078 is delivered by BL-1085; BL-1082 and BL-1049 ride the re-bless; BL-1071 owes a further reading (Ben: measure more first)
- FILED, NOT IN SPRINT: BL-1093 (true migration routes), BL-1103 (convoy arrival duty), BL-1105 (Ages view, four spans), BL-1109 (campaign tech from the Industry mask)

**Done when.** A player can follow one people, one realm and one place from the Culture round to the seat card without a name, a colour or a thread breaking; round 6 plays from the 1960 close while the tail builds and closes on the map the campaign opens on; Begin only seats the player; the four flair asks are on their rounds; one re-bless, taken once.

**Risk.** THE CURSOR. BL-1084 splits a 2,800-line function whose locals thread through every stage; the composition must be byte-identical to the monolith at every round boundary or every pin moves for the wrong reason. Prove it on the 16 seeds before any UI lane builds on the moved world. SECOND: three world-movers (purchase verb, band, tariffs) and one re-bless — every one of them is read on the 16 seeds first (Rule 0b) and none is tuned to make a seed behave. THIRD: the sprint is wide; the identity wave is worth shipping alone if the seams overrun.

THE CUT (2026-09-24). Twenty-seven ids minted (BL-1083..BL-1109); the rulings record is docs/development/drafts/sprint-47-rulings.md, with the two reader workflows beside it (sprint-47-scoping-readers.json, sprint-47-design-prep.json). Delegated readings are NR-918..NR-930.

BEN'S PICKS THAT WENT DEEPER THAN THE RECOMMENDATION: the whole world moves (not the sim close); hard borders by a share threshold with hysteresis (not top-3); the purchase verb and sea-lane tier built now (a sim beat); the recipe band from the history; mid-span works events; the search inside the round's wait. No per-round close; no prebuild of the next round; no heat blooms.

THE ONE SAVE BUMP PER SEAM: envelope (per-span seeds, migration_timelapse, the new record kinds, series fields, the name tables) and world (campaign_band, corporation founded_year/origin_region, lane_level) — each claimed once through next_save_version.js by the first lane that needs it.

THE CLOSE FORM (2026-09-25). Ben carried BL-1084 (the world built once and moved) and wave 3 (BL-1086, BL-1098, BL-1107) to sprint 48, so the done-when drops "every seam opens on the ground the last round left with nothing recomputed" (sprint 48's). The re-bless was taken once (2f877e2c). Twelve calls ruled (NR-920, 921, 931, 932, 934, 935, 937, 939, 940, 941, 942, 943; NR-936 folded into BL-1084); four become sprint-47 tasks before the live clicks (BL-1083 Culture reroll removed, BL-1090 frontiers only, BL-1099 origin sentence, BL-1085 settle progress tap) and four are filed, not sprinted (BL-1114..BL-1117).

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
| 37 | how humanity spreads before it fights | CLOSED 2026-09-10. REOPENED 2026-09-09 for a design addendum, having been closed the same day. The gap found: the migration builds a family tree of peoples (BL-856) and DISCARDS it, so kinship -- the natural substrate for culture similarity -- is unrecoverable by the empire phase. BL-865 (the addendum's whole reason to reopen) LANDED 2026-09-09 and is verified on main -- culture now carries `parent`/`origin_farm_class`, walking every daughter back to a cradle. KEPT OPEN 2026-09-10 for seven owed items -- BL-849, BL-852, BL-853, BL-854, BL-855, BL-859, BL-861 -- ALL NOW DELIVERED (BL-859 cancelled and redirected to BL-888, also delivered). BL-861 (no conquest across the 4000-year span) resolved as a measured SIDE EFFECT of BL-849/852/855/888 together, not a direct fix to war logic -- history_sweep across 16 seeds now shows every seed fighting (median 61 battles, range 7-141; seed 0 specifically went from 0 to 61). Sprint 38's BL-868 is blocked on the same symptom and should be re-checked against this before further diagnosis. |
| 38 | empire — the whole phase | 8 of 9 items landed and verified on main (BL-873, BL-871, BL-866, BL-837, BL-867, BL-870, BL-869, BL-872). REFRAMED 2026-09-10 on Ben's call (option B): the sprint no longer closes on BL-868's harness, but on a DISTRIBUTIONAL reading of history_sweep. The sweep was run and the reading is in: conflict is fixed, asymmetry and volatility are not. BL-889, BL-890, BL-891 and BL-892 were added to the sprint on that basis; BL-868 is now blocked on BL-889. |
| 40 | three trees, one grammar | CLOSED 2026-09-10, the same day it opened, as a DESIGN sprint: the three trees, the grammar, the lint and the Empire scorer are written and linted; the six build items BL-881..BL-886 go to the pool. Ben: "I'm reluctant to push further when Empire and Industry have not landed yet" - the build waits on sprints 38 and 39. |
| 39 | the drama of the time-lapse | OPEN. Redefined 2026-09-11 (Ben) from "tighten the levers" to the DRAMA of generation: the political map is highly stable and empires never fragment back into city states, and the rounds render a computed record after the fact. Sixteen items filed off a holistic read of the generation layer (seven parallel readers, 67 stability claims adversarially checked); five design calls resolved on the form 2026-09-11 (NR-835..NR-839). Batch-delivered in one go. |
| 40 | exploration | CLOSED 2026-09-12. All fifteen planned items (BL-930..BL-944) built, independently verified and archived across six waves -- BL-945 stays parked for Digitisation. Every item rebuilt and reverified in the main session, not taken on a sub-agent self-report. |
| 41 | Exploration trade | COMPLETE 2026-09-14. Nine items built and merged; the wave world shape authorised at Alarm 525 (NR-867); the wants claim weakened in EXPLORATION.md (NR-865). |
| 42 | generation sharpened before Digitisation | CLOSED 2026-09-16. Twenty-five items delivered across two waves (wave 0: nine; wave 1: sixteen, one of them — BL-1000 — built but open on a live click). Every gate green on the integrated tree bar two knowns: the exploration R3b pin, held red as the wave re-bless NR-877 asks Ben to authorise, and era_world_harness's three reds, which pre-date the sprint (BL-1010). Five items moved the world and each was measured in isolation. Four design calls and one reading are in the queue as NR-878..881; four items of owed work are filed as BL-1007..1010. |
| 43 | the 1960 baseline | CLOSED 2026-09-17. Four items delivered in one day, one lane plus one worktree port; no shipped world moved (digests identical). The gate is NR-888: W1, W2 and the clock, on the 1960 baseline. |
| 44 | the corporate web's plumbing | CLOSED 2026-09-18. Four items: harness parity, a world-bytes pin, the charter-budget seam (off by default, 16/16 pins held), and the cost sweep. No shipped world moved. NR-889 (the density cap) carries the cost readings for Ben. |
| 45 | industrialisation makes the web real | OPEN 2026-09-18. Ten items in three waves: world copies, the span boundary, the Industry tree and the charter rules first, all behind switches; then the span, Beat 1 and the stockpile budget; then the real-stockpile sweep and one re-bless that turns it all on at epoch 0. |
| 46 | the generation reaches the game | CLOSED EARLY 2026-09-24 (Ben: cut v0.1.25, move to sprint 47). The core landed — the wizard plays the whole arc, Begin keeps the world it built, the player chooses the corporation — and the economy thread it exposed was rebuilt; the carry rows and the draft's unfiled rows go to the sprint 47 cut. |
| 47 | one history, told through the rounds | OPEN 2026-09-24. Cut from Ben's design form (docs/development/drafts/sprint-47-rulings.md): the world is built once at the Life gate and moves forward, rerolls are per stage, round 6 absorbs Begin, and a realm keeps its colour, shade and name to the seat card. Twenty-three items in four waves plus the five owed live clicks and one re-bless. |

**Next up.** SPRINT 47 OPEN (2026-09-24): wave 0 first — BL-1083 then BL-1084 in one generation lane, BL-1085 in a core lane, BL-1101/BL-1096/BL-1097/BL-1102 as world-mover lanes; the identity wave opens as soon as BL-1083 lands. Re-bless once after wave 0's world-movers.

**The standing debt out of P1**, worth repeating here because it spans four items: nothing built in that sprint was ever *rendered*. The session ran in a container that cannot build the GUI, so every UI half is compile-clean and arithmetically checked and visually unseen, and no golden was blessed. For a sprint whose own method note is *build it, look at it, then rule*, that is the thing to fix first.

*63 sprints archived cold; 1 open/gated in the hot store (2 completed and awaiting archive_sprints.js).*
