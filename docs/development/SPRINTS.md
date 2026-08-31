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

### Sprint 21 — The other half of the economy - demand
*Paused · opened 2026-08-26*

**Goal.** Author the demand side. The roster grew a supply chain and never grew the demand for it, so the ancient economy terminates in artisan goods nobody buys and most spawns are structurally unprofitable - not mistuned, unbought. Eight demand channels (MARKETS.md § Demand channels), of which the load-bearing ones scale with the economy rather than with an authored weight. Then MEASURE, and expect to go round more than once: Ben, 2026-08-26 - 'we will get interesting data and do a few passes over viability'. The felt goal: a world where the goods a player is meant to chase are wanted by somebody, and it is legible WHO.

**Planned.**
- BL-648 (admission rule names an injector) - FIRST. The guard that stops this recurring. Expect it RED on ten goods the day it lands; the rest of the sprint turns it green
- BL-649 (demand census) - FIRST, with it. The instrument: per resource, per band, who actually wants it. Run before and after every pass, deltas kept
- BL-640 (era-banded household basket) - wave 1: the basket ladders by era as well as stratum; resurrects four of the seven ancient dead ends
- BL-641 (building upkeep in goods) - wave 1: the largest structural sink. A unit already pays credits + goods; a building pays credits only
- BL-642 (construction actually draws) - wave 1: the opening years build, and centres draw materials as they grow
- BL-644 (space programme budget line) - wave 2: the tenth line, and space goods' first buyer that is not a militia contract
- BL-647 (endemic luxury demand) - wave 2: tobacco, spices, coffee and furs, wanted by wealth and flavoured by national character
- BL-643 (network upkeep draws) - wave 2: a sink scaled by geography rather than population
- BL-646 (battles burn ordnance) - wave 2: the only channel that couples the two pillars - Conflict moves Trade
- BL-645 (research consumes goods) - GATED on BL-619, which is a design session with Ben, not a build slice
- VIABILITY PASSES - after each wave: census, measure spawn viability, retune, repeat. The passes are the sprint, not an epilogue

PAUSED 2026-08-28 on Ben's instruction, wave 0 complete (BL-648 guard, BL-649 census). Nothing is abandoned: the guard is deliberately red, the census is the before/after instrument, and wave 1 resumes where it stopped. Opened on Ben's instruction, 2026-08-26 ('A, and open it as sprint 21'), after the session's own measurement answered his question: the ancient band has TWO live demand sinks, and ten goods pass the orphan check by naming a 'mercantile demand' that grep says was never built (NR-671). Ceiling advanced to 21 on the same instruction. ORDERING IS LOAD-BEARING: BL-648 and BL-649 go FIRST, not last. The guard makes the gap visible and the census makes each pass measurable - without them a viability pass is a guess with a number attached. A red guard with a named list is worth more than a green one that means nothing; do not weaken it to pass while the channels are being built. SEVERAL CHANNELS ARE DESIGNED-BUT-INERT rather than missing, which is why the sprint is cheaper than it looks: `strategic_reserve` is already a goods-buying budget line no consumer claims on; `logistics_maintenance` already names network upkeep; recipes already carry the era field the demand baskets need; and `run_unit_upkeep` is already the credits-plus-goods shape BL-641 wants for buildings. RELATIONSHIP TO SPRINT 20: Sprint 20 stays open and owns the ledger, the buyout and the spawn shortlist. Its BL-634 (acquisition viability) CANNOT honestly pass until this sprint lands - a corp at -28/qtr never saves up for anything - so BL-634's measurement is the natural close-out for BOTH sprints. BL-630's shortlist gains a real mechanism to gate on once demand exists: BL-635 measured that a spawn's survival currently depends on whether the generator handed it a resource anyone wants.

### Sprint 25 — UI visibility - batch 5: canvases & the zoom ladder
*Proposed · opened 2026-08-28*

**Goal.** Review the three canvases and the ladder between them - Solar, Circumplanetary, Planetary - plus the render layers that sit on them (tiles, terrain, roads, settlements, fog, borders, provinces) and the keyboard navigation model.

**Planned.**
- UI-001..UI-024, UI-094..UI-097 in the catalogue
- The body-to-body lenses deferred from batch 1 (Supply, Supply-routes, Reach) - Ben, 2026-08-28: "I don't think we need body-to-body lenses yet. Keep the code but ignore for this session." They are canvas-grain, so they belong here
- BL-694 (top-bar tracker) - Ben, 2026-08-30, the one piece carried out of the retired shell-chrome batch. Steel / Boost / Research points on the top bar as 'absolute (+delta/qtr)'. TWO OF THE THREE QUANTITIES DO NOT EXIST YET: Boost is a new untradeable resource and untradeability is not a property the resource model has, and research points have no representation in src/world at all. So this is a design item before it is a UI one, and its research third is gated on BL-619.

