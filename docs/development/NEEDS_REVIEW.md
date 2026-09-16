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

*15 entries — 15 open, 0 resolved.*

---

## Open

### NR-807 — CONCEPT.md still names the ancient arc as the live product, and the epoch moved to 1960
*question · raised 2026-09-09 · from The doc-contradiction sweep at the close of sprint 35.*

CONCEPT.md line 74 says: the live product is the ancient arc, the campaign epoch is 0 CE, the player a mercenary company. Your 2026-09-08 calendar makes the epoch 1960, and era_band_for_epoch flips to industrial at 1700 - so generation now runs the INDUSTRIAL arc. ALSO MANUAL.md (found 2026-09-09 during the Empires respan): lines 115 and 423 both say 'generation runs 4000 years of history, from 4000 BCE to the campaign epoch of 0 CE'. That is stale twice over -- the 2026-09-08 calendar moved the epoch to 1960, and the 2026-09-09 ruling made pass 1 3,600 years from 2400 BCE. It is the SAME call as CONCEPT.md's, so it is added here rather than opened as its own entry: whether the player-facing docs describe the ancient arc or the arc generation actually runs. CALENDAR HALF SETTLED (Ben, 2026-09-09): 'fix MANUAL.md to match the new calendar'. MANUAL.md sections 2.1 and 4.14 now state 2400 BCE -> 1960 with the two passes and the 1200-1560 coast. THE IDENTITY HALF IS STILL OPEN AND IS WHY THIS ENTRY STAYS SO: MANUAL.md line 35 still has the player arriving at the epoch as 'a company of armed professionals', and CONCEPT.md still names the ancient arc as the live product. A mercenary company arriving at an INDUSTRIAL 1960 epoch is the tension, and it is a product call rather than a date. The dates were fixed; the sentence about who the player is was deliberately left alone.

**Why it matters.** I fixed the generation-side citation because the calendar is that doc subject. I did NOT touch this one, because it is not a date - it names WHO THE PLAYER IS and which product is live. Changing the live arc from ancient to industrial in the doc that owns player identity is a product call, not a reconciliation, and CONCEPT.md is the authority the rest of the corpus reads for it.

- The live product is now the industrial arc; CONCEPT.md is updated and the mercenary-company framing revisited with it.
- The ancient arc stays the live product and 1560-1960 is generation own arc for this work, with both supported.
- The epoch move was about generation calendar only and CONCEPT.md is correct as written.

> **Recommendation:** The second, provisionally - both arcs already exist in the code (era_band_for_epoch branches on the epoch, and HISTORY.md was already written arc-aware), so nothing forces a product choice yet. But it should be YOUR sentence, not mine, and it is the kind of thing that quietly becomes true by being left alone.

*Files: `docs/CONCEPT.md`, `docs/generation/GENERATION_STRATEGY.md`, `src/world/era_band.hpp`, `docs/MANUAL.md`*

### NR-809 — Region count still scales ~quadratically AFTER BL-844, so regions cannot become provinces one-for-one - and no optimisation is waiting to change that
*decision · raised 2026-09-09 · from Sprint 37 build session, 2026-09-09. The region-count sensitivity measurement NEXT_SESSION.md asks for before any scale decision. Measured with history_span_cost, build_gen (/O2), seeds 0 and 1.*

Ben's brief says 'transform regions into provinces ... we don't have to aim for a small set'. That spans a factor of sixteen: 1,372 regions after 4,000 years against 22,153 land provinces. The measurement says the cost of that factor is not sixteen. Reading reach cost against region count across the span table, the exponent is 1.95-2.27 on seed 0 and 2.13-2.27 on seed 1 - reach is QUADRATIC in regions, as expected from a per-region Dijkstra (BL-844). Seed 0: 533 regions = 0.017 ms/rebuild, 833 = 0.045, 1174 = 0.079. Seed 1: 563 = 0.035, 931 = 0.114, 1260 = 0.218. Reach is 24-37% of the whole run. Sixteen times the regions is therefore ~256x the reach term: seed 1's 4,000-year run (6,541 ms total, 1,546 ms reach) would spend roughly 400 SECONDS in reach alone. CHECKED AGAINST BL-844 (reach Dijkstra heap), WHICH IS ALREADY COMPLETE - delivered 2026-09-09, the same day. These figures are the POST-FIX state, and they reproduce BL-844's own closing measurement exactly (it records 0.017 ms/rebuild at 529 regions rising to 0.079 at 1,160 on seed 0; this session measured 0.017 at 533 and 0.079 at 1,174). BL-844's delivery note is explicit that the remaining growth is NOT the algorithm: the heapless scan's quadratic is gone, and what is left is the NEIGHBOUR GRAPH DENSIFYING as regions fill in, so edges are not O(N) in this world and O(E log V) still grows. In its own words, that is a property of the graph rather than of the algorithm, and no further heap work fixes it.

