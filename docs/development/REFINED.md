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
- **Backlog wave A** (CLOSED 2026-09-16). Twelve lanes, sixteen items: twelve delivered, four archived unmerged; re-blessed in 32e04a19. Waves B-D never ran -- Ben stopped at wave A and archived the rest. Record in the DEVLOG entry of the same date.
- **Sprint 44 — the corporate web's plumbing** (CLOSED 2026-09-18). BL-1030..1033 delivered; the retro is archived with the sprint; NR-889 carries the cost readings.

## Sprint 45 — industrialisation makes the web real (opened 2026-09-18)

Three waves. Every world-mover lands behind a switch and turns on together in BL-1044, so the sprint
spends one re-bless. Memory caps concurrency (15.5 GB; a sweep holds ~2.9 GB): lanes run their own
harnesses one at a time and never `player_seed_sweep`; the main session runs the 16-seed
`--digest-check` on the integrated tree after each merge. Requirement groups:
`world-copy-ticks-exact`, `span-boundary-resume`, `resume-road-tier`, `industry-tree-wired`,
`charter-spend-rules`; later waves get theirs when promoted.

**Wave 0 — neutral, parallel worktrees**

- [ ] **BL-1034 (world copies diverge)** — files: src/world/world.hpp/.cpp, tools/verify/world_copy_determinism.cpp (new). In flight from 2026-09-18.
  provides: a faithful world copy; the reproduction harness. consumes: harness_params.hpp's app-order build and settle (landed).
- [ ] **BL-1036 (span boundary resume)** then **BL-1037 (resume road tier)** — lane A, generation-dev. files: src/world/history_sim.hpp/.cpp, era_minus_one.cpp, tools/verify/digitisation_sim_harness.cpp.
  provides: resume_dated_objects, consolidation_year, near_home_cutoff_year, resume_seeds_corridor_tier (off), the fidelity mode. consumes: exploration_output (landed).
- [ ] **BL-1038 (Industry tree wired)** — lane B, generation-dev. files: src/world/industry_tree_data.hpp (generated), history_sim.hpp/.cpp, tools/session/tree_lint.js, tools/verify/exploration_sim_harness.cpp. Shares history_sim.* with lane A in disjoint regions; the main session resolves the merge.
  provides: industry_tree_enabled (off), industry_open_year, the Industry mask triple and fold, the urban-mass rate, the fork-reachability lint. consumes: the amended industry_tree.json (this cut).
- [ ] **BL-1039 (charter spend rules)** — lane C, economy-dev. Starts when BL-1034 lands (both edit player_seed_sweep and harness_params). files: src/world/charter_budget.hpp, corporation_generation.cpp, budget_system.hpp, tools/verify/player_seed_sweep.cpp, harness_params.hpp, charter_cost_sweep.json. Its timing rows run on a quiet machine, serially.
  provides: the sqrt cap rule, density_ceiling reason, the capital-from-unspent rule, per-good tallies. consumes: charter_budget / charter_spend_params (landed, BL-1032).

**Wave 1 — the span and Beat 1, still behind switches**

- [ ] **BL-1040 (Digitisation span)** — waits on BL-1036. files: era_minus_one.*, hard_coded_world.*, world_gen_config.hpp, history_sim.hpp, src/ui/startup_screens.cpp, digitisation_sim_harness.cpp, exploration_sweep.cpp.
- [ ] **BL-1041 (industry points)** — waits on BL-1038 and BL-1040. files: settlement.hpp, history_sim.*, hard_coded_world.cpp, digitisation_sim_harness.cpp.
- [ ] **BL-1042 (stockpile to budget)** — waits on BL-1041 and BL-1039. files: population_generation.*, world.hpp, hard_coded_world.cpp, landscape_search.hpp, src/core/app.cpp, harness_params.hpp, verify_api.cpp, main.cpp, player_seed_sweep.cpp, world_determinism.cpp.

**Wave 2 — measure, then Ben's calls**

- [ ] **BL-1043 (real-stockpile charter sweep)** — waits on BL-1042 and BL-1034. Quiet machine, keep-awake. Ends in NEEDS_REVIEW calls.

**Wave 3 — ship**

- [ ] **BL-1044 (Beat 1 ships)** — waits on BL-1037, BL-1043 and Ben's rulings. The one re-bless, the 1960 readings, the cold review.
