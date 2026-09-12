# REFINED — active worklist

Sprint 40 ("Exploration") opens 2026-09-12, Delivery — Full, Batch Delivery
(`docs/development/DELIVERY.md` § Batch Delivery). Sequencing corrected per NR-846: BL-930 ->
BL-931 -> BL-937 opens the sprint, ahead of the NEXT_SESSION.md wave table's literal order.

## Wave 1 — the engine and the actor's technology — LANDED 2026-09-12

- [x] **BL-930** (EXPLORATION_TREE) — commit `85501a31`. Tree wired, stubs pinned, tree_lint OK.
- [x] **BL-931** (EXPLORATION_PHASE_RUNS) — commit `e06445d8`. Span runs opt-in
      (`world_params::exploration_sim_enabled`, default false — flagged for Ben, NR pending).
      Both independently rebuilt and reverified in the main session: `exploration_sim_harness`
      ALL PASS, `world_determinism` ALL PASS (digests unmoved), `history_sim_harness` 2
      pre-existing failures only (R3a2/R3a3), full app BUILD_OK. Archived to
      `archive/backlog-design-2026-Q3.json`.

## Wave 1.5 — instrumentation (lands before any mechanism item, not at the close)

- [ ] **BL-937** (EXPLORATION_SWEEP_READINGS) — the ten readings at 1660 over a seed spread, in
      `tools/verify/history_sweep.cpp`. Displacement is a RATIO (neighbour-war vs frontier-skirmish);
      a fall in both is a failure. `requires: [BL-931]`.

## Waves 2-5 — not yet promoted

BL-932/939/940 (treasury/scarcity/throughput), BL-933/934/935 (treaties/subjects/ports),
BL-941/942/936 (deterrence/two-strategies/preference), BL-943/938 (exemplars/tree migration).
`BL-944` gated on a Ben decision (NEXT_SESSION.md § One decision owed). `BL-945` parked for
Digitisation, not this sprint. Promoted wave by wave as the prior wave lands and verifies.
