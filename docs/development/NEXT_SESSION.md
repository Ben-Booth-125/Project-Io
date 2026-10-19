# Next session — sprint 55 continues, with sprint 56 merged in

Handoff written **2026-10-19** by the session that continued sprint 55 and integrated sprint
56. Main carries both sprints and is **not pushed** — pushing is Ben's.

Read `docs/development/sprints.json` (sprint 55) for the goal and
`node tools/session/backlog_query.js --grep <id>` for any item named here. Do not re-read the
corpus: each item names its own authority doc.

---

## 1. The merge, and the two things it taught

**Sprint 56 is merged** (`claude/sprint-56-456d2b`, 71 commits), built and verified on the
merge rather than on either side: `save_roundtrip` OK, `world_determinism` ALL PASS,
`logistic_points_harness` 34/0, `market_attraction_harness` 17/0, and sprint 56's own
`no_empty_chest_harness` ALL PASS.

- **The save format is v54 on top of v53.** v53 is BL-1340's tile record (a road is an edge),
  v54 adds BL-1337's exact-net trailing section, and v53 is a strict prefix of v54 — stacked
  in the order sprint 56's own integrator note asked for. **One integration bug was fixed
  with it:** the net section was written *unconditionally*, so a stream written at an older
  version — which `write_world_snapshot_pre_road_edges` does, to give the road migration a
  real v52 stream — carried bytes no reader consumes. It is gated on the version written now,
  and the two harnesses that slice trailing sections slice off the pre-edge stream with no net
  tail.
- **FIVE ids collided.** Both sprints minted BL-1372, BL-1374, BL-1377, BL-1382 and BL-1386
  for different work, because `next_id.js` cannot see an unmerged branch's store. Sprint 56's
  rows keep those ids (more citations); sprint 55's moved to **BL-1390** (road halves draw
  their own tier), **BL-1391** (rail is a crossing), **BL-1392** (ledger tabs fixture),
  **BL-1393** (the verify world is not roaded) and **BL-1394** (a road reads as connected
  every way), with their code citations moved too. Commits that landed them still say the old
  ids; that is history and stays. **Before minting beside a worktree, check the other
  branch's store.**

**Still owed from sprint 56's own handoff** (`HANDOFF_SPRINT_56.md`): **BL-1338's digest
re-pin at integration, which is Ben's** — this session did not re-pin anything.

## 2. Delivered since the previous handoff

| Item | What landed |
|---|---|
| BL-1381 | The verify summary names golden diffs and assertion failures apart. |
| BL-1352 | **One Logistic Point system.** A march draws its own nation's pool and claims before production; the legacy per-anchor pool, its plumbing and its authored rate are gone. |
| BL-1385 | The muted road press names the rule that actually refused: of 5017 roaded tiles only 53 admit a crossing, and **4647 of the refusals were another nation's ground**, not the tier the message blamed. `road_offer_census.cpp` prints it per seed. |
| BL-1390 | Each half of a tile's road draws at its own tier — 845 mixed-tier tiles on the shipped start, 603 baked runs now tapering at the apex. |
| BL-1387 | `roads_painted.lua` no longer kills the app: SDL 3.2.0's software blitter mis-samples a quad measured at **50,025 × 20,254 px against a 1,941 × 1,281 view**, so the ground quad is clipped to the view with its UVs moved by the same fraction. No pixel changes. The crash report now names module, function, file and line. |
| BL-1346 | **Market attraction.** Catchment is logistics cost over the centre's pull, not grid distance; the off-road leg gives every tile a market; same-nation tiles pull 1.2× harder, moving **279 of 31581 tiles (0.88%)**. |
| BL-1353 | **Partly — see § 3.** |

**The world moved and Ben accepted it:** `market_viability` FULL pooled G1 92.0 → 90.1, G2
182.1 → 185.0, all five gates passing both sides; `haulage_measure --far-trade` convoys
771 → 1112, volume 3543.5 → 6282.9 (+44% / +77%). Ben: *"That jump looks right."*

## 3. Start here

