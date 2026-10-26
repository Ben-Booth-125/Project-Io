---
name: rebless-shape-probe
description: Describe a Project Io digest re-bless in world SHAPE, not hash - run tools/verify/rebless_shape_probe on both sides of the re-bless (the tree before the wave's movers and the tree after) and compare the per-seed ROW and MIX lines. Use at every sprint re-bless, before re-pinning any digest or gate floor.
---

# rebless-shape-probe

`tools/verify/rebless_shape_probe.cpp` prints one `ROW seed=<n> key=value ...` line per seed
(centres, scales, provinces, markets, roads, wars, sea, buildings, seats, haulage, treasuries) and
one `MIX` line (live buildings on the home body by extraction target and processor recipe). It is
built to compile against BOTH sides of a re-bless: a field one side lacks prints -1, never zero.
It REPORTS; it does not gate (Ben, 2026-10-26, NR-966: the one probe each re-bless uses).

DELIVERY.md § The digest re-bless is one act per WAVE asks a re-bless to state what changed in
world shape rather than in hash. This is that instrument.

## Run

```bash
cmd //c "tools\\verify\\build_lua_harness.bat rebless_shape_probe"
build_gen/verify/rebless_shape_probe.exe            # the 16 curated seeds
build_gen/verify/rebless_shape_probe.exe --seeds 0,28,46
```

For the BEFORE side, build it in a worktree checked out at the wave's base commit
(`git worktree add <dir> <base sha>`), from that worktree's root. Save each side's output and
diff the ROW lines field by field per seed.

## At a re-bless

1. Run both sides; pool each field across seeds (mean and range).
2. Name each mover: the commit or ruling whose change explains each shape movement.
3. Write the shape summary into the re-bless's requirement row and the sprint's retro, then
   re-pin only on Ben's go.

## Rules

- A side the probe does not build against means the probe needs a `requires` guard for the
  missing field, not a skipped side.
- Never re-pin a digest the shape summary cannot explain.
