# Next session — sprint 55 continues

Handoff written 2026-10-18 by the session that opened sprint 55. Main is pushed at `fc1f66b3`
and every gate on it is green. **27 items of sprint 55 remain**, and the order below is the one
the dependencies force, not a preference.

Read `docs/development/sprints.json` (sprint 55) for the goal, and
`node tools/session/backlog_query.js --grep <id>` for any item named here. Do not re-read the
whole corpus: each item names its own authority doc.

---

## 1. Ben's calls, before the work they block

Four of these block items outright. Put them on one form rather than asking across a session.

| Call | Blocks | What is actually being asked |
|---|---|---|
| **Is the centralised-capital picture the intent, and which doc owns it?** | **BL-1367**, and the honesty of BL-1353's lens | Ben described wanting "a centralised market capital city, with a high number of logistics points, and logistics diminishing as we get further away, but extended by logistics buildings further away". The census (BL-1351) measured the opposite and **no authority doc owns where generation puts a logistics source**, so that picture has nothing to be pass/fail against. Only 10.3% of hubs reach a market centre; the capital is the peak of its own nation's field in 19% of cases and holds *zero* points in 28%; 78% of national ground is uncovered while every consumer is covered. Either the picture becomes a rule in `LOGISTICS.md` and generation seeds hierarchically, or the expectation goes. |
| **Space asset calibration** (NR-1047) | nothing, but it shapes BL-1328..BL-1332 | Launch costs 10k–60k, reach 0–45k plus 20k/AU, upkeep 500–3000/qtr, lost after four unpaid quarters. All chosen in-band. The 2040 run will show whether a nation can afford a moon landing at those numbers. |
| **Does a reroll leaking into a committed System draw matter?** (BL-1345) | nothing | A Life reroll that changes the viability attempt count moves the System round's draws too — measured: theta 1.519 → 1.462, plates 9 → 8 on one seed. A comment claims this cannot happen. Either the System draws become attempt-independent, or the claim comes out. |
| **The `Track` name collision** (BL-1369) | nothing | The road ladder's first rung and the rail row above it are both called "Track", now visible because both are compact one-line rows. Renaming either moves a term, not a label, so it is Ben's. |

**Two live clicks are owed** and neither is hard: the **Taxes control** (BL-1335, delivered) and
the **muted road press** (BL-1379, delivered). Close those two items on his word.

---

## 2. The dependency spine — do these in this order

Everything in the market and logistics arc hangs off work that is settled in the docs and not yet
in the code. The road placement rules landed on 2026-10-18; these are what they unblocked.

1. **BL-1346 market attraction** — catchment becomes logistics cost ÷ the centre's pull, not grid
   distance. Off-road tiles priced through the per-step off-road leg so **every** tile belongs to a
   market. Same-nation tiles pull 1.2× harder. Recompute on change, never on a clock. *World-mover:
   measure `market_viability` and `haulage_measure --far-trade` before and after.*
2. **BL-1352 LP one system** — the root of everything Ben saw in the lens. **Two unrelated Logistic
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

- **BL-1381 — the verify summary calls every failure a golden diff.** `verify_api.cpp:7088` prints
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

## 5. State

- Main pushed at `fc1f66b3`; working tree clean; every gate green
  (`road_generation_harness` OK, `world_determinism`, `save_roundtrip`, `road_edges` 38/0,
  `construction_progress` 911/0, `roads` 42/0, `tile_card` 71/0, `market_viability` FULL all five
  gates pass, `history_sim_harness` at its standing R3a2/R3a3 baseline).
- **World save is at v53.** v51 (space assets), v52 (taxes and Discontent) and v53 (roads as edges)
  all landed this sprint; the ledger is current.
- **Four verify-API exceptions** now stand in `.claude/rules/io-standing-rules.md`, all granted this
  sprint: `find_deposit_tile`, `road_admits`, `find_water_crossing`, and a staging call reporting its
  own write. A fifth is a new grant and Ben's to give.
- 27 sprint-55 items open, 37 open elsewhere.
