# Project Io — Sprints

> **Generated file.** Produced by `node tools/session/render_sprints.js` from
> [`sprints.json`](sprints.json) (canonical, open sprints) and `archive/sprints-*.json`
> (completed sprints, cold). Edit the JSON, then re-run; hand edits here are overwritten.

A lightweight rhythm layered over the backlog/Delivery system: a **goal** stated at the start, a **retro** at the close comparing what landed against it. This is feedback *for Ben* — pacing and priority signal, not a new authority (backlog.json/REFINED.md/DEVLOG stay the source of truth for what’s actually true about an item).

Entries are one per **sprint** — a sprint is a themed span of work, not a fixed calendar week; it closes when its goal is settled (landed or deliberately descoped), not on a clock. A gap with no entry means no sprint goal was set — that’s fine, skip it rather than backfilling.

**Fully in JSON since 2026-08-24** (Ben: move sprints into JSON; archive them once complete). Drained from prose 2026-08-19; the last hand-kept surface — this file’s status table — retired 2026-08-24 after NR-598 caught it drifting. `sprints.json` holds the open/proposed/gated sprints; every completed sprint lives whole in `archive/sprints-*.json`, moved by `tools/session/archive_sprints.js`.

## Format

```
## Sprint N — theme (opened YYYY-MM-DD)

**Goal.** 1-2 sentences: the outcome this sprint is aiming for, referencing backlog item ids
and/or a version goal (v0.1.1 etc.).

**Planned.** BL-ids targeted, one line each.

**Retro** (filled in at close).
- Landed: ...
- Slipped: ... (+ why — scope grew, blocked on a dependency, deprioritized)
- Feedback: anything worth Ben knowing about how the sprint actually went — a pattern, a
  misjudged estimate, a design call that needed more/less discussion than expected.
```

(The old **Runtime** line — total session time vs. items delivered — is dropped from the template. It went uncollected for seven-plus consecutive entries; see Sprint 19’s retro in the archive for the reasoning. If the pacing signal is wanted again later, derive it from commit timestamps rather than reviving an unstarted timer.)

---

**Active-sprint cap (Ben, 2026-08-23).** No more than 3 sprints open/proposed at once — enough that a session doesn’t have to weigh a long tail of undocumented parallel work.

**Sprint-number ceiling (Ben, 2026-08-24): keep sprints 18 and below.** Nothing is planned or authored past sprint 18 — narrower than the count cap above, this bounds *how far ahead* the sprint horizon is allowed to reach at all.

**Unstarted plans are DELETED, not archived (Ben, 2026-08-24).** A sprint that was only ever open/proposed and never really executed is a stale reference once dropped — closing it and keeping its prose around just to reopen the same number later isn’t worth the upkeep. On adopting the cap, 27 open/proposed sprints (plus Sprint 32, authored and dropped same-day on the ceiling above) were removed outright from `sprints.json`, freeing their numbers rather than retiring them: 20, 21, 22, 22-24 preamble, 23, 24, 25, 26-33, 26a, 26b, 27, 28, 29, 30, 31, 32, B1, B2, B3, C1, C2, C3, D2, D3, D4, N3, N4, ST1, W1. This is narrower than it looks: a sprint that actually **landed work** — even a partial or unsatisfying result (Sprint 19’s goal NOT met, P1’s rendering debt) — is a real historical fact and stays in the status table as closed. Only the never-executed ones were deleted. A freed number picked back up is authored fresh against the current docs, never resurrected under its old prose.

**Also deleted 2026-08-24, same basis:** six orphaned backlog items (BL-579–BL-584) a concurrent review-queue-purge session had filed with no sprint attached, plus Sprint 32’s own three items (BL-595–BL-597, Logistic Points / Throughput lens) — all unstarted, all removed from `backlog.json` outright rather than archived.

**Sprint 25b deleted 2026-08-24, same policy** (the Sprint 18 design form verdict): never opened, gated on a sequence (25a → 21 → 23 → 25b) whose middle numbers the purge had already freed. Its undelivered half — interception narration, the out_cuts sink — is unowned until re-filed (NR-599); the interdiction core itself landed 2026-08-21 and is unaffected.

**Sprint-number ceiling advanced to 19 (Ben, 2026-08-25: 'Open sprint 19 with these items').** The 2026-08-24 ceiling stands in spirit - author no sprint past the one Ben opens - with 19 now the horizon.

**Sprint-number ceiling advanced to 20 (Ben, 2026-08-26: 'Write this up as sprint 20').** The 2026-08-24 ceiling stands in spirit - author no sprint past the one Ben opens - with 20 now the horizon.

**Sprint-number ceiling advanced to 21 (Ben, 2026-08-26: 'A, and open it as sprint 21').** The 2026-08-24 ceiling stands in spirit - author no sprint past the one Ben opens - with 21 now the horizon. Two sprints are open (20, 21), inside the three-sprint cap.

