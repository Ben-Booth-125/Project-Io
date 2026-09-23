# Next session — read the overnight suite, close BL-1050 and BL-1044, then the sprint 46 cut

Written 2026-09-22 at ~00:05. **Read the memory `io-sprint-45-open` first, then this.** BL-1044 is
built, measured, ruled and RE-BLESSED; what is left is one open call, two suite readings, and the
paperwork that closes sprint 45.

## Where it stands

- **BL-1044 (Beat 1 ships) and BL-1050 are both still OPEN, and both are done but for R8.** The
  requirement group is `beat-one-ships`: R1-R7 and R9-R11 complete, **R8 partial** — the ctest suite
  and the visual suite are the only rows outstanding.
- **The re-bless is taken and confirmed** (Ben authorised it at Gate 2): the shipped arc's 16 pins at
  650:2 re-check 16/16, the legacy arc's re-pinned D_settle/D_seat re-check 16/16 (D_search/D_land
  untouched), `--guard 12` passes S1-S7 including the new one-player row, and the seed library is
  blessed (6 of 16 moved; the tier-off control reproduced all 16, so BL-1037's tier is the only
  mover).
- **Every ruling of the day is recorded**: NR-909 to NR-914 resolved, written into DIGITISATION.md
  § 1, CORPORATION_GENERATION.md § Pass 1 and NATION_GENERATION.md § Pass 7.

## 1. Read the overnight suite (R8's last two rows)

It ran unattended from ~00:00 in
`%TEMP%\claude\C--Users-benbo-Project-Io\<session>\scratchpad\overnight2\` (session-scoped — if the
directory is gone, re-run the three steps; they are in `status.txt` order):

- `status.txt` — one line per stage, and `build_failures.txt`, the CTest targets that would not
  build. **Expect some**: `nmake all` has stopped at a broken target since 2026-09-02, which is why
  the script builds every target by name instead.
- `02_ctest.log` — the 157-row suite. The question it answers is **which world-building rows now
  breach the 60 s default** (setup went 21 -> 51 s on seed 28 before this item; the budget row's
  generation is now a median 53.3 s in Release). A row that times out is a timeout to raise, not a
  regression to fix blindly.
- `03_verify_all.log` — the 119-script visual suite against ONE span world. `--verify` now spends
  the charter budget on the seed candidate (NR-909), so captures move; goldens are demoted
  (`ben-treats-goldens-as-disposable`), and the reading is "does every script still run and does the
  world look right", not a golden diff.

Then set R8 complete (or record what it found), and **close BL-1050 and BL-1044** — status
`complete` with a DELIVERED line, as BL-1064 was closed.

## 2. NR-915 is open and it is Ben's call

The live tick at the pinned 650:2 is a **median x0.78** of the same seed's legacy world — inside the
band NR-908 set — but the tail is heavy: seeds 41, 31 and 28 run **x5.79, x4.12 and x3.13** (30.9 s,
25.5 s and 12.8 s a live tick). The cost follows the SPECIALISTS the budget charters, not the
background firms (seed 41 seats 65 and holds 56 firms; the median world seats 8.5). The options are
in the entry; the recommendation is measure the phase split, then bound the seat side rather than
move the divisor the re-bless just pinned.

## 3. Then sprint 45's retro and the sprint 46 cut

- The retro goes in `sprints.json` (`render_sprints.js`). Sprint 45 delivered 16 items and spent one
  re-bless, as it was cut to do.
- The cut is `docs/development/drafts/sprint-46-items.json` — **21 rows now**, the new
  `DENSITY_FOLLOWS_CITIES` included (Ben, NR-913: density becomes its own design pass). Its own
  `_note` says what the cut does: mint ids (`next_id.js`), file each row with sprint "46", write a
  requirement group per item, amend BL-1047, raise the campaign-tech scope flag as a novel-work NR,
  then DELETE the file. Every file:line in it predates BL-1042/BL-1044/BL-1050 and must be
  re-checked as each item is filed.

## The machine changed (2026-09-23)

Ben added RAM: **31.2 GB total, ~20 GB free** (it was 15.5 GB, with ~6-7 GB free and one
`player_seed_sweep` peaking ~6 GB). 16 cores. So the old "one sweep at a time" rule is a MEMORY
rule that no longer binds — two or three sweeps fit. It is still true that a TIMING row (the cost
mode, a ctest timeout row) must not share the machine with another run, and that is a CPU-and-cache
question, not a memory one.

## Standing hazards, still true

- **Another session cut v0.1.24 mid-flight today** (d0d3fa72, 8421c3b4, 20:10). This checkout is
  shared: `git status` before committing, and commit in increments.
- Keep-awake is a `.ps1` FILE run in the background; its log must say "held".
- A `player_seed_sweep` peaks ~6 GB and the machine has ~7 GB free — one heavy run at a time, and
  kill nothing without reading its command line first.
- The Release `build_gen` path compiles every world TU; the Debug CTest targets name their Lua TUs
  by hand, and one had gone dark that way since BL-1030 (fixed 2026-09-22, ee7ef3c3). A harness that
  passes under `build_lua_harness.sh` can still fail to LINK under ctest.
