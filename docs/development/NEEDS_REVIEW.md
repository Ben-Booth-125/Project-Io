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

### NR-885 — CALL: the corpus says the player is a mercenary company and the code says it is a corporation picking a seat
*decision · raised 2026-09-16 · from Resolving NR-807 (the live arc). Ben ruled the ARC half — the industrial arc is the live product — and the identity half did not follow from it, but reading the corpus to write that ruling surfaced a disagreement the arc question was hiding.*

CONCEPT.md § Player identity and MANUAL.md § 1.2 say the player is a MERCENARY COMPANY: armed and for hire, not the state, it does not legislate or research, it PROCURES force rather than producing it, and it is paid for outcomes. That is Ben's 2026-08-12 ruling (NR-177) and it is load-bearing across roughly fourteen docs — it supplies the design test every system is measured against ("does this change what the company can FIELD, or what it must ANSWER TO?"), and CONCEPT.md explicitly says a system that only moves a cost or a price "is being designed for the corporate player the identity moved away from". BUT THE BUILT AND FILED WORK SAYS SOMETHING ELSE. BL-880 (CORPORATION_SELECTION_CANVAS) has Begin open a "corporation selection" canvas where the player picks a SEAT over the generated landscape — Ben, 2026-09-09, explicitly reversing the 2026-08-26 retirement of the selection screen. NR-881 describes the live shortlist the player picks from as "0 of 8 SPECIALISTS" gated on solvency and trailing net — corporate quantities. CLAUDE.md's own premise line says the game is one "in which CORPORATIONS extract, trade and fight". Era 1 in CONCEPT.md says the player competes "for terrestrial resources and market position". The campaign's verbs are corp verbs (apply_corp_command), the AI grants are written over background CORPORATIONS and nations, and the economy docs are a corporate economy throughout.

**Why it matters.** This is a doc-versus-code disagreement, and the standing rules say one of them is wrong and the fix is work rather than a footnote. It is not a wording nit: the mercenary identity supplies the DESIGN TEST used to accept or reject systems, so if the player is actually a corporation picking a seat, every system accepted on "does it change what the company can FIELD" was measured with the wrong instrument — and if the player really is a mercenary company, then the selection canvas, the specialist shortlist and the seat are building the wrong opening. IT WAS NOT SETTLED BY THE ARC RULING and must not be inferred from it. The 2026-08-12 identity was argued on a reason that names no century — a company reaches force directly where a corporation's levers are all economic, which is Conflict's route to being load-bearing — so moving the epoch from 0 CE to 1960 leaves that argument exactly as it was. A mercenary company at an industrial 1960 epoch is perfectly coherent. The tension is with the SEAT, not with the century.

- The player is a corporation that holds a seat; the mercenary framing is retired and the design test is re-derived. This matches the built opening (BL-880, the specialist shortlist) and CLAUDE.md's premise, and it costs a sweep of roughly fourteen docs plus a replacement for the FIELD/ANSWER-TO test — which existed to keep Conflict load-bearing, so the replacement has to do that job or Conflict quietly becomes flavour again.
- The player is a mercenary company and the seat is what it is HIRED FROM, not what it is. The selection canvas picks a base of operations and a specialism, not a firm; the shortlist's solvency gate is wrong for that reading and BL-1020 is already re-cutting it (it now reads the static landscape score rather than trading history, which suits either reading). Costs the least doc churn and keeps NR-177 intact; needs BL-880's canvas re-described.
- They are the same thing at different grain: the company IS a corporation in the economy's data model (it has one owner, a balance, holdings, filed returns) and a mercenary company in what it DOES. Say so once in CONCEPT.md and stop treating the two words as rivals — but only if the design test survives being stated that way, because the test is the part that is actually load-bearing.

> **Recommendation:** The second or the third, and the question that separates them is whether the SEAT is an identity or an address. The first is the most honest reading of what is being built and the most expensive, and it should not be taken by default just because the code drifted — that is the quiet precedent the standing rules warn about. Worth noting that nothing is blocked on this today: BL-1020 re-cuts the shortlist gate in a way that suits every option, and BL-880 has not been built. But it should be answered BEFORE BL-880, because a selection canvas is the first thing the player ever sees and it will state an answer whether or not one has been given.

*Files: `docs/CONCEPT.md`, `docs/MANUAL.md`, `docs/development/ROADMAP.md`, `docs/development/backlog.json`*

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