**Why it matters.** It bounds objective 3 before a line of it is written. At today's per-region Dijkstra the region count can roughly DOUBLE inside a 4x wall-clock budget - about 2,500-3,000 regions on seed 0, which STARTUP.md's watched-wait ruling can absorb ('a watched wait needs no budget', Ben 2026-09-08). It cannot go to sixteen. That is not an argument against the design - BL-849 already ruled that colonisation SEEDS the partition and the late pass still DRAWS it, so regions never had to map one-for-one - but it does decide how far 'don't aim for a small set' can be taken in THIS sprint. AND THE CHEAP WAY OUT IS ALREADY SPENT. The first draft of this entry offered 'do BL-844 first' as the route to the sixteen-fold; BL-844 is done, and it explicitly declines to pursue the residual growth because reach is no longer what a long run is made of. So the choice is not between optimising and capping - it is between capping the region count and changing the ADJACENCY MODEL, which is a much larger piece of work than this sprint.

- Cap the colonisation span at roughly 2-3x today's region count (~2,500-3,000) and let the province partition keep doing the rest. BL-849 already rules that colonisation SEEDS the partition and the late pass still DRAWS it, so regions never had to map to provinces one-for-one. Ships this sprint; nothing new is needed.
- File a new item against the ADJACENCY MODEL itself - the neighbour graph densifies as regions fill in, which is what BL-844 left standing. Capping neighbour degree, or moving reach off a per-region graph entirely, is what a sixteen-fold region count would need. That is a sprint of its own and it goes in front of the span Ben asked for.
- Raise the region count anyway and accept a multi-minute watched wait in round 4, on the grounds that STARTUP.md rules the wait IS the content.

> **Recommendation:** Option 1 for this sprint, with option 2 filed rather than done. Two reasons, and the second is the stronger. First, Ben's stated acceptance criterion for round 4 is the ARC - origin, communication, conquest or union, a stable dark age - and that arc reads at 2,500 regions exactly as it reads at 22,000; spending the sprint on a scaling term buys resolution on a surface whose own test is shape. Second, the colonisation walk itself is NOT what scales badly here: it is a single multi-source flood over ~9,500 land tiles, linear in tiles and independent of region count. What scales is the Era -1 sim's per-region reach, which is a different pass with a different owner. So capping the region count costs the colonisation design nothing at all - it only defers how finely the sim resolves the politics on top of it.

*Files: `tools/verify/history_span_cost.cpp`, `src/world/history_sim.cpp`, `docs/generation/COLONISATION.md`, `docs/generation/PROVINCES.md`*

### NR-811 — Round 4 pays a full world build, and Begin then pays it again
*novel-work · raised 2026-09-09 · from Sprint 37 build session, 2026-09-09. Raised as a novelty flag by the BL-829 implementer rather than assumed acceptable.*

To obtain one recorded history pass, round 4's worker runs make_hard_coded_world and throws the world away, keeping only the record. Pressing Begin at the end of the wizard then builds the world again. Roughly 73 seconds twice in Debug.

**Why it matters.** The approach is defensible and was chosen for a good reason: it uses generation's own single invocation, and the alternative - re-deriving settlement and the era inside the UI layer - is precisely the drift era_minus_one.hpp exists to stop (it is the seventh-caller defect NR-733 closed). But paying a full world build twice is not an idiom any doc owns, and nobody has decided it is acceptable. STARTUP.md's 'a watched wait needs no budget' (Ben, 2026-09-08) covers the wait INSIDE round 4; it does not obviously cover paying for it a second time at Begin.

