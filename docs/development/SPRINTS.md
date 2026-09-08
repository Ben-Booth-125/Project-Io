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

**Sprint 33 deleted, and 32a/32b/32c renumbered to 32/33/34 (Ben, 2026-09-08).** Sprint 33 (long-term market viability, the growth half) was opened 2026-09-02 and never executed: all five of its planned items were cancelled unstarted on 2026-09-07, leaving an open sprint with zero live items. It was deleted outright under the 2026-08-24 unstarted-plans rule rather than archived. Its number was re-taken the same day by the renumber, so the six cancelled rows that carried `sprint: "33"` now read `"33 (2026-09-02)"` — the date-qualified form this store already uses for colliding keys — and three live prose references were qualified in place. **A bare "sprint 33" written before 2026-09-08 in closed prose** (the sprints archive, backlog-design, the DEVLOG) **means the DELETED market-viability sprint**, not the water-model sprint that now holds the number. The renumber itself: the gamified-generation arc ran as 32a/32b/32c because it was authored as one sprint and split twice in flight. Ben collapsed the suffixes to plain 32, 33 and 34 once all three had closed. The suffixed form was a symptom of one theme carrying six independent bodies of work, which is what this renumber and sprint 35 exist to stop repeating.

**Sprint-number ceiling advanced to 35 (Ben, 2026-09-08: “We will open sprint 35 looking at the startup budget”).** The 2026-08-24 ceiling stands in spirit — author no sprint past the one Ben opens — with 35 now the horizon. **One** sprint is open, well inside the three-sprint cap. Four further strands were scoped in the same session and deliberately **not** authored as sprints: authoring five at once would have re-created the sprawl that split sprint 32 three ways, and an unstarted plan is a stale reference by this store's own rule. They live as backlog items, with the grouping recorded in sprint 35's notes.

## Open now

### Sprint 35 — The startup budget - what the player actually waits through
*Open · opened 2026-09-08*

**Goal.** THE STARTUP WAIT BECOMES A NUMBER THE PROJECT CHOSE, rather than one nobody had looked at. Measured 2026-09-03 in the release play build: the warm start is 72-73 SECONDS on both arcs, against the ~6 s app.cpp's own comment budgets for it - 12x. Generation itself is ~8 s. So the Era -1 sim this whole arc has been optimising (197-323 ms) is under half a percent of what the player waits through, and every conversation about whether generation can afford more complexity was being had about the wrong number.

THE SHAPE. Retire the warm start (BL-772) and hand its burden to phase 6, whose search finally has a generation caller as of sprint 34. Keep the arithmetic honest against Ben's 3-6 minute target as each piece moves (BL-773). And make the game state its own budget on the generating screen (BL-754), because a budget nobody can see in the game is not a budget.

WHY THIS STRAND FIRST. It is the only one of the five whose done-when the player can feel, its blocker cleared in sprint 34, and it is the largest single win available anywhere in the project - roughly 72 seconds.

**Planned.**
- BL-761 (warm start is 72 seconds) - FIRST, because it is the profile and everything else is a decision taken on it. run_economy_step dominates both arcs (57.0 s industrial / 42.4 s ancient) and convoys are the reverse (14.8 / 28.3). That is 530-710 ms PER TICK, which is also what the player pays per tick at speed once playing - a play-speed problem wearing a loading-screen costume. Profile before cutting.
- BL-772 (retire the warm start) - the headline, and its 2026-09-06 blocker is GONE: landscape_search now has a caller in app.cpp. What remains is the restructure the sprint 34 retro named - generate_corporations appends and runs BEFORE the registry loads, so phase 6 cannot vary the specialist roster at the live seam. That is the actual work, and it is now written down rather than guessed at.
- BL-773 (generation budget 3-6 min) - keeps the arithmetic true as each piece lands. Phase 6 already costs +20 s of measured startup, which is a regression until BL-772 removes the 72 s beside it.
- BL-754 (generation budget) - the ON-SCREEN half, still owed. The console half landed 2026-09-06 and was proved; R1 is deliberately not marked complete because nobody has opened the app and looked at the generating screen. A live look closes it.

**Done when.** The pre_game_ticks loop is gone, a world handed to play is settled by phase 6, and startup drops by roughly 72 seconds measured in the release play build on BOTH arcs. Total generation sits inside Ben's 3-6 minutes. And the app prints its own per-phase budget on the generating screen - confirmed by opening the built game and looking at it, not by a clean compile.

**Risk.** RETIRING THE WARM START HANDS PHASE 6 A BURDEN IT MAY NOT BE READY FOR. The warm start exists so play opens on a settled position rather than a cold one; phase 6 has to hand over a world in that same state. Sprint 34 already refused this trade once, correctly - the implementer took the block rather than trading correctness for a wall clock, and that judgement should hold again if the restructure does not land clean.

AND THE SEARCH IS PARTLY BLIND. NR-793 (open, confirmed on a live world) says road tier is INVISIBLE to the phase 6 objective, because the resource-coverage boolean is saturated. Handing the settle burden to a scorer that cannot see one of its inputs is the risk worth stating up front. It is the reason the roads strand is the strongest candidate to run next, and it may prove to be a precondition rather than a neighbour - decide that on a measurement, not in advance.

