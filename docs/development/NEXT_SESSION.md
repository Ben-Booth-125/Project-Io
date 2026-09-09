# Next session — sprint 37, building the colonisation span

Written 2026-09-09 at the close of the design pass. **The design is finished; this is a build
session.** `docs/generation/COLONISATION.md` is new and is the authority — read it before anything
else, and do not re-derive what it settles.

## What Ben asked for

> *"The first objective is making a page in the pre-generation wizard. This should carry our two
> dimensional grid-map. We then want to make sure it works and produces interesting cultures. For
> this task, we don't have to aim for a small set. We are looking to transform regions into
> provinces, and leave trimming down the final country count to the next phase."*

Three things, in his order:

1. **A wizard page carrying the 2D grid-map.**
2. **Make it work, and show it produces interesting cultures.**
3. **Regions become provinces. Do not aim for a small set** — trimming the country count belongs
   to the empire phase, not this one.

**The framing that makes objective 2 tractable: the page is the instrument, not just the surface.**
There is no other way to judge whether colonisation produces interesting cultures. A harness can
say the pass is deterministic and self-consistent; only watching 4,000 years play out on a map can
say whether the cultures are worth having. Build it as the thing you will use to judge the rest.

---

## The design, settled 2026-09-09 — seven items, all `designed`

`COLONISATION.md` owns the span. Ben's rulings, so none of them is re-opened by accident:

- **Diffusion, no actor.** Nothing decides to colonise; surplus, ground and package are the whole
  model. **This is why the span needs no AI-behaviour grant, and it must not become one.**
- **No infrastructure.** No roads, works, logistics or supply. The only cost is that people
  physically move, and it is paid in **years**.
- **A stated boundary year**, not a derived one.
- **A domestication package** spreads from each cradle and gates settlement; **breadth** is the
  asymmetry generator.
- **Culture arrives by route**, not by proximity — superseding nearest-cradle everywhere.
- **Predation caps the pressure**, decaying **logarithmically in population**.
- **Fragmentation comes from contact**; the creeds' tribal marches retire.

| Item | What it is |
|---|---|
| **BL-846** (colonisation span) | The spine — the diffusion loop, the boundary, the year-cost walk |
| **BL-847** (domestication package) | Affinity and breadth; the gate; crossing |
| **BL-848** (culture by route) | Inheritance by arrival rather than proximity |
| **BL-849** (colonisation seeds partition) | Settled cells as a hard input to the province partition |
| **BL-850** (predation caps pressure) | Wildlife danger, log decay, re-wilding on a sack |
| **BL-851** (sweep separates cradle outcomes) | Four outcomes currently read as one |
| **BL-852** (fragmentation from contact) | The marches retire; fragmentation from interpenetration |

