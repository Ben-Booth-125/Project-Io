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

*9 entries — 7 open, 2 resolved.*

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

### NR-788 — Five harnesses are now ad hoc, waiting on Ben to name them as skills
*question · raised 2026-09-06 · from Accumulated across sprint 32a and 32b wave 1. Authoring the check was ours; wrapping it as a skill is his call (CLAUDE.md § Tool creation is skill creation).*

Each of these is a committed, working check that nothing can invoke by name, because .claude/skills/verifier-headless/SKILL.md does not list it. The rule is that authoring a check is ours and naming it as a skill needs Ben's permission, so they sit as 'ad hoc' until he says.

  continent_drift          - the plate time axis (BL-763)
  sim_water_census         - the water figures the whole water model is judged against
  the promoted saturation measure - BL-775
  deposit_origin           - BL-762, asserts the Body phase places no biological deposit
  landscape_score_harness  - BL-770, the phase 6 objective's discrimination check

The cost of leaving them unnamed is not zero: an unnamed check is one a later session does not know to run, which is the same failure mode as a check that does not exist.

**Why it matters.** Two of these guard invariants that only they can see - deposit_origin is the only thing asserting the origin split holds, and landscape_score_harness is the only thing that can tell whether phase 6 has anything to search on.

- Name all five in the verifier-headless skill
- Name a subset
- Leave them ad hoc and run them by hand

> **Recommendation:** Name all five. They are already committed and already green; the skill entry is the only thing standing between them and being run by a session that does not know they exist.

*Files: `.claude/skills/verifier-headless/SKILL.md`*

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

### NR-791 — Coastal territory has no surface that shows ownership, so the water model cannot be checked by looking
*observation · raised 2026-09-06 · from The 2026-09-06 live check of wave 1/2a. Two of three visual requirements were verifiable; this one was not, and the reason is a gap rather than an oversight.*

BL-776 and BL-777 make coastal water OWNED and open ocean unowned. That is measured and green headlessly - the ownership table reads coast 2177 owned, lake 1312 owned, open ocean 0 owned against 15336 unowned, and sim_water_census reports coastal_water 318 / open_ocean 0 with the stored domain disagreeing with the substrate in ZERO cases.

BUT IT CANNOT BE SEEN. Live, in the built app: the hover card over water reports TERRAIN and HABITABILITY ('Ocean, Habitability: 48%') and says nothing about an owner. Clicking a water tile does not update the Selection panel at all. No lens in the nav rail colours territory by nation - the map/book slot is the Generation Ledger.

What IS visible, and it is genuinely encouraging: the province partition extends over water as coherent outlined water provinces, and open-ocean tiles report no owner, which is what the design says. But that is consistent-with, not a demonstration-of.

SO BL-780's done-when has a hole. It asks Ben to authorise a re-bless 'against a stated description of what changed in the world's SHAPE rather than against the hash' - and the single most important shape change, coastal water acquiring an owner, is the one a human cannot currently look at.

**Why it matters.** BL-780 is the wave's single judgement point and it is meant to be judged by looking. If the central change is invisible on every surface the game has, the judgement collapses back onto trusting the harness - which is exactly what BL-780 exists to avoid.

- Add ownership to the water hover card (cheapest - the card already exists and already reads the tile)
- Make water tiles selectable so the Selection panel answers it
- A territory/political lens that colours by owning nation, which would answer it at a glance and serve far more than this item
- Accept it: judge BL-780 on the harness figures and the province outlines, and record that the shape claim was not eyeballed

> **Recommendation:** The hover card, before BL-780. It is the smallest change that closes the hole, the card is already on the tile, and it turns "coastal water is owned" from a number in a log into something a person can point at. The lens is the better long-term answer and should be its own item rather than a rider on the water wave.

*Files: `docs/ui/TOOLTIP.md`, `docs/ui/SELECTION.md`, `docs/ui/LENSES.md`, `docs/generation/PROVINCES.md`*

---

## Resolved

Kept, not pruned: the reasoning is the point. Prune only in a deliberate sweep, once the
answer has landed in an authority doc.

