---
name: market-viability
description: Run the sprint 49 market viability gate (BL-1184) — one 16-seed reading on the shipped 1960 start that says whether the market economy is viable from day 1, against Ben's targets (pooled: >= 70% of built processors running at handoff, field income at tick 50 >= 50% of the settle close, >= 70% of firms alive at tick 400). Use it to judge ANY sprint 49 economy or generation change, before and after; a world-mover measures its own before/after with it, in isolation.
---

# market-viability

The sprint 49 gate. `tools/verify/market_viability.cpp` builds the **shipped start** per seed
(`build_app_start_world`, `tools/verify/harness_params.hpp`), runs the 12-tick settle as the
app's validation run does, seats the player as `app::seat_player` does, then steps play ticks
as `run_app_live_window` does. It is a **reader**: it writes nothing the simulation reads.

Authority: `docs/economy/MARKETS.md`. The item: BL-1184 (market viability gate) —
`node tools/session/backlog_query.js --grep BL-1184 --full` carries the 2026-10-04 reading it
formalises. Targets: Ben, the sprint 49 form, 2026-10-04.

## Build and run (repo root)

```bash
bash tools/verify/build_lua_harness.sh market_viability      # Lua class: never build_harness.js
./build_gen/verify/market_viability.exe                       # 16 library seeds, 400 play ticks
./build_gen/verify/market_viability.exe --seeds 0,43 --ticks 50   # a quick look (G3 then reads t50)
```

- Default seeds: `docs/generation/seed_library.json`, in library order (market_census's scan).
- `--ticks N`: PLAY ticks after the handoff (default 400). Below 50 the tick-50 rows are
  empty and the run fails its honesty check.
- Never target `%TEMP%`; the exe lives in `build_gen/verify/`.

## How long it takes

The build is ~4 minutes. On an idle machine a seed takes 1.5-5 minutes (world build plus 400
play ticks): the full 16 x 400 is about an hour. Under load it stretches badly — the
2026-10-04 baseline saw seeds of 82 s to 1,697 s, 8,772 s summed. The per-seed header prints
the build / settle / play split.

An agent's background command is capped at 2 hours, and that cap stopped the baseline run
after 10 seeds. On a busy machine, split it: `--seeds` the first ten, then the last six. Each
seed's rows are deterministic, so a split run reads the same per-seed numbers; only the
pooled block is per invocation, so pool the per-seed rows by hand (sum / sum). Hold
keep-awake (`tools/session/keepawake.ps1 -Process market_viability`) on an unattended run.

## What each row means

"Handoff" is the end of the 12-tick settle. "Tick N" is play tick N, counted from the handoff.

- **G1, processors by state**, at the handoff (the settle's last economy report) and at tick 50.
  `run` produced output. `input` is input-starved: the scarcest input covers less than `t_idle`.
  `nolab` has no effective workforce. `unsupplied` has labour but zero batches (supply scalar 0
  or workforce target 0). `decom` is idled (the loss reflex or an idle verb); a second line
  splits it by the state on the last tick it reported — ran, input-starved, other, or never
  reported. `build` is under construction.
  **Share running = run / built processors** (every state but `build`). The share over all
  processors, construction included, is printed beside it. Target: pooled >= 70% at handoff.
- **G2, field income per tick** — the sum of every corporation's filed `quarterly_return::income`,
  at the settle's last tick and at tick 50, and their ratio. Pooled = sum / sum.
  Target: pooled >= 50%. The settle's 12-tick mean and the play 26-50 mean print as context;
  the 2026-10-04 reading's "54-74k / 10-19k" were those window means.
  **Read G2 with care.** The settle's last tick comes after the opening stock is auctioned, and
  it swings 12k-45k across seeds. On the baseline the single-tick gate PASSES (64.8%) while the
  window form reads 27.5%. Which form is the gate is Ben's call (flagged with BL-1184).
- **G3, firm survival** — of the corporations present at the handoff (seat included), how many
  stand at the last play tick. Pooled = sum / sum. Target: pooled >= 70%. Firms born later are
  not in the cohort; the end count prints beside it.
- **G4, reported, no target.**
  The seat's operating net (income less expenditure, maintenance, wages, levies, upkeep;
  interest excluded) for each of the last 8 settle quarters, and each of its processors with
  how many of those 8 ticks it ran.
  Home-body markets with no exchange in play ticks 1-4 (market_census's CLEARS NOTHING; an
  overflowed exchange ring marks it an upper bound).
  All buildings at tick 50: live, under construction, decommissioned.

The run closes with a per-seed table, the pooled rows, one `PASS`/`FAIL` line per target, and:

```
market_viability: G1 x/70 G2 y/50 G3 z/70
```

## Exit code

`0` for a completed reading, **whatever the targets say** — the baseline is expected to fail
them, so they are reported rather than enforced. `1` only when an honesty check fails: a seed
that did not build, no corporations, zero processors at the handoff, zero field income at the
settle close, no seat, or fewer than 50 play ticks.

## How to use it

- **Before and after, in isolation.** A change that moves the world runs the full reading on
  its own base and on its own tip, nothing else merged between. Report which G rows moved and
  why that movement is the intended one. Do not compare against a reading taken on another
  base: main moves, and the baseline moves with it.
- **Read per seed, not only the pool.** A pooled pass can hide one collapsed seed.
- The committed baseline lives at `docs/development/drafts/sprint-49-viability-baseline.txt`;
  it is a dated reading on one base, not a pin.
