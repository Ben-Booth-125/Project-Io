# Project Io — Needs Review

**Ben's review queue.** Readable mirror of [`NEEDS_REVIEW.json`](NEEDS_REVIEW.json),
which is canonical — the JSON wins on any disagreement.

> **Generated file.** Produced by `node tools/session/render_needs_review.js`.
> Edit the JSON, then re-run; hand edits here are overwritten.

Things here are waiting on **your judgement**, not on work. Three kinds:

| Kind | Meaning |
|---|---|
| **question** | An open call nobody has made. Not blocking — a blocking item is a backlog entry with `blocked_on` set. |
| **decision-taken** | A call made **on your behalf** so work could continue. Recorded so it can be *overturned* rather than quietly becoming precedent. |
| **observation** | Something noticed in passing, too small or too cross-cutting to file, that a human should still see. |

**How this differs from the neighbours.** [`review.json`](review.json) is a *blocker* list —
items blocked on a visual artifact only you can produce; work there cannot proceed at all.
[`backlog.json`](backlog.json) is *work*. Entries here are neither: they are questions and
reversible calls. If an answer creates work, file a backlog item and resolve the entry with
that item's id.

This queue is **transient**: resolved entries are pruned promptly rather than kept for
posterity — the reasoning lands in code, an authority doc, or a backlog item at the moment
the work happens, and that is the durable record. What stays here is what is still open.

*1 entries — 1 open, 0 resolved.*

---

## Open

### NR-886 — CALLS LEFT BY WAVE A: seven readings on merged work that only Ben can judge
*question · raised 2026-09-16 · from Backlog wave A close-out. Ben stopped the backlog at wave A ("reinvent anything important later"), so these are recorded as calls rather than filed as work.*

1. COASTAL GROUND IS CHEAPER TO TAKE (BL-1022, your NR-828 ruling said a yes comes to you). Deaths per region taken over 16 seeds: inland 3,084, coastal 2,613 (about 15% cheaper, in 12 of 16 seeds), apparently from smaller coastal garrisons rather than the sea-legs ration; the cheapest route is the held-shore forage crossing (2,028). Is that the inversion you rejected?
2. TRADE RE-BASE HALF-HOLDS (BL-1021). Trade now helps a realm pay its armies but not its campaigns; and with only three trade classes a region earns at most 80 a year, so income grows with regions held, not with kinds of ground reached -- which was the reason for the re-base. The reach floor and the cross-border reading were chosen, not ruled.
3. SOLVENCY NO LONGER GATES THE SEAT (BL-1020). 74 of 84 shortlisted seats have a negative trailing net. The live click was never run.
4. THREE HARNESS ROWS WENT HONESTLY RED ON THE 12-TICK SETTLE (BL-1008): spawn_solvency R4 (no rival fields a standing force within 12 ticks -- read longer, reword as "can afford to hire", or stay red); acquisition_viability R1 (judges one quarter); and material_floor's "dead" definition (zero output across the whole settle).
5. SMALL GRUDGES NOW FADE LINEARLY (BL-842), faster than the design's 230-year half-life and faster on finer steps. Keeping the exact rate needs a stored remainder, which brings back stationary rounds.
6. THE CULTURE FOLD (BL-1017): a culture whose only ground is a future founding is not folded; kinship walks the living tree rather than coined_from. Neither moves any shipped world today.
7. DECISIONS TAKEN ON YOUR BEHALF: the far-trade reading's defaults (epoch 1960, first year of play, nearest by dispatcher pricing, cargo counted first) and the Digitisation readings' interpretations -- which generate at epoch 0 and so read "advanced chains" as zero by construction.

**Why it matters.** Each sits on code now on main and re-blessed. None blocks anything, because the backlog is empty; they are the questions to settle before any of that work is picked up again.

> **Recommendation:** Take 1 and 2 whenever Exploration or the Era -1 economy is next touched; 3 before the seat canvas is designed (it sits beside NR-885); 4-7 can wait for the harness or item that next reads them.

*Files: `tools/verify/history_sweep.cpp`, `src/world/history_sim.cpp`, `src/world/spawn_seat.cpp`, `tools/verify/spawn_solvency.cpp`, `tools/verify/acquisition_viability.cpp`, `tools/verify/material_floor.cpp`, `tools/verify/haulage_measure.cpp`, `tools/verify/digitisation_sim_harness.cpp`*

---

## Resolved

Kept, not pruned: the reasoning is the point. Prune only in a deliberate sweep, once the
answer has landed in an authority doc.

