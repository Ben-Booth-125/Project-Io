# Sprint 48 running handoff — the world moves forward (OPEN)

Updated 2026-10-02, wound down for the night. Read this, then `REFINED.md` § Sprint 48 (waves 3
and 3b) and the sprint 48 row in `sprints.json`.

## Main (all merged items gated there)

Merged and gated this session: BL-1150 (province fill crosses the settled line), BL-1146 (firm cap
scales with the centre; Pass 6 flat), BL-1153 (lane port at a coastal seat), BL-1156 (rivers divide
their banks), BL-1154 (rivals start armed; the seat opens unarmed), NR-959 + BL-1152 (fleets bound
their army and project power; 20 men a hull / halving 10 / conversion 50 in the Exploration age;
the Industrialisation age rule-off). Last gate: 14babd92, world_determinism C32FBDF09F66889A.

Known failures on main: history_sim R3a2/R3a3; exploration_sim R3b/R3d.1 (pins, re-bless);
logistics T7-T10 (BL-1131); road_generation_harness R2 Highway (re-read when BL-1137 lands);
sea_lane_stamp 16-seed seed 28 lays no lane; centre_census C13 C2b slope on seed 46 (BL-1158);
legacy `--digest-check` (pins predate BL-1130-1133; rivals now armed on the legacy arc too).

## The one branch NOT merged

`worktree-agent-ad3922191c20849ad` (centres): BL-1137 rebuild, BL-1149 (scale credit from works,
x4 employs, Ben's rung), the open-work pull, any centre with open work draws, NR-964 treasury by
employed heads, BL-1155 (every centre a work candidate), world_determinism and centre_region_bind on
the works table. Tip af836f93 was ready; a tail-review FIX ROUND was running at wind-down (capital
always a work candidate — a reading taken, Ben may overturn; rows for BL-1155, NR-901, two
destinations; s1 restored; heads moved split corridor vs own-region; stale comments; seed_library
--check). Its last report gives the commit. NEXT: merge -> gate (world_determinism is now a Lua
build) -> re-read road_generation_harness R2.

Readings at x4: urban 26.0%, centres 13,374 (341-1,207 a seed), 2,908 cities, employed 87.7%,
points per world 49.6M, C15 16/16, pre-1660 moved only by BL-1155.

## Rulings since 2026-10-01 (in their docs)

NR-962 rivers divide banks · NR-963 rivals armed · NR-964 treasury by employed heads · NR-965 a
refused crossing is not a candidate · the pull is open work, any centre with open work draws, rate
12 · works employ x4 · the ~500 aim retired: a world carries what its forces leave (~850) · fleets
20/10/50 in the Exploration age; the industrial age rule-off until BL-1157.

## Then

BL-1125 (market folds) on the thinned world -> BL-1138 (roads pull to markets) -> NR-944 re-read ->
BL-1145 -> BL-1107 -> BL-1139 (abandonment in play, recorded ids) -> BL-1151 (seat menu
re-anchored) -> the one re-bless (every cause named: BL-1130..1156, rivals armed on legacy, the
fleet rule, works x4) -> the live walk. Filed, not in the sprint: BL-1157 (fleet upkeep and
rebuilding; then industrial fleet values), BL-1158 (slopes divide weakly).

## Open for Ben

Nothing in the queue. The capital-always reading on BL-1155 is his to confirm.

## Hazards this sprint taught

- Brief a ruling at its stated scope (BL-1146's Pass 6). A re-scoped red test is a finding.
- Lua-free harnesses see no works table; world_determinism now builds Lua (build_lua_harness.sh).
- Requirement group names can collide with archived groups; check before pushing a group.
- A `git merge` can fail on a transient lock with a clean tree; retry once and read the message.
- Worktrees start stale; every lane merges main first. New gate folder per run.