**Sprint-number ceiling advanced to 22 (Ben, 2026-08-27, corrected 2026-08-28 00:16).** The 2026-08-24 ceiling stands in spirit - author no sprint past the one Ben opens - with 22 now the horizon. Two sprints open (21 demand, 22 UI visibility), inside the three-sprint cap. Ben has named 23 for the new UI elements he will describe once Sprint 22's improvements land, so 23 is spoken for but NOT yet authored.

**A sprint was renumbered and un-renumbered across the 2026-08-27/28 midnight; both moves are recorded because neither lost anything.** The demand sprint was moved 21 -> 22 to free the number for a UI visibility pass, then moved back at 00:16 on 2026-08-28 when Ben clarified that 'sprint 21' means the demand sprint to him. What made the round trip safe is that the demand sprint had LANDED WAVE 0 and was therefore moved WHOLE both times, never deleted under the 2026-08-24 unstarted-plans rule. The lesson worth keeping: a number is cheap to move and expensive to be wrong about, so confirm which sprint a bare number refers to before renumbering anything - the pre-move records (the DEVLOG heading, the NEEDS_REVIEW sources, the `sprint-21-wave-0` requirements batch key) all said 21, and they were right.

**The active-sprint cap is deliberately exceeded, as PROPOSED not open (Ben, 2026-08-28: "Let's make a sprint for each batch").** The 3-sprint cap counts sprints being worked; 23-27 are a decomposition of one agreed body of work, authored together so the batch boundaries are settled once rather than re-argued five times. Only one is worked at a time. If that reads as cap-breaking later, close them back down to a single sprint with a batch list — the content is the value, not the numbering.

## Open now

### Sprint 33 — Long-term market viability, the growth half - the field that keeps producing
*Open · opened 2026-09-02*

**Goal.** A field that keeps producing over thirty years. Sprint 31 ended with a majority of corps operating-positive and debtors a tenth of the field - and valued production still falling across the run (x0.2 on seed 0, x0.6 on seed 1 from a level ten to twenty times the old baseline), with most buildings sitting at the new supply floor for want of power and the remaining debt entries being processors that buy at the ceiling and convert at a loss. The felt goal is unchanged from sprint 31 - every player can make a steady profit - with the emphasis moved from "not going broke" to "still growing at year 30".

**Planned.**
- BL-746 stage 2 (the generation bootstrap, NR-782 (c)) - FIRST. The field's mean supply factor sits at ~0.57: most buildings run at the floor because power does not arrive (generation 18 -> 3 units against a demand of 64; a generator short of power throttles itself; only network-reached tiles can receive it). Design the bootstrap - generators exempt from the power draw, a building drawing power only once its catchment market has priced it, and whatever makes generation get BUILT - and measure it on the debt instrument: the done-when is the mean supply factor climbing off the floor toward 1.
- BL-745 (processor input bid cap) - the remaining debt entries. 42 of 57 are processors producing less than they buy: construction materials at 8-10x base through the boom (free materials moved the median entry tick 22 -> 34) and inputs bought above the recipe's output value. The anchor's M1 identity carried to the live tick, at the one shared seam, and the scorer's build tempo pricing materials at the live market.
- BL-738 (industry rates go live) - re-measure the goods-upkeep rates against the floored world: with the floor and the no-wire rule the draw is a real cost again, not a death sentence, and its size should be set by what the field can supply.
- BL-726 (sweep debt dynamics) - seed 1 still shows interest at 70% of net loss at year 30 while seed 0 is at 5%; the spiral is half-tamed. Interest rate, the balance floor, the exit thresholds, on the standard lapse.
- BL-725 (sweep price levers) - the ceiling derivation clears again (ceil > 8.84 against 10.0); the sweep re-runs on the anchored tables once the two levers above have moved the field, not before.
- The instrument: campaign_lapse --epoch 1960 in both forms (--warm 0 --ticks 60 for where debt begins; the standard 80/120 for the done-when), with the debt.csv classification (starved / loss-converting / unsold) as the per-entry verdict.

**Done when.** On the standard industrial lapse, both seeds: valued production at quarter 200 at or above quarter 81 (the field grows or holds), a majority of corps operating-positive (kept from sprint 31), the mean supply factor above 0.8, and interest below a quarter of net loss on every seed.

**Risk.** The generation bootstrap is a design change to a channel that was authored to be systemic (power as a grid good); exempting generators or gating the draw on a priced market could hide the very scarcity the grid is meant to express. Measure the supply factor trend and the power price together, so a fix that only silences the draw reads as one. And every retune moves the goldens and the sweep baselines - one deliberate re-bless with provenance, not a dribble.

THE BASELINE THIS OPENS ON (2026-09-02, final-ind-s0/s1, standard lapse): corps 86 -> 61 / 71 -> 55; debtors 13 -> 6 / 7 -> 4; op-positive at 200: 46 of 61 / 31 of 55; median op net +16.9 / +1.7 per quarter; median balance 4,424 / 3,797; active buildings 224 -> 140 / 122 -> 85; valued production 20,761 -> 4,364 / 5,775 -> 3,531; convoys 156 -> 131 / 194 -> 79; interest share of net loss 5% / 70%; mean supply factor ~0.57. Sprint 32 is not skipped by accident: Ben named this 33.

