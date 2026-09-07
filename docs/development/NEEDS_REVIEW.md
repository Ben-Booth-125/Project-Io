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

*6 entries — 6 open, 0 resolved.*

---

## Open

### NR-783 — The span boundary: an authored 400 years before the epoch, or the year the first polity lights a furnace?
*question · raised 2026-09-03 · from BL-747 (two-span prehistory), building it 2026-09-03. The design doc records the authored offset as the default and the derived one as the better-founded alternative; the code now has the authored one.*

The two spans need a boundary year. Built as authored: world_params::industrial_years = 400, so the boundary is epoch - 400 and a 1960 arc runs 1160 -> 1560 -> 1960.

(a) KEEP THE AUTHORED OFFSET. One number, same on every seed, trivially reproducible. But it says every world industrialised in the same year, which is exactly the flat outcome the asymmetry ruling (2026-08-31) tells generation not to produce.

(b) DERIVE IT from the first furnace - the year a polity crosses the Industrial rung of the capacity ladder inside the run (BL-748 puts that event inside the sim). Then the boundary is a CONSEQUENCE of endowment, per seed, and the span structure varies the way everything else in generation does. Costs: the boundary is not known until the run reaches it, so the tick-band table cannot be built up front, and the run has to switch bands mid-flight from a state-derived predicate - which must be a pure function of sim state or it is a die roll wearing a timestep.

(c) HYBRID - authored offset as a backstop, derived if the furnace lands first. Keeps a bound on the industrial span while letting an early-industrialising seed have a longer one.

**Why it matters.** It decides whether "when did this world industrialise" is a fact about the world or a constant. It also sets the cost profile: a derived boundary makes the industrial span variable-length, and the industrial span is the expensive one.

- (a) authored offset, as built
- (b) derived from the first furnace
- (c) hybrid: authored backstop, derived if earlier

> **Recommendation:** (b) eventually, (a) for now. The authored offset is in and measurable today; deriving it is a small change once BL-748 puts the furnace inside the run, and it is the option consistent with the asymmetry ruling. Do not build (c) unless the measurement shows the derived span running away.

*Files: `src/world/era_minus_one.cpp`, `src/world/hard_coded_world.hpp`, `docs/lore/HISTORY.md`*

### NR-784 — The ancient arc keeps its unrestricted band ladder, because capping it would move every 0 CE world
*decision taken on your behalf · raised 2026-09-03 · from BL-747 (two-span prehistory), 2026-09-03. Found by implementing, not by reading: the design as written implied a cap the invariant forbids.*

The design says pass 1 runs the Classical and Medieval bands. Read literally, that caps the roster band on the ANCIENT arc too, since there the whole run is pass 1. It cannot be done: today a 0 CE polity may climb the capacity ladder to gunpowder or industrial, and capping it changes the decision set, the argmax, and therefore every 0 CE world - which BL-747's own acceptance test (world_determinism digests 039EE9880739CDF6 / B0EBBA249B3DDABB / DE55600457797638) forbids.

TAKEN, so the work could proceed: the ceiling is OPT-IN. history_sim_params::span1_band_ceiling defaults to `industrial` (no restriction) and boundary_year to INT64_MIN (no year is before it), so a single-span run is inert twice over and the ancient arc executes the same values through the same code. Generation sets the medieval ceiling only on an epoch that has an industrial span.

WHAT IS LEFT FOR BEN, and it is a real question: SHOULD the ancient arc be capped at medieval? A polity fielding gunpowder units in 200 BCE is an anachronism the ladder currently permits, and the works roster has the same hole. If the answer is yes it is a deliberate re-blessing of the 0 CE digests, not a bug fix - and it should be taken as its own item with its own before/after, never folded into BL-747.

**Why it matters.** It is the difference between a change that cannot move the ancient world and one that quietly does. It also names an anachronism nobody has looked at: what band a 400 BCE polity can actually reach in 100 rounds is unmeasured.

- Leave the ancient arc uncapped (as built)
- Cap the ancient arc at medieval too, as a separate item with a deliberate digest re-bless
- Measure first: report the band distribution a 0 CE run actually reaches, then decide

> **Recommendation:** Measure first. history_sweep can report the highest band reached per polity on the ancient arc; if nothing ever passes medieval the question is moot and the cap is free.

*Files: `src/world/history_sim.hpp`, `src/world/history_sim.cpp`, `src/world/era_minus_one.cpp`*

