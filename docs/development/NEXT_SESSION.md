# Next session — sprint 37, the colonisation span is half-wired

Written 2026-09-09 at the close of the build session that followed the design pass.
`docs/generation/COLONISATION.md` is still the authority and none of it was re-opened.

## What landed, and what it is verified by

| Item | State | Evidence |
|---|---|---|
| **BL-817** (playback record) | **complete** | `world_determinism` ALL PASS · `save_envelope_roundtrip` PASS · 9 new assertions |
| **BL-848** (culture by route) | **built, acceptance test green** | `colonisation_harness` C11 on real worlds |
| **BL-846/847/850** (span, package, predation) | **mechanisms built, not yet wired into the sim** | `colonisation_harness` 28 assertions, 0 failures |
| **BL-851** (cradle outcomes) | **built** | C10a–f + a real-world census |
| **BL-829/830** (round 4) | **merged, press-check green** | `history_lapse_press.lua`, 5 assertions |

`src/world/colonisation.{hpp,cpp}` is the whole span as pure functions over a tile raster.
`run_settlement` now takes every founding's **date and culture** from it.

---

## The one thing that decides the next session

**Round 4 still cannot show the arc, and it is now precisely diagnosed.** Two causes, and only
the first is a decision:

1. **The sim watches 400 years of a 4,000-year story.** `prehistory_years = 400`, so by the time
   the record opens, every region already exists. Fixing it means the sim spans the whole 4,000
   years and settlement hands it a **founding schedule** rather than a finished map — and that
   makes every world build pay ~6.5 s of era instead of ~60 ms, **which every harness in the
   project then pays**. That cost is the decision; the code is not hard. **NR-810.**
2. **The flood fills the map in ~1,600 years, not 4,000.** Founding years span `-4000..-2378`
   (seed 0), `-4000..-3132` (seed 2). The rate is `colonisation_base_centiyears` = 12 years a
   tile. **It is a MAGNITUDE and COLONISATION.md puts every magnitude here in `history_sweep`'s
   hands** — do not retune it to make a chart look right. That is the whole failure mode this
   layer's calibration rule exists to prevent.

**Do not start (1) without Ben's answer.** It is a project-wide cost, not a local one.

---

## Ben's open calls, all filed and none of them work

| Entry | Question |
|---|---|
| **NR-809** | Region count: cap at ~2–3× this sprint, or chase the sixteen-fold? **BL-844 is already complete**, so no optimisation is waiting — the cap is the adjacency model (BL-855). |
| **NR-810** | The arc gap above. Recommended: retitle now (**done**), wire the span next. |
| **NR-811** | Round 4 pays a full world build and Begin pays it again (~73 s twice, **Debug** — Release unmeasured). |
| **NR-812** | Does the wizard's `ACTIONS.json` exemption reach a Run button? The implementer read the doc as broader than its brief, followed the doc, and flagged it. |
| **NR-813** | A scrubber for the time-lapse. Recommended: defer until there is a real arc to sit through. |

New items: **BL-853** (the sim's terrain view carries no rivers, so the walk prices the coast as
the cheapest route — contradicting the design's own first example), **BL-854** (B384a asserts
every world goes to war; the colonisation design says that is wrong — raise, don't delete),
**BL-855** (the neighbour graph densifies, which is what actually caps the region count).

---

## Measurements taken this session — quote these, do not re-derive them

All `build_gen` (/O2). **Debug timings are not comparable; quote the tree.**

**Region-count sensitivity** (the thing NEXT_SESSION asked for first). Reach cost is
**~quadratic in region count** — exponent 1.95–2.27 across seeds 0 and 1. Seed 0: 0.017 / 0.045 /
0.079 ms per rebuild at 533 / 833 / 1,174 regions. Seed 1: 0.035 / 0.114 / 0.218 at 563 / 931 /
1,260. Reach is 24–37% of a 4,000-year run. **These reproduce BL-844's own closing figures
exactly** — it is the post-fix state, and BL-844 says the residual is the graph densifying, not
the algorithm.

**After culture-by-route:** regions hold on two seeds of three (149→149, 195→182) and thin on the
third (**162→101**, 62% of land reached, 34% farmable). Whether that is emptiness working as
designed or packages coming out too narrow **cannot be told from three seeds** — it is the
sweep's.

**The world changed.** Two-span digest `FD92A6F981DA6E67` → `4FCA63A24E05406D`. `world_determinism`
is still ALL PASS because it compares same-seed runs **to each other** — it asserts determinism,
not stability, and pins no literal. Nothing needed re-blessing. Worth knowing: there is less
pinning here than "the digests" suggests.

---

## Traps, including three new ones

- **A decay coefficient is meaningless without the population range it will meet.** Predation's
  first cut used 90 per doubling, which exhausted its floor at 1,024 heads — so across every
  population the game actually has it was a **constant wearing a decay's name**. It read as a
  tuning choice and was a dead mechanism. Sized to the range, it is 40.
- **A classifier that asks "how much" when it should ask "why".** The cradle-outcome test first
  reported 317 encircled / 186 spread / **0 sterile**, because it asked whether a source got much
  ground. With 149 streams on one map most are simply born into ground their neighbours hold —
  being outnumbered is not being penned. Reading each source's own frontier (wall against rival)
  gives 316 dilution / 4 encirclement. **A sweep told the first version would have tuned the
  barrier costs to fix a crowding effect.**
- **A synthetic test case can assert nothing while reading green.** C3a's map put the target
  equidistant from both cradles under the wrapped Chebyshev metric, so the proximity half of the
  route claim was a tie. The margin is now six tiles.
- **`save_envelope_roundtrip` still cannot be built by either headless builder** (`core/save_game.hpp`
  links imgui) or in a worktree (no CMake tree). Build it from the main checkout:
  `cmd //c` a batch file that calls the BuildTools vcvars then
  `cmake --build build --target save_envelope_roundtrip`. **Bash quoting mangles the vcvars path —
  use a .bat file, not an inline `cmd //c` string.** BL-817's save field carries an assertion this
  time *because* this was done; it had landed unasserted twice before.
- **The computer-use grant for "ProjectIo" resolves to a stale Sep-2 build in the orphaned
  worktree `elated-mclean-7dd61c`**, which is not in `git worktree list`. Screenshot filtering is
  `mask`, so both monitors come back black. **The live watch on round 4 is still owed** and could
  not be taken this session.

---

## Reproduce

```
node tools/verify/build_harness.js colonisation_harness && build_gen/verify/colonisation_harness.exe 3
node tools/verify/build_harness.js history_sim_harness  && build_gen/verify/history_sim_harness.exe
node tools/verify/build_harness.js world_determinism    && build_gen/verify/world_determinism.exe
node tools/verify/build_harness.js history_span_cost    && build_gen/verify/history_span_cost.exe 2
build/ProjectIo.exe --verify scripts/verify/history_lapse_press.lua
```

**Baselines:** `colonisation_harness` 28/0. `history_sim_harness` **4 known failures** (R3a2,
R3a3, B384a, B384c) with 5/8 worlds fighting. `world_determinism` ALL PASS. Take them before the
first edit.