### Sprint 32b — Gamified generation, 32b - the water model, and the reorder that 32a built the instruments for
*Open · opened 2026-09-06*

**Goal.** CONTINUE 32a, and the distinction between them is the point. 32a delivered foundations and instruments: the Era -1 sim runs on the 1960 arc as two spans, the sweep measures the run that actually builds a world, continents carry a time axis, the tick question is settled, and the saturation measure sits where generation can call it. NOT ONE of those changed a generated world - the 0 CE digests are unmoved across all five, deliberately.

32b IS WHERE THE WORLD CHANGES. Every wave-1 item moves it: the carve claims the shoreline, Settle stops founding on open ocean, campaigns become legal or illegal by domain, and naval rows stop being worth zero. That is a different KIND of sprint from 32a and wants a different discipline - which is why BL-780 exists, and why nothing else should start until wave 1 closes.

THE THROUGH-LINE FROM 32a's FINDINGS. Two of its measurements are what make wave 1 buildable rather than guesswork: 1105 of 3819 regions sit on water (613 on open ocean) and 43% of campaign adjacency already crosses sea. Ben's water ruling (2026-09-06) then turned both from defects into a model - coastal water is owned by whoever owns the shore, open ocean is owned by nobody, and a unit type declares which domains it can cross. So wave 1 is not new design; it is 32a's measurements plus one ruling, built.

AND THE REORDER CONTINUES BEHIND IT. Phases 2, 3 and 4 of Ben's eight-phase reorder still stand - the Lagrangian tile representation, paleo deposits, the population map drawn early, empires tuned to form - along with phase 6's static saturation search, which 32a unblocked by promoting the measure it needs.

**Planned.**
- WAVE 1 - THE WATER MODEL, in this order, and nothing else concurrent with it.
- BL-776 (coastal territory) - ONE PREDICATE. nation_generation.cpp:689 builds its unclaimable mask from is_water (coast, lake AND ocean), so the carve refuses every water tile. Narrow it to is_open_ocean. Coastal provinces then become owned for free, because province ownership derives from tiles.
- BL-777 (region domain) - regions carry the same three-way domain the province layer already uses, and Settle stops founding on OPEN OCEAN: ~613 regions, not the ~1105 BL-756 proposed deleting. Save-format bump; region has a positional read chain.
- BL-778 (unit traversal domains) - roster_row gains the field it lacks. Land units may cross OWNED coastal water, which is the deliberate middle case: you may walk your own shore, not someone else's.
- BL-779 (naval units become real) - unit_class::naval returns base power 0 and sum_stack skips the class outright, so three authored, port-gated, raisable rows are worth nothing. This is their first job. Rare naval combat is the design, not a shortfall.
- BL-780 (one re-bless) - closes the wave. The four above each move every world; landed separately they would produce four absorbed digest drifts and no single point where anyone asks whether the new world is BETTER.
- --- THEN, each wanting its own scoping rather than a slot in a batch ---
- BL-764 (Lagrangian tiles) - difficulty 5, a representation change to the oldest layer in the generator, every pass downstream reads its output. Its own item says split it at promotion. BL-765 (paleo deposits) follows, and unblocks BL-762's deposit split.
- BL-766 (population map early) - difficulty 5, touches the sim, the ECS and two save versions. It collides with BL-758 and probably DISSOLVES it: if centres exist before the sim, the sim's seed-any-empty-region rule has nothing to fire on.
- BL-767 (empires reliably form) - unblocked by 32a's BL-757. Tune until the rise-peak-fall shape is common across the seed spread; report the distribution, never clamp a world.
- BL-770 (Era 0 static saturation search) - unblocked by 32a's BL-775. SLICE THE SCORER FIRST and measure whether completeness discriminates between candidate rosters at all; if it does not, the search has nothing to search on and the rest is wasted.
- CHEAP AND OWED: BL-774 (bash harness builder - it has cost every agent and every hand-build in 32a), BL-759 (the 1960 baselines sprint 33 quotes are no longer reproducible), BL-760 (the band ceiling has no observable; the new save field has no round-trip assertion), BL-754 (the app still never prints its own generation budget).

**Done when.** Wave 1 is in and the 0 CE digests have been re-blessed EXACTLY ONCE, with Ben authorising against a stated description of what changed in the world's shape rather than against the hash. Concretely: coastal and lake tiles carry a nation and open ocean tiles do not; no region anchors on open ocean; a land stack cannot enter unowned water but can cross coastal water its polity owns; a naval row contributes real power and the ancient sim fields coastal units; and sim_water_census reports the new figures beside the 2026-09-03 ones (1105 of 3819 regions on water, 613 on open ocean, 43% of adjacency crossing sea).