### NR-786 — Phase 6 at 4 years a tick simulates a DIFFERENT economy - take 25 honest years at the quarterly tick instead?
*question · raised 2026-09-03 · from BL-771, auditing what a 16x tick actually rescales, 2026-09-03. The item was filed assuming the tick length was the dial; the audit says it is the wrong one.*

Ben's phase 6 (BL-770) is specified as a 400-year pass at four years a tick. Auditing every rate and duration the economy carries turns up two classes of breakage.

RATES THAT COMPOUND: debt interest is charged per tick and compounds. 16 x 0.015 = 0.240 against a compounded 0.269, so a 4-year tick under-charges debt by ~11% - and more importantly hands a debtor 16 years to trade out of a hole quarterly interest would have closed. Sentiment's two decays have the same shape.

QUANTITIES COUNTED IN TICKS - about fifteen of them, and this is the one that settles it. These change MEANING, not magnitude: a convoy leg (travel_ticks = 1) becomes FOUR YEARS; build_duration_ticks becomes decades; procurement lead (base_lead_ticks = 2) becomes eight years; the deposit depletion taper becomes 32 years; survey transit and scan become decades; and corp_ai's planning horizon silently stretches 16x.

So a 16x tick does not simulate the same economy faster. It simulates one where goods take years to move, buildings take decades to raise and debt is cheap - and phase 6's entire purpose is to find which corporate landscape is VIABLE. It would be measuring viability under logistics the campaign does not have, which is the BL-462 defect (a check measuring a different run) in its most expensive form.

THE ALTERNATIVE COSTS THE SAME. Keep the quarterly tick and simulate fewer years: 100 quarterly ticks is 25 years at 53-71 s per candidate, which is the budget the reorder already assumed. Six candidates in parallel still fit inside 3-6 minutes.

**Why it matters.** It decides whether phase 6 measures the economy the player will actually run. It also decides whether BL-771 (make the tick a parameter) is work at all - if the tick stays quarterly, nothing varies it and that item is cancelled rather than deferred.

- (a) Quarterly tick, fewer years - 25 years per candidate, same budget, honest logistics. Recommended.
- (b) 4-year tick, and fix the ~15 tick-counted quantities to be time-based rather than tick-counted. Correct but large, and it changes the campaign economy too.
- (c) 4-year tick with the durations left as they are, accepting that phase 6 ranks candidates under unrealistic logistics.
- (d) Something between: a 1-year tick, which is 4x rather than 16x and breaks less.

> **Recommendation:** (a). Phase 6 does not need 400 years to RANK candidate rosters, it needs long enough for the ranking to stabilise - and how long that is happens to be measurable, by running one candidate for 200 quarterly ticks and watching when the ranking stops moving. That is the right first experiment for BL-770 and it is cheap. Take (b) only if the 400-year span turns out to carry design weight the ranking actually needs, and note it would change the campaign economy as well as the generation pass.

> **RESOLVED.** RULED (a), the quarterly tick (Ben, 2026-09-03: "go with the quarterly tick"). Phase 6 keeps econ_tick_days at 90 and simulates fewer years rather than rescaling every rate in the economy. Consequences applied the same day: BL-771's R1 (make the tick a parameter), R2 (declare every rate's period) and R4 (the differential harness) are CANCELLED rather than deferred - nothing varies the tick, so a parameter for it is not work. BL-771 closes on its audit, which is what produced this ruling. ONE CONSEQUENCE IS LARGER THAN THE ITEM AND IS RECORDED ON BL-770: at the quarterly tick, 100 ticks is 25 years, not the 400 Ben's point 6 named - so phase 6's SPAN is now the open question, and the arithmetic says the full 1560-1960 span is reachable only if the per-tick cost is fixed first (BL-761). That is a prerequisite relationship, not a neighbouring one.

*Files: `src/core/sim_loop.hpp`, `src/world/budget_system.hpp`, `src/world/supply_system.hpp`, `src/world/recipe_registry.hpp`, `docs/generation/GENERATION_STRATEGY.md`*

