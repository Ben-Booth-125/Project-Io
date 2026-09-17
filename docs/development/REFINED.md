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

## Sprint 44 — the corporate web's plumbing (opened 2026-09-17)

Sequential; one lane at a time. Requirements for the seam: `requirements.json § charter-budget-seam`.

- [x] **BL-1030 (player_seed_sweep parity)** (complete 2026-09-17, 0133ee2b) — files: tools/verify/harness_params.hpp, tools/verify/player_seed_sweep.cpp.
  provides: shipped world build helper, validation settle helper (harness_params.hpp). consumes: apply_shipped_landscape (landed).
- [x] **BL-1031 (landscape world pin)** (complete 2026-09-17, ab6b162f; 16/16 check on main) — files: tools/verify/player_seed_sweep.cpp. Waits on BL-1030.
  provides: --digest, --digest-check, the pinned table. consumes: the BL-1030 helpers, write_world_snapshot, state_hash (landed).
- [ ] **BL-1032 (charter budget seam)** — files: src/world/charter_budget.hpp (new), landscape_search.hpp/.cpp, corporation_generation.hpp/.cpp, tools/verify/harness_params.hpp. Worktree lane, cold review. Waits on BL-1031.
  (amended 2026-09-17: app.cpp takes no behavioural change; reviewed against its harness copy.)
  provides: charter_budget, charter_spend_params, charter_spend_report, landscape_search_params::budget/spend, the apply_landscape_candidate budget overload, charter_web_from_budget. consumes: the BL-1031 pins, place_starting_assets anchor_window (landed), nearest_region (landed).
- [ ] **BL-1033 (charter budget cost)** — files: tools/verify/player_seed_sweep.cpp, tools/verify/harness_params.hpp. Waits on BL-1032.
  provides: --charter-budget, --budget-scale, --province-cap, --specialist-price, the synthetic builder (tools/verify only). consumes: everything BL-1032 provides.
