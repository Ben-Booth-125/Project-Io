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

*4 entries — 3 open, 1 resolved.*

---

## Open

### NR-800 — BL-814 carried a phase-6 restructure, and it was deleted with the budget chain
*decision taken on your behalf · raised 2026-09-08 · from Re-authoring sprint 35 around generation visibility, on your ruling "drop it - a watched wait needs no budget".*

BL-813, BL-814 and BL-815 were deleted outright under the 2026-08-24 unstarted-plans policy. BL-812 (phase 6 sees roads) was kept, as it is not part of that chain.

**Why it matters.** BL-814 was filed as a startup-time item but its actual content was a PHASE 6 restructure: retire the warm start so phase 6 becomes the only judge of the position play opens on, and it named a specific known blocker - generate_corporations appends and runs before the registry loads, so phase 6 cannot vary the specialist roster at the live seam. That blocker is a fact about the code, not about the budget, and it will still be true when phase 6 is picked back up. The ruling was about the budget; deleting the restructure with it is my reading of it, not yours.

- Leave it deleted - the substance is recorded in the sprint 35 risk note and can be re-filed from there.
- Re-file the restructure as its own item, under phase 6 rather than under startup time.

> **Recommendation:** Leave it deleted for now. Phase 6 has no search built at all (BL-770 was cancelled with the board clear), so the restructure has nothing to serve yet; re-file it when phase 6 is next picked up.

*Files: `docs/development/sprints.json`, `docs/development/backlog.json`*

### NR-807 — CONCEPT.md still names the ancient arc as the live product, and the epoch moved to 1960
*question · raised 2026-09-09 · from The doc-contradiction sweep at the close of sprint 35.*

CONCEPT.md line 74 says: the live product is the ancient arc, the campaign epoch is 0 CE, the player a mercenary company. Your 2026-09-08 calendar makes the epoch 1960, and era_band_for_epoch flips to industrial at 1700 - so generation now runs the INDUSTRIAL arc.

**Why it matters.** I fixed the generation-side citation because the calendar is that doc subject. I did NOT touch this one, because it is not a date - it names WHO THE PLAYER IS and which product is live. Changing the live arc from ancient to industrial in the doc that owns player identity is a product call, not a reconciliation, and CONCEPT.md is the authority the rest of the corpus reads for it.

- The live product is now the industrial arc; CONCEPT.md is updated and the mercenary-company framing revisited with it.
- The ancient arc stays the live product and 1560-1960 is generation own arc for this work, with both supported.
- The epoch move was about generation calendar only and CONCEPT.md is correct as written.

> **Recommendation:** The second, provisionally - both arcs already exist in the code (era_band_for_epoch branches on the epoch, and HISTORY.md was already written arc-aware), so nothing forces a product choice yet. But it should be YOUR sentence, not mine, and it is the kind of thing that quietly becomes true by being left alone.

*Files: `docs/CONCEPT.md`, `docs/generation/GENERATION_STRATEGY.md`, `src/world/era_band.hpp`*

### NR-809 — Region count still scales ~quadratically AFTER BL-844, so regions cannot become provinces one-for-one - and no optimisation is waiting to change that
*decision · raised 2026-09-09 · from Sprint 37 build session, 2026-09-09. The region-count sensitivity measurement NEXT_SESSION.md asks for before any scale decision. Measured with history_span_cost, build_gen (/O2), seeds 0 and 1.*

Ben's brief says 'transform regions into provinces ... we don't have to aim for a small set'. That spans a factor of sixteen: 1,372 regions after 4,000 years against 22,153 land provinces. The measurement says the cost of that factor is not sixteen. Reading reach cost against region count across the span table, the exponent is 1.95-2.27 on seed 0 and 2.13-2.27 on seed 1 - reach is QUADRATIC in regions, as expected from a per-region Dijkstra (BL-844). Seed 0: 533 regions = 0.017 ms/rebuild, 833 = 0.045, 1174 = 0.079. Seed 1: 563 = 0.035, 931 = 0.114, 1260 = 0.218. Reach is 24-37% of the whole run. Sixteen times the regions is therefore ~256x the reach term: seed 1's 4,000-year run (6,541 ms total, 1,546 ms reach) would spend roughly 400 SECONDS in reach alone. CHECKED AGAINST BL-844 (reach Dijkstra heap), WHICH IS ALREADY COMPLETE - delivered 2026-09-09, the same day. These figures are the POST-FIX state, and they reproduce BL-844's own closing measurement exactly (it records 0.017 ms/rebuild at 529 regions rising to 0.079 at 1,160 on seed 0; this session measured 0.017 at 533 and 0.079 at 1,174). BL-844's delivery note is explicit that the remaining growth is NOT the algorithm: the heapless scan's quadratic is gone, and what is left is the NEIGHBOUR GRAPH DENSIFYING as regions fill in, so edges are not O(N) in this world and O(E log V) still grows. In its own words, that is a property of the graph rather than of the algorithm, and no further heap work fixes it.

