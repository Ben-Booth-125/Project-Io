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

### NR-891 — DECISIONS TAKEN: how the Industry tree's scorer terms and research rate read the sim (BL-1038)
*decision taken on your behalf · raised 2026-09-18 · from BL-1038 (Industry tree wired), its build lane and its cold review; merged as 29b74f69, off by default.*

INDUSTRY_TREE.md § The scorer gives each term a reading, and two name quantities the Era -1 sim does not carry. The build took the nearest ones it does carry; the doc now says so (PROPOSED). READINGS: labour_bound = of the held surplus above subsistence, the share standing under arms (the doc's 'what the held works need' has no source). credit_bound = how far the capital's treasury falls short of the post-road price, the one venture the sim prices in capital (nodes are not paid in stores). colonial_reach = the seat's line to a held region crosses sea (the same test that makes a campaign a sea leg). fuel_bound = the seat market's unmet energy want after inbound trade. threatened = the heaviest grudge any living polity holds against this one. many_peoples = the share of held regions whose plurality people is not the realm's. known = per node, held by a living polity this one has met. PINNED AT 0: ground_forest (no forest field on a region), tariff_pressure (no landed price in the sim), plague_struck (the history sim runs no plague). Charcoal Iron is still reached, because a seamless polity finds Coke gated out. RATE: the three largest held regions' urban population, summed, capped at 600k, to the power 1.5 via an integer square root, around a reference of 200k at 6 per mille; TREES.md § State records the form and k = 3. The reference and fraction were re-priced once after a first cut that did not bind (anything above ~200k bought a node every Invest round).

**Why it matters.** These readings decide which side of each Industry fork a polity takes, and so the 1960 spread and the Works lean. Recorded so a reading can be overturned rather than quietly becoming the design.

- Accept the readings and the rate as built.
- Re-read named terms (say which, and what they should read).
- Give ground_forest a source now (a forest share on the region), so the forest polity chooses Charcoal rather than being pushed to it.

> **Recommendation:** Accept for Beat 1. Revisit ground_forest when a region carries forest, and tariff_pressure when the sim carries a landed price.

*Files: `src/world/history_sim.cpp`, `src/world/history_sim.hpp`, `docs/generation/trees/INDUSTRY_TREE.md`, `docs/generation/trees/TREES.md`*

### NR-892 — CALL: furnace_lit reads 1000 for every polity that has invested at all — what should it read?
*question · raised 2026-09-18 · from BL-1038 cold review (code-reviewer, 2026-09-18).*

INDUSTRY_TREE.md § The scorer defines furnace_lit as 'the polity holds the spire's ring-1 major', and the build reads exactly that. But the root is the only node available from an empty mask, so every polity holds it after its first Industry purchase, and furnace_lit is 1000 on every later re-pick. It is the pursued-when term of Electrification, Synthetic Chemistry and Plant Registry, so those three always score the term's maximum, and the term separates no polity from another. The doc's cause column ('a lit engine house; the furnace crossing already recorded') points at something narrower than holding the root.

**Why it matters.** A term that reads the same for every investor does no work in the scorer, and three ring-3 nodes lean on it.

- Keep it as written: an entry marker, knowingly inert.
- Read the Fuel Doctrine taken: Coke Smelting or Charcoal Iron held, so the furnace is lit when the polity has answered the fuel question.
- Read a held region's own furnace crossing (Stage 4). No region lights before 1700 today, so this reads 0 until the Stage 4 gate runs in the span.

> **Recommendation:** Read the Fuel Doctrine taken. It is already recorded in the mask, it separates polities, and it matches the doc's cause.

*Files: `src/world/history_sim.cpp`, `docs/generation/trees/INDUSTRY_TREE.md`*

### NR-893 — CALL: the Industry spread is set by how often the Invest verb wins, not by the research rate
*question · raised 2026-09-18 · from BL-1038 (Industry tree wired) build lane and fix round; exploration_sweep --through 1960 --industry-open 1660 over the 16 library seeds (lane figures, merged as 191c212b; the main session re-runs them before quoting them as evidence).*

With the Industry switch on from 1660, at 1960: 862 living polities, 553 hold an Industry node; nodes held pooled 0/0/6/27/42 of 45 (min/p25/median/p75/max); the leader holds a median 38 and finishes the tree in several seeds; the median polity holds a median 7 (range 0-19). 290 living polities (34%) never had an Industry round at all. WHY: research accrues only on rounds the Invest verb wins (history_sim.cpp case sim_verb::invest), Invest's score is divided by the band already held, and each win buys at most one node — progress resets to 0 on purchase and the surplus is discarded. So the spread between leaders and the rest is set by verb frequency, and re-pricing the rate moves the leaders, not the median. The same rule governs the Empire and Exploration trees.

**Why it matters.** Beat 1 reads Industry capacity nodes as a multiplier on industry points, and INDUSTRY_TREE.md says the 1960 spread is manufactured by the fuel gate and the pace. If a third of polities never invest, their cities build points on scale, fuel and treasury alone, and the Works fork is untaken by most (502 of 862 take neither side).

- Accept for Beat 1: a polity that never invests still builds points from scale, fuel and treasury; read the industrialisation reading before changing anything.
- Carry surplus progress past a purchase, in every tree: removes the one-node-per-round cap; moves every shipped world (Empire and Exploration share the rule).
- Let Industry research accrue every round as a flow (TREES.md § State: research is a flow), with Invest choosing only the node: Industry only, so no shipped world moves; widens who reaches ring 1.

> **Recommendation:** Accept for Beat 1 and read BL-1041's industrialisation reading first. If points turn out to track headcount because capacity is flat for most polities, take the Industry-only flow.

*Files: `src/world/history_sim.cpp`, `docs/generation/trees/TREES.md`, `docs/generation/trees/INDUSTRY_TREE.md`*

---

## Resolved

Kept, not pruned: the reasoning is the point. Prune only in a deliberate sweep, once the
answer has landed in an authority doc.

