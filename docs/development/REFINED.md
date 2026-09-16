# REFINED — active worklist

## Drained 2026-09-16

Three finished blocks were cleared at the session close; each one's record lives in the
DEVLOG entry for its session, its requirement group, and the archived backlog rows.

- **Sprint 41 — Exploration trade** (COMPLETE 2026-09-14). Nine items; world shape authorised
  at Alarm 525. Plan and collision map in `cacd27dc`.
- **Post-sprint-40 review block** (opened 2026-09-13). Its last open task was the lapse pace
  control, which landed 2026-09-16 at 30 s / 1 m / 1 m 30 s after Ben watched the first cut.
- **Sprint 42 wave 0** (COMPLETE 2026-09-15). Instruments and gates; re-blessed on NR-875.
- **Sprint 42 wave 1** (COMPLETE 2026-09-16). Sixteen items; re-blessed on NR-877 the same day.

## Backlog wave A (opened 2026-09-16)

Ben, 2026-09-16: "switch to delivery mode and build each item, attempting to work in parallel where
possible." Twelve worktree lanes, sixteen items: every item with no open prerequisite that is NOT a
wall-clock measurement. Timing items (BL-1024 region count, BL-1025 held world, BL-983 span cost)
wait for a quiet machine, because twelve concurrent /O2 builds and sweeps make a timing meaningless.
Every world-mover measures its own before/after in its worktree; the sweeps are regenerated once on
the integrated tree; ONE re-bless at the close, with N named causes, authorised by Ben.

- [ ] **BL-1006** (FAR_TRADE_READING) - lane A1, economy-dev. Satisfies: R1-R2.
- [ ] **BL-996** (STRATUM_DEMAND_LADDER) - lane A2, economy-dev. Satisfies: R1-R5.
- [ ] **BL-1008** (HARNESSES_READ_THE_VALIDATION_RUN) - lane A3, economy-dev. Satisfies: R1-R2.
- [ ] **BL-1009** (DIGEST_SEES_THE_MONEY) - lane A4, generation-dev. Satisfies: R1-R3.
- [ ] **BL-842** (SMALL_GRUDGES_FREEZE) - lane A5, generation-dev. Satisfies: R1-R3.
- [ ] **BL-841** (ASSIMILATION_LOST_TO_THE_TAIL) - lane A5, generation-dev (after BL-842, same lane). Satisfies: R1-R2.
- [ ] **BL-1018** (ALARM_STOPS_SATURATING) - lane A6, generation-dev. Satisfies: R1-R4.
- [ ] **BL-1019** (THE_FIRST_CROSSING) - lane A6, generation-dev (after BL-1018, same lane). Satisfies: R1-R4.
- [ ] **BL-1021** (TRADE_REBASED_ON_WHAT_A_REALM_REACHES) - lane A7, generation-dev. Satisfies: R1-R4.
- [ ] **BL-1017** (EMPTY_CULTURES_FOLD_INTO_PARENT) - lane A8, generation-dev. Satisfies: R1-R4.
- [ ] **BL-982** (DIGITISATION_READINGS) - lane A9, generation-dev. Satisfies: R1-R2.
- [ ] **BL-1023** (FOREST_IS_THE_FIFTH_BRANCH) - lane A10, generation-dev. Satisfies: R1-R3.
- [ ] **BL-1016** (CREED_CLASSIFIER_COMPARES_ONE_SCALE) - lane A10, generation-dev (after BL-1023, same lane). Satisfies: R1-R3.
- [ ] **BL-1010** (ERA_WORLD_THREE_REDS) - lane A11, generation-dev. Satisfies: R1-R1.
- [ ] **BL-1022** (IS_COASTAL_GROUND_CHEAPER_TO_TAKE) - lane A11, generation-dev (after BL-1010, same lane). Satisfies: R1-R2.
- [ ] **BL-1020** (SEAT_FLOOR_READS_THE_STATIC_SCORE) - lane A12, ui-dev. Satisfies: R1-R3.

Collision map (file layer): history_sim.cpp is shared by A5 (BL-842), A6, A7 and A8; settlement.cpp
by A5 (BL-841) alone; exploration_sweep.cpp by A6 and A10 (BL-1016); history_sweep.cpp by A7, A8 and
A11 (BL-1022); economy.lua and market_clearing.cpp by A2 alone; haulage_measure.cpp by A1 alone.
Merge order: instruments and docs first (A4, A1, A3, A9, A10, A11), then the history movers (A5, A6,
A7, A8), then the economy mover (A2), then the UI (A12, live click).

Waves B-D were NOT RUN. Ben, 2026-09-16: "This work is highly unstructured, so just leave it at wave A
and we can reinvent anything important later. (So archive)." Their 24 items are cancelled unbuilt and
cold in the archive; each design stands in its authority doc.