### NR-785 — BL-749 (sea legs) is blocked on five design calls, and its premise was wrong - it prices an existing capability rather than adding one
*question · raised 2026-09-03 · from The sprint 32 subsystem map, 2026-09-03, scoping BL-749 for wave 1. It came out under-specified and was held back rather than guessed at.*

Two facts invert the item and are filed as their own work: the sim ALREADY campaigns across up to nine tiles of open water for free (BL-755, region adjacency is water-blind Chebyshev at radius 9), and it ALREADY founds regions on ocean with no terrain test (BL-756, where terrain_combat returns 0 defence, so such a region is silently undefendable). So the "before" number for any sea-leg measurement is not zero, and every tuning constant in history_sim_params was measured with free overseas conquest happening.

The five calls, all recorded in full on BL-749: which walk (coast tiles only, or all is_sea) and how the four harbour rows map onto ranges, given nothing in the code labels a sea "enclosed"; where the walk is anchored, since a region has no harbour tile and adding one is a save-format change; how "holds a harbour work" is tested, since work_row has no harbour flag; whether the naval roster rows (Coastal Galley, Broadside Ship, Ironclad - all base power 0 and skipped by sum_stack today) gain real power here or stay inert; and whether an overseas daughter region re-surveys port_q instead of inheriting 0.7x of its parent's.

The good news is in the same map: the sim already holds the coastline (sim_terrain_view::substrate carries ocean/lake/coast), so a deterministic coastal walk needs no signature change, no world& and no ECS access.

**Why it matters.** BL-749 is the item that produces "the extent of colonisation by major powers", which is half of what the sprint is for. Building it on the stated premise would have priced a capability that already exists and asserted a gate the Settle verb does not have.

- Answer the five calls on BL-749 and build it in wave 2
- Take BL-755 and BL-756 first, measure how much free overseas conquest actually happens, then decide the design against that number
- Park BL-749 for this sprint and deliver the two spans, the furnace and the tariff only

> **Recommendation:** The middle one. The measurement is cheap, it is needed for the before/after either way, and the answer to "how much is already happening" changes several of the five calls.

*Files: `docs/development/backlog.json`, `src/world/history_sim.cpp`*

### NR-787 — A stagnant lid is IMMOBILE in the paleo frame - taken on doc authority, and it made a red harness row green
*decision taken on your behalf · raised 2026-09-06 · from BL-764 slice 1 (the paleo query), building it 2026-09-06. Found by implementing: the first pass wound stagnant ground back and P6 failed.*

run_continents draws a position, direction and SPEED for every plate, and THEN early-returns when plate_count == 1. So a stagnant-lid body carries a drift vector that nothing consumes.

The paleo query (BL-764) has to decide what that vector means. Winding stagnant ground back along it made harness row P6 FAIL. The implementer instead made a stagnant lid immobile in the paleo frame, on the authority of CONTINENTS.md ('interior locked into a single stagnant plate') and of continent_snapshot_at, which already early-returns for the same case.

WHY IT IS FLAGGED RATHER THAN FILED QUIETLY: the shape 'a check went red, and the fix was to change the model so it goes green' is exactly the shape of weakening a test, even when it is not one. Here the reasoning is that the harness was asserting against a drift the design says does not exist - the vector is vestigial data, not a modelled motion. That reading looks right and it is not mine to confirm.

The alternative reading: a stagnant lid DOES drift as one piece, the vector is meaningful, and P6 was correct to fail - in which case the paleo frame owes stagnant bodies a real wind-back.

**Why it matters.** It decides whether ~a third of generated bodies (every stagnant lid) have a past position at all, which is the input BL-765 pins paleo deposits to. If stagnant lids are immobile and should not be, their coal and oil land in the wrong place and nothing will say so.

- Confirm: a stagnant lid is immobile in the paleo frame (as built)
- Overturn: a stagnant lid drifts as one piece, and the paleo frame must wind it back
- Delete the vestigial vector instead, so nothing can read a motion that is not modelled

> **Recommendation:** Confirm, and take the third option alongside it. The doc is unambiguous that a stagnant lid is locked, and continent_snapshot_at already agrees - so the model is consistent and the harness row was the outlier. But leaving an unconsumed drift vector on the plate is what created the ambiguity in the first place; deleting it makes the next reader unable to make the same mistake.

*Files: `src/world/continents.cpp`, `docs/generation/CONTINENTS.md`, `tools/verify/continent_drift.cpp`*

