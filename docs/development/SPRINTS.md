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

## Open now

### Sprint 18 — Logistic Points land with their consumers
*Open · opened 2026-08-24*

**Goal.** The network gets its ceiling: the bifold city-generated throughput rate LOGISTICS.md argues end-to-end, landing with both consumers — the priced march first (Ben, 2026-08-24), convoy admissibility second — plus the two rival network grants and the three riders the design form settled. Cuts v0.1.18 (amended: Logistic Points pulled forward from v0.1.20).

**Planned.**
- BL-599 (rival roads and hubs) — wave 1: the scorer builds the generator, per Ben’s “before LP lands” (grant 2026-08-24)
- BL-596 (LP active march) — the bifold city rate + the priced march, one landing (settled rule 3)
- BL-597 (LP passive convoys) — convoy admissibility; war flips the queue observably
- BL-598 (throughput lens) — Reach with a magnitude; live click
- BL-600 (rival directed dispatch) — the scorer directs convoys through the shared seam (grant 2026-08-24)
- BL-601 (dispatch form) — the Selection-card form SUPPLY.md designs; live click
- BL-602 (sea port gate) — the Port gates the sea leg (park lifted 2026-08-24)
- BL-603 (upkeep zeros) — the reach field starts to bite (data edit; Ben calibrates)

**Done when.** A march costs credits through active LP in a live game; an over-cap dispatch leg is refused and the refusal announced; the queue flip is observable under mobilisation in the harness; a rival places a road deterministically across two runs of one seed; the Throughput lens and the dispatch form each pass a live click.

**Risk.** LP’s seven constraints each killed an earlier cut — constraint 3 (cost formula before allocation sort key) is the live one. The substrate deliberately keeps its measured mispricings (route-wide crosses_ocean, unpriced intra-catchment distance): Ben chose depth over correction on the design form, so LP’s first-cut rates are argued against numbers known to be imperfect and must be re-argued if a later sprint fixes them. Golden churn from upkeep zeros and the sea gate is real and is attributed, never blessed away.

Verdict provenance — the 2026-08-24 design form: shape A over B/C/D (B’s pricing-honesty roster stays purged-designed reference); purged records enter FRESH, archive as reference only; Sprint 25b deleted under the purge policy (its interception-narration work is unowned — NR-599); rival grants place_road/hub + directed dispatch (standing rules, dated); riders all three (dispatch form, sea-gate park lift, upkeep zeros); LP consumer order ACTIVE FIRST; rates first-cut-then-tune (NR-600). Number reuse, deliberate (Ben, 2026-08-24): the superseded military-engagement Sprint 18 (opened 2026-08-15) is archived cold alongside the ‘18 retro’ and ‘18b’ entries; this sprint is a fresh authoring against the current docs, not a resurrection. PROGRESS 2026-08-25: seven of eight items landed and independently re-verified on the merged tree (never trusted from a sub-agent report) — BL-599/BL-600 (scorer network verbs), BL-601 (dispatch form, live click OWED — no interactive access in this container), BL-602 (sea Port gate), BL-603 (upkeep zeros, self-corrected against value_anchor), BL-596 (active LP prices the march), BL-598 (Throughput lens, live click OWED). BL-597 is PAUSED, not merged: harness-green 34/34 but haulage_measure measured real convoy traffic collapsing 1055 -> 284 (802 -> 246 intra-body), because its draw is distance-proportional — the exact formula LOGISTICS.md constraint 3 and rule 1 both forbid. Preserved on wip/bl-597-lp-passive-convoys (3ab3b452); awaiting Ben on NR-620. METHOD FINDINGS worth carrying: (1) no standing harness could see the trade collapse — ai_skill_harness never dispatches a convoy, data_creep_harness seeds them directly past the gate, and the item own fixtures ran a generous rate, so every golden stayed digit-for-digit identical while trade died; haulage_measure, which runs the real tick loop, was the only instrument that could. (2) Worktree isolation forked from a stale base on every wave-1 agent (a leftover detached-HEAD worktree, elated-mclean-7dd61c/jolly-tesla-49df05, still on disk and now also misdirecting computer-use tooling — NR-618, NR-622); waves 2-3 were run in-place on the shared checkout instead, and every wave-1 item was cherry-picked commit-by-commit rather than merged wholesale. (3) Sub-agents repeatedly stalled by backgrounding their own builds and ending the turn to await a notification that never comes. BL-597 LANDED 2026-08-25 after Ben ruled NR-620: rebuilt on a cargo-QUANTITY draw instead of route distance, restoring 1041/789 convoys against the 1055/802 baseline (98.7%) with the cap still binding on ~14 legs a run. The 20.0 rate was right all along; only the formula was wrong. All eight items are now in.

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
| 26 | Re-baseline (the gate; nothing else may open first) | Subsumed — split at execution into 26a/26b, themselves deleted in the 2026-08-24 purge |
| 18 | Logistic Points land with their consumers | Open 2026-08-24 — ALL EIGHT landed (BL-596..BL-603); two live clicks owed. Cuts v0.1.18 |

**Next up.** As of 2026-08-24, **Sprint 18 (Logistic Points land with their consumers)** is the only open sprint — eight items, BL-596..BL-603, cutting v0.1.18 (Logistic Points pulled forward from v0.1.20 by the design-form verdict). Two cap slots free; any candidate numbers 18 or below. **BL-518** (the Era −1 sim redrawing borders as its wars resolve) and **BL-514** (blend all tiles, HELD) stay parked in `backlog.json`.

**The standing debt out of P1**, worth repeating here because it spans four items: nothing built in that sprint was ever *rendered*. The session ran in a container that cannot build the GUI, so every UI half is compile-clean and arithmetically checked and visually unseen, and no golden was blessed. For a sprint whose own method note is *build it, look at it, then rule*, that is the thing to fix first.

*30 sprints archived cold; 1 open/gated in the hot store.*
