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

*19 entries — 14 open, 5 resolved.*

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

---

## Resolved

Kept, not pruned: the reasoning is the point. Prune only in a deliberate sweep, once the
answer has landed in an authority doc.

### NR-808 — The creeds' tribal marches are wars, sitting inside a span whose premise is that war is not yet the process
*question · raised 2026-09-09 · from The colonisation design session, 2026-09-09. Recorded as an open question in COLONISATION.md and raised here because it is a call, not work.*

CREEDS.md's tribal marches resolve single-round pairwise wars at cradle grain and WELD two cradles into one, lowering fragmentation_q before nation_params_from_ladder reads it. Under the new colonisation span (COLONISATION.md) they run inside a span whose stated premise is that spreading, not fighting, is the interesting process.

**Why it matters.** They are the only wars in the peaceful span, so leaving them where they are makes the span's premise partly false on its own terms. But moving them is not free: welding drives fragmentation_q, fragmentation_q drives the SEED BUDGET, and the seed budget drives the nation count. Moving the marches past the boundary changes how many nations a world has. That is a measured change, not a design one, which is why nothing was moved by this session.

- Leave them where they are - a peaceful span is a comparative claim, not an absolute one, and cradle-grain welding is coarse enough not to contradict it.
- Move them to the boundary, so the colonisation span is warless by construction and the marches become the first act of the empire phase.
- Fold them into the span as a NON-violent mechanism - two cradles that meet merge by contact rather than by conquest, keeping the fragmentation effect and dropping the war.

> **Recommendation:** Do not decide it dry. Whoever builds BL-846 (colonisation span) should measure the nation-count distribution across a seed spread with the marches in each position, and bring you the three numbers. The third option is the most interesting design and the least measured.

> **RESOLVED.** BEN, 2026-09-09: option 3, re-derive fragmentation from contact. The tribal marches RETIRE; fragmentation_q is read off how far two peoples' streams interpenetrate, which the colonisation span already produces in culture_shares.

THE FINDING THAT REFRAMED THE QUESTION: the marches' TIMING was the lesser problem and their GRAIN the greater one. They compare cradle to cradle - a scalar against a scalar between two discrete points - but under diffusion a cradle is a SOURCE, not a point, and what meets at a frontier is two streams already mixing. Moving them past the boundary would have fixed when they ran and left that untouched.

WHAT IT SEPARATES: aggression_q drove both how consolidated the political map is and how a polity fights once it exists. It now drives only the second; package breadth drives the first. One cause each.

