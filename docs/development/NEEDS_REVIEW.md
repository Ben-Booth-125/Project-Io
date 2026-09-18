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

### NR-896 — CALL: forest and coal both read the BEST held region, which grows with realm size — and Charcoal now outnumbers Coke
*question · raised 2026-09-18 · from BL-1041 (industry points) build lane and cold review, after the wave 1 ruling "score forest like fuel" (NR-895). Figures are the lane's 16-seed run; the main session re-runs them before quoting them as evidence.*

With forest scored like fuel (the best held region against the world mean, capped 1000) the Fuel Doctrine split moved from 391 coke / 132 charcoal / 333 neither to 200 / 319 / 337. 261 of the 319 Charcoal polities had passed the seam gate — Coke was open and they took Charcoal. Mean held forest score by side: coke 739, charcoal 859, neither 306. TREES.md § The scorer: 'Never a rank, never a term inside the scorer that grows with the polity's own size.' A best-of-held maximum can only rise as a realm grows (to its 1000 cap), so both ground_fuel (already a best seam) and ground_forest now breach the letter of that rule, weakly; INDUSTRY_TREE.md rules exactly this shape. The docs disagree, and wide realms read near the cap on both terms.

**Why it matters.** The Fuel Doctrine is the tree's central fork: coke to scale and the Blast Works, charcoal to quality and a ceiling. If breadth, not ground, decides it, the 1960 spread it manufactures is about realm size.

- Both terms read the SHARE of held regions whose score clears the world mean — size-neutral, like with like; the fuel GATE still passes on any held seam.
- Keep best-of-held for both, and write it into TREES.md as a stated exception to the size rule.
- Both terms read the held MEAN of the scores — size-neutral, but a large realm with one coalfield reads low.

> **Recommendation:** The share of held regions over the world mean, for both terms. It keeps "any seam opens the gate" and stops breadth from deciding the pull.

*Files: `src/world/history_sim.cpp`, `docs/generation/trees/INDUSTRY_TREE.md`, `docs/generation/trees/TREES.md`*

### NR-897 — DECISIONS TAKEN (and a call): the treasury-to-points constants, and treasury points landing only on the capital
*decision taken on your behalf · raised 2026-09-18 · from BL-1041 (industry points) build lane and cold review. Figures are the lane's 16-seed run, to be re-run by the main session.*

The lane chose, on your behalf: 1000 industry points per million urban heads per year (defines the point); a fuel factor of 250 + 750 x reading/1000 (floor 250, so fuel is at most a x4 lever); a treasury share of 250 per mille of the round's surplus, where surplus is the treasury's net rise across the round's earn and army/navy bills, floored at 0 and DEBITED (a conversion, not a copy), and tribute is not counted; and an exchange rate of 1000 points per treasury unit. At 1:1 the treasury could not move any region's rank; at 1000:1 the treasury supplies a median 69% of all points (0.16-0.82 by seed), and because treasury points land on the capital's own region (PROPOSED 2026-09-18, not overturned), one region holds up to 65% of a world's points (median top-region share 0.28). The charter budget follows the points, so charters would concentrate at capitals.

**Why it matters.** "Density follows cities" becomes "density follows capitals" if the treasury term dominates and lands in one place; the real-stockpile sweep (BL-1043) and the re-bless read this.

- Spread treasury points over the polity's centres by their urban scale, not the capital alone; keep the constants.
- Keep the capital landing; lower the exchange rate so treasury is a minority input (e.g. a third of points).
- Accept as built and judge on the real-stockpile sweep.

> **Recommendation:** Spread treasury points by scale over the polity's centres. A treasury builds its realm's works where its people are, and the capital still leads because it is usually the largest centre.

*Files: `src/world/history_sim.cpp`, `src/world/history_sim.hpp`, `docs/generation/DIGITISATION.md`*

---

## Resolved

Kept, not pruned: the reasoning is the point. Prune only in a deliberate sweep, once the
answer has landed in an authority doc.

