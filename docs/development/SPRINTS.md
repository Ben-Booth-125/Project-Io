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
that a session doesn't have to weigh a long tail of undocumented parallel work. On adoption,
every open/proposed sprint except 16 and 17 was closed as descoped (not landed); pick back up
from `sprints.json`'s `notes` on the item you want to reopen, one at a time as 16/17 close out.

## Where things stand (updated 2026-08-23)

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
| 16 | The mercenary vertical slice | **Open 2026-08-12** — re-planned 2026-08-23 in dependency order (BL-569→BL-578); cuts v0.1.15 |
| 17 | Economy breadth: the chain is the growth track | **Open 2026-08-15** — chain-depth spine (BL-428) → roster/methods (BL-429/430) → UI (BL-431) → guard harness (BL-432); cuts v0.1.17 |
| 18b | Roster invariants (retro-recorded) | **Closed 2026-08-16** — BL-432 landed; BL-435 paused 4/6; BL-436 filed |
| 19 | The economy tells the truth | **Closed 2026-08-17 — goal NOT met.** The blame moved three times and landed on supply; goldens left red and unblessed |
| 20 | Not yet chosen | **Closed 2026-08-23** — descoped under the 3-active-sprint cap; three candidates still recorded in `sprints.json` for later |
| 25a | The draw (upkeep, ordnance, convoy seam) | **Closed 2026-08-18** — 6/6; goldens left red and attributed |
| 26–33 | **The Fall** — the arc to the Era −1 collapse | **Closed 2026-08-23** — descoped under the 3-active-sprint cap; eight rulings stay recorded in `sprints.json` |
| 26a | The three authority edits | **Closed 2026-08-23** — descoped under the cap; determinism case was mid-flight |
| 26b | Close-out and the doc truth pass | **Closed 2026-08-23** — descoped under the cap; doc pass was mid-flight |
| 27 | The run is retained, and its failure falsifiable | **Closed 2026-08-23** — descoped under the cap; Lane A, BL-384 assertion half only |
| B1 | The substrate becomes a number | **Closed 2026-08-23** — descoped under the cap; Lane B census harness was mid-flight |
| C1 | Stance, and the first cut of Logistic Points | **Closed 2026-08-23** — descoped under the cap; BL-464 design half never taken |
| W1 | The watch — an agent plays, a human watches | **Closed 2026-08-23** — descoped under the cap; all three items had landed |
| D1 | A tech can express a buff | **Closed 2026-08-19** — BL-479 complete same-day; 35/35; BL-443 rider deliberately not taken (NR-296 is Ben's) |
| D3 | A law has an author, and a tax is a transfer | **Closed 2026-08-23** — descoped under the cap; BL-480 had landed, harnesses green |
| B2 | Roads that reach, ports that mean something | **Closed 2026-08-23** — descoped under the cap; Lane B road_generation cuts unstarted |
| B3 | Density, and corporations stop all being the same thing | **Closed 2026-08-23** — descoped under the cap; density clamp still sized for the 180×84 map |
| C3 | The engagement trigger, and the fight you can watch | **Closed 2026-08-23** — descoped under the cap; BL-467 had landed (26/26) |
| D4 | The border costs something (the international half) | **Closed 2026-08-23** — descoped under the cap; depended on D3 + Lane B, neither active |
| P1 | The province becomes a thing you can see, and then a thing worth seeing | **Closed 2026-08-21** — done_when met by BL-515, then overshot by five: the NR-438/439 ceiling ruling, BL-519 axis split, BL-520 texturing, BL-516 water kinds, BL-521 click injection. **Owed: nothing was rendered** |
| ST1 | A global style sheet — narrowing down the visual language | **Closed 2026-08-23** — descoped under the cap; Joe's design track paused |
| N1 | The two spines, landed inert | Closed 2026-08-23 — all three landed inert; two of the three were UNSOUND and were fixed in the closing pass (NR-546, NR-547) |
| N2 | The spines move | Closed — three lanes merged; one lane’s interpretation withdrawn after adversarial verification (NR-554) |
| N3 | The spines get a caller | **Closed 2026-08-23** — descoped under the cap; absorbed into Sprint 16's BL-572 |
| N4 | The channel becomes visible | **Closed 2026-08-23** — descoped under the cap; unopened |
| 32 | Logistic Points, and the map that shows them through both fogs | **Gated 2026-08-23** — third sprint under the cap; BL-595/596/597 designed, **paused pending NR-583** — no promotion until Ben rules |

Every other sprint number that has ever appeared in this file (3–5's original theming, 17–18,
20–24, 25b, and the full Lane A/B/C/D breakdown of the 26–33 arc — 28, 29, 30, 31, B2, B3, C2,
C3, D1–D4) is recorded in `sprints.json` with its full goal/planned/risk/notes prose, whether it
closed, was superseded, or was descoped under the 2026-08-23 cap — each carries a `notes` line
saying so, so reopening one is a lookup, not a re-derivation.

**Next up.** As of **2026-08-23**, three sprints are active under the cap: **16** (mercenary
vertical slice), **17** (economy breadth), and the newly authored **32** — Logistic Points and
the Throughput lens, closing the seam where exploration (survey) and logistics (the reach field)
meet on the same canvas, correctly rendered through both fogs. Everything else that was open or
proposed — the Fall gate (26a/26b/27), Lane B (B1/B2/B3), Lane D (D3/D4), C1, C3, W1, ST1, N3, N4,
and Sprint 20's three candidates — was closed 2026-08-23 as descoped, not landed; each keeps its
prose in `sprints.json` for whoever reopens it. **BL-518** (the Era −1 sim redrawing borders as
its wars resolve) and **BL-514** (blend all tiles, HELD) are not part of any active sprint and
stay parked.

**Sprint 32's sequencing is flagged, not assumed (NR-583).** ROADMAP.md names Logistic Points for
**v0.1.20**, several minors past 16 (v0.1.15) and 17 (v0.1.17) — pulling it forward is a
deliberate jump under the new cap, made because Ben named this theme directly, not a default.
Nothing in LP's own dependency chain is unmet (the reach field, roads, and stance/hostility are
all already built) — the open question is release-numbering, not build-order.

**PAUSED 2026-08-23, pending NR-583.** BL-595/596/597 are designed but not promoted — do not move
any of them into REFINED.md or start build until Ben rules on NR-583's sequencing question. This
is the sprint's own gate, ahead of any other work on it.

**The standing debt out of P1**, worth repeating here because it spans four items: nothing built in
that sprint was ever *rendered*. The session ran in a container that cannot build the GUI, so every UI
half is compile-clean and arithmetically checked and visually unseen, and no golden was blessed. For a
sprint whose own method note is *build it, look at it, then rule*, that is the thing to fix first.
