# REFINED — active worklist

## Drained 2026-09-16

Three finished blocks were cleared at the session close; each one's record lives in the
DEVLOG entry for its session, its requirement group, and the archived backlog rows.

- **Sprint 41 — Exploration trade** (COMPLETE 2026-09-14). Nine items; world shape authorised
  at Alarm 525. Plan and collision map in `cacd27dc`.
- **Post-sprint-40 review block** (opened 2026-09-13). Its last open task was the lapse pace
  control, which landed 2026-09-16 at 30 s / 1 m / 1 m 30 s after Ben watched the first cut.
- **Sprint 42 wave 0** (COMPLETE 2026-09-15). Instruments and gates; re-blessed on NR-875.

## Sprint 42 — wave 1, the batch (opened 2026-09-15) — COMPLETE 2026-09-16; re-bless gated on NR-877, BL-1000 open on its live click

Sixteen items, one worktree agent each, merged in dependency order: docs and instruments first
(BL-1002, BL-1001, BL-976, BL-999, BL-968), then the physical stages (BL-965 before BL-961 and
BL-963), then the history movers (BL-967, BL-972, BL-973, BL-998, BL-975), then the seam (BL-977,
BL-978), then the UI (BL-1000, live click). Every world-mover measures its own before/after in its
worktree; the sweeps are regenerated once on the final integrated tree; ONE re-bless at the close,
with N named causes, authorised by Ben.

- [x] **BL-965** (TILE_PASS_REENTRY) — split at the Body/Life boundary, bit-identical. Satisfies: R1, R2.
- [x] **BL-961** (PLANETOLOGY_THERMAL_SERIES) — per-epoch thermal series; Life samples it. Satisfies: R1–R3.
- [x] **BL-963** (TILES_RIDE_PLATES) — measure cost first; slice or stop on the ceiling. Satisfies: R1–R3.
- [x] **BL-967** (RIVERS_PRICED_IN_WALK) — river edges take the coastal discount. Satisfies: R1, R2.
- [x] **BL-968** (EPHEMERAL_CULTURES_MEASURED) — step 1 only: the reading, then Ben's call. Satisfies: R1, R2.
- [x] **BL-972** (FORCE_UPKEEP_IN_THE_WORLD) — per-head upkeep; caps removed once it binds. Satisfies: R1, R2.
- [x] **BL-973** (TREE_EFFECTS_GENERATED) — effects table generated; generic apply; hand-wired nodes gone. Satisfies: R1, R2.
- [x] **BL-976** (TARIFF_DERIVATION_HANDS_TO_DIGITISATION) — single-span derivation retired. Satisfies: R1, R2.
- [x] **BL-975** (NATION_TREASURY_FROM_EXPLORATION) — 1660 treasuries credit nations at the fold. Satisfies: R1, R2.
- [ ] **BL-977** (SEARCH_AXES_LIVE_AND_REACH_TERM) — reach-quality term; roster regenerates. Satisfies: R1, R2.
- [x] **BL-978** (WARM_START_RETIRED) — validation run replaces pre_game_ticks. Satisfies: R1, R2.
- [x] **BL-998** (CONSOLIDATION_FOLDS_EVERY_SEAT) — every held seat folds at 1200. Satisfies: R1, R2.
- [x] **BL-999** (HELD_SEEDS_CAUSE_MEASURED) — four causes on the face. Satisfies: R1, R2.
- [ ] **BL-1000** (WIZARD_LEADERBOARD_POPULATION_SHARE) — board by population; live click. Satisfies: R1.
- [x] **BL-1001** (COLLAPSE_DOC_FOLDED_AND_RETIRED) — fold, move to research, repoint. Satisfies: R1.
- [x] **BL-1002** (BREADCRUMB_DROPPED) — section removed, citations repointed. Satisfies: R1.

Collision map (file layer): history_sim.cpp is shared by BL-972, BL-973, BL-998 (and BL-968's
sweep read); tile_generation.cpp by BL-965, BL-961, BL-963; landscape_* by BL-977 and BL-978;
CIVILISATION.md by BL-1001 and BL-968's reading. Worktrees absorb it; merge order above.
