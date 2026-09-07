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

### Sprint 33 — Context economy - the corpus stops charging every session for what one session needs
*Open · opened 2026-09-07*

**Goal.** A session pays for what it reads, not for the corpus. The backlog hot/cold split already does its job - backlog_query.js unions the archive deliberately, so "is this built?" stays answerable while backlog.json keeps meaning exactly one thing. The cost that remains is elsewhere: ~650K tokens of authority docs with no summary layer, so traversal opens whole docs to discover it wanted a different one; --full prose now resolving against 1.9MB of archived designs; and CLAUDE.md plus the standing rules loading in full for a one-line doc tweak, the AI-behaviour grant history included. The felt goal is that a Light-mode session costs Light-mode context.

**Planned.**
- BL-792 (the cold union misses 620 items) - FIRST, and it is priority A. Surfaced while delivering BL-790: archive_store.js globs backlog-design-*.json only, so the union sees 138 items of the 762 in the archive directory. 420 completed items are invisible to --touches, the tool CLAUDE.md names as the way to answer "is this built?". The session that OPENED this sprint answered that the union keeps the question answerable; it does not, today.
- BL-787 (doc summary headers) - a 10-line "what this doc settles" block at the head of every authority doc, so traversal can read headers until it knows which doc it actually needs. The highest-value item: it attacks the 650K directly.
- BL-788 (backlog query summary mode) - --summary on backlog_query.js/requirements_query.js: row, short_name and a one-line precis, never full prose. --full becomes opt-in for the one item being built.
- BL-789 (standing rules split by mode) - the AI-behaviour grant history is load-bearing only when touching corp_ai.cpp. Move it to its authority doc, leave a one-line pointer, and every Light session stops paying for a dozen dated precedents.
- BL-790 (source-to-doc index) - --touches answers "what work touched this doc"; nothing answers "which doc owns this file". A generated src/ -> doc index lets traversal start from the code.
- BL-791 (fan-out as compression, in DELIVERY.md) - record the practice already in the method and underused: a sub-agent reads 40K and returns 500 tokens. The main session's context is the scarce resource, not the agent's.

**Done when.** Every authority doc in CLAUDE.md section 3 carries a header block; backlog_query.js defaults to summary output; the standing rules fit on one screen with the grant history reachable in one hop; a src/ -> doc lookup exists and is named in CLAUDE.md; DELIVERY.md states the fan-out practice. No src/ change in the whole sprint.

**Risk.** A summary header is a second place the doc says what it is, and a second place is a place to drift. Headers must be derived-feeling and short enough that a doc edit obviously touches them; if a header can be stale without looking stale, it is worse than nothing. The standing-rules split has the sharper risk: the AI-behaviour grants exist BECAUSE they were raised rather than assumed (the NR-517 precedent), so moving them must not make the next widening easier to take quietly - the pointer has to read as a gate, not a footnote.

FILED 2026-09-07 out of the session that asked whether querying both stores defeats the archive. Answer recorded there and not repeated: the archive keeps the HOT FILE's meaning clean, the union keeps the QUESTION answerable, and those do not fight. This sprint is the other half - the creep that is real. The previous sprint 33 (long-term market viability) is renumbered 34 and is untouched. AMENDED 2026-09-07: BL-792 joins the sprint and leads it. The opening answer needs its correction recorded - the hot/cold SPLIT is sound and the union is the right design, but the union as implemented is incomplete, and that is a defect rather than a context cost.

### Sprint 34 — Long-term market viability, the growth half - the field that keeps producing
*Open · opened 2026-09-02*

**Goal.** A field that keeps producing over thirty years. Sprint 31 ended with a majority of corps operating-positive and debtors a tenth of the field - and valued production still falling across the run (x0.2 on seed 0, x0.6 on seed 1 from a level ten to twenty times the old baseline), with most buildings sitting at the new supply floor for want of power and the remaining debt entries being processors that buy at the ceiling and convert at a loss. The felt goal is unchanged from sprint 31 - every player can make a steady profit - with the emphasis moved from "not going broke" to "still growing at year 30".