1. **BL-1353's two owed halves.**
   - **The brighter-white roads do not show.** The three tier colours in the vector pass the
     item named are changed, and a real bug was found and fixed on the way — the static stroke
     bucket's hash did not include the overlay, so strokes built under one lens were reused
     under another. Even so the roads read **sand** in every captured rung under the lens, and
     a 2.4× width multiplier changed nothing visible, which is the evidence that these strokes
     are not what reaches the screen there. **Find the pass that actually paints the road under
     this lens before touching another colour.**
   - **The hover card** needs a `structure_kind` meaning "the ground"; adding one changes
     SELECTION.md's click model, so it is a design call with a live click attached.
   - A capture showing real numbers needs a world where **the player's own nation has a running
     source**: the `--verify` world has two sources, neither the player's, so every number
     there is an honest zero (measured, not assumed).
2. **BL-1354** — the tile surface's logistics section, honest now that BL-1352 has landed.
3. **BL-1360** (road tier extends reach), then **BL-1356 / BL-1361** (routed requests, a
   shipment draws points). **Budget a cold review for each.** Three items this sprint passed
   their own harnesses and then failed an independent read on substance: BL-1346's review
   caught a catchment field that was *not monotone in roads* and a cache that rebuilt once per
   idling port.
4. **BL-1367** is buildable: LOGISTICS.md now owns the capital-peak rule, its three rules are
   checks, and the BL-1351 census is its gate.
5. **Ben's owed live click: BL-1335, the Taxes control.**

## 4. Red rows found, none of them caused by this work

- **BL-1388** — a press on a single-building tile selects the TILE, not the building. Nine rows
  of `ground_bake.lua`, proven on a binary built before the change. Check it against
  SELECTION.md's click model, not against what the old binary did.
- **BL-1383** — `lp_pool_harness`'s L7 "v49 stream" row has been red since roads became edges:
  it fabricates an old stream by relabelling current bytes, and the tile record is
  version-dependent now. Fix by writing a real old stream, never by relaxing the row.
- **BL-1384** — `lp_anchor_field_check`'s F1 lost its subject (nothing builds the field now the
  march does not); it reports a loud SKIP naming the item, and BL-1361 restores it.
- **The CMake `all` target does not link**: `battle_engagement_harness` references
  `active_mercenary_contract_for`, defined nowhere. Unfiled.

## 5. Traps this session paid for

- **A harness exe is not rebuilt by building the app.** `bash tools/build_rel.sh` builds the
  game; `build_gen/verify/<name>.exe` stays as it was, and an after-measurement against a stale
  harness reads **exactly** like the baseline. BL-1346's first haulage reading came back
  digit-identical and "nothing moved" was wrong by 44%.
- **Get the stack before the fix.** BL-1387 cost two wrong fixes guessed from a bare address;
  one build with symbols plus a backtrace in the crash handler named the faulting function in a
  single run.
- **Measure the number you are about to write down.** That crash's first write said "hundreds of
  thousands of pixels"; logged, it was 50,025.
- **A union of two stores silently displaces rows.** Merging sprint 56's backlog took five of
  sprint 55's rows out of the file entirely before they were noticed and restored. After any
  store union, diff the id set against **both** parents, not just the result.
- **`next_id.js` hangs in this checkout** (it scans `.claude/worktrees`, which holds whole
  repos). Mint from
  `grep -ohE '"BL-[0-9]+"' backlog.json archive/*.json | sort -n | tail -1`.
- **`cmd //c build_app.bat` fails**; `cmd //c "cd /d <repo> && call .\build_app.bat"` works.
- **A verify run through a pipe prints nothing until it exits**, and a Debug `--verify` of a
  capture script takes 20–30 minutes.
- **The heavy-run lock is shared with other sessions' worktrees.** Judge it by age and by
  whether shard output is growing, never by whether its pid resolves — Git Bash records an MSYS
  pid that Windows will not find.

## 6. State

- Main carries sprints 55 and 56; **nothing is pushed**, no tag cut.
- World save at **v54** (v53 tile record + v54 exact nets).
- Backlog: 77 open items, `backlog_lint` 0 fails (17 hot rows that duplicated cold archive
  copies were re-evicted as part of the merge).
- Gates green on the merge: build, `save_roundtrip`, `world_determinism`,
  `logistic_points_harness` 34/0, `market_attraction_harness` 17/0, `no_empty_chest_harness`,
  `roads` 0, `roads_painted` 0 (it used to crash the app), `throughput_lens` 0.
- Open review calls: **NR-1049** (the march's weight — a 50-unit army asks ~60 points of a pool
  measured in the hundreds, so guns and butter barely compete yet) and **NR-1050** (the off-road
  leg's price, and what crossing water should cost).
