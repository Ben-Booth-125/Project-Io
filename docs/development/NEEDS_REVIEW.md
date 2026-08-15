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

*7 entries — 7 open, 0 resolved.*

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

### NR-233 — Requirements history physically time-sliced (BL-421) — three data calls taken
*decision taken on your behalf · raised 2026-08-14 · from Ben asked for doc-system improvements (2026-08-14 session); the split itself follows the established backlog pattern, but three data edits rode along.*

(1) 219 resolved requirement groups' rows+resolution moved to archive/requirements-<quarter>.json — reversible via archive_requirements.js --restore. (2) Legacy statuses normalised: 9 group 'completed' + 1 'closed' (header-chrome-tightening, the NR-075 retroactive closure) -> 'complete'; row 'completed' -> 'complete'. (3) 16 legacy groups with brief:null got deterministic title-slug briefs so the cold store can key on them.

**Why it matters.** All three are edits to permanent history records. The normalisation is blessed by REQUIREMENTS.md ('normalise on sight'), but 'closed'->'complete' flattens the nuance that that group closed retroactively with pending rows (the resolution text preserves it), and the synthesized briefs are new identifiers Ben never chose.

> **Recommendation:** Accept; overturn via --restore plus git if any of the three reads wrong.

*Files: `docs/development/req/requirements.json`, `docs/development/archive/requirements-2026-Q2.json`, `docs/development/archive/requirements-2026-Q3.json`, `docs/development/archive/requirements-undated.json`*

### NR-234 — Untrusted-boundary invariant promoted into the standing rules — delegated call
*decision taken on your behalf · raised 2026-08-14 · from BL-387 (seam actor authority) design: "Two instances is enough to call it a rule rather than an incident, and it should go in the standing rules." Acted on while landing the seam batch.*

Added a bullet to .claude/rules/io-standing-rules.md: an AI-facing seam is an untrusted input boundary — validate the value that lands in the field (range-check before narrowing casts), reject the whole command on violation, rejections mutate nothing, actor authority lives at the protocol layer. AI_OPPONENT.md § 6 records the session-actor model.

**Why it matters.** The standing rules are the always-on invariants — adding one binds every future session. The pattern occurred four times in one session (NaN guard, actor authority, parser truncation, float narrowing), so the design itself asked for the promotion, but the wording and its placement are mine.

> **Recommendation:** Keep; reword or demote to AI_OPPONENT.md-only if you want the standing rules leaner.

*Files: `.claude/rules/io-standing-rules.md`, `docs/ai/AI_OPPONENT.md`*

### NR-235 — Three stale worktree-agent branches carry one unmerged commit each
*observation · raised 2026-08-14 · from First run of the new tools/session/agent_base_check.js (seam batch, 2026-08-14).*

worktree-agent-a06bd53e3dbdb3302 and worktree-agent-a6aea9bd1514a3604 (10 commits behind main) and worktree-agent-a06e732cbc6ad6282 (90 behind) each hold exactly one commit main does not contain.

**Why it matters.** Either abandoned duplicates of work that landed another way (delete the branches) or genuinely lost agent output (recover it). The stale-base retro found agents whose commits were painful to integrate; these may be the ones that never were.

> **Recommendation:** A ten-minute triage: `git log -1 --stat` each branch; delete if duplicated, cherry-pick if lost.

### NR-236 — history_ages parked out of the visual suite — gate-coverage call taken
*decision taken on your behalf · raised 2026-08-14 · from Gate-hygiene block: the check hung two full visual passes (>8 min solo, both map sizes).*

Moved scripts/verify/history_ages.lua to scripts/verify/parked/ (excluded from --verify-all and the per-script sweep by location, visibly rather than by a flag). Filed BL-425 (ages lazy-sim cost) as the debt item. The Ages view keeps its ledger/comms coverage via history_ledger_and_comms.lua; what it loses is the three time-lapse captures.

**Why it matters.** A parked check is a hole in the gate. Parking is my call, not yours — overturn by un-parking if you would rather the suite carry an 8-minute script until BL-425 lands.

> **Recommendation:** Accept the park; take BL-425 (ages run cost) in a session with BL-403 (profiling harness), which wants the same instrument.

*Files: `scripts/verify/parked/history_ages.lua`*

### NR-237 — Golden-diffing policy: keep, demote to a curated world-independent set, or drop
*question · raised 2026-08-15 · from Ben, twice on 2026-08-15: "What's your defence for keeping goldens?... I have never personally needed to use a golden... we are often changing significant amounts of the UI."*

The measured case FOR his instinct: every diff in this week's passes was either intended change or world drift (35-39% everywhere after the Selection band change — zero information beyond "something changed", answer always "bless"); the genuine catches were harness-infrastructure bugs (capture-size poisoning, batch state leaks) — goldens policing the harness, not the game; there is no CI to run them, so they fire only in manual stewardship passes; and every world change obligates a ~30min pass plus a 200-file binary commit. The case AGAINST dropping entirely: captures (the PNGs) are how Claude verifies UI work and must stay regardless; a small WORLD-INDEPENDENT golden set (icon_silhouettes, header layout, text-fit surfaces) has high signal and near-zero churn; and assertion-based checks (the text_overflow_floor model — "the integer is the verdict") already cover the clipping class better than pixels do.

**Why it matters.** Decides whether ~239 committed goldens and the re-bless obligation stay, shrink to ~10-15 stable surfaces, or go. Also decides BL-402's remaining scope.

- A) Keep the full set + routine blessing (status quo, now cheap-ish via --verify-all)
- B) Demote: goldens only for world-independent surfaces; everywhere else captures + targeted assertions (verify API grows draw-count/non-empty assertions); delete the rest
- C) Drop golden-diffing entirely; captures + assertions only

> **Recommendation:** B. It matches the project's own stated preference (text_overflow_floor: PNGs incidental, never the pass criterion) and the measured signal. Re-introduce goldens per surface as each UI area freezes approaching v0.1.0 — freezing is what makes a golden pay.

*Files: `scripts/verify/golden/`*

---

## Resolved

Kept, not pruned: the reasoning is the point. Prune only in a deliberate sweep, once the
answer has landed in an authority doc.