**Planned.**
- BL-746 stage 2 (the generation bootstrap, NR-782 (c)) - FIRST. The field's mean supply factor sits at ~0.57: most buildings run at the floor because power does not arrive (generation 18 -> 3 units against a demand of 64; a generator short of power throttles itself; only network-reached tiles can receive it). Design the bootstrap - generators exempt from the power draw, a building drawing power only once its catchment market has priced it, and whatever makes generation get BUILT - and measure it on the debt instrument: the done-when is the mean supply factor climbing off the floor toward 1.
- BL-745 (processor input bid cap) - the remaining debt entries. 42 of 57 are processors producing less than they buy: construction materials at 8-10x base through the boom (free materials moved the median entry tick 22 -> 34) and inputs bought above the recipe's output value. The anchor's M1 identity carried to the live tick, at the one shared seam, and the scorer's build tempo pricing materials at the live market.
- BL-738 (industry rates go live) - re-measure the goods-upkeep rates against the floored world: with the floor and the no-wire rule the draw is a real cost again, not a death sentence, and its size should be set by what the field can supply.
- BL-726 (sweep debt dynamics) - seed 1 still shows interest at 70% of net loss at year 30 while seed 0 is at 5%; the spiral is half-tamed. Interest rate, the balance floor, the exit thresholds, on the standard lapse.
- BL-725 (sweep price levers) - the ceiling derivation clears again (ceil > 8.84 against 10.0); the sweep re-runs on the anchored tables once the two levers above have moved the field, not before.
- The instrument: campaign_lapse --epoch 1960 in both forms (--warm 0 --ticks 60 for where debt begins; the standard 80/120 for the done-when), with the debt.csv classification (starved / loss-converting / unsold) as the per-entry verdict.

**Done when.** On the standard industrial lapse, both seeds: valued production at quarter 200 at or above quarter 81 (the field grows or holds), a majority of corps operating-positive (kept from sprint 31), the mean supply factor above 0.8, and interest below a quarter of net loss on every seed.

**Risk.** The generation bootstrap is a design change to a channel that was authored to be systemic (power as a grid good); exempting generators or gating the draw on a priced market could hide the very scarcity the grid is meant to express. Measure the supply factor trend and the power price together, so a fix that only silences the draw reads as one. And every retune moves the goldens and the sweep baselines - one deliberate re-bless with provenance, not a dribble.

THE BASELINE THIS OPENS ON (2026-09-02, final-ind-s0/s1, standard lapse): corps 86 -> 61 / 71 -> 55; debtors 13 -> 6 / 7 -> 4; op-positive at 200: 46 of 61 / 31 of 55; median op net +16.9 / +1.7 per quarter; median balance 4,424 / 3,797; active buildings 224 -> 140 / 122 -> 85; valued production 20,761 -> 4,364 / 5,775 -> 3,531; convoys 156 -> 131 / 194 -> 79; interest share of net loss 5% / 70%; mean supply factor ~0.57. Sprint 32 is not skipped by accident: Ben named this 33. RENUMBERED 33 -> 34 on 2026-09-07 (Ben) to free 33 for the context-economy housekeeping sprint; nothing about the goal, baseline or planned set changed.

### Sprint 32c — Gamified generation, 32c - the water model finishes, and phase 6 gets its search
*Open · opened 2026-09-06*

**Goal.** FINISH WHAT 32b STARTED AND LEFT STANDING. Two chains, and they are independent of each other:

THE WATER MODEL REACHES ITS JUDGEMENT POINT. BL-776 and BL-777 landed - coastal water and lakes are
owned, open ocean is not, and no region anchors on open ocean any more. BL-778 (unit traversal
domains) and BL-779 (naval rows become real) remain, and then BL-780 re-blesses ONCE. That re-bless
is the sprint most important single act, and it now carries FOUR attributed causes rather than one
- the water carve, the population map, the empire forces and the paleo deposits all moved the world
inside 32b. Every one of them measured its own before/after in isolation, so the causes stay
separable even though the tree digest is a fifth value belonging to none of them.

PHASE 6 GETS ITS SEARCH. The objective can finally see a roster (BL-770 slices 1-2), which is the
precondition everything downstream waited on. What is missing is candidate generation, parallel
evaluation and a deterministic argmax - landscape_score.cpp still has no caller outside its own
harness. Building it unblocks BL-772 (retire the warm start, the 72-73 s Release win) and BL-773
(the 3-6 minute budget).

**Done when.** The water model is complete and the 0 CE digests have been re-blessed EXACTLY ONCE, with Ben
authorising against a stated description of what changed in the world SHAPE - FOUR named causes,
not a hash. A land stack cannot enter unowned water but can cross coastal water its polity owns; a
naval row contributes real power.

AND phase 6 runs a real search: candidates evaluated, a deterministic argmax with an explicit
tie-break, the winner identical across thread counts, and landscape_score called from generation
rather than only from its harness.

**Risk.** BL-780 IS THE RISK, and it is a judgement risk rather than a technical one. It was designed as the
single point where the WATER model moves the world once and a human asks whether the new world is
BETTER. 32b carve put three reorder items alongside it, so the movement is now four causes
superposed. The mitigation is real but partial: every item measured its own before/after in
isolation, so attribution survives. What does NOT survive is the simplicity of the question.

AND THE CENTRAL CLAIM CANNOT BE SEEN (NR-791). The hover card over water reports terrain and
habitability, clicking water selects nothing, and no lens colours territory by owner. BL-780 asks
for a judgement by looking at a change that is currently invisible. Close that before the re-bless,
not after.

THE BUILDER GAP KEEPS COSTING. Three fixes landed in 32b and the first two verifications were run
in the main checkout, where the bug could not appear. Any tooling fix for worktree agents must be
tested FROM a worktree or it is not tested.

