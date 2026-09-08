# Next session — resume stage 4 (the history pass)

Written 2026-09-08 when Ben paused development mid-sprint to get the design down first.
Sprint 35 is **paused, not closed**. Read this, then
`docs/generation/GENERATION_STRATEGY.md` §§ Round 4's arc · The scorer asks two more
questions · Population is civilian · Turbulence is the parameter.

## Where the work actually stands

**Merged into `main` and independently verified by the main session** (not taken on agent
self-report — two of the three reports were wrong on substance, so re-run the command):

| Item | State |
|---|---|
| BL-816, BL-824 — wizard rounds 4 and 5, `Begin` → `Next` | Built. Live click-through done: five rounds, labelled placeholders, `Begin` only on round 5. |
| BL-825 — the 4000-year measurement | Done, and it **refuted** the hypothesis it was written on. |
| BL-826, BL-827, BL-828 — culture shares, grudges, the handoff type | Built. `pass_one_handoff` passes 0 failures. BL-828 is **half done** — `pass_one_output` exists and validates, but `generate_nations` and friends still reach past it. |

**The batch's step 4a review barrier has NOT run.** A single `verifier-review` pass across
the whole integrated set is still owed before any of this is called finished.

## The two numbers that should shape everything next

**4000 years is not free.** Per-year cost at 4000 years is **6–9×** its cost at 400 — ten
times the years for seventy to eighty times the time. Reach/Dijkstra is **66–86%** of the
run. `rebuild_reach` caches into a *single shared slot*, so each of twelve polities evicts
the previous one every round: 12,000 rebuilds over 1,000 rounds. Its own comment claims the
cache survives until a capital moves; with more than one polity it does not survive one
iteration. **BL-834** fixes it. Do this before anything that makes reach load-bearing.

**Most of the war is not war.** All 258 battles of the seed-0 fixture are the *same region*
— zero population, zero defenders — taken and retaken for four thousand years.
`battles == conquests == 258`, exactly 1:1, which `history_sim_harness` prints unaided. Do
not tune anything against the battle count until BL-835 lands; it is measuring one dead tile.

## What Ben settled on 2026-09-08, in order

1. **Population is civilian; armies are distinct; stage 4 does not simulate total warfare.**
   This is the root fix for the dead region — war stops *producing* empty regions, so the
   pathology has no cause rather than a block. **BL-835**, re-authored around it.
2. **The scorer asks two more questions.** *Can I keep it?* (BL-837, logistics and ancient
   roads) and *Will others attack me for fear of being next?* (BL-838).
3. **Turbulence is the round 4 lean** — roll for a world with fewer or more countries.
   **BL-839**. It tunes forces and never clamps a count.

## Three traps, each already paid for once

- **BL-838 must not become a rank term.** "Largest polity" is not the trigger; "the polity
  that has been doing this to people like me" is. A size coefficient would pass every
  obvious check and violate the standing rule the item exists to satisfy. The test is that a
  large *peaceful* polity attracts no coalition.
- **R5 is failing and must stay untouched** until BL-835 is fixed and it is re-measured. It
  is a correct check pointing at a real defect. Do not relax it, do not add seeds until one
  goes green. (`R5b` is trivially true when both counts are zero — that is why a printf was
  added to make the failure diagnosable.)
- **BL-839 is not a name bank.** Its two archetypes come from real history as *mechanisms*.
  What transfers is the mechanism; a proper noun never does.

## Open calls waiting on Ben

`NR-803` the 1200→1560 coast · `NR-804` the wizard has no `ACTIONS.json` entries ·
`NR-805` profiling counters inside `world/*` · `NR-801` rounds 4 and 5 break the
wizard's "nothing is generated here" promise.

## Reproduce

```
node tools/verify/build_harness.js history_span_cost && build_gen/verify/history_span_cost.exe 2
node tools/verify/build_harness.js pass_one_handoff  && build_gen/verify/pass_one_handoff.exe
node tools/verify/build_harness.js history_sim_harness && build_gen/verify/history_sim_harness.exe
```

The play build is `build_rel` (Release, Ninja) — `build/` is Debug and its timings are not
comparable. Quote the build tree with any figure.
