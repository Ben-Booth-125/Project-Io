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

### NR-164 — Re-stress generation and time-lapse feel when playable
*question · raised 2026-08-11 · from 2026-08-11 notes session with Ben.*

Ben wants to re-stress-test world generation and the staged-generation time-lapse once the game is playable, to judge whether it feels alive rather than just correct. RULING 2026-08-13 (Ben, elicitation form): keep open until playable - not scheduled, not folded into another item.

*Files: `docs/ui/STARTUP.md`*

### NR-221 — BL-404 was measured rather than implemented, and the measurement blocked it — a design call is owed
*question · raised 2026-08-13 · from BL-404 (inter-body pull is un-netted), 2026-08-13*

You asked for BL-404 next. It is NOT implemented, deliberately. Building the measurement first (tools/verify/interbody_pull_harness.cpp, now in the ctest tier) showed the fix I had recommended in the item cannot be written: `inject_interbody_demand` reads ONE of the home body's 17 markets as though it were the whole body. That market carries 12% of the body's demand, and a share of its supply that differs by toolchain — 1491 of 7498 on the MSVC build, 0 of 2863 on g++, same seed. So 'net against the previous tick's supply' silences 33% of the pull on one build and 0% on the other. Filed the underlying defect as BL-406 (the home market is an arbitrary pick) and made BL-404 require it.

**Why it matters.** Three things need your call, and none of them are mine to take because they all move prices. (1) BL-406's fix: aggregate over the body (truest, multiplies the pull ~8x, needs pull_fraction re-tuned, outpost prices move), name a primary market per body (cheap, stable, keeps today's magnitude, but keeps 'the body's demand' meaning one market's), or pull per-counterpart-market (most faithful, largest, needs machinery that does not exist). (2) BL-404's own a/b/c, which should be decided in the same pass since aggregating changes what netting does. (3) Whether outpost prices are allowed to move now at all, or whether this waits behind BL-381 giving demand real weight. I have left both items open and resumable rather than picking.

- BL-406 (a) aggregate over the home body + re-tune pull_fraction, and settle BL-404's netting in the same pass (recommended)
- BL-406 (b) store a primary_market per body — cheap and stable, keeps current magnitudes, defers the meaning question
- BL-406 (c) per-counterpart-market pull — most faithful to a trade model, largest change
- Hold both until BL-381 (what supply and demand mean) lands, since the netting question is downstream of it

> **Recommendation:** Option (a), with BL-404 decided alongside it. But if outpost prices moving is unwelcome right now, (b) is a clean stopgap that makes the pick authored instead of accidental and costs almost nothing.

*Files: `src/world/market_clearing.cpp`, `tools/verify/interbody_pull_harness.cpp`*

---

## Resolved

Kept, not pruned: the reasoning is the point. Prune only in a deliberate sweep, once the
answer has landed in an authority doc.