Carried from 32b: 28 open items. The two ad-hoc harness families still want Ben naming them as skills (NR-788, now five: continent_drift, sim_water_census, the saturation measure, deposit_origin, landscape_score_harness - plus centre_region_bind from BL-783). Open calls waiting: NR-783 (span boundary), NR-784 (cap the ancient arc), NR-785 (three sea-leg calls), NR-787 (stagnant lid), NR-790 (fossil epoch derivation), NR-791 (coastal ownership invisible), and BL-758.

THE SESSION OF 2026-09-06 PIVOTED TO DOC RECONCILIATION BEFORE ANY BUILD, on Ben's call, and it was the right order. Scoping the two chains found THREE PAIRS OF AUTHORITY DOCS CONTRADICTING EACH OTHER - all three because a 2026-09-06 ruling landed in one doc and not its siblings. MILITARY.md contradicted ITSELF (naval strategic-only vs its own domains section); TILE_GENERATION.md still carried the exact deferral PROVINCES.md quotes as lifted; CORPORATION_GENERATION.md Pass 6 still recurred through a settle GENERATION_STRATEGY.md retires the same day. Briefing implementers off those docs would have built the contradiction into code.

EIGHT CALLS TAKEN on an elicitation form and written into the owning docs: water forage (adjacency to owned shore, not a flat zero), buildings on water (a port, nothing else), realisation as a FOURTH scored term, candidate generation as greedy refinement over fixed rounds, the wave re-bless rule into DELIVERY.md as method, water tiles selectable, Pass 6 density is the cap (the 0.90 ratio that never bound is retired), and the five ad-hoc harnesses named in the verifier-headless skill. BL-656 - design-owed since 2026-08-26 - was closed by the density ruling.

SIX FURTHER CONFLICTS NEEDED NO RULING and were resolved on newest-dated-wins or on the archive rather than being put to Ben.

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
| 32a | Gamified generation, 32a - the arc runs, and the instruments that measure it are honest | CLOSED 2026-09-06 at a natural boundary. Five items delivered - the two-span sim, the sweep that measures the real run, the continent time axis, the tick-length audit, and the saturation measure promoted where generation can call it. The remaining 29 carry to 32b, led by the water model. |
| 32b | Gamified generation, 32b - the water model, and the reorder that 32a built the instruments for | CLOSED 2026-09-06. ELEVEN ITEMS DELIVERED in three waves - the market batch (BL-774, BL-759, BL-760, BL-770 slices 1-2), wave 1 (BL-776, BL-766, BL-767, BL-748, BL-762, BL-764 slice 1) and wave 2 (BL-777, BL-783, BL-765, BL-750, BL-769, BL-768, BL-754 partial). The world CHANGED, on purpose, and the digests are moved and DELIBERATELY UNBLESSED - BL-780 owns that and now carries four attributed causes rather than one. The remaining 28 carry to 32c. |
| 33 | Context economy - the corpus stops charging every session for what one session needs | OPENED 2026-09-07 and its first block ran the same day: five items delivered in one batch (BL-787 doc headers across 71 docs, BL-788 query summary mode, BL-789 the grant register moved out of the always-on rules, BL-790 doc_owner.js, BL-791 the fan-out paragraph). What remains is what the batch FOUND - five defect items, two of them priority A. |
| 34 | Long-term market viability, the growth half - the field that keeps producing | OPENED 2026-09-02 (as sprint 33; renumbered to 34 on 2026-09-07) on Ben's call as sprint 31's second half. Sprint 31 made the field solvent; this sprint makes it grow. The instrument is campaign_lapse with its debt columns, and the two levers are already filed. |
| 32c | Gamified generation, 32c - the water model finishes, and phase 6 gets its search | OPENED 2026-09-06 as 32b continuation. 28 items carried. Two chains lead: the water model to its single re-bless (BL-778 -> BL-779 -> BL-780), and phase 6 to an actual search (BL-770 -> BL-772 -> BL-773). |

**Next up.** SPRINT 32a CLOSED 2026-09-06 (5 of 34 delivered - the arc runs and its instruments are honest). SPRINT 32b IS OPEN and carries the remaining 29, led by the water model. SPRINT 33 IS NOW THE CONTEXT-ECONOMY HOUSEKEEPING SPRINT (opened 2026-09-07); the long-term market viability sprint that held that number is RENUMBERED 34 and is otherwise unchanged. THE NEXT NEW SPRINT IS 35.

**The standing debt out of P1**, worth repeating here because it spans four items: nothing built in that sprint was ever *rendered*. The session ran in a container that cannot build the GUI, so every UI half is compile-clean and arithmetically checked and visually unseen, and no golden was blessed. For a sprint whose own method note is *build it, look at it, then rule*, that is the thing to fix first.

*47 sprints archived cold; 3 open/gated in the hot store.*
