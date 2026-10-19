# Next session — sprint 55 continues

Handoff written 2026-10-18 by the session that opened sprint 55, **brought current 2026-10-19**
by the session that continued it. The order below is the one the dependencies force, not a
preference.

**What has moved since the handoff was written.** Ben's four calls are TAKEN and written into
their owning docs (§ 1 records what he ruled). BL-1381 (the verify summary) and BL-1352 (one
Logistic Point system) are delivered; BL-1385 is new and delivered. The spine now starts at
BL-1346.

Read `docs/development/sprints.json` (sprint 55) for the goal, and
`node tools/session/backlog_query.js --grep <id>` for any item named here. Do not re-read the
whole corpus: each item names its own authority doc.

---

## 1. Ben's calls — ANSWERED 2026-10-18, and where each answer lives

Put on one form and ruled the same day. Nothing here blocks work any more; it is kept so the
next session knows what was settled rather than re-asking.

| Call | Ben's ruling | Written in |
|---|---|---|
| **Is the centralised-capital logistics picture the intent, and which doc owns it?** | **Yes, and LOGISTICS.md owns it.** Generation seeds hierarchically: the strongest source at the capital, lesser ones at the other market centres, the periphery left for a player or rival to cover. | `docs/economy/LOGISTICS.md` § Where generation puts the sources — with three rules that are CHECKS: every capital holds a source, no tile outruns its own capital at generation, and uncovered GROUND is intended while uncovered CONSUMERS are not. **BL-1367 is now buildable**, and the BL-1351 census is its gate. |
| **Space asset calibration** (NR-1047) | **Keep as chosen; the 2040 run judges it.** No dial named ahead of the evidence. | NR-1047 resolved. |
| **Does a reroll leaking into a committed System draw matter?** (BL-1345) | **The leak is acceptable, so the CLAIM comes out** — the rounds are one pre-commit search, and the attempt index must stay in the stream for a reroll to be a reroll. | `planetology.cpp`'s comment and `PLANETOLOGY.md`'s R8 row. Delivered. |
| **The `Track` name collision** (BL-1369) | **The ROAD rung moves; rail keeps Track.** TRAIL chosen in band, so the ladder reads Trail → Road → Highway. | BL-1369 carries the work — **the rename is still owed** (GLOSSARY, LOGISTICS § 4/4a, RENDERING, the history-lapse ladder, ACTIONS.json, app.cpp's kName). |

**Live clicks.** BL-1379 (the muted road press) is CLOSED on Ben's click. **BL-1335 (the Taxes
control) is still owed.**

**What his clicks then found**, which is the better reason to hand him a build early:

- **BL-1382** — he read a new road as connecting every way. Measured: the renderer draws only
  laid edges (5780 crossings, zero drawn across a roadless boundary, before and after a build),
  so what radiates from a town is the generated lattice. Not a defect.
- **BL-1385** — DELIVERED. "Every tile with a road I try to press has the build button greyed
  out." The rule was right and the sentence was lying: of 5017 roaded tiles only 53 admit a
  crossing, and **4647 of the refusals are another nation's ground**, not the tier the message
  blamed. The press now names the real rule, `tools/verify/road_offer_census.cpp` prints the
  number per seed, and Ben ruled the home-nation-only reach stands.
- **BL-1386** — a crossing whose halves differ in tier rendered at one tier, because both
  renderers weight a whole TILE by `max_road_tier`. 845 tiles of the shipped start carry two
  tiers. Built, smoke-tested, awaiting Ben's go-ahead to commit.
- **NR-1049** — four in-band readings on the march's draw. The one worth his eye is the WEIGHT:
  a 50-unit army asks ~60 points of a pool measured in the hundreds, so guns and butter barely
  compete yet.

## 2. The dependency spine — do these in this order

Everything in the market and logistics arc hangs off work that is settled in the docs and not yet
in the code. The road placement rules landed on 2026-10-18; these are what they unblocked.

1. **BL-1346 market attraction** — catchment becomes logistics cost ÷ the centre's pull, not grid
   distance. Off-road tiles priced through the per-step off-road leg so **every** tile belongs to a
   market. Same-nation tiles pull 1.2× harder. Recompute on change, never on a clock. *World-mover:
   measure `market_viability` and `haulage_measure --far-trade` before and after.*
2. ~~**BL-1352 LP one system**~~ — **DELIVERED 2026-10-18 at 91aa0baa.** The march draws its
   own nation's pool and claims before production; the legacy per-anchor pool, its plumbing and
   its authored rate are retired; the Throughput lens reads the tick's own resolve. A cold review
   caught three missed call sites and two wrong rules (a unit stranded for ever on unowned
   ground; an invader drawing its victim's pool) — **budget a fix round on everything this
   size**, which BL-1346, BL-1356 and BL-1361 all are. The original brief said: **Two unrelated Logistic
   Point systems are live at once**: the real one (nation pools from hubs/stations/ports, consumed by
   production) and a retired anchor model (a flat 20/tick at *every population centre*) that the
   throughput lens and marching units still read. That is why cities appear to supply logistics when
   the doc says they make none. Retire the anchor pools; move the march onto the nation pool.
3. **BL-1353 lens rework** and **BL-1354 tile logistics section** — only honest once BL-1352 lands.
   The lens drops its taglines, shows per-tile numbers and filters to the player's own nation;
   recolouring roads brighter white is **cheap** (change three colour constants in the vector pass
   that already redraws them under this lens) and must **not** be attempted in the bake, which would
   re-bake every visible chunk on every lens toggle.
4. **BL-1360 road tier extends reach** — Track 6 / Road 8 / Highway 10 at full strength, fading to
   zero at double; rail carries 20 and delivers only at the tile it connects. The road half is a
   recalibration: per-tier step costs already yield 6 / 8.6 / 12.
5. **BL-1356 routed requests** and **BL-1361 trade draws points** — the model change. A tile asks for
   what its buildings need, filled over one path; a source must reach the tile **and** be connected
   back toward the centre; split across sources in distance order; distance **orders the queue and
   never raises the bill** (a distance draw was measured at a 73% collapse in convoy traffic, NR-620);
   the march claims first. A shipment draws by cargo quantity again.
6. **BL-1367** once Ben rules, then **BL-1349 / BL-1350 / BL-1358 / BL-1359** (market-centre
   buildings, the provisional pull techs, building a centre, hubs stacked at centres).

Running in parallel is safe for: **BL-1335's tail BL-1336** (AI tax behaviour — Ben parked it until
after his live run), **BL-1326** (AI grows to digitisation), the **space arc** (BL-1328..BL-1332),
and **BL-1333** (spectator growth charts).