- Accept it for the prototype - the wait is watched in round 4 and the Begin wait already existed.
- Cache the round-4 world and hand it to Begin, so the second build is skipped. Needs the wizard to hold a world rather than a record, which is a bigger change than it sounds.
- Have Begin reuse the recorded pass rather than re-running it, which is the same shape as the NR-733 fix one layer up.

> **Recommendation:** Accept for now and revisit if the Debug figure is representative of Release, which has not been measured. Worth measuring before deciding - build_rel timings and build timings are not comparable, and the 73 s figure is Debug.

*Files: `src/ui/startup_screens.cpp`, `src/core/app.hpp`, `docs/ui/STARTUP.md`*

### NR-819 — Twelve farm classes, four Colonisation branches — which class teaches which ground, and is there a fifth branch?
*question · raised 2026-09-10 · from Authoring COLONISATION_TREE.md: the brief assumed one branch per origin farm class and sized the tree for four; classify_farm_class in src/world/colonisation.cpp yields twelve (boreal, volcanic, floodplain, montane, coastal, woodland, arid, valley, highland, grassland, stone, steppe) and COLONISATION.md names none.*

The tree folds them into four ground families — Wet Ground (floodplain, valley), High Ground (highland, montane, volcanic), Open Ground (grassland, steppe), The Shore (coastal) — and leaves woodland, boreal, arid and stone belonging to no branch. A people coined on unowned ground climbs the spire on contact alone. Related: the gate vocabulary has no highland atom, so High Ground's majors carry no gate.

**Why it matters.** Which ground teaches what is the whole content of a carried tree; a class that teaches nothing makes its peoples uniformly poor, and a fifth branch re-shapes the tree (rule 4 breadth, milestone requires_any sets). Cheap now, expensive after BL-882.

- A - fold the four unowned classes into the nearest family (woodland/boreal -> High Ground, arid/stone -> Open Ground) and add a table to the doc
- B - a fifth branch, Forest, for woodland and boreal; arid and stone stay unowned as hostile country
- C - unowned classes teach nothing by design: peoples from hostile ground are late by construction

> **Recommendation:** A, with a highland gate atom raised as its own small item only if the sweep shows High Ground nodes held by peoples who never stood on high ground. B is the deeper option and costs ~7 nodes; the cap has room.

*Files: `docs/generation/trees/COLONISATION_TREE.md`, `docs/generation/trees/colonisation_tree.json`, `src/world/colonisation.cpp`*

### NR-827 — BL-895: trade income is still 0.25% of production after both sinks land, and upkeep claims 63%
*question · raised 2026-09-11 · from BL-895's remaining half -- build both sinks, then re-measure whether trade becomes a material share. Measured across 16 seeds at --epoch 0.*

With a standing-army upkeep of 200 per 1000 heads per year and a road build cost of 2000: production fell 241,000,000 -> 113,922,140 per world, the three sinks together claimed 63% of it, and 16,026,384 heads were sent home unpaid. The sinks are unambiguously live. But TRADE is 281,280 of that 113,922,140 -- 0.25% -- so what makes war affordable is still INDUSTRY, not the network. The item's own 'done when' asks that a polity connected to unlike ground sustains campaigns a disconnected one cannot, and that is not yet what the numbers say.

**Why it matters.** Raising `trade_income_per_link` until trade is a visible share would be fitting a figure to a target, which is the thing this project refuses. The honest readings are either (a) trade should be paid on something that scales -- the number of unlike PAIRS a realm touches is bounded by its adjacency, so a per-link constant can never grow with a realm the way industry does; or (b) trade is correctly a minor income in this phase and the 'rich pay for war' story belongs to pass 2, where resources become capital.

- Accept trade as a minor income here; say so in CIVILISATION.md and stop tuning it.
- Re-base trade on something that scales with a realm (e.g. distinct trade classes reachable, not roaded pairs), then re-measure.
- Cut industry's yield instead, so the same trade figure becomes a larger share.