Written into COLONISATION.md (new section: Fragmentation comes from contact), CREEDS.md (the creed drives, rewritten) and HISTORY.md (the ladder pass + the pipeline diagram). Work is BL-852. TWO THINGS CARRIED FORWARD, both open in COLONISATION.md: the non-hegemony floor must be re-derived for the new mechanism (the old welding carried an explicit half-fragmentation floor for BL-224's sake), and the nation-count distribution must be MEASURED across a seed spread before the change lands.

*Files: `docs/lore/CREEDS.md`, `docs/generation/COLONISATION.md`, `src/world/creeds.cpp`*

### NR-814 — Culture relations: four calls that decide whether the empire phase has an engine
*question · raised 2026-09-09 · from The Empires design pass, 2026-09-09. Ben asked for a quick design aside on point 5 (culture relations); CIVILISATION.md sec Culture relations carries it and these are the calls it deliberately did not answer.*

Cultures are to be alike or opposed, and that opposition is meant to be the engine of conquest. The SIMILARITY half has an answer already: the migration builds a family tree of peoples, so kinship is a similarity measure EARNED by history rather than assigned (BL-865 retains it). The OPPOSITION half does not, and kinship alone will not supply it -- kinship gives DISTANT, not OPPOSED, and two peoples on opposite sides of a continent who never met are distant with no quarrel.

**Why it matters.** It decides whether the empire phase is a map of armies bumping into each other or a map of peoples who want different things. It also decides how much new machinery sprint 38 needs: three of the candidate axes are quantities that ALREADY exist and are already earned, so a good answer here may cost almost nothing, and a bad one invents a whole relations system beside the ones already running.

- IS OPPOSITION SYMMETRIC? `grudge` is a DIRECTED table, so the sim already has a precedent for asymmetry -- but a disagreement about how one ought to live may be mutual. Directed makes relations a matrix; symmetric makes it a set of pairs, and halves the storage.
- DOES KINSHIP DECAY, or is the tree enough? Two peoples five generations apart may be as foreign as two unrelated ones, or kinship may hold indefinitely and make whole branches of the tree natural allies.
- DOES OPPOSITION CAUSE CONQUEST, OR MERELY PERMIT IT? The sim already scores campaigns with w_cult discounting foreign ground. Feeding relations into that weight is the SMALLER change and keeps one scorer; giving relations a score of their own is what would make them the engine Ben describes. This is the one that most changes sprint 38's size.
- HOW DOES A CIVILISATION RELATE TO OPPOSED CULTURES INSIDE IT? If two opposed peoples end up in one civilisation, does it resolve the opposition, inherit it, or fracture?

> **Recommendation:** Answer (3) first, because the other three are cheap once it is settled and expensive to revisit after. My lean is PERMIT rather than CAUSE for the first cut: it keeps a single scorer, it is measurable against the existing sweep, and it can be raised to CAUSE later if the histories come out flat -- whereas a second scorer is hard to remove once other things read it. On (1) I lean symmetric for similarity and directed for grievance, which is how the two already behave: kinship is a fact about a shared past, a grudge is something one party holds.

> **RESOLVED.** PARTLY RESOLVED 2026-09-09. Ben, on call (3): 'Go with permit rather than cause for now. This is really work for sprint 38.'

SO OPPOSITION PERMITS CONQUEST RATHER THAN CAUSING IT: relations feed the existing w_cult weight rather than raising a score of their own. One scorer, measurable against the sweep already in place, and raisable to CAUSE later if the histories come out flat -- whereas a second scorer would be hard to remove once other things read it.

CALLS (1), (2) and (4) REMAIN OPEN and are sprint 38's, along with the build itself (BL-870). They were deliberately not answered here: (3) was the one that decides the size of the others, and answering it first is what makes them cheap.

WHAT LANDED IN SPRINT 37 INSTEAD is the substrate the whole question rests on: BL-865 retains the migration's family tree, and it turns out to have real depth -- deepest descent 9 and 10 on seeds 0 and 1, across 676 and 929 cultures, every one of which walks back to a cradle. Kinship distance therefore carries actual signal rather than being flat, which is what a similarity measure needs to be worth reading.

*Files: `docs/generation/CIVILISATION.md`, `src/world/creeds.hpp`, `src/world/history_sim.cpp`*

### NR-818 — Does the Culture round COAST to the boundary, or does its terminating condition become it?
*question · raised 2026-09-09 · from Your elicitation answer on the span boundary: 'Run this from 0 CE to 1200 CE'. It names the Empires start and does not say what happens to the migration's own ending.*

Two readings. (A) THE COAST, adopted: the Culture round still ends when every habitable landmass carries some culture - a derived year - and the world then holds what migration left it until 0 CE, simulating nothing in between. (B) THE CLAMP: the Culture round's terminating condition simply becomes 0 CE.

**Why it matters.** COLONISATION.md settled the derived terminating condition on 2026-09-09, overturning a stated-year ruling made earlier the same day. Reading B would overturn it a second time within the day; reading A keeps both your rulings true at once. It also changes a written line in two authority docs, which is why it is raised rather than assumed.

- A - coast to 0 CE, derived ending preserved (ADOPTED)
- B - the migration's terminating condition becomes 0 CE
- C - derived ending, and the Empires start floats with it

> **Recommendation:** A, on the grounds that it is the only reading under which neither of your two rulings has to be discarded, and because the design already uses exactly this device across 1200 -> 1560. The consequence to accept is that a world filling early sits still for a while - which is what a dark age looks like, and it is cheap. Overturn to B if you want the migration span itself bounded.

> **RESOLVED.** RESOLVED (Ben, 2026-09-09): reading A, the COAST, confirmed - and the boundary moved with it. The Culture round keeps its derived terminating condition, and the world holds what migration left it until the stated boundary, simulating nothing in between. THE BOUNDARY IS 400 BCE, NOT 0 CE, and pass 1 now covers 3,600 years rather than 4,000: Culture 2400 BCE -> 400 BCE (2,000 years), Empires 400 BCE -> 1200 CE (1,600 years). 1200 CE is unmoved, so the coast to 1560, pass 2 and the 1960 epoch are untouched. One claim written under the first cut is now FALSE and was removed: 0 CE was 'a year the engine already knows' because the sim's ancient arc ends there - 400 BCE is not, so the split is real work rather than free. Written into CIVILISATION.md sec The span is 400 BCE to 1200 CE, which owns the arithmetic; COLONISATION.md, STARTUP.md and GENERATION_STRATEGY.md take their figures from it. Carried by BL-871.

*Files: `docs/generation/CIVILISATION.md`, `docs/generation/COLONISATION.md`, `docs/ui/STARTUP.md`*

### NR-823 — CALL: reach GATES conquest (2026-09-09) and the arc requires conquest to COMPOUND (2026-09-10) pull against each other
*question · raised 2026-09-10 · from Sprint 38 close. history_sim.hpp sec sustainable_campaign_floor_q already says in the source: "80 IS WHERE THIS SHIPS, PENDING BEN'S CALL."*

Ben ruled 2026-09-09 that reach GATES a campaign rather than pricing it -- a wall, not a toll -- so a rich polity cannot buy past geography. Ben ruled 2026-09-10 that the world must show polities eliminated, empires forming, empires collapsing, and unequal survivors. A hard gate caps how far success can carry before it must stop, and the field's own comment records that it "removes that long tail entirely" in which a shrinking realm's last holdout finally falls. Both rulings are live and they conflict.

**Why it matters.** This is the crux of BL-889 (conquest must compound) and it decides what that item is allowed to change. Measured evidence that the gate is a hard veto: history_sim_harness fails R3a2/R3a3 on main with "near: 6 battles / 1 conquests | far: 0 battles / 0 conquests" in a case that deliberately sets neighbour_radius=40 and w_dist=0 so ONLY supply decay can stop the far target. A prior investigation already showed floors of 80 and 20 give identical elimination counts, so this is not a calibration question.

> **Recommendation:** A, on the evidence -- it satisfies both rulings rather than trading one off. A wall that MOVES when you win is still a wall, and BL-892 (reach works inert) suggests the intended mechanism for moving it already exists and is broken.

> **RESOLVED.** BEN, 2026-09-10: option A -- keep the wall, make it move. The 2026-09-09 reach-GATES-not-prices ruling stands; the gate is not fixed, so winning extends reach outward and geography must be BUILT past rather than bought past. Written into CIVILISATION.md sec The arc the phase must produce. B (soften to a price) and C (centre chains, BL-887) both explicitly declined. Consequence: BL-892 raised to priority A -- it records the widening mechanism inert (W7b), which is the lever BL-889 now rests on. BL-889 may NOT be delivered by lowering sustainable_campaign_floor_q.

### NR-824 — CALL: BL-861 (no conquest in the span) is met as literally written and unmet in intent -- close it or rewrite it?
*question · raised 2026-09-10 · from Sprint 38 close, measured against history_sweep at 16 seeds on main (c83a0d74).*

BL-861 was filed on "0 battles, 0 conquests, 611 foundings" on seed 0 and asked that the cause be measured. Measured: battles median 412, conquests median 386, zero worlds with zero conquest, B384a passes. Its stated DONE WHEN is satisfied. But the world it wanted -- one with a real arc in it -- is not there: 0 eliminations in 16/16, largest empire 3.3%, rise/peak/fall 0/16.

**Why it matters.** Leaving it open with a stale premise is how backlog prose rots into a false record; closing it as delivered risks reading as if the underlying design intent was met, which it was not. BL-889 now carries the surviving question.

> **Recommendation:** A -- the item did its job (it forced the measurement that found the real defect), and BL-889 carries the intent forward with the evidence attached.

> **RESOLVED.** BEN, 2026-09-10: CANCEL as superseded -- not the "close as delivered" that was recommended. BL-861 is marked cancelled rather than complete, which is the honest record: the item asked for a cause to be measured, the measurement happened, and it found a different defect than the one the item described. Its surviving question is carried by BL-889 (conquest must compound). A cancelled row is also what "--status cancelled finds work that was closed unbuilt" is for.

