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

*5 entries — 5 open, 0 resolved.*

---

## Open

### NR-164 — Re-stress generation and time-lapse feel when playable
*question · raised 2026-08-11 · from 2026-08-11 notes session with Ben.*

Ben wants to re-stress-test world generation and the staged-generation time-lapse once the game is playable, to judge whether it feels alive rather than just correct. RULING 2026-08-13 (Ben, elicitation form): keep open until playable - not scheduled, not folded into another item.

*Files: `docs/ui/STARTUP.md`*

### NR-532 — The design register is live — 41 open calls across ten sections, as a form
*observation · raised 2026-08-22 · from Ben, 2026-08-22: "now please revisit each one and open forms for answering the open questions."*

Published at https://claude.ai/code/artifact/debe7b8f-7315-429a-a805-0e295e9405bc. Every open question across the eight new authority docs plus SYSTEMS.md § The progression chain plus four cross-cutting calls, gathered into one form: 41 questions in 10 sections. Each carries its evidence, 3-5 options with one marked as suggested, and a free-text field that overrides the options. Progress is tracked per section; "Copy all answers" puts everything on the clipboard in one block. It is a LIVE DOC - radios and contenteditable fields are captured as edits, so answers reach a watching session directly; deliberately no <textarea> and no <select>, neither of which is captured. Answers also persist to localStorage as a per-viewer draft.

The generator is committed rather than being a one-off (CLAUDE.md § Tool creation is skill creation): tools/session/register/questions.js is the canonical question set and build.js emits the HTML. Verified to regenerate byte-identically. Republish to the same URL to keep answers in place.

**Why it matters.** The open questions were the point of writing the docs as capture rather than design, and they were spread across ten files. A form is the difference between 41 questions that get answered and 41 that get skimmed. It also means the answers arrive in one structured block that can be propagated in a single pass, the way the six nations rulings were.

> **Recommendation:** Answer in any order. The four that change the most downstream: LOGISTICS Q1 (what generates LP), EVENTS Q4 (drive the collapse metagame or express it - it decides the system's size), PEOPLE Q2 (one bias or several - cheapest to overturn now), and NATIONS Q1 (whether the grant reaches a rival, which blocks the player-facing halves of two items).

*Files: `tools/session/register/questions.js`, `tools/session/register/build.js`*

### NR-577 — BL-572 concurrent-offer escrow split — a tick's contracted_force share fills open offers oldest-first, not a rule Ben stated
*decision taken on your behalf · raised 2026-08-23 · from BL-571/BL-572 design ratification, elicitation form (Ben, 2026-08-23).*

Ben's elicitation answer chose 'several open concurrently' over the proposed one-offer-at-a-time cadence for BL-572, but the form did not ask how one tick's `contracted_force` spendable share is allocated when more than one offer is open and filling at once. Taken on his behalf: oldest-issued offer fills first, exhausting the tick's share before a younger offer sees any of it, rather than splitting the share proportionally across open offers. Ratified into CONTRACTS.md § Where offers come from and BL-572's design field. (Renumbered from NR-576 on integrating a concurrent session's push, which had already used that id for an unrelated review-queue-purge entry now archived.)

**Why it matters.** The split rule changes how fast a province under sustained multi-front threat gets its offers funded — oldest-first means a nation's oldest want is always serviced before a newer, possibly more urgent one; a proportional split would fund all fronts slower but evenly. Either is deterministic and legal; this is a pacing/feel call, not a correctness one.

- Confirm oldest-issued-first.
- Switch to a proportional split across all open offers each tick.
- Switch to highest-value-first (by the offer's scored fee).

> **Recommendation:** Confirm oldest-issued-first — it is the simplest deterministic rule, matches a FIFO reading of 'the nation gets to what it can afford', and needs no additional per-offer priority field.

### NR-578 — BL-575 live-click pass confirmed March is reachable and dispatches, but did not confirm the unit visibly moving
*observation · raised 2026-08-23 · from Main-session live verification (computer-use) of BL-575 before closing Wave 1.*

Opened the built app, selected the starting unit, pressed March, and clicked a confirmed-different province (verified via the repeat-click cycle showing its own province id). The mode disarmed without falling through to normal building/tile selection — the Selection panel stayed on Unit — which is strong evidence the corp_command dispatched rather than the click landing on ordinary marker resolution, and corp_command.cpp's march_unit case reads correct on inspection. What was NOT achieved: relocating the marker after the order to watch its tile position actually change. Two compounding factors: the economy/march tick in this build is QUARTERLY-cadence (the header counts down 'd to Q2/Q3'), not daily, despite the calendar advancing daily, so only one quarter boundary elapsed during the pass; and re-finding one unit marker on a freshly generated, unfamiliar world via manual pan/zoom proved impractical within reasonable effort. Halt and Disband were not individually live-clicked (same code pattern as March, confirmed by reading, not by pressing). (Renumbered from NR-577 on integrating a concurrent session's push.)

**Why it matters.** This satisfies the standing rule's core concern — BL-449 shipped clean on both compile and harness while a press rendered unreachable, and this pass proves March IS reachable and does dispatch. But it stops short of the full 'watch it work' bar the rule aspires to, and per the rule's own reasoning (a scripted capture is not enough), neither is a click that merely disarms without a rejection message.

> **Recommendation:** Optional: a follow-up live session with more sim-time (past a quarter boundary, unit pre-located before issuing the march) to directly watch a unit's marker move. Not a blocker for Wave 1 close-out given the code-path and headless evidence.

### NR-579 — scripts/contracts.lua's fee_mult and deadline_ticks (take 1.0/180, hold 1.25/90) are legible placeholders, not measured
*decision taken on your behalf · raised 2026-08-23 · from BL-570 (CONDITION_PROVINCE_SUBJECT), authoring scripts/contracts.lua's first two template rows.*

BL-570's task is the vocabulary + template table shape, not a priced contract -- nothing reads fee_mult or deadline_ticks yet (BL-572 derives the offer/fee, BL-573 builds the live contract), so there was nothing to measure against. Authored round, legible defaults instead, matching economy.lua's own stated discipline for a first cut: take = 1.0x fee, 180-tick deadline; hold = 1.25x fee (a premium for standing watch), 90-tick deadline (a defensive window is naturally shorter than an offensive one). Both numbers are a guess at the right SHAPE (hold costs more, hold is shorter), not a derived magnitude.

**Why it matters.** BL-572 (contract offers) is the first item that actually prices a fee off these multipliers against a nation's contracted_force budget line, and BL-573 (contract record) is the first to run out a real deadline. If either lands without revisiting these two rows, the mercenary vertical slice's first offers will be priced/paced on an unexamined guess rather than a playtested number.

> **Recommendation:** Re-derive or explicitly re-affirm fee_mult and deadline_ticks once BL-572 can measure them against a real nation treasury and BL-573 against a real contract's pacing -- same discipline CONTRACTS.md and economy.lua already apply elsewhere (iterate by playtest, not by authoring once and forgetting).

*Files: `scripts/contracts.lua`*

---

## Resolved

Kept, not pruned: the reasoning is the point. Prune only in a deliberate sweep, once the
answer has landed in an authority doc.

