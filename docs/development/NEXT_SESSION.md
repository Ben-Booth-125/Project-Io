# Sprint 50 running handoff — logistics and trade flow (updated 2026-10-08)

Read this first after a compaction or in a new session. Then `REFINED.md` § Sprint 50 and the
sprint 50 row in `sprints.json`. Mode for this work: **Delivery — Full**.

## Where the work lives

- **Worktree:** `C:\Users\benbo\Project-Io\.claude\worktrees\sprint-50`, branch
  `worktree-sprint-50`. All sprint 50 work merges here, **not** to `main`. `main` is checked out
  in the main repo folder (another session's sprint 51 prep sits there, one unpushed commit
  `30457d2d`); this session cannot move `main`.
- **Ben: ignore the UI branch on main.** `s50-ui-to-main` (main + lens, strip, Quick Start) is
  built and unpushed; Ben said ignore it (2026-10-08). The UI reaches main with the sprint.
- Git in this isolated session: run plain commands only (no loops, no `$(...)`, no `bash script`,
  no `cmd`/`powershell`); call `./tools/verify/build_lua_harness.sh <name>` and `./build_app.bat`
  directly. `next_id.js` takes >5 min (many branches): run it in the background, no timeout.
  Stores (backlog/requirements/sprints/NEEDS_REVIEW .json) are 1-space JSON with CRLF; edit with a
  node script in the scratchpad that reads, splits CRLF, and writes `JSON.stringify(o, null, 1)`.

## The gate and where it stands

`market-viability` skill (`tools/verify/market_viability.cpp`). **Targets raised by Ben
2026-10-08: G1 >= 85% running at handoff (every non-running plant counts — Ben kept G1 as is),
G1b <= 5% input-starved (now or decommissioned after starving), G2 >= 50%, G3 >= 70%.**

| 16 seeds | sprint start | integrated 2026-10-08 (88304efb+) |
|---|---|---|
| G1 | 54.7 | **71.4 FAIL** |
| G1b | — | **12.8 FAIL** |
| G2 | 54.4 | 73.4 |
| G3 | 88.9 | 97.7 |
| starved/read | 47.0 | 24.6 |

The 5 tuning seeds (0,43,10,28,38) read ~80 G1 — optimistic; judge on 16. Per seed, non-running
plants are now mostly 'other' (decommissioned / no workforce / unsupplied, 30-57) vs starved (5-26).

## In flight

- **IDLE16 diagnosis lane** (agent `abed8eabc82b0e9cb`, worktree branch
  `worktree-agent-abed8eabc82b0e9cb`): `tools/verify/handoff_idle_probe.cpp`, 16 seeds — classifies
  every non-running processor at handoff (decom by whom/why; nolab; unsupplied; still-starved) with
  a healthy-or-defect verdict and fix family. **Ben chose this as the next step (2026-10-08).**
  When it reports: cold-check the claims, merge the probe, then bring Ben a lever form. Build nothing
  before he picks.

## Merged this sprint (all cold-reviewed; see backlog rows for detail)

- BL-1223 gate logistics row; BL-1222 trade-flow lens (sprint 51 owns its player polish + unmet-want
  view); BL-1225 every lens on strip (Ben liked); Quick Start + loading bars at 15% (Ben's click passed).
- Lever D (background demand consumes) ON; BL-1226 background pull split by catchment, razed
  skipped, electronics KEPT (NR-981 resolved).
- BL-1227 idle mines (complete): stack-rank estimate; forecast veto + dead-market veto; a bid =
  demand + silenced want + off-book WANT (held for the cadence) + launch fuel + own-pool upkeep +
  running consumers (DISCOVERY: running is observable); chain-start grant (AI_OPPONENT § 11);
  **save v38**; follow-up merged (records never read ahead; procurement wants across lead time;
  chain start restored after BL-1234's gate).
- BL-1229 steel stays home (complete): an order is a floor, not a hold.
- BL-1230 power crosses markets (complete): wired provinces join a grid; market shelf on its
  centre's grid; grid-level shelf cap.
- BL-1232 power plants per grid (complete): generation sized per grid (output keyed by market
  centre, unpowered first, unreachable grid never holds others up); solver reads power on its grid
  and prices inputs at posted price. **AI power gate HELD** on `bl1232-gate-remeasure` (Ben).
- BL-1233 processors to inputs (complete): spare-supply test at t_idle in every pass; refused
  processor's in-reach want enters derived demand.
- BL-1234 settle scorer starves (complete): new processor judged on supply; under-construction
  plants count against spare; landed-cost pricing.
- Dropped by Ben: lever A (pre-draw pricing), the stack-aware ranking round (ee4c10ca).

## Open for Ben (NEEDS_REVIEW)

NR-980 (lens 'no room' shows rivals' cargo), NR-982 (order-floor readings), NR-983 (spare is a
conservative estimate), NR-984 (unheld want lands at the capital market), NR-985 (unreachable grid
doesn't hold up others). BL-1235 (dial hold outlasts the loss reflex) is his call too.

## Filed follow-ups (not this sprint unless pulled in)

BL-1224 dust convoys; BL-1231 broken harnesses + probe drift; BL-1236 plant builds stall (incl.
stalled sites reserving their draw); BL-1237 grid keeps its generator; BL-1238 half-plant on
settled need.

## Then (the plan after the G1 round)

1. Lever form from the IDLE16 diagnosis -> build -> cold review -> merge -> 16-seed gate.
2. Wave 3 (after G1 work, so it doesn't move what is being measured): BL-1195 convoy lane follows
   legs; BL-1119 roads tree and detour R4/R6/R7.
3. Close: **the doc check** (REFINED.md § Close — every ruling in its doc, code matches, grep old
   wording in siblings; Ben asked for it); BL-1165 untraced re-bless movements; the one re-bless
   (goldens and pss digests have moved — never re-bless without Ben); retro; version cut.

## Hazards learned

- Lanes merge `worktree-sprint-50` into their branches: merging a lane can drag in OTHER unreviewed
  lanes' work (happened with the settle probe) — check `git diff --stat HEAD~1 HEAD -- src` after
  every merge.
- Every lane round has needed a cold review; most found a real defect. Budget it.
- "Apply, don't flag" stops at visibility and AI-grant boundaries — raise those (memory).
- Parallel lanes editing the same function (corp_ai processor candidate, corporation_generation
  walk) conflict; tell the later lane to merge the sprint branch before measuring.
