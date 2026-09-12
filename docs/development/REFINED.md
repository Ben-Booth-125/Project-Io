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

## Wave 1.5 — instrumentation — LANDED 2026-09-12

- [x] **BL-937** (EXPLORATION_SWEEP_READINGS) — commit `50bc60e7`. Readings 1-2 measured for real
      (16-seed sweep, `tools/verify/exploration_sweep.cpp`); 3-10 scaffolded, honestly marked
      NOT YET MEASURABLE rather than faked. Baseline for later waves to move: displacement ratio
      median 0.16 (no displacement yet — expected, its mechanism is later), conflict-persists
      PASSES. Independently rebuilt and rereplicated in the main session; world_determinism and
      history_sim_harness at their pre-existing baselines. NR-848 flags the operationalisation.
      Archived.

## Wave 2 — the three quantities everything else reads

- [ ] **BL-932** (POLITY_TREASURY) — capital-seat treasury, consolidated once at 1200 CE.
- [ ] **BL-939** (SCARCITY_SIGNAL_PER_MARKET) — one integer per (good, market), never a price.
- [ ] **BL-940** (CORRIDOR_THROUGHPUT_AND_THE_ROAD_LADDER) — throughput off `network_supply_q`,
      the road ladder's third rung.

These three feed BL-930's stubbed scorer terms (`purse_low`, `wants_unmet`, `throughput_bound`)
and BL-937's readings 8-9 — landing them should turn those stubs live and move the sweep.

## Waves 3-5 — not yet promoted

BL-933/934/935 (treaties/subjects/ports), BL-941/942/936 (deterrence/two-strategies/preference),
BL-943/938 (exemplars/tree migration). `BL-944` gated on a Ben decision (NEXT_SESSION.md § One
decision owed). `BL-945` parked for Digitisation, not this sprint. Promoted wave by wave as the
prior wave lands and verifies.
