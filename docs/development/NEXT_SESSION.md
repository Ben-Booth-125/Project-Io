# Next session — Sprint 20: read the sweep, rule on the army, settle the forms

The 2026-08-26 session built and ran the spawn-viability sweep (BL-626, spawn viability
sweep). The data is in and the headline is settled: **every named corp is insolvent by tick 3
in every campaign-real seed**, and the cause is identified — the BL-476 starting army under
BL-454 upkeep, compounded by BL-073 debt interest over the 80-tick warm start. Full data,
arithmetic and options: NR-648. The devlog entry has the narrative.

## What waits on Ben

1. **The army ruling (NR-648).** Four options logged, none chosen: retainer upkeep for seeded
   units, warm-start interest grace, a funded spawn form, or hire-when-affordable instead of
   seeding. (a)/(b)/(d) are mechanical once ruled; (c) is the forms session.
2. **The viability verdict rule (NR-647).** Which combination of solvent / earning / active
   is "viable" — wanted before the forms session so the data is read against one definition.
   Recommendation on file: runway (balance + 4× trailing net > 0) as headline, strict
   solvent∧earning alongside.
3. **The sweep defaults review (NR-646).** Spectate default, seeds 0..N-1, trailing-year
   net, the active definition — delegated calls, reversible by flag.
4. **The forms session (BL-627, corp spawn forms).** Candidate forms and constraints are in
   the item; settle against the sweep data once 1–2 are ruled.

## Mechanics for whoever runs next

- Sweep: `cmake --build build --target spawn_viability`, then
  `spawn_viability [seeds] [ticks] [--played] [--lean] [--csv <path>]` from the repo root.
  Ctest label `sweep` behind `IO_RUN_SWEEPS`. Data lands in `build_gen/verify/*.csv`
  (gitignored — regenerate, don't hunt for it).
- The 2026-08-26 session ran it on Linux by extending the NR-264 recipe with Lua 5.4.7 +
  sol2 v3.5.0 (GitHub mirrors); folding that into `build_harness.js` would close part of the
  NR-558 live-Lua gap — unfiled, mention-only.

## Carried debts (unchanged from the 2026-08-25 close-out)

- Two live clicks owed: dispatch form, Throughput lens (container access, three sprints
  running) — in Sprint 20's definition of done.
- Dispatch form UX fixes (pool-stock pre-check, priced-leg preview) await Ben's
  A/backlog-or-B/build-now call.
- One deliberate baseline re-bless wave at the END of the viability pass, with provenance
  (NR-596 precedent) — not a dribble.
- `--serve`'s 12-tick default vs the app's 80 (stale `main.cpp` comment) — align before
  trusting wire-test numbers against sweep numbers.
