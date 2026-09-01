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
*Complete · opened 2026-08-26*

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
*Complete · opened 2026-08-31*

**Goal.** ENSURE SPECTATOR MODE WORKS, AND VISIBLY WATCH AI PLAY IN ORDER TO FURTHER DEVELOP THE META (Ben, 2026-08-31). That is the sprint in one sentence and everything else is subordinate to it. Spectate is today reachable only from a --verify script, and the decision feed reads ‘overridden’ at 0.00 on every row, so the two things needed to WATCH are both absent or broken - they are the sprint. Behind them sits the design the watching is meant to inform: restraint lives in the world, not the agent (AI_OPPONENT.md § Where restraint comes from), so no corp holds itself back and instead LEADING IS EXPENSIVE - coalitions form against whoever leads. The close race becomes an emergent property rather than a target any actor aims at. What is learned by watching feeds docs/ai/STRATEGIES.md, which owns the meta.

**Planned.**
- BL-695 (live spectate route) - FIRST. Spectate is reachable ONLY from a --verify script today (verify.spectate is the single assignment site in the tree), so "watch the AI play in real time" is not currently possible from the running game. Everything else is watched through this.
- BL-696 (decision feed reasons) - FIRST, with it. NR-626: every feed row reads ‘overridden’ at 0.00, so the only window onto rival reasoning is blank. And a coalition that cannot be READ is just a hidden handicap again, which is the exact thing the reframe rejects.
- BL-700 (composite standing index) - the prerequisite for the two below, minted separately so they do not each invent their own number. Net worth + science + summed unit_strength. All three components verified to exist 2026-08-31.
- BL-697 (skill harness margin metric) - the measurement. Bands the SPREAD in composite standing across the field; absolute bands demote to a solvency floor. Carries NR-305’s stale-band debt. Ben’s first-cut target: a leader ~28% ahead.
- BL-699 (rival coalitions) - THE BRAKE, and the sprint’s real deliverable. The four stance verbs from BL-448 exist and the scorer issues none of them. Stance scores against standing, so the leader accumulates enemies and the field accumulates friends.
- BL-701 (climate doc) - DONE 2026-08-31, documentation only per Ben. Rewritten twice more the same day: climate runs on planetology's scalars unfrozen (NOT tile hazard), and it is the NEXT Era's catastrophe, outside prototype scope. The Era catastrophe model itself was promoted out of the research doc into ERAS.md § The point of an Era in the same pass (NR-750).
- BL-698 (opt-in margin dial) - DEMOTED to C the day it was minted. Kept as the explicit difficulty knob Ben preserved ("we can keep both modes"), off by default. If the systemic brakes need this dial to produce a close race, the brakes are the thing to fix.
- BL-702 (spectate god view control) - with BL-695. god_view is read at every site and written only by verify.god_view. Spectating without it is watching silhouettes.
- BL-705 (selectable 1960s start) - Ben, 2026-08-31: "we will be working on the 1960s start". The branch is live and unreachable; nothing in src/ ever sets epoch_year. Also fixes the hard-coded 1960 epoch constant, which a 1960s start would otherwise make accidentally correct.
- BL-703 (watch session finding) - THE ITEM THAT DELIVERS THE SECOND HALF OF THE SPRINT GOAL. A run watched end to end on the 1960s start, written up into STRATEGIES.md. Without it the sprint can end with a working spectate mode and nothing learned.
- BL-704 (rival trace export) - optional. Turns a finding from a memory of watching into something that cites evidence. Check first whether the existing world history log already carries it.

THE REFRAME, and it is the whole sprint. Ben, 2026-08-31: "The angle we are taking here is a literal handicap. I prefer to consider it as always a force from within the game system. With 7 corporations, we have plenty of room for alliances to form against leaders, and we haven't built a critical system which is climate." He named the contradiction with his own earlier steer himself and chose to keep both modes, with the systemic one as the foundation. NR-746 records the supersession; NR-743 (margin measured against the strongest corp) is answered by it and should be read as historical.

THE HARD CONSTRAINT ON EVERY BRAKE, Ben the same day: "'how far restraint goes' should never exclude extension and construction. If a player loses out, they should be able to see that the world doesn't wait for them." A pressured corp still builds. A brake scales what a corp does and never forbids a category of it. This binds BL-698 and BL-699 equally and is written into AI_OPPONENT.md § The constraint on restraint.

