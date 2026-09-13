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

## Wave 4 — the phase's claims — LANDED 2026-09-12

- [x] **BL-941** (THE_ARMS_RACE_IS_DETERRENCE), **BL-942** (TWO_WAYS_TO_BE_STRONG), **BL-936**
      (CULTURAL_GOOD_PREFERENCE) — commit `7ebad1c9` (one commit, same interleaved-work pattern as
      wave 3). Deterrence moved the median displacement ratio 0.01 -> 0.88 across 3 seeds (one seed
      crossed 1.0) — real progress, not yet over the bar on the median (NR-855). Two-strategies
      does NOT show separation on this 3-seed sweep — 3/3 expansionist-leaning, 0/3 consolidator-
      leaning among top-3-by-region realms, NOT forced per the item's own instruction (NR-856).
      Cultural preference produces real numbers (52 entries, non-uniform weight 250-1000), was
      previously scaffolding. Independently rebuilt (full app BUILD_OK), `exploration_sim_harness`
      43/43 PASS, `world_determinism` ALL PASS, `history_sim_harness` at the pre-existing baseline.
      One agent stall recovered mid-wave (its uncommitted work was checked and was complete, not
      broken, before continuing). Archived.

## Wave 5 — the last four items

- [ ] **BL-943** (FLEET_AND_CARAVAN_EXEMPLARS) — visual only, a filter on BL-940's corridor number
      (now live). UI item: needs a live click in the built app, not just a capture.
- [ ] **BL-938** (INDUSTRY_TREE_RING_ONE_MIGRATES) — Industry's ring 1 duplicates the newly-authored
      Exploration tree; this is a migration (ring 1 out, rings 2-4 shift down), NOT a trim — ring 4
      is the 20th century and the 1960 epoch needs it. Doc-and-store work, `tree_lint.js` is the gate.
- [ ] **BL-944** (THE_SCHISM_VERB) — GATED on a Ben decision: reassertion fired zero times in an
      earlier 16-world sweep (the secession floor already removes badly-reached ground), so a
      schism verb built on the same substrate would also fire zero times. Ask before building —
      do not pick a fix (higher floor or a second binding term) silently.
- [ ] **BL-945** (DEPLETION_RETROFIT_FORMULA) — PARKED for Digitisation, not sprint 40 work.

BL-943 and BL-938 can run together (disjoint files: UI vs. tree stores). BL-944 blocks on Ben's
call in NEEDS_REVIEW (search "schism" / the wave-4-era entry in NEXT_SESSION.md § One decision
owed) before any code is written for it.
