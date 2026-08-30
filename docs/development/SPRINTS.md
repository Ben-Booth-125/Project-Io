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

### Sprint 24b — UI visibility - batch 3b: the ledgers not yet read
*Open · opened 2026-08-30*

**Goal.** Finish the ledger pass. 24a reviewed and rebuilt most of the rail; SIX SURFACES WERE NEVER READ - History, Research, Generation, AI decisions, Strategy readout, and Budget beyond a glance. History is the first and the largest, and Ben has it queued.

**Planned.**
- HISTORY (slot 10) is first, and Ben named it. Three concrete targets, all resolution-independent: the AGES VIEW HANGS - no frame in nineteen minutes (NR-710), so a whole view has never been opened by any capture, golden or person; the biography REPEATS ITSELF VERBATIM (3.39 Gya and 3.27 Gya draw the identical sentence and the identical consequence line); and it OPENS AT 6.28 Gya, so a player meets the star forming before their own world.
- The Tectonics view (BL-660) is uncatalogued - ui_elements.json still describes a Tiles view that no longer exists, and ui_state.hpp`s history_view comment reads "2=Tiles, 3=Ages" against an enum of Story/Chain/Ages/Tectonics. Two errors in one line.
- RESEARCH (slot 4) is the only ledger that hijacks the whole canvas instead of living in the column, and its node labels overlap and truncate - two nodes both read "Semiconductor F...". Ben has DEFERRED tech, so this is a layout pass on a mock, not a design of the system behind it.
- BUDGET (slot 2) - small and resolution-independent now that the backlog id is gone: the Extraction Levy line clips, building names truncate, and the Profit label overlaps the axis label. Its density complaint DISSOLVED at 1080p and should not be re-raised.
- The developer tail (11-13) - Generation, AI decisions, Strategy readout. Ben: these are debug tools. Cheapest pass in the batch: clipped corp names, an unexplained "overridden" on every decision row, and a legend whose red tier never appears in any bar.
- NR-731 - the retired mercenary system still runs every tick and still posts to the Public comms channel. A player is told about contracts with no door to open. Cheapest fix is to stop calling the two tick functions.

Opened because 24a grew past its own scope: it set out to review thirteen ledgers and instead rebuilt seven, created two, deleted two, and spent its second half on findings the review turned up rather than on the surfaces still unread. Splitting rather than extending keeps 24a`s retro honest about what it actually did.