**Risk.** THE SCOPE GREW FASTER THAN THE DELIVERY IN 32a - 8 items to 34, 5 delivered - and 32b inherits 29 of them. The wave structure is the mitigation and it only works if it is honoured: BL-776..780 is one self-contained body of work with a single judgement point, and starting anything else alongside it re-creates exactly the sprawl that closed 32a.

THE WATER MODEL MOVES EVERY WORLD, understood rather than discovered. BL-780 exists so it moves ONCE, and so the re-bless is judged against a description of the new world's SHAPE - regions lost, territory gained, campaigns made illegal, naval engagements that now occur - rather than against a hash. The standing rule is that goldens are contracts: report movement, never re-bless without authorisation.

THE REVIEW BARRIER IS STILL OWED FROM 32a. Sub-agent capacity was unavailable for that entire session - twelve launches, twelve 529s, zero tool calls - so every slice was hand-built and no cold cross-slice review ran over the five delivered items. If agents are available, run it before building on them.

AND THE 32a LESSON THAT APPLIES DIRECTLY HERE: a green check is not evidence that it looked. Four instruments were found measuring something other than their subject, and one of them let a real regression through a wave already called verified. Wave 1 changes the world on purpose, so the instruments that judge it must be known to see it.

CARRIED FROM 32a, and none of it is optional reading before wave 1 starts.

FOUR CALLS WAIT ON BEN, three of which block work here:
  * BL-758 - does era-seeded demography at 1960 belong, or is it scope BL-747 never claimed? era_world_harness R2 is deliberately RED for it and must NOT be weakened to pass. BL-766 may dissolve the question entirely.
  * Water gives 0 defence AND 0 forage. Zero cover at sea reads correct; zero forage starves a fleet where it sits, which may be the right blockade pressure or an accident of a table written for land.
  * Can a coastal province hold a port? Buildings refuse water outright today.
  * NR-783 - is the span boundary authored at epoch minus 400, or derived from the first furnace?

THREE HARNESSES ARE AD HOC pending Ben naming them in .claude/skills/verifier-headless/SKILL.md: continent_drift, sim_water_census, and the promoted saturation measure. Authoring the check was 32a's; wrapping it as a skill is his call.

THE BEFORE-FIGURES ARE ALREADY CAPTURED - do not re-measure them:
  world_determinism  039EE9880739CDF6 / B0EBBA249B3DDABB / DE55600457797638
  era report seed A  years=400 battles=270 conquests=207 foundings=833
  1960 two-span      1160 -> 1560 -> 1960, digest DB86651B9A596F7B
  sim_water_census   1105 of 3819 regions on water (982 sea, 613 open ocean); 43% of edges cross sea
  warm start         72-73 s on BOTH arcs (53.7 s on one later run - treat 72 s as standing until BL-761 measures it properly)
  generation         ~8.2 s Release; the era pass itself only 197-323 ms

--- RE-PLANNED 2026-09-06 ON BEN CALL: THE GENERATION-SIDE MARKETS GO FIRST, NOT THE WATER WAVE ---

Asked which body of market work to continue, Ben chose 32b GENERATION-SIDE MARKETS and chose it with
the gate stated as unmet. That re-orders this sprint: wave 1 (the water model, BL-776..BL-780) is not
next, and the market chain is. The water wave keeps its shape and its captured before-figures for
whenever it opens - nothing about it is withdrawn, only deferred.

THE GATE HE ACCEPTED, recorded so the result is read correctly rather than rediscovered. BL-770 was
gated on sprint 33 showing valued production flat or rising, because a search over a field whose
production falls (x0.2 on seed 0, x0.6 on seed 1 across the standard lapse) selects the least-bad
SHRINKING economy - it ranks degrees of failure and reports a winner. Ben took that knowingly. The
discipline it costs: phase 6 candidate scores in this state are ORDINAL and provisional, not
evidence that the chosen landscape is viable, and the item must say so in whatever it reports. The
first slice happens to be the one least exposed to this, which is what makes the ordering survivable.

WHAT IS ACTUALLY REACHABLE UNDER THIS CHOICE - and it is one item, not five:
  * BL-770 (Era 0 candidate search), FIRST SLICE ONLY: the scorer, sliced out and measured for
    whether it DISCRIMINATES between hand-made candidate rosters. Requires BL-775, which is complete.
    This is the whole of the reachable work today.
  * BL-772 (retire warm start) requires BL-770 - the 72-second budget win, unreachable until the
    search exists.
  * BL-768 (roads and markets from history) requires BL-766 (population map early), a difficulty-5
    phase foundation that has not started.
  * BL-750 (tariff posture) requires BL-748 (industrial pass ladder), not started. Its own design
    call is now taken (derived at handoff), so it is unblocked in DESIGN and blocked in SEQUENCE.
  * BL-752 (colonial ties) requires BL-749 (sea-leg campaign), which is itself held on the five
    design calls at NR-785. Its BL-751 require was repointed to BL-770 when BL-751 was cancelled.

