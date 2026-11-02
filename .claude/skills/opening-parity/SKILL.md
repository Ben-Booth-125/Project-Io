---
name: opening-parity
description: Measure how equal the major nations' openings are and whether they stay workable over several years of play - run tools/verify/opening_parity over the seed library at one or more nation-count pairs, then pool the shards. Use when testing a major opening kit configuration (CORPORATION_GENERATION.md § A major's opening kit), before and after any change to the kit, the opening treasury, the opening army or the nation-count targets.
---

# opening-parity

`tools/verify/opening_parity.cpp` (BL-1475, opening parity census) builds each world through the
real path - `make_hard_coded_world` with the wizard's nation targets, then `finish_campaign_world`
(search, fold, reserve, cut, settle) - and steps N play quarters with **nobody seated**, so every
major runs on its own scorer on equal terms. It REPORTS; it does not gate.

Per seed and pair it prints every major at the handoff (tiles, treasury, buildings by category
ext/proc/logi/civic/mil, units, men, land pool) and over play (mean and minimum quarterly net,
quarters in the red, treasury, buildings and men at the end), the within-seed max/min spread, how
many major pairs share a border, and the minors' medians. `ROW` and `SEED` lines are machine-readable.

## Run

```bash
bash tools/verify/build_lua_harness.sh opening_parity
bash tools/verify/par_seeds.sh opening_parity -- --pairs 20:4,30:4
node tools/verify/opening_parity_pool.js
```

`--quarters N` sets the play span (default 20, five years). One world is about 3 minutes, so always
sweep through `par-seeds` (all 16 library seeds run at once, about 10 minutes for two pairs).
`--tier quick` (4 seeds) while iterating.

## Reading it

- **Spread** is max/min within a seed; 1.00 is perfect parity. Read the median and the worst seed.
- **Majors losing money** and **ending under 100 credits** are the workability check: a kit that
  leaves a major broke within five years is not a working start.
- **Touching** counts tiles within one grid step, corners included, so it may overcount a little.
- Compare a configuration against the baseline taken on the same tree; a re-bless moves both.

## Baseline (main 5b51db7a, 2026-11-02, before the kit)

| | 20:4 | 30:4 |
|---|---|---|
| Treasury spread, median / worst | 21x / 107x | 10x / 229x |
| Buildings spread, median | 13x | 5x |
| Majors' median treasury / buildings / men | 16,074 / 29 / 600 | 15,212 / 27 / 450 |
| Majors losing money / ending under 100 cr | 17/64, 13/64 | 18/64, 12/64 |
| Seeds with majors sharing a border | 13/16 | 13/16 |