ORDERING: instruments (BL-695, BL-696, BL-700, BL-697) before the brake (BL-699). Three of those exist because the thing that would have shown or measured the brake is broken or absent.

SCOPE HONESTY: climate is a system, not an item, and is deliberately design-only here. If the sprint runs long, BL-699 is the item to protect - it can produce a close race on its own, and climate is what later makes that race fair when the PLAYER is the one leading.

STILL UNOWNED after this sprint (NR-744): lobbying, nation stance gating the player, and the Stage C dialogue layer - purged as BL-539/BL-540/BL-334 while their dated grants still stand in the standing rules.

BEN'S RESTATED AIM, 2026-08-31, and it narrows the sprint rather than adding to it: "Our aim for sprint 26 is to ensure spectator mode works, and visibly watch AI play in order to further develop the meta." Read BL-695 and BL-696 as the sprint's actual deliverable and BL-699 as what makes watching worth doing. If the sprint runs short, a working spectate route plus a feed that reads is a GOOD outcome on its own - the coalitions can slip. The reverse is not true: coalitions landed but unwatchable would leave the sprint unable to say whether they work.

THE META IS A DOCUMENT AND IT SHOULD RECEIVE THE OUTPUT. docs/ai/STRATEGIES.md owns the meta, authored ahead of the game as research rather than authority. Watching real rival play is the first chance to check that authored meta against what the scorer actually does; expect edits to it, and treat a divergence as a finding about one or the other rather than a defect in the scorer by default.

CLIMATE LEFT THE SPRINT AND LEFT THE PROTOTYPE, 2026-08-31. Ben: "I am not against climate change being a large problem for Era 2, but our prototype works solely on Era 1 for now." CLIMATE.md is rewritten as the NEXT Era's catastrophe and is out of scope. ONE KNOCK-ON MATTERS HERE: AI_OPPONENT.md had named climate as the second systemic brake on a runaway leader, so COALITIONS NOW CARRY THAT BRAKE ALONE. If BL-699 does not brake a runaway in this sprint, nothing else will - and that is a finding about coalitions, not a reason to reach for BL-698's opt-in dial.

THE LADDER IS 1-BASED AND THE START IS THE 1960s (Ben, 2026-08-31, resolving NR-749). Era 1 Terrestrial, catastrophe nuclear war - that is the era this sprint watches. Era 2 Early Space, catastrophe climate. The renumber is the DOCUMENT's Era axis only; era_band, condition_subject::era and every E0-/E1- tech id are untouched and nothing in src/ moved. One divergence created and tracked: the F9 mock viewer still labels its tabs by the old numbers (NR-751), and ACTIONS.md correctly still mirrors the viewer rather than the docs.

### Sprint 27 — The other half of the economy - demand, resumed
*Open · opened 2026-08-31*

**Goal.** Give the goods a buyer. Sprint 26 measured the economy the AI actually plays: on the industrial band iron_ore is produced 42991.6 against total demand 0.000, three resources carry any household demand at all, five of the eight designed demand channels do not exist, and of the three that do Industry ships its rates at ZERO. Every AI corp is insolvent 29-30 ticks of 30 on every seed, monotonically. That is not a tuning problem and no scorer change can move it. The sprint's felt goal: a world where the goods a player is meant to chase are wanted by somebody, and it is legible WHO - and, measured, an economy where a competent corporation can end a run solvent.

