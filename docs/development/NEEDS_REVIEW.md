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

*40 entries — 40 open, 0 resolved.*

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

### NR-810 — Round 4 promises four thousand years and plays four hundred, because settlement places the whole map before the sim starts
*observation · raised 2026-09-09 · from Sprint 37 build session, 2026-09-09. Found by building BL-829 (time-lapse view) and watching what it draws; confirmed in the main session from scripts/verify/history_lapse_press.lua, whose own assertion reads 'the record spans years (-400 -> 0)'.*

Round 4's subtitle asks 'Who claimed this ground, and who lost it, over four thousand years?' The record it plays covers 400. On the first frame every continent is already claimed and all 12 powers already exist, because run_settlement places all ~533 regions and dates them BEFORE run_history_sim starts - so what the map animates is borders shifting, not land filling. 384 foundings do happen inside the run, but they land on ground that already has an owner colour.

**Why it matters.** It is not a drawing problem and it cannot be fixed in the UI. Ben's stated acceptance criterion for round 4 is the ARC - origin, communication, conquest or diplomatic union, a stable dark age - and the origin phase is not missing from the DRAWING, it is missing from the RECORD. Round 4 is currently showing the last tenth of the story it advertises. It also means objective 2 ('make sure it works and produces interesting cultures') cannot be judged from this round yet: there is no spreading to look at.

- Wire BL-846 (colonisation span) so foundings happen INSIDE the recorded span - run_settlement takes each site's founded_year and culture from the colonisation flood's arrival record instead of from a settle-score formula, and the sim's pre-boundary span plays that schedule out, emitting an ownership change as each region is reached. The flood is already built and harnessed.
- Leave the round honest about its scope in the meantime - retitle it to the span it actually plays - and treat the arc as blocked on the colonisation integration.
- Both: retitle now, wire BL-846 next.

> **Recommendation:** Option 3. The retitle is a one-line honesty fix and should not wait; the integration is the real answer and is the natural next piece of sprint 37, since the flood exists and is green. Worth noting that this finding independently confirms an architectural reading that was genuinely ambiguous when the span was being built: COLONISATION.md could be read as colonisation dating the map before the sim, or as founding regions inside it. A round built to show the arc proves it must be the second.

*Files: `src/world/settlement.cpp`, `src/world/history_sim.cpp`, `src/world/colonisation.cpp`, `src/ui/history_lapse.cpp`, `docs/ui/STARTUP.md`*

### NR-811 — Round 4 pays a full world build, and Begin then pays it again
*novel-work · raised 2026-09-09 · from Sprint 37 build session, 2026-09-09. Raised as a novelty flag by the BL-829 implementer rather than assumed acceptable.*

To obtain one recorded history pass, round 4's worker runs make_hard_coded_world and throws the world away, keeping only the record. Pressing Begin at the end of the wizard then builds the world again. Roughly 73 seconds twice in Debug.

**Why it matters.** The approach is defensible and was chosen for a good reason: it uses generation's own single invocation, and the alternative - re-deriving settlement and the era inside the UI layer - is precisely the drift era_minus_one.hpp exists to stop (it is the seventh-caller defect NR-733 closed). But paying a full world build twice is not an idiom any doc owns, and nobody has decided it is acceptable. STARTUP.md's 'a watched wait needs no budget' (Ben, 2026-09-08) covers the wait INSIDE round 4; it does not obviously cover paying for it a second time at Begin.

- Accept it for the prototype - the wait is watched in round 4 and the Begin wait already existed.
- Cache the round-4 world and hand it to Begin, so the second build is skipped. Needs the wizard to hold a world rather than a record, which is a bigger change than it sounds.
- Have Begin reuse the recorded pass rather than re-running it, which is the same shape as the NR-733 fix one layer up.

> **Recommendation:** Accept for now and revisit if the Debug figure is representative of Release, which has not been measured. Worth measuring before deciding - build_rel timings and build timings are not comparable, and the 73 s figure is Debug.

*Files: `src/ui/startup_screens.cpp`, `src/core/app.hpp`, `docs/ui/STARTUP.md`*

### NR-812 — Does the wizard's ACTIONS.json exemption cover a Run button on a pass round?
*question · raised 2026-09-09 · from Sprint 37 build session, 2026-09-09. The BL-829 implementer was briefed that round 4 was outside the exemption, read the doc as broader than the brief, followed the doc, and flagged the disagreement rather than silently picking either way.*

The brief for round 4 said the pre-game-wizard ACTIONS.json exemption covers the preference rows, so a new Run button needs an entry. ACTIONS.json's own _note (Ben, 2026-09-09, ruling on NR-804) reads wider: 'The startup wizard - its rounds, per-round Reroll, Next and Begin - gets no entries here and no startup.* family... The any-control-change-updates-its-entry rule above does not reach it.' A Run button on a wizard round arguably sits inside 'its rounds'. No entry was added.

**Why it matters.** Small either way - the entry is a two-minute add - but the exemption's edge decides whether every future control on rounds 4 and 5 needs one, and those rounds are about to grow controls (transport, leans). Better settled once than re-argued per control.

- The exemption covers the whole wizard including new controls on its rounds. Nothing to do.
- The exemption covers only the rounds' NAVIGATION and preference rows; functional controls like Run get entries.

> **Recommendation:** The implementer followed the doc as written, which was the right call. If option 2 is what was meant, the _note wants a sentence saying so, because as written it reads as option 1.

*Files: `docs/ai/ACTIONS.json`, `src/ui/startup_screens.cpp`*

### NR-813 — Should round 4's time-lapse get a scrubber?
*decision · raised 2026-09-09 · from Sprint 37 build session, 2026-09-09. BL-829 left transport controls deliberately open, to be decided by watching rather than in advance.*

The plain transport was built as briefed: Run, then it plays, then Restart. The question BL-829 parked is whether the player may scrub, pause or replay.

**Why it matters.** The wizard's standing premise is 'you set conditions, you do not steer', and the globe's no-input ruling is a real precedent against any steerable control. A scrubber would be the wizard's first.

- Keep it plain - Run and Restart only. Consistent with the globe ruling.
- Add a scrubber. The argument for it: the interesting moments are unevenly spaced - most centuries are static and two or three are not - and at ~30 s for the span you cannot go back to the one you missed. Restart-and-rewatch is a 30-second answer to a 2-second question.
- Add pause as well.

> **Recommendation:** Not yet, and for a reason that outranks the argument: the round currently plays 400 years of a 4000-year record (NR-810), so how it should be steered cannot be judged from what it does now. Decide after the colonisation span is wired and there is a real arc to sit through. If it is decided sooner, a scrub earns its place and a pause does not - a frozen map is what the round already looks like most of the time. The ~30 s duration is also unvalidated and was not Ben's.

*Files: `src/ui/history_lapse.cpp`, `docs/ui/STARTUP.md`*

### NR-815 — Culture opposition is SYMMETRIC, so relations are a matrix and not directed pairs
*decision taken on your behalf · raised 2026-09-09 · from The Empires design pass; the elicitation form settled the two opposition AXES but not the shape of the relation.*

Opposition between two cultures is symmetric: a triangular matrix over cultures rather than a directed table. The directed layer is left to `grudge`, which already records who wronged whom at a place and a date.

**Why it matters.** It decides the data shape before anything reads it, and a directed table is hard to make symmetric later once consumers assume asymmetry. It also keeps two questions apart - `are we opposed` and `what did you do to me` - which would blur into one quantity if opposition were directed too.

