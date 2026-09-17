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

### Sprint 43 — the 1960 baseline
*Open · opened 2026-09-17 · Ben (2026-09-17, elicitation: "Sprint 43 items; 44-45 as goal rows"); Claude (cut from a seven-lane engine read with an adversarial cross-check)*

**Goal.** Take every Digitisation reading, per seed, on the shipped world continued to 1960, beside a 1660 control, with the span's wall-clock cost and both inherited weaknesses counted, before any Digitisation mechanism is admitted.

**Planned.**
- BL-1026 (seed library re-read) — fingerprints re-blessed from a library-seed sweep; replacement candidates for seeds 4, 6, 12, 13 brought to Ben.
- BL-1027 (span cost to 1960) — exploration_sweep --through; per-seed wall clock and profile split for each span half; a saturation line.
- BL-1028 (weakness counters to 1960) — contact, binding, alarm saturation, displacement, the contact funnel, per half; digests unmoved.
- BL-1029 (Digitisation readings at 1960) — digitisation_sim_harness --through with a fingerprint-checked 1660 control; honest SUMMARY and structural zeros.

**Done when.** All four items terminal. The sixteen library seeds read at 1660 and 1960, per seed, with cost; world_determinism and the goldens bit-identical; a NEEDS_REVIEW decision entry puts W1 (displacement against a saturated alarm) and W2 (polities rarely meet) to Ben with the readings, and a second offers seed replacements.

**Risk.** "Through 1960" means exploration_stop_year = 1960 at epoch 0, never epoch_year = 1960, which selects the superseded two-span arc with Exploration off. The stretched span runs Exploration's forces only, so a frozen or saturated 1960 world is an inherited finding, not a Digitisation result. BL-1028 may add pure counters to history_sim; a moved digest means one touched a decision. Readings 3 and 8's region half stay structural zeros whatever the span, and reading 3 cannot move until the epoch flip decouples arc from recipe band.

THE PLAN OBEYS WHAT BROKE THE LAST ONE (archived 2026-09-16 as "highly unstructured"): one goal per sprint; work ordered by property, not by prerequisite depth across the whole backlog; the root before its leaves; every reading on one world config; any save-format or difficulty-5 work broken down and run alone with a cold review; an open design question gets its own measure item before its mechanism; timing work on a quiet machine, never beside a world-mover.

RULINGS THAT SHAPE IT (Ben, 2026-09-17, elicitation): the seed library is re-blessed and its four lost seeds replaced rather than reworded; the weaknesses are measured here and ruled at the gate; neither blocks sprints 44-45.

### Sprint 44 — the corporate web's plumbing
*Proposed · Ben (2026-09-17, elicitation: "Sprint 43 items; 44-45 as goal rows"); Claude (cut from a seven-lane engine read with an adversarial cross-check)*

**Goal.** A per-centre charter budget reaches the landscape search and charters specialists and background firms around each centre, with the path off by default and its live-play cost measured before any conversion constant is chosen.

**Done when.** An empty budget reproduces today's world byte for byte, proven by a determinism check, not a re-bless. A synthetic budget at 1x, 2x and 4x charters specialists and background firms around centres through the existing anchor_window seam, with unplaced budget counted. Milliseconds per economy tick and per search evaluation reported at each density; seat shortlist size and trailing-net spread reported, not gated. No save-format change.

**Risk.** Background firm placement is population-blind today (corporation_generation.cpp:2366-2374), so density following the city is a new placement rule, not a tuning change. The seat reads specialists, and the budget charters them (Ben, 2026-09-17), so the seat shortlist moves with the budget: NR-886 item 3 (solvency no longer gates the seat) sits on this path. The integration seam is two-sided — app.cpp and harness_params.hpp change together, and the direct callers of the firm pass (verify_api.cpp, main.cpp, three harnesses) must name the legacy path.

RULINGS (Ben, 2026-09-17, elicitation): the budget charters the whole web, specialists included (DIGITISATION.md § 1); the path stays off until Beat 1's industry points switch it on, with no stand-in source in between. OPEN, measured here rather than argued: budget per firm, the split between specialists and background firms, whether a budget is spent whole.

### Sprint 45 — industrialisation makes the web real
*Proposed · Ben (2026-09-17, elicitation: "Sprint 43 items; 44-45 as goal rows"); Claude (cut from a seven-lane engine read with an adversarial cross-check)*

**Goal.** The Digitisation span runs 1660 -> 1960 from the exploration_output struct, the Industry tree is invested in, large cities accumulate located industry points, and each centre's unspent points become the charter budget that sprint 44's path spends, so property 1 reads density following cities as a consequence.

**Done when.** Density-follows-cities and industrialisation readings taken at 1960 per seed on the sprint 43 harness; the charter budget is read only from the stockpile; one re-bless authorised by Ben against the shape; a cold review passed with its fix round budgeted.

**Risk.** A span resumed from the struct is not mechanism-free: consolidation fires on the round where year == start_year, near-home classifies contacts by first.year < start_year, and the resume path carries only polities, grudges, contacts and corridors, so treaties, tribute and trade flows drop at 1660 unless a resume path is added (history_sim.cpp:888-916, 870, 1481-1497). Wiring the Industry tree follows BL-930 (exploration tree) and BL-973 (tree effects) across the store, lint, generator, tree_effect.hpp and history_sim; the Works-fork lean needs new effect keys in three synchronised lists. Moves every shipped world the moment the budget turns on.

OWED WHEN THE ITEMS ARE CUT, not before: whether consolidation and the near-home cutoff re-anchor at 1660; the resume path for dated objects and trade flows; the span's clock (DIGITISATION.md names none, and sprint 43 measures at Exploration's 4-year band); whether the three sinks (rail, mechanised force, works) come in the first cut or points only stockpile. The epoch flip (NR-869) stays the phase's done-when, and it must select this span, never the superseded arc (DIGITISATION.md, opening: a 1960 epoch names a calendar).

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
| 43 | the 1960 baseline | OPEN 2026-09-17. Measure before mechanism: four harness items, one lane, no shipped world moves. Gate at close: Ben rules the two inherited weaknesses on per-seed readings. |
| 44 | the corporate web's plumbing | PROPOSED 2026-09-17. Items cut at the sprint 43 gate. A per-centre charter budget reaches the landscape search, off by default; no world moves. |
| 45 | industrialisation makes the web real | PROPOSED 2026-09-17. Items cut after sprint 44. Digitisation runs as its own span from exploration_output, cities accumulate industry points, and sprint 44's budget switches on — one re-bless, one cold review. |

**Next up.** SPRINT 43 OPEN (2026-09-17): the 1960 baseline — BL-1026 (seed library re-read), then BL-1027 (span cost to 1960), BL-1028 (weakness counters to 1960), BL-1029 (Digitisation readings at 1960), serially in one lane. Sprints 44 (the corporate web's plumbing) and 45 (industrialisation makes the web real) are proposed goal rows; their items are cut at the gates.

**The standing debt out of P1**, worth repeating here because it spans four items: nothing built in that sprint was ever *rendered*. The session ran in a container that cannot build the GUI, so every UI half is compile-clean and arithmetically checked and visually unseen, and no golden was blessed. For a sprint whose own method note is *build it, look at it, then rule*, that is the thing to fix first.

*61 sprints archived cold; 3 open/gated in the hot store.*
