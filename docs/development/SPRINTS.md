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

**Sprint-number ceiling advanced to 22 (Ben, 2026-08-27).** The 2026-08-24 ceiling stands in spirit - author no sprint past the one Ben opens - with 22 now the horizon. Two sprints open (21 demand, 22 UI visibility), inside the three-sprint cap. Ben has named 23 for the new UI elements he will describe once Sprint 22's improvements land, so 23 is spoken for but NOT yet authored.

**A sprint was renumbered and un-renumbered on 2026-08-27; both moves are recorded because neither lost anything.** The demand sprint was moved 21 -> 22 to free the number for a UI visibility pass, then moved back when Ben clarified that 'sprint 21' means the demand sprint to him. What made the round trip safe is that the demand sprint had LANDED WAVE 0 and was therefore moved WHOLE both times, never deleted under the 2026-08-24 unstarted-plans rule. The lesson worth keeping: a number is cheap to move and expensive to be wrong about, so confirm which sprint a bare number refers to before renumbering anything - the pre-move records (the DEVLOG heading, the NEEDS_REVIEW sources, the `sprint-21-wave-0` requirements batch key) all said 21, and they were right.

## Open now

### Sprint 21 — The other half of the economy - demand
*Open · opened 2026-08-26*

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

Opened on Ben's instruction, 2026-08-26 ('A, and open it as sprint 21'), after the session's own measurement answered his question: the ancient band has TWO live demand sinks, and ten goods pass the orphan check by naming a 'mercantile demand' that grep says was never built (NR-671). Ceiling advanced to 21 on the same instruction. ORDERING IS LOAD-BEARING: BL-648 and BL-649 go FIRST, not last. The guard makes the gap visible and the census makes each pass measurable - without them a viability pass is a guess with a number attached. A red guard with a named list is worth more than a green one that means nothing; do not weaken it to pass while the channels are being built. SEVERAL CHANNELS ARE DESIGNED-BUT-INERT rather than missing, which is why the sprint is cheaper than it looks: `strategic_reserve` is already a goods-buying budget line no consumer claims on; `logistics_maintenance` already names network upkeep; recipes already carry the era field the demand baskets need; and `run_unit_upkeep` is already the credits-plus-goods shape BL-641 wants for buildings. RELATIONSHIP TO SPRINT 20: Sprint 20 stays open and owns the ledger, the buyout and the spawn shortlist. Its BL-634 (acquisition viability) CANNOT honestly pass until this sprint lands - a corp at -28/qtr never saves up for anything - so BL-634's measurement is the natural close-out for BOTH sprints. BL-630's shortlist gains a real mechanism to gate on once demand exists: BL-635 measured that a spawn's survival currently depends on whether the generator handed it a resource anyone wants.

### Sprint 22 — UI visibility pass
*Open · opened 2026-08-27*

**Goal.** Sweep the built UI for visibility - what the simulation knows that the screen does not say. The catalogue in docs/ui/ui_elements.json (95 elements) is the spine, not a feature list, and Ben expects MANY elements to change: 'there are quite a few elements to change, so we will be using multiple sessions' (2026-08-27). Wave 0 is the SCAN - which elements carry a visual check and which carry a committed golden - so that a session changing an element already knows what re-verification it owes and what will silently pass. The felt goal: a player can see the state the model is already tracking, without opening a ledger to find it.

**Planned.**
- WAVE 0 - the coverage scan: per UI element, which scripts/verify/*.lua check covers it and whether a golden exists. Run first; every later wave is scoped off it
- WAVES 1..N - the element changes themselves, carved from the scan and from Ben's pass over it. NOT YET AUTHORED - this sprint is deliberately opened with its worklist empty (2026-08-27)
- CARRIED, pending Ben's call: NR-663's family (four checks whose green means less than it appears), BL-639 (panel columns), NR-686 thorough fix (a dev build resolving scripts/ from the repo root). These were labelled a 'visibility pass' on 2026-08-26 while that phrase still pointed at the demand sprint, and they are VERIFICATION visibility rather than UI visibility - see notes

NUMBERED 22, NOT 21 (Ben, 2026-08-27). It opened as 21 on a misreading: the demand sprint was moved to 22 to free the number, and Ben then clarified that when he says 'sprint 21' he means the DEMAND sprint. Both moves were reverted the same day - demand is 21 again, this sprint is 22, and the new UI elements Ben will describe become 23. Nothing of either sprint's plan or landed work changed in either direction. Opened on Ben's instruction, 2026-08-27: 'Let's open sprint 21. We're going to do a visibility pass on the UI, there are quite a few elements to change, so we will be using multiple sessions. First off, do a scan for goldens on each UI element.' MULTI-SESSION BY DESIGN, which is why the scan is wave 0 and not an epilogue: several sessions need one shared, queryable answer to 'what does changing this element cost me in re-verification', and the alternative is each session rediscovering it. THE WORD 'VISIBILITY' NOW CARRIES TWO SENSES AND THEY ARE NOT THE SAME WORK - (a) UI visibility, this sprint: the screen does not say what the model knows; (b) verification visibility, the 2026-08-26 steer: a check whose green means less than it appears (NR-663 family, NR-686, BL-639). Sense (b) was attached to the number 21 while 21 was the demand sprint. It is parked in `planned` above rather than silently assigned, because which sprint owns it is Ben's call, not an inference.

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
| 21 | The other half of the economy - demand | Opened 2026-08-26 - every good gets a buyer that is a mechanism, then viability is measured over repeated passes |
| 22 | UI visibility pass | Opened 2026-08-27 as Sprint 21, renumbered to 22 the same day - scan coverage first, then change elements across several sessions |

**Next up.** TWO sprints open. Sprint 21 (demand) has wave 0 landed: BL-648's guard is RED on eleven goods by design and turns green as channels land, BL-649's census is the before/after instrument. BL-654 (a short pool buys up to a reservation ceiling) gates BL-641's rates going non-zero and should land early. Sprint 22 (UI visibility) has its wave 0 landed too - the coverage scan, `tools/session/ui_coverage.js`, and a catalogue refreshed against the code - and continues with a full capture run for Ben to hand-pick surfaces needing improvement. Sprint 23 is NAMED but unauthored: the new UI elements, which Ben will describe once Sprint 22's improvements land. The candidate pool is the 2026-08-27 sweep of live model state with no surface - the order book, stance/sentiment, nation_budget, law, procurement, tariff - plus the three nav-rail slots already labelled for surfaces that do not exist (3 Workforce, 7 Corp. Strategy, 8 Diplomacy). Sprint 20's slipped items carry forward; three are design-owed on Ben. UNRESOLVED: NR-663's family, BL-639 and NR-686 - verification visibility, which sprint owns them is Ben's call (NR-692).

**The standing debt out of P1**, worth repeating here because it spans four items: nothing built in that sprint was ever *rendered*. The session ran in a container that cannot build the GUI, so every UI half is compile-clean and arithmetically checked and visually unseen, and no golden was blessed. For a sprint whose own method note is *build it, look at it, then rule*, that is the thing to fix first.

*33 sprints archived cold; 2 open/gated in the hot store.*