METHOD CARRIED FORWARD, and 24a proved all three halves. Capture the class first and send it to Ben in one go. Run the review barrier over the integrated diff. And MEASURE BEFORE RESHAPING - which in 24a overturned a stated plan five times, twice where the wrong premise was mine and once where it was Ben`s.

ONE NEW RULE THE BATCH EARNED: capture at 1920x1080 for a design review, and keep shell_pass at 1280x720 for the fit check. The two are different questions and 24a needed both on the same day - a density complaint measured at 720p was made against half the height Ben has, and the fourteenth rail slot broke the 720p floor invisibly at 1080p.

### Sprint 25 — UI visibility - batch 4: shell chrome
*Proposed · opened 2026-08-28*

**Goal.** Review the persistent chrome - header, profile tile, nav rail, time panel, comms dock, minimap and the system/options menus. Everything on screen at all times, and therefore everything whose cost is paid on every frame the player looks at.

**Planned.**
- UI-025..UI-028, UI-051..UI-069, UI-099 in the catalogue
- Observations already banked from batch 1: the minimap is near-empty at Planetary zoom; the Comms dock carries one line in a full column; the header shows NET +-0/qtr with no Runway readout (UI-055, NONE coverage); the nav rail glyphs read alike at rail size

One of six batches the 2026-08-28 UI review split into, one sprint each on Ben's instruction: "Let's make a sprint for each batch." The decomposition is by ELEMENT CLASS from docs/ui/ui_elements.json, because that is the spine the coverage tool already reports against - `node tools/session/ui_coverage.js --captures --since N` groups a run by element, so a batch's review list is a command rather than a judgement. Batch 1 (lenses) is sprint 22, closed. METHOD, proven in batch 1: capture the whole class first, send it to Ben in one go, take his calls, build, re-capture, and expect the re-capture to find something the code read did not.

### Sprint 26 — UI visibility - batch 5: canvases & the zoom ladder
*Proposed · opened 2026-08-28*

**Goal.** Review the three canvases and the ladder between them - Solar, Circumplanetary, Planetary - plus the render layers that sit on them (tiles, terrain, roads, settlements, fog, borders, provinces) and the keyboard navigation model.

**Planned.**
- UI-001..UI-024, UI-094..UI-097 in the catalogue
- The body-to-body lenses deferred from batch 1 (Supply, Supply-routes, Reach) - Ben, 2026-08-28: "I don't think we need body-to-body lenses yet. Keep the code but ignore for this session." They are canvas-grain, so they belong here

One of six batches the 2026-08-28 UI review split into, one sprint each on Ben's instruction: "Let's make a sprint for each batch." The decomposition is by ELEMENT CLASS from docs/ui/ui_elements.json, because that is the spine the coverage tool already reports against - `node tools/session/ui_coverage.js --captures --since N` groups a run by element, so a batch's review list is a command rather than a judgement. Batch 1 (lenses) is sprint 22, closed. METHOD, proven in batch 1: capture the whole class first, send it to Ben in one go, take his calls, build, re-capture, and expect the re-capture to find something the code read did not.

### Sprint 27 — UI visibility - batch 6: startup & the wizard
*Proposed · opened 2026-08-28*

**Goal.** Review the path in - main menu, the New World wizard and its three rounds - the only surfaces a player meets before the game exists, and the only ones with no world behind them to make them look busy.

**Planned.**
- UI-091..UI-093 in the catalogue
- STARTUP.md owns the flow

One of six batches the 2026-08-28 UI review split into, one sprint each on Ben's instruction: "Let's make a sprint for each batch." The decomposition is by ELEMENT CLASS from docs/ui/ui_elements.json, because that is the spine the coverage tool already reports against - `node tools/session/ui_coverage.js --captures --since N` groups a run by element, so a batch's review list is a command rather than a judgement. Batch 1 (lenses) is sprint 22, closed. METHOD, proven in batch 1: capture the whole class first, send it to Ben in one go, take his calls, build, re-capture, and expect the re-capture to find something the code read did not.

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
| 21 | The other half of the economy - demand | PAUSED 2026-08-28 with wave 0 landed - the UI batches take priority; resume at wave 1 (BL-640/641/642) |
| 24a | UI visibility - batch 3: ledgers | Closed 2026-08-29 - batch 3, the ledgers; goal met and exceeded, and four designed mechanisms found never to have run |
| 24b | UI visibility - batch 3b: the ledgers not yet read | Opened 2026-08-30 straight out of 24a - the ledgers batch 3 did not reach |
| 25 | UI visibility - batch 4: shell chrome | Proposed 2026-08-28 - one of the six UI review batches |
| 26 | UI visibility - batch 5: canvases & the zoom ladder | Proposed 2026-08-28 - one of the six UI review batches |
| 27 | UI visibility - batch 6: startup & the wizard | Proposed 2026-08-28 - one of the six UI review batches |

**Next up.** Sprint 22 (UI visibility, batch 1: lenses) CLOSED 2026-08-28. Sprint 21 (demand) is PAUSED with wave 0 landed. Next is sprint 23 (selection & hover), which Ben opens as a dedicated coding session - the two half-built region selections from batch 1 are its first work (NR-697, NR-698), and BL-659/BL-660 carry into it. Five items carry out of batch 1 with Ben's decisions already recorded: BL-659 deposit->Market Prices, BL-660 plate->a new tectonic History section (needs a divergent classification in continent_state, which does not exist - only `convergent` is computed), BL-661 population heatmap at scaling 2, BL-662 Scarcity re-cut keeping its name and taking the Opportunity glyph, BL-663 Company glyph as a cluster of small forms.

**The standing debt out of P1**, worth repeating here because it spans four items: nothing built in that sprint was ever *rendered*. The session ran in a container that cannot build the GUI, so every UI half is compile-clean and arithmetically checked and visually unseen, and no golden was blessed. For a sprint whose own method note is *build it, look at it, then rule*, that is the thing to fix first.

*35 sprints archived cold; 5 open/gated in the hot store (1 completed and awaiting archive_sprints.js).*
