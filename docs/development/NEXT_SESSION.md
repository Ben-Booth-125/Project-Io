# Sprint 48 running handoff — the world moves forward (OPEN)

Updated 2026-09-27, session wound down early (tokens). Read this, then `REFINED.md` § Sprint 48
(wave 3 and wave 3b list the chains) and the sprint 48 row in `sprints.json`.

## Main

Last CODE merge gated: 572a0afb (the sea fixes) — world_determinism 0C8A42270888B95C twice. Since
then main carries only docs and stores (the rulings below). Known failures on main: history_sim
R3a2/R3a3; exploration_sim R3b/R3d.1 (pins, re-bless); logistics T7-T10 (BL-1131); **new:**
road_generation_harness R2 "Highway reachable" (red since BL-1141 left 148 cities; re-read when
BL-1137 lands, noted on it); sea_lane_stamp_harness 16-seed "lays at least one lane" on seed 28
(earns none; pre-existing).

## Rulings taken 2026-09-27 (all in their docs)

NR-952 no ruins, a province's id recorded for play (reading taken: stable id at generation's end;
say if meant otherwise) · NR-953 A · NR-954 B the fill crosses the settled line · NR-955 B lane port
at a coastal seat · NR-956 A · NR-957 B carve counts firms and specialists · NR-958 A · NR-959 B sea
far-penalty 300 · NR-960 B firm cap 2 per rung (budget world ONLY) · NR-961 fleets bound their army
and are stopped by out-projecting defenders (target + mutual-defence partners) · scale credit reads
employed heads · the stream's pull is open work · works survive a sack · every centre-holding region
is a build candidate · the stream rate is picked from a re-run ladder.

## Branches NOT merged (each: finish fix round -> merge -> gate on main -> record)

| Lane | Branch `worktree-agent-…` | Items | State | Next |
|---|---|---|---|---|
| Centres | `ad3922191c20849ad` | BL-1137 rebuild (dccf7e05, d79e0c81), BL-1149 (ec5f2308) | BL-1149 review fix round (loud when no works table, call site pinned, loader rows, works-table determinism run) | then: the open-work pull, BL-1155 (every centre a work candidate; read "every span" vs "Industrialisation only"), re-run the 6/9/12/18 ladder -> Ben picks -> merge |
| Provinces | `a810ef485bc200aef` | BL-1150 (adc45b16 + fixes 67118846) | READY: centre_census C13 checks rivers per body on the 16 shipped seeds, red on 3 (12, 37, 38; main: 1) for NR-962 -- an expected red | merge + gate |
| Firm cap | `aee7aacb8b39cd83b` | BL-1146 (c6670be9 + fixes 97cb0cce) | READY: Pass 6 flat 2 again, legacy byte-identical to main on the 7 rows compared; one helper `province_anchors` (province.cpp); refused 6.0% -> 3.1% | merge + gate; re-read the firm census after BL-1150; spawn_solvency R4 red (NR-963). NOTE: `--digest-check --arc legacy` exits 1 on main too -- the pins predate BL-1130-1133 (re-bless) |
| Lane ports | `af645cfdcf963a520` | BL-1153 (80381678 + fixes 18442abe) | READY: laid 327 -> 377 of 446 (unlaid: 58 coastless realms, 5 same-port skipped, 6 unreachable); braids 0; degree keyed by port tile; mutations red | merge + gate |
| Sea | `acc0cdbc7ceec74b6` | NR-959 (300, c83d459f), BL-1152 (fleets project power, c4b5027e) | built behind a switch, both constants 0 (digests unchanged); P0-P6 rows with mutations; NOT cold-reviewed | measured: 88-90% of wet crossings launch from a hub with no built port, 69-77% with no fleet; ~165 mutual-defence pairs a seed. Readings taken: embark at the hub's coast, defenders project from built ports else the seat's coast, a non-aggression partner stays out. Next: NR-965, the ladders (a 14-seed partial pass in scratchpad lane-sea), span-time cost, a cold review |

Each lane's final report is in this session's transcript; every branch is cold-reviewed except
BL-1152. Merge world-movers in this order: BL-1150, BL-1146, BL-1153, then BL-1137+1149 at Ben's
rung, then BL-1152.

## Then

BL-1125 (market folds) on the thinned world -> BL-1138 (roads pull to markets) -> NR-944 re-read ->
BL-1145 (after BL-1150) -> BL-1107 -> BL-1139 (abandonment in play, recorded ids) -> BL-1151 (seat
menu re-anchored) -> the one re-bless (every cause named) -> the live walk. Centres: BL-1150 alone
takes 22,550 -> 13,599 pooled (~850 a seed); re-read the ~500 aim with BL-1137.

## Open for Ben

NR-962 (rivers divide banks?), NR-963 (rivals start armed?), NR-964 (the treasury spread by
employed heads?), NR-965 (the campaign scorer cannot see the fleet rule). The rate rung after the re-run ladder. Recommendations in each entry.

## Hazards this sprint taught

- Brief a ruling at its stated scope: widening "both sites" moved a pinned legacy row (BL-1146).
- A re-scoped test that went red when the change landed is a finding, not a fix (BL-1150 rivers).
- Lua-free harnesses build no works table: the stream and scale credit are inert there.
- Worktrees start stale (147-150 commits behind): every lane must fast-forward before measuring.
- TaskStop can leave a gate script running; new output folder per rerun. Five lanes on one PC:
  timings indicative. Main-session build: `scratchpad/main_session_build_rel.bat`.