**Planned.**
- BL-654 (a channel must bid) - FIRST, and it GATES THE REST. Two of the three live demand channels are POOL draws that never reach market_component::demand, so wanting a good never induces its supply. Ben settled the shape 2026-08-26: "Buy on the market, but at a threshold, buying is not allowed" - a short pool bids the shortfall and PAYS for it, above a reservation ceiling it does not bid and the shortfall rule weakens the building instead. ONE RULE FOR EVERY GOODS DRAW, unit upkeep included. The ceiling is authored in the PRICE BAND family and MEASURED against the census, never guessed. Turning BL-641s rates non-zero is gated on this landing.
- BL-652 (injectors must not skip silently) - with it. A demand injector that cannot price a basket entry must SAY SO. The guard that stops the next silent zero.
- BL-707 (the ancient chain does not convert) - wave 1, and DIAGNOSE before tuning. The ancient band fails DIFFERENTLY from the industrial one: its endpoints work and its middle does not. fibre produced 27613.5 against demand 89.7 near the price floor, while leather is produced 6.8 against demand 87.1 at 9.97x base ceiled in 13 of 14 markets. A good ceiled in 13 of 14 markets is an absent industry, not a shortage.
- BL-706 (chain completeness read) - the generation-side instrument. Measures the fraction of chains terminating in a region that can be sourced within reach, and reports the SPREAD across the world. Reports, never gates: the enemy is uniformity, not self-sufficiency.
- BL-708 (power as a grid) - generation buildings, an electricity upkeep draw, transmission on the ROAD network with flat one-tick latency. Closes the fuel chain: coal and petroleum are both on the census's "extractable, NO market sink" list, and the generator buys its fuel on the market as an ordinary processing input. Needs no new graph - connectivity is tile_reach_cost read as a boolean.
- BL-709 (construction as a rate) - after BL-708. Construction becomes a sector with a throughput and a production method, so the channel that currently measures 0.000 becomes continuous and economy-scaled, and seeded capacity gives it a non-zero reading from tick 0. Structural: it reaches construct_building, placement, the build door and the scorer, and NR-592 is fixed here on Ben's call.
- BL-642 (construction actually draws) - wave 1. The opening years build, and centres draw materials as they grow.
- BL-644 (space programme budget line) - wave 2. The State channel, currently ABSENT: nation budget lines spend credits and no goods purchase exists anywhere.
- BL-647 (endemic luxury demand) - wave 2. The Endemic trade channel, currently ABSENT: tobacco, spices, coffee and furs are authored, generated, given latitude bands, and wanted by nobody.
- BL-643 (network upkeep draws) - wave 2. The Infrastructure channel, currently ABSENT: no material draw for roads, ports or hubs anywhere in src/world.
- BL-646 (battles burn ordnance) - wave 2. The Conflict channel, currently ABSENT, and the only one that couples the two pillars: Conflict moves Trade.
- BL-645 (research consumes goods) - GATED on BL-619, a design session rather than a build slice. Carry, do not start.
- VIABILITY PASSES AFTER EACH WAVE - census before, census after, deltas kept, spawn viability measured. Ben, 2026-08-26: "we will get interesting data and do a few passes over viability." The passes ARE the sprint.
- ADJACENT AND WORTH CHECKING AGAINST THE SAME MEASUREMENT, not yet scheduled here: BL-657 (a failing firm exits), BL-658 (the solvency gate tests the price, not the position after the debt transfers), BL-653 (the return sees the whole quarter). All three are about what happens to a firm that is underwater, which is currently every firm.

THE MEASUREMENT THAT OPENS IT, from sprint 26 wave 1 (NR-758). ai_skill_harness, five seeds, thirty econ ticks: net worth -624100 / -997700 / -694800 / -1234354 / -1090065, solvency_below_zero 29-30 of 30, and final == min on every seed so it never recovers. demand_census on the industrial band: iron_ore produced 42991.6 demand 0.000, petroleum 6169.9 demand 0.000, timber 1224.2 demand 0.000. Only agricultural_produce, water and food_rations carry any household demand.

THE ANSWER IS ALREADY SETTLED AND OWNED - BL-654, Ben 2026-08-26 - and I briefly filed it as an open question (NR-760, withdrawn) because scripts/economy.lua still carries the pre-ruling comment saying the call is Ben's. Read the store, not only the code comment.

THE DEADLOCK HAS A PRECEDENT IN THIS CODEBASE and it should be read before designing the fix. BL-440 solved the same shape one layer up, in the scorer: "a processor must be RUNNING to bid a scarce input's price up, and it cannot run without that input. The deadlock is the defect." Its answer was to take the pull from the recipe graph - a static, deterministic world fact - rather than from the price signal that cannot work. corp_ai_params::input_demand_pull is that fix. The pool-vs-market question is the same deadlock at the demand layer.

