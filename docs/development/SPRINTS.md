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

## Where things stand (updated 2026-08-18)

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
| 16 | The mercenary vertical slice | **Open 2026-08-12** — BL-377 playable end-to-end + BL-315 on the critical path; cuts v0.1.15 |
| 18b | Roster invariants (retro-recorded) | **Closed 2026-08-16** — BL-432 landed; BL-435 paused 4/6; BL-436 filed |
| 19 | The economy tells the truth | **Closed 2026-08-17 — goal NOT met.** The blame moved three times and landed on supply; goldens left red and unblessed |
| 20 | Not yet chosen | **Proposed 2026-08-17** — three candidates laid out; Ben's call |
| 25a | The draw (upkeep, ordnance, convoy seam) | **Closed 2026-08-18** — 6/6; goldens left red and attributed |
| 26–33 | **The Fall** — the arc to the Era −1 collapse | **Proposed 2026-08-18** — eight rulings settled; Sprint 26 is the gate. Supersedes 21, 23, 25b |
| 26a | The three authority edits | **Open 2026-08-18** — grants landed, v0.1.11 un-parked; determinism case in flight |
| 26b | Close-out and the doc truth pass | **Open 2026-08-18** — 6 items closed on evidence; doc pass in flight |
| 27 | The run is retained, and its failure falsifiable | **Open 2026-08-18** — Lane A; BL-384 assertion half only, expected RED |
| B1 | The substrate becomes a number | **Open 2026-08-18** — Lane B; substrate census harness in flight |
| C1 | Stance, and the first cut of Logistic Points | **Open 2026-08-18** — BL-448+449+461 build, BL-464 design-only |
| W1 | The watch — an agent plays, a human watches | **Open 2026-08-19** — all three items LANDED same day; first watch session ran (local model attached); open on the live-check rows + BL-481/BL-306 |
| D1 | A tech can express a buff | **Closed 2026-08-19** — BL-479 complete same-day; 35/35; BL-443 rider deliberately not taken (NR-296 is Ben's) |
| D3 | A law has an author, and a tax is a transfer | **Open 2026-08-19** — BL-480 LANDED, harnesses green; open on the NR-382 rate ruling + the live levy-line look |

Every other sprint number that has ever appeared in this file (3–5's original theming, 17–18,
20–24, 25b, and the full Lane A/B/C/D breakdown of the 26–33 arc — 28, 29, 30, 31, B2, B3, C2,
C3, D1–D4) is recorded in `sprints.json` with its full goal/planned/risk/notes prose, whether it
closed, was superseded, or is still a live proposal awaiting Ben's pick.

**Next up.** As of 2026-08-19 the open work is Sprint W1 (the watch — BL-412 agent seam, BL-408
god view, BL-411 strategy readout, toward v0.1.16), Sprints D1+D3 as one Lane D batch (BL-479
tech effect union, BL-480 law author + treasury, toward v0.1.11), plus the still-open Fall gate
sprints (26a remainder, 26b remainder, 27, B1) and C1's BL-464 design half. The 2026-08-19
version-alignment session also executed Sprint 26's dropped board-surgery half — see entry 26's
amendment and ROADMAP § the ancient arc for the v0.1.16 split (v0.1.19–v0.1.22 named). Sprint
20's three candidates remain Ben's call, unopened.
