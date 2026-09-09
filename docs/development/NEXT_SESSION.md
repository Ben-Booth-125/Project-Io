# Next session — sprint 37 is closed; the roads pass is what blocks a playable world

Written 2026-09-09 at the close of sprint 37. **Sprint 37 is complete and archived.** The
migration span exists, runs inside the recorded history, and has its own wizard round.

## The one thing that blocks everything

**The roads pass crashes the full generation pipeline (Ben, 2026-09-09).** `--autostart-windowed`
walks the wizard, presses Begin, and never reaches in-game — the frames-rendered line is absent.
Until that is fixed there is no playable world at the end of the wizard, however good the rounds
look.

**Do not read `exit=0` as success on that command.** It exits cleanly via a window close; the
proof of success is the line `[autostart-windowed] OK  N in-game frames rendered`. Grep for it.

---

## What the wizard is now

Five rounds: **System · Life · Culture · Empires · Industrialisation**. Each pass round starts its
own pass on arrival — there is no Run button, because arriving is the instruction. Reroll varies
the history without disturbing the planetology above it (`world_params::era_seed`).

Rounds **Culture** and **Empires** still replay the **same recorded age**, and both say so on
screen. The rounds are split; generation still emits one span. **Splitting it is now
BL-871 (empire span 400 BCE to 1200 CE), filed 2026-09-09.**

## The Empires design pass closed 2026-09-09

Six calls settled by elicitation, written into `docs/generation/CIVILISATION.md`:
**span 400 BCE → 1200 CE** · **a settlement is a seat flag on a region**, hinterland by pointer ·
**stores sit at the seat and fall with it** · opposition from **pantheon temperament + origin farm
class**, not contact · **a civilisation is a named record** with an ethic · **reach GATES a
campaign** rather than pricing it.

Ben's outcome brief is the frame for all of it: a **sparse road network** connecting city states,
forming empires, giving population centres *derived by possible supply and governance* — then
empires that persist, expand reach, and **collapse into smaller nations ready for an industrial
boom**. That last clause makes non-hegemony an input requirement for pass 2, not just a
watchability one (BL-823).

New items: **BL-871** (empire span), **BL-872** (centres from supply and governance), **BL-873**
(culture coining year — small, and it blocks BL-870). Amended: **BL-837** (reach gates),
**BL-823** (the ending), BL-866, BL-867, BL-869, BL-870.

**The calendar, restated (Ben, 2026-09-09, confirming NR-818 and revising the numbers).** Pass 1
covers **3,600 years, 2400 BCE → 1200 CE**, divided at **400 BCE**: Culture 2,000 years, Empires
1,600. **1200 CE is unmoved**, so the coast to 1560, pass 2 and the 1960 epoch are untouched. The
Culture round keeps its derived ending and the world **coasts** to 400 BCE.

**One claim from the first cut was false and is gone:** 400 BCE is *not* a year the engine already
knows — the ancient arc ends at 0 CE — so the split is real work, not free. NR-818 is resolved.

---

## Numbers to quote rather than re-derive

All `build_gen` (/O2) unless stated. **Debug timings are not comparable; quote the tree.**

| | start of sprint | end |
|---|---|---|
| Regions at the epoch | 609 | **1,906** |
| Ownership changes | 533 | **4,432** |
| Distinct peoples holding ground | 12 | **38 / 56 / 33** |
| Route divergence from a Voronoi | — | **23–33%** |
| Conquests in the span | 0 | ~472 transfers |

**Regions are ~3× where the day started, and that is the live cost risk.** NR-809 measured reach as
roughly quadratic in region count; **BL-844 is already spent**, so no optimisation is waiting — the
cap is the **adjacency model** (BL-855), which is a sprint of its own.

---

## Open calls, none of them work

| Entry | Question |
|---|---|
| **NR-809** | Region count vs the adjacency model. Sharper now that regions tripled. |
| **NR-811** | Round pays a world build and Begin pays another. |
| **NR-812** | Does the wizard's `ACTIONS.json` exemption reach a Run button? (Run is now retired, so this may be moot.) |
| **NR-813** | A scrubber for the time-lapse. Defer until the spans are actually split. |

Open items worth pairing: **BL-859** (most land habitable — its premise was overturned by the
unfarmed-class census: the shortfall is polar ice and mountains, so the lever is planetology or a
cold-affinity package, **not** the affinity floor) and **BL-861** (no conquest — which today's
polity fix may have resolved as a side effect; **measure before believing it**).

---

## Traps this sprint paid for, all of them the same shape

**A check that looks like coverage and is not**, six times:

- **Never pipe a harness into `grep`.** A pipeline returns the *filter's* status, and a segfault
  read as `exit 0`. Run to a file, then read `$?`.
- **`--verify` adopts the harness's own world** on the lapse rounds, so the wizard's *stopped*
  generation path has no coverage at all. That shipped an empty map for 4,000 years while the
  header reported 611 foundings.
- **`--debug` on `build_harness.js` does not set `_ITERATOR_DEBUG_LEVEL`.** An out-of-bounds write
  passed both harness builds and aborted only in the app's real CMake Debug build, with exit 3 and
  no message.
- **A `static_assert` can be far too loose to fire** — `sizeof(app)`'s bar is ~6× the real limit
  while the process died at startup.
- **A synthetic case can assert a tie.** C3a's two cradles were equidistant under the wrapped
  metric.
- **A classifier can measure the wrong thing entirely** — "how much ground" measured crowding, not
  encirclement.

**What found the real defects was driving the app.** Three separate times, behind green harnesses.

**And a comment can hold an invariant nothing checks.** *"Sized to the polity table, which the sim
never grows"* was true when written and false the moment a polity could be born mid-run.

---

## Reproduce

```
node tools/verify/build_harness.js colonisation_harness && build_gen/verify/colonisation_harness.exe 3 4000
node tools/verify/build_harness.js world_determinism    && build_gen/verify/world_determinism.exe
build/ProjectIo.exe --verify scripts/verify/history_lapse_press.lua
build/ProjectIo.exe --autostart-windowed     # grep for "in-game frames rendered"
```

**Baselines:** `colonisation_harness` 35/0. `world_determinism` 0 failures.
`history_lapse_press` **has 4 failures** — the round renumbering moved the pass-round footer, and
the coordinates must be **read off a capture, never guessed**. It fails loudly rather than silently
only because `verify.wizard_round()` reads the round back.
