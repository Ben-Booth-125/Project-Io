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
  **REPORTED (1b58926a, unmerged probe):** reproduces G1 71.4 / G1b 12.8 exactly. Of 958
  non-running: **503 (52.5%) the scorer's workforce dial zeroed on settle tick -11** (reading its
  own market's opening glut — e.g. food rations 8.50 vs base 13.60; buyers elsewhere), never
  re-raised in 11 ticks, then the loss reflex mothballed them (maintenance-only loss) — ~10 points
  of G1, all generation-built plants; 102 of those are propellant_atmospheric with no output price
  (a generation defect). 430 input-starved (G1b): food_rations -> consumer_goods the largest gap.
  Labour not a cause. Upper bound if the dial-zeroed plants ran: G1 ~82-86%. Ties to BL-1235 (dial
  hold vs reflex). **Cold check DONE:** mechanism confirmed in code (dial zero -> ai_cooldown hold
  blocks re-raise until ~T+16 -> reflex mothballs at T+8, no exemption); overstated: steel /
  refined_fuel read demand 0 at their own market EVERY tick (buyers elsewhere — a permanent
  own-market misread, not a start-up glut); '~10 points' untested; propellant has no base price
  anywhere (authoring defect). **Round 2 running** (same agent): census of the 503 by demand 0/>0
  with the solver's own forecast, counterfactuals (no settle dial-zeroing / reflex skips dial-idled /
  solver reads demand pooled over reachable buyers), the sell-order leak. **ROUND 2 DONE, probe
  merged:** of 503 zeroed: 223 no local bid (180 no bidder ANYWHERE that tick — steel, refined fuel,
  silicon, clean water; 216 profitable at base), 178 true start-up glut (recover by handoff; not
  re-dialled), 102 unpriced propellant. Counterfactuals (16 seeds): no settle zeroing G1 83.5 at
  handoff but 62.7 at t50 (rescued plants mothballed in play); reflex-skip 71.4 / t50 63.4 / G2 76.7;
  body-pooled demand 72.8. Sell-order leak: not a factor. Hypothesis: processors silenced by the
  fair-price ceiling never show as demand, so the dial sees no buyer — the dial reading silenced want
  is a NEW AI input (grant). **Lever form sent to Ben** (dial reads silenced want: measure first /
  grant / no; hold & reflex; propellant pricing; G1 at t50). **BEN RULED (bd1cd032):** measure the
  dial reading silenced want first (IDLE16 agent round 3); BL-1235 designed — a dial-idled plant is
  not losing, the hold ends when its forecast recovers (lane DIALHOLD, agent `a1b28029261a2f580`);
  propellant gets a base price derived from its inputs (lane PROPELLANT, agent `a14f239f7a0f14bad`);
  **G1 >= 85% at tick 50 too** (gate prints it). **ROUND 3 DONE, merged (a5734335):** silenced
  want (`hauler_want`) is 0 at EVERY zeroing market — steel 42 events (3 with demand, 0 want),
  refined_fuel 123 (0/0), food_rations 159 (all demand, 0 want). Dial reading it: G1 71.5, moves 2
  plants — the silenced-want lever is moot. Body-pooled 73.1; BL-1235 variant 79.5 handoff but
  **t50 62.2**, G1b t50 ~25%; pooled+BL-1235 80.0. **No variant holds t50 past 62.6.** Open
  question is upstream: why does no market bid steel / refined_fuel / silicon / clean_water at
  settle start (how industrial demand is registered). **BEN RULED (2026-10-08, round 4 form):**
  drop the silenced-want dial lever; keep G1 85% at t50 and EXTEND the sprint; round 4 diagnoses
  industrial demand registration + t50 decay under BL-1235. **Round 4 lane** runs in hand-made
  worktree `.claude/worktrees/s50-round4-demand` (branch `s50-round4-demand`) — Agent isolation
  refused ("git identity could not be verified") though git answers fine. **ROUND 4 DONE, probe
  committed 57c85a35** (copied in; the agent could not commit). Findings: silicon/clean_water
  healthy (round 2 mislabelled — plants had switched recipe); STEEL consumers (244) live off the
  generation opening stock (`corporation_generation.cpp:913-948`) for 2 ticks, want-net-of-pool
  (NR-281) bids nothing, the dial reads that empty -12 register and its hold outlasts demand's
  arrival at -9; REFINED FUEL has no live consumer (propellant unpriced -> zeroed; ~34 producers
  per seed vs ~0.6 consumers); reflex rescue `output_ratio` (economy_system.cpp ~2820) reads an
  unpriced output as 1.0 = healthiest, so floored fuel plants switch INTO propellant (spot-checked,
  true). t50 (shipped code, NOT bh — agent's src switch was denied): of 665 lost, 408 mid-chain
  starvation (steel 79 top input), 158 inputs >= revenue; dial little. Fix menu to Ben: opening
  stock / dial abstains on pre-demand registers (scope) / dial reads composite bid (GRANT) / count
  stock-fed use as demand (overturns NR-281) / rescue reads unpriced as floored / fuel over-placement.
  **BEN RULED (2026-10-09, round 5 form; docs bb09107e, worklist REFINED § Wave 2b D1-D8):** NEW
  GRANT — the dial reads the build bid (AI_OPPONENT § 11); dial forecasts at base where no fact
  exists; rescue reads unpriced as floored; opening stock held until bid; no processor beyond its
  output's want; diagnose t50 starvation in parallel. Lanes: GEN D5+D6 (generation-dev, isolated
  worktree) and STARVE D7 (economy-dev probe) running; D3+D4 (dial + rescue) wait for DIALHOLD to
  merge (same function). The hand-made `s50-round4-demand` worktree can be removed. Then merge the probe and
  bring Ben a lever form (dial reads past the start-up glut / reflex skips dial-idled plants / dial
  re-raises; generation must not place unpriced-output recipes; mid-chain starvation).

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