> **Recommendation:** Option 2 is the only one that changes the mechanism rather than the arithmetic, and it keeps the no-market constraint intact -- 'how many unlike things can this realm reach' is still a property of the network and still reads only DIFFERENCE.

*Files: `src/world/history_sim.cpp`, `docs/generation/CIVILISATION.md`*

### NR-828 — BL-899: no launched crossing starves any more, and the max ration (900) sits close to the full forage you rejected
*question · raised 2026-09-11 · from BL-899 (seafaring creed), delivered 2026-09-11. Measured across 16 seeds at --epoch 0 against a sea_legs_ration_q=0 control.*

The spectrum is genuinely live in the code -- ration = sea_legs_ration_q * legs / 1000, so a people at the floor lands on 270 per-mille and one at full legs on 900. But of the 405 wet crossings that actually LAUNCH across the sweep, ZERO starve: every hub that clears the port and can_field_naval gates also clears the sea-legs floor of 300, so the floor is redundant with the port gate rather than discriminating on its own. Wet launches themselves fell 816 -> 405 and conquests rose about 9% (median 526 -> 573); the arc held, hegemony stayed 0/16.

**Why it matters.** Your call 1 was explicit that a people with no sea legs STILL STARVES, and in the measured world nothing does. The discrimination that makes 'most peoples cannot' true is happening upstream at the BL-778 legality gate (median 969 wet contacts refused per world), not at this floor. Separately, 900 is close enough to full forage that the inversion you rejected -- coastal ground becoming cheaper to take than inland ground -- is worth testing directly rather than assumed absent.

- Leave both magnitudes; the upstream legality gate is doing the discriminating and that is legitimate.
- Lower the max ration well below 900 so a crossing is always visibly dearer than a dry march, and re-measure whether coastal ground is cheaper.
- Raise the floor above what the port gate already implies, so the floor discriminates on its own rather than being redundant.

> **Recommendation:** Worth one measurement before any tuning: compare the cost of taking coastal ground against inland ground directly. If coastal is not cheaper, option 1 is honest and nothing needs changing. Neither magnitude should be moved to make a number look right.

*Files: `src/world/history_sim.cpp`, `src/world/era_minus_one.cpp`, `docs/lore/CREEDS.md`*

### NR-829 — BL-898: an inherited grudge decays away in about three campaign years
*question · raised 2026-09-11 · from BL-898 (grudges seed nation sentiment), delivered 2026-09-11. Raised by the building agent; nothing about decay was changed.*

Era -1 grudges now seed nation->nation sentiment at world setup: on a real generated era, 22 grudge pairs produced 17 seeded rows, worst trust -8.835, mean -4.621, and the quarrel stays printable as who-wronged-whom-where. But the authored trust decay is a NINE-TICK HALF-LIFE (economy.sentiment, NR-568), so a -8.8 opening grudge is under -1 within roughly three campaign years.

**Why it matters.** CIVILISATION.md sec What the dark age must leave names live grudges as one of three things the dark age hands to a colonial era, on the reasoning that WHO COLONISES WHOM IS NOT A FRESH ROLL -- it is the last quarrel continued by other means. A grudge that is gone before the player has finished their opening moves cannot do that job, and the item would be delivered in the letter while failing in the substance. The opposite reading is equally defensible: a starting condition SHOULD fade, because a history the player did not live through should not bind them forever.

- Leave it. Seeded sentiment is an opening condition and fading is correct.
- Exempt the historical_grudge factor from decay, or give it a much longer half-life, so an inherited quarrel persists as a standing fact.
- Keep the decay but let the grudge re-assert -- a slow floor the pair cannot rise above while the record stands.

> **Recommendation:** Option 3 if the colonial-era reading is the one you want: it keeps the quarrel legible and consequential without making a thousand-year-old wrong permanent. Worth deciding before any colonial-phase item reads this sentiment, because all three produce very different opening maps.

*Files: `src/world/grudge_sentiment.cpp`, `docs/politics/RELATIONS.md`*

