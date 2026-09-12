# REFINED — active worklist

Sprint 40 ("Exploration") opens 2026-09-12, Delivery — Full, Batch Delivery
(`docs/development/DELIVERY.md` § Batch Delivery). Sequencing corrected per NR-846: BL-930 ->
BL-931 -> BL-937 opens the sprint, ahead of the NEXT_SESSION.md wave table's literal order.

## Wave 1 — the engine and the actor's technology

- [ ] **BL-930** (EXPLORATION_TREE) — wire the Exploration tree into `history_sim`: `exploration_mask`,
      `exploration_investing`, `exploration_progress_q` per polity; generalise
      `gen_empire_tree_table.js`; honour `excludes`. Four new scorer terms (`purse_low`,
      `wants_unmet`, `throughput_bound`, `subject_held`) land STUBBED — their real quantities
      (treasury/scarcity/throughput) don't exist until wave 2. `requires: []`.
- [ ] **BL-931** (EXPLORATION_PHASE_RUNS) — the 1200->1660 span runs on the shared `history_sim`
      engine, taking `pass_one_output`. Round-level upkeep step (earn/pay/invest) and objects-with-
      a-term (treaties expire) as the two honest additions. `requires: [BL-930]`.

## Wave 1.5 — instrumentation (lands before any mechanism item, not at the close)

- [ ] **BL-937** (EXPLORATION_SWEEP_READINGS) — the ten readings at 1660 over a seed spread, in
      `tools/verify/history_sweep.cpp`. Displacement is a RATIO (neighbour-war vs frontier-skirmish);
      a fall in both is a failure. `requires: [BL-931]`.

## Waves 2-5 — not yet promoted

BL-932/939/940 (treasury/scarcity/throughput), BL-933/934/935 (treaties/subjects/ports),
BL-941/942/936 (deterrence/two-strategies/preference), BL-943/938 (exemplars/tree migration).
`BL-944` gated on a Ben decision (NEXT_SESSION.md § One decision owed). `BL-945` parked for
Digitisation, not this sprint. Promoted wave by wave as the prior wave lands and verifies.
