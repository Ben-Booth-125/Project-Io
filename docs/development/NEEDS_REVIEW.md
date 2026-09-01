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

*1 entries — 1 open, 0 resolved.*

---

## Open

### NR-774 — Four definitional calls taken so the success sweeps are measurable - ratify or overturn
*decision taken on your behalf · raised 2026-09-01 · from Success-lever session, 2026-09-01: the sweep battery (BL-723..BL-729) cannot log "GDP", "influence", a band, or a proxy without someone picking each. Picked to keep the session moving; the instrument encodes them.*

1. GDP := VALUED PRODUCTION - sum of building output x its market's resolved price, per tick - with net income (filed returns) and net worth logged beside it. The money loop is deliberately not closed (the market is a buyer of last resort paying with nobody's money), so no income-side aggregate is conservation-meaningful; one definition is picked and held for every comparison.

2. INFLUENCE := catchment market share (standing.cpp's axis) + footprint tiles + trade-route spread, with stance/lobby verb counts logged beside them. Sentiment cannot carry it yet - eight of ten factor weights are authored at zero, so sentiment today moves only through procurement conduct.

3. BAND := the industrial 1960 start (your 2026-08-31 ruling: "we will be working on the 1960s start"); ancient runs remain available per-run via --epoch. The two bands fail differently, so one sweep cannot serve both.

4. PROXY := the corp AI accepted as the skilled-player stand-in, with two brackets stated in every report: cadence pessimism (one strategic batch per corp per 4 quarters vs a human acting every tick) and subsidy optimism (under spectate every corp can claim nation budget lines; a played player-corp is never a claimant).

---

## Resolved

Kept, not pruned: the reasoning is the point. Prune only in a deliberate sweep, once the
answer has landed in an authority doc.