So the honest read of this ordering is: it buys the phase 6 scorer experiment now, and the rest of
the generation-side market chain still waits on phase foundations that are not market work at all.
If that experiment comes back with completeness flat across candidates, the chain has no root and
sprint 33 becomes the only market work left standing.

EIGHT CALLS TAKEN THE SAME DAY, on the market-work elicitation form. Five are recorded in authority
docs and all eight on their items:
  * BL-746 stage 2 - NO PRICE, NO DRAW. A building draws a grid good only once its catchment market
    has priced it. PRODUCTION.md § A shortfall scales output. Done-when is a PAIR (supply factor AND
    the grid good price), because the gate silencing the draw looks identical to the gate working.
  * BL-745 - both purchase leaks in one item; ordered after BL-746 stage 2.
  * BL-782 (NEW) - the agency idle rule reads operating net at the recipe own bid cap, split out of
    BL-745 on Ben call.
  * BL-738 - industry rates wait for the field to hold power; its stage 1 figures are void.
  * BL-751 - CANCELLED superseded; its parts are named on BL-770 and BL-772.
  * BL-770 - the objective is completeness + supply-to-demand ratio + the SPREAD rewarded for
    unevenness. Recipe margin deliberately excluded (a precondition, not an axis). Scorer first.
  * BL-750 - protection DERIVED at handoff; the scored-verb form held with flatness as its only
    trigger. NATIONS.md § 4 Tariffs.
  * BL-730 - trade_goods_misc joins the endemic luxury basket, with the asymmetry dilution stated in
    MARKETS.md rather than left to be found.

GENERATION_STRATEGY.md § Three passes WAS REWRITTEN in the same pass: it still described pass 3 as
"the warm start, promoted", which BL-751 cancellation makes a fiction. Pass 3 now SELECTS a landscape
rather than settling one, and pass 3 table row names the static scorer plus one validation run.

--- THE MARKET BATCH DELIVERED 2026-09-06: FOUR ITEMS, AND THE HEADLINE IS A NEGATIVE RESULT ---

BL-774, BL-759, BL-760 and BL-770 slice 1. All four requirement groups complete.

BL-770 SLICE 1 ANSWERED ITS QUESTION, AND THE ANSWER IS NO. The phase 6 objective does not
discriminate between candidate rosters: five candidates on one fixed world (corporation_count
4/8/16 and two placement seeds - exactly the axes Ben point 3 names) score IDENTICALLY, every
term at relative range 0.000e+00. The positive control, built in so "flat" could be told from
"a scorer that returns a constant", moves 1.000e+00 across three different worlds.
THE CAUSE IS STRUCTURAL. Every term reads tiles, markets and population; market_saturation.cpp
contains neither "corporation" nor "building". The objective measures the WORLD saturation
POTENTIAL - whether a chain COULD close in reach of a market - and says nothing about whether
any firm closes it. So phase 6 cannot search on it as ruled, and a roster-aware term (actual
against potential completeness) is owed before the parallel search is worth a line of code.
This is exactly what the slice was commissioned to find out, at the cost the ruling predicted.

BL-759 RE-BASED THE SPRINT 33 NUMBERS AND THREE OF THEM MOVED MATERIALLY.
  * Seed-1 interest share of net loss 70% -> 7%. That VOIDS BL-726 premise outright, and
    sprint 33 interest done-when ("below a quarter of net loss on every seed") is ALREADY MET.
  * The field is healthier than assumed: op-positive at q200 is 48 of 55 and 50 of 63, against
    the recorded 46 of 61 and 31 of 55.
  * Valued production falls x0.57 and x0.68, not x0.21 and x0.61. The growth gate is still
    UNMET on both seeds, but the gap is a third of what sprint 33 was written against.
  Unmoved: mean supply factor at 0.598 / 0.570 against ~0.57, so BL-746 is sized as it was.

BL-760 GAVE THE BAND CEILING AN OBSERVABLE. Over 16 seeds: classical 8,179,138 units, medieval
101,814, gunpowder 0, industrial 0; span 0 8,280,952, span 1 zero. Assertion B1 (the ancient
span fields nothing above medieval) passes and can now FAIL. Works are zero in every band -
that is BL-757 (build_work never wins the scored contest), reported rather than asserted past.
The save round-trip differential was RUN: swapping the two year writes makes it fail.

BL-774 CLOSED THE AGENT TOLL AND UNCOVERED A BIGGER DEFECT. The bash builder exists; the
Lua-class routing is DERIVED (21 of 138, against a hand list that named four) and validated
against 16 real link probes with zero errors in both directions. On the way: build_harness.js
DID NOT WORK AT ALL, for anyone, from any shell - a spawnSync double-wrap split the vcvars path
at its first space and >nul swallowed the error, so the whole non-Lua verifier-headless tier
was unbuildable. Fixed, because BL-760 needed it.