SUCCESS IS MEASURABLE AND SHOULD BE STATED UP FRONT: the sprint is done when ai_skill_harness shows a field that is not monotonically insolvent - not when the channels are built. Channels built and corps still broke is the failure mode this sprint exists to avoid, and it is exactly what BL-641 produced on its own.

CARRIED FROM SPRINT 26, unowned by any sprint until demand lands: BL-697 (skill harness margin metric), BL-699 (rival coalitions), BL-703 (watch session finding), BL-704 (rival trace export), BL-698 (opt-in margin dial). BL-699's weights are the whole design and would need redoing against a solvent economy, which is why they were not built against a bankrupt one.

THE TWO BANDS FAIL DIFFERENTLY, measured 2026-08-31, and the sprint should not apply one band's fix to the other. ANCIENT: endpoints largely work (BL-640's banded basket delivered), the middle does not convert - raw inputs glut at the floor while finished goods price at the ceiling in 11-13 of 14 markets. INDUSTRIAL: endpoints barely exist - household reaches 6 resources, construction 0, building upkeep 0, and 69% of all demand is the background-industrial STOPGAP. Strip it and 1.8% of production has a genuine buyer.

MARKETS.md § Three properties now carries two more, both Ben's, 2026-08-31. Property 4: every resource needs a path to a TERMINAL sink, and intermediates earn DERIVED demand through the chain rather than their own channel - which is why property 3 (a channel must bid) is load-bearing, since derived demand only propagates through links that bid. Property 5: terminal demand is universal because population is everywhere, supply is regional because deposits are, and local self-sufficiency is NOT forbidden - the asymmetry is generation's to produce, not to guarantee.

### Sprint 28 — The AI that holds back - the brake, once standing means something
*Proposed · opened 2026-08-31*

**Goal.** Build the systemic brake now that a corporation's standing can be trusted. Sprint 26 landed the instruments - spectate, a decision feed that reads, a composite standing index - and CANCELLED its own wave 2 on its own measurement: coalitions form against whoever leads, and among corps insolvent 27-29 ticks of 30 the leader is merely the least bankrupt. These three items are not wrong, they were UNMEASURABLE. They become measurable when sprint 27 gives the goods a buyer.

**Planned.**
- BL-697 (skill harness margin metric) - band the SPREAD in composite standing across the field; the absolute bands demote to a solvency floor beneath it. Carries NR-305 (bands stale since 2026-08-16) and NR-752 (spectator_determinism byte-identity golden stale by 150+ commits). ONE deliberate re-bless with dated provenance, not a dribble - the NR-596 precedent.
- BL-699 (rival coalitions) - THE BRAKE, and the real deliverable. The four BL-448 stance verbs exist on the seam and the scorer issues none of them. Legibility is a REQUIREMENT of the item, not a follow-up: a coalition the player cannot see is exactly the hidden handicap the whole reframe rejects.
- BL-698 (opt-in margin dial) - priority C, off by default. If the systemic brake needs this dial to produce a close race, the BRAKE is the thing to fix, not the dial.

WHY IT WAITS. AI_OPPONENT.md § Where restraint comes from puts the brake in the world rather than in the agent - coalitions form against whoever leads. That needs a leader worth forming against. Measured 2026-08-31 with the survey-state fix applied: four of five seeds end deeply negative and every seed is insolvent 27-29 ticks of 30, so 'whoever leads' currently means 'least bankrupt'. BL-699's weights ARE its design, and tuning them against that field would mean re-deriving them the moment demand lands.

CLIMATE IS NOT AVAILABLE TO THIS SPRINT, DELIBERATELY. AI_OPPONENT.md named two systemic brakes; climate is Era 2's catastrophe and out of prototype scope, so COALITIONS CARRY THE BRAKE ALONE. If BL-699 does not brake a runaway, nothing else will - and that is a finding about coalitions, not a reason to reach for BL-698.

CHECK BEFORE STARTING: BL-712 (recipe choice is scale-blind) in sprint 27 fixes an argmax that stops the AI building whole categories of building. A coalition brake tuned against a field that cannot build power or construction is a brake tuned against a crippled opponent.

### Sprint 29 — The world gets a face - detailed canvas rendering
*Open · opened 2026-09-01*

