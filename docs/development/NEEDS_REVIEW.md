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

*7 entries — 4 open, 3 resolved.*

---

## Open

### NR-778 — Base prices are now ERA-BANDED - an ancient override table beside the shared one
*decision taken on your behalf · raised 2026-09-02 · from BL-744 stage 2, 2026-09-02. Applying reading A per band: the ancient band reaches steel by timber -> charcoal -> blooms -> steel (three doubling stages) where the industrial band reaches it in one (ore + coal). One shared table priced off the bloom chain put steel at 151 and dragged the industrial ladder to 600x iron ore; priced off the coal route it left every ancient bloom recipe red.*

world_gen.lua gains kepler_market.base_price_ancient (steel 113.0, ordnance 113.0, construction_capacity 6.6; absent = inherit), world_gen_config carries it, hard_coded_world seeds markets from base_price_for_epoch(epoch_year) using the recipe registry's 1700 threshold, and recipe_margin checks each band against its own table. Saves are untouched: a market carries its own base_price from seeding. This is the same banding recipes already have (BL-433) and demand baskets have (BL-640), applied to prices - the mechanism the anchor needed, not a new kind of thing. Ancient prices moved most: steel 8 -> 113, ordnance 43 -> 113, tools 25.5 -> 90.6, blooms 9 -> 24.6, rigging 14.5 -> 38; the industrial ladder tops at 112x iron ore (spacecraft_components 280). The ancient band is not the ruled sweep band (NR-774), so its calibration is the lower-stakes half. Overturn if you would rather one table with the ancient chain shortened to depth 1.

### NR-779 — Three constants moved beyond the ruling: processing maintenance 10 -> 2, extraction base_rate 20 -> 40, regolith 0.6 -> 1.0
*decision taken on your behalf · raised 2026-09-02 · from BL-744 stage 2, 2026-09-02. Each was needed to make a ruled change land, and each is a data change in economy.lua / world_gen.lua.*

1. PROCESSING MAINTENANCE 10 -> 2. You ruled the fixed-cost cut for extraction; at maintenance 10 the floor half (M2) was pricing every cheap processed good off the factory's lights rather than its inputs - clean_water 3 -> 11.9, silicon 5 -> 12.9, charcoal 4 -> 10.4 - and those prices then compounded up the ancient chain. At 2 the marginal half binds almost everywhere (clean_water 7.7, silicon 9.6, charcoal 6.5). Same lever, other building type; the 2026-09-01 ledger named maintenance as 80% of every credit leaving the field.

2. EXTRACTION base_rate 20 -> 40. economy.lua's own BL-436 note says the two producer base_rates scale TOGETHER to keep the 2.5:1 extraction:processing ratio; with processing at 16 (ruled), extraction at 20 would have made one processor at typical staffing eat 1.6 mines' output instead of 0.8, shifting the raw/processed balance the sweeps were measured on. Doubling it is also what lets maintenance 2 / wage 4 - the numbers on your card - clear stone at 1.0 at the floor without touching a raw price. Consequence to watch: deposits draw down twice as fast per site (reserve unchanged), so deposit life halves.

3. REGOLITH 0.6 -> 1.0, the one raw price moved. At 0.6 no fixed cost that leaves a mine a building could cover the floor half (an off-world site earns 3.0/tick at the floor against 4.8 of costs), and its in-situ steel route was re-costed to 8.5 units with it. Side effect: the "cheapest priced good" in the ceiling derivation is now 1.0, not 0.6, so ceil_mult's bound relaxes (haulage_measure re-run owed to MARKETS.md).

### NR-780 — The anchor route rule: k = 1.0 on a good's cheapest in-band route, k = 0 plus the floor half on every alternate
*decision taken on your behalf · raised 2026-09-02 · from BL-744 stage 2, 2026-09-02. Your sentence says ALL recipes; applied to every route at ONE price it forces every route to the same input cost, which erases the point of alternate methods (PRODUCTION.md § Alternate production methods - the easier route is meant to be easier).*

recipe_margin now finds, per band and per primary output, the route with the lowest marginal cost per unit (ties to the earlier recipe) and holds it to profit_over_marginal (1.0); every other route must clear alternate_profit_over_marginal (0.0 - profitable at base, no more) AND the floor half regardless, because the lights do not care which recipe runs. A recipe whose primary output is an extractable raw (hydroponics -> agricultural_produce) is always an alternate: the Farm is that good's cheapest route. Both constants are data in economy.recipe_margin_anchor. Under this rule 9 alternates stayed red on the ruled tables and were re-costed by quantity (charcoal 1.5 timber, the kiln 0.8 timber + 0.05 blooms, the converter 0.05 machinery, the in-situ smelter 8.5 regolith, hydroponics 5 units per batch); the rest cleared unchanged. Raise alternate_profit_over_marginal if you want alternates held to more than "profitable".

### NR-781 — Two harness rows red for design reasons after the retune - mining out-earns refining, and the seated corp earns 3x the field
*question · raised 2026-09-02 · from BL-744 stage 2 harness battery, 2026-09-02. Neither is a bug and neither test should be weakened to pass; both encode a claim the ruled constants now contradict.*