**Why it matters.** It bounds objective 3 before a line of it is written. At today's per-region Dijkstra the region count can roughly DOUBLE inside a 4x wall-clock budget - about 2,500-3,000 regions on seed 0, which STARTUP.md's watched-wait ruling can absorb ('a watched wait needs no budget', Ben 2026-09-08). It cannot go to sixteen. That is not an argument against the design - BL-849 already ruled that colonisation SEEDS the partition and the late pass still DRAWS it, so regions never had to map one-for-one - but it does decide how far 'don't aim for a small set' can be taken in THIS sprint. AND THE CHEAP WAY OUT IS ALREADY SPENT. The first draft of this entry offered 'do BL-844 first' as the route to the sixteen-fold; BL-844 is done, and it explicitly declines to pursue the residual growth because reach is no longer what a long run is made of. So the choice is not between optimising and capping - it is between capping the region count and changing the ADJACENCY MODEL, which is a much larger piece of work than this sprint.

- Cap the colonisation span at roughly 2-3x today's region count (~2,500-3,000) and let the province partition keep doing the rest. BL-849 already rules that colonisation SEEDS the partition and the late pass still DRAWS it, so regions never had to map to provinces one-for-one. Ships this sprint; nothing new is needed.
- File a new item against the ADJACENCY MODEL itself - the neighbour graph densifies as regions fill in, which is what BL-844 left standing. Capping neighbour degree, or moving reach off a per-region graph entirely, is what a sixteen-fold region count would need. That is a sprint of its own and it goes in front of the span Ben asked for.
- Raise the region count anyway and accept a multi-minute watched wait in round 4, on the grounds that STARTUP.md rules the wait IS the content.

> **Recommendation:** Option 1 for this sprint, with option 2 filed rather than done. Two reasons, and the second is the stronger. First, Ben's stated acceptance criterion for round 4 is the ARC - origin, communication, conquest or union, a stable dark age - and that arc reads at 2,500 regions exactly as it reads at 22,000; spending the sprint on a scaling term buys resolution on a surface whose own test is shape. Second, the colonisation walk itself is NOT what scales badly here: it is a single multi-source flood over ~9,500 land tiles, linear in tiles and independent of region count. What scales is the Era -1 sim's per-region reach, which is a different pass with a different owner. So capping the region count costs the colonisation design nothing at all - it only defers how finely the sim resolves the politics on top of it.

*Files: `tools/verify/history_span_cost.cpp`, `src/world/history_sim.cpp`, `docs/generation/COLONISATION.md`, `docs/generation/PROVINCES.md`*

---

## Resolved

Kept, not pruned: the reasoning is the point. Prune only in a deliberate sweep, once the
answer has landed in an authority doc.

### NR-808 — The creeds' tribal marches are wars, sitting inside a span whose premise is that war is not yet the process
*question · raised 2026-09-09 · from The colonisation design session, 2026-09-09. Recorded as an open question in COLONISATION.md and raised here because it is a call, not work.*

CREEDS.md's tribal marches resolve single-round pairwise wars at cradle grain and WELD two cradles into one, lowering fragmentation_q before nation_params_from_ladder reads it. Under the new colonisation span (COLONISATION.md) they run inside a span whose stated premise is that spreading, not fighting, is the interesting process.

**Why it matters.** They are the only wars in the peaceful span, so leaving them where they are makes the span's premise partly false on its own terms. But moving them is not free: welding drives fragmentation_q, fragmentation_q drives the SEED BUDGET, and the seed budget drives the nation count. Moving the marches past the boundary changes how many nations a world has. That is a measured change, not a design one, which is why nothing was moved by this session.

- Leave them where they are - a peaceful span is a comparative claim, not an absolute one, and cradle-grain welding is coarse enough not to contradict it.
- Move them to the boundary, so the colonisation span is warless by construction and the marches become the first act of the empire phase.
- Fold them into the span as a NON-violent mechanism - two cradles that meet merge by contact rather than by conquest, keeping the fragmentation effect and dropping the war.

> **Recommendation:** Do not decide it dry. Whoever builds BL-846 (colonisation span) should measure the nation-count distribution across a seed spread with the marches in each position, and bring you the three numbers. The third option is the most interesting design and the least measured.

> **RESOLVED.** BEN, 2026-09-09: option 3, re-derive fragmentation from contact. The tribal marches RETIRE; fragmentation_q is read off how far two peoples' streams interpenetrate, which the colonisation span already produces in culture_shares.

THE FINDING THAT REFRAMED THE QUESTION: the marches' TIMING was the lesser problem and their GRAIN the greater one. They compare cradle to cradle - a scalar against a scalar between two discrete points - but under diffusion a cradle is a SOURCE, not a point, and what meets at a frontier is two streams already mixing. Moving them past the boundary would have fixed when they ran and left that untouched.

WHAT IT SEPARATES: aggression_q drove both how consolidated the political map is and how a polity fights once it exists. It now drives only the second; package breadth drives the first. One cause each.

Written into COLONISATION.md (new section: Fragmentation comes from contact), CREEDS.md (the creed drives, rewritten) and HISTORY.md (the ladder pass + the pipeline diagram). Work is BL-852. TWO THINGS CARRIED FORWARD, both open in COLONISATION.md: the non-hegemony floor must be re-derived for the new mechanism (the old welding carried an explicit half-fragmentation floor for BL-224's sake), and the nation-count distribution must be MEASURED across a seed spread before the change lands.

*Files: `docs/lore/CREEDS.md`, `docs/generation/COLONISATION.md`, `src/world/creeds.cpp`*

