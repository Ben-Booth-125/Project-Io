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

*3 entries — 3 open, 0 resolved.*

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

---

## Resolved

Kept, not pruned: the reasoning is the point. Prune only in a deliberate sweep, once the
answer has landed in an authority doc.

