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

UPDATE 2026-09-17 (item 7): answered by BL-1029 (Digitisation readings at 1960). digitisation_sim_harness --through 1960 reads the shipped world continued to 1960 beside a fingerprint-checked 1660 control; "advanced chains" stays a STRUCTURAL ZERO there too, because the recipe band follows epoch_year, and now prints as one. The far-trade reading (haulage_measure) still runs epoch_year 1960, the superseded arc; it moves with the epoch flip. Items 1-6 unchanged.

**Why it matters.** Each sits on code now on main and re-blessed. None blocks anything, because the backlog is empty; they are the questions to settle before any of that work is picked up again.

> **Recommendation:** Take 1 and 2 whenever Exploration or the Era -1 economy is next touched; 3 before the seat canvas is designed (it sits beside NR-885); 4-7 can wait for the harness or item that next reads them.

*Files: `tools/verify/history_sweep.cpp`, `src/world/history_sim.cpp`, `src/world/spawn_seat.cpp`, `tools/verify/spawn_solvency.cpp`, `tools/verify/acquisition_viability.cpp`, `tools/verify/material_floor.cpp`, `tools/verify/haulage_measure.cpp`, `tools/verify/digitisation_sim_harness.cpp`*

### NR-888 — GATE (sprint 43): rule the two inherited weaknesses and the span's clock on the 1960 baseline
*question · raised 2026-09-17 · from Sprint 43 (the 1960 baseline): BL-1027 (span cost to 1960), BL-1028 (weakness counters to 1960), BL-1029 (Digitisation readings at 1960). exploration_sweep_1960.json and digitisation_readings_1960.json at the repo root, 16 library seeds, the shipped world with Exploration's own call continued to 1960, Release. Read per seed, never the median.*

THE BASELINE (what 300 more years of Exploration's own forces do):
- Cost: the 1660 -> 1960 half costs a median 1.7-1.9 s a seed (two runs), at most 10-15 s, less than the 460 years before it. Affordable at the 4-year band.
- The world coasts. Urban share 14.7% -> 14.6% (rising in 5 of 16); subjects median 3 -> 3, fewer in 0 of 14 worlds; standing flows 51 -> 52; checkered regions median 1 -> 0; conquests per century fall on every seed. Only industrial polities climb (median 12 -> 21). No Digitisation beat arrives as momentum.

W1 — THE ALARM IS A SEAL, NOT A DETERRENT. Near-home alarm reads at the ceiling 89.7% before 1660 and 88.5% after; 99.1% / 99.7% of near-home campaign candidates are treaty-blocked; pooled displacement 3.02 -> 1.74 across the halves, below 1 after 1660 on seeds 46, 40, 13, 41, 38. On the unmerged branch, an alarm set from measured capability collapsed displacement to 0.50 and no weight restored it.
  Inherit: Beat 3's proxy war carries its own displacement force, and Digitisation reads epoch Alarm from its own scale, never through the 5000 reference. No re-bless.
  Revisit Exploration: re-derive the capability reference on main and find a force other than the weight. Re-blesses the 1660 world and the seed library; open-ended.

W2 — REFRAMED BY MEASUREMENT: POLITIES MEET; WHAT THEY MEET NEVER BINDS. 1,250 first contacts by 1660 (3 to 300 a world), but 0 of 492 pairs first met after 1200 hold non-aggression at 1660 and 0 of 506 at 1960; another verb wins 88% (96% after 1660) of the rounds an unmet target clears. After 1660, 43 new contacts in 300 years across the library, none by crossing on 8 seeds. Far trade, emigration and a world war that spreads by mutual defence all need far pairs that bind.
  Inherit: Digitisation owns a force that binds far pairs (trade, migration, alliance) and raises contact itself.
  Revisit Exploration: re-price the far-pair treaty penalty or replace it with a force, so new contacts can bind before 1660. Re-blesses.

THE CLOCK. DIGITISATION.md names none. Exploration's 4-year band is measured affordable; a 1-year band costs about four times as much and would give war deaths, migration and decolonisation finer grain.

CORRECTED ON THE WAY: EXPLORATION.md's contact paragraph rested on "the seeds that met one polity", an unmerged snapshot artefact; it now states main's measurement (9cb4cb7e).

**Why it matters.** Sprint 45 cuts the Digitisation span itself; sprint 44 (the corporate web's plumbing) depends on neither weakness. Inheriting keeps Exploration frozen, as Ben directed on 2026-09-11 (stop adding Era -1 mechanism), and moves both forces into Digitisation's beats; revisiting re-blesses the 1660 world a second time in a week.

- W1 inherit, W2 inherit, 4-year clock (recommended).
- W1 inherit, W2 revisit Exploration, 4-year clock.
- Revisit both in Exploration before sprint 45.
- Measure once more first: the existing --set visible_capability_reference and treaty_far_penalty_q tuning flags on the 1960 sweep, no world moved.

> **Recommendation:** Option 1. The baseline shows the continued world carrying no momentum into any beat, so Digitisation supplies each force anyway; a far-binding force and a proxy-war displacement belong to beats the doc already owns (Beat 2, property 7, Beat 3). Keep the 4-year band until war deaths are built and measured.

*Files: `exploration_sweep_1960.json`, `digitisation_readings_1960.json`, `docs/generation/EXPLORATION.md`, `docs/generation/DIGITISATION.md`*

---

## Resolved

Kept, not pruned: the reasoning is the point. Prune only in a deliberate sweep, once the
answer has landed in an authority doc.

