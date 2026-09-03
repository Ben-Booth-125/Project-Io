# REFINED — active worklist

Sprint 32 (gamified generation), wave 1. Promoted 2026-09-03 after the subsystem map.

**Scope of this wave.** BL-747 (two-span prehistory) and BL-754 (generation budget). These
two are the foundation and the measurement: the arc running at all, and the number that says
whether the rest is affordable — which is Ben's stated question for the sprint.

**Not in this wave, and why.** BL-751 (economic settle pass) is gated on sprint 33's growth
half. BL-749 (sea-leg campaign) was scoped and found under-specified: the map turned up two
pre-existing defects it sits on (BL-755, the sim already crosses water free; BL-756, it settles
on ocean) plus five design calls only Ben can take. BL-748 (industrial pass ladder) and BL-750
(tariff posture) follow BL-747 and are wave 2.

## BL-747 — two-span prehistory

- [ ] T1. `era_minus_one_enabled` drops the `epoch_year < 1700` clause and gates on
      `prehistory_years > 0` alone. The settlement clause stays at the call site.
- [ ] T2. `history_sim_params` gains `boundary_year` (default: past any stop year, so the
      struct default is one span). `era_minus_one_sim_params` derives it as
      `epoch_year - 400`, and fills `tick_bands` per span rather than one band.
- [ ] T3. `run_history_sim` reads `boundary_year` to ceiling the roster band a polity may
      reach — Medieval before, unrestricted after — for the works roster and the unit roster,
      from one derivation, not two.
- [ ] T4. The 0 CE arc executes identically: the boundary falls past `stop_year`, the ceiling
      never lifts, and the second span is empty by construction. Verified against the
      2026-09-03 digests, not by re-blessing.
- [ ] T5. `history_sweep` reports both spans.

## BL-754 — generation budget (instrumentation half only)

- [ ] T6. Generation times each pass and reports it; `world_determinism` and `history_sweep`
      print the per-pass wall clock. Reported, never asserted.
- [ ] T7. Record the two-span cost against the 2026-09-03 baseline (era pass 6.4 s of an
      11.2 s world build) on BL-754.

**Requirement groups.** `two-span-prehistory` (R1–R5), `generation-budget` (R1).
