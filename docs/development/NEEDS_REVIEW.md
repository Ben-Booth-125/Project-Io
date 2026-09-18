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

*3 entries — 3 open, 0 resolved.*

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

READINGS (BL-1033, charter_cost_sweep.json, seeds 0/28/46, Release, serial; synthetic TEST budget, seeded weights, never population; firm price 1 point; live ms is the median economy tick over 8 ticks after the seat, a stated LOWER BOUND on the app's step — comms, history recorders and the strategy readout are not timed):
  specialists/firms and live tick         seed 0          seed 28         seed 46
  legacy (no budget)                     6/81   3.1 s    6/81   4.2 s    8/81  11.1 s
  1x  cap kept                           2/79   6.4 s    3/75  15.7 s    2/81   8.6 s
  2x  cap kept                           5/81   8.1 s    6/81  25.7 s    5/81   7.4 s
  4x  cap kept                          11/81   6.7 s   11/81  28.0 s   11/81  10.5 s
  1x  cap lifted                         2/79   7.2 s    3/75  14.5 s    2/81   8.8 s
  2x  cap lifted                         5/154 14.3 s    6/150 23.1 s    5/158 20.3 s*
  4x  cap lifted                        11/200 18.5 s   11/200 26.8 s   11/200 43.6 s
  2x  cap kept,   specialist price 8     2/81   6.3 s    2/81  16.8 s    2/81   7.6 s
  2x  cap lifted, specialist price 8     2/158 11.8 s    2/158 20.7 s    2/162 18.4 s
  * straddled a PC sleep; re-run (machine in use): 25.2 s against that run's legacy 14.8 s, the same 1.7x.
WHAT IT SAYS. (1) Keeping the cap freezes background firms at 81 a body on every seed whatever the budget; at 4x, 208-228 of 348 points go unspent as no_gap and only specialists grow (2 -> 5 -> 11). (2) Lifting it buys firms up to the 200-per-body anti-runaway (85-110 points then unspent as body_cap), so at 4x the runaway guard, not the city, sets density. (3) Density costs 4-6x the legacy live tick at 4x lifted. (4) WHERE firms stand also costs: on seed 28 even the cap-kept rows run 3.7-6.7x the legacy tick with FEWER firms, so clustering around cities is itself a cost, not only the count. (5) The search barely moves (0.8-2.2 s per evaluation, flat by density on each seed). (6) The specialist price is a clean dial: at 8 firm charters a 2x budget buys 2 specialists instead of 5-6. (7) The province cap is not what binds at density (at most 19 points), and turning it off changes ticks by under 10%.

**Why it matters.** Beat 1 (sprint 45) produces real budgets; this ruling decides whether capital beyond breadth x 8 buys firms or stays unspent, and what that costs every live tick. The readings are in.

- (a) keep the per-resource cap on budget worlds; capital beyond it stays unspent
- (b) lift the per-resource cap on budget worlds; the 200-per-body cap and market absorption bound density
- (c) scale the cap with the body's charter capital

> **Recommendation:** Keep the cap (a) for now. Lifting it multiplies an already-slow tick 4-6x and hands density to the 200-per-body runaway guard rather than to the city; and clustering alone already costs up to 6x on one seed. Revisit (b) or (c) when the economy tick is cheaper, with this table as the baseline. The specialist price sits between 4 and 8 firm charters depending on how many seats you want a rich world to offer.

*Files: `src/world/corporation_generation.cpp`, `docs/generation/CORPORATION_GENERATION.md`, `docs/generation/DIGITISATION.md`, `tools/verify/player_seed_sweep.cpp`*

### NR-890 — NOVEL: a copied world ticks differently from its original — filed as BL-1034, not scheduled
*novel-work · raised 2026-09-17 · from BL-1033 lane and its cold review (sprint 44).*

A determinism defect no sprint owns: on seed 28 a world copied before or after the landscape settles to D_settle 18F78EB9B2B20F29 instead of the pinned 265C48A23E313B1A. Shipped worlds are unaffected today (the search copies but never ticks a copy). Filed as BL-1034 (world copy ticks diverge) with a reproduction-first plan; sprint 44 works around it by never copying a world before ticking.

**Why it matters.** Determinism is a standing rule, and the defect is latent: the first feature that previews or branches a world by copying it will diverge silently. It is not in sprint 44's goal, so scheduling it is your call.

- Schedule BL-1034 into sprint 45 ahead of Beat 1
- Leave it filed until a feature needs a ticking copy

> **Recommendation:** Schedule it early in sprint 45: Beat 1 and the wizard's time-lapse round are the likeliest first features to copy a world.

*Files: `src/world/world.hpp`, `tools/verify/harness_params.hpp`*

---

## Resolved

Kept, not pruned: the reasoning is the point. Prune only in a deliberate sweep, once the
answer has landed in an authority doc.

