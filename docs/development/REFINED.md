# REFINED — active worklist

## Sprint 41 — Exploration trade: COMPLETE 2026-09-14

All nine items merged and archived; world shape authorised at Alarm 525 (NR-867). The full plan and collision map are in git history (`cacd27dc`).

## Post-sprint-40 review block (opened 2026-09-13)

Ben live-reviewed the sprint-40 NEEDS_REVIEW queue and asked for a Culture-round fix, the
Exploration round in the wizard, and a speed control on every lapse round. BL-946 and BL-947
landed (`e18ab370`, `fdc445fb`) and are archived; the former Wave A moved into sprint 41 above.

- [ ] **BL-948** (LAPSE_TIMELAPSE_SPEED_CONTROL) — 45s/90s/180s control on every lapse round,
      default 90s. Covers the Exploration round too. Not in sprint 41.

## Sprint 42 — generation sharpened before Digitisation: wave 0 (opened 2026-09-15)

Instruments and gates; no item in this wave may move the world (BL-969 and BL-981 move no digest
by construction, and both prove it with world_determinism). One worktree agent per task; the main
session merges, builds, runs the gate list (world_determinism, history_sim_harness at its 2-failure
baseline, exploration_sim_harness, colonisation_harness) and verifies.

- [ ] **BL-981** (COLONISATION_D1_D3_REGRESSION) — bisect on seed 2 with the cached builder across
      the sprint-40/41 commits; fix at the verb; extend D1/D3 to the Exploration span if the culprit
      is there. Files: `src/world/history_sim.cpp`, `tools/verify/colonisation_harness.cpp`. Satisfies: R1, R2.
- [ ] **BL-980** (GENERATION_CORPUS_RECONCILE) — the twelve contradictions, in place or as NR calls.
      Files: the twelve named in the item. Satisfies: R1, R2.
- [ ] **BL-974** (TREE_ROOTS_AND_HEADER_REGEN) — two roots, header-matches-store lint, one-root rule.
      Files: `docs/generation/trees/*.json`, `tools/session/tree_lint.js`, `tools/session/gen_empire_tree_table.js`. Satisfies: R1–R3.
- [ ] **BL-962 + BL-964** — endowment spread asserted; drift digest asserted.
      Files: `tools/verify/planetology_sweep.cpp`, `tools/verify/continent_drift.cpp`. Satisfies: R1, R2.
- [ ] **BL-966** (TERRAIN_ECONOMIC_GATE) — census bands asserted; new `survey_endowment_harness`.
      Files: `tools/verify/earthlike_tile_census.cpp`, `tools/verify/survey_endowment_harness.cpp`, the skill. Satisfies: R1–R3.
- [ ] **BL-970** (SHARE_READINGS_POPULATION_WEIGHTED) — both share columns; scoreboard on population; JSON regenerated.
      Files: `tools/verify/history_sweep.cpp`, `src/world/history_sim.hpp/.cpp` (record only), `history_sweep.json`. Satisfies: R1–R3.
- [ ] **BL-971** (DISPLACEMENT_VOLUME_WEIGHTED) — weighted readings; silent-seed count; `exploration_sweep.json` checked in.
      Files: `tools/verify/exploration_sweep.cpp`, `exploration_sweep.json`. Satisfies: R1–R3.
- [ ] **BL-979** (INSTRUMENTS_RUN_THE_WINNER) — shared start helper; nine instruments; fraction-in-band reading.
      Files: nine `tools/verify/*.cpp`, `tools/verify/harness_params.hpp`, `src/world/market_saturation.hpp/.cpp`. Satisfies: R1–R3.
- [ ] **BL-969** (PASS_ONE_VALIDATOR_IN_PRODUCTION) — validators on the shipped path; struct consumers; culture table crosses and is checked.
      Files: `src/world/hard_coded_world.cpp`, `src/world/history_sim.hpp/.cpp`. Satisfies: R1–R3.

Collision map (file layer): BL-969 and BL-970 both touch `history_sim.hpp/.cpp` (a validator block
versus a population-per-sample record) — worktrees absorb it, merged BL-970 first. BL-980 touches
`hard_coded_world.hpp` (a comment) beside BL-969's `.cpp`. Everything else is disjoint.
