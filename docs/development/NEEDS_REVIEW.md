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

*30 entries — 26 open, 4 resolved.*

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

### NR-847 — DECISION TAKEN: BL-931 wires the Exploration span opt-in, world_params::exploration_sim_enabled default false
*decision taken on your behalf · raised 2026-09-12 · from BL-931, wave 1 of sprint 40.*

Flipping exploration_sim_enabled to true by default would move region::nation from the 1200 CE map to the 1660 CE one and shift every generation golden -- real re-baselining work BL-931 did not scope in. Landed opt-in instead so the rest of sprint 40 can build against a real, running span without moving main's default generated world out from under every other in-flight consumer. A later item (likely the wave-5 tree-migration/exemplar wave, or its own item) needs to flip the default once the phase is far enough along that the golden move is worth taking once rather than piecemeal.

---

## Resolved

Kept, not pruned: the reasoning is the point. Prune only in a deliberate sweep, once the
answer has landed in an authority doc.

### NR-831 — w_aggr_q's lean sits INSIDE the season loop and so compounds across summer and winter
*observation · raised 2026-09-11 · from Noticed by the BL-838 agent while siting its own lean; out of that item's scope, so flagged rather than changed.*

BL-868's creed-aggression lean (`w_aggr_q`) is applied inside the per-season loop in `history_sim.cpp`, so it is applied more than once per candidate and compounds across the seasons. BL-838's fear lean was deliberately sited OUTSIDE that loop, on the reasoning that the property being read is a fact about the TARGET rather than about the weather.

**Why it matters.** If the compounding is unintended, then BL-868's delivered magnitude is not the magnitude it appears to be -- it was tuned through seven failed attempts to a figure that includes a doubling nobody wrote down, and any future tuning of it starts from a false baseline. If it IS intended, the two lean sites now disagree about what a per-mille weight means in this file, which is the kind of quiet inconsistency that makes the next calibration wrong.

> **Recommendation:** Worth ten minutes to read and then either fix or comment. It is cheap to settle now and expensive to discover during a future calibration -- this file has already published three wrong diagnoses that were caught only by one number contradicting another.

> **RESOLVED.** BL-927 (2026-09-11): the lean was relocated outside the per-season loop in history_sim.cpp, so it applies once per candidate instead of compounding across summer and winter -- confirming the compounding WAS unintended, per this entry's own concern. Measured before/after on the wave-1 sweep: battles 3114 -> 3114 median (unchanged); world_determinism digests for seeds A and B unchanged by this item alone, but seeds 0 and 3 in the 4-seed reading moved slightly. No further action.

*Files: `src/world/history_sim.cpp`*

### NR-843 — NOVEL WORK: two new authority docs and a FOURTH tree, from one design session
*novel-work · raised 2026-09-11 · from Exploration design session (BL-930..BL-937).*

No doc owned the phase after Empires, so the session created two: docs/generation/EXPLORATION.md (full authority for 1200-1660) and docs/generation/DIGITISATION.md (a PLACEHOLDER, per Ben's ruling, owning only the boundary). Both are now rows in CLAUDE.md section 3.

The scope growth worth flagging is the TREE. trees/TREES.md asserted "three trees, one grammar" as a settled shape (Ben, 2026-09-10); Ben's 2026-09-11 ruling that "exploration and industry use different trees" makes it four. The session re-pointed the chain in prose and in one store string - Empire's rim milestone now opens the EXPLORATION tree, and the Industry tree's root is gated behind Exploration's rim - but the fourth tree itself is unauthored and is BL-930's work. Until it exists, the rim milestone opens a tree that is not there.

FLAGGED BECAUSE a design session turning a settled three into a four is the kind of growth the novelty rule wants chosen rather than accreted. Nothing in src/ moved.

=== RESOLVED (Ben, 2026-09-11): "please go ahead and design a mock-up tech tree." EXPLORATION_TREE.md and exploration_tree.json now exist -- 31 nodes, 3 rings, 4 branches + spire, 3 milestones, 3 forks, lint OK. tree_lint.js knows the tree. The four-tree chain is confirmed and the rim no longer opens a tree that is not there. ===

### NR-844 — DECISION TAKEN: Exploration tree sized at 3 rings / 4 branches / ~34 nodes, from the span ratio alone
*decision taken on your behalf · raised 2026-09-11 · from Exploration design session; trees/TREES.md sec Sizes, BL-930.*

TREES.md's sizing rule is that a tree is sized to the rounds its phase gives a leading polity, asserted by the sweep rather than picked. The Exploration tree has no sweep yet, so its row was set by SPAN RATIO: 460 years against the Empire phase's 1,600, so three rings rather than four, four branches, three milestones, ~34 nodes of the 64 cap. That is a guess wearing the rule's clothes, and it is recorded as a target.

RELATED, AND NOT FIXED: the INDUSTRY tree is authored at 62 nodes across 4 rings for a 400-year span, and the split leaves it 300 years. By the same rule it is now oversized. The size row in TREES.md was left stating what the tree IS (4 rings, ~60) rather than what the rule wants, with a paragraph saying so - reshaping an authored 62-node tree is real work, not a row edit, and it was not in this session's scope.

=== RESOLVED (Ben, 2026-09-11): "we should shrink the industry tree." BOTH HALVES ANSWERED, and the second differently than expected. (1) The Exploration tree is authored at 31 nodes over 3 rings -- under the ~34 target, reported rather than padded. (2) The Industry shrink is NOT a trim: measurement showed ring 4 is the twentieth century (electrification, oil, flight, broadcast, antibiotics) and must survive, while RING 1 IS THE EXPLORATION AGE and duplicates newly-authored Exploration nodes. The shrink is a migration of ring 1 with every ring shifting down, landing at ~44 nodes. BL-938 owns it. ===

### NR-846 — DECISION TAKEN: build order is BL-930 -> BL-931 -> BL-937, not the NEXT_SESSION.md wave table's BL-937-first
*decision taken on your behalf · raised 2026-09-12 · from Sprint 40 batch delivery, opening session.*

NEXT_SESSION.md's wave table puts BL-937 (the ten readings) in Wave 0, ahead of Wave 1's BL-931/BL-930, with the framing 'lands first, not last'. But BL-937's own requires field names BL-931, and BL-931's requires BL-930 -- there is nothing for the readings to measure until the engine runs, and no tree for it to run against until the tree is wired. Read literally, the wave table asks for an impossible build order.

RESOLUTION: build BL-930 (wire the tree) then BL-931 (the span runs) then BL-937 (instrument the readings), immediately, before any of waves 2-5. This preserves the intent the wave table was protecting -- instrumentation lands before any of the thirteen mechanism items, not deferred to the sprint's close -- while respecting the actual dependency graph. Nothing in EXPLORATION.md or the four rulings is touched by this; it is a sequencing correction, not a design change.

