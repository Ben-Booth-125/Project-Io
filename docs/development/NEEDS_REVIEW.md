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

*4 entries — 1 open, 3 resolved.*

---

## Open

### NR-900 — CALL: the top-third bar — rank every region or only those carrying the resource; and a cliff at the survey's 1000 cap
*question · raised 2026-09-19 · from BL-1059 (top-third bar) lane (commit 15713143) and its cold review, 2026-09-19. Raised first as a decision taken with a recommendation to keep it; the review showed it departs from the ruled wording, so it is re-raised as a call. Per-seed carrier counts owed from the next main-session run.*

NR-899 ruled 'the top third of every region's score ... fuel and forest clear at the same rate world-wide'. The lane ranked only NONZERO scores (k = floor(m/3) of m carriers; a tie band straddling the cut falls out whole). Two problems. (1) RARITY TILT RETURNS, milder: with fuel in 150 of 300 regions and forest in 285, nonzero-only clears 50 fuel regions (17% of the world) against 95 forest (32%); ranking every region clears about 100 of each, which is the ruling's 'same rate world-wide'. The built split is coke 239 / charcoal 277 / neither 340 (from 142 / 373 / 341). (2) A CLIFF AT THE CAP: survey scores clamp at 1000 (twice the world mean, settlement.cpp:377); if more than a third of carriers sit at 1000 the tie rule pushes them all out, the bar becomes 1001 and fuel clears NOWHERE on that world (30 capped: 30 clear; 31: none). The 16 library seeds avoid it (each bar clears 31.8-33.3% of carriers), but nothing fails if a seed hits it.

**Why it matters.** It decides which ground counts as a coalfield or a forest, so the 1960 Fuel Doctrine split BL-1043 measures and BL-1044 re-blesses.

- Rank every region's score (the ruling's words), and rank the unclamped survey share so a pile at the cap cannot empty the bar.
- Keep nonzero-only (clears a third of CARRIERS, not of the world), and rank the unclamped share.
- Keep as built, and make the harness fail when a bar reaches 1001 with three or more carriers.

> **Recommendation:** The first: it is what NR-899 says, it makes fuel and forest clear at one rate world-wide, and ranking the unclamped share removes the cliff rather than alarming on it. The per-seed carrier counts (fuel vs forest nonzero) will show how far apart the options really are; they come from the next main-session run.

*Files: `src/world/history_sim.cpp`, `src/world/settlement.cpp`, `tools/verify/digitisation_sim_harness.cpp`, `docs/generation/trees/INDUSTRY_TREE.md`*

---

## Resolved

Kept, not pruned: the reasoning is the point. Prune only in a deliberate sweep, once the
answer has landed in an authority doc.

### NR-896 — CALL: forest and coal both read the BEST held region, which grows with realm size — and Charcoal now outnumbers Coke
*question · raised 2026-09-18 · from BL-1041 (industry points) build lane and cold review, after the wave 1 ruling "score forest like fuel" (NR-895). Figures are the lane's 16-seed run; the main session re-runs them before quoting them as evidence.*

With forest scored like fuel (the best held region against the world mean, capped 1000) the Fuel Doctrine split moved from 391 coke / 132 charcoal / 333 neither to 200 / 319 / 337. 261 of the 319 Charcoal polities had passed the seam gate — Coke was open and they took Charcoal. Mean held forest score by side: coke 739, charcoal 859, neither 306. TREES.md § The scorer: 'Never a rank, never a term inside the scorer that grows with the polity's own size.' A best-of-held maximum can only rise as a realm grows (to its 1000 cap), so both ground_fuel (already a best seam) and ground_forest now breach the letter of that rule, weakly; INDUSTRY_TREE.md rules exactly this shape. The docs disagree, and wide realms read near the cap on both terms.

**Why it matters.** The Fuel Doctrine is the tree's central fork: coke to scale and the Blast Works, charcoal to quality and a ceiling. If breadth, not ground, decides it, the 1960 spread it manufactures is about realm size.

- Both terms read the SHARE of held regions whose score clears the world mean — size-neutral, like with like; the fuel GATE still passes on any held seam.
- Keep best-of-held for both, and write it into TREES.md as a stated exception to the size rule.
- Both terms read the held MEAN of the scores — size-neutral, but a large realm with one coalfield reads low.

> **Recommendation:** The share of held regions over the world mean, for both terms. It keeps "any seam opens the gate" and stops breadth from deciding the pull.

> **RESOLVED.** RULED (Ben, 2026-09-19): option A. Both ground_forest and ground_fuel read the share of held regions whose score clears the world mean; the fuel gate still passes on any held seam. Written into INDUSTRY_TREE.md § The scorer; the work is BL-1056 (points size-neutral).

