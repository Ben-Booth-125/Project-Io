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

## Wave 2 — the three quantities everything else reads — LANDED 2026-09-12

- [x] **BL-932** (POLITY_TREASURY) — commit `670d7072`. Capital-seat treasury, one-time 1200
      consolidation plus recurring per-round income (NR-850 flags the recurring-vs-one-time
      reading for Ben). Correlation with corridor touch: 0.330.
- [x] **BL-939** (SCARCITY_SIGNAL_PER_MARKET) — commit `2c3b0a59`. One integer per (good, market),
      contact-gated read, no price.
- [x] **BL-940** (CORRIDOR_THROUGHPUT_AND_THE_ROAD_LADDER) — commit `a6afe7b7`. Throughput reads
      `network_supply_q`; the road ladder's third rung (Post Roads) never fired in an 8-seed sweep
      (NR-849, flagged not silently re-tuned).
      All three: BL-930's stubbed scorer terms now read live quantities. Independently rebuilt and
      reverified — `exploration_sim_harness` 39/39 PASS, `world_determinism` ALL PASS (digests
      unmoved), `history_sim_harness` at the pre-existing 2-failure baseline. Archived.

## Wave 3 — the mechanisms that spend and bind — LANDED 2026-09-12

- [x] **BL-933** (TREATIES_WITH_A_TERM), **BL-934** (COLONIES_ARE_SUBJECTS), **BL-935**
      (PORTS_NAVIES_AND_UPKEEP) — commit `46eabedd` (one commit for all three, NR-853). Treaties
      form/break deterministically (sweep: 1660 formed, 21 broken, 301 standing at 1660); colonies
      are live polities with an `overlord` field (24 subjects, 20 overlords measured); ports/navies/
      armies are treasury-funded and decay (192 polities holding a navy at 1660). Independently
      rebuilt (full app BUILD_OK — the sub-agent's worktree couldn't run the full build, this
      session's rebuild was the first), `exploration_sim_harness` 43/43 PASS, `world_determinism`
      ALL PASS, `history_sim_harness` at the pre-existing baseline. NR-851 (displacement still
      hasn't moved — expected, deterrence is a later wave), NR-852 (treaty/colony mechanics lack
      direct harness assertions, verified by sweep + code read instead), NR-854 (subject_kind is a
      coastal/interior proxy, not the doc's literal two-seat model) flagged for Ben. Archived.

## Wave 4 — the phase's claims (need wave 3 to exist)

- [ ] **BL-941** (THE_ARMS_RACE_IS_DETERRENCE) — Ceiling/Alarm at polity grain (ERAS.md's shape),
      visible capability read by neighbours. This is the mechanism NR-851 flags as the likely fix
      for displacement's continued non-movement.
- [ ] **BL-942** (TWO_WAYS_TO_BE_STRONG) — consolidator/expansionist disposition derived from
      `zeal`/`dominion`/`sea_legs_q`, never a flag.
- [ ] **BL-936** (CULTURAL_GOOD_PREFERENCE) — preference attaches to culture, derived from what
      ground never held and route exposure, weights a want rather than setting a price.

## Wave 5 — not yet promoted

BL-943 (fleet/caravan exemplars — visual only, a filter on the throughput number), BL-938
(Industry ring-1 migration, NOT a trim — see NEXT_SESSION.md's "traps" section). `BL-944` gated on
a Ben decision (NEXT_SESSION.md § One decision owed — reassertion fired zero times in the earlier
16-world sweep; ask before building). `BL-945` parked for Digitisation, not this sprint.
