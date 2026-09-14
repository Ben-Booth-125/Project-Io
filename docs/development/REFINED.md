# REFINED — active worklist

## Sprint 41 — Exploration trade (Batch Delivery, opened 2026-09-14)

A 16-seed `exploration_sweep` review found the round busy but not trading. Five items filed, the
design forks answered on the form the same day and written into `docs/generation/EXPLORATION.md`;
the three sprint-40 tuning items (formerly Wave A below) fold in as the final wave. Requirements:
`requirements.json` batch `sprint-41-exploration-trade`.

### Collision map

| Task | Writes | provides | consumes |
|---|---|---|---|
| T1 BL-952+BL-951 | `tools/verify/exploration_sweep.cpp` | `exploration_row` capture fields; reading 3 metric | — |
| T2 BL-956 | `history_sim.hpp/.cpp` (handoff section), `hard_coded_world.cpp`, `era_minus_one.hpp`, `exploration_sim_harness.cpp` | `struct exploration_output`, `make_exploration_output`, `exploration_output_valid`, `era_minus_one_fixture::exploration_handoff` | `pass_one_output` (landed), `history_sim_state` (landed) |
| T3 BL-953 | `history_sim.hpp/.cpp` (campaign scorer, subjection, preference), `exploration_sim_harness.cpp`, `exploration_sweep.cpp` (reading 6) | `polity_good_want_q(...)`, `history_sim_params::w_want_q` | `derive_culture_preference`, `region::scarcity_q` (landed) |
| T4 BL-954 | `history_sim.hpp/.cpp` (upkeep, scarcity, treaty value), `settlement.hpp`, `exploration_sim_harness.cpp`, `exploration_sweep.cpp` (reading 11) | `struct trade_flow`, `history_sim_state::trade_flows`, `region::scarcity_raw_q`, `history_sim_params::treasury_trade_income_q`, `treaty_trade_weight_q`; removes `treasury_market_income_q` | `has_treaty_clause`, `network_supply_q`, `port_stock_q`, `navy_stock` (landed) |
| I1 integrate wave 1 | main session | `exploration_output::trade_flows` | T2 `exploration_output`, T4 `trade_flows` |
| T5 BL-955 | `history_sim.hpp/.cpp` (`run_exploration_upkeep` spend), `exploration_sim_harness.cpp`, `exploration_sweep.cpp` (reading 7) | spend allocation | T3 `polity_good_want_q`, T4 post-flow `scarcity_q` and upkeep signature |
| T6 BL-949+BL-950 | `history_sim.cpp` constants/deterrence, `exploration_sweep.cpp` | wave digest shape description | everything above, integrated |

**Split call.** Wave 1 fans out to four `generation-dev` worktree agents: the tasks are
slice-able and T2–T4 share `history_sim.cpp` only in disjoint sections, which worktrees absorb.
T5 waits on T3 and T4 because it reads both. T6 is tuning and must measure the integrated world,
so it runs last and owns the wave's digest description (DELIVERY.md § The digest re-bless is one
act per WAVE). No item re-blesses anything.

**Doc coverage.** `EXPLORATION.md` and `DIGITISATION.md` already carry every design this batch
builds (commit `89cc4d4e`, `> ⟳` note in EXPLORATION.md). Items that uncover a design gap file it;
they do not settle it in code.

### Wave 1 — mechanisms and instruments (parallel)

- [x] **T1 · BL-952** (sweep builds each world once) **+ BL-951** (fair strength metric) — one
      agent, one file. Satisfies BL-952 R1–R3, BL-951 R1.
- [x] **T2 · BL-956** (Exploration handoff crosses) — struct, validator, world setup reads 1660
      grudges and corridors. Satisfies R1, R2, R4.
- [x] **T3 · BL-953** (wants point outward) — campaign lean, subjection ranking, live preference.
      Satisfies R1–R5.
- [x] **T4 · BL-954** (trade flows between polities) — flows, relief, income, treaty value,
      reading 11. Satisfies R1–R6.
- [x] **I1** — merge T1–T4 in order T1, T2, T4, T3; add `trade_flows` to `exploration_output`
      (BL-956 R3); `verifier-review` over the merged diff; build; `exploration_sim_harness`,
      `history_sim_harness`, `world_determinism`, 3-seed sweep.

Wave 1 merged to main as `f260d202` (2026-09-14) after a cold review and one fix round.

### Wave 2 — spend becomes a choice

- [ ] **T5 · BL-955** (spend is scored) — allocation inside upkeep. Satisfies R1–R4.

### Wave 3 — tune against the integrated world

- [ ] **T6 · BL-949** (new road tier fires) **+ BL-950** (displacement clears the bar) — 16-seed
      baseline first (NR-862: the 0.88 premise was three seeds on a pre-schism world), then tune,
      then the wave's digest movement described in world shape for Ben. Satisfies BL-949 R1–R2,
      BL-950 R1–R3, BL-951 R2.

## Post-sprint-40 review block (opened 2026-09-13)

Ben live-reviewed the sprint-40 NEEDS_REVIEW queue and asked for a Culture-round fix, the
Exploration round in the wizard, and a speed control on every lapse round. BL-946 and BL-947
landed (`e18ab370`, `fdc445fb`) and are archived; the former Wave A moved into sprint 41 above.

- [ ] **BL-948** (LAPSE_TIMELAPSE_SPEED_CONTROL) — 45s/90s/180s control on every lapse round,
      default 90s. Covers the Exploration round too. Not in sprint 41.
