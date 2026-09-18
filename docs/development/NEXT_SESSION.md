# Next session — finish sprint 45, then cut sprint 46

Written 2026-09-18 at 21:30, as the PC went to sleep mid-run. **Read this, then
`node tools/session/sprints` (or `docs/development/SPRINTS.md` § sprints 45 and 46), then
`docs/development/REFINED.md` § Sprint 45.**

## First thing, in this order

1. **Chain 8** was running when the PC slept (started 20:55; the script is in the old session's
   scratchpad, so re-run it rather than look for its log): on main at or after `9f7a0e92`, run
   `player_seed_sweep --digest-check` (16 seeds, ~60-70 min), `exploration_sweep --out` then
   `seed_library.js --check --from`, `world_determinism`, `exploration_sim_harness`,
   `charter_refusal_probe`, `survey_endowment_harness`, `digitisation_sim_harness --fidelity`.
   A 16/16 digest check closes BL-1040, BL-1051, BL-1053 and BL-1041's R1 rows. Record the
   evidence on the requirement rows, then close the items as wave 0 was closed (`e8692579`).
2. **BL-1041's fix round** was in its lane (worktree `agent-a7e880239e1f2e4d6`) when the PC
   slept: six neutral fixes from its cold review (reading 8's headcount test against integrated
   heads, the missing original-vs-re-surveyed split, capacity-kind nodes only, tests for Defaults A
   and B, the switch set pinned, stale comments). If the agent cannot be resumed, snapshot its
   worktree diff, checkpoint-commit, and finish in the main session. Merge, re-run, record.
3. **BL-1039's timed rows** (Ben: first thing 2026-09-19, quiet machine, keep-awake, serial):
   ```
   bash tools/verify/build_lua_harness.sh player_seed_sweep
   ./build_gen/verify/player_seed_sweep.exe --charter-cost --seeds 0,28,46 --budget-scales 1,2,4 --resource-cap sqrt --density-ceilings 120,160 --specialist-prices 4 --no-extra --no-forced --out charter_cost_sweep_bl1039.json --note "BL-1039 sqrt c=8 (B on firms), goods in turn, ceilings 120/160, draw capital; quiet machine, main at <hash>, Release /O2 MSVC 14.44.35207, serial"
   node tools/verify/charter_cost_merge.js charter_cost_sweep.json charter_cost_sweep_bl1039.json bl1039_sqrt
   ```
   21 rows. Then file the R6 call: the density ceiling (120 or 160) read off them.
4. **Two open calls for Ben** (NEEDS_REVIEW.md): NR-896 (forest and coal both read the best
   held region, which grows with realm size; Charcoal now outnumbers Coke 319 to 200) and NR-897
   (the treasury-to-points constants; treasury is a median 69% of points, all on the capital).
   Both change what BL-1042 and BL-1043 measure — put them to Ben before BL-1042 starts.

## Sprint 45 — where it stands

- **Delivered:** BL-1034, BL-1036, BL-1037 (switch off), BL-1038 (switch off).
- **Merged, owed only the digest check:** BL-1040 (span, switch off), BL-1051 (span-open survey),
  BL-1053 (setup reads the 1960 close; the loading bar ends full, checked live by Ben).
- **Merged, fix round in its lane:** BL-1041 (industry points).
- **Merged, owed the timed rows and the R6 call:** BL-1039 (round 2: capital back to the draw, B
  on firm points, the ceiling fills goods in turn).
- **Not started:** BL-1042 (stockpile to budget), BL-1043 (real-stockpile sweep), BL-1050
  (order-independent reads — merged only with BL-1044), BL-1044 (Beat 1 ships: the re-bless).
- The wizard's "Loading the X round" wait gained a progress bar (`2387af55`, Ben watched it).

## Sprint 46 — proposed, cut it after sprint 45's re-bless

Ben ruled twelve calls on 2026-09-18 (NR-898, archived; sprint 46's notes carry them whole). The
cut owes: filing ~17 items from `sprints.json` sprint 46's `planned` list (the old session's
three-lane read is summarised in NR-898 and the planned rows; re-read the code where an item's
file:line matters), requirement groups, and the doc sweep's own item. Campaign tech state lands
per corporation (the existing earned_techs gates), not per nation — raise the scope flag. The
select-corporation screen needs a design session with Ben before it is built (BL-880, cancelled,
was its first design).

## Standing hazards

- Memory caps concurrency: 15.5 GB; a `player_seed_sweep` holds ~2.9 GB. Lanes never run it;
  check `systeminfo` before heavy runs.
- The PC sleeps ~22:00; long correctness runs resume on wake, timing runs must be re-run.
- The computer-use grant for ProjectIo resolves to a pruned worktree path; launch the app from
  `build/` via Bash for Ben to watch instead.
- `backlog_query --grep` does not search design text; continuity-pass items carry a
  `CONTINUITY PASS` marker in their design (BL-1045, 1046, 1048, 1049, 1052, 1055).
