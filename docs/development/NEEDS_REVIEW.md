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

### NR-800 — BL-814 carried a phase-6 restructure, and it was deleted with the budget chain
*decision taken on your behalf · raised 2026-09-08 · from Re-authoring sprint 35 around generation visibility, on your ruling "drop it - a watched wait needs no budget".*

BL-813, BL-814 and BL-815 were deleted outright under the 2026-08-24 unstarted-plans policy. BL-812 (phase 6 sees roads) was kept, as it is not part of that chain.

**Why it matters.** BL-814 was filed as a startup-time item but its actual content was a PHASE 6 restructure: retire the warm start so phase 6 becomes the only judge of the position play opens on, and it named a specific known blocker - generate_corporations appends and runs before the registry loads, so phase 6 cannot vary the specialist roster at the live seam. That blocker is a fact about the code, not about the budget, and it will still be true when phase 6 is picked back up. The ruling was about the budget; deleting the restructure with it is my reading of it, not yours.

- Leave it deleted - the substance is recorded in the sprint 35 risk note and can be re-filed from there.
- Re-file the restructure as its own item, under phase 6 rather than under startup time.

> **Recommendation:** Leave it deleted for now. Phase 6 has no search built at all (BL-770 was cancelled with the board clear), so the restructure has nothing to serve yet; re-file it when phase 6 is next picked up.

*Files: `docs/development/sprints.json`, `docs/development/backlog.json`*

### NR-801 — Rounds 4 and 5 break the wizard promise that nothing is generated in it
*decision taken on your behalf · raised 2026-09-08 · from Writing the sprint 35 visibility design into STARTUP.md and GENERATION_STRATEGY.md.*

STARTUP.md has said since BL-167 that NOTHING is generated in the wizard - every control move re-runs the chain as a pure throwaway preview and m_world is untouched. Rounds 4 and 5 cannot honour that: the history sim is the most expensive pass in the project. So I wrote them as running the REAL pass inside the round, on a Run press, drawing as it computes - the doc now says the preview model does not transfer.

**Why it matters.** That promise is not decoration. It is what makes Back a plain revision with no snapshot, and what lets the wizard reroll freely. Rounds that generate for real make Back across round 4 expensive and make a reroll a minute rather than a frame. Your "a watched wait needs no budget" licenses the wait; it does not by itself choose this mechanism over the alternative, which is to collect leans against an illustrative preview and run everything at Begin as today.

- The pass runs inside the round, on a Run press - the wait is the content (what I wrote).
- Leans are collected against an illustrative preview; the real passes still run at Begin behind the loading screen.

> **Recommendation:** Keep what I wrote. The second option gives the player a lean whose effect they cannot see when they set it, which is the one thing the planetology rounds get right and the reason the wizard works at all.

*Files: `docs/ui/STARTUP.md`, `docs/generation/GENERATION_STRATEGY.md`*

---

## Resolved

Kept, not pruned: the reasoning is the point. Prune only in a deliberate sweep, once the
answer has landed in an authority doc.

