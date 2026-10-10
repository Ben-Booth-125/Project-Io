> **Sprint 51 CLOSED 2026-10-10, cut as v0.1.29** (visibility: the ground and the opening market; merged to main).
> Carried to sprint 52: BL-1247 (ground benchmark: the click-through painting wait is 46-68 s), BL-1293 (anchor rings
> clip, cause not found), BL-1255 (rail rung). All five live clicks passed 2026-10-10; Ben called the sprint a success.
> Local main holds the close unpushed (4fee131e+); bring it onto origin from another session. The sprint 50 handoff below is unchanged.

# Sprint 50 running handoff — logistics and trade flow (updated 2026-10-08)

Read this first after a compaction or in a new session. Then `REFINED.md` § Sprint 50 and the
sprint 50 row in `sprints.json`. Mode for this work: **Delivery — Full**.

## SHELF ECONOMY AND TRADE MERGED; RE-BLESS DONE — next is the cut (2026-10-10, later)

The shelf/trade branch is merged into `worktree-sprint-50` with the sprint's ONE re-bless, authorised
by Ben against the shape (`docs/development/drafts/sprint-50-shelf-rebless.md`; DEVLOG 2026-10-10
(later)). BL-1265..BL-1270 complete (BL-1268 on Part A only). Gate at merge: G1 93.3, G1 t50 87.5,
G1b 5.7 (accepted by Ben), G2 113.4, G3 99.8. Ben (2026-10-10): **move towards cutting**; the fixes
(trade about 87% lower, cities demoted, G1b, the estimate's optimism) wait for a later sprint whose
items he files himself — do NOT file them. No trade-flow floor in the gate.

**Done at the shelf close (2026-10-10):** visual checks on the merged tip — trades_tab 49/49 (the
script now grants the player a Marketplace: the seat opens with no trade building, so a manual trade
had nowhere to leave from; the tab's refusal now names that rule), sea_lane and air_gate pass;
`ruling-check` — nine 2026-10-10 rulings added to the sprint 50 register (54, 0 failing; 82 dated
rulings, 0 flagged), three stale passages fixed; archive — 20 landed backlog items and 91 resolved
review entries cold (69 / 16 open remain). **Owed:** retro, version cut, integration onto main (Ben
brings the branch onto main in another session); a live click of the Trades tab's new refusal text.

## (superseded) SPRINT 50 IS EXTENDED — read HANDOFF_SHELF_TRADE.md first (Ben, 2026-10-10)

Ben redesigned the economy's spine mid-close: corporation pools retire (every good is on a market-owned
shelf), and trade becomes the only way goods move between markets (Planetary Marketplaces and Ports make
trade points; per-resource capacity; auto and reserved trades; AI manual trades; trade through all three
generation rounds). Design: docs/economy/TRADE.md, MARKETS.md § The shelf economy. Work: BL-1265..BL-1270.
**Another session builds it** — its handoff is docs/development/HANDOFF_SHELF_TRADE.md. Sprint 50 stays
open; ONE re-bless after it lands. BL-1165 (untraced re-bless movements) is complete (097dc8d0; Ben ruled
all five traced movements as they stand, ce4e982c). Nothing else is in flight in the sprint 50 session.
Owed at the close, after the redesign: the 16-seed gate, the visual checks (sea_lane's convoy now comes
from a trade), `ruling-check`, the ONE re-bless with Ben's go, retro, version cut, integration onto main.
Cleanup for Ben: `.claude/worktrees/bl1165-x\` (~30 exported commit trees; rm -rf is denied here).

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
  merge (same function). **DIALHOLD DONE** (c0dde0d3, branch worktree-agent-a1b28029261a2f580;
  in cold review): G1 71.4 -> 80.1 handoff, t50 61.6 -> 61.9, G1b 12.8 -> 14.6 (worse), G2 75.8,
  G3 98.1; of the 503, 319 run at handoff / 262 at t50; history_sim R3a2/R3a3 fail on the base
  too (pre-existing). **MERGED 3bca733a** after a clean cold review (probe hunk dropped, tip's kept);
  app builds. Review raised NR-986 (seat inherits dial-zeroed plants at 0 — back to auto?). **D3+D4
  lane (dial reads build bid + base forecast; rescue unpriced = floored) launched** on the merged tip.
  **PROPELLANT DONE** (01bd8b63, branch worktree-agent-a14f239f7a0f14bad, base bd1cd032; in cold
  review): base 41.8 (electrolysis route, 2x(inputs+0.75 wage)+0.1; atmospheric anchor would be
  64.0 — Ben's call). Alone: G1 71.4 -> 73.2, t50 61.6 -> 59.8, G1b 13.5. Generation now builds far
  fewer propellant plants (166 -> 17); unpriced-zeroed 102 -> 0. Open: launch propellant is not
  reserved, so clearing sells the pool each tick (unmeasured — call); PRODUCTION ~494 stale line.
  **COLD REVIEW: HELD.** 41.8 correct by the doc (64.0 is not what the doc says); merges clean.
  SEVERE: auto-surplus (`market_clearing.cpp` ~1491-1511) now lists the whole pool every clear
  (`processor_reservation` ~228 reserves recipe inputs only), so the launch burn
  (`supply_system.cpp:356`) sees 0 — the player's `dispatch_convoy` on a space lane is always
  rejected; breaks ACTIONS dispatch_convoy, PRODUCTION Launchpad ~489-495, ERAS:294 reserve gate.
  Fix options to Ben: (a) reserve launch propellant for a pad's pool (Full, clearing seam) /
  (b) launch burn buys from the shelf / (c) accept + rewrite docs. Also: electrolysis (the
  "airless" route) is cheapest everywhere — nothing gates it to airless bodies (inverts PRODUCTION
  :280). Merge note must say "world-moving". **Propellant form sent to Ben** (launch fuel a/b/both/
  drop; gate electrolysis to airless; NR-986 seat back to auto).
  **D3+D4 DONE** (d0ac1de7, 11acaf69, branch worktree-agent-a472a14f8f39f276e; in cold review):
  base 80.1/61.9/14.6/75.8 -> D3 81.4/63.7/13.9/72.9 -> D3+D4 83.0/67.7/15.6/69.7 (G1/t50/G1b/G2).
  Steel zeroings at -11 unchanged (48) — listed opening stock still reads as glut; D5 owns it. D4:
  switches into propellant 144 -> 0.
  **D7 DONE, probe merged f98d43df** (`starve_trace_probe.cpp`, shipped code): of 665 lost by t50,
  386 starved. Causes: ceiling-silenced while shelf holds 105 (symptom of drawdown); background pull
  55+12 (ruled cost; cf pull-off G1t50 -1.4, net positive); households 25+20 (ruled); other
  processors 38; opening stock ran out 61 (D5/D6 may move); stock held in other pools unlisted 42
  (reservation vs order floor unsplit); upstream died 18. Ceiling-off cf: -8.0. STEEL root: a
  background-firm CONSTRUCTION BOOM — 725 sites drawing steel on 33 starving markets at t50, 701
  background-owned (ports/hubs 264, food 181, fuel 79...); site need 373/tick vs processors 35.
  Copper ore: dispatcher "would send" 36/47 yet arrivals ~8/tick (undelivered hauls — SUPPLY,
  untested). Making builds read steel scarcity = NEW GRANT. 155 inputs>=revenue: 80 underwater at
  tick 1 (35 unpriced propellant), input side dominates.
  **BEN RULED (2026-10-09, propellant form; docs 2e608ec0):** pad's pool keeps its propellant
  (reserved from auto-surplus); electrolysis only on airless bodies, atmospheric only under air ->
  price 64.0; seat's dial-idled plants back to auto at handoff (NR-986 resolved). PROPELLANT agent
  resumed with all three. **D3/D4 cold review:** F1 dial double-counts shelf-fed running draws
  (+ procurement whole-contract per tick) — likely the G2 fall; F2 D3b base forecast fires on dead
  markets in play (no never-cleared gate) — likely the G1b rise; F4 probe solver args; F3 D4 also
  switches OUT of unpriced incumbents (ruling-literal, kept). D3/D4 agent resumed with F1/F2/F4.
  **D5+D6 DONE** (53a8d92c, 359f1b11, branch worktree-agent-a5d82d46e2c9f9d33; in cold review):
  71.4/61.6/12.8/73.4 -> D5 71.6/60.5/12.7/94.4 -> D5+D6 77.9/67.6/12.0/95.2 (G1/t50/G1b/G2).
  Built processors at handoff 3351 -> 2752 (denominator shrinks); G2 jump = lower settle-income
  baseline (held stock not auctioned) — read with care. D5 adds `world::opening_stock_held`, SAVE
  v39. D6: fuel plants 538 -> 0 at generation (all via make_chain_feasible's no-good-named branch:
  451 attached to extractors); only the traced path bounded. All 16 digest pins move (expected;
  re-bless at close with Ben). Agent's open questions: bound the second processor too; want omits
  refused draws; D5 bid excludes silenced want; convoys may haul held stock.
  **D5/D6 COLD REVIEW: HELD.** G1 gain is mostly denominator — running at handoff FELL ~249
  (2393 -> ~2144); G2 not evidence (settle baseline moved); measured on the pre-BL-1235 base.
  D5 sound (bid definition defensible: silenced want excluded per FINANCE 2026-10-03; composite would
  release on clear 1). D6 bounds one path of several: (d) chartered processing firm, (e) hard-coded
  roster default recipes, (f) enforce_chain_feasible_roster keep/readmit, (g) assign_default_recipes
  re-runs — unbounded; want omits refused draws, uses body_demand not walk's consumer_demand, power
  body-wide. Fix round sent to the D5/D6 agent (+ absolute counts in the gate line, save row,
  dispatch comment). **Call for Ben:** with D6, refined fuel -> propellant is a cold start at
  generation (no fuel maker -> propellant not feasible -> fuel has no derived demand); space access
  then depends on settle/play building both. Ruling arguably accepts it; unmeasured.
  **BEN (2026-10-09):** "So long as most placed industries are running, and we expect most
  companies to make money, then that's not a problem. We can push the long term viability work for
  another sprint, after the various other gameplay loops are settled." Read as: the smaller
  denominator and the fuel cold start are fine; t50 and the D7 starvation findings (construction
  boom, undelivered hauls, pool-held stock) move to a later sprint — reading put to Ben to confirm.
  **D3 FIX ROUND DONE** (3a5d01ff; re-review running): new `market_component::unposted_rate`
  (dial reads a per-tick rate; veto unchanged), SAVE v40 (stacks on D5's v39 — merge D5/D6
  first), shared `market_has_cleared` (D3b now near-inert). 16 seeds: D3fix 81.2/65.9/13.8/73.4;
  +D4 82.2/71.1/16.2/71.3 (G1/t50/G1b/G2). G1b rise is D4's. Steel zeroings unchanged (D5's).
  **RE-REVIEW: F1 NOT CLOSED.** (1) procurement rate = Q/(lead left) climbs to the whole contract on
  the last tick, then is held after delivery; (2) space-programme lump noted whole every tick it
  accumulates; (3) top-up pool draws can also sit in posted demand (double count); (4) the register
  overwrites on a new tick, so sources written after the scorer (upkeep, launch fuel, nation wants)
  vanish wherever a processor records first. D4: rescue treats an incumbent with ONE unpriced
  byproduct as floored every tick, onto reach-obtainable (not stocked) recipes — plausible G1b rise;
  moot once propellant is priced. Doc: § 11 says "same composite bid"; dial now reads a different
  quantity. Merge mechanics fine (v40 over v39 trivial). **Form to Ben:** narrow the dial to demand +
  pool-fed running draws / fix every source / hold D3 and measure D4+D5/D6 first; confirm deferral.
  **PROPELLANT REWORK:** e07ac0b5 pad's pool keeps propellant (`auto_surplus_reservation`;
  dispatch reads it too; corp_ai `listable_surplus` still sees it as sellable — allowed by ruling
  via order) + 6deacb4a seat's dial-idled plants back to auto (both handoff paths). Measured at
  6deacb4a (incl. merged BL-1235): 82.9/66.9/15.7/73.3. Commit 2 (route gate) BLOCKED: no atmosphere
  on body_component — main session chose option A (copy atmosphere_class onto body_component, SAVE
  v41), price 64.0, recipe_margin anchors on the air route; agent resumed. Save order: v39 D5, v40
  D3, v41 PROPELLANT. Airless = atmosphere class none|thin (planetology's own reading; PRODUCTION
  f8b48923); test worlds default moderate.
  **BEN RULED (2026-10-09, dial form; 3c0bd8a2):** dial NARROWED to posted demand + stock-fed running
  draws (AI_OPPONENT § 11 rewritten; § 2B sentence); **G1 at t50 and all long-term viability work
  DEFERRED** (sprint row done_when, SKILL.md, REFINED). D3 agent resumed with the narrowed rework.
  Deferred findings FILED as BL-1248 (plants survive to t50), 4acc101f.
  **D5/D6 FIX ROUND** (9f61b46c, 26867936): G1 85.8 PASS but running 2683 -> 2324 at handoff,
  3474 -> 2881 t50, play income -18.5%; G1b 13.2. Default-recipe paths (e/g) unbounded — form to
  Ben (first wanted recipe / not placed / idle). **Re-review:** cause of the running drop is the
  SECOND-WORKS CUT (a chartered processing firm's second works unplaced once the first covers the
  gap -> steel/copper/silicon/medical/water firms halve, upstream charters fall), not fuel switching.
  Defects: walk power want reads the unpowered-serve set; legacy Pass 6 power bound inconsistent;
  roster keep sweep suspends all over-coverers and cascades; second works' spare refusal drops its
  draw; save row can pass vacuously. Round 2 sent, with a keep-second-works counterfactual for Ben.
  **BEN RULED (70664ec1):** a recipe-less processor whose default output is unwanted is not placed
  (added to D5/D6 round 2).
  **D3 NARROWED REWORK DONE** (b0a9dbb0; in cold review): `dial_bid` = demand + held
  `dial_pool_draw` (per-processor room excludes units already posted; one writer after production;
  v40 now `dial_pool_draw`+tick). 16 seeds: D3 81.6/65.9/13.6/73.6; +D4 82.1/67.9/16.3/71.1. D4 drives
  the G1b rise; no by-product switches. Steel zeroings 47 (D5's).
  **D3 rework review:** room logic correct; veto bit-identical; F1 hold double-counts the
  pool->demand transition (record only written when q>0) — fix: stamp every running-touched key each
  tick incl. 0, hold bridges only untouched ticks (within ruling); F2 market_has_cleared may read a
  same-step unposted bid on a new market; F3 two stale comments; F4 vacuous v40 row. D4 G1b
  hypothesis: closed exit into unpriced propellant keeps fuel plants claiming contended inputs.
  Round 3 sent. Merge with D5/D6 conflicts: reservations jsonl, world_save.hpp (keep v40 + both),
  save_roundtrip.cpp (take D5's assertion style).
  **PROPELLANT COMPLETE** (e07ac0b5 pad, 6deacb4a seat, 4d48ef77 route gate + 64.0, save v41
  `body_component::atmosphere`; in cold review): 83.2/65.5/15.5/73.2/98.7 (G1/t50/G1b/G2/G3);
  electrolysis 0 plants anywhere (home bodies have air); 102 unpriced-zeroed -> 0. src/core edits
  (app.cpp, agent_protocol.cpp, verify_api.cpp) NOT compiled by the lane — build at merge.
  PROPELLANT review: logic sound; UI doors offer the wrong-air route (fix round sent: doors,
  ACTIONS.json, remaining default paths, verify name); docs fixed 7ed47883 (anchor clause, pad =
  body). Ben BACKLOGGED rival scorer/space programme draining pad fuel -> BL-1249.
  **D5/D6 ROUND 2** (f807e048, a504062b, eff4e96f): at a504062b G1 87.8 PASS (OFF) / 87.5 (keep
  second works ON: +120 running, +8% income), G1b 11.3. HEAD eff4e96f (default recipes unplaced)
  REMOVES THE STEEL CHAIN on every seed — the unowned pre-authored Kepler installation
  (hard_coded_world.cpp:2659) is the only steel seed. Handoff losses of steel/medical/water arise
  in the settle, not generation. **Form to Ben:** exempt the authored plant / walk seeds chains /
  accept; keep or cut second works. Do not merge eff4e96f until ruled.
  **BEN RULED (f16e65e9):** authored installation keeps its default; second works kept. Sent to
  D5/D6 agent.
  **D3 ROUND 3** (a37f2099; focused re-review running): draw register now written in clear_markets
  with demand (same tick); stamps 0 for touched keys; market_has_cleared reads only clear-written
  fields (veto identical over 2.5M calls). D3 81.7/65.4/13.5/71.8; +D4 82.4/67.6/15.9/68.8. D4 G1b
  cause: fuel plants denied the propellant exit switch into silicon/alloys and starve on
  silica/ree_alloy (+59) — moot once D6 removes the fuel plants.
  **D3/D4 READY TO MERGE** (head 6d79848a): round 3 re-review clean; `dial_bid_harness` D1-D6 pass
  and were shown able to fail. NR-987 filed (cleared-proxy decision). MERGE TRAP with D5/D6 in
  save_roundtrip.cpp: close D5's `if (held_key...)` block before the v40 block; reservations keep
  v39+v40(+v41); world_save.hpp constant ends at 41 with all three notes.
  **D5/D6 DONE** (head 067e0055; final review running): steel chain restored (gen steel 177, alloys
  16, consumer goods 112, machinery 28); 87.5/67.4/11.6/91.6/96.9; running/built h 2426/2773, t50
  2854/4234; play income 47,307/seed-tick. Authored installation identified as "the one recipe-less
  unowned processor" (review asked whether a marker is needed).
  **D5/D6 final review:** fixes 1,2,5,6 correct; keep sweep counts to-be-suspended plants as
  coverage (can destroy a covered chain — HIGH); walk books a stale rung refusal; installation by
  elimination; exception 2 uncapped (bounded by processing_mix); 11 raw-world harnesses unaudited.
  Round 4 sent (all five).
  **PROPELLANT READY** (head a354831b: doors filter wrong-air rows, switch shows refusal, ACTIONS
  updated, default paths ask `default_recipe_id_at`, verify names wrong_air). World unchanged
  (83.2/65.5/15.5/73.2/98.7). App build NOT run by the lane (syntax-only) and the Build-door live
  click is owed at merge. Merge order once D5/D6 round 4 clears: D5/D6 (v39) -> D3/D4 (v40, head
  6d79848a) -> PROPELLANT (v41) -> build_app -> live click -> 16-seed integrated gate.
  **ALL THREE MERGED (2026-10-09):** 54a22ff7 D5/D6, 6d04dfe1 D3/D4 (v40; save_roundtrip brace trap
  resolved by hand), 2aeb48ef PROPELLANT (v41; assign_default_recipes = D6's version +
  default_recipe_id_at per building). build_app BUILD_OK. Harnesses pass: save_roundtrip,
  dial_bid, unposted_bid, recipe_switch, recipe_margin, charter_refusal_probe, world_determinism
  (digests unchanged). **Integrated gate running** in two halves -> build_gen/s50_int_a.txt /
  s50_int_b.txt (pool by hand). Owed: Build-door live click (Kepler tile: no electrolysis row);
  ai_skill R5 (rivals build 0 processors on the legacy harness world after D5 — check rival
  processor builds in play on the real seeds); holdless non-player roster corp on legacy worlds
  (decide: accept as NR decision). -> NR-988 filed (accepted, reversible).
  **INTEGRATED 16-SEED GATE (2aeb48ef, pooled by hand from s50_int_a/b):** G1 **87.5 PASS**
  (2391/2732), G1t50 68.9 (3021/4384; deferred), G1b **11.5 FAIL** (314/2732: 170 starved now, 144
  decommissioned after starving), G2 **91.0 PASS**, G3 **97.3 PASS** (2341/2407); play income 48,805
  per seed-tick. Running at handoff ~unchanged vs wave-2b start (2393 -> 2391); built fell 3351 ->
  2732. Rivals DO build processors in play on real seeds (built 2732 -> 4384 by t50), so ai_skill
  R5's zero is the legacy harness world only. Next: Ben on G1b; Build-door live click owed.
  **BEN (2026-10-09, sprint 50 form):** diagnose G1b now, extend the sprint; START WAVE 3 NOW.
  Lanes: G1b diagnosis (probe, baseline = its merged tip); BL-1195 convoy lane follows legs
  (build); BL-1119 R7 cold review of 8b921ee6. BL-1119 R4 (quiet Release gen_step_costs 16 seeds)
  waits for a quiet machine; R6 haulage reading after.
  **BL-1119 R7 DONE (review of 8b921ee6, an ancestor):** direction logic correct and deterministic,
  BUT reused floods price routes on a STALE road field (caches never cleared mid-pass), against
  LOGISTICS § 4 "priced on the field as it stands"; round 4's 10x history speed-up IS that reuse
  (history corridors into different hubs no longer reuse each other's roads -> parallel roads;
  spurs read town fields from before the backbone; border probes compare mixed vintages). Also:
  R2's result_metric is stale (row demoted to control; assertion moved to R2s-b, BL-1159) — fix the
  text after running the harness; spur pass = one whole-body flood per target (~569 on seed 0,
  ~200 MB peak) — BL-1077's bounded per-village search fixes cost and staleness for spurs;
  detour test mixes link directions (small); stamp_history_roads early returns leave stats unset.
  **Form to Ben:** measure parallel roads first / rule stale ok / reshape to price on today's field.
  **BEN: MEASURE FIRST.** Lane running: probe switch clears caches after every stamp (FRESH) vs
  shipped (STALE), 16 seeds — road tiles by pass, parallel-road count, routes changed, border
  winners changed, loaded pass time, road_generation_harness R2s-b.
  **G1b DIAGNOSED, probe merged eac0c3e4** (baseline 6ecdd441): 308/314 have the input in reach —
  they starve on QUANTITY. Classes: (c) outbid/rationed 174 (households 75, 55 of them strategic
  switches onto consumer_goods; background 63; pro-rata 36 healthy); (e) haul elsewhere never
  arrives 48 (25 surplus not shipped — one-destination-per-pass / passive LP cap; 23 consumed at
  source); (g) 52 (25 held in another pool — 18 cargo landing after the draw; 27 short supply);
  (d) ceiling-silenced with stock 32 (price flips across the ceiling); (a) no producer 6 (switch
  passed a STOCK test on the opening shelf); (f) 2. 144 decommissioned: reflex 69, strategic idle 75.
  Fix families: switch uses the SUPPLY test (AI_OPPONENT, grant check); input_reach counts household
  + background draws (AI scorer input — grant); background draws after processors (MARKETS,
  re-rules "the cost, accepted"); ceiling hysteresis (MARKETS/FINANCE); dispatcher holds cargo it
  would send + cargo lists after the draw (SUPPLY). Counterfactuals: pull off 9.6%; ceiling off
  14.9% (worse); opening-stock release no change. Others untested. **Form to Ben.**
  **BEN RULED (5fde6f22):** every recipe switch (chase + rescue) judged on supply; NEW GRANT spare
  supply net of household + background draws (§ 11; generation sized rule too); background pull
  leaves one tick of processor want (MARKETS re-rule). NOT chosen: ceiling damping, haul fixes.
  Lanes: R1+R2 (switch + spare) and R3 (background order), each measured alone.
  **BL-1195 DONE** (4963c356; cold review running): lane = priced legs; convoy gains origin_tile/
  port_a/port_b (SAVE v42 — ledger row says 43, lane's edit refused: Ben to see); gate unchanged;
  pre-existing fails convoy_command R4.2/4.4/4.5, supply_advance 4, LP-trim 1 (same on base).
  Live look at a sea-route convoy owed. Save stack now: v39 D5, v40 D3, v41 PROPELLANT, v42 BL-1195;
  the G1b lanes may claim more — reconcile at merge.
  BL-1195 review: lane correct, merge clean; BUT interdiction samples one head tile per tick (T=1-3
  for most hauls, so inland-leg units rarely intercept) and the head is spread evenly by tiles
  across legs of ~5x different speed. Fix round sent (swept interdiction, time-weighted head,
  real-tick rows, origin-tile row, body-pool lanes, doc honesty, P8 convoy 1).
  **ROADS MEASURED** (4578340a on worktree-agent-a806ebb03681d766e, probe + verify-only switch
  `g_road_probe_fresh_floods`; switch off == tip digests; NOT merged pending Ben): STALE vs FRESH
  pooled 16 seeds — road tiles 79,154 vs 73,503; long parallels d1K8 235 vs 103 (hist-hist 176 vs
  42); history cost gap 1.87% vs 0.96%; time gen+hist 184+88 s vs 416+419 s (loaded); seed 0 gen
  15->77 s, hist 7->62 s; split nations 47 vs 54; R2s-b passes both (229/229 Highway, 14/16).
  Half of history corridors change. **Form to Ben:** keep stale (doc ruling) / fresh everywhere /
  design a cheaper targeted refresh (this sprint or later).
  **BEN RULED (2c898ebd):** keep stale reuse (LOGISTICS § 4 written); parallels fixed THIS SPRINT
  as BL-1252 (no parallel roads): d1K8 <= 103 pooled, harness green, time within ~1.25x of stale.
  Probe merged aa007264. BL-1252 lane running (measure candidates: targeted refresh / post-pass
  merge / bounded spur search).
  **BL-1195 FIX ROUND DONE** (e14a76a6; re-review running): swept interdiction
  (`progress_before`, transient), leg-time head clock (`convoy_route::at`, renderer uses it),
  real-tick rows R11-R17, body-pool lanes, SUPPLY.md says only origin+ports fixed at dispatch.
  sea_port_gate 89/0; 5-seed gate identical (no hostility in gate worlds). Live look owed.
  **G1b R3 DONE** (59e102dd; cold review running): `processor_want` per market (transient) leaves
  one tick of posted processor want before the background draws. 16 seeds alone: G1 87.5 -> 88.4,
  G1b 11.5 -> 10.6 (c-background 63 -> 38), t50 68.9 -> 72.0, G2 91.0 -> 92.7. Silenced want not
  protected (57 plants) — reviewer checking against FINANCE 2026-10-03.
  R3 review clean; **BEN RULED (35eb5707): silenced processor want is protected too** (processor
  part only). R3 round 2 sent (+ real-clear row, stale comments).
  BL-1195 re-review clean (low items); original agent unresumable (worktree unverifiable) — fresh
  agent does the tidy round on top of worktree-agent-a2c0cfcb69b515f0f (river-weighted clock,
  reset on every return, per-frame lane cache, delete convoy_head_index, doc, R17 real tick).
  **G1b R1+R2 DONE** (30a929c8 R1, e0bd50d9 R2 on worktree-agent-a123195de030662e7; base 5fde6f22;
  NOT reviewed yet): before 87.5/68.9/11.5, run/built h 2391/2732, income 48,805 | R1 89.8/78.0/9.2,
  2458/2737, 48,238 | R1+R2 96.4/86.0/**2.1**, **1736/1801**, **34,572 (-29%)**, firms 2407->2048.
  R2's generation form charges households' whole basket at base (play fills 25-94%) -> ~34% fewer
  processors. R2 makes background_fill SAVED (claims v45). corp_ai_harness R9(c) flipped (asserted
  the overturned behaviour). charter_refusal_probe fixture households moved. **Form to Ben:**
  measure R1+R3 and R2-at-expected-fill / accept / R2 in play only.
  **BEN: ACCEPT R1+R2 AS BUILT (8dcecb83, recorded in CORPORATION_GENERATION Pass 3).** Cold review
  running. Then merge R1+R2, then R3 (round 2), then the integrated gate.
  **R3 ROUND 2 DONE** (ea624f40): silenced processor want protected; population_mvp T1-T5 drive the
  real clear. Alone: G1b 10.6 -> 10.4, c-background 38 -> 2 but d-ceiling 36 -> 65; t50 72.0 ->
  70.1; G2 92.7 -> 90.9. No save field. Ready to merge after R1+R2.
  **R1+R2 REVIEW:** core logic clean; gaps — no play-form / shared-charge / rescue rows,
  background_fill not hashed, run_contend fixture half-tests R2, generation charges razed centres
  and unpriced goods (fixed as a defect: phantom buyers, not price). Fix round sent. Save stack to
  renumber at merge: v42 BL-1195 (ledger says 43), v45 R2 -> sequential.
  **BL-1195 MERGED 3694d297** (tidy round 9ba2bb4a via a fresh agent): save v42, ledger corrected
  to 42; app BUILD_OK; sea_port_gate 95/0; save_roundtrip OK. Live look at a sea-route convoy
  still owed. Note: a body-pool convoy records source_market null (not the body); lane falls back
  to origin_tile. R2 must renumber to v43 at its merge.
  **G1b MERGED:** 30224883 R1+R2 (fix round bcc4cfed: play-form/shared-charge/rescue rows
  mutation-checked, background_fill hashed, run_contend fixed, generation skips razed/unpriced —
  numbers unchanged; save renumbered 45 -> 43), 39907baf R3 (components.hpp comment conflict
  resolved). Building + integrated 16-seed gate next. Save stack now v39-v43 sequential.
  **INTEGRATED 16-SEED GATE, ALL PASS (39907baf; s50_int2_a/b pooled):** G1 96.6 (1739/1801), G1b
  1.9 (35), G1t50 86.7 (2283/2633), G2 95.8, G3 96.3 (1973/2048); play income 34,922/seed-tick.
  App BUILD_OK; save_roundtrip, population_mvp (45), build_only_what_runs pass. Sprint row
  status_line updated. LEFT: BL-1252 (running); BL-1119 R4 quiet timing + R6 haulage; live clicks
  (Build door no electrolysis on Kepler; sea-route convoy legs); close — doc check (REFINED § Close,
  ~35 rulings now), BL-1165, the one re-bless (Ben's go), retro, version cut, integrate onto main.
  **BL-1252 BUILT** (2f1ed0a0, worktree-agent-abf66128b6886e5cc): stamp_edge SNAPS a 2+-tile run
  beside an existing road onto that road before stamping (pricing/stale reuse unchanged — a NEW
  mechanism, novelty to file). Parallels d1K8 235 -> 47, time 1.04x, road tiles 79,154 -> 75,696,
  harness green. BUT 5-seed gate (on a pre-G1b base): G2 89.5 -> 78.7, t50 77.6 -> 70.0 (seed 43
  built by t50 265 -> 165). Re-measure sent: 16 seeds on the current tip, snap off / thr 2 / thr
  3, and why income falls. Then a form to Ben.
  **BL-1252 RE-MEASURED on the tip** (head 7c8ed047, tooling only): snap at 2 costs nothing —
  G1 96.6, t50 86.7 -> 88.7, G1b 1.8, G2 96.1, income 34,958; parallels 47; time 0.99x. Threshold 3
  fails both (153 parallels, t50 84.3). Income swings are the start cascade (market folds read the
  road field: seed 43 15 -> 16 markets), not dearer hauls (+0.6%, travel ticks unchanged). NR-989
  novelty filed. Cold review running; merge after.
  BL-1252 review: no correctness bug; gaps — doc overclaims "no second road" (47 left), snap uses
  tile counts not the cost model and is unbounded (3x snake possible), double-visit, g_last_laid
  not thread_local, stale comment, kSnapRun unused. Tidy round sent (cost-bounded snap).
  **Live clicks attempted 2026-10-10** in the sprint-50 build (computer-use: grant the exe by FULL
  PATH — "ProjectIo"/"projectio.exe" resolve to stale worktrees and mask the window). Build door
  folds recipes into one row per group, so the air filter isn't visible there; the method grid
  needs a finished building; no sea convoy on screen. **Ben: build the scripted visual check
  instead** — lane running (two scripts/verify checks: air gate on door data + method grid +
  set_recipe refusal; sea-route convoy lane + leg-time head; each shown able to fail).
  **BL-1252 MERGED** (tidy 10e17321): snap bounded at 1.75x (1.25 gave 190 pairs); parallels 59;
  road tiles 75,952; 1.06x; gate G1 96.7 / G1b 1.7 / t50 87.3 / G2 95.6 / income 34,896. App
  BUILD_OK, road_generation_harness 0 failures. LEFT: visual-check lane; BL-1119 R4 quiet timing
  (now on the snapped roads) + R6 haulage; close.
  **VISUAL CHECKS MERGED 5b334c76:** air_gate 30/30, sea_lane 137/137 (both shown able to fail);
  A3 fixed by drawing the refusal on the one-method grid (not by loosening). Refusal text clips at
  panel width (cosmetic); sea capture beam faint at zoom 4 (data asserts carry it). NR-990 novelty
  (verify fixtures fabricate state). **BL-1119 R4+R6 lane running** (quiet machine — launch nothing
  heavy until it reports). Then: CLOSE.
  **BL-1119 R4/R6 MERGED 0217f351** (both PASS; R4 marked complete despite ~30% residual load —
  two runs agree). Filed BL-1261 (gen_step_costs road replay diverges; renumbered from 1259 after an id collision with sprint 51) and BL-1262 (round 6 tail
  > 35 s on seeds 11/31/13 incl. the span; search grew 2.6 -> 10.6 s on seed 0).
  **CLOSE STARTED:** doc-check lane running (all ~40 rulings: doc/code/siblings/owner; fixes doc-
  only gaps; writes a repeatable tools/session script, skill needs Ben's permission); BL-1165 lane
  running (bisect sprint 48 re-bless movements; re-point haulage_measure pricing). Then: the one
  re-bless (Ben's go), retro, version cut, integrate onto main.
  The hand-made `s50-round4-demand` worktree can be removed. Then merge the probe and
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
