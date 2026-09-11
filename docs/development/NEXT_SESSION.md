# Next session — build BL-906, and nothing else until it is green

Written 2026-09-11. The design work is **done and committed** (`fc688d44`). This session is a
**build** session. Delivery — Full mode.

## Your job

Build **`BL-906 (empire span runs to 1200)`**. One item. Do not start a second item until it is
merged and verified.

Read the item first:

```bash
node tools/session/backlog_query.js --grep BL-906 --full
```

## What BL-906 is, in four sentences

The Empires phase is designed to run **400 BCE → 1200 CE** (1,600 years). It actually runs
**400 BCE → 0 CE** (400 years). The cause is one coupling: pass 1 stops at the **epoch**, and the
game's epoch is 0 CE. Give pass 1 its own stop year, defaulting to **1200**, so it no longer takes
that value from the epoch.

`stop_after_ancient_era` already exists as the halt point (it shipped with `BL-871 (empire span)`).
What it halts **at** is the thing to change.

Files: `src/world/history_sim.cpp`, `src/world/hard_coded_world.cpp`, `src/world/era_minus_one.hpp`,
`tools/verify/history_sweep.cpp`.

## Hard rules — read these before you write code

1. **Do NOT reach 1200 by moving the epoch to 1960.** That pulls in two phases that do not exist
   (Globalisation, Digitisation — `BL-913`). The epoch stays at 0 CE.
2. **Do NOT tune any constant to keep an old number stable.** Running 1,600 years instead of 400
   will move nearly every figure in sprint 38's scoreboard. **That is expected and is the point.**
   A figure measured over 400 years was never a measurement of this phase.
3. **Do NOT treat a moved number as a regression.** Record the new value beside the old one and
   move on.
4. **Do NOT touch `src/world/corp_ai.cpp`** or add any AI behaviour. Read
   `.claude/rules/io-standing-rules.md` if you are unsure.
5. **Determinism is not negotiable.** No floats in the sim's decision path. `world_determinism`
   must stay green.

## Commands

Build the app:

```bash
./build_app.bat
```

Build and run the sweep harness:

```bash
node tools/verify/build_harness.js history_sweep
```

```bash
./build_gen/verify/history_sweep.exe 16 --epoch 0
```

Determinism check (this one is Lua-linked, so it uses the **other** builder):

```bash
cmd //c "tools\verify\build_lua_harness.bat world_determinism"
```

```bash
./build_gen/verify/world_determinism.exe
```

## Done when

- The Empires round runs **400 BCE → 1200 CE** with the epoch still at 0 CE.
- `history_sweep` prints the span it actually ran, on the face of the report.
- `world_determinism` is unchanged (0 failures).
- The sprint-38 figures are **re-read at full span** and written into the item's `resolution`
  beside the old ones. The old ones, for comparison:

| | 400-year value |
|---|---|
| hegemony rate | 0 / 16 worlds at a 50% share |
| largest share | median 12%, range 6–19% |
| rise → peak → fall | 15 / 16 worlds |
| secessions | median 2 per world |
| materials sinks | 8% of production |

- **Measure the wall clock before and after.** This quadruples the empire round and pass 1 is
  already the most expensive pass. If it lands badly, a shorter **step** is the admissible lever —
  never a shorter span. Report the number either way.

## Commit

One commit for the item, via the `scoped-commit` skill — the working tree carries unrelated
modified `perf_*.csv` and `history_sweep.json` that must **not** be swept in.

```
BL-906: the Empires phase runs its full 1,600 years

Tasks: <N completed>, <N cancelled>
Requirements: <N completed>, <N pending>, <N failed>
```

## If BL-906 lands and you still have time

Take **`BL-907 (closure contract scoreboard)`** next — it is the reason BL-906 matters. Nothing
else. The other items in sprint 38 have dependencies that BL-906 and BL-907 unblock.

Do **not** start `BL-912 (empire tree wired to sim)` in a low-effort session. It is difficulty 6.

## Where the design lives

`docs/generation/CIVILISATION.md` § **The closure of the Empire era** — what crosses at 1200 CE and
the seven readings the contract is judged on. Written 2026-09-11, settled on two elicitation forms.
Do not re-open those calls; build against them.

The other seven items filed with it: `BL-907` scoreboard · `BL-908` contact record · `BL-909`
directed want · `BL-910` capitals and markets · `BL-911` network crosses · `BL-912` empire tree ·
`BL-913` Globalisation/Digitisation phases (design-owed, no sprint).

## Traps that have cost time here before

- **`build/` holds stale harness exes and `ctest` does not rebuild them.** Always build the target
  before running it. A failure straight after a merge is a stale binary until a fresh build
  reproduces it.
- **A harness failing on `sol/sol.hpp` is the wrong builder, not broken code.** Use
  `build_lua_harness.bat` for that one.
- **Check `git status` before committing.** This checkout is shared with other sessions.
- **Patch scripts must preserve CRLF.** Read and write binary, or a 10-line edit becomes a
  1000-line diff.

## Still open, not yours this session

`BL-891` (round 4 arc readout, partial) · `BL-904` (wizard footer reachability — **needs a human at
the keyboard**, no script can answer it) · `BL-905` (reach gate refuses nothing) · `BL-901`
(culture crossed water) · `BL-902` (pass_one_handoff fixture red) · `BL-903` (communication rung,
design owed).

Awaiting Ben's judgement, do not resolve these yourself: `NR-827` · `NR-828` · `NR-829` · `NR-830`
· `NR-831` · `NR-832`.
