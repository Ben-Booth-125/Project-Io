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

UPDATE 2026-09-17 (item 3): the "74 of 84 shortlisted seats negative" figure came from player_seed_sweep before it reached app parity (BL-1030). On the app's world, over the 16 library seeds: 109 of 170 specialists shortlisted, 81 of the 109 (74.3%) with negative trailing net, every shortlisted seat with a processor, and a different corporation seated on every seed. The call stands; its number moved.

**Why it matters.** Each sits on code now on main and re-blessed. None blocks anything, because the backlog is empty; they are the questions to settle before any of that work is picked up again.

> **Recommendation:** Take 1 and 2 whenever Exploration or the Era -1 economy is next touched; 3 before the seat canvas is designed (it sits beside NR-885); 4-7 can wait for the harness or item that next reads them.

*Files: `tools/verify/history_sweep.cpp`, `src/world/history_sim.cpp`, `src/world/spawn_seat.cpp`, `tools/verify/spawn_solvency.cpp`, `tools/verify/acquisition_viability.cpp`, `tools/verify/material_floor.cpp`, `tools/verify/haulage_measure.cpp`, `tools/verify/digitisation_sim_harness.cpp`*

### NR-889 — CALL: a charter budget cannot raise a body's firm count above Pass 6's breadth cap — which ruling gives way?
*question · raised 2026-09-17 · from BL-1032 (charter budget seam), its lane's novelty flag and cold review, on seed 0 with a synthetic budget (test input only).*

TWO RULINGS MEET. CORPORATION_GENERATION.md Pass 6 (Ben, 2026-09-06): density IS the per-resource firm cap — a body holds at most 8 firms per demanded good, so the count is exactly specialists + 8 x goods with demand. DIGITISATION.md § 1 (Ben, 2026-09-15/17): on a Digitisation world the budget decides how many firms stand where, and breadth only decides which goods.
WHAT THE SEAM SHOWS. The budget path keeps Pass 6's gap selection and its cap, so the cap still binds. Seed 0: the legacy landscape lays 81 background firms. A synthetic budget at 1x (87 points) charters 2 specialists and 79 firms and spends everything; at 4x (348 points) it charters 11 specialists and 81 firms and leaves 219 points unspent as no_gap across 204 centres. So a richer world moves firms toward its cities and adds specialists, but cannot make a body denser than breadth x 8.
OPTIONS. (a) Keep the cap: capital beyond breadth x 8 stays unspent, and density follows cities only in where firms stand, not how many there are. (b) Lift the per-resource cap on a budget world only: capital decides the count, bounded by the 200-per-body anti-runaway and by what each market can absorb. (c) Scale the cap with the body's charter capital. (b) and (c) amend the 2026-09-06 ruling for Digitisation worlds, and raise the live tick cost BL-1033 measures.

READINGS TAKEN ON YOUR BEHALF in the seam (reversible, none moves today's world): rung 2 of a charter's anchor is its centre's own region, not BL-283's home-plus-three; a specialist that finds no ground in either window is not chartered and its price is counted unspent (no asset-light specialist); a firm that fails to place ends that centre's firm spend; only a firm's anchor is held to the window — its other holdings walk outward as Pass 3 lays them, now reported by distance; the synthetic test budget weighs centres by a seeded 1/rank law so that a 1x budget can afford any specialist at all (population is never read).

UPDATE 2026-09-17 (Ben, elicitation): measure first. BL-1033 runs the per-resource cap kept and lifted side by side on budget worlds; the call is ruled on its tick cost and unspent points. The entry stays open until then.

**Why it matters.** BL-1033 (charter budget cost) measures tick cost at 1x/2x/4x. Under (a) the 4x world is barely denser than 1x and the cost reading understates what Beat 1 could ask for; under (b) or (c) the measurement must lift the cap to mean anything. Better ruled before BL-1033 runs.

- (a) keep the per-resource cap on budget worlds; capital beyond it stays unspent
- (b) lift the per-resource cap on budget worlds; the 200-per-body cap and market absorption bound density
- (c) scale the cap with the body's charter capital

> **Recommendation:** Measure (a) and (b) side by side in BL-1033 before ruling: the cost sweep already runs the province cap on and off, and the per-resource cap is the same kind of switch. Rule after reading tick cost and unspent points at 4x under both.

*Files: `src/world/corporation_generation.cpp`, `docs/generation/CORPORATION_GENERATION.md`, `docs/generation/DIGITISATION.md`, `tools/verify/player_seed_sweep.cpp`*

---

## Resolved

Kept, not pruned: the reasoning is the point. Prune only in a deliberate sweep, once the
answer has landed in an authority doc.