THE CALENDAR REBASE MUST NOT BE LOST. Play opens at epoch_year and the opening position owes the calendar nothing (BL-369). ERAS.md carries that as the load-bearing, pass-independent half, and it survives whatever replaces the warm start.

AND EVERY NUMBER HERE IS A RELEASE-BUILD NUMBER. The 72-73 s was measured in build_rel with --autostart; the harness path and the Debug build are different worlds. Quote the build with the figure or the figure means nothing.

THE BASELINE THIS OPENS ON (2026-09-03, release play build, --autostart):
  epoch 1960 (two-span)  warm_start_80_ticks  73167 ms | convoys 14763.9 | run_economy_step 57023.9
  epoch 0    (ancient)   warm_start_80_ticks  72088 ms | convoys 28321.1 | run_economy_step 42361.6
Generation end to end ~8 s in the Release harness path; clear_markets ~0.11 s, apply_budget ~0.77 s and history_recorders ~0.39 s are all noise beside it.

THE OTHER FOUR STRANDS sprints 32-34 were carrying are NOT authored as sprints, and deliberately so - the ceiling ruling stands, and an unstarted plan is a stale reference. They remain backlog items, and the grouping is recorded here so it is not re-derived from scratch: ROADS MEAN TRAFFIC (BL-784 one nation roaded solid, plus NR-793); INSTRUMENTS (BL-753 generation scoreboard, BL-758 1960 demography, BL-781 query ignores unknown flags); MARITIME CLOSE-OUT (BL-786 port on owned coastal water, BL-749 sea-leg campaign, BL-752 colonial ties - the middle one held on three NR-785 calls); DEEP TIME (BL-764 the Lagrangian frame, difficulty 5, splits at promotion).

BL-758 (1960 demography) IS STILL A CALL FOR BEN, not a build. It has been red since 2026-09-03 and the two readings are recorded on the item.

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
| 31 | Long-term market viability - every recipe pays at base price | CLOSED 2026-09-02 on Ben's call, stage one a SUCCESS: the field ends the standard thirty-year lapse with a majority of corps operating-positive (46 of 61, 31 of 55) where it began with four and none, debtors a tenth of the field, median balances climbing, buildings running. The growth half - valued production still declines over the run - is sprint 33 (2026-09-02), deleted unstarted 2026-09-08. |
| 32 | Gamified generation, 32 - the arc runs, and the instruments that measure it are honest | CLOSED 2026-09-06 at a natural boundary. Five items delivered - the two-span sim, the sweep that measures the real run, the continent time axis, the tick-length audit, and the saturation measure promoted where generation can call it. The remaining 29 carry to 33, led by the water model. |
| 33 | Gamified generation, 33 - the water model, and the reorder that 32 built the instruments for | CLOSED 2026-09-06. ELEVEN ITEMS DELIVERED in three waves - the market batch (BL-774, BL-759, BL-760, BL-770 slices 1-2), wave 1 (BL-776, BL-766, BL-767, BL-748, BL-762, BL-764 slice 1) and wave 2 (BL-777, BL-783, BL-765, BL-750, BL-769, BL-768, BL-754 partial). The world CHANGED, on purpose, and the digests are moved and DELIBERATELY UNBLESSED - BL-780 owns that and now carries four attributed causes rather than one. The remaining 28 carry to 34. |
| 34 | Gamified generation, 34 - the water model finishes, and phase 6 gets its search | CLOSED 2026-09-07. GOAL MET ON BOTH CHAINS. The water model is complete and blessed ONCE against a stated shape description; phase 6 has a real search WITH a generation caller. Nine items delivered. The remaining 28 were cut to 12 on Ben's call - sprint 29, sprint 33 (2026-09-02, since deleted) and ownerless items archived unstarted, to be revisited with a narrower focus. |
| 35 | The startup budget - what the player actually waits through | OPENED 2026-09-08 on Ben's call, as the first of the five strands sprints 32-34 were carrying at once. One number: the wait between pressing new game and playing. Four items, one measurable done-when. |

**Next up.** SPRINT 35 IS OPEN (2026-09-08) and is the only open sprint: the startup budget — the 72-73 second warm start, retiring it into phase 6, and the game stating its own generation budget. Its blocker cleared in sprint 34, when `landscape_search` finally got a generation caller. SPRINTS 32, 33 AND 34 ARE CLOSED — the gamified-generation arc, renumbered from 32a/32b/32c on 2026-09-08. **Sprint 33 as it existed before that date** (long-term market viability) **was deleted**, unstarted. Eight backlog items remain outside sprint 35, grouped into four strands in that sprint's notes — roads, instruments, maritime close-out, deep time. **Roads is the strongest candidate to run next**, because NR-793 says the phase 6 objective cannot see road tier, and sprint 35 hands that objective more work. THE NEXT NEW SPRINT IS 36.

**The standing debt out of P1**, worth repeating here because it spans four items: nothing built in that sprint was ever *rendered*. The session ran in a container that cannot build the GUI, so every UI half is compile-clean and arithmetically checked and visually unseen, and no golden was blessed. For a sprint whose own method note is *build it, look at it, then rule*, that is the thing to fix first.

*48 sprints archived cold; 1 open/gated in the hot store.*