### NR-877 — AUTHORISE: the sprint-42 wave-1 re-bless — five named causes, one world
*decision · raised 2026-09-16 · from Sprint 42 wave 1 (the batch), sixteen items merged and verified on sprint-42-wave-1.*

DELIVERY.md sec The digest re-bless is one act per WAVE: one authorisation, N named causes, each measured in isolation in its own worktree. FIVE items moved the world; eleven did not.

CAUSE 1 — BL-961 (planetology thermal series). Coal and petroleum magnitudes scale by the interior's own budget at the fossil epoch instead of today's; about a percent over the record's depth, world deposits +0.086%. Moved all four digests in isolation.
CAUSE 2 — BL-967 (rivers priced in the walk). A river step now takes the same 30% corridor discount as a shore step (it was 22%, a river-cheaper-than-shore preference no doc stated). The migration reaches inland earlier: 4,000-11,000 tiles earlier per world, none later. More cultures coin on a different farm class (seed 0: 380 -> 444). Moved all four digests in isolation.
CAUSE 3 — BL-972 (force upkeep in the world). Navies and standing armies pay a per-head bill from the treasury each round, the scorer's saturation caps are gone, and a paid army is levied from the ground's manpower. Moved seedA/on only.
CAUSE 4 — BL-973 (tree effects generated). Every node effect in the two wired trees now reaches the sim through one generic apply; the three hand-wired nodes are retired. Effects that did nothing now do something, in both spans. Moved seedA/on, seedB/on and the 1960 arc.
CAUSE 5 — BL-975 (nation treasury from Exploration). A nation opens with its folded polities' 1660 chest through one stated per-mille (0.01 per mille; about thirteen quarters of its own levy for the median nation), so garrisons differentiate. Moved seedA/on and seedB/on.

THE SHAPE, not the hash. The world the player would generate today: people spread further inland and split into more, smaller peoples; realms that hold ground pay to keep armies and fleets standing and can bankrupt themselves doing it; a realm's technology actually changes what it can do; and a nation arrives at the campaign with the money its history banked. The Exploration round is calmer and more commercial than it was (Exploration over 16 seeds: battle rate 55.9 -> 28.7 per century, displacement median 1.35 -> 1.70 while the pooled ratio fell 1.68 -> 1.28, held seeds 6 -> 5, silent seeds 9 -> 4 and 7, trade flows 687 -> 643. Empires: more and smaller realms (powers at 1200 49 -> 55, largest share by people 110 -> 100 per mille, peak 132 -> 116) with slightly more fighting (battles 6479 -> 6762, conquests 5464 -> 5843)).

THE DIGESTS. Authorised wave-0 baseline -> integrated wave-1: seedA/on 457483363D79D700 -> 983298AE413B0A8E; seedB/on 728607C66CE6A4BE -> F2A66ACE583F1784; seedA/off F9BF05466A631FF9 -> 5346EB2A9C4E1144; 1960 two-span 851FE345B2E37618 -> 82EE79155BA2F47D.

THE PIN. exploration_sim_harness R3b (the w_want_q = 0 span) reads battles 322 (pinned 308), conquests 207 (306), foundings 739 (472), subjections 65 (5), freed 62 (1), tribute 245,920,676 (134,358,620), treaties 479 (293), broken 0 (2), owner changes 2410 (2196). Left red with this entry as its stated cause.

EVERYTHING ELSE IS GREEN on the integrated tree: world_determinism, pass_one_handoff, colonisation_harness, deposit_origin, planetology_harness, continent_drift, tile_height_retention, survey_endowment_harness (34/0), earthlike_tile_census (120 seeds, 0 fail), tree_lint (all four, both headers fresh), landscape_score/search, haulage_measure (2035 all / 1627 intra-body against the 1055/802 baseline), demand_census. history_sim_harness stays at its 2-failure baseline; era_world_harness's three reds pre-date the wave (BL-1010).

**Why it matters.** A re-bless is the moment the old world stops being reproducible. Ben authorises against the shape above, never against the hashes, and until he does the pin stays red and the digests stay unblessed.