- Symmetric matrix, grudges stay directed (TAKEN)
- Directed, mirroring the grudge table
- Symmetric with a directed override

> **Recommendation:** Symmetric. Ben's own phrasing is 'cultures have similarities, and sometimes directly opposing views' - a view held in opposition is held by both sides. Overturn this if a one-sided resentment should be expressible WITHOUT an event behind it; today every such case has a grudge.

*Files: `docs/generation/CIVILISATION.md`*

### NR-816 — Kinship is measured in YEARS since the common ancestor, not in tree hops
*decision taken on your behalf · raised 2026-09-09 · from The Empires design pass; the form did not carry this call, and CIVILISATION.md left it open as 'does kinship decay'.*

Similarity between two cultures is the time since their most recent common ancestor, not the number of hops between them in the descent tree. This needs one new retained field - the year a culture was coined - in the same shape BL-865 (culture descent retained) used for `parent`.

**Why it matters.** It answers the open 'does kinship decay' call without introducing a decay constant, which is the kind of dial this layer keeps refusing. Hop count is also coarse and blind to timing: the tree runs 9-10 deep over 676 and 929 cultures, so two peoples four hops apart may have parted three thousand years ago or three hundred, and those are not the same relationship.

- Years since the common ancestor (TAKEN)
- Hop count alone
- Hop count with an authored decay constant

> **Recommendation:** Years. It costs one integer per culture that the migration already knows, and it makes the measure a fact about the world's history rather than a number assigned to it. The cost is that the coining year must be RETAINED - a third discarded migration fact, alongside parentage and origin farm class.

*Files: `docs/generation/CIVILISATION.md`, `src/world/creeds.hpp`*

### NR-817 — A civilisation cannot form across opposition above a bar, and inherits what is left as strain
*decision taken on your behalf · raised 2026-09-09 · from The Empires design pass; CIVILISATION.md asked whether a civilisation resolves, inherits or fractures over opposed member cultures.*

Two of the three candidates apply at different moments. FRACTURE is the formation rule - peoples too opposed do not produce a shared answer about how to live, so no civilisation is coined there. INHERIT is the consequence - one that does form over residual opposition carries it as internal strain. RESOLVE is rejected.

**Why it matters.** Resolve would make a civilisation a peacemaker that flattens the world at exactly the scale the design wants asymmetry, which is a forced outcome. Inherit alone gives no reason for a civilisation NOT to form everywhere, so the map would carry one over every mixed region. The pair gives both a gate and a consequence.

- Fracture as the gate, inherit as the consequence (TAKEN)
- Inherit only
- Resolve - belonging softens opposition

> **Recommendation:** Keep the pair. The open half is WHERE THE BAR SITS, and that is a measurement rather than a judgement: set it from a sweep producing both alliance-shaped and enmity-shaped worlds, never from a number picked to make one seed look right.

*Files: `docs/generation/CIVILISATION.md`*

### NR-819 — Twelve farm classes, four Colonisation branches — which class teaches which ground, and is there a fifth branch?
*question · raised 2026-09-10 · from Authoring COLONISATION_TREE.md: the brief assumed one branch per origin farm class and sized the tree for four; classify_farm_class in src/world/colonisation.cpp yields twelve (boreal, volcanic, floodplain, montane, coastal, woodland, arid, valley, highland, grassland, stone, steppe) and COLONISATION.md names none.*

The tree folds them into four ground families — Wet Ground (floodplain, valley), High Ground (highland, montane, volcanic), Open Ground (grassland, steppe), The Shore (coastal) — and leaves woodland, boreal, arid and stone belonging to no branch. A people coined on unowned ground climbs the spire on contact alone. Related: the gate vocabulary has no highland atom, so High Ground's majors carry no gate.

**Why it matters.** Which ground teaches what is the whole content of a carried tree; a class that teaches nothing makes its peoples uniformly poor, and a fifth branch re-shapes the tree (rule 4 breadth, milestone requires_any sets). Cheap now, expensive after BL-882.

- A - fold the four unowned classes into the nearest family (woodland/boreal -> High Ground, arid/stone -> Open Ground) and add a table to the doc
- B - a fifth branch, Forest, for woodland and boreal; arid and stone stay unowned as hostile country
- C - unowned classes teach nothing by design: peoples from hostile ground are late by construction

> **Recommendation:** A, with a highland gate atom raised as its own small item only if the sweep shows High Ground nodes held by peoples who never stood on high ground. B is the deeper option and costs ~7 nodes; the cap has room.

*Files: `docs/generation/trees/COLONISATION_TREE.md`, `docs/generation/trees/colonisation_tree.json`, `src/world/colonisation.cpp`*

