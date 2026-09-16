# REFINED — active worklist

## Sprint 41 — Exploration trade: COMPLETE 2026-09-14

All nine items merged and archived; world shape authorised at Alarm 525 (NR-867). The full plan and collision map are in git history (`cacd27dc`).

## Post-sprint-40 review block (opened 2026-09-13)

Ben live-reviewed the sprint-40 NEEDS_REVIEW queue and asked for a Culture-round fix, the
Exploration round in the wizard, and a speed control on every lapse round. BL-946 and BL-947
landed (`e18ab370`, `fdc445fb`) and are archived; the former Wave A moved into sprint 41 above.

- [ ] **BL-948** (LAPSE_TIMELAPSE_SPEED_CONTROL) — 45s/90s/180s control on every lapse round,
      default 90s. Covers the Exploration round too. Not in sprint 41.

## Sprint 42 — generation sharpened before Digitisation: wave 0 (opened 2026-09-15) — COMPLETE 2026-09-15, gated on NR-875 (the wave re-bless)

Instruments and gates; no item in this wave may move the world except BL-981, whose movement is reported and re-blessed on authorisation (BL-969 moves no digest
by construction, and both prove it with world_determinism). One worktree agent per task; the main
session merges, builds, runs the gate list (world_determinism, history_sim_harness at its 2-failure
baseline, exploration_sim_harness, colonisation_harness) and verifies.

- [x] **BL-981** (COLONISATION_D1_D3_REGRESSION) — bisect on seed 2 with the cached builder across
      the sprint-40/41 commits; fix at the verb; extend D1/D3 to the Exploration span if the culprit
      is there. Files: `src/world/history_sim.cpp`, `tools/verify/colonisation_harness.cpp`. Satisfies: R1, R2.
- [x] **BL-980** (GENERATION_CORPUS_RECONCILE) — the twelve contradictions, in place or as NR calls.
      Files: the twelve named in the item. Satisfies: R1, R2.
- [x] **BL-974** (TREE_ROOTS_AND_HEADER_REGEN) — two roots, header-matches-store lint, one-root rule.
      Files: `docs/generation/trees/*.json`, `tools/session/tree_lint.js`, `tools/session/gen_empire_tree_table.js`. Satisfies: R1–R3.
- [x] **BL-962 + BL-964** — endowment spread asserted; drift digest asserted.
      Files: `tools/verify/planetology_sweep.cpp`, `tools/verify/continent_drift.cpp`. Satisfies: R1, R2.
- [x] **BL-966** (TERRAIN_ECONOMIC_GATE) — census bands asserted; new `survey_endowment_harness`.
      Files: `tools/verify/earthlike_tile_census.cpp`, `tools/verify/survey_endowment_harness.cpp`, the skill. Satisfies: R1–R3.
- [x] **BL-970** (SHARE_READINGS_POPULATION_WEIGHTED) — both share columns; scoreboard on population; JSON regenerated.
      Files: `tools/verify/history_sweep.cpp`, `src/world/history_sim.hpp/.cpp` (record only), `history_sweep.json`. Satisfies: R1–R3.
- [x] **BL-971** (DISPLACEMENT_VOLUME_WEIGHTED) — weighted readings; silent-seed count; `exploration_sweep.json` checked in.
      Files: `tools/verify/exploration_sweep.cpp`, `exploration_sweep.json`. Satisfies: R1–R3.
- [x] **BL-979** (INSTRUMENTS_RUN_THE_WINNER) — shared start helper; nine instruments; fraction-in-band reading.
      Files: nine `tools/verify/*.cpp`, `tools/verify/harness_params.hpp`, `src/world/market_saturation.hpp/.cpp`. Satisfies: R1–R3.
- [x] **BL-969** (PASS_ONE_VALIDATOR_IN_PRODUCTION) — validators on the shipped path; struct consumers; culture table crosses and is checked.
      Files: `src/world/hard_coded_world.cpp`, `src/world/history_sim.hpp/.cpp`. Satisfies: R1–R3.

Collision map (file layer): BL-969 and BL-970 both touch `history_sim.hpp/.cpp` (a validator block
versus a population-per-sample record) — worktrees absorb it, merged BL-970 first. BL-980 touches
`hard_coded_world.hpp` (a comment) beside BL-969's `.cpp`. Everything else is disjoint.

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