- Authorise: re-pin R3b to 322/207/739/65/62/245920676/479/0/2410 and record the four digests as the baseline.
- Authorise the physical and economic causes but hold BL-972 or BL-973 for a live look at the round first.
- Not yet: read the wave live in build_rel before anything is re-pinned.

> **Recommendation:** Option 1 if the round reads right live; the causes are attributable one by one, and every reading that judges them is now checked in.

### NR-878 — CALL: nothing in the scorer reads the purse, so a polity spends itself to nothing once the caps are gone
*novel-work · raised 2026-09-16 · from BL-972 (force upkeep in the world), measured over 16 seeds against a control run.*

With the saturation caps deleted, a polity ranked above about 500 on either lean buys a stock step whenever one is affordable, and the new per-head bill then drains what it bought with. Over the span the per-seed MEDIAN polity treasury falls from 1.26M to 211 while the top decile loses about 3%; roughly 14% of polity-rounds cannot pay the army bill. Rate sensitivity says it is structural, not a tuning miss: at a tenth of the rate the median still ends at 181k, because a cap-sized army over 115 rounds costs about one median hoard. The caps were hiding a 600x treasury spread.

**Why it matters.** EXPLORATION.md sec Force persists asked for a cost in the world rather than a brake in the scorer, and it got one — but the scorer's hold term still has no purse in it, so the actor has no reason to stop before it is broke. Displacement's median fell to 0.85 on this arm (pooled held at 1.64), which is the visible consequence.

- Accept the spent-purse shape: a realm that over-builds is poor, and poverty is the brake.
- Add a solvency term to the step's eligibility — a step needs next round's bill covered — which is in the world rather than in the scorer's ranking.
- Keep a cap as well as the bill, and say in the doc that the cap is the actor knowing its own limit.

> **Recommendation:** Option 2. It is the smallest in-world rule that gives the actor a reason to stop, and it leaves the bill as the cost the doc asked for.

### NR-879 — CALL: 85% of coined cultures never hold ground — fold them into the parent, or stop coining them?
*decision · raised 2026-09-16 · from BL-968 step 1 (ephemeral cultures measured), 16 seeds.*

Pooled: 5,453 cultures coined, 790 holding ground when the Empires round opens (14.4%), 471 still holding at 1200 CE (8.6%). The empty mass is overwhelmingly first-generation daughters (4,455 empty leaves against 208 empty interior nodes); no cradle is ever empty. Holding falls a further 40% during the Empires round through assimilation and conquest.

**Why it matters.** The lineage palette, kinship years, culture opposition and the Empires round's polity seeding all walk a tree whose leaves are mostly names nobody ever lived under. Step 1 measured it and built no rule, by the brief.

- A) Fold an empty culture back into its parent at the boundary, keeping the lineage link; the tree shrinks about 85%, empty interior nodes need re-parenting, every digest moves.
- B) Gate coining on a minimum settled footprint, so the name never exists; the split counters move, the tails and the Empires-round erosion are untouched, every digest moves.
- Neither yet: the count is harmless while nothing reads it, and the Empires-round erosion is the bigger half anyway.

> **Recommendation:** B, if either. It keeps the record self-consistent without re-parenting, and a culture that never settled anywhere is a name the generator should not have minted.

### NR-880 — READING: four of the six held seeds have no frontier at all, and alarm saturates on every near pair
*question · raised 2026-09-16 · from BL-999 (held seeds cause measured), 16 seeds.*

Seeds 0, 3, 6 and 7 are NO FRONTIER: by 1660 they have met exactly one polity across a landmass. Tens of thousands of unmet candidates reach the scorer each run, 2-8% clear the threshold, and one or two are ever chosen in 460 years — so the frontier those seeds could displace onto never opens. Seed 15 is NO EXPLORER (no polity took the rim). Seed 12 fits none of the four labels: 91% of its near pairs are bound by treaty, and the four unbound pairs carry 51% of allowed near campaigns against the frontier's 0.9%. Cross-cutting: visible_capability_reference = 5000 saturates, so alarm reads 1000 on essentially every near pair on every seed and the deterrence weight acts as a flat constant rather than a discriminator.