**Four things stay open in the doc and none is a design call** — three magnitudes the sweep argues
(the boundary year, the predation coefficient, the package's representation cost) and one
derivation that belongs inside BL-852 (the non-hegemony floor). Do not treat any of them as a
reason to pause.

**NR-808 is resolved.** Two entries remain open in the queue and neither bears on this work.

---

## The dependency that decides the first day

**BL-829 (the time-lapse view) draws "polity colour per region from the BL-817 record" — and
BL-817 (history playback record) is `designed`, not built.** The wizard page cannot play anything
back until the record exists. That is the real first task, and it is generation-layer work, not UI.

**What already exists, so nobody rebuilds it:**

- **BL-816 (wizard rounds four and five) is COMPLETE and verified by live click.** Round 4 exists
  in the built app as an honestly-labelled amber placeholder. Five pips navigate, Back from 5 lands
  on 4, and the `static_assert` binding `chain_round_count` to `wizard_round_count` was already
  re-expressed as two asserts. **The shell is there; the content is not.**
- **`STARTUP.md` § Rounds 4 and 5 already settles what the round shows** — 2D map replacing the
  globe in the same pane, 4000-year time-lapse to 1200 CE, leaderboard on the left, rerollable.
  That is Ben's 2026-09-08 ruling. Read it rather than re-designing the round.
- **`GENERATION_STRATEGY.md` § The two watched passes** carries the same ruling from the generation
  side.
- **BL-830 (top-sixteen scoreboard)** is designed and is the leaderboard half.

**The acceptance criterion for the round is Ben's arc, and it is not a number:** *origin →
communication → conquest or diplomatic union → a stable dark age.* A run reaching 1200 CE without
that shape has failed even if every figure is plausible.

**One trap inside BL-830:** the research column lies while research is parked — BL-822 accrues
research points from population, so research speed and population are the same number in two
columns. Label it or omit it; do not ship a board showing a correlation it never measured.

---

## The scale question Ben's brief raises — and the number that makes it sharp

**"Transform regions into provinces" spans a factor of sixteen.**

| Thing | Count on a homeworld |
|---|---|
| Regions after 4,000 years (seed 0) | **1,372** |
| Land provinces after the partition | **22,153** (`PROVINCES.md` § The size band) |

The design session already ruled on the *relationship*: **colonisation seeds the partition, the
late pass still draws it** (BL-849), so regions need not become provinces one-for-one. But
"don't aim for a small set" says the region count should grow, and nothing has said how far.

**Do not pick a target by judgement. The cost is superlinear and it was expensive to win.** Sprint
36 took the 4,000-year span from 6,923 → 1,288 ms on seed 0, and **reach is still 41% of that run
as a per-region Dijkstra** (BL-844). Sixteen times the regions is not sixteen times the cost.

**So the first measurement of the session is region-count sensitivity** — run the span at several
region counts and plot the cost before committing to a number. That is a cheap experiment and it
converts an unbounded decision into a bounded one. `history_span_cost` is the instrument.

---

## Where to start

| Doc | For |
|---|---|
| `docs/generation/COLONISATION.md` | **The authority for the whole span. Read first, in full.** |
| `docs/ui/STARTUP.md` § Rounds 4 and 5 | What round 4 shows, and what it must not take as input |
| `docs/generation/GENERATION_STRATEGY.md` § The two watched passes | The same ruling, generation side |
| `docs/generation/PROVINCES.md` § The settled cells are a binding input | What BL-849 changes, and the road not taken |
| `docs/economy/POPULATION.md` § Region demography | `advance_region_demography` — the surplus predation caps |

A suggested order of work, given the dependency: **BL-817 (the record) → BL-846 (the span) →
BL-829 (the view) → BL-847 (the package) → judge the cultures on the map → the rest.** The view
early is deliberate: it is what objective 2 is judged on, and every later item is easier to
evaluate with it running.

---

## Reproduce, and the baselines to take BEFORE touching anything

```
node tools/verify/build_harness.js history_sim_harness && build_gen/verify/history_sim_harness.exe
node tools/verify/build_harness.js world_determinism   && build_gen/verify/world_determinism.exe
node tools/verify/build_harness.js history_span_cost   && build_gen/verify/history_span_cost.exe 2
node tools/verify/build_harness.js demography_harness  && build_gen/verify/demography_harness.exe
```

**Current state, as of this handover:** `history_sim_harness` has **4 known failures** (R3a2, R3a3,
B384a, B384c). `world_determinism` is ALL PASS. `demography_harness` is 30/0.

**Take the baseline first.** BL-846 changes what the span produces and BL-852 changes the nation
count; `world_audit` R1/R3, the creeds harness and `history_sim_harness` all read those paths. Take
the baseline before the first edit or you cannot separate your breakage from the four that were
already there.

**`B384a` may want retiring rather than fixing.** It asserts that every world sees at least one
region change hands by war, and it fails because several seeds fight zero times. **This design says
that is correct behaviour** — the colonisation span is a settlement process with occasional
violence. Raise it rather than quietly deleting it.

The play build is `build_rel` (Release); `build/` is Debug and its timings are not comparable.
**Quote the build tree with any figure.**

---

## Traps, all of them paid for already

- **Code comments still describe superseded rules, and they are CORRECT until their items land.**
  `settlement.hpp:18` and `hard_coded_world.cpp:532` describe nearest-cradle inheritance
  (BL-848 supersedes it); `creeds.hpp:17/87/89` and `hard_coded_world.cpp:520` describe welding
  (BL-852 supersedes it). **Fix each WITH its change, never before** — a comment that describes
  undelivered design is worse than one that describes shipped code.
- **A worktree agent cannot build `save_envelope_roundtrip`.** It includes `core/save_game.hpp`,
  which links imgui, so neither headless builder compiles it. Build it through CMake under the
  pinned VS2022 BuildTools vcvars. A save-format field has landed with no assertion twice now for
  this exact reason (BL-748, and again in sprint 36).
- **Worktree agents arrive stale.** Sprint 36's arrived 26 commits behind. Check the merge-base,
  merge `main`, rebuild, and re-measure before quoting any figure.
- **An agent's conclusions held on re-measurement; its figures did not — twice in sprint 36.**
  Quote your own numbers.
- **A UI requirement needs a live click.** A `verifier-visual` capture proves round 4 renders; it
  does not prove the reroll is reachable. Open the built app and press it.
- **Real history is a mechanism reference, never a name source**, and cultures are exactly where a
  proper noun is most tempting. Every generated name stays sci-fi/fantasy out of the seeded banks.

---

## State at handover

`main` at `a1361806`, **34 commits ahead of `origin/main`** — push before branching, or a worktree
agent inherits a base that predates the entire design pass.

Three commits carry this session: `65308724` (the doc and its siblings), `ff9bee16` (predation and
the cradle outcomes), `a1361806` (the marches retire, log decay). The working tree holds six
modified `perf_*.csv` files that are **not** this session's — leave them alone.
