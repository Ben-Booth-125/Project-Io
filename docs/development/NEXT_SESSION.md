# Next session — sprint 37, the colonisation phase

Written 2026-09-09 when Ben redirected sprint 36 mid-flight. **Sprint 36 is closed, not paused**
— its three landed items are verified and committed. This is a fresh subject.

Read `docs/development/SPRINTS.md` § 37, then the docs named under *Where to start* below.

## What Ben asked for

> *"Switch focus on sprint 36 to be this initial colonisation and spreading of humanity. Focusing
> on how populations grow and develop agriculture in appropriate locations. This gives us a chance
> to look at cultures, how they grow, and what kind of philosophies grow as a precursor to
> military doctrine."*

The ordering is the point. **Doctrine is downstream** of how a people came to live where they
live — not an independent axis bolted on beside it.

His reasoning, in his words: *"the rules for initial colonisation and spreading of peaceful
societies are vastly different from the later periods of war and empire."*

## The evidence that this is a real seam, not a mood

All measured on the merged tree this session, not inherited:

- Seed 0 over 4,000 years: **533 regions → 1,372, with 839 foundings and 9 battles.** The ancient
  span is overwhelmingly a *settlement* process with occasional violence.
- **`B384a` fails** — "every world sees at least one region change hands by war". Several seeds
  fight zero times. That is not a broken check; it is the early span being a different process.
- The 258-battle pathology sprint 36 removed was **war logic operating on ground colonisation had
  produced** and had no answer for. It was fixed by making the two concerns stop sharing a
  variable — the same seam, one layer down.
- One loop with one parameter set serves both phases, which is why `defence_levy_q` has to satisfy
  empty frontiers *and* contested borders at once.

## The open design question — answer it before building anything

**Where does the boundary between the two phases sit?** A year? A settled-density threshold? First
sustained contact between polities? Each gives a different world. Picking one by convenience is how
this becomes another dial nobody can justify. There is no obvious answer and this note does not
have one.

Second-order, but live: **which phase owns armies.** BL-835 closed this sprint and its model is
right, but that question re-opens the moment there are two phases.

## Where to start

Design pass first — **no items are scoped yet, deliberately**. Read, in this order:

| Doc | For |
|---|---|
| `docs/generation/GENERATION_STRATEGY.md` | The map of the generation layer; start here. |
| `docs/economy/POPULATION.md` | Population centres, habitability, agglomeration. |
| `docs/economy/TILES.md` | Two-axis terrain and deposit profiles — what makes ground *appropriate*. |
| `docs/lore/HISTORY.md` | The institutional ladder driving the Era −1 sim. |
| `docs/lore/CREEDS.md` | Pantheons per cradle-culture, generated tongues — the philosophy end. |

`docs/generation/MILITARY_HISTORY.md` is the *sibling* phase. Read it to know where the seam falls,
not to work in it.

## The trap that applies double here

**Real history is a mechanism reference, never a name source.** Creeds, philosophies and cultures
are exactly where a real proper noun is most tempting. Every generated name stays sci-fi/fantasy,
out of the seeded template banks and phoneme tables. What transfers is the mechanism — how a
frontier stalls, how a charter binds a promise. If a doc says "Rome" it is naming an analogy for
the reader, never content for the game.

## What sprint 36 left behind

**Landed and verified** (four commits, `9a61bfd7`..`d096d550`, each re-measured by the main session
rather than taken on a report):

| Item | Result |
|---|---|
| BL-834 (reach cache per polity) | 12,000 rebuilds → 5,256/5,597. Output-identical. |
| BL-844 (the reach Dijkstra gets a heap) | Reach 71%/42% → 41%/22% of the run. Output-identical. |
| BL-835 (population is civilian, armies apart) | 258 battles on one region → 9 across 1,372. R5 now passes, untouched. |
| The save round-trip assertion | `army_stock`, mutation-tested. |

Together the 4,000-year span went **6,923 → 1,288 ms** (seed 0) and **15,667 → 5,381 ms** (seed 1).
Both cost fixes produced bit-identical digests on all four worlds.

**In the pool, unbuilt and untouched** — all four are empire-phase and each carries a note to
re-scope against whatever boundary this sprint settles: BL-837 (ancient logistics and roads),
BL-838 (fear of being next), BL-823 (anti-hegemon levers), BL-839 (turbulence lean).

**BL-845** (in the quiet worlds every battle is still a conquest) may well be *answered* by this
sprint rather than needing work of its own.

## Two process lessons, both paid for in sprint 36

- **An agent's conclusions held on re-measurement; its figures did not — twice.** Its worktree also
  arrived 26 commits stale (the known trap), which it caught itself. Merge, rebuild, re-measure,
  and quote your own numbers.
- **A worktree agent cannot build `save_envelope_roundtrip`** — it includes `core/save_game.hpp`,
  which links imgui, so neither headless builder compiles it. Build it through CMake under the
  pinned VS2022 BuildTools vcvars. A save-format field landed with no assertion because of this,
  for the *second* time; the harness's own comment records BL-748 doing exactly the same.

## Reproduce

```
node tools/verify/build_harness.js history_sim_harness && build_gen/verify/history_sim_harness.exe
node tools/verify/build_harness.js world_determinism   && build_gen/verify/world_determinism.exe
node tools/verify/build_harness.js history_span_cost   && build_gen/verify/history_span_cost.exe 2
```

Current state: `history_sim_harness` has **4** known failures (R3a2, R3a3, B384a, B384c) —
`B384a` and `B384c` are the ones this sprint's subject bears on. `world_determinism` is ALL PASS.
`demography_harness` is 30/0. The play build is `build_rel` (Release); `build/` is Debug and its
timings are not comparable — quote the build tree with any figure.
