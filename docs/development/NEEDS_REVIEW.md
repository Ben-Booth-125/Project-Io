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

*2 entries — 2 open, 0 resolved.*

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

### NR-887 — PICK: which library seeds to replace — three lost their reason, two you named still hold, and the floor case got rich
*question · raised 2026-09-17 · from BL-1026 (seed library re-read). Library sweep of the sixteen seeds plus a 48-seed pool sweep on main at 4c9feb4c, Release, 2026-09-17 (136 s and 295 s). The two runs agree on all 1,552 shared fields.*

Every library seed moved under wave A; the fingerprints are re-blessed (your ruling). Your ruling named 4, 6, 12 and 13 for replacement on partial evidence (seven seeds were visible then). With all sixteen read against the pool:

LOST THEIR REASON — replace:
- Seed 4, "The frontier with no neighbours": it has neighbours now (3 neighbour wars, displacement 27.3, 1 subject). Candidates: 9 (0 neighbour wars, 229 frontier battles, 50 polities, 1 subject, 41 flows, chest 380 — the ordinary one) or 16 (0 neighbour wars, 728 frontier battles, 42 polities, 2 subjects, 49 flows, chest 43k — the busy frontier).
- Seed 6, "The inward world" (wealthy, held, roads): displaced 1.80, chest 127k, 9 roads. Candidates: 40 (held 0.88, chest 1.64M, 26 post roads — second most — 36 flows, 33 polities) or 39 (held 0.49, chest 216k, 16 roads, 42 flows, 49 polities).
- Seed 19, "The insular rich" (NOT on your list): chest 285 and displaced 1.39, so neither rich nor inward. Candidates: 7 (held 0.99, chest 2.6M, 20 polities, 23 flows) or 38 (held 0.50, chest 8.6M, 30 polities, 27 flows).

NAMED, BUT STILL ANSWER THEIR QUESTION — keep recommended:
- Seed 12, "The most inward world": still inward with no subjects and no capital (20 flows, chest 274), and now the only SILENT seed in the pool (12 Exploration battles). Only the verdict word changed. Candidate if you still want it out: 30 (held 0.49, chest 335, 2 subjects — but 69 polities and 57 flows, so not sparse).
- Seed 13, "The colonial world": 7 subjects, tied for the most in the pool, at a middling 50 polities and 53 flows. Displacement fell from 46 to 1.16. Candidates if the displaced half matters: 36 (4 subjects, displacement 5.3, 43 flows) or 20 (4 subjects, displacement 73.5, 1,250 frontier battles, chest 173).

CHANGED SHAPE — your call:
- Seed 17, "The quiet world", the floor case: still the fewest polities (17), corridors (786) and flows (16), but no longer SILENT (41 Exploration battles) and no longer poor (median chest 11.5M, second in the pool). Keep as the floor on breadth, or candidates for a floor on capital too: 28 (22 polities, 11 flows and 320 volume — the lowest — 0 roads, chest 66k) or 3 (30 polities, 27 Exploration battles, 19 flows, 9 treaties — the fewest — chest 54k).

HOLD, REWORDED TO TODAY'S NUMBERS: 46, 11, 31 (colonial tag dropped: 3 subjects), 37, 41 (treaties tag dropped: 68), 43, 32 (now also the poorest chest, 163), 10, 25, and 0 by definition.

**Why it matters.** Sprint 43 reads every Digitisation reading per seed, never the median, so each library seed has to be the question it says it is. A replacement is cheap now and expensive after BL-1027..1029 have written per-seed baselines against the old list.

- As recommended: replace 4, 6 and 19 (pick one candidate each); keep 12, 13 and 17 with their rationale reworded.
- As ruled: replace 4, 6, 12 and 13, and also 19; keep 17.
- Replace every seed flagged, including 17.

> **Recommendation:** Option 1, with 9 for seed 4, 40 for seed 6 and 38 for seed 19 — each is the cleanest match to the question the old seed was chosen to put. For 19, seed 7 matches the old size and chest better, but its displacement of 0.99 sits on the held line and would likely flip on the next world-mover; 38 is held at 0.50.

*Files: `docs/generation/seed_library.json`, `seed_library_sweep.json`*

---

## Resolved

Kept, not pruned: the reasoning is the point. Prune only in a deliberate sweep, once the
answer has landed in an authority doc.