### NR-790 — BL-765 authored WHEN a fossil forms, because no doc owned the question
*decision taken on your behalf · raised 2026-09-06 · from BL-765 (paleo deposits), 2026-09-06. Flagged by the implementer as novel: nothing in its reading list said when coal forms relative to the drift record.*

The paleo query answers where a tile WAS at age T. It does not say which T a coal seam should be read at, and no authority doc does either.

TAKEN, so the work could proceed: the formation epoch is derived from the two windows the planetology chain ALREADY computes - `land_burial_gyr` for coal, `marine_anoxia_gyr` for oil - each clamped against its own chain step (S7 / S6) rather than extrapolated, and mapped across `continent_drift_epochs`. Oil is forced never shallower than coal, from the chain's own ordering: anoxia opens at oxygenation, burial only after land. Written into TILE_GENERATION.md § The Life phase.

It is defensible and it is still AUTHORED. A different mapping - a fixed epoch, or one keyed on biosphere peak rather than burial - would place every seam somewhere else on the same world, and nothing in the repo would object.

**Why it matters.** It decides where every coal and oil deposit on every generated world sits, and it is the first rule in the generator that reads the drift history for anything. Every later paleo consumer will copy its shape.

- Confirm the derivation as authored (windows from the chain, clamped, oil never shallower than coal)
- Key the epoch on something else - biosphere peak, or a fixed age per fossil
- Leave it derived but expose the mapping as tunable data rather than code

> **Recommendation:** Confirm. It reuses quantities the chain already derives rather than inventing a constant, and the oil-after-coal ordering follows from the chain rather than from taste. The third option is worth taking later if a second fossil family arrives - at one coal and one oil it would be a knob with no second reader.

*Files: `src/world/tile_generation.cpp`, `docs/generation/TILE_GENERATION.md`*

### NR-793 — Road tier is INVISIBLE to the phase 6 objective - confirmed on a live ten-market world; the cause is a saturated resource-coverage boolean
*question · raised 2026-09-07 · from Lane C (BL-770 slice 3, the search), measured by tools/verify/landscape_search_harness.cpp, 2026-09-07.*

ROAD TIER 1, 2 AND 3 SCORE BIT-IDENTICALLY on every term of the objective. Verified independently on the harness here: every road_tier proposal in a four-round walk returned the incumbent's exact composite (0.007301, then 0.008397), never once differing in the last digit.

IT IS NOT A NO-OP IN THE WORLD. The harness carries a fixture control: the base field is tier1=2973 / tier2=173 / tier3=0, and applying road_tier=3 genuinely raises 3146 tiles to Highway. The tiles change; the score does not.

THE CAUSE IS STRUCTURAL. A road tier scales traversal COST (x0.67 / x0.50 / x0.40). The objective reads catchment MEMBERSHIP and terminal CLOSURE - both booleans - and this world's reach field never flips one on a cost discount. So the axis moves a continuous quantity the objective only reads through a threshold.

Ben's point 3 named three axes: rosters, placements, road tiers. On this world only PLACEMENT actually moved the winner (+15.0% composite, entirely through that axis). SECONDARY, from the same run: corps=10 scores bit-identically to corps=8, while corps=6 and corps=7 differ - so the roster axis is live but COARSE, and extra corps close no additional terminals past a point.

--- MEASURED 2026-09-07 (Ben: 'run it'), AND IT SPLIT THE FINDING IN TWO ---

THE DECISIVE NUMBER, summed over every market's row, either side of the tier-3 uplift:

  road_tier=1   catchment=31581   IN_REACH=22875   raws_in_reach=32
  road_tier=3   catchment=31581   IN_REACH=23148   raws_in_reach=32

SO THE REACH FRONTIER DOES MOVE: +273 tiles, +1.2%. That REFUTES the first explanation offered - that roads sit where the economy already is and the 24.0 budget's frontier is out where there are no roads to upgrade. The tier genuinely pulls ground inside the budget.

WHAT DOES NOT MOVE IS WHAT THOSE TILES CONTAIN. raws_in_reach is 32 before and 32 after. The objective's closure question is per-resource and BOOLEAN - 'is there ANY reachable deposit of resource R in this catchment' - and with 32 resources already found across 22,875 reachable tiles, 273 more tiles cannot introduce a 33rd. The axis moves a continuous quantity that the objective reads only through a saturated boolean.

--- BUT THE FIXTURE IS DEGENERATE, AND THAT UNDERMINES THE WHOLE FINDING ---

