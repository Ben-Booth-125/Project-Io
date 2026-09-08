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

### Sprint 35 — Watching the world be made
*Gated · opened 2026-09-08*

**Goal.** THE PLAYER WATCHES THE WORLD BE MADE, and generation hands the campaign a world whose trade and grudges have a cause.

Generation has exactly one surface that works - the wizard, where you set a lean, watch a globe resolve, and understand what you chose - and it stops dead at planetology. Everything after it is a bar. This sprint carries the two passes a player would most want to have watched into that idiom.

ROUND 4 IS PASS 1, THE POLITY PASS. A 2D map replaces the globe and runs a time-lapse of 4000 years to 1200 CE, with an ordered scoreboard of the top sixteen on the left. The arc it must produce is Ben’s: origin -> communication -> conquest or diplomatic union -> a stable dark age. What it hands back is grudges, cultural mixes and the provinces each polity holds.

ROUND 5 IS PASS 2, THE ECONOMY PASS, 1560-1960. Metros grown from the centres pass 1 sacked, colonial reach across water, firms and their charters, the market carve and its price field.

AND THE CALENDAR IS NOW STATED, not derived: 4000 years to 1200, a deliberate coast to 1560, then 1560-1960. The epoch is 1960, which makes this an INDUSTRIAL-band campaign rather than the ancient one the 0 CE default produced.

**Planned.**
- WAVE 0 - MEASURE, and nothing is designed on top of it until it returns. BL-825 (four thousand years measured). The sim already runs on the region graph, the O(N^2) build is outside the year loop, and the stepped decision clock exists - so 4000 years may already be cheap and has never been run. If something is superlinear this names it, and that is what gets fixed, never the span.
- WAVE 1 - THE SIM. BL-826 (cultural mixes), BL-827 (grudges), BL-828 (pass one handoff), BL-823 (anti-hegemon levers), BL-822 (research from population). This is Ben’s "get the sim right first": the take-back before the surface that shows it.
- WAVE 2 - THE RECORD AND THE SHELL, which are independent of one another and run together. BL-817 (history playback record), BL-816 (wizard rounds four and five), BL-824 (begin becomes next).
- WAVE 3 - THE SURFACES. BL-829 (time-lapse view), BL-830 (top sixteen scoreboard), BL-820 (the pass runs in the round). Every one closes on a LIVE look, not a capture.
- WAVE 4 - PASS 2, THE ECONOMY PASS. BL-831 (pass two economy span), BL-832 (colonial ties), BL-833 (tariff posture from history), BL-819 (substrate round). BL-832 is what turns round 5’s reach-across-water placeholder into a real drawing.
- CARRIED, outside the waves: BL-812 (phase 6 sees roads), independent of everything here. BL-818 (history leans) sits after wave 3 - a lean is not worth offering until its effect is visible on the round’s own map.

**Done when.** The world wizard runs five rounds in the built app on a live click-through. Round 4 plays a full history span back as polity colour on the globe, with the boundary year marked. Round 5 draws metros, reach, firm markers and the market carve, with every placeholder visibly labelled as one. At least one lean per pass moves a measurable outcome across a seed sweep. And a timed run from menu to play has no stretch of more than a few seconds in which nothing on the globe changes. And pass 2 runs 1560-1960 as an economy pass, with colonial ties reaching the order book and tariff postures landing as ordinary import_tariff laws a player can read in the law panel.

**Risk.** THE ROUNDS DRAW OVER PASSES THAT ARE MID-REBUILD. Phase 4’s pass 1/pass 2 split and phase 6’s static search are both design-complete and largely unbuilt, and BL-770 (the search itself) was cancelled with the board clear. A surface built against today’s history_sim may be showing something that changes underneath it. The mitigation is the sprint’s own method — scaffold first, placeholders labelled, real content as each phase lands — but it is a real risk and not a small one.

DROPPING THE BUDGET CHAIN DROPS SOMETHING THAT WAS NOT ONLY BUDGET. BL-814 was titled as retiring the warm start, and that is a PHASE 6 restructure as much as a startup-time win: it promotes phase 6 to being the only judge of the position play opens on, and it named a specific known blocker (generate_corporations appends and runs before the registry loads). Deleted with the chain on Ben’s ruling; the substance is recorded here and in NR so it can be re-filed rather than rediscovered.

A WATCHED WAIT THAT IS DULL IS WORSE THAN A BAR. The bar at least admits it is a wait. If the playback has dead stretches the answer is to make them move, never to hide them behind a caption — and that judgement can only be made by opening the app and sitting through it.

AND THE PREVIEW MODEL DOES NOT TRANSFER. STARTUP.md’s standing promise that nothing is generated in the wizard — every control move a pure throwaway preview — is true of planetology and cannot be true of the history. The doc now says so explicitly; anyone reading only the old paragraph will build the wrong thing.

THE ERA BAND FLIPS, AND THAT IS WORLD-MOVING. epoch_year = 1960 puts the campaign in the industrial band (era_band_for_epoch flips at 1700) where the 0 CE default put it in the ancient one. Eras, the resource split and the unit roster all key on it. This owes the digest discipline: ONE deliberate re-bless against a stated description of what changed, never an absorbed drift.

AND THE BATCH IS LARGE - fifteen items in five waves. The barrier that protects it is wave 0: if 4000 years is not affordable, waves 1 through 4 are being planned against a span that does not exist, and the sprint is re-argued on the new number rather than carried on the old one.