DETERMINISM UNMOVED ACROSS THE BATCH despite history_sim.cpp being touched: world_determinism
ALL PASS with all four digests at their recorded values.

WHAT THE BATCH DID NOT DO, and it is the honest limit of the scope call: it did not advance the
generation-side market chain past its first question. BL-772, BL-768, BL-750 and BL-752 remain
blocked on BL-770, BL-766, BL-748 and BL-749 respectively - unchanged. The chain is still a
sequence, and its root now has a known defect to fix before the next link.

--- RE-PLANNED AGAIN 2026-09-06: THE WATER MODEL COMES BACK IN, AND THE SPRINT IS 20 ITEMS ---

Ben: "bring the water model back into view for this sprint. Then we should be able to do
everything but those loose instruments now." Scope is therefore every open 32b item EXCEPT the
loose instruments (BL-753 scoreboard, BL-758 demography, BL-761 warm-start measure, BL-781 query
flag) and the non-sprint leaves (BL-714, BL-727, BL-729, BL-730, BL-731, BL-733, BL-734).
Sprint 33 keeps its own six.

BRINGING THE WATER MODEL BACK CLOSED TWO ITEMS OUTRIGHT. BL-755 (sim crosses water free) and
BL-756 (sim settles on ocean) are cancelled superseded, on the strength of the water items own
text: BL-778 says "WHAT IT REPLACES. BL-755 measured 43% of the sim adjacency edges crossing sea
... this is what prices them", and BL-777 says its fix "is now the SMALLER HALF of what BL-756
proposed" - 613 open-ocean regions rather than a blanket ban on 1105, because coastal founding is
legitimate under the ownership rule. Both items had already delivered their measurement half
(sim_water_census) and both had their decision half taken by Ben water ruling. Neither had
outstanding work. Backlog 39 -> 37 open.

AND IT SHRANK NR-785 FROM FIVE CALLS TO THREE. BL-779 answers call (4) outright - the naval rows
DO gain real power. BL-777 largely answers (2), because region save format is bumped by the water
model anyway, so a harbour anchor field costs no extra migration. Still Ben and still blocking
BL-749: (1) which walk, coast tiles or all is_sea, and how four harbour rows map onto ranges;
(3) how "holds a harbour work" is tested, since work_row has no harbour flag; (5) whether an
overseas daughter re-surveys port_q or inherits it.

THE SHAPE, topologically waved (20 items, difficulty sum 71, six waves deep):
  W1 (d30) BL-776 coastal territory | BL-770 candidate search slice 2 | BL-766 population map
           early | BL-764 Lagrangian tiles | BL-762 resource origin | BL-767 empires form |
           BL-748 industrial pass ladder
  W2 (d23) BL-777 region domain | BL-772 retire warm start | BL-754 generation budget |
           BL-768 roads and markets from history | BL-769 consequence folds in | BL-765 paleo
           deposits | BL-750 tariff posture
  W3 (d6)  BL-778 unit traversal domains | BL-773 the 3-6 minute budget
  W4 (d4)  BL-779 naval units become real
  W5 (d6)  BL-780 the ONE re-bless | BL-749 sea leg campaign
  W6 (d2)  BL-752 colonial ties

ONE SEQUENCING CORRECTION INSIDE WAVE 5, and it matters more than its size. BL-780 must land
BEFORE BL-749, not beside it. BL-780 exists so the water model moves every world EXACTLY ONCE, at
a single point where someone asks whether the new world is BETTER. The sea leg also moves every
world - it changes where polities can campaign and settle - so landing them together folds two
independent world changes into one digest movement and destroys the very property BL-780 was
created to protect. Re-bless the water model, look at it, THEN open the sea leg.

THE THROUGHPUT RISK IS THE ONE WORTH STATING PLAINLY. This session delivered four items at a
difficulty sum of 9. The proposed scope is 71 across six dependency waves - roughly eight
sessions at that rate, and the deepest items (BL-764 Lagrangian tiles d5, BL-766 population map
d5, BL-770 d5) are each larger than this whole batch. Sprint 32a planned 8 and delivered 5 of 34.
The wave structure is only a mitigation if waves 1 and 2 are not opened simultaneously: W1 alone
is d30, which is more than three of these sessions.

WHAT IS ACTUALLY GATED, as opposed to merely large: only BL-749 and BL-752, on the three
remaining NR-785 calls. NR-783 (the span boundary) does NOT hard-block BL-748 - Ben own
recommendation there was "(b) eventually, (a) for now", and the authored offset is what is built,
so BL-748 can proceed and NR-783 becomes a follow-on rather than a gate. Everything else in the
20 is reachable today.

## Where things stand