The search and both axis tests run on ONE fixture world, and on that world:
  markets            2      (the control worlds carry 10, 10 and 15)
  balance            0.00000 on EVERY candidate - the harness itself prints 'ALL CANDIDATES ZERO - the term is DEAD, not flat'
  composite          0.000000 on every candidate, seed and winner alike
  spread             0.04348 flat (against 0.368, 0.788, 0.408 on the controls)

So term 2 is dead and term 3 is nearly dead ON THIS FIXTURE, and the composite the axis test compares is identically zero. Concluding 'the road axis is invisible to the objective' from a world where the objective evaluates to zero for every input is not sound. The search still ranked and improved (realised 0.326 -> 0.500) only because compare_landscape is LEXICOGRAPHIC and fell through to the realisation term.

--- CONFIRMED ON A LIVE WORLD 2026-09-07 (Ben: 're-run the axis test on a 10-market world'). THE FINDING STANDS ---

Seed ABCDEF01, ten markets, every term alive - balance 0.05238, spread 0.36781, composite 0.010256174, none of them zero:

  tier=1  mkts=10  IN_REACH=25658  raws=122  composite=0.010256174
  tier=3  mkts=10  IN_REACH=26024  raws=122  composite=0.010256174

IN_REACH moves +366 tiles. Every scored term is bit-identical. So the degenerate fixture was NOT what hid the axis - the objective genuinely cannot see a road tier, on a world where it can see everything else.

THE CAUSE IS NOW PINNED, and it is one number: raws_in_reach is 122 either side. The objective asks a per-resource COVERAGE question - is there any reachable deposit of resource R in this catchment - and with 122 resource-market pairs already covered across 25,658 reachable tiles, 366 more tiles introduce no pair that was not already covered. The coverage boolean is SATURATED, so a cost discount can never reach the score.

THE FIXTURE PROBLEM IS SEPARATE AND STILL REAL. The default-seed fixture (2 markets, balance and composite identically zero) is not a sound basis for any phase 6 measurement, and slice 1's negative result and slice 2's discrimination figure were both taken on it. That is now its own question rather than a confound on this one.

**Why it matters.** A third of every round's work is spent proposing a change that cannot be scored. That is not just waste: it makes the search look like it explores three dimensions when it explores two, and a later session reading the doc would believe road tier is being optimised.

It also echoes the slice-1 finding exactly, one level up. Slice 1 found the objective blind to ROSTERS and the fix was a new term (realisation). This is the same shape - an axis the objective cannot see - and the same two exits are available.

--- WHAT THE 2026-09-07 MEASUREMENT CHANGES ---

The road-axis question is now SECOND in line. The first question is why the phase 6 fixture is a 2-market world on which two of four terms evaluate to zero, when three control worlds at other seeds carry 10-15 markets and a live composite. Every phase 6 finding so far - slice 1's flat negative, slice 2's discrimination figure, and this axis test - was taken on that fixture.

- FIRST fix the fixture: re-run the axis test on a world with 10-15 markets and a non-zero composite, then re-read the road finding. Nothing should be ruled until the objective is non-degenerate.
- Give the objective a term that can see a cost discount - it WOULD see this one, now that +273 in-reach tiles are measured. The earlier 'no term could see it' reading was wrong.
- Drop road tier as a candidate axis and say so in GENERATION_STRATEGY.md - two axes, honestly, rather than three where one is inert.
- Change what a road tier DOES so it can flip a boolean - e.g. tier affects catchment reach, not only cost. Largest blast radius; touches LOGISTICS.md.

> **Recommendation:** OPTION 2 or OPTION 3, and the measurement now supports choosing between them rather than guessing. Option 2 is VIABLE - the reach field moves by 366 tiles, so a continuous term (mean traversal cost to market, or in-reach tile COUNT rather than coverage) would see the axis immediately. Option 3 is the honest cheap answer if infrastructure is not something phase 6 should be selecting at all. What is now ruled OUT is 'leave it as-is': the axis costs a third of every round and provably buys nothing, on a live world as much as on a degenerate one.

SEPARATELY AND FIRST: the 2-market fixture should stop being the phase 6 measuring world, whatever is decided about roads.

*Files: `src/world/landscape_score.cpp`, `src/world/landscape_search.cpp`, `docs/generation/GENERATION_STRATEGY.md`*

---

## Resolved

Kept, not pruned: the reasoning is the point. Prune only in a deliberate sweep, once the
answer has landed in an authority doc.

