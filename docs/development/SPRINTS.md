# Project Io — Weekly Sprints

A lightweight weekly rhythm layered over the backlog/Delivery system: a **goal** stated at the
start of the week, a **retro** at the end comparing what landed against it. This is feedback *for
Ben* — pacing and priority signal, not a new authority (backlog.json/REFINED.md/DEVLOG stay the
source of truth for what's actually true about an item).

Entries are newest-first, one per **sprint** — a sprint is a themed span of work, not a fixed
calendar week; it closes when its goal is settled (landed or deliberately descoped), not on a
clock. A gap with no entry means no sprint goal was set — that's fine, skip it rather than
backfilling.

**Drained 2026-08-19.** Every sprint's goal, planned items, retro and free-form notes (rulings,
audit tables, amendments, risk paragraphs) now live in `docs/development/sprints.json`'s
`sprints` array, following the same split `backlog.json`/`BACKLOG.md` already established. This
file keeps only the format template and a regenerated status index below — query the JSON for
substance, don't read it whole (it runs ~150 KB).

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

(The old **Runtime** line — total session time vs. items delivered — is dropped from the
template. It went uncollected for seven-plus consecutive entries; see Sprint 19's retro in
`sprints.json` for the reasoning. If the pacing signal is wanted again later, derive it from
commit timestamps rather than reviving an unstarted timer.)

---

**Active-sprint cap (Ben, 2026-08-23).** No more than 3 sprints open/proposed at once — enough
that a session doesn't have to weigh a long tail of undocumented parallel work.

**Sprint-number ceiling (Ben, 2026-08-24): keep sprints 18 and below.** Nothing is planned or
authored past sprint 18 — narrower than the count cap above, this bounds *how far ahead* the
sprint horizon is allowed to reach at all.

**Unstarted plans are DELETED, not archived (Ben, 2026-08-24).** A sprint that was only ever
open/proposed and never really executed is a stale reference once dropped — closing it and
keeping its prose around just to reopen the same number later isn't worth the upkeep. On adopting
the cap, 27 open/proposed sprints (plus Sprint 32, authored and dropped same-day on the ceiling
above) were removed outright from `sprints.json`, freeing their numbers rather than retiring them:
20, 21, 22, 22-24 preamble, 23, 24, 25, 26-33, 26a, 26b, 27, 28, 29, 30, 31, 32, B1, B2, B3, C1,
C2, C3, D2, D3, D4, N3, N4, ST1, W1. This is narrower than it looks: a sprint that actually
**landed work** — even a partial or unsatisfying result (Sprint 19's goal NOT met, P1's rendering
debt) — is a real historical fact and stays in the table below as closed. Only the never-executed
ones were deleted.

## Where things stand (updated 2026-08-24)