THE BUDGET CHAIN IS DELETED, NOT ARCHIVED. BL-813, BL-814 and BL-815 were authored the same morning and never started, so the 2026-08-24 unstarted-plans-are-deleted policy applies exactly. Ben’s ruling on the visibility form was “drop it — a watched wait needs no budget”. BL-812 is NOT part of that chain and is kept.

THIS IS THE SECOND AUTHORING OF SPRINT 35 AND THE NUMBER DOES NOT MOVE. The first authoring landed the same day; nothing was built against it. Renumbering a sprint that delivered nothing would only cost the cross-references — see the 2026-08-27/28 round trip in § rulings for why a number is cheap to move and expensive to be wrong about.

EVERY UI ITEM HERE CLOSES ON A LIVE LOOK, not a capture. That is the standing rule for interactive surfaces, and it binds harder than usual on a sprint whose entire deliverable is “what it feels like to sit through generation”.

--- MOVED TO BATCH DELIVERY (Ben, 2026-09-08) ---

BARRIER SEMANTICS, per DELIVERY.md § Batch Delivery: every item clears a step before any starts the next. Step 4 is the load-bearing barrier - all tasks across all items reach a terminal state before any item is committed, and a blocked task is CANCELLED, not held. Step 4a runs verifier-review ONCE over the whole integrated set, because the failure it hunts is cross-slice. Commits stay one per item, back to back, after the review barrier closes clean.

THE DOC-COVERAGE MAP, determined up front as the method requires. Docs this batch CHANGES, one owner each and disjoint, so they fan out: GENERATION_STRATEGY.md (the passes, the levers, the take-back, the calendar - already written), ui/STARTUP.md (rounds 4 and 5 - already written), politics/RELATIONS.md (grudges as an input to sentiment - OWED, BL-827), politics/NATIONS.md (tariff posture - OWED, BL-833), lore/HISTORY.md (the epoch moves off 0 CE - OWED, BL-831), economy/ERAS.md (an industrial-band campaign - OWED, BL-831). Each changed doc gets a transient "what changed" note and a standing S-tier review item.

THE DOC THAT IS ALREADY WRONG AND IS THE FIRST THING TO FIX: lore/HISTORY.md carries the campaign epoch as 0 CE on Ben’s own NR-177 ruling. The epoch is now 1960. That is a doc and a ruling disagreeing with a later ruling, which is the exact case the newest-dated-wins rule exists for - but it must be EDITED, not left to the rule.

WHAT THIS SPRINT MUST NOT DO: start wave 1 before wave 0 returns a number. Every span decision in here is taken on a measurement nobody has made, and the whole reason wave 0 is one item and two points of difficulty is so that it cannot be skipped.

--- PAUSED 2026-09-08, AND WHY IT IS THE RIGHT CALL ---

Ben: "Let us pause development in this session, and write these into design documentation. Then we can resume stage 4 history generation in another session." See docs/development/NEXT_SESSION.md.

WHAT THE PAUSE IS ACTUALLY FOR. Waves 0-2 did what a batch is supposed to do - they measured before building and they found two things nobody knew. 4000 years costs 6-9x per year what 400 does, and reach is 66-86% of it (BL-834). And the sim battle count is one dead region flipping 258 times, not a history (BL-835). The second finding then pulled the whole stage 4 model open: population is civilian, armies are distinct, no total warfare - which is a bigger change than anything this sprint planned, and it is upstream of the time-lapse and the scoreboard that were meant to show it off.

SO THE SURFACES WAIT ON THE SIM, DELIBERATELY. Building the time-lapse now would have rendered an artefact beautifully. The map would have shown one border flickering 258 times and it would have been the most visible thing on screen.

FIVE ITEMS ARE NEW SINCE THE SPRINT OPENED and belong to stage 4 rather than to this sprint: BL-834 reach cache, BL-835 civilian population, BL-836 winter tie, BL-837 logistics and roads, BL-838 fear of being next, BL-839 turbulence lean. BL-837-839 are marked sprint 36 already.

STILL OWED ON WHAT LANDED: the step 4a review barrier has never run. One verifier-review pass across the whole integrated set, before any of it is called finished.

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
| 35 | Watching the world be made | PAUSED 2026-09-08 mid-batch on Ben call: get the design down first, resume stage 4 in another session. Waves 0-2 landed and are verified; the stage 4 sim design was settled the same day and is much larger than the sprint assumed. |

**Next up.** SPRINT 35 IS PAUSED (2026-09-08), not closed. Waves 0-2 landed and are verified in the main session: the wizard runs five rounds with labelled placeholders, the 4000-year span is measured, and culture shares, grudges and the handoff type are built. Two findings redirected the sprint - reach is 66-86% of a long run and rebuilds 12,000 times (BL-834), and the sim war is one dead region flipping 258 times (BL-835). Ben then settled stage 4 model: population is civilian, armies are distinct, no total warfare; the scorer asks can I keep it and will others fear me; turbulence is the round 4 lean. RESUME AT docs/development/NEXT_SESSION.md. The step 4a review barrier is still owed. THE NEXT NEW SPRINT IS 36.

**The standing debt out of P1**, worth repeating here because it spans four items: nothing built in that sprint was ever *rendered*. The session ran in a container that cannot build the GUI, so every UI half is compile-clean and arithmetically checked and visually unseen, and no golden was blessed. For a sprint whose own method note is *build it, look at it, then rule*, that is the thing to fix first.

*52 sprints archived cold; 1 open/gated in the hot store.*