1. tier_margin R2 - "a processing facility out-earns an extraction site per tick" (RESOURCES.md: margin widens up the tiers). Measured after the retune: extraction 28.24/tick, processing 24.94/tick. Under reading A a processor earns profit = marginal cost (k = 1) while a mine at an untouched raw price earns 24x its marginal cost (iron_ore 2.40 margin on 0.10 wage per unit) and now runs at base_rate 40. Per BATCH the tiers still widen (spacecraft 138 margin vs iron 2.4); per BUILDING-TICK they do not. Options: (a) accept - the ladder claim is per unit of value, not per building, and reword R2; (b) raise raw prices so extraction margin is also ~k (propagates up the whole ladder, was rejected on the card); (c) halve extraction base_rate back to 20 and cut its fixed costs to ~1/2 instead (deposit life restored, mines earn less per tick). Recommendation: (a) now, revisit under BL-745 when the field runs.

2. spawn_solvency R3 - "the seated corp is not out-earning the field per holding (no handed subsidy)", tolerance 3.0x: seated 99.3 vs field 31.9 cr/qtr per holding. No subsidy path changed; the seated spawn's holdings are mostly extraction on rich deposits, and extraction income per building doubled with base_rate 40 while the field's mean includes idle processors. Recommendation: keep the row, raise the tolerance only with a stated reason, or re-express it as seated <= max(field) rather than 3x mean. Your call; nothing changed until you make it.

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

### NR-777 — What is "marginal cost" for a processor - inputs included (A) or conversion only (B)? The reading decides the size of the retune
*question · raised 2026-09-02 · from Applying NR-776 (approved) to the recipe_margin table, 2026-09-02. With k = 1.0 and inputs inside marginal cost, routes (a)+(b) cannot get a single processing recipe green: profit >= marginal cost means revenue >= 2 x inputs at EVERY stage, so route (c) - prices - is forced on the whole ladder, not the residue.*

Two readings of Ben's sentence, quantified with the cheapest route setting each good's price, raws pinned at their extraction price, k = 1.0:

A - MARGINAL COST INCLUDES INPUTS (the economics reading, and the one the harness runs today): every processing stage must double the value it takes in. At processing base_rate 16 (wage/batch 0.75): 24 of 50 goods re-priced - steel 8->13.5, machinery 22->55.5, alloys 34->79.5, electronics 29->41.5, spacecraft_components 140->277.5, ordnance 43->110.5, consumer_goods 12->55.5, tools 25.5->88.8 - ladder top 56x -> 111x; 17 alternate routes need their input cost cut to the cheapest route's bar (steel_from_blooms 54.5 -> 6.0, steel_bessemer 13.1 -> 6.0, hydroponics_bay 9.0 -> 0.8, the four construction methods ~25-40%). Every price-dependent tuning moves with it (demand baskets, reservation_mult, procurement lumps, build costs, spawn capital, ceil_mult), the sweeps' baselines go stale, goldens re-bless. In return M2 (fixed cost at the floor) becomes attainable at today's maintenance: steel's value-add 7.5/batch x 8 batches x 0.25 = 15/tick against 16.8 of wages + maintenance - nearly there, and the tiers above clear.

B - MARGINAL COST IS THE CONVERSION COST (wage per batch; inputs pass through at cost): value-add >= 2 x wage. At base_rate 16: 9 goods re-priced by small amounts (food_rations 6->7.5, consumer_goods 12->17, silicon 5->5.5, clean_water 3->4.5), ladder unchanged at 56x, 9 alternates need input cuts (steel_from_blooms 25 -> 6.5, hydroponics_bay 6.3 -> 1.5, charcoal 4.5 -> 2.5; steel's coal route 7.0 -> 6.5 or steel 8 -> 8.5). Cheap - but it leaves gross margins at 10-15% and M2 fails almost everywhere: steel's value-add 2/batch x 8 x 0.25 = 4/tick against 16.8, so the floor half needs maintenance cut ~4x and wages with it - a second, larger retune of the very constant the 2026-09-01 ledger named as 80% of drains.

Either way, processing base_rate 8 -> 16 (wage/batch 1.5 -> 0.75) is route (a) and halves the alternate-route cuts; and EXTRACTION's floor half is independent of the reading: at W = 0.5 a site needs price >= 3.8 to cover 9.5/tick of wages + maintenance at the floor, so raws either rise (iron_ore 2.5 -> ~4, which propagates up under both readings) or extraction's fixed costs fall (maintenance 5 -> ~2, wage 8 -> ~4).

Recommendation: A, with base_rate 16 - it is the sentence as written, it makes profit robust across the whole band rather than only at base, and it is the retune that also clears the floor half without gutting maintenance. It is a sprint, not a pass: one re-priced table, the 17 alternates re-costed, ceil_mult re-derived, sweeps re-baselined. B is the light touch if the 56x ladder must not move.

> **RESOLVED.** RESOLVED. Ben, 2026-09-02, via the session form: reading A (inputs included), processing base_rate 16, extraction floor half by cutting extraction fixed costs. Applied under BL-744 stage 2; the three delegated consequences are NR-778/779/780.

