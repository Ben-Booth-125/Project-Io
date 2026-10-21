# Next session — sprint 55 continues: finish the logistics arc, then cut v0.2.0

Handoff written **2026-10-22** by the session that built sprint 55's logistics spine
(BL-1353 → BL-1396). Main is **10 commits ahead of origin and not pushed** — pushing is Ben's.

Read `docs/development/sprints.json` (sprint 55) for the goal and
`node tools/session/backlog_query.js --grep <id> --full` for any item named here. Each item names its
own authority doc; do not re-read the corpus.

---

## 1. Where it stands

**v0.2.0 is "the economy functions", logistics included** (Ben, 2026-10-22, ROADMAP.md § The
industrial arc's milestones). Ben judged it reached, but **do not cut yet**: finish the two logistics
items below first (his call, 2026-10-22). v0.3.0 is a working military system; its done-definition
is owed.

**Delivered this session** (all on local main, each with a measured before/after on its row):

| Item | What landed |
|---|---|
| BL-1353 | Throughput lens: roads draw white (the per-edge colour overrode the lens); a hover-only GROUND card (`structure_kind::ground`, Ben's option A) naming coverage, network, cost and full/fade. |
| BL-1354 | Tile card Logistics section: Makes / Uses of need; the hover names the pool, coverage here, and each source and building. Uses counts the player's own buildings (NR-1055). |
| BL-1360 | Road tiers reach 6 / 8 / 10 tiles, fading to zero at double; rail 20 steps, point to point. |
| BL-1356 | **Routed requests.** One nation pool fed only by sources CONNECTED to the nation's CAPITAL by a chain (either direction); the fade caps a source's share and the next tops it up; a short pool serves requests inside the falloff first, nearest first. Generation lays hubs outward along the chain and bridges water with port pairs, island-hopping. |
| BL-1395 | Rivals price and lay the whole hub chain a build needs (AI grant, AI_OPPONENT.md § 11, 2026-10-21), through the build door's own check; generation untouched. |
| BL-1396 | A port reaches 50 water tiles at full strength, gone at 100, over water only (lakes too) — it lands on the shore and goes no further inland. Six-sided sea walk, cached per port. |

**Latest gates** (BL-1396 landed): market_viability FULL pooled G1 95.1 · G1t50 90.3 · G1b 2.5 ·
G2 176.9 · G3 95.5 — all pass; per seed G1t50 under 85 on seeds 12 (83.8) and 9 (81.0).
`lp_coverage_census` seeds 0/28/46: every consumer covered. world_determinism ALL PASS,
save_roundtrip OK, logistic_points_harness 93/0.

## 2. Start here, in order

1. **Lake size cap 150 → 75** (Ben, 2026-10-22: "we can retune later again if needed"). Not yet
   built. It is authored twice and must move together: `scripts/world_gen.lua` (`lake_size_cap`)
   and `lake_size_cap_default` (`src/world/world_gen_config.hpp`). TILES.md § Water kinds states the
   cap with its measured reason (the 89–143 band, "150 keeps 301 lakes and makes 59 seas") — rewrite
   it as Ben's ruling, with the new counts from `tools/verify/lake_census.cpp` (`node
   tools/verify/build_harness.js lake_census`). A world mover: more enclosed water becomes sea (ports
   allowed, wells lose fresh water). File a backlog row (mint the id from the stores — see § 5), and
   measure market_viability FULL and the census before and after.
2. **BL-1361 — a shipment draws points**, by cargo quantity, at the anchor it leaves, AFTER
   production (Ben's order: march, production, trade; LOGISTICS.md § How a tile's points get to it).
   Revive the zeroed code (`market_lp_left`, `commit_trade_shipment`, `trade_lp_spare`, v46 save
   field) rather than rewrite. Never refused whole — send what the anchor admits. Under routing,
   decide (and log) what "the anchor it leaves" draws against now that the pool is the nation's and
   only connected sources feed it. Measure `haulage_measure --far-trade` (the convoy counts are the
   check that matters) and market_viability G1/G1b. Budget a cold review.
3. **BL-1367 — where generation puts sources.** LOGISTICS.md § Where generation puts the sources
   owns the capital-peak rule and its three checks; BL-1356 added the outward chain and the sea
   bridge to that section. Re-read the item against what generation now does before building — the
   premise may have moved.
4. Then **cut v0.2.0** (Ben's go first): sprint close, ruling-check skill, re-bless decision, tag.

## 3. Open calls for Ben (NEEDS_REVIEW.md)

- **NR-1060** — rival hub chains on the PLAYER's nation's ground: builder or ground reading?
  (Built conservatively: the old one-hub rule there.)
- **NR-1056** — rail's fade past 20 (built: 8, gone at 28).
- **NR-1055** — tile card Uses counts own buildings only (visibility).
- **NR-1049 / NR-1050** — from the previous session (march weight; the off-road leg's price).

## 4. Owed

- **Live clicks:** BL-1335 (the Taxes control); the BL-1356 card wording ("Not tied to the
  capital", "not tied to the capital, adds nothing", "Topped up from N sources") has not been seen live.
- **Red rows, none caused here:** lp_pool_harness L7 (BL-1383); lens_one_tier's no-lens
  building-press row, which predates the 2026-10-13 tile-press ruling (on BL-1388); BL-1384; the
  CMake `all` target does not link (`battle_engagement_harness`, unfiled).
- **Stale backlog rows:** BL-1352 (one LP system) landed in the session before last and still reads
  `designed`. Sweep sprint 55's rows for others before the cut.

## 5. Traps this session paid for

- **Never `taskkill` `cl.exe`.** Killing a compiler mid-build corrupts `build/` (blank `.obj.d`, a bad
  PCH, an obsolete `vc140.pdb`). The recipe that works is in memory (`io-interrupted-build-corrupts-
  build-dir`): clean the target's objs, `.obj.d`, `.pch`, `.pdb`, `compiler_depend.internal` all at
  once and restore the two-line `compiler_depend.make` stub. Deleting ONE pdb breaks the PCH.
- **Shell heredocs eat `\n` escapes.** A C++ `"\n"` written through `python - <<'EOF'` arrives as a
  real newline and breaks the build. Write edit scripts to a FILE (scratchpad) and run them; use
  `chr(92)` for a backslash; or use the Edit tool.
- **Mixed line endings.** These sources are CRLF in the working tree. Text inserted with bare `\n`
  makes a file `w/mixed`, and `\r\r\n` makes git call it binary (`w/-text`). Check with
  `git ls-files --eol` after any scripted edit.
- **A true before-baseline** for a world mover: reverse-apply just the world files from a saved
  patch, build the harness, copy the exe to `build_gen/verify/<name>_before.exe` (never `%TEMP%`),
  re-apply, and `cmp` the diff against the saved patch. `par_seeds.sh <name>_before` runs the copy;
  its merger keys gates on the harness name, so merge by copying the shards under the real name.
- **Hold keep-awake** for full-tier sweeps (`mcp__ccd_host__request_keep_awake`); one sweep slept
  12 hours mid-run.
- **Appending a backlog row:** items sit at two-space indent, keys at three. The linter counts raw
  `"id"` keys by indentation and reports a wrongly indented row as FUSED records.
- **NR ids:** mint across the hot file and `archive/needs-review-*.json`; the hot maximum lies after
  an archive pass.
