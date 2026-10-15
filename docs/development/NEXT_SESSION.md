# Sprint 55 handoff — spectator to the end of Era 1 (2026-10-15)

Read this first in the session that opens sprint 55. Sprint 53 (the player is a nation) is closed
and cut as **v0.1.32**; its retro is in `archive/sprints-2026-Q4.json`. The previous handoff
(sprint 53) is in git history at `9a8f2832`.

Sprint 55 is planned in `sprints.json`: BL-1324..BL-1333, pacing first, space in wave 3.
**Mode to open with: Design** for the open calls, then **Delivery — Full** (BL-1327 (space asset
model) touches the save format).

## What is open

| Item | Note |
|---|---|
| BL-1310 (nations contiguous) | From the 2026-10-15 live click. Tagged sprint 56 in the backlog. |
| BL-1311 (ground paint perf) | From the same live click. Tagged sprint 56. |
| BL-1297 (sphere of claim gap) | Proposed; not built. |
| BL-1302 (war aftermath) | Designed; not built. |
| BL-1277 (tech wave re-bless) | Owed. Sprint 53's and 54's world movers fold into it. |
| BL-1308 (state build UI readings) | Filed; not built. |
| Rail track-per-tile tedium | Laying track one tile at a time is tedious. No item yet — Ben's call. |
| Rate 1 and the treasury band | The economy pass re-laddered to rate 1. Does the treasury band still hold? Ben's call. |

Also unbuilt from sprint 53: BL-1275 (nation research AI), BL-1285 (quest deed state),
BL-1282 (minor nations shallow), BL-1284 (player nation verbs).

## Gate baseline

Measured on main 2ddffb36 (2026-10-15), par_seeds full tier:
- `market_viability`: G1 89.1, G1 t50 86.2, G1b 3.5, G2 113.2, G3 89.6 — all five pass.
- `history_patterns`: rise and fall, late culture, world war, civil war 16/16; revolution 7/16.
- `world_determinism`, `exploration_sim_harness`, `save_roundtrip`, `player_seed_sweep --guard`,
  `nation_targets_harness` (quick), `state_industry_harness --strict`, `railway_harness`,
  `lp_pool_harness`, `nation_tech_harness`: all pass. `history_sim_harness`: R3a2/R3a3 only.
- Open call: the treasury->industry rate sits at its floor (1), so treasuries feed ~8% of industry
  points, and nation chest medians (750-1,200) sit partly under the 1,000-2,000 band.

## Traps learned

- **Agents yield mid-wait.** Resume them: "read the output now, finish, commit".
- **Ids collide across parallel sprints.** Sprint 54 ran beside 53 and minted the same ids.
  Merge main before `next_id.js`, and scan cold as well as hot.
- **Pattern targets interfere.** One history pattern's lever moves another's rate. Run the
  whole pattern set (`history_patterns` through `par_seeds`) after any single pattern change.
