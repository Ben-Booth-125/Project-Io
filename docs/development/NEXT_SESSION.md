# Sprint 48 running handoff — the world moves forward (OPEN)

Updated 2026-09-26 afternoon. Read this, then `REFINED.md` § Sprint 48 (wave 3 lists the
chains in order) and the sprint 48 row in `sprints.json`. The DEVLOG's 2026-09-25 (evening) entry
is the session record up to the first merges.

## Landed on main (all gated there)

- **BL-1084 (world built once and moved):** the cursor, the wizard and its review fixes;
  byte-identical proof. Live clicks R4-R6 still owed at a walk.
- **BL-1117 (settle tick one), BL-1126 (path cost reads the cache, CLOSED), BL-1114 (epoch 0
  retired, CLOSED), BL-1119 rounds 1-4 (roads tree and detour).** BL-1119 R5 (the 35 s tail)
  FAILED on seed 0 earlier; re-read owed quiet (the settle step read 49.6 s on seed 0 under load).
- **BL-1130 (centres consolidate) + review fixes; BL-1133 (a province is its centre's ground).**
- **BL-1132 (settle spacing 3 tiles, room-aware) + review fix:** regions 43,130 -> 15,194 pooled;
  a Settle refuses a scheduled founding's ground (NR-956); C10 too-close pairs 0. War ~1.6x.
- **BL-1141 (a region deepens into one centre):** centres 42,251 -> 22,521 pooled, 9.6% of land;
  the history unmoved; the partition harness's D A1 now passes.
- **BL-1136 (search on two threads) + review fixes; BL-1086 (the carve counts the budget's planned
  firms) + review fixes:** the plan honours the 120-a-body ceiling; the loading screen's charter
  list published again (live click owed, cold path --autostart seed 0).
- **Sea: BL-1120 currents, BL-1140 lanes from trade, BL-1098 lanes stamped (save v28).**
- **UI:** the bowed-arc lane, roads carried across rounds, the Culture round worded as the record.
- **Tools:** stockpile_budget_check --firm-census (BL-1146).

## Reverted

- **BL-1137 (industrial urbanisation)** merged b591d1e9 then reverted 8cbdbc9d: it created carrying
  capacity (population 6.08B -> 10.92B) and a sack deleted ~40% of a city silently.

## In flight (worktree lanes)

- **Centres:** BL-1141 fixes + BL-1137 rebuilt under NR-958 (a migrant carries its food; a sack
  never lowers a ceiling; razing counted uncapped; conservation rows).
- **Sea:** BL-1142 (far pairs across water, cargo against the current) + the four-way lane walk,
  built on its branch, NOT merged: fix round running (stale ladder, seat-move break, loss only on
  sea legs, seller's navy). Then BL-1147 (naval points carry over; Ben's ruling today).

## Then

BL-1125 (markets fold: twins, gravity aimed at 20-40, conquest) on the thinned world, after the
BL-1137 rebuild -> BL-1138 (roads pull to markets) -> NR-944 / NR-954 re-read -> BL-1145 (province
front-tile readers) -> BL-1107 -> BL-1139 (abandonment in play) -> the one re-bless (every cause
named; the pins R3b/R3d.1 and every digest are red by design until then) -> the live walk.

## Open for Ben

NR-952 (ruin fallback), NR-953 (scale weights reach), NR-954 (hemmed villages, anchors, giant
provinces; ~half of all centres are anchors), NR-955 (inland ports, lane vs road cost; option E
added), NR-956 (pending ground reserved), NR-957 (what the carve counts), NR-958 (the stream
conserves), NR-959 (sea far-penalty; held for the re-run ladder), NR-960 (the firm cap).
Filed for later sprints: BL-1143 (asymmetric hex table), BL-1144 (colonial distance gates inert).

## Hazards this sprint taught

- A lane report with a large shift in a conserved quantity waits for its cold review before it
  merges (BL-1137 cost a revert). Six of seven reviewed merges carried real defects.
- TaskStop can leave a gate script's bash running; give every rerun a NEW output folder and check
  log mtimes against the run's start.
- Five lanes on one PC: every agent timing is indicative; take headline numbers quiet.
- `--verify-all` carries state between scripts; run a failing wizard script alone first.
- Main-session build: `scratchpad/main_session_build_rel.bat` (writes BUILD_REL_EXIT). The
  "ProjectIo" Start-menu entry points at an old worktree build; screen access needs the right exe.