Was sprint 26, one of six UI review batches split out on 2026-08-28. RENUMBERED to 25 on 2026-08-30, when Ben closed out the other two proposed batches on the grounds that their surfaces are already working: batch 4 (shell chrome - header, profile tile, nav rail, time panel, comms dock, minimap, menus) and batch 6 (startup - main menu, the New World wizard, the generation screen). Neither was descoped for cost; both were judged done enough not to earn a review pass.

ONE ITEM WAS RESCUED FROM THE DELETED BATCH 4 and is carried here rather than dropped with it: the top-bar tracker, BL-694. It is header-grain rather than canvas-grain, so it sits slightly outside this sprint's theme - deliberately, because it is the only piece of shell chrome Ben still wants and keeping it needs no sprint of its own.

METHOD, unchanged and proven across batches 1 and 3: capture the whole class first, send it to Ben in one go, take his calls, build, re-capture, and expect the re-capture to find something the code read did not. Capture at 1920x1080 for a design review and keep the fit check at 1280x720 - the two are different questions (sprint 24b's rule).

### Sprint 26 — The world that brakes a leader - a rival worth watching
*Open · opened 2026-08-31*

**Goal.** ENSURE SPECTATOR MODE WORKS, AND VISIBLY WATCH AI PLAY IN ORDER TO FURTHER DEVELOP THE META (Ben, 2026-08-31). That is the sprint in one sentence and everything else is subordinate to it. Spectate is today reachable only from a --verify script, and the decision feed reads ‘overridden’ at 0.00 on every row, so the two things needed to WATCH are both absent or broken - they are the sprint. Behind them sits the design the watching is meant to inform: restraint lives in the world, not the agent (AI_OPPONENT.md § Where restraint comes from), so no corp holds itself back and instead LEADING IS EXPENSIVE - coalitions form against whoever leads. The close race becomes an emergent property rather than a target any actor aims at. What is learned by watching feeds docs/ai/STRATEGIES.md, which owns the meta.

**Planned.**
- BL-695 (live spectate route) - FIRST. Spectate is reachable ONLY from a --verify script today (verify.spectate is the single assignment site in the tree), so "watch the AI play in real time" is not currently possible from the running game. Everything else is watched through this.
- BL-696 (decision feed reasons) - FIRST, with it. NR-626: every feed row reads ‘overridden’ at 0.00, so the only window onto rival reasoning is blank. And a coalition that cannot be READ is just a hidden handicap again, which is the exact thing the reframe rejects.
- BL-700 (composite standing index) - the prerequisite for the two below, minted separately so they do not each invent their own number. Net worth + science + summed unit_strength. All three components verified to exist 2026-08-31.
- BL-697 (skill harness margin metric) - the measurement. Bands the SPREAD in composite standing across the field; absolute bands demote to a solvency floor. Carries NR-305’s stale-band debt. Ben’s first-cut target: a leader ~28% ahead.
- BL-699 (rival coalitions) - THE BRAKE, and the sprint’s real deliverable. The four stance verbs from BL-448 exist and the scorer issues none of them. Stance scores against standing, so the leader accumulates enemies and the field accumulates friends.
- BL-701 (climate doc) - DONE 2026-08-31, documentation only per Ben ("it is not the focus here"). docs/CLIMATE.md owns the commons; it runs on planetology's scalars unfrozen, NOT on tile hazard (Ben's correction), and the Era 0 exit is named as a nuclear war. THE BUILD IS UNOWNED ON PURPOSE - mint it once NR-747 (the recovery curve) is answered.
- BL-698 (opt-in margin dial) - DEMOTED to C the day it was minted. Kept as the explicit difficulty knob Ben preserved ("we can keep both modes"), off by default. If the systemic brakes need this dial to produce a close race, the brakes are the thing to fix.

THE REFRAME, and it is the whole sprint. Ben, 2026-08-31: "The angle we are taking here is a literal handicap. I prefer to consider it as always a force from within the game system. With 7 corporations, we have plenty of room for alliances to form against leaders, and we haven't built a critical system which is climate." He named the contradiction with his own earlier steer himself and chose to keep both modes, with the systemic one as the foundation. NR-746 records the supersession; NR-743 (margin measured against the strongest corp) is answered by it and should be read as historical.

THE HARD CONSTRAINT ON EVERY BRAKE, Ben the same day: "'how far restraint goes' should never exclude extension and construction. If a player loses out, they should be able to see that the world doesn't wait for them." A pressured corp still builds. A brake scales what a corp does and never forbids a category of it. This binds BL-698 and BL-699 equally and is written into AI_OPPONENT.md § The constraint on restraint.

ORDERING: instruments (BL-695, BL-696, BL-700, BL-697) before the brake (BL-699). Three of those exist because the thing that would have shown or measured the brake is broken or absent.

SCOPE HONESTY: climate is a system, not an item, and is deliberately design-only here. If the sprint runs long, BL-699 is the item to protect - it can produce a close race on its own, and climate is what later makes that race fair when the PLAYER is the one leading.

