# Next session — rule NR-915, chase BL-1066, then cut sprint 46

Written 2026-09-23 at ~16:30. **Read the memory `io-sprint-45-open` first, then this.**
**Sprint 45 is CLOSED**: all seventeen items delivered, one re-bless spent, the retro written
(`sprints.json`, rendered into SPRINTS.md). `REFINED.md` holds no open work.

## What is waiting, in the order I would take it

### 1. BL-1066 (the player cannot build) — priority S, and the biggest of the three

Found running BL-1044's owed visual suite. On the SHIPPED verify world the player places a military
base and it never completes: `tech_locked` at tick 0, placeable after 12 econ ticks, still
unfinished 100 ticks later while the corp's balance climbs past 70,000 credits. Not cash, not the
tech gate, not the fixture's 40-tick bound — construction itself does not proceed. The app lays the
same web (NR-909), so this is the world a player is handed and the core loop's first move may not
land. The item names the two candidates (construction capacity on the body, or the materials the
build draws); the probes are in the session scratchpad and re-run with
`build_rel/ProjectIo.exe --verify <probe>.lua` — the RELEASE app runs the visual suite about four
times faster than Debug, which is how the suite finished in 98 minutes.

It also blocks eleven of the twelve failing verify scripts: `scripts/verify/lib.lua`'s shared UI
fixture cannot raise the player's force. Fixing BL-1066 unblocks them; the twelfth
(`battle_card.lua` calling `verify.goto_surface()` with no argument) is a 2026-08 API mismatch to
fix on its own.

### 2. NR-915 — Ben's call, the only open queue entry

The live tick at the pinned 650:2 is a median **x0.78** of the same seed's legacy world, inside
NR-908's band, but the tail is heavy: seeds 41, 31 and 28 run **x5.79, x4.12, x3.13** (30.9, 25.5
and 12.8 s a live tick). The cost follows the SPECIALISTS a world seats, not its firms. The entry
recommends reading the phase split first (the cost run prints it) and then bounding the seat side,
rather than moving the divisor the re-bless just pinned.

### 3. BL-1065 (the CTest tier relinked)

Nine targets do not link the Lua TUs their harnesses now pull through `harness_params.hpp`, three no
longer compile (two read `world::mercenary_contracts`, retired by NR-885), one was only a test name.
Until that is done the suite reports 9 rows as Not Run. The clean ctest pass otherwise reads: 96 of
157 pass, 48 TIME OUT at the 60 s default (the world-building family, all green under the Release
`build_gen` path — raise those timeouts), 4 fail for reasons that predate this work.

### 4. Then the sprint 46 cut

`docs/development/drafts/sprint-46-items.json` — 21 rows, including the new DENSITY_FOLLOWS_CITIES
(NR-913: density becomes its own design pass). Its `_note` says what the cut does: mint ids
(`next_id.js`), file each row with sprint "46", write a requirement group per item, amend BL-1047,
raise the campaign-tech scope flag as a novel-work NR, then DELETE the file. BL-1065 and BL-1066 are
already filed against sprint 46 and should join the cut rather than be re-filed.

## The machine changed (2026-09-23)

31.2 GB (was 15.5) and 16 cores. The old "one sweep at a time" rule was a MEMORY rule and no longer
binds — two or three fit. A TIMING row (the cost mode, a ctest timeout row) still needs the machine
to itself, and that is CPU and cache, not memory.

## Standing hazards

- **CHECK FOR ORPHANS before starting a suite.** Three `--verify-all` processes from stopped chains
  ran beside each other for ten hours on 2026-09-22 and made both the ctest and the visual readings
  worthless. `tasklist | grep -i ProjectIo` first.
- **Another session cut v0.1.24 mid-flight** (d0d3fa72, 8421c3b4). This checkout is shared: check
  `git status` before committing, and commit in increments.
- Keep-awake: `keepawake.ps1 -WhileFile <lock>` holds across a CHAIN of runs; the process form
  releases in the first gap between them. Its log must say "held".
- `build_app.bat` EATS STDIN: a `while read` loop over target names builds only the first.
- The 8 golden diffs from the visual suite (click_injection, corp_dashboard, icon_silhouettes,
  sell_order, trades_tab) are the world moving under demoted goldens — **not** blessed, deliberately.