---

## 3. Defects worth clearing early — they cost the last session real time

- ~~**BL-1381**~~ — **DELIVERED at 867822f7**: the summary names golden diffs and assertion
  failures apart. The original note: `verify_api.cpp:7088` prints
  `"%d capture(s) failed golden diff"` from a counter that totals assertion failures *and* image
  diffs. Thirteen failed assertions reported as thirteen golden diffs, and **two readers in one day
  took it as a re-bless question that did not exist**. One line, and it removes a class of wrong
  triage. Do this first; it is an hour saved for everyone after.
- **BL-1374 — `construction_ledger_tabs` is red**, and its filed cause was wrong: the fixture is
  refused `needs_road`, not for want of funds. **Ben ruled: check the ordering first** — confirm
  `needs_road` is the *last* refusal `can_place_in_world` returns, then road only the tile actually
  refused, never the whole 24-tile candidate ring.
- **BL-1377 — the `--verify` world can still hold a roadless building**, because its roster is
  stamped by a third path *after* generation lays site roads. The one world the checks run in is the
  one world where the rule does not hold.
- **BL-1369** — the road ladder's three tier rows are the last tall rows on a surface that is
  otherwise a one-line grid.

---

## 4. Things this session learned the hard way

Each of these cost an hour or a wrong conclusion. They are in `DEVELOPMENT_PRACTICES.md` now, but
read them before measuring anything.

- **A baseline is a commit, not a probe-off.** Switching your own change off is a *simulation* of
  the baseline and drifts from it. Two lanes disagreed by 5.5 points of G2 this way.
- **`market_viability` exits 0 on a failed gate.** "All shards exit 0" is not "no failing row".
- **A harness argument may be a count, not a list.** `haulage_measure --seeds N` is *how many*
  seeds; `--first-seed S` picks where they start. Two runs quoting `--seeds` can measure different
  worlds entirely — that explained a "regression" that was never there.
- **Ben's running game holds `build_rel/ProjectIo.exe`.** The link then fails with `LNK1104` and
  your checks silently run the **old** exe. Confirm `BUILD_REL_EXIT 0` before believing any result.
  This cost this session a false alarm on two assertions.
- **Worktree lanes arrive stale.** Six of nine lanes started behind main, one by 33 commits.
  `git log -1` and `git merge main` before writing a line, and say so in the report.
- **A worktree agent cannot reach `cmd.exe`**, so `build_app.bat` is unusable there. Use
  **`bash tools/build_rel.sh`** (committed 2026-10-18 for exactly this).
- **Roading a fixture is not neutral.** It switches on reach discounts and, once, the power-wire
  path. Nine harnesses and one Lua script needed it; re-measure rather than assume.

---

## 5. State (2026-10-19)

- **Main carries BL-1381 (867822f7), Ben's four rulings (f96ad172), BL-1352 (91aa0baa), BL-1385
  (cd9ca67b, e2808b23) and two audit counters (7daa6f86, 04065743).** Pushing is Ben's.
- **BL-1386 is built and uncommitted**, awaiting his go-ahead; a copy and a patch sit in the
  working session's scratchpad so a concurrent commit in this shared checkout cannot lose it.
- Gates: `logistic_points_harness` 34/0 (rewritten onto the nation pool), `world_determinism`
  ALL PASS, `save_roundtrip` OK, `market_viability` FULL byte-identical to the f96ad172 baseline
  pooled AND per seed, `haulage_measure` 944/0, `roads` and `throughput_lens` green.
- **Three red rows found and filed, none of them this work's:** BL-1383 (lp_pool_harness's v49
  row, red since roads became edges), BL-1387 (`roads_painted.lua` CRASHES the app at its last
  framing, proven on main), BL-1388 (a press on a single-building tile selects the tile, proven
  on the pre-change binary). The CMake `all` target also does not link on main —
  `battle_engagement_harness` references `active_mercenary_contract_for`, defined nowhere.
- **World save is at v53**; nothing this session moved it.
- **`next_id.js` hangs in this checkout** (it scans `.claude/worktrees`, which holds whole
  repos). Mint from `grep -ohE '"BL-[0-9]+"' backlog.json archive/*.json | sort -n | tail -1`.
- Sprint-55 items open: the spine from BL-1346, plus BL-1369's rename, BL-1336, BL-1326, the
  space arc and BL-1333.