**Goal.** Move the Planetary canvas off the minimal vector style: the ground renders as baked painterly chunks in the ratified C-F direction (painterly relief + near-future grade) - hillshade from the height field, authored biome brushes, NO on-ground hex grid, installations as rendered geometry - on the existing 2D backend, with the vector bake surviving as fallback-by-coverage. Felt goal: the surface reads as terrain, not as a diagram. Camera stays top-down this stage; the oblique end-state (2.5D vs 3D) is a flagged future milestone.

**Planned.**
- BL-732 (ground bake renderer) - chunk bake + cache + invalidation, no-grid rule, installation stamps, grade pass, animated overlays, verify pinning. Buildable procedural-first.
- BL-733 (biome brush art pipeline) - authored C-F brushes and structure stamps, judged against it3 C-F. Unblocked.
- BL-734 (ground/chrome layer contract) - the surviving analytic channels over painterly ground; lands as PLANETARY/CANVASES/LENSES/ICONS edits.
- BL-735 (ground wave 2) - stepped x2 zoom ladder pairing bake tiers {6,12,24,48,96 px/r}, all baking on a worker thread, border band muted to a single frontier ring. Ben rulings 2026-09-01 evening.

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
| 21 | The other half of the economy - demand | CLOSED 2026-08-31, wave 0 only (BL-648 guard, BL-649 census). Its remaining waves are RESUMED AS SPRINT 27 on Ben's call - "unpause demand. Let's work on this for sprint 27" - re-planned against sprint 26's census measurement rather than its own original ordering. |
| 25 | UI visibility - batch 5: canvases & the zoom ladder | Proposed 2026-08-28 as sprint 26; RENUMBERED to 25 on 2026-08-30 when Ben retired the shell-chrome and startup batches - "Sprint 25 and 27 don't need a revisit, UI items for these are working great." |
| 26 | The world that brakes a leader - a rival worth watching | CLOSED 2026-08-31 AT WAVE 1 on Ben's call, GOAL MET. Spectator mode works and the feed reads; the measurement that wave 1 produced then made wave 2 not worth running, and demand takes priority as sprint 27. |
| 27 | The other half of the economy - demand, resumed | OPENED 2026-08-31 on Ben's call, resuming sprint 21's waves 1+ against sprint 26's measurement. Wave 0 (the guard, the census) already landed under 21. |
| 28 | The AI that holds back - the brake, once standing means something | PROPOSED 2026-08-31 on Ben's call, taking the three sprint-26 items whose tuning is blocked on demand. NOT open: it starts when sprint 27 has given standing a meaning. |
| 29 | The world gets a face - detailed canvas rendering | OPENED 2026-09-01. Design forms ruled, RENDERING.md landed, BL-732 (ground bake renderer) DELIVERED same day; Ben judged the first bake live and ruled wave 2 - stepped x2 zoom with bake tiers, threaded bake, muted 1-tile borders (BL-735, in flight). |

**Next up.** SPRINT NUMBERING, reset by Ben on 2026-08-30: the proposed shell-chrome (old 25) and startup (old 27) batches are DELETED - "Sprint 25 and 27 don't need a revisit, UI items for these are working great" - and the canvases batch renumbered from 26 to 25. THE NEXT NEW SPRINT IS 26. Two of the six UI review batches from 2026-08-28 therefore never run, and that is a judgement that their surfaces are good enough rather than a deferral.

OPEN AND PAUSED: sprint 24b (the ledgers not yet read) is the live one; sprint 21 (demand) stays PAUSED with wave 0 landed - its guard (BL-648) is deliberately red and its census (BL-649) is the before/after instrument for when it resumes. Sprint 20 also stays open and owns BL-634 (acquisition viability), which cannot honestly pass until sprint 21's demand channels land.

NEXT UP: sprint 25 (canvases & the zoom ladder), carrying BL-694 (top-bar tracker) out of the retired batch 4.

**The standing debt out of P1**, worth repeating here because it spans four items: nothing built in that sprint was ever *rendered*. The session ran in a container that cannot build the GUI, so every UI half is compile-clean and arithmetically checked and visually unseen, and no golden was blessed. For a sprint whose own method note is *build it, look at it, then rule*, that is the thing to fix first.

*37 sprints archived cold; 6 open/gated in the hot store.*
