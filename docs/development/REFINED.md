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

- [ ] **BL-946** (EXPLORATION_ROUND_WIRED_INTO_WIZARD) — flip `exploration_sim_enabled` default to
      true (golden re-baseline, authorised); insert an Exploration round between Empires and the
      renamed Digitisation placeholder; build a real tap so the Exploration span's own events
      reach the lapse map (closes NR-857's gap, which is also why BL-943's exemplars are currently
      inert on this span).
- [ ] **BL-947** (CULTURE_ROUND_COASTS_TO_400BCE) — diagnosed: the migration's terminating
      condition is derived per-seed, and CIVILISATION.md's "coast to 400 BCE" design was never
      implemented at the wizard-round level. Independent of BL-946 (round 3's index doesn't move).
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
