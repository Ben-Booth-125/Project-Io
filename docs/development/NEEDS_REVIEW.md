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

### NR-792 — Coastal water is 100% owned while land is 39% owned - the carve claims water by GROWTH, not from the shore the doc says owns it
*question · raised 2026-09-06 · from Lane D's water_ownership_census (BL-780 prep), 2026-09-06. The harness passes; this is what it measured on the way past.*

MEASURED over 4 seeds, on main: coastal water 6680 seen / 6680 owned, lake 4992 / 4992, open ocean 0 of 65963 owned - and LAND 48689 owned of 125969, or 39%. Water ownership is TOTAL; land ownership is not.

THE DOC SAYS OWNERSHIP IS DERIVED FROM THE SHORE. docs/generation/PROVINCES.md § Who owns water: 'Coastal water belongs to whoever owns the shore... A province's owner is derived from its tiles, so a coastal province becomes owned the moment its tiles are claimed.'

THE CODE DOES SOMETHING ELSE. src/world/nation_generation.cpp:804 marks ONLY open ocean `unclaimable`; coast and lake are ordinary claimable ground for the carve's growth. So coastal water is claimed by the SAME FLOOD that claims land - and it is claimed more completely than land is. With 61% of land unowned, there must be coastal water whose adjacent shore carries no nation and which is nonetheless owned.

So the causal story in the doc - shore first, water derived - is not the mechanism. The mechanism is: water is cheap to grow across and the flood reaches all of it.

**Why it matters.** Two reasons, and the second is the one with a deadline.

(1) It is a doc-vs-code disagreement on the WATER MODEL'S CENTRAL CLAIM, in the same week BL-780 asks Ben to authorise a re-bless of that model by looking at it. The standing rule is that one of them is wrong and the fix is work, not a footnote.

(2) It changes what BL-778's legality test MEANS. 'A land stack may cross coastal water its polity owns' still works - owned by SOMEONE is not owned by YOU. But there is currently NO NEUTRAL COASTAL WATER anywhere in any world, so the rule 'you may walk your own shore, not someone else's' never encounters unclaimed shore. If the doc's reading is the intended one, a large fraction of coast would be unowned and the water model would bite considerably harder.

- The DOC is right, the code is wrong: derive coastal water's owner from its adjacent shore, leaving water unowned where the shore is. Produces real neutral coastal water and moves every carve again.
- The CODE is right, the doc's phrasing is loose: water is claimed by the same growth as land, and 'belongs to whoever owns the shore' is an approximation to be reworded.
- Neither yet - accept it for this wave, record it against BL-780's shape description, and decide after the water model is complete and can be judged whole.

> **Recommendation:** The THIRD for this wave, then the first. Changing the carve now adds a FIFTH world-moving cause to a re-bless that already carries four, which is exactly what BL-780 exists to prevent - and lane A is mid-flight against the current world. But the first option is very likely the real answer: neutral coastal water is the thing that makes 'you may walk your own shore, not someone else's' mean anything, and 100% ownership quietly deletes it.

*Files: `src/world/nation_generation.cpp`, `docs/generation/PROVINCES.md`, `tools/verify/water_ownership_census.cpp`*

---

## Resolved

Kept, not pruned: the reasoning is the point. Prune only in a deliberate sweep, once the
answer has landed in an authority doc.