STILL UNOWNED after this sprint (NR-744): lobbying, nation stance gating the player, and the Stage C dialogue layer - purged as BL-539/BL-540/BL-334 while their dated grants still stand in the standing rules.

CLIMATE CAME OUT SMALL, and the reason is worth carrying: it needs NO NEW QUANTITY. tile_component.hazard_level and habitability are written exactly once at tile_generation.cpp:1962 and never mutated, and the (1 - hazard) multiplier is already in the production formula, in POPULATION.md's agglomeration and in the military layer. Climate is the system that makes those two move, so every consequence is already wired. What was filed at difficulty 8 as a system build is a strain model over a per-body stock.

BEN'S RESTATED AIM, 2026-08-31: "What we are really aiming to do is observe how well AI strategizes, and begin considering when Era 1 ends, how the hazard affects players, and how players begin to enter space." The first half is this sprint and is unchanged - BL-695, BL-696, BL-700, BL-697, BL-699. The second half is now DOCUMENTED rather than built: CLIMATE.md § 9 connects the hazard to the Era boundary and to the motive for entering space, and marks the Era 1 -> Era 2 gate as still undesigned rather than designing it.

BEN'S RESTATED AIM, 2026-08-31, and it narrows the sprint rather than adding to it: "Our aim for sprint 26 is to ensure spectator mode works, and visibly watch AI play in order to further develop the meta." Read BL-695 and BL-696 as the sprint's actual deliverable and BL-699 as what makes watching worth doing. If the sprint runs short, a working spectate route plus a feed that reads is a GOOD outcome on its own - the coalitions can slip. The reverse is not true: coalitions landed but unwatchable would leave the sprint unable to say whether they work.

THE META IS A DOCUMENT AND IT SHOULD RECEIVE THE OUTPUT. docs/ai/STRATEGIES.md owns the meta, authored ahead of the game as research rather than authority. Watching real rival play is the first chance to check that authored meta against what the scorer actually does; expect edits to it, and treat a divergence as a finding about one or the other rather than a defect in the scorer by default.

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
| 22 | UI visibility - batch 1: lenses | Closed 2026-08-28 - the lens batch reviewed and reworked; the remaining element classes become their own sprints (23-27) |
| 23 | UI visibility - batch 2: selection & hover | Closed 2026-08-28 - goal met. Twelve items across three waves; every planned item landed |
| 24a | UI visibility - batch 3: ledgers | Closed 2026-08-29 - batch 3, the ledgers; goal met and exceeded, and four designed mechanisms found never to have run |
| 24b | UI visibility - batch 3b: the ledgers not yet read | Closed 2026-08-30 - the six unread ledgers reviewed, three rebuilt, and the batch's review queue worked through rather than filed forward. |
| 21 | The other half of the economy - demand | PAUSED 2026-08-28 with wave 0 landed - the UI batches take priority; resume at wave 1 (BL-640/641/642) |
| 25 | UI visibility - batch 5: canvases & the zoom ladder | Proposed 2026-08-28 as sprint 26; RENUMBERED to 25 on 2026-08-30 when Ben retired the shell-chrome and startup batches - "Sprint 25 and 27 don't need a revisit, UI items for these are working great." |
| 26 | The world that brakes a leader - a rival worth watching | OPENED 2026-08-31 on Ben’s central aim. REFRAMED the same day, before any build: restraint moves OUT of the agent and INTO the world - coalitions and climate - with the scorer-side dial kept only as an opt-in mode. AI_OPPONENT.md § The goal carries both the ruling and what it supersedes. |

**Next up.** SPRINT NUMBERING, reset by Ben on 2026-08-30: the proposed shell-chrome (old 25) and startup (old 27) batches are DELETED - "Sprint 25 and 27 don't need a revisit, UI items for these are working great" - and the canvases batch renumbered from 26 to 25. THE NEXT NEW SPRINT IS 26. Two of the six UI review batches from 2026-08-28 therefore never run, and that is a judgement that their surfaces are good enough rather than a deferral.

OPEN AND PAUSED: sprint 24b (the ledgers not yet read) is the live one; sprint 21 (demand) stays PAUSED with wave 0 landed - its guard (BL-648) is deliberately red and its census (BL-649) is the before/after instrument for when it resumes. Sprint 20 also stays open and owns BL-634 (acquisition viability), which cannot honestly pass until sprint 21's demand channels land.

NEXT UP: sprint 25 (canvases & the zoom ladder), carrying BL-694 (top-bar tracker) out of the retired batch 4.

**The standing debt out of P1**, worth repeating here because it spans four items: nothing built in that sprint was ever *rendered*. The session ran in a container that cannot build the GUI, so every UI half is compile-clean and arithmetically checked and visually unseen, and no golden was blessed. For a sprint whose own method note is *build it, look at it, then rule*, that is the thing to fix first.

*37 sprints archived cold; 3 open/gated in the hot store.*
