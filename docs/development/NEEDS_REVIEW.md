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

*3 entries — 1 open, 2 resolved.*

---

## Open

### NR-777 — What is "marginal cost" for a processor - inputs included (A) or conversion only (B)? The reading decides the size of the retune
*question · raised 2026-09-02 · from Applying NR-776 (approved) to the recipe_margin table, 2026-09-02. With k = 1.0 and inputs inside marginal cost, routes (a)+(b) cannot get a single processing recipe green: profit >= marginal cost means revenue >= 2 x inputs at EVERY stage, so route (c) - prices - is forced on the whole ladder, not the residue.*

Two readings of Ben's sentence, quantified with the cheapest route setting each good's price, raws pinned at their extraction price, k = 1.0:

A - MARGINAL COST INCLUDES INPUTS (the economics reading, and the one the harness runs today): every processing stage must double the value it takes in. At processing base_rate 16 (wage/batch 0.75): 24 of 50 goods re-priced - steel 8->13.5, machinery 22->55.5, alloys 34->79.5, electronics 29->41.5, spacecraft_components 140->277.5, ordnance 43->110.5, consumer_goods 12->55.5, tools 25.5->88.8 - ladder top 56x -> 111x; 17 alternate routes need their input cost cut to the cheapest route's bar (steel_from_blooms 54.5 -> 6.0, steel_bessemer 13.1 -> 6.0, hydroponics_bay 9.0 -> 0.8, the four construction methods ~25-40%). Every price-dependent tuning moves with it (demand baskets, reservation_mult, procurement lumps, build costs, spawn capital, ceil_mult), the sweeps' baselines go stale, goldens re-bless. In return M2 (fixed cost at the floor) becomes attainable at today's maintenance: steel's value-add 7.5/batch x 8 batches x 0.25 = 15/tick against 16.8 of wages + maintenance - nearly there, and the tiers above clear.

B - MARGINAL COST IS THE CONVERSION COST (wage per batch; inputs pass through at cost): value-add >= 2 x wage. At base_rate 16: 9 goods re-priced by small amounts (food_rations 6->7.5, consumer_goods 12->17, silicon 5->5.5, clean_water 3->4.5), ladder unchanged at 56x, 9 alternates need input cuts (steel_from_blooms 25 -> 6.5, hydroponics_bay 6.3 -> 1.5, charcoal 4.5 -> 2.5; steel's coal route 7.0 -> 6.5 or steel 8 -> 8.5). Cheap - but it leaves gross margins at 10-15% and M2 fails almost everywhere: steel's value-add 2/batch x 8 x 0.25 = 4/tick against 16.8, so the floor half needs maintenance cut ~4x and wages with it - a second, larger retune of the very constant the 2026-09-01 ledger named as 80% of drains.

Either way, processing base_rate 8 -> 16 (wage/batch 1.5 -> 0.75) is route (a) and halves the alternate-route cuts; and EXTRACTION's floor half is independent of the reading: at W = 0.5 a site needs price >= 3.8 to cover 9.5/tick of wages + maintenance at the floor, so raws either rise (iron_ore 2.5 -> ~4, which propagates up under both readings) or extraction's fixed costs fall (maintenance 5 -> ~2, wage 8 -> ~4).

Recommendation: A, with base_rate 16 - it is the sentence as written, it makes profit robust across the whole band rather than only at base, and it is the retune that also clears the floor half without gutting maintenance. It is a sprint, not a pass: one re-priced table, the 17 alternates re-costed, ceil_mult re-derived, sweeps re-baselined. B is the light touch if the 56x ladder must not move.

---

## Resolved

Kept, not pruned: the reasoning is the point. Prune only in a deliberate sweep, once the
answer has landed in an authority doc.

### NR-775 — Two constants taken for the recipe margin anchor - k = 1.0 and typical_workforce = 0.5
*decision taken on your behalf · raised 2026-09-02 · from Sprint 31 opening, 2026-09-02: the instrument needed both numbers to run, and neither is in your sentence exactly.*

1. profit_over_marginal k = 1.0 - your sentence read verbatim: "a greater profit than marginal costs" = margin >= 1.0 x marginal cost, i.e. revenue at least twice marginal cost. The harness prints the roster's count at 0 / 0.5 / 1 / 2 beside it so the bar can move on a measurement (today: ancient 9/2/1/0 of 19, industrial 16/2/2/0 of 25).

2. typical_workforce W = 0.5 for the floor half (M2) - the value corporation_generation.cpp seeds every generated building with (default_workforce_assigned), i.e. the staffing the world opens at. W=1.0 would halve the per-batch share of fixed cost and turn several M2 rows green without any table changing; 0.5 is the honest opening case. Both live in economy.lua's recipe_margin_anchor as data.

> **RESOLVED.** RESOLVED. Ben, 2026-09-02: approved as taken - k = 1.0 (profit at least equal to marginal cost) and typical_workforce = 0.5 stand as the authored anchor constants.

### NR-776 — Retune direction for the roster - rates and costs first, or prices?
*question · raised 2026-09-02 · from recipe_margin first run, 2026-09-02: 41 of 44 priced recipes fail M1, and the failure has a shape - zero or negative value-add before wages, then a 1.50 wage per batch.*

Three routes to green, not exclusive, and the ORDER is the call:

(a) COSTS AND RATES FIRST (recommended). Processing base_rate 8 batches/tick at W=1 against wage 12 makes wage/batch 1.50 - larger than the entire authored margin of steel (1.0), refined_copper (1.5), silicon (1.0), oil_power (1.58). Raising processing base_rate (or lowering base_wage) shrinks that per-batch term for every recipe at once, and maintenance 10/tick likewise shrinks per batch. Extraction's M2 failure is the same shape: 5 maintenance + 4 wages per tick at W=0.5 against 2.5 x price at the floor. One or two constants move 60+ rows. Risk: throughput changes ripple into the demand/upkeep rates the sweeps just measured.

(b) INPUT QUANTITIES where value-add is zero or negative: steel_from_blooms (22 in, 8 out), consumer_goods (14 in, 12 out), hydroponics_bay (6.25 in, 3 out), food_rations (6 in, 6 out), clean_water (3 in, 3 out), steel_bessemer (8.05 in, 8 out). These are recipe-by-recipe edits and cannot be fixed by (a) alone.

(c) BASE PRICES LAST, up the chain in order, because each tier's price is the next tier's input cost: lifting steel lifts machinery's inputs, and so on to spacecraft_components at 140. The ladder widens, ceil_mult re-derives (haulage_measure), and RESOURCES.md's 56x space premise moves. Doing (c) first is the route that compounds.

Recommendation: (a) then (b), re-run recipe_margin on every edit, (c) only where a row still fails, and re-derive ceil_mult once at the end. Say if you would rather move prices.

> **RESOLVED.** RESOLVED. Ben, 2026-09-02: approved the recommended order - rates and costs first, then input quantities where value-add is zero or negative, base prices last with ceil_mult re-derived once at the end. NR-777 records the consequence found on applying it: under k = 1.0 the processing half cannot go green from (a)+(b) alone.

