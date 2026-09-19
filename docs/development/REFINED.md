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

- [x] **BL-1034 (world copies diverge)** — DELIVERED 2026-09-18: merged d61b6ae8, cold-reviewed, --digest-check 16/16 on 26c8ec6e. files: src/world/faithful_unordered_map.hpp (new), world.hpp, province.hpp, tools/verify/world_copy_determinism.cpp (new), world_digest.hpp (new).
  provides: a faithful world copy; the reproduction harness. consumes: harness_params.hpp's app-order build and settle (landed).
- [x] **BL-1036 (span boundary resume)** then **BL-1037 (resume road tier)** — DELIVERED 2026-09-18: merged 614a02e5, follow-ups 58193812, --digest-check 16/16. BL-1036 R6 partial (BL-1049). Lane A, generation-dev. files: src/world/history_sim.hpp/.cpp, era_minus_one.cpp, tools/verify/digitisation_sim_harness.cpp.
  provides: resume_dated_objects, consolidation_year, near_home_cutoff_year, resume_seeds_corridor_tier (off), the fidelity mode. consumes: exploration_output (landed).
- [x] **BL-1038 (Industry tree wired)** — DELIVERED 2026-09-18: merged 29b74f69, fix round 191c212b, --digest-check 16/16. Lane B, generation-dev. files: src/world/industry_tree_data.hpp (generated), history_sim.hpp/.cpp, tools/session/tree_lint.js, tools/verify/exploration_sim_harness.cpp. Shares history_sim.* with lane A in disjoint regions; the main session resolves the merge.
  provides: industry_tree_enabled (off), industry_open_year, the Industry mask triple and fold, the urban-mass rate, the fork-reachability lint. consumes: the amended industry_tree.json (this cut).
- [x] **BL-1039 (charter spend rules)** — DELIVERED 2026-09-19: timed rows on a quiet machine (runs.bl1039_sqrt), ceiling 120 (NR-902), --digest-check 16/16 and empty/zero/refused 3/3 on be5b3d72 (chain 11). files: src/world/charter_budget.hpp, corporation_generation.cpp, budget_system.hpp, tools/verify/player_seed_sweep.cpp, harness_params.hpp, charter_cost_sweep.json.
  provides: the sqrt cap rule, density_ceiling reason (filling goods in turn), per-good tallies; the unspent-points capital rule was built and reverted (NR-895). consumes: charter_budget / charter_spend_params (landed, BL-1032).

**Wave 1 — the span and Beat 1, still behind switches**

- [x] **BL-1040 (Digitisation span)** — DELIVERED 2026-09-19: merged 70923856, cold-reviewed, --digest-check 16/16 on 849a358d (chain 8). files: era_minus_one.*, hard_coded_world.*, world_gen_config.hpp, history_sim.hpp, src/ui/startup_screens.cpp, digitisation_sim_harness.cpp, exploration_sweep.cpp.
- [x] **BL-1051 (span open survey)** — DELIVERED 2026-09-19: merged 44545123, cold-reviewed, --digest-check 16/16 on 849a358d (chain 8). Every region surveyed for fuel and forest at the span open; ground_forest and furnace_lit read from it (NR-891, NR-892). files: settlement.hpp/.cpp, hard_coded_world.cpp, history_sim.*, exploration_sim_harness.cpp, digitisation_sim_harness.cpp.
- [x] **BL-1041 (industry points)** — DELIVERED 2026-09-19: merged f8137b8d, fix round 849a358d, --digest-check 16/16 (chain 8), readings 8 and 9 on 849a358d. files: settlement.hpp, history_sim.*, hard_coded_world.cpp, digitisation_sim_harness.cpp.
- [x] **BL-1053 (setup reads span close)** — DELIVERED 2026-09-19: merged 9725ee79, --digest-check 16/16 (chain 8), cold review clean (two low findings in BL-1058). When the span runs, world setup reads the 1960 close (treasuries, grudges, roads, junction markets); plus the BL-1040 review's harness and validator tightening and the loading-bar count. Must land before BL-1043. files: hard_coded_world.cpp, nation_generation.cpp, history_sim.*, app.cpp, startup_screens.cpp, digitisation_sim_harness.cpp.
- [x] **BL-1056 (points size-neutral)** — DELIVERED 2026-09-19: merged acc81398, fix round 2867cdaa, --digest-check 16/16 (chain 9). The resulting Fuel Doctrine split (coke 142 / charcoal 373) is NR-899, Ben's call before BL-1043 measures. files: history_sim.*, settlement.hpp, digitisation_sim_harness.cpp, exploration_sim_harness.cpp.
- [x] **BL-1059 (top-third bar)** — DELIVERED 2026-09-19: three rounds (NR-900, NR-904 in the build), --digest-check 16/16 on be5b3d72 (chain 11). files: history_sim.*, settlement.*, digitisation_sim_harness.cpp, exploration_sim_harness.cpp.
- [ ] **BL-1042 (stockpile to budget)** — MERGED c9431b0b, fix round 587b5657 (NR-901: no town, no conversion; razed as its own reason), cold-reviewed; --digest-check 16/16 and stockpile_budget_check (plain and --r8) PASS on be5b3d72 (chain 11). Owed: the main-session stockpile rows (chain 11's last step, then chain 12 after BL-1060).
- [ ] **BL-1060 (charter spend hardening)** — MERGED 9087e5ea, round 2 71af0b5c (NR-903 skip, NR-905 even share, ceiling 120), cold-checked; round 3 (NR-906: the share is a reservation) in its lane. Owed: merge round 3, chain 12 (--digest-check 16/16, empty/zero/refused, stockpile rows, one --charter-cost row), R7.

**Wave 2 — measure, then Ben's calls**

- [ ] **BL-1043 (real-stockpile charter sweep)** — waits on BL-1042, BL-1053 and BL-1034. Quiet machine, keep-awake. Ends in NEEDS_REVIEW calls.

**Wave 3 — ship**

- [ ] **BL-1050 (order-dependent reads)** — built on a branch, merged only with BL-1044's switch flips (it moves the pins). Five readers walk sorted ids; `world_copy_determinism --copy-by snapshot` passes (NR-894).
- [ ] **BL-1044 (Beat 1 ships)** — waits on BL-1037, BL-1043, BL-1050 and Ben's rulings. The one re-bless, the 1960 readings, the cold review.