**Why it matters.** Displacement is the Exploration phase's one structural claim, and the census says the held seeds are not failures of deterrence but of first contact: the constants implicated are the campaign threshold and prize pricing for an unmet target, not the alarm weight that was tuned in sprint 41.

- Accept the census and file the first-crossing question as its own item.
- Accept a fifth label (UNCLASSIFIED) for seed 12's shape, or widen DETERRENCE-INERT to cover "bound majority, unbound remainder carries the war".
- Re-scale visible_capability_reference first, so alarm discriminates before anything else is read.

> **Recommendation:** All three eventually; the saturation finding is the cheapest to act on and it makes every later deterrence reading mean something.

### NR-881 — CALL: the seat viability floor reads eight quarters, and the pre-game is now twelve ticks
*question · raised 2026-09-16 · from BL-978 (warm start retired): a Release run seated with "shortlist 0 of 8 specialists — VIABILITY FLOOR UNMET".*

The warm start is gone: the settle is phase 6's single validation run, 12 ticks, chosen as the first tick at which the 4- and 8-tick trailing convoy means are both within 5% of the settled level. The seat shortlist's floor reads solvency plus trailing net over eight quarters — and over ticks 5 to 12 the field is still ramping, so on the measured run no specialist cleared it.

**Why it matters.** The player picks a seat from that shortlist. Either the floor's window is wrong for a 12-tick pre-game, or 12 ticks is too short for what the floor is asking, and the two cannot both stand. The 2026-08-26 record says most corps were underwater at 80 ticks too, so this may be an old condition the shorter settle merely exposed.

- Shorten the floor's window to the settle's length and re-read it.
- Lengthen the validation run until the floor is meetable, and say in ERAS.md that the seat, not the trade, sets the settle's length.
- Neither: the floor is measuring the wrong thing for a world that has not traded yet, and it should read the static score instead.

> **Recommendation:** Option 1 first, because it costs nothing to measure; option 3 is the honest answer if the shortlist stays empty.

### NR-882 — DECISION TAKEN: the PROPOSED readings in DIGITISATION.md the form did not ask about stand until overturned
*decision taken on your behalf · raised 2026-09-15 · from Digitisation design session, elicitation form follow-through (BL-982..BL-997).*

Ben ruled nine calls and set the three beats and seven properties. Eight further readings were written into DIGITISATION.md as PROPOSED and filed into items without being asked: (1) 1960 computing is the existing electronics good, no new resource; (2) an industry point is LOCATED at a centre and capturable; (3) the Works fork (State Arsenal / Private Works) decides spend versus stockpile; (4) migration is two streams (urbanisation, emigration) pulled by industry-point output; (5) colonies are lost by renewal refusal plus a holding cost that scales with the subject; (6) proxy war is the arms race's next displacement, a patron link; (7) war kills through exactly three mechanisms — conscript dead, displacement, blockade famine; (8) a world war spreads through the mutual-defence clause; plus the stub sources for tech, regime and garrisons. Also: Ben's note on war deaths was read as taking MILITARY_HISTORY.md's authored-mechanism exception for the Digitisation span only, not overturning war-is-non-demographic for Empires and Exploration.

**Why it matters.** (7) and the scope of the war-deaths reading decide whether the dead-region loop can return; (2) decides whether capturing an industrial city is worth a war; (3) decides why two industrial powers open with different corporate webs.

- Keep all as written.
- Overturn any by number; the doc and its item are amended together.
- Apply war deaths to the Exploration span as well.

> **Recommendation:** Keep all as written. Each is a consequence of an upstream scalar the sim already holds, and each is filed with a reading that would show it failing.

*Files: `docs/generation/DIGITISATION.md`, `docs/generation/MILITARY_HISTORY.md`*

### NR-883 — DECISION TAKEN: the detail written under the ten trade and demand rulings
*decision taken on your behalf · raised 2026-09-15 · from Trade and demand design form follow-through (BL-995, BL-996, BL-1003..BL-1006).*

