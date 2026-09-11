# Next session — batch-deliver sprint 39, the drama of the time-lapse

Written 2026-09-11. The design work is done: sixteen items filed (BL-914..BL-929, BL-928 sprintless),
four rulings written into their authority docs, five calls resolved on a form (NR-835..NR-839).
This session is a **build** session. Delivery — Full mode, Batch Delivery (DELIVERY.md § Batch
Delivery). Ultracode-style fan-out is appropriate: one worktree agent per item, the main session
merges, builds, sweeps and live-clicks.

## Read first

```bash
node tools/session/backlog_query.js --status designed
```

Then the three sections the sprint is built against, and nothing else on a first pass:
`docs/generation/CIVILISATION.md` § The unit is the city state, § The arc the phase must produce,
§ How an empire actually falls; `docs/ui/STARTUP.md` § The wait is the round; `docs/generation/COLONISATION.md`
§ Migration spawns cultures. `docs/development/sprints.json` sprint 39 carries the sequencing and the
eight ranked causes of stability the holistic read found.

## Order

1. **BL-926 (sweep sees the arc) FIRST, alone.** Save the 16-seed sweep on main as the baseline
   before any other item lands. Every other item is read against it.
2. Independent wave: BL-927, BL-918, BL-919, BL-915, BL-916, BL-922.
3. Dependents: BL-914, BL-917, BL-920, BL-921, BL-924, BL-925, BL-929.
4. BL-923 last (requires BL-920 and BL-922).

## Hard rules

- **Measure, never target.** No number in any done-when is a target; read the spread before and
  after. A peaceable seed is legitimate.
- **Determinism.** BL-914's tap is write-only and asserted tapped == untapped. BL-922 changes the
  decision digest: ONE authorised re-bless of world_determinism as a wave act, never per item.
- **No AI behaviour, no corp_ai.cpp.** The Organise verb and the pooled levy are generation-sim
  verbs under the existing grants; anything beyond that is raised, not assumed.
- **A UI item needs a live click**, not just a capture: open build_rel, arrive on rounds 3 and 4,
  watch them run.
- **Check git status before committing.** The tree carries another session's modified
  `history_sweep.json` and six `perf_*.csv` files; never sweep them in. One commit per item via
  `scoped-commit`.

## Commands

```bash
./build_app.bat
```

```bash
node tools/verify/build_harness.js history_sweep
```

```bash
./build_gen/verify/history_sweep.exe 16 --epoch 0
```

```bash
cmd //c "tools\verify\build_lua_harness.bat world_determinism"
```

## Traps

- `build/` holds stale harness exes; always build the target before running it.
- A harness failing on `sol/sol.hpp` is the wrong builder (use build_lua_harness.bat).
- Worktree agents can branch from a stale base — brief them to fetch and fast-forward, and check
  the merge-base yourself.
- The shell rejects very long inline commands here; write scripts to the scratchpad and run them.
