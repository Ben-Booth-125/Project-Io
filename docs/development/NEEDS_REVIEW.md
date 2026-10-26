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

### NR-1082 — Two disclosure tensions the NR-1066 doc pass found
*question · raised 2026-10-26 · from NR-1066 doc pass*

(1) A rival's treasury is private (Ben, 2026-10-26), but DISCOVERY.md § The operational fog says an open book shows "flows, balance", and FINANCE.md § Surfaces gives the Profitability ledger one row per nation, so a rival's filed return could expose its treasury. Whether a rival's return is readable at all is unruled. (2) The Industry lens, kept, weights rival plants by output_quantity, against "production stays private" (DISCOVERY.md; TOOLTIP.md). Also: the seat screen lists every major's treasury before play.

**Why it matters.** Each decides what the player can see of a rival nation.

- (1) A: a rival's return is unreadable; B: readable without its balance line. (2) A: the Industry lens shows rival plant count or presence, not output; B: output is public through the lens.

> **Recommendation:** (1) B; (2) A. The seat screen is a pre-play pick, so it may show treasuries.

*Files: `docs/ui/DISCOVERY.md`, `docs/economy/FINANCE.md`, `docs/ui/LENSES.md`*

### NR-1083 — May rival nations use fronts?
*question · raised 2026-10-26 · from sprint 58 military design*

Ben granted fronts for the player's assigned units (AI_OPPONENT.md § 11). Whether a rival nation may assign its own units to fronts and score hold or push is a separate subject, so it was not written into the grant.

**Why it matters.** Fronts are the cheapest way for a scorer to use an army without a planner. Without this grant, rivals have no way to fight a war beyond march_unit.

- A: grant rivals fronts, hold and push scored by the nation layer; B: rivals hold only; C: no rival fronts yet

> **Recommendation:** A, on the standing terms. It is what makes BL-1427 (fronts) worth building for v0.3.0's AI half.

*Files: `docs/ai/AI_OPPONENT.md`, `docs/military/MILITARY.md`*

### NR-1084 — Four combat calls taken in writing the land model
*decision taken on your behalf · raised 2026-10-26 · from sprint 58 military design*

Writing Ben's land rulings into MILITARY.md § The campaign force model took four calls he did not make: (1) defence and breakthrough are separate stats (defending vs attacking); (2) no piercing-versus-armour check, hardness alone; (3) readiness takes the larger share of landed damage, so most fights end in retreat; (4) the seven classes - motorised infantry replaces trucks, one main battle tank line replaces light/medium/heavy, light armour is low-hardness reconnaissance, and air defence is added.

**Why it matters.** Each shapes BL-1421 (soft hard resolver) and BL-1422 (land classes and kit) before they are built.

> **Recommendation:** Keep all four; the realism case is in research/MILITARY_1960_PRESENT.md § 4 and § 10.

*Files: `docs/military/MILITARY.md`, `docs/research/MILITARY_1960_PRESENT.md`*

### NR-1085 — Five calls taken in retiring the Era catastrophe
*decision taken on your behalf · raised 2026-10-26 · from era catastrophe retirement design pass*

Writing Ben's retirement (ERAS.md § The point of an Era) took five calls he did not make: (1) each nation crosses into the next Era on its own when it meets the gate, not the world at once; (2) Alarm is a third sentiment dimension beside Access and Trust (RELATIONS.md § 6), directed per pair, decaying, under the informs-never-declares invariant; (3) three Digitisation nodes that read the retired Ceiling were rewritten in the store: Inspection Regimes 'Alarm raised by your visible capability -10%', Arms Control Verification's second effect 'every nation's Alarm toward every other decays 10% faster', Quiet Borders' deed 'no nation's Alarm toward you rises for 8 quarters'; Planetary Accords keys 'shared limits on climate Load'; (4) the tempo trap retired with the countdown; (5) climate is Era 2's continuous pressure with no test.

**Why it matters.** Each shapes what a nation chases and what Alarm costs it before any of it is built.

> **Recommendation:** Keep all five. (1) makes the race to orbit the success Ben asked for; (2) reuses a tested substrate rather than adding a new store.

*Files: `docs/economy/ERAS.md`, `docs/politics/RELATIONS.md`, `docs/economy/tech/digitisation_tree.json`, `docs/CLIMATE.md`*

### NR-1086 — Why are strategic nuclear weapons never fired, now there is no catastrophe?
*question · raised 2026-10-26 · from era catastrophe retirement design pass*

Ben ruled tactical nuclear use permitted and strategic never fired (MILITARY.md § Missiles), the same day the Era catastrophe retired. The old reason - 'firing one is the catastrophe' - is gone. The rule stands as written, with the arsenal acting through Alarm alone.

**Why it matters.** A rule with no stated cause reads as arbitrary to the player, and the AI needs to know what an arsenal is for.

- A: keep the rule; the arsenal is pure deterrence through Alarm
- B: allow strategic use with ruinous consequences (sentiment collapse with every nation, climate strain, the target razed)
- C: strategic use only in answer to a strategic strike (second-strike only)

> **Recommendation:** A for v0.3.0: cheapest and legible; revisit B once war goals and peace exist.

*Files: `docs/military/MILITARY.md`*

---

## Resolved

Kept, not pruned: the reasoning is the point. Prune only in a deliberate sweep, once the
answer has landed in an authority doc.