### NR-789 — Petroleum is +68% across the seed spread, and peat is down 58% - both honest consequences of BL-765, neither asked for
*question · raised 2026-09-06 · from BL-765 (paleo deposits), building it 2026-09-06. Reported by the implementer rather than tuned away, which is why it is a question and not a defect.*

Two magnitudes moved at economy scale when fossils started reading the past.

PETROLEUM +67.9% (120-seed median 51544.8 -> 86536.2). Per-tile magnitude is UNCHANGED and now carries a belt weight <= 1, so the whole rise is AREA. The old rule gated petroleum on `substrate == barren` - roughly 10% of land, and a bad proxy for an ancient sea. The new one is the lower 40% of land height in a productive belt, which is `ore_fields_for`'s own already-shipped definition of old shelf and basin. Concentration also TIGHTENED (top-10% p05 18.6 -> 54.6), so it is not a smear.

The implementer deliberately did NOT tune the percentile to preserve the number the bad proxy produced. That was the right instinct - reverse-engineering a constant to protect an artefact is how a defect becomes a requirement - but it means the world now carries substantially more oil than any economy tuning has ever seen.

PEAT -58% (7792 -> 3307 raw). The design text says peat comes from PRESENT marsh; the body phase had been reaching it through `sedimentary + scrub` (the old tundra row), and marsh is rarer than scrub. So the new number may simply be the design finally being followed - or the design may have meant the broader ground all along.

**Why it matters.** Petroleum is an Industrial-band input and sprint 33 is tuning the field it feeds. A 68% supply change lands directly on the market work, and it moved for a generation reason rather than an economic one - so whoever reads a price shift next will look in the wrong layer for it.

- Accept both: the old petroleum gate was wrong and the new one is principled; peat now follows its own design text
- Accept petroleum, re-examine peat: the "present marsh" reading may be too narrow
- Re-scope the petroleum percentile against what the economy can absorb, and say so explicitly as an economic constraint rather than a geological one

> **Recommendation:** Accept petroleum and re-examine peat. The petroleum change replaces a proxy nobody defends with the definition the ore-field layer already used, and tuning it back toward the artefact would be exactly the forced outcome the standing rules reject. Peat is the weaker case: -58% rests on reading "marsh" narrowly, and that reading was never explicitly chosen. Either way the numbers should reach sprint 33 BEFORE it tunes prices again.

> **RESOLVED.** RULED (Ben, 2026-09-06): ACCEPT PETROLEUM, RE-EXAMINE PEAT - and a design steer larger than both.

PETROLEUM ACCEPTED at +68%. The old substrate == barren gate was a bad proxy nobody defends; the
lower-40%-in-a-warm-belt rule is ore_fields_for own shipped definition of shelf and basin. Not
tuned back toward the artefact.

PEAT RE-EXAMINED AND RESTORED, and the re-examination overturned the change. RESOURCES.md
§ Ambient goods authors peat as a PAIR - scrub cover on sedimentary substrate, plains or valley.
BL-765 narrowed it to marsh-only on a reading of the prose (a peat bog is a marsh), which the
resource table does not sanction, and marsh is rarer than scrub - so 58% of the world peat was
lost as a side effect of a comment rather than a decision. The doc rule is restored and MEASURED
BACK: 7930.4 raw against the 7792 baseline, with body-phase peat 0 so the origin split still
holds. If peat should come from marsh, RESOURCES.md Peat row changes first.

THE STEER, and it is the part that outlives this item: "our per-province infrastructure scores
will account for anything which seems too rich." Richness is absorbed by the province sustain
score - area x habitability x ROADS x population - never clamped at placement. Ground is a fact
about the world; what a corporation can DO with it is gated behind infrastructure it has to build.
Written into PROVINCES.md § Richness is absorbed here, never clamped at generation, with the
consequence for measurement stated: a magnitude moving is not itself a defect, and the question is
never "is this number too big" but "does the province score already bound what can be taken from
it".

*Files: `src/world/tile_generation.cpp`, `docs/generation/TILE_GENERATION.md`, `docs/economy/RESOURCES.md`*