| Sprint | Theme | State |
|---|---|---|
| 1 | Procedural generation v1 | **Closed** — goal met (food cluster landed 2026-08-02, see amendment) |
| 2a | Close out the v0.1.0 cut set | **Closed** — all four planned items landed |
| 2b | BL-210 oral-history pivot (nations/corps rewrite) | **Closed** — all four rungs built (BL-217, BL-208, BL-218, BL-219) |
| 3 | Corp AI stage B + skill harness | **Closed** — BL-203, BL-204 both landed |
| 4 | Communication surface (BL-205 chat log) | **Mostly landed** — slice 1 complete 2026-07-26/28; only the C-route remainder (§7 Stage C) stays open, unstaffed |
| 5 | Era −1 history sim, 0–2000 CE (BL-271–275) | **Closed 2026-08-10** — four of five landed; BL-274 and BL-317 carried to v0.3.0 |
| 6 | The release sprint | **Closed** — five versions tagged; every cut minor now carries a done-definition |
| 7 | The stub minors become releases | **Closed** — v0.1.3 and v0.1.4 cut; post-v0.1.0 swept |
| 8 | Who the player is (design only) | **Closed 2026-08-10** — BL-094 rewritten as the militia, BL-350 filed, v0.3.0 roster reconciled |
| 9 | The militia takes the field | **Closed 2026-08-10** — v0.1.5 cut; BL-325/BL-331 landed, BL-332 designed and re-versioned |
| 10–14 | Re-sequenced 2026-08-10 (the living world inserted) | **Overtaken** — Sprints 10/11 closed 2026-08-11; Sprints 12–14 superseded 2026-08-12 by the 0 CE refocus (NR-177), never opened |
| 15 | The 0 CE refocus (retro-recorded) | **Closed 2026-08-12** — epoch 0, 3× map, Era −1 sim wired in, mercenary seam designed |
| 16 | The mercenary vertical slice | **Closed 2026-08-24** — re-planned 2026-08-23 in dependency order (BL-569→BL-578); all ten landed and v0.1.15 was cut |
| 17 | Economy breadth: the chain is the growth track | **Open 2026-08-15** — chain-depth spine (BL-428) → roster/methods (BL-429/430) → UI (BL-431) → guard harness (BL-432); cuts v0.1.17 |
| 17b | The shell stops fighting the map | **Open 2026-08-24** — batch delivery over BL-596–BL-604 in three parallel slices; BL-599/BL-600 landed same-day; four design calls gate three of the remaining items |
| 18b | Roster invariants (retro-recorded) | **Closed 2026-08-16** — BL-432 landed; BL-435 paused 4/6; BL-436 filed |
| 19 | The economy tells the truth | **Closed 2026-08-17 — goal NOT met.** The blame moved three times and landed on supply; goldens left red and unblessed |
| 25a | The draw (upkeep, ordnance, convoy seam) | **Closed 2026-08-18** — 6/6; goldens left red and attributed |
| D1 | A tech can express a buff | **Closed 2026-08-19** — BL-479 complete same-day; 35/35; BL-443 rider deliberately not taken (NR-296 is Ben's) |
| P1 | The province becomes a thing you can see, and then a thing worth seeing | **Closed 2026-08-21** — done_when met by BL-515, then overshot by five: the NR-438/439 ceiling ruling, BL-519 axis split, BL-520 texturing, BL-516 water kinds, BL-521 click injection. **Owed: nothing was rendered** |
| N1 | The two spines, landed inert | Closed 2026-08-23 — all three landed inert; two of the three were UNSOUND and were fixed in the closing pass (NR-546, NR-547) |
| N2 | The spines move | Closed — three lanes merged; one lane’s interpretation withdrawn after adversarial verification (NR-554) |

Sprint numbers 20–32 (and the lettered Fall-arc lanes B1–B3, C1–C3, D2–D4, plus N3, N4, ST1) do
**not** appear above — they were never-executed open/proposed sprints, deleted outright 2026-08-24
rather than kept as closed placeholders (see the ceiling note above). If one of these is picked
back up, it is authored fresh against the current docs, under a number that fits the ceiling —
not resurrected under its old number.

**Next up.** As of **2026-08-24**, Sprint **16** closed with v0.1.15 cut, **17** (economy breadth)
runs on, and the third slot the count cap left open is now filled by **17b** — numbered under the
ceiling by construction, and slotted between 17 and 18 on Ben's instruction because 17 is the
economy work in the same span and this is the UI work beside it. It is the project's first
deliberate **batch delivery of a UI theme**: nine items filed together, three parallel slices,
one cross-slice review, one re-bless at the close.

**BL-514** (blend all tiles) stops being parked with this sprint. It was held on Ben's own
sequencing — *"hold off on that until I see what it looks like with larger provinces"* — and
the look has now happened at 7–12 tiles: he called the result a blur. So the held question is
answered, against the direction it proposed, and it is folded into **BL-597** rather than left
waiting. **BL-518** (the Era −1 sim redrawing borders as its wars resolve) is still not part of
any active sprint and stays parked in `backlog.json`.

**Also deleted 2026-08-24, same basis:** six orphaned backlog items (BL-579–BL-584) a concurrent
review-queue-purge session had filed with no sprint attached, plus Sprint 32's own three items
(BL-595–BL-597, Logistic Points / Throughput lens) — all unstarted, all removed from
`backlog.json` outright rather than archived.

**The standing debt out of P1**, worth repeating here because it spans four items: nothing built in
that sprint was ever *rendered*. The session ran in a container that cannot build the GUI, so every UI
half is compile-clean and arithmetically checked and visually unseen, and no golden was blessed. For a
sprint whose own method note is *build it, look at it, then rule*, that is the thing to fix first.