| Sprint | Theme | State |
|---|---|---|
| 1 | Procedural generation v1 | Closed — goal met (food cluster landed 2026-08-02, see amendment) |
| 3 | Corp AI stage B + skill harness | Closed — BL-203, BL-204 both landed |
| 4 | Communication surface (BL-205 chat log) | Mostly landed — slice 1 complete 2026-07-26/28; only the C-route remainder (§7 Stage C) stays open, unstaffed |
| 2a | Close out the v0.1.0 cut set | Closed — all four planned items landed |
| 2b | BL-210 oral-history pivot (nations/corps rewrite) | Closed — all four rungs built (BL-217, BL-208, BL-218, BL-219) |
| 5 | Era −1 history sim, 0–2000 CE (BL-271–275) | Closed 2026-08-10 — four of five landed; BL-274 and BL-317 carried to v0.3.0 |
| 6 | The release sprint | Closed — five versions tagged; every cut minor now carries a done-definition |
| 7 | The stub minors become releases | Closed — v0.1.3 and v0.1.4 cut; post-v0.1.0 swept |
| 8 | Who the player is (design only) | Closed 2026-08-10 — BL-094 rewritten as the militia, BL-350 filed, v0.3.0 roster reconciled |
| 9 | The militia takes the field | Closed 2026-08-10 — v0.1.5 cut; BL-325/BL-331 landed, BL-332 designed and re-versioned |
| 10 | The living world | Closed 2026-08-11 — the living world: real firms produce and consume; market saturation made real, not injected |
| 11 | Procurement, and the goods it is about | Closed 2026-08-11 — all three landed in build order, each verified before the next; v0.1.14 cut |
| 15 | The 0 CE refocus | Closed 2026-08-12 — epoch 0, 3× map, Era −1 sim wired in, mercenary seam designed |
| 16 | The mercenary vertical slice | Closed 2026-08-24 — all ten (BL-569..BL-578) landed in six dependency waves; the loop live-clicked end to end; v0.1.15 cut |
| 12 | v0.1.11 reconciled | Superseded 2026-08-12 by the 0 CE refocus (NR-177), never opened |
| 13 | Generation visibility, and the owed timelapse | Superseded 2026-08-12 by the 0 CE refocus (NR-177), never opened |
| 14 | v0.2.0 (the AI opponent) | Superseded 2026-08-12 by the 0 CE refocus (NR-177), never opened |
| 17 | The ancient roster becomes a ladder | Closed 2026-08-24 — all ten items (BL-585..BL-594) landed. Cut v0.1.17, done-definition written at the cut |
| 18 | The military engagement surface | Superseded — the military-surface remainder subsumed into the re-planned Sprint 16 (2026-08-23) |
| 19 | The economy tells the truth | Closed 2026-08-17 — goal NOT met. The blame moved three times and landed on supply; goldens left red and unblessed |
| 18 retro | The growth gate, and four things measurement overturned | Retro-recorded 2026-08-16 — BL-428 complete; four plausible stories overturned by measurement |
| 18b | Roster invariants | Closed 2026-08-16 — BL-432 landed; BL-435 paused 4/6; BL-436 filed |
| 25a | The draw (upkeep, ordnance, convoy seam) | Closed 2026-08-18 — 6/6; goldens left red and attributed |
| 27 | The run is retained, and its failure is falsifiable | Closed 2026-08-20 — Lane A landed: the run retained, “the sim conquers nothing” a red assertion |
| D1 | A tech can express a buff | Closed 2026-08-19 — BL-479 complete same-day, 35/35; BL-443 rider deliberately not taken (NR-296 is Ben's) |
| P1 | The province becomes a thing you can see, and then a thing worth seeing | Closed 2026-08-21 — done_when met, then overshot by five items; owed: nothing was rendered |
| N1 | The two spines, landed inert | Closed 2026-08-23 — all three landed inert; two of the three were UNSOUND and were fixed in the closing pass (NR-546, NR-547) |
| N2 | The spines move | Closed — three lanes merged; one lane’s interpretation withdrawn after adversarial verification (NR-554) |
| 17b | The shell stops fighting the map | Closed 2026-08-24 — goal met. All nine items; 93 scripts / 4 failures, all four pre-existing (NR-606). Four doc fictions found, one fixed |
| 18 | Logistic Points land with their consumers | Closed 2026-08-25 — all eight landed (BL-596..BL-603, BL-606..BL-608); Sprint 17b merged alongside; v0.1.18 uncut pending Ben |
| 26 | Re-baseline (the gate; nothing else may open first) | Subsumed — split at execution into 26a/26b, themselves deleted in the 2026-08-24 purge |
| 19 | The world reads lived-in - population foundations | Closed 2026-08-25 - thirteen of fourteen items landed (BL-610..BL-618, BL-620, BL-621, BL-623, BL-624); v0.1.19 recut after the NR-640 verdict wave |
| 20 | The books open, and the start earns its way | Closed 2026-08-26 - GOAL MET AND PROVED. Fifteen items landed; BL-634 measured Ben's own criterion and it holds. Cuts v0.1.21. |
| 21 | The other half of the economy - demand | CLOSED 2026-08-31, wave 0 only (BL-648 guard, BL-649 census). Its remaining waves are RESUMED AS SPRINT 27 on Ben's call - "unpause demand. Let's work on this for sprint 27" - re-planned against sprint 26's census measurement rather than its own original ordering. |
| 22 | UI visibility - batch 1: lenses | Closed 2026-08-28 - the lens batch reviewed and reworked; the remaining element classes become their own sprints (23-27) |
| 23 | UI visibility - batch 2: selection & hover | Closed 2026-08-28 - goal met. Twelve items across three waves; every planned item landed |
| 25 | UI visibility - batch 5: canvases & the zoom ladder | Proposed 2026-08-28 as sprint 26; RENUMBERED to 25 on 2026-08-30 when Ben retired the shell-chrome and startup batches - "Sprint 25 and 27 don't need a revisit, UI items for these are working great." SUPERSEDED 2026-09-02 on Ben's call (archive all prior sprints): never opened. The canvas review it proposed ran instead as sprints 29-30, the baked ground. |
| 24a | UI visibility - batch 3: ledgers | Closed 2026-08-29 - batch 3, the ledgers; goal met and exceeded, and four designed mechanisms found never to have run |
| 24b | UI visibility - batch 3b: the ledgers not yet read | Closed 2026-08-30 - the six unread ledgers reviewed, three rebuilt, and the batch's review queue worked through rather than filed forward. |
| 26 | The world that brakes a leader - a rival worth watching | CLOSED 2026-08-31 AT WAVE 1 on Ben's call, GOAL MET. Spectator mode works and the feed reads; the measurement that wave 1 produced then made wave 2 not worth running, and demand takes priority as sprint 27. |
| 28 | The AI that holds back - the brake, once standing means something | PROPOSED 2026-08-31 on Ben's call, taking the three sprint-26 items whose tuning is blocked on demand. NOT open: it starts when sprint 27 has given standing a meaning. SUPERSEDED 2026-09-02 on Ben's call (archive all prior sprints): never opened. BL-697 landed under sprint 26's close; BL-698 and BL-699 were cancelled with it. |
| 27 | The other half of the economy - demand, resumed | CLOSED 2026-09-02 on Ben's call (archive all prior sprints, sprint 31 opens). Both buyers and the whole operating-loss block landed; what it measured is the reason sprint 31 exists. |
| 29 | The world gets a face - detailed canvas rendering | CLOSED 2026-09-02 on Ben's call (archive all prior sprints). The ground has a mechanism: baked painterly chunks in the C-F direction, stepped zoom with bake tiers, a threaded bake, muted borders. |
| 30 | Canvas texture update | CLOSED 2026-09-02 on Ben's call (archive all prior sprints) with wave 1 landed and merged to main the same day: edges back on the ground, the land tilts at the top rungs. |
| 31 | Long-term market viability - every recipe pays at base price | CLOSED 2026-09-02 on Ben's call, stage one a SUCCESS: the field ends the standard thirty-year lapse with a majority of corps operating-positive (46 of 61, 31 of 55) where it began with four and none, debtors a tenth of the field, median balances climbing, buildings running. The growth half - valued production still declines over the run - is sprint 33. |
| 32a | Gamified generation, 32a - the arc runs, and the instruments that measure it are honest | CLOSED 2026-09-06 at a natural boundary. Five items delivered - the two-span sim, the sweep that measures the real run, the continent time axis, the tick-length audit, and the saturation measure promoted where generation can call it. The remaining 29 carry to 32b, led by the water model. |
| 33 | Long-term market viability, the growth half - the field that keeps producing | OPENED 2026-09-02 on Ben's call as sprint 31's second half. Sprint 31 made the field solvent; this sprint makes it grow. The instrument is campaign_lapse with its debt columns, and the two levers are already filed. |
| 32b | Gamified generation, 32b - the water model, and the reorder that 32a built the instruments for | OPEN 2026-09-06, continuing 32a. RE-PLANNED TWICE the same day: the market batch delivered (BL-774, BL-759, BL-760, BL-770 slice 1 - which returned the NEGATIVE result that the phase 6 objective cannot see a roster), then the WATER MODEL brought back in on Ben call. Scope is now 20 items, d-sum 71, six waves - everything except the loose instruments. Closing BL-755 and BL-756 as subsumed by the water model took the backlog 39 -> 37. |

**Next up.** SPRINT 32a CLOSED 2026-09-06 (5 of 34 delivered - the arc runs and its instruments are honest). SPRINT 32b IS OPEN and carries the remaining 29, led by the water model. Sprint 33 (long-term market viability, the growth half) is also open and untouched by this session. THE NEXT NEW SPRINT IS 34.

**The standing debt out of P1**, worth repeating here because it spans four items: nothing built in that sprint was ever *rendered*. The session ran in a container that cannot build the GUI, so every UI half is compile-clean and arithmetically checked and visually unseen, and no golden was blessed. For a sprint whose own method note is *build it, look at it, then rule*, that is the thing to fix first.

*46 sprints archived cold; 2 open/gated in the hot store.*