*Files: `src/world/history_sim.cpp`, `docs/generation/trees/INDUSTRY_TREE.md`, `docs/generation/trees/TREES.md`*

### NR-897 — DECISIONS TAKEN (and a call): the treasury-to-points constants, and treasury points landing only on the capital
*decision taken on your behalf · raised 2026-09-18 · from BL-1041 (industry points) build lane and cold review. Figures are the lane's 16-seed run, to be re-run by the main session.*

The lane chose, on your behalf: 1000 industry points per million urban heads per year (defines the point); a fuel factor of 250 + 750 x reading/1000 (floor 250, so fuel is at most a x4 lever); a treasury share of 250 per mille of the round's surplus, where surplus is the treasury's net rise across the round's earn and army/navy bills, floored at 0 and DEBITED (a conversion, not a copy), and tribute is not counted; and an exchange rate of 1000 points per treasury unit. At 1:1 the treasury could not move any region's rank; at 1000:1 the treasury supplies a median 69% of all points (0.16-0.82 by seed), and because treasury points land on the capital's own region (PROPOSED 2026-09-18, not overturned), one region holds up to 65% of a world's points (median top-region share 0.28). The charter budget follows the points, so charters would concentrate at capitals.

**Why it matters.** "Density follows cities" becomes "density follows capitals" if the treasury term dominates and lands in one place; the real-stockpile sweep (BL-1043) and the re-bless read this.

- Spread treasury points over the polity's centres by their urban scale, not the capital alone; keep the constants.
- Keep the capital landing; lower the exchange rate so treasury is a minority input (e.g. a third of points).
- Accept as built and judge on the real-stockpile sweep.

> **Recommendation:** Spread treasury points by scale over the polity's centres. A treasury builds its realm's works where its people are, and the capital still leads because it is usually the largest centre.

> **RESOLVED.** RULED (Ben, 2026-09-19): option A. Treasury points spread over the polity's centres by urban scale; the lane's constants are kept. Written into DIGITISATION.md § Beat 1; the work is BL-1056 (points size-neutral).

*Files: `src/world/history_sim.cpp`, `src/world/history_sim.hpp`, `docs/generation/DIGITISATION.md`*

### NR-899 — CALL: under the ruled share, Charcoal leads Coke 373 to 142 — and span-founded ground is left out of both Fuel Doctrine pulls
*question · raised 2026-09-19 · from BL-1056 (points size-neutral) build, cold review and fix round; figures from the main session's chain 9 on 2867cdaa (digitisation_sim_harness --through 1960, 16 library seeds).*

NR-896 ruled both pulls read the share of held regions scoring over the world mean, to stop breadth deciding the fork. It did that: Charcoal polities are now the wooded ones (mean forest share 666 against Coke's 271; Charcoal wooder than Coke on 14 of 16 seeds). But the split moved further toward Charcoal, not back: coke 142 / charcoal 373 / neither 341 (best-of-held was 200 / 319 / 337; before forest was scored, 391 / 132 / 333). Coal scores sit on few regions, so few realms have a large SHARE of held ground over the mean on fuel, while forest is widespread. Also taken on your behalf: regions the span founds inherit fuel at x0.7 (Default A) but no forest reading, so to keep one region set both pulls exclude them (they are 0.3% of points).

**Why it matters.** The Fuel Doctrine is the tree's central fork: coke to scale and the Blast Works, charcoal to quality and a ceiling. BL-1042 and BL-1043 measure the 1960 world it manufactures; a world where Charcoal outnumbers Coke 2.6 to 1 builds fewer Blast Works.

- Accept: ground decides the fork now, and a coal-poor world industrialising on charcoal is a legitimate outcome; judge on BL-1043's sweep.
- Keep the share for forest, but let the fuel pull read the share of held ground within reach of a seam (a coalfield serves more than its own region), so coal's pull is not starved by its rarity.
- Compare each pull against its own spread, not the mean: a region counts when it is in the top third of the world for that resource, so fuel and forest clear at the same rate world-wide.

> **Recommendation:** The third option. It keeps the share and its size-neutrality, and makes 'like with like' literal: each pull counts regions in the same top fraction of the world for its own resource, so coal's rarity stops being a handicap. If you prefer to see the world before touching it, the first.

> **RESOLVED.** RULED (Ben, 2026-09-19): option C, and founded ground stays excluded from both pulls. Each pull counts held regions in the top third of the world for its own resource, a bar fixed at the span open. Written into INDUSTRY_TREE.md § The scorer; the work is BL-1059 (top-third bar).

*Files: `src/world/history_sim.cpp`, `docs/generation/trees/INDUSTRY_TREE.md`*

