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

*6 entries — 5 open, 1 resolved.*

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

### NR-801 — Rounds 4 and 5 break the wizard promise that nothing is generated in it
*decision taken on your behalf · raised 2026-09-08 · from Writing the sprint 35 visibility design into STARTUP.md and GENERATION_STRATEGY.md.*

STARTUP.md has said since BL-167 that NOTHING is generated in the wizard - every control move re-runs the chain as a pure throwaway preview and m_world is untouched. Rounds 4 and 5 cannot honour that: the history sim is the most expensive pass in the project. So I wrote them as running the REAL pass inside the round, on a Run press, drawing as it computes - the doc now says the preview model does not transfer.

**Why it matters.** That promise is not decoration. It is what makes Back a plain revision with no snapshot, and what lets the wizard reroll freely. Rounds that generate for real make Back across round 4 expensive and make a reroll a minute rather than a frame. Your "a watched wait needs no budget" licenses the wait; it does not by itself choose this mechanism over the alternative, which is to collect leans against an illustrative preview and run everything at Begin as today.

- The pass runs inside the round, on a Run press - the wait is the content (what I wrote).
- Leans are collected against an illustrative preview; the real passes still run at Begin behind the loading screen.

> **Recommendation:** Keep what I wrote. The second option gives the player a lean whose effect they cannot see when they set it, which is the one thing the planetology rounds get right and the reason the wizard works at all.

*Files: `docs/ui/STARTUP.md`, `docs/generation/GENERATION_STRATEGY.md`*

### NR-803 — The 1200 - 1560 gap is unsimulated, and I have written it up as a deliberate coast
*decision taken on your behalf · raised 2026-09-08 · from Your two spans: pass 1 ends at 1200 CE, pass 2 runs 1560 to 1960.*

That leaves 360 years nothing steps. I have written it into GENERATION_STRATEGY.md and HISTORY.md as a deliberate COAST rather than a gap: pass 1 ends in a stable dark age, a span defined by little changing is not worth simulating, and the world arrives at 1560 holding what 1200 left it.

**Why it matters.** It reads as intentional and it fits the arc you named - a stable dark age is precisely a span where skipping loses little. But you did not say it, I inferred it, and the alternative reading is that pass 1 should run to 1560 and 1200 was approximate. The two differ by 360 years of assimilation and grudge decay, which are the accumulators that move on their own even when borders do not.

- A deliberate coast, with self-moving accumulators advanced cheaply across it (what I wrote).
- Pass 1 runs to 1560 and the two spans meet.
- 1200 was approximate; the boundary is wherever the measurement says it can afford to be.

> **Recommendation:** Keep the coast. It is the cheapest span in the whole calendar and the one where the least is lost - but BL-831 has to name which accumulators cross it, because silently dropping 360 years of assimilation would undo the lever that makes conquest fragile.

*Files: `docs/generation/GENERATION_STRATEGY.md`, `docs/lore/HISTORY.md`*

### NR-804 — The wizard has no entries in the action dictionary, and the five rounds did not add any
*question · raised 2026-09-08 · from Merging the wizard shell (BL-816, BL-824) and verifying it live.*

CLAUDE.md says any control change updates its ACTIONS.json entry. The wizard gained two rounds, a per-round Reroll, and a Begin that moved. None of it was recorded, because ACTIONS.json has no startup family at all - its dictionary is gameplay.* and canvas.* only.

**Why it matters.** Either the pre-game wizard is deliberately outside the action dictionary - defensible, since the dictionary exists for an agent playing the game and nobody plays the wizard - or it is a gap that has been quietly widening since the wizard was built. The rule as written does not distinguish, so every session touching startup will face this again and answer it differently.

- The wizard is out of scope for ACTIONS.json; say so in the doc so it stops being a question.
- Add a startup.* family and backfill the wizard.

> **Recommendation:** Out of scope, stated explicitly. The dictionary is the AI seam and no agent presses Begin; a startup family would be inventory nobody reads. But it needs writing down, because silence is what made this a question.