### NR-820 — DECISION TAKEN: the grammar gained requires_fork and requires_any beside the AND requirement
*decision taken on your behalf · raised 2026-09-10 · from Authoring the Empire and Colonisation trees against the agreed grammar. The Sworn Province wants the granary fork TAKEN either way (the ladder's house rule for a fork under a vertex); the Colonisation milestones cannot demand two named branches of a people coined on one ground.*

TREES.md § Milestones now carries three forms: requires (all held), requires_fork (either side of a fork pair), requires_any (any count of a set). tree_lint enforces all three at the milestone's own ring and counts guaranteed breadth for rule 4 in the worst case.

**Why it matters.** Rule 2 says meaning is AND. Two softer forms are a widening of the grammar you agreed, taken so the trees could be finished; they are recorded so they can be overturned rather than become precedent.

- A - keep both (ADOPTED)
- B - keep requires_fork only; the Colonisation milestones name the spire major and ONE branch major each, accepting that peoples off that ground climb late
- C - keep neither; the Colonisation tree becomes a documented exception

> **Recommendation:** A. Both are lintable and both are used by exactly the case that needed them.

*Files: `docs/generation/trees/TREES.md`, `tools/session/tree_lint.js`, `docs/generation/trees/colonisation_tree.json`, `docs/generation/trees/empire_tree.json`*

### NR-821 — DECISION TAKEN: the old ladder is kept as a calibration reference with a superseded banner, not deleted
*decision taken on your behalf · raised 2026-09-10 · from Ben, 2026-09-10: "write the docs and JSON to replace what we had pre-existing." docs/research/ANCIENT_TECH_LADDER.md (52K) and ancient_tech_ladder.json are cited by ten docs, by history_sim.hpp's own comments, by ladder_lint.js, and transcribed into scripts/tech_tree.lua's Era -1 section.*

The three trees under docs/generation/trees/ are now the authority for the pre-game layer; the research doc carries a banner saying so and stays as the Earth-calibration reference (§ What "not every nation is equal" means, § Acquisition model, the band tables). ladder_lint.js still runs; the tech_tree.lua transcription retires under BL-885.

**Why it matters.** Deleting would break header_graph on ten docs and orphan the calibration facts the sweep is tuned against; keeping leaves a superseded 52K research doc in the corpus. Your call which cost to pay.

- A - keep with banner (ADOPTED)
- B - delete doc + JSON + ladder_lint.js and re-point the citations at TREES.md
- C - move both under docs/research/archive/ and re-point

> **Recommendation:** A until BL-885 lands, then C — the transcription is the last thing that reads the JSON.

*Files: `docs/research/ANCIENT_TECH_LADDER.md`, `docs/research/ancient_tech_ladder.json`, `tools/session/ladder_lint.js`*

### NR-822 — DECISION TAKEN: one milestone per ring, so the trees carry 3 / 4 / 4 milestones rather than the 2 / 3 / 4 first stated
*decision taken on your behalf · raised 2026-09-10 · from The sizing table in the 2026-09-10 assessment said 2, 3 and 4 milestones; the spire rule (one major and one milestone per ring, chained) makes the count equal the ring count.*

Colonisation 3 rings / 3 milestones, Empire 4 / 4, Industry 4 / 4. The ring-1 milestone requires two ring-1 branch majors, so a polity cannot leave ring 1 by the spire alone.

**Why it matters.** A rule-driven count is checkable and the stated count was not; but it means one more gate per tree than you agreed to, which slows the climb by one milestone cost.

- A - one per ring (ADOPTED)
- B - no ring-1 milestone: the root opens ring 2 directly

> **Recommendation:** A.

*Files: `docs/generation/trees/TREES.md`, `tools/session/tree_lint.js`*

### NR-825 — BL-855's degree cap (10) reduces reach-rebuild growth but doesn't cleanly flatten it at 4,000+ regions
*question · raised 2026-09-10 · from BL-855 (adjacency caps region count), delivered 2026-09-10 per your NR-809 call to pursue the fix now.*

max_neighbour_degree=10 was measured (mean degree ~7-9 early game, climbing past 20 by 4,000yr uncapped) and chosen to sit at the top of the early-game band. history_span_cost post-fix shows ms/rebuild growth reduced (roughly region^1.5 instead of region^2) but not fully flat at the larger region counts this run reached (up to ~4,400) -- e.g. seed 0: 0.084/0.164/0.235/0.156 ms/rebuild at 1748/2931/3601/3718 regions, with a drop at the final checkpoint rather than a plateau. Reach-rebuild count is also driven by battle frequency independent of the fix, which confounds a clean read.

**Why it matters.** The mechanism (bounding E) is real and correctly deterministic. Whether 10 is the right cap, or whether flatness at this scale needs a lower cap or a different structure (nearest-k rather than radius+cap), is a magnitude question this harness reports but does not resolve on its own.

- A - accept as delivered: the mechanism is sound, the item's own risk section already flagged the scale question as unsettled
- B - lower the cap further (e.g. 6-8) and re-measure
- C - investigate whether reach-rebuild frequency itself (not just degree) needs bounding, since battle-heavy seeds dominate wall time regardless of degree

> **Recommendation:** A, unless the play build feels slow at the 4,000-year end of a run once BL-855 is exercised live.

*Files: `src/world/history_sim.hpp`, `src/world/history_sim.cpp`, `tools/verify/history_span_cost.cpp`*

### NR-826 — BL-896: three of the four open secession calls were taken at build time
*decision taken on your behalf · raised 2026-09-11 · from BL-896 (collapse is network failure). The item named four calls as OPEN FOR THE BUILD; three had to be settled to write the code, and the fourth answered itself.*

(1) WHICH FLOOR: a new `secession_supply_floor_q` rather than reusing `sustainable_garrison_floor_q`. That floor is ZERO on generation's round, so reusing it would have made secession dead code; the new floor is sited at 60, just above `sustainable_settlement_floor_q` (40), so ground too far out for its towns to grow is the same ground too far out to be ruled. (2) THE UNIT IS A CONTIGUOUS BLOCK, min 2 regions: one region leaving alone shatters a realm into specks, a cut-off block leaving together splits it, and a split is what produces nations of unequal strength. (3) WHAT THE SUCCESSOR INHERITS: the ground's OWN culture (the new seat's plurality, not the parent's), the parent's capacity and progress ladders and its cohesion, and nothing of the parent's seat stock -- that stock sits in a capital it no longer holds. A grudge is raised parent->successor as `ground_taken`, rather than adding a fifth `grudge_kind`. (4) HINTERLAND: answered by (2) -- the block IS the hinterland, since seat_region is rewritten across the whole block.

**Why it matters.** Each is a design call the item explicitly left to Ben, and each shapes what the dark age hands forward. The block-size floor in particular is the dial between a shattered map and a split one, and the culture choice decides whether a successor is a fragment of the empire or a people reasserting itself.

> **Recommendation:** Keep all four. The culture call is the one worth a second look: taking the seat's plurality means a successor can be culturally alien to its parent, which is what makes secession carry the pantheon record forward rather than clone the empire.

*Files: `src/world/history_sim.cpp`, `src/world/history_sim.hpp`, `src/world/era_minus_one.cpp`, `docs/generation/CIVILISATION.md`*

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

### NR-830 — BL-838: fear of being next fires, but hegemony does not move at all -- the item's third done-when is unmet
*question · raised 2026-09-11 · from BL-838 (fear of being next), delivered 2026-09-11 under your dated grant. Measured across 16 seeds at --epoch 0 against a w_fear_q=0 control.*

The mechanism works and the scope demonstrably held: six hand-built checks show a LARGER PEACEFUL polity attracting zero fear while a smaller aggressive one attracts it, and clearing the ledger with nothing else changed drops both to zero -- which a size coefficient could not do. In the sweep the lean fires in 6 of 16 worlds, worst world 4,323 leans against 4 realms, conquests median 526 -> 558. BUT the two headline figures are digit-identical on both arms: HEGEMONY RATE 0/16 at 50% share, largest share median 10% (range 6-18%). The item's third 'done when' -- hegemony frequency falls across a seed sweep -- is NOT met.

**Why it matters.** The magnitude (w_fear_q = 400) is a placeholder and was deliberately not tuned toward the target, because tuning it until hegemony moved would be fitting a figure to a done-when. Two readings are open and they lead opposite ways. (1) The weight is too small and the lever is real. (2) Fear of annihilation is simply NOT a brake on a riser at this span -- and note the phase runs 400 of its designed 1,600 years, so there may not be a riser large enough to fear yet. There is also a third possibility worth stating: hegemony is ALREADY 0/16 without this lever, so there may be no headroom for it to improve anything, and the done-when may have been written against a world that no longer exists.

- Raise w_fear_q and re-measure -- but only against a stated target, and knowing hegemony has no headroom at 0/16.
- Accept it. The lever is admissible, legible and live; hegemony was already controlled by reach and secession, and this adds a second cause rather than a needed brake.
- Re-write the done-when. It asks for a fall in something already at its floor, which no lever can deliver.

> **Recommendation:** Option 3 then 2. The done-when predates BL-837, BL-894 and BL-896, all of which landed since and between them took hegemony to 0/16 -- so the bar it sets cannot be cleared by anything, and passing it is not evidence of quality. Judge this lever on whether a coalition is legible and caused, which it measurably is.

*Files: `src/world/history_sim.cpp`, `docs/ai/AI_OPPONENT.md`, `docs/generation/CIVILISATION.md`*

### NR-832 — BL-839 bumped the save format 11 -> 12, and reused `lean::any` for an axis where it has no meaning
*decision taken on your behalf · raised 2026-09-11 · from BL-839 (turbulence lean), delivered 2026-09-11. Both calls taken by the building agent so the work could finish.*

(1) SAVE FORMAT 11 -> 12. Unavoidable: the lean has to reach `make_hard_coded_world`, the same call the wizard's round-4 worker makes. The roundtrip check is pinned to a LITERAL rather than to the original value, so a writer and reader that both omitted the field cannot round-trip clean and call it a pass. (2) `lean::any` READS AS ORDINARY. The existing `lean` enum was reused rather than minting a new one, to avoid a second serialiser and bound. The cost is that `any` is representable on this axis but meaningless; the field defaults to `mid`, the departure is documented at both sites, and the wizard row offers only three options so `any` is never reachable from the UI.

**Why it matters.** A save-format bump is the kind of change that should be seen rather than discovered, and this one landed inside a batch. The `lean::any` reuse is the smaller call but the more likely to bite later: a value that is representable and meaningless is exactly what a future reader mishandles, and it is only unreachable because one UI row happens not to offer it.

- Accept both. The bump was forced and the enum reuse is documented and UI-unreachable.
- Keep the bump, but mint a proper three-value type for this axis so `any` cannot be represented at all.

> **Recommendation:** Option 2 when something next touches that serialiser. Not worth a bump of its own, but worth not leaving a meaningless-but-representable value in a saved enum indefinitely.

*Files: `src/world/planetology.hpp`, `src/world/era_minus_one.cpp`, `src/world/world_save.cpp`*

### NR-833 — BL-903 (communication rung), built and measured, refuses ZERO campaigns -- the settled definition is inert against the only candidate list it can gate
*novel-work · raised 2026-09-11 · from BL-903 (COMMUNICATION_RUNG), attempted this session. Not committed -- the code was built, measured, then reverted.*

Implemented the settled rule literally: a region is communication-reachable iff (A) it borders held ground, or (B) it borders a foreign polity's ground that itself borders held ground, gated on that intermediate polity carrying no live grudge from us. Checked FIRST in the Campaign verb's target loop, before the BL-778 water gate and the BL-837 reach gate, with its own counter. `history_sweep --epoch 0`, 16 seeds: `REFUSED comms gate   median 0` on every seed, while `REFUSED traversal (water)  median 967` and `REFUSED reach gate  median 0` (the last matching BL-905's independent finding). The reason is structural, not a tuning miss: Campaign's target loop already only ever enumerates `neighbours[hi]` for `hi` in `held` -- i.e. it is BY CONSTRUCTION already exactly condition (A)'s set, unconditionally. Condition (A) as settled grants that whole set with no test attached, so nothing the gate checks can ever be false for a candidate Campaign generates. Condition (B) only ever ADDS ground beyond that set, and nothing in the file (Campaign, Settle, Invest, Consolidate, Build-a-work) ever offers a two-hop target as a candidate to gate in the first place. The mechanism is mathematically a no-op given today's candidate-generation code, not a tuning problem and not a duplicate of reach (it asks a genuinely different question) -- it simply has nothing to bind on.

**Why it matters.** The item's own DONE WHEN requires the bound to show up in the sweep, distinguishable from reach. It cannot, as specified, against this file's current candidate lists. Two ways forward exist and only Ben can pick: (1) widen the settled definition so (A) is conditional rather than an unconditional grant -- e.g. requiring good relations even for a direct border -- which changes what 'immediate border' means and was explicitly ruled settled this session; or (2) widen Campaign's candidate GENERATION to actually offer two-hop targets (through a friendly neighbour) as real campaigns, which is what would give condition (B) something to enable and condition (A) something to be compared against -- a materially bigger change than 'add a gate check', touching the execute path (`gather_army`, staging-hub distance, `dry_contact`) as well as the scorer, and arguably a different-sized item than a difficulty-3 single-file gate.

- Leave BL-903 undelivered and re-scope it as the candidate-widening item (2) above -- bigger, touches execute as well as score.
- Re-open the settled definition and make (A) conditional on relations too, so a hostile direct neighbour's ground stops being a free candidate -- smaller, but reverses a call made this session and changes ordinary border-war behaviour (a polity could no longer campaign against a disliked neighbour it has never fought before).
- Decide BL-903 is answered by its own null result -- ground beyond one hop was already, implicitly, off the table before this item; document that in CIVILISATION.md's arc description instead of building a gate for it.

> **Recommendation:** Option 3 is cheapest and matches what BL-905 is independently finding about reach: BOTH of this phase's post-water gates may be doing nothing because the water gate (BL-778, refusing ~967 of every world's contacts) is consuming almost every candidate before either ever gets a chance to matter. Read BL-905's outcome before deciding between 1 and 2 -- if the water gate turns out to be over-tight, loosening it is the one change that would make ALL THREE gates (water, reach, communication) start doing distinguishable work at once, instead of tuning any one of them in isolation.

*Files: `src/world/history_sim.cpp`, `tools/verify/history_sweep.cpp`*

### NR-840 — NOVEL: culture_kinship_years read every dated culture as undated for the life of BL-870
*novel-work · raised 2026-09-11 · from BL-918 (culture diversity/kin split), caught by the split census's "adjacent pairs 0" line.*

culture_kinship_years (src/world/creeds.cpp) tested `year < 0` to detect "undated", but every culture in the migration is coined BCE -- cradles at colonisation_start_year (-2400), daughters between it and the epoch -- so the test read every dated tree as undated and the function returned -1 for every pair on every world since BL-870 introduced it. culture_opposition_q, which reads the kinship figure to discount opposition between kin, therefore weighed every neighbour as a stranger: the kin discount has never fired in a real run. Fixed in the same commit as BL-918 (the sentinel is now INT64_MIN, not "negative") because the split census could not proceed without it, not because BL-918 asked for it -- flagged here as the novel work it is rather than folded silently into that item's scope.

**Why it matters.** This is a correctness fix to a mechanism CIVILISATION.md already describes (opposition discounted by kinship) that has silently never worked; every prior reading of creed/opposition behaviour (including any earlier sprint's tuning) was taken with the discount permanently off. No design change is proposed -- the fix makes the existing design true for the first time.

> **Recommendation:** No action needed beyond this record; the fix is already live and covered by history_sim_harness R9a/R9b/R9c/R9d (opposition/kinship symmetry).

*Files: `src/world/creeds.cpp`, `docs/generation/CIVILISATION.md`*

### NR-841 — DECISION TAKEN: the wizard's Culture/Empires preview now draws rivers (was terrain only)
*decision taken on your behalf · raised 2026-09-11 · from BL-915 (terrain underlay on the lapse).*

generate_home_surface_preview (src/world/hard_coded_world.cpp), which builds the tile surface the wizard's Culture and Empires rounds draw their political fill over, never ran the river pass -- only make_hard_coded_world (the actual campaign world) did. BL-915 draws river strokes under the political fill on both lapse rounds, and the only surface those rounds hold is the preview's, so generate_rivers now runs there too, seeded `params.seed ^ 0x52490001u` -- the SAME formula the campaign uses, so the rivers a frontier stalls at in the wizard are the rivers the campaign will actually have.

**Why it matters.** This is a world-generation change (the preview surface now differs from what it drew before, in a deterministic, seed-derived way) made inside a UI item's scope because the UI requirement could not be met without it. No gameplay numbers move -- the preview was never gated on anything -- but it is a generation-layer change and is recorded as a decision taken on Ben's behalf rather than folded silently into BL-915's UI story.

> **Recommendation:** No action needed; confirmed live in the round-3/round-4 click-through (rivers visible on both maps).

*Files: `src/world/hard_coded_world.cpp`, `docs/generation/TILE_GENERATION.md`*

### NR-842 — DECISION TAKEN: round 3 under --verify now replays the migration record, not the Empires sim
*decision taken on your behalf · raised 2026-09-11 · from BL-919 (lineage palette).*

Before BL-919, `lapse_from_report` built BOTH wizard lapse rounds (Culture and Empires) from the same finished generation_report -- so under --verify (the adopt path, which reuses generation's own finished run instead of re-simulating for the capture) round 3 (Culture, whose owners are meant to be CULTURES) silently replayed the Empires sim's polities instead, because a migration-only record was never threaded through. BL-919 needed the real migration record to build the lineage tree (cradles + daughters, parent-indexed), so round 3 under --verify now calls build_migration_timelapse off settlement_state directly (battles/conquests zeroed, foundings set to region count, since a migration is a diffusion with no battles in it) and only round 4 (Empires) still folds the finished report's own counters.

**Why it matters.** This changes what the --verify capture PROVES for round 3: it now checks the actual Culture-round substrate (migration/settlement) rather than a stand-in. Any saved golden or capture for round 3 taken before this commit was checking the wrong record; no goldens are known to exist for this surface (verifier-visual has none scoped here).

> **Recommendation:** No action needed; confirmed live in the round-3 click-through (lineage-palette hue families visible, ticker text is migration-flavoured, not battle-flavoured).

*Files: `src/ui/startup_screens.cpp`, `src/world/hard_coded_world.cpp`, `docs/ui/STARTUP.md`*

### NR-860 — OBSERVATION: the migration overruns the 400 BCE boundary on 10% of a 60-seed sweep, up to 343 years over
*observation · raised 2026-09-13 · from BL-947 (CULTURE_ROUND_COASTS_TO_400BCE), pre-fix measurement sweep, generation-dev sub-agent.*

CIVILISATION.md names the case where settlement_state::migration_end_year (the diffusion's own derived terminating year, 'every habitable landmass carries some culture') runs later than the Empires round's own opening year (-400 at the wizard's defaults) a defect in the migration, not in this boundary, and asks that the frequency be measured rather than assumed. A 60-seed sweep (tools/verify/culture_round_coast_measure.cpp, seeds 0xC001D00D + i*0x9E3779B9) found 6/60 (10%) overran, from 46 years over (seed 954185457, end -108) to 343 years over (seed 3665124156, end -57); the other 54/60 ended between -1652 and -411, comfortably inside the span. BL-947's fix keeps the true (later) end year and shows it honestly on an overrun rather than clamping to -400, per the doc's own instruction, so the wizard's Culture round is never wrong on these seeds -- it is just longer than 2,000 years, cutting into the Empires round's own 1,600-year budget for that world.

**Why it matters.** 10% is not the '~1-in-10 worlds without an arena' kind of rare tail CIVILISATION.md is used to living with elsewhere in this pass (BL-276's own 90%-likely acceptance gate) -- it is the same order of magnitude, but nothing today rerolls or flags a slow-filling seed the way the arena gate does for a small homeworld. Whether that is fine (a diffusion is allowed to sometimes run long, and the doc already says so) or whether it wants its own gate/reroll is a design call BL-947 was not asked to make.

- Leave it: an overrun is rare enough (1 in 10, and none of the 60 seeds moved the boundary by more than a fifth of the Empires round's own span) that showing it honestly is sufficient.
- Add a BL-276-style reject-and-reroll on the migration's own seed when it fails to finish inside the 2,000-year budget, so an overrun becomes a hard tail rather than a routine one.
- Widen the Culture round's own budget (currently 2400 BCE -> 400 BCE) if 10% overrunning suggests 2,000 years is undersized for the map scale in general.

> **Recommendation:** Leave it for now -- BL-947's fix already makes the overrun visible and honest rather than silently wrong, which was the actual bug Ben saw live; a reroll or budget change is tuning work against a sweep this item's 60 seeds is too small to calibrate from.

*Files: `src/world/colonisation.hpp`, `src/world/settlement.cpp`, `docs/generation/CIVILISATION.md`*

### NR-861 — OBSERVATION: treaties almost never break -- 4 breaks against 5,161 formed across 16 seeds
*observation · raised 2026-09-14 · from exploration_sweep, 16 seeds (0-15), main session review of the Exploration round.*

Reading 4 printed treaties formed=5161, broken=4, non-aggression-blocked campaigns=3,853,341, with 930 standing at 1660 (median 20 years left). The break test in history_sim.cpp (~2592) re-scores a bound pair against the same treaty_value_q formation uses and breaks only below HALF the formation threshold, as hysteresis. At a 0.08% break rate the harness line "treaties both stand AND break" is technically true and substantively false.

**Why it matters.** EXPLORATION.md: "A treaty that cannot be broken is a rule, not a promise, and a phase whose actors never defect produces a flat map." 3.85M blocked campaigns against 4 defections suggests the non-aggression block is the main reason neighbour war fell (reading 2 fell 8x), which is calming rather than displacement -- the failure mode the doc names. Treaties also expire on term, so standing treaties are not permanent; whether expiry-and-reform is enough churn, or defection itself should be commoner, is a judgement.

- Leave it: expiry already churns treaties, and defection being rare is historically plausible.
- Treat it as a measurement first: add a reading of expiries vs renewals vs defections so the churn is visible before any tuning.
- Tune toward more defection by letting a want or an opportunity (a treatied neighbour whose ground holds a wanted good) lower treaty value -- which BL-953 (wants point outward) would make possible without a new term.

> **Recommendation:** Option 2 now, inside BL-952 (sweep builds each world once), and revisit after BL-953/954 land, since outward wants and trade value both change treaty value.

*Files: `src/world/history_sim.cpp`, `tools/verify/exploration_sweep.cpp`, `docs/generation/EXPLORATION.md`*

### NR-862 — OBSERVATION: displacement regressed 0.33 -> 0.08 (16 seeds) at the schism verb, and NR-855's 0.88 was a 3-seed artefact
*observation · raised 2026-09-14 · from exploration_sweep at HEAD 643ebf79 vs NR-855 (sprint 40 wave 4, ec335d2c).*

BISECTED 2026-09-14 with exploration_sweep at four commits. Seeds 0-2: wave-4 close ec335d2c and its successor 4f07f5ea (pre-schism) both read 0.0066 / 1.1054 / 0.8775 (median 0.88, exactly NR-855). 4605a90d (schism merged) and HEAD 643ebf79 both read 0.0116 / 1.4241 / 0.0392 (median 0.04). The only src/world commit between 4f07f5ea and 4605a90d is 3d58001d (BL-944, schism verb). It changes the EMPIRES round (seed 0: 6000 -> 6479 battles before 1200), so the Exploration span opens on a different world; seed 2 loses nearly all its frontier war (18.70 -> 0.43 skirmishes/century). WIDER SPREAD: the wave-4 tree over 16 seeds has a median of 0.33, not 0.88 -- the 3-seed median rested on seed 2 alone. HEAD over 16 seeds: 0.08.

**Why it matters.** BL-950 (displacement clears the bar) was designed to push from 0.88 to above 1.0. The honest starting point was 0.33 before the schism and 0.08 after. And the regression was silent: the sweep reports rather than gates, and nothing else reads displacement, so an upstream Empires verb cut the phase's headline reading by three quarters without a red line anywhere.

- Treat BL-944 as legitimate (a schism is a real force) and let BL-950 tune against the post-schism world, from the integrated sprint-41 16-seed baseline.
- Investigate why the schism collapses frontier war on some seeds (fewer, smaller realms at 1200 with no sea reach?) before tuning.
- Make exploration_sweep gate on a displacement floor so the next upstream change goes red.

> **Recommendation:** Option 1, with option 2 as the first step of BL-950's tuning pass (cheap once the 16-seed run exists). Option 3 is a separate call: the harness deliberately reports.

*Files: `src/world/history_sim.cpp`, `tools/verify/exploration_sweep.cpp`*

### NR-863 — DECISION TAKEN: three calls made while writing and integrating the Exploration trade design
*decision · raised 2026-09-14 · from Exploration trade batch, design-form follow-through (BL-953, BL-954).*

Two things the form did not ask were written into the authority doc. (1) Outward wants lean campaigns and subjection ONLY, not settlement -- the form carried this question but it came back unanswered, so the recommended option was taken. (2) A trade flow earns BOTH ends (seller and buyer capitals), not the seller alone -- the form asked whether flow income replaces the flat market income (yes) but not who earns it. (3) ADDED at integration, 2026-09-14, after the cold review: a seller's HOLDING is shared across its buyers, mirroring the buyer's want being shared across sellers. Without it a one-province farm city-state bound to five buyers exported five holdings' worth and earned five times over. The holding stays a per-mille SHARE of held ground, so a one-province holder and a hundred-province holder with the same share export the same volume -- that scale question is untouched.

**Why it matters.** (2) decides whether being a buyer is ever profitable. Seller-only makes wealth flow to holders of scarce goods (a sharper resource asymmetry); both-ends makes any market on a busy line rich (a trading-hub asymmetry, and it keeps treasuries from collapsing for polities that hold little but sit on lines). Replacing the flat income with seller-only income would bankrupt most polities that hold no wanted good.

- Keep both calls as written.
- Seller-only income: a trade line enriches the holder, not the hub.
- Let wants also lean settlement across water.
- Make holding an absolute quantity (held regions dominant in the good, not a share), so a large realm out-exports a small one.

> **Recommendation:** Keep both as written; revisit (2) from reading 8 (treasury spread) once BL-954 lands.

*Files: `docs/generation/EXPLORATION.md`, `src/world/history_sim.cpp`*

### NR-864 — OBSERVATION: reading 3 classifies a creed by comparing a PRODUCT lean to an AVERAGE lean, which structurally makes consolidators rare
*observation · raised 2026-09-14 · from Sprint 41 T1 (BL-951, fair strength metric) -- main-session review of the agent diff and report.*

BL-951 moved reading 3 to a treasury ranking; on seeds 0-2 all 18 top-3 entries (by treasury AND by region count) still classify expansionist, with consolidator lean 104-190 against expansion lean 610-860. The classifier (exploration_sweep.cpp, sprint-40 origin, kept by BL-951) calls a culture a consolidator when consolidator_lean_q > expansion_lean_q. But consolidator_lean_q = dominion x (1000 - sea_legs) / 1000 (a product, at most the smaller factor) while expansion_lean_q = (sea_legs + zeal) / 2 (a mean, at least the smaller input). For a middling culture (sea 300, zeal 500, dominion 500) the leans are 350 vs 400: expansionist, though nothing about it is seafaring. The two leans are on incommensurable scales -- BL-318 incommensurability again -- so comparing them answers a question about arithmetic, not about creeds. DECISION TAKEN 2026-09-14 for BL-955 (spend is scored), which could not wait: the spend allocation uses each lean only as a PER-MILLE RANK among the round's living polities, never the raw value, so the two leans are compared on one scale without changing the creed arithmetic. The raw-lean comparison in exploration_sweep reading 3 is unchanged pending this call.

**Why it matters.** EXPLORATION.md sec Two ways to be strong says: if the creed axes do not separate the strategies, the fix is upstream in the Empire phase. That conclusion should not be drawn from a comparison that could not have come out the other way. The sim itself uses the two leans independently as weights in choose_exploration_node, so the defect may be in the reading only -- or the same scale mismatch may also bias the node choice and BL-955 (spend is scored), which will read both leans.

- Classify by rank within the world: a culture is a consolidator if its consolidator lean is in the top third of cultures by that lean, and likewise for expansion (can be both or neither).
- Put both leans on one form (both products, or both means) in src/ so they are commensurable everywhere they are read, including BL-955.
- Leave the classifier and accept the reading as upstream evidence.

> **Recommendation:** Option 2 for BL-955 before it reads the leans, plus option 1 for the reading. Measure the lean distribution over ALL living polities on the 16-seed integrated sweep first (T6), so the call is made on the population, not the top 3.

*Files: `tools/verify/exploration_sweep.cpp`, `src/world/history_sim.cpp`, `docs/generation/EXPLORATION.md`*

### NR-866 — CALL: the navy and army saturation in the spend scorer is a brake inside the scorer, where EXPLORATION.md wants a cost in the world
*question · raised 2026-09-14 · from Sprint 41 wave 2 (BL-955, spend is scored), cold review finding 4.*

BL-955's allocation scores a navy step 0 once the fleet exceeds 400 per held region and an army step 0 once paid heads exceed 300 per held region. Without the army cap, polities bought an army step every round (12,371 steps vs 800 ports on 3 seeds), starving fleets and sea trade. But EXPLORATION.md sec Force persists now says upkeep is 'a cost in the world rather than a handicap in the scorer', and the standing rule prefers systemic forces over agent terms. Separately, paid heads cost no manpower or population, and now that they persist, a 44-region realm at the cap holds 13,200 paid heads at no population cost, which saturates neighbours' Alarm against visible_capability_reference 5000 -- unmeasured.

**Why it matters.** A cap in the scorer is the polity knowing it has enough. A per-head running cost is the world making more expensive to keep. They can produce similar spreads, but only the second is visible on the map as a cause (a treasury drained by its garrison), and only the second makes an over-built polity poorer every round, which is the pressure the doc names.

- Keep the scorer saturation (diminishing value of more of what you hold is a reading of the world, not a handicap).
- Replace it with an in-world bill: paid heads and hulls cost treasury per unit per year; unaffordable upkeep decays them. The scorer then sees a falling purse, not a cap.
- Both: the bill as the force, a soft saturation only as tie-breaking.

> **Recommendation:** Option 2: it is what the doc already says, and it fixes the free-manpower problem at the same stroke if paid heads also draw from manpower. It moves the world again, so it would be its own item after this sprint rather than folded into the tuning wave.

*Files: `src/world/history_sim.cpp`, `src/world/settlement.hpp`, `docs/generation/EXPLORATION.md`*

### NR-868 — CALL: COLLAPSE.md's six strategies, seven culminations and pairing matrix — still design, or retired?
*decision · raised 2026-09-15 · from BL-980 (generation doc-versus-code reconciliation), item 4.*

docs/lore/COLLAPSE.md authors an Era -1 collapse metagame: a strain accumulator per major polity, six strategies a polity plays against it, seven culminating events, and a matrix pairing them. No source file cites the doc (src/world/CLAUDE.md lists it as an owner and nothing else does) and none of the roster's names or mechanics appear in src/. The collapse the history sim actually models is written in CIVILISATION.md: secession as network failure (BL-896, collapse is network failure — sec The network is the estate, and it crosses; sec What the dark age must leave) and the schism verb in CREEDS.md (BL-944, the schism verb). A superseded banner now sits at the top of COLLAPSE.md pointing at both; no content was deleted.

**Why it matters.** An authority doc that no code reads and no other doc defers to is either design still owed or a retired exploration wearing an authority header. Left as it is, the next reader takes the strain/strategy/culmination model as the design and builds against a roster the sim does not have; retiring it without a ruling loses a written exploration Ben asked for (2026-08-20). Either answer is cheap to record; only Ben can give it.

- RETIRE: move COLLAPSE.md to docs/research/ (or archive) as a mechanism reference; CIVILISATION.md and CREEDS.md are the collapse authority. Remove the banner with the move.
- KEEP AS DESIGN: the roster is owed to a later pass (a strategy layer over the network-failure model). File a backlog item that owns it, cite it from the banner, and leave the doc as authority.
- FOLD: keep only the parts CIVILISATION.md does not already say (the ideological-axis narration, the affordability notes) as a section of CIVILISATION.md; retire the rest.

> **Recommendation:** Option 1. The built model (network failure + schism) is a consequence of upstream state, which is the shape Ben chose for generation stages; a strategy roster is an agent-side term the same rulings have refused elsewhere. Keep the prose as research.

*Files: `docs/lore/COLLAPSE.md`, `docs/generation/CIVILISATION.md`, `docs/lore/CREEDS.md`*

### NR-869 — CALL: does the default epoch move from 0 CE to 1960 before Digitisation?
*decision · raised 2026-09-15 · from BL-980 (generation doc-versus-code reconciliation), item 8.*

GENERATION_STRATEGY.md said the campaign epoch is 1960 on the arc generation runs. The code default is world_params::epoch_year = 0 (hard_coded_world.hpp; Ben, 2026-08-12, NR-177); 1960 is the opt-in --epoch 1960 flag (main.cpp), and the industrial span and the ruptures pass run only on a 1700+ epoch. The doc now says the default honestly (0 CE, 1960 opt-in) and the pass map is rewritten from make_hard_coded_world's actual order. What the doc cannot decide is whether the default itself should move.

**Why it matters.** Every default-epoch world today opens at 0 CE with the Exploration span having run to 1660 and the clock rebased. DIGITISATION.md is the placeholder for 1660 -> 1960; if the default epoch is to be 1960, the placeholder becomes the next phase to build and the Exploration handoff is its input. If the default stays 0 CE, the 1960 arc is a sandbox and Digitisation has no live consumer, which changes what sprint 42's seam work is for.

- Default stays 0 CE until Digitisation is built; 1960 remains the opt-in arc. Docs say so (done in BL-980).
- Default moves to 1960 now, with Digitisation absent: the campaign opens after a 300-year gap the sim does not simulate. Cheap; honest only if the gap is labelled on the wizard.
- Default moves to 1960 when Digitisation lands, as that item's DONE WHEN — file it against DIGITISATION.md.

> **Recommendation:** Option 3. It keeps the doc and code agreeing at every point in between and makes the move a consequence of a built phase rather than a flag flip.

*Files: `docs/generation/GENERATION_STRATEGY.md`, `docs/generation/DIGITISATION.md`, `src/world/hard_coded_world.hpp`, `src/main.cpp`*

### NR-870 — CALL: the per-tile derivation breadcrumb — owed a surface, or dropped from the design?
*decision · raised 2026-09-15 · from BL-980 (generation doc-versus-code reconciliation), item 9.*

GENERATION_LEDGER.md sec Per-tile derivation breadcrumb and sec Surfacing design a five-step per-tile "why" (height, band and moisture, substrate and cover, landform, deposits) built once and shared by the ledger, the hover card and the Selection element. Ben ruled DELETE on its builder, draw_tile_derivation, on 2026-08-30 (archived NR entry) when the ledger lost its tab strip; the function is gone from src/. generation_ledger.hpp and .cpp still pointed at it as surviving — corrected in BL-980 — and the doc section now carries a banner saying it is design with no surface, with the call recorded here.

**Why it matters.** The ledger exists to answer "why did this tile generate as it did" (the doc's own opening line) and today only the per-body half of that answer has a surface. Keeping a designed-but-unsurfaced section in an authority doc is exactly the pattern NR-717 named; deleting it loses the best written explanation of the tile pipeline the ledger family has. The 2026-08-30 ruling deleted the code; it did not say whether the design goes with it.

- DROP: remove sec Per-tile derivation breadcrumb and the per-tile half of sec Surfacing from GENERATION_LEDGER.md; the tile-grain "why" is not a question the game answers.
- OWE: file a backlog item for a per-tile derivation frame in the Selection element (a tile is already a Selection subject), cite it from the section, keep the design.
- OWE, as a lens only: the field overlay lenses (BL-304) show the spatial structure of each pass; the per-tile numbers come from the Tile Ledger's existing rows and no breadcrumb is built.

> **Recommendation:** Option 2 if the tile-grain question matters for tuning generation (it did when the ledger was designed); option 1 otherwise. Not option 3: the lens answers a different question than the breadcrumb.

*Files: `docs/generation/GENERATION_LEDGER.md`, `src/ui/generation_ledger.hpp`, `src/ui/generation_ledger.cpp`, `docs/ui/SELECTION.md`*

### NR-871 — CALL: at 1200 CE, do EVERY seat's stores flow to the capital (doc) or only the capital seat's own stock (code)?
*decision · raised 2026-09-15 · from BL-980 (generation doc-versus-code reconciliation), item 12.*

EXPLORATION.md sec Capital arrives says consolidation is the phase's opening act: "At 1200 CE every seat's stores flow to the capital, once" (Ben, 2026-09-11: consolidate all stores of material into the polity level). history_sim.cpp (the consolidation block in the exploration round, at the span's start year) does `seat.treasury += seat.material_stock; seat.material_stock = 0;` for the CAPITAL region only. A polity holding several seats keeps every non-capital seat's material_stock where it stands; those stores are never folded into the treasury. Nothing was edited: the two readings are both defensible and the difference is a design choice, not a typo.

**Why it matters.** The doc reading makes the opening treasury the whole realm's hoard, so a wide empire opens the exploration age rich and a city state opens poor — the asymmetry EXPLORATION.md wants, and a thing the time-lapse can show. The code reading leaves most of a multi-seat realm's wealth on the map as material, where conquest of a seat still takes it (CIVILISATION.md sec Materials are spent) but the treasury and the spend scorer never see it. Which one is true changes every treasury figure the sprint 41 sweep reported, and the doc's claim that consolidation is visible.

- DOC IS RIGHT: fold every seat the polity holds (`is_seat` and owner == polity) into the capital's treasury at start_year; non-capital seats keep material_stock = 0 thereafter. Re-run exploration_sweep; treasury and Alarm readings move.
- CODE IS RIGHT: only the capital's own stock becomes capital; the other seats' stores stay material on the ground, taken by conquest, never by the purse. Reword EXPLORATION.md sec Capital arrives to say so.
- BOTH, staged: the capital's stock consolidates at 1200 and the other seats' stores flow in at a per-round rate over the network (reach-gated), so centralisation is visible as a process rather than an instant.

> **Recommendation:** Option 1 is what Ben said and what the doc records; option 2 is what the code does. Given the standing preference for the doc, option 1 — but it moves the world, so it is a sprint 42 item with a sweep, not a one-line fix.

*Files: `docs/generation/EXPLORATION.md`, `src/world/history_sim.cpp`, `src/world/settlement.hpp`*

### NR-872 — FINDING: six of the ten varying endowments move together with metallicity, so 55% of accepted homeworlds are poor in nothing
*decision · raised 2026-09-15 · from BL-962 (endowment spread asserted), building agent's measurement over 20,000 accepted homeworlds; main session review.*

planetology_sweep now prints the per-resource endowment distribution and asserts it has tails. The brief asked for "at least one resource per world in that resource's bottom decile"; the measurement says 55.5% of accepted homeworlds sit in NO resource's bottom decile. Cause: iron, copper, silica, rare earths, platinum-group metals and iron-nickel all scale with the same metallicity draw, so a metal-rich world is rich in all six at once. The assertion was pinned at the measured 44% rather than committed red. Also seen: every endemic good and most Tier-2/3 goods carry endowment 1.0 from S8's initial fill and are never overwritten (harmless while nothing reads them).

**Why it matters.** GENERATION_STRATEGY.md sec Asymmetry is the deliverable names endowment as the first instrument of supply asymmetry and asks for a wide spread with real tails. A metallicity axis that moves six resources together produces rich worlds and poor worlds, not worlds rich in one thing and starved of another, which is the shape trade needs. Whether that is the planetology's honest physics or a generator flattening the map is the call.

- Accept: metallicity is one real axis and the spread across worlds is enough; the within-world asymmetry comes from terrain and the endemic bands, not from endowment.
- Decouple one or two metal endowments from metallicity with a second seeded scalar (a generator change; digests move; re-bless once with the movement described).
- Leave until Digitisation shows whether the coupling is visible in the market; revisit with a chain-completeness reading in hand.

### NR-873 — READING: displacement at Alarm 525 is 1.58 pooled and 1.33 median, with six held seeds and two silent
*question · raised 2026-09-15 · from BL-971 (displacement volume-weighted), exploration_sweep over 16 seeds on the sprint-42 wave-0 tree; exploration_sweep.json is now checked in.*

Ben asked on NR-867 for a volume-weighted displacement reading. On the integrated wave-0 tree (which carries BL-981's schism fix): pooled (sum of frontier over sum of neighbour battles) 1.68; volume-weighted per-seed mean 5.76, dominated by seed 10; median 1.35. Per seed: 7 displaced by ratio, 2 displaced with zero neighbour wars, 6 HELD where neighbour war still leads, 1 SILENT under the 20-battle floor (seed 9). Conflict persists: Exploration 55.9 battles/century against Empires 404.9. Before the schism fix the same tree read pooled 1.58, weighted 3.88, median 1.33, six held, two silent (9, 14); nine of sixteen seed rows moved. exploration_sweep.json at the repo root is the integrated-tree artefact.

**Why it matters.** The sprint-41 authorisation rested on the median. The pooled reading is the robust one and also clears 1.0, so the conclusion survives the correction; but six of sixteen worlds still fight their neighbours more than their frontier, and the doc's claim is that conflict MOVES. Whether "clears on the pooled reading with six held seeds" is the phase's honest state or a tuning question is Ben's.

- Authorisation stands on the pooled reading; the held seeds are a legitimate spread.
- Treat the held count as the reading to move next, with a measured cause per held seed before any constant changes.
- Require both pooled and median above 1.0 AND held seeds below a stated count before Digitisation is designed.

### NR-874 — DECISION TAKEN: a schism moves seats, it does not raze them (BL-981)
*decision taken on your behalf · raised 2026-09-15 · from BL-981 (colonisation D1/D3 regression), building agent; main session review.*

The schism verb (BL-944) grouped a breakaway by residue culture and wrote is_seat = (r == seat) over every member, demoting any seat that walked out with the block; parent hinterland outside the block kept pointing at the demoted seat. The fix: every seat in the block stays a seat under the new realm (a realm born of a schism holds as many seats as walked out with it); a block member keeps its pointer if its seat came along, otherwise re-points at the new seat; parent ground whose seat left re-points at the parent capital. The alternative, demote and re-point everything at the new seat and parent capital, also satisfies D1/D3 but razes a settlement over a faith fracture and strands its material_stock and treasury on a non-seat.

**Why it matters.** CIVILISATION.md's hinterland-pointer rule is satisfied either way; which reading is right is a design call about what a schism IS. Taken on Ben's behalf because the harness was red and both sprints 40 and 41 had shipped over it.

- Stands: seats move.
- Reverse: a schism razes the seats it takes, and the material is lost.

### NR-875 — AUTHORISE: the sprint-42 wave-0 re-bless — seedA/on digest and the exploration R3b pin moved by the schism fix
*decision · raised 2026-09-15 · from BL-981 merged on sprint-42-wave-0; world_determinism and exploration_sim_harness on the integrated tree.*

One cause: the schism verb no longer demotes seats (NR-874). Digest seedA/on 49FB45407FF7C3C0 -> 457483363D79D700; seedB/on 728607C66CE6A4BE, seedA/off F9BF05466A631FF9 and the 1960 two-span 851FE345B2E37618 are unchanged. exploration_sim_harness R3b (the w_want_q = 0 pin) reads battles 308 (pinned 306), conquests 306 (304), foundings 472 (522), owner_changes 2196 (2174); subjections, freed, tribute, treaties, broken unchanged. Shape on seed 2 of the colonisation harness: 1720 regions and 116 seats, where the razing world had 1700 and 154 — fewer seats because breakaway realms now keep theirs instead of the parent re-founding. history_sweep.json and exploration_sweep.json are regenerated on the integrated tree and checked in; their movement is in the sprint-42 wave-0 DEVLOG entry.

**Why it matters.** DELIVERY.md sec The digest re-bless is one act per WAVE: the pin and the digest are re-blessed against a described shape, by Ben, never absorbed. Until authorised, R3b stays red with this entry as its stated cause.

- Authorise: re-pin R3b to 308/306/472/2196 and record 457483363D79D700 as the seedA/on baseline.
- Not yet: keep the pin red and read the sweep movement first.

### NR-876 — READING: by population the Empires arc is more concentrated and has three times fewer real risers than region count showed
*question · raised 2026-09-15 · from BL-970 (share readings population-weighted), history_sweep over 16 seeds; both columns now on the face and in history_sweep.json.*

Largest share at 1200 CE: median 8% by regions, 10% by population (ranges 5-18 / 6-19). Peak share: 11% vs 13% (8-20 / 9-25). Hegemony 0/16 both. Rise/peak/fall worlds 16/16 both. Polities that rose AND fell per world: median 74 by regions, 22 by population. Eliminations 16/16 gross (median 191 dead of about 237 ever held). Weakest survivor is 4916 heads in every seed, a derived founding floor with no spread.

**Why it matters.** The audit's premise was that region growth inflates the arc. On concentration it deflated it (the biggest polities hold denser ground than the founded frontier); on shape it inflated it (most risers were founding empty ground, not gathering people). Sprint 38's distributional done_when reads the same verdict on either column, but the magnitudes Ben has been shown were the region ones. Is a 10-25% biggest-empire world the shape Ben wants before Digitisation, and should the wizard's leaderboard show the population column?

- Accept the shape; make the population column the one the wizard shows.
- Tune toward larger empires (a wave-2 item, tuned by forces, never clamped).
- Read again after wave 2 moves the world.

---

## Resolved

Kept, not pruned: the reasoning is the point. Prune only in a deliberate sweep, once the
answer has landed in an authority doc.