Ben ruled ten calls. The specifics below were written into the docs without being asked: (1) the industrial rung assignment — 1 staples, 2 clean water and medical supplies, 3 consumer goods, 4 refined fuel, 5 electronics (consumer goods move from every centre to Town+); (2) ancient rungs, marked PROPOSED; (3) heads_per_demand_unit derived from the generated distribution and held fixed in play, so a growing city grows its market; (4) qualification scales rungs 4-5 only; (5) the net-price dispatch quantity bound q = supply_d x ((price_d / landed)^2 - 1), from the sqrt price law, with a non-zero data threshold; (6) the arrival duty REPLACES the matched-trade charge (one point of charge), is priced at the destination, and off-world markets pay none; (7) labour pools stay per body while goods pools go per market; (8) a body with no market keeps one body-level pool until its first building spawns one; (9) uniform base_price is kept — gaps come from forces. Also filed: the background-demand stopgap multiplies with market count and counts razed centres (folded into BL-996 as a defect).

**Why it matters.** (1) moves consumer goods off small centres, which shrinks demand in village markets; (3) decides whether demand grows with a city in play; (6) decides whether a protected nation earns anything from trade that never touches the order book.

- Keep all as written.
- Overturn any by number; doc and item amended together.
- Keep consumer goods at every stratum and start the ladder at clean water.

> **Recommendation:** Keep all as written. Each is the smallest reading of a ruled call that the reading items (BL-1006, demand_census) can show failing.

*Files: `docs/economy/POPULATION.md`, `docs/economy/SUPPLY.md`, `docs/economy/MARKETS.md`, `docs/economy/PRODUCTION.md`*

### NR-884 — COLONIAL_ERA.md merged a week late and claims a span two other docs now own
*decision · raised 2026-09-16 · from The four-branch merge at the 2026-09-16 session close.*

docs/generation/COLONIAL_ERA.md was written 2026-09-09 on claude/sprint-39-market-design-a7936b and sat unmerged for a week. It owns "the span after the dark age: 1560 -> 1960" and specifies the wizard's round 5 as a STILL. Since it was written, EXPLORATION.md has taken 1200 -> 1660 and been BUILT (treasuries, treaties with a term, colonies as subjects, decaying ports and navies, trade flows), DIGITISATION.md has taken 1660 -> 1960 and carries twenty filed items, and both wizard rounds are time-lapses, not stills. Merged with a banner saying all of this rather than dropped, because the SUBJECT it owns is not superseded: the demand side generated as history - how a people comes to want a luxury it cannot grow, where a nation's opening wealth is from, the two ways ground is claimed across water and what each costs, and the ordering that corporations come AFTER the demand they answer. It also brought 21 backlog items (BL-812, BL-818..843, BL-874..880), each stamped with the same warning in its design field.

**Why it matters.** Three documents now describe overlapping centuries and only two of them are true about the code. A doc that is wrong about a date but right about a mechanism is the most expensive kind to leave alone, because the next reader takes the date with the mechanism. It is also the exact shape the standing rules warn about: a ruling lands in its owning doc and a sibling keeps asserting the overturned claim. Digitisation is the next phase, so this gets read soon.

- Fold the demand-side content into DIGITISATION.md and EXPLORATION.md where each half belongs, then retire COLONIAL_ERA.md to docs/research/ - the same treatment COLLAPSE.md got on 2026-09-16.
- Keep it as its own authority and re-span it to whatever is left that neither sibling owns, which may be nothing.
- Leave it with the banner: it is honest about its own staleness and costs nothing until somebody reads it.

> **Recommendation:** The first, and it is the same call Ben already made once this week on COLLAPSE.md: fold the parts the live docs lack into them and retire the rest. The 21 items it brought want the same pass - several are likely duplicates of the Digitisation block (a cultural-demand item exists on both sides), and that is cheaper to settle in one reading than one item at a time. Not done here because merging a branch and re-writing three authority docs are different acts, and the second is a design session.

*Files: `docs/generation/COLONIAL_ERA.md`, `docs/generation/EXPLORATION.md`, `docs/generation/DIGITISATION.md`, `docs/development/backlog.json`*

---

## Resolved

Kept, not pruned: the reasoning is the point. Prune only in a deliberate sweep, once the
answer has landed in an authority doc.

