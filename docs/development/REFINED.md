# REFINED — active worklist

Post-sprint-40 review block opens 2026-09-13, Delivery — Full. Ben live-reviewed the sprint-40
NEEDS_REVIEW queue and answered nine open calls (nine NR entries resolved), then asked for three
more things while looking at the live build: a Culture-round bug diagnosed and fixed, the
Exploration span wired into the wizard with a Digitisation placeholder, and a speed control on
every lapse round's time-lapse.

## Wave A — sim tuning (independent of the wizard work, disjoint files)

- [ ] **BL-949** (POST_ROAD_RUNG_ACTUALLY_FIRES) — lower the treasury cost/threshold so the road
      ladder's third rung fires on a sweep.
- [ ] **BL-950** (DISPLACEMENT_CLEARS_THE_BAR) — strengthen BL-941's near/far deterrence split so
      the median displacement ratio clears 1.0 on a 16-seed sweep.
- [ ] **BL-951** (STRENGTH_METRIC_READS_STRATEGY_FAIRLY) — reading 3's "strongest realms" metric
      moves from region count to treasury/throughput rank.

All three live in `history_sim.cpp`/`exploration_sweep.cpp` only — bundle into one worktree agent,
same pattern as sprint 40's waves 2-4.

## Wave B — the wizard restructure (bigger, UI + generation, sequenced internally)

- [x] **BL-946** (EXPLORATION_ROUND_WIRED_INTO_WIZARD) — commit `e18ab370`. LANDED 2026-09-14. The
      wizard walks 6 rounds now (System, Life, Culture, Empires, Exploration, Digitisation); round
      5 runs its own pass and shows a real populated time-lapse (treaties forming, non-zero
      battles/conquests/foundings, its own 1200-1660 CE span stated on screen — confirmed by a
      live click, not just harnesses). Golden flip's digests reproduced exactly independently.
      save_roundtrip/save_envelope_roundtrip clean despite the version bump (13→14). Archived.
- [x] **BL-947** (CULTURE_ROUND_COASTS_TO_400BCE) — commit `fdc445fb`. LANDED 2026-09-13. Displayed
      span now `max(true migration end, Empires opening year)`, never clamped backward — an
      overrun (measured at 6/60 seeds, 10%) shows honestly instead of being papered over.
      Independently rebuilt and reverified (121/121 on the new harness, `world_determinism`
      digest unchanged from the BL-944 baseline — a pure display fix). NR-860 records the 10%
      overrun rate for Ben; a second identical bug site in `startup_screens.cpp`'s golden-dir
      reuse path was found but not fixed (flagged, needs a UI build to verify). Archived.
- [ ] **BL-948** (LAPSE_TIMELAPSE_SPEED_CONTROL) — 45s/90s/180s control on every lapse round,
      default 90s. Requires BL-946 (must cover the new Exploration round too, not just Culture/
      Empires) — land last in this wave.

BL-947 can run in parallel with BL-946 (touches `settlement.cpp`/`hard_coded_world.cpp`'s round-3
block, not the round-count/tap machinery BL-946 touches) but both land in `hard_coded_world.cpp`,
so watch for a real merge on that file even though the sections shouldn't overlap.

## Bookkeeping

Nine NR entries resolved this pass (NR-847, 848, 849, 850, 852, 854, 855, 856, 857); NR-851
superseded by NR-855. `docs/development/NEEDS_REVIEW.md` has the readable list of what remains
(pre-existing debt from before sprint 40, not this session's to resolve).