*Files: `docs/ai/ACTIONS.json`, `CLAUDE.md`*

### NR-805 — Profiling counters now live inside world/, and no doc owns that
*novel-work · raised 2026-09-08 · from Raised by the BL-825 agent about its own task; filed by the main session.*

BL-825 added history_sim_profile / history_sim_last_profile() - a report-only wall clock and rebuild counters living next to sim state in src/world/history_sim.hpp.

**Why it matters.** world/* is the deterministic core, and a wall clock is the least deterministic thing there is. The precedent it followed is real and close - era_minus_one.hpp already argues exactly this case for ms_era, kept off the save seam and out of every digest - and the instrumentation was proven output-neutral empirically rather than by assertion. But no doc owns the general question, so the next session adding a counter has the same argument again from scratch.

- Write the rule into DEVELOPMENT_PRACTICES.md: profiling state in world/* is permitted if it is off the save seam, out of every digest, and proven output-neutral by world_determinism.
- Keep it case-by-case.

> **Recommendation:** Write it down. The constraints are already clear from the two instances, and a stated rule is cheaper than a third argument.

*Files: `src/world/history_sim.hpp`, `src/world/era_minus_one.hpp`, `docs/development/DEVELOPMENT_PRACTICES.md`*

---

## Resolved

Kept, not pruned: the reasoning is the point. Prune only in a deliberate sweep, once the
answer has landed in an authority doc.

### NR-802 — A 4000-year pass 1 ending at 1200 CE contradicts prehistory_years = 400 and epoch_year = 0
*question · raised 2026-09-08 · from Your description of round 4 against the live app: a time-lapse of the first 4000 years, ending at 1200 CE.*

The code defaults are prehistory_years = 400 (hard_coded_world.hpp:73) and epoch_year = 0 CE, with era_band_for_epoch flipping to industrial at 1700. HISTORY.md carries the campaign epoch as 0 CE (your ruling, NR-177), and GENERATION_STRATEGY.md sets the pass 1 / pass 2 boundary at 400 years before the epoch. A 4000-year pass 1 ending at 1200 CE fits none of those three numbers.

**Why it matters.** It is not a naming mismatch. It reads as a re-basing of the whole calendar: pass 1 becomes 2800 BCE to 1200 CE, pass 2 runs 1200 CE to an epoch that must now be well past 1700 - which puts the campaign in the industrial band rather than the ancient one, and that band choice reaches into eras, resources and the military roster. It also multiplies the most expensive pass in the project by ten, against your other requirement for this round, which is that it be rapid.

- The epoch moves late (a modern arc) and 1200 CE is the pass 1 / pass 2 boundary year. Pass 1 is 4000 years, pass 2 is 1200 to the epoch.
- The epoch stays at 0 CE and 1200 CE is wrong - the time-lapse ends at the epoch, whatever it is.
- 4000 years is the SPAN and 1200 CE is illustrative; the real numbers fall out of the cost measurement.

> **Recommendation:** Option 1, because it is what you actually described and the arc you named - through to a stable dark age - is a medieval end point, not a classical one. But it moves the campaign into the industrial era band, so it is not a small consequence and I am not taking it on your behalf.

> **RESOLVED.** RESOLVED 2026-09-08 by Ben the same day: "another focused pass on economy from 1560 to 1960". That is option 1 - the epoch moves late. The calendar is now stated rather than derived: 4000 years to 1200 CE, a coast to 1560, then 1560-1960, epoch 1960. Written into GENERATION_STRATEGY.md § Pass 2 is the economy pass and lore/HISTORY.md § The epoch and the run. The consequence I flagged holds and is now carried by BL-831: an epoch of 1960 puts the campaign in the INDUSTRIAL era band, not the ancient one, and that owes a deliberate digest re-bless.

*Files: `src/world/hard_coded_world.hpp`, `src/world/era_band.hpp`, `docs/lore/HISTORY.md`, `docs/generation/GENERATION_STRATEGY.md`*

