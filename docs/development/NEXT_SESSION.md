# Next session — finish wave 1 of sprint 39, then waves 2 and 3

Written 2026-09-11, mid-batch, when the session had to pause. Delivery — Full, Batch Delivery
(DELIVERY.md § Batch Delivery). Read `docs/development/sprints.json` sprint 39 for the goal, the
sequencing and the eight ranked causes of stability; `REFINED.md` is the worklist.

## State of main (0d5f4144)

Wave 1 items **merged into main, built (`build_app.bat` BUILD_OK) and captured clean**
(`history_lapse_press.lua` 0 failures, `history_lapse.lua` 0 failures):

| item | what landed | notes |
|---|---|---|
| BL-926 sweep sees the arc | per-polity table, gross deaths, pieces, supply histogram, culture census, 70-key JSON row | determinism unchanged |
| BL-927 aggression lean once | lean outside the season loop | digests unchanged; before/after: battles 3114→3114 median, seeds 0 and 3 moved slightly. **Record these beside NR-831 and prune it.** |
| BL-919 lineage palette | hue by root cradle, step per daughter; round 3 under `--verify` now replays the migration record (it used to replay the Empires sim silently) | `build_migration_timelapse` declared in hard_coded_world.hpp |
| BL-918 culture diversity | split interval 600→200 y; biome trigger on every class transition; ISOLATION pass in run_settlement (Culture round only); census on colonisation_harness C15 | **digests moved on all four arcs — re-bless owed**. **Pre-existing defect fixed on the way: `culture_kinship_years` returned −1 for every BCE coining year, so opposition treated every neighbour as a stranger; the kin discount is live for the first time — file a `novel-work` NR.** Cultures per world ~800→~380 (old depth-16 chains were tile noise); cultures holding ground slightly down (61→39 seed 0); the four constants are dials, C15 is the gauge — Ben may want more. |
| BL-916 lapse event layer | typed events on era_timelapse, `polity::parent`, save v13, ticker + markers + arc from events + Pop/Mt columns | 4-seed: 1,039 events, 73 realm ended, 5 broke away. Verify world arc: "37 polities, 12 destroyed" (was 0). Road promotions ringed but not narrated. Ticker 6 rows above the arc; at 1080p the arc's first line sits at the fold. |
| BL-915 terrain underlay | baked terrain base, rivers, relief, seats, frontiers both axes, greedy 20-slot neighbour colouring | **the wizard preview had no rivers; `generate_rivers` added to `generate_home_surface_preview` (world-side, same seed formula as the campaign).** Merge with BL-919/916 needed a link fix: `build_migration_timelapse` now has a roster-less overload that rebuilds the culture list from the settlement record. |

**BL-922 (reach has a gradient) is NOT merged.** Its agent was still probing five magnitudes
(uncapped supply cost 10 / 1000 / 2000 / 3000 / 5000) when the session paused. Its worktree
`.claude/worktrees/agent-a344894c3ea02f0f6` (branch `worktree-agent-a344894c3ea02f0f6`, base
6995b41e) holds **uncommitted** edits to era_minus_one.cpp, history_sim.cpp/.hpp,
history_sim_harness.cpp, history_sweep.cpp. First thing: look at that diff. If it is coherent,
finish it there (choose the smallest rescale at which "REFUSED reach gate" is neither 0 nor nearly
everything, print the supply histogram before/after, adjust only the fixtures whose premise was the
old currency, commit with the BL-922 message shape); if not, re-brief a `generation-dev` agent from
the item text. The ruling is binding: **capital is the basis for everything, traversal over HELD
ground only, rescaled so distance bites** (NR-838). It will conflict with BL-926/916's
history_sim.hpp/.cpp and history_sweep.cpp additions — resolve by keeping both.

## Wave-1 close-out still owed (do before wave 2)

1. Merge BL-922; `build_app.bat`; `node tools/verify/build_harness.js history_sweep`.
2. **One authorised re-bless of `world_determinism`** for the wave (BL-918 and BL-922 both move the
   digest): `cmd //c "tools\verify\build_lua_harness.bat world_determinism"` then run it; record
   old→new digests in the DEVLOG. Never per item.
3. Run `history_sim_harness` (3 pre-existing failures on main: R3a2, R3a3, B384c — BL-922 should
   move R3a2/R3a3), `save_roundtrip`, `save_envelope_roundtrip` (`build_app.bat save_envelope_roundtrip`),
   `colonisation_harness`.
4. `history_sweep 16 --epoch 0` from a scratch dir; compare against the baseline at
   `C:\Users\benbo\AppData\Local\Temp\claude\gen_sweep\baseline\sweep16.txt` (main 6995b41e:
   powers 42, largest share 11%, peak 20%, battles 3114, conquests 1812, taken-3+ 31%, secessions 2,
   reach refusals 0, rose-and-fell 2/world in 15/16). Record the wave-1 readings in the DEVLOG.
5. `verifier-review` over the integrated diff, then a `code-reviewer` pass (author ≠ reviewer).
6. **Live click**: open build_rel (rebuild it — it is stale), arrive on rounds 3 and 4, watch them,
   confirm the ticker, seats, rivers, relief, Pop/Mt columns. The captures prove render, not reach.
7. Bookkeeping per item: flip the `requirements.json` rows (batch `sprint-39-drama`) with result
   metrics from the agents' reports (each report is in this session's DEVLOG entry to write), drain
   the REFINED.md tasks, run `backlog_lint.js`, `archive_landed.js` for the seven, `mirror_check.js`,
   `devlog_index.js`. NR entries to file: the kinship defect (novel-work), BL-915's rivers-in-preview
   world change and BL-919's verify-path change (decision-taken), NR-831 numbers (then prune).
8. Push.

## Wave 2 (after the close-out): BL-914, BL-917, BL-920, BL-921, BL-924, BL-925, BL-929

Brief each `generation-dev` / `ui-dev` (`claude` for cross-layer) worktree agent as wave 1 was:
fetch and confirm the merge-base, read only the item (`backlog_query.js --grep BL-9xx --full`) and
its named doc sections, block on waits, commit on the branch with the item message, report, stop.
Specifics learned this wave:
- Tell agents `cmd //c` is refused inside a worktree; use `bash tools/verify/build_lua_harness.sh`.
- Tell agents not to yield mid-wait (several did, and resumed on their own notifications).
- BL-920: on the 0 CE arc **every founding is the Settle verb** (schedule = 0, BL-926's census); a
  Settle daughter belongs to its founder, only scheduled ground arrives unorganised. Threshold
  provisional 300,000 heads; BL-926's census prints the opening distribution.
- BL-914: `run_secs` is already 30; the work is the write-only tap and the clamped playhead;
  retire Restart on both pass rounds and give its row to the board; Pause + scrubber after landing.
- BL-917: BL-916 already emits `road_promoted` events with year (region = end a, other = end b).
- BL-921 and BL-929 read BL-922's capital-based supply.

Wave 3: BL-923 (requires BL-920 and BL-922).

## Traps
- The shell rejects long inline commands (ENAMETOOLONG / spurious EOF): write scripts to the
  scratchpad and run them.
- `build/` holds stale harness exes; build the target before running it.
- The tree carries another session's `history_sweep.json` and six `perf_*.csv` — never sweep them
  into a commit; `REFINED.md` and `req/requirements.json` are this batch's and are committed here.
- Three items rewrote `history_lapse.cpp` at once; expect the same in wave 2 (BL-914, BL-917, BL-925
  all draw on it) — sequence their merges and reconcile by hand.
