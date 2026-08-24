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

## Open now

### Sprint 25b — The cut (interdiction)
*Gated*

**Goal.** The interception trigger in the economy tick, convoy_tile_at lifted out of the renderer, stance as the sole predicate, and the comms/ledger/canvas surfaces that stop it being silent.

**Planned.**
- BL-458 — supply lines cannot be cut (designed, A, d4): requires BL-315, BL-448, BL-452.

**Done when.** A player can lose a convoy to a hostile company and be told exactly what was taken and by whom.

**Risk.** Real risk is scope creep into a combat system — the moment interception grows escorts, blockades, ambient banditry or neutral interception it stops being a logistics item and becomes BL-315's job done twice.

Gated; cannot start before Sprints 21 and 23. This item is why BL-315's own ruling has been waiting: army/mercenary/pirate are three derived readings of one company, the reading is derived never authored; army and mercenary have paths to being read, pirate has NO mechanic at all — taking a convoy is exactly that act. SETTLED 2026-08-17 (Ben, on NR-310) — capture, with destruction as the fallback. Cargo leaves the source pool at dispatch, so both answers conserve. On interception the cargo credits the interceptor's (corp, body) pool at the interception body; where it cannot be credited it is destroyed instead, and the outcome says which — an interceptor with no market access holds goods it cannot sell, a legitimate outcome, NOT special-cased away. What decided it: destroy-only pays the interceptor zero, so BL-450's scored-utility stance layer would correctly never rank an interception — interdiction would ship as a capability only the player ever uses, the same unreachable-capability defect this sprint exists to fix. Capture gives the scorer a number, and earns BL-315's third derived reading: a company that takes cargo reads as a pirate, one that only burns it reads as a saboteur. Harness's load-bearing row: captured quantity == quantity credited exactly, with the destroyed-fallback path asserting zero credited and zero minted. Not a battle by default — a convoy is cargo, not a force, the first cut resolves interception as an OUTCOME; escort (a unit assigned to a convoy, turning interception into a real resolve_campaign_battle call) is the obvious follow-on, deliberately out because it needs BL-315's engagement machinery SHIPPED not merely built. SETTLED 2026-08-17 (Ben, on NR-312): 25a runs next, sequence is 25a → 21 → 23 → 25b — interdiction, the payoff this sprint was rescoped to carry, is now three sprints out; the alternative (opening Sprint 23 stance first as shortest path) was on the table, Ben chose the substrate instead, the reading that leaves 25b with fewer unknowns when it arrives. The check: interdiction — convoy_tile_at agrees with the renderer on every lane in a generated world IN BOTH KEY ORIENTATIONS (the bug this item is most likely to ship); a hostile unit on the head tile intercepts and a neutral one does not; goods conserved exactly under capture and destroyed exactly under destroy, no minting either way; two runs of one seed intercept the same convoys on the same ticks; an interception with no stance entry never fires. Plus a --verify capture of the Convoys tab, and a dispatch→hold→resume round-trip asserting a rejection mutates nothing. What this sprint deliberately does not carry: escort stays out (needs BL-315 shipped); ambient banditry, neutral interception, terrain blockade stay out (stance is the sole predicate); BL-325 S3 not cut but not separately scoped (converges with BL-454's shortfall rule); per-node throughput out of prototype scope; air mode/airfield deferred; a second reach field forbidden outright by BL-325 ruling 3; waypoint routing and standing lane orders stay out; a second and third military good stay out.

### Sprint 18 — Logistics
*Open · opened 2026-08-24*

**Goal.** Logistics-focused (Ben, 2026-08-24) — being designed in this session; goal text lands when the design settles.

Number reuse, deliberate (Ben, 2026-08-24): the superseded military-engagement Sprint 18 (opened 2026-08-15) is archived cold alongside the ‘18 retro’ and ‘18b’ entries; this sprint is a fresh authoring against the current docs, not a resurrection. Fits the sprint-number ceiling (18 and below).

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
| 26 | Re-baseline (the gate; nothing else may open first) | Subsumed — split at execution into 26a/26b, themselves deleted in the 2026-08-24 purge |
| 25b | The cut (interdiction) | Gated — needs BL-315’s engagement machinery shipped; its settled sequence (25a → 21 → 23 → 25b) points at numbers purged 2026-08-24 (NR-598) |
| 18 | Logistics | Open 2026-08-24 — logistics-focused (Ben); design in flight this session |

**Next up.** As of 2026-08-24, **Sprint 18 (logistics)** is open — the theme is Ben’s call, the design is in flight. **25b (interdiction)** stays gated; its sequence points at purged numbers (NR-598). Two cap slots free; any candidate numbers 18 or below. **BL-518** (the Era −1 sim redrawing borders as its wars resolve) and **BL-514** (blend all tiles, HELD) stay parked in `backlog.json`.

**The standing debt out of P1**, worth repeating here because it spans four items: nothing built in that sprint was ever *rendered*. The session ran in a container that cannot build the GUI, so every UI half is compile-clean and arithmetically checked and visually unseen, and no golden was blessed. For a sprint whose own method note is *build it, look at it, then rule*, that is the thing to fix first.

*29 sprints archived cold; 2 open/gated in the hot store.*
