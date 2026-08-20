# Project Io — REFINED (active worklist)

## Sprints A/B/C/D — the four-lane parallel batch (promoted 2026-08-20)

Requirements: requirements.json § `province-partition`, § `battle-state-in-world`,
§ `road-network-cuts`, § `settlement-density-clamp`, § `tariff-and-money-conservation`,
§ `era-minus1-conquest-assertion`.

Follows the watch + meta batch (`18849f9`). Four lanes chosen for file disjointness: Lane A is
harness-only, Lane B is generation-only, Lane C is `src/world` military, Lane D is `src/world`
money. Only C and D share a file (`economy_system.cpp`) — worktree-isolated, C merges first.

**Main session owns** the board files (`backlog.json`, `sprints.json`, `SPRINTS.md`, `ROADMAP.md`,
`NEEDS_REVIEW.json`, `requirements.json`, this file) plus `question_log.json` at merge — **no agent
touches them**.

### Lane M — the owed live check (main session, runs first)

- **[1] M1 — GUI verification pass for the six landed-awaiting items.** BL-412 (live agent
  seam), BL-408 (spectator god view), BL-411 (strategy readout), BL-480 (law author — the
  read-only levy line), BL-429 (ancient building roster), BL-453 (convoy ledger). Build, open the
  app, press the thing, look. Per the standing rule a scripted capture does not prove a press is
  reachable. Flips six items complete, or files what is actually broken. No code expected.

### Lane A — Sprint 27: the run's failure becomes falsifiable

- **[2] A1 — BL-384 (Era −1 sim conquers nothing), ASSERTION HALF ONLY.** Sub-agent, worktree.
  Files: `tools/verify/history_sim_harness.cpp`, `tools/verify/history_sweep.cpp` (read-only on
  `src/world/history_sim.*`). Add a conquest-count assertion plus elimination and dominance-share
  counters, reusing `history_sweep.cpp`'s `hegemony_threshold_q` shape. **Expected outcome is a
  RED assertion committed against today's build** — the fix is explicitly out of scope, no constant
  tuned, no scorer term changed. If it unexpectedly PASSES, that is a major finding to report, not
  a seed set to adjust (Sprint 27 § risk).

### Lane B — Sprints B2 + B3: the world looks physically civilised

- **[3] B1 — Sprint B2, the three road-network cuts.** Sub-agent, worktree.
  Files: `src/world/road_generation.cpp` (+ its header). The `<2 centres` early return at :90;
  water crossing and cross-water adjacency at :65 and :195. BL-188 (coastal ports) is **out** of
  this task — sea being priced backwards is its own item and its own merge.
- **[2] B2 — Sprint B3, the density clamp.** Sub-agent, worktree.
  Files: `src/world/population_generation.cpp`, `src/world/settlement.cpp`,
  `tools/verify/substrate_census.cpp`. `clamp(tiles/1000, 20, 40)` is a 180×84 constant against a
  312×145 map. Lands **BL-463 (settlement count is seed-invariant)** with it — same file, same
  census harness, and a re-tuned clamp that is still seed-invariant is a fixed clamp twice.
  BL-374 (corp density) and the corporation-KIND axis are **out** — BL-374 is `design-owed`.

### Lane C — Sprint C3: the engagement envelope, then the fight

- **[4] C1 — BL-466 (province partition).** Sub-agent, worktree. **Foundation — blocks C2.**
  Files: new `src/world/province.{hpp,cpp}`, `src/world/world.hpp`, `src/world/serialisation.cpp`,
  harness. A deterministic tile→province partition with a stable sorted id order. Serialisation
  seam: one appender, no reordering of an existing record.
- **[4] C2 — BL-467 (battle state in world) + BL-315's remainder.** Sub-agent, worktree,
  **after C1 merges.** Files: `src/world/campaign_battle.{hpp,cpp}`, `src/world/world.hpp`,
  `src/world/corp_command.{hpp,cpp}`, `src/world/economy_system.cpp`, `docs/military/MILITARY.md`.
  The world-held battle record {province, attacker, defender, unit ids, rounds_fought, trace};
  discovery each tick in sorted (province id, corp-pair) order; the trigger on directed hostility
  (BL-448 is complete, so the predicate exists); losses applied to `unit_component`. Both resolvers
  currently return results nothing reads — this gives `unit_to_stack_entry` its production caller.
  The upkeep-rates-off-zero rider is **out** (BL-458's own item).

### Lane D — Sprint D4: money moves between two named actors

- **[4] D1 — BL-392 (procurement contracts destroy value) then the tariff.** Sub-agent, worktree.
  Files: `src/world/procurement.cpp`, `src/world/budget_system.cpp`,
  `src/world/economy_system.cpp`, `src/world/market.{hpp,cpp}`, `src/world/law.{hpp,cpp}`, harness.
  **Ordering is not optional** — per D4's adversarial finding 9, procurement minting goods with no
  supplier and destroying money must be fixed *before* anything else touches procurement. Then:
  a nation on `market_component` (the `centre_tile` → `tile_to_nation` hook already resolves and is
  unused), a tariff rate set by the enacting nation's own law, and a cross-border sale that debits
  the buyer and **credits the enacting nation's treasury** (landed with BL-480). The binding
  requirement is conservation: no flow in this lane may mint or destroy a credit.

### Deferred this batch, with reasons

- **Sprint D2 (research becomes a currency).** BL-478 (ancient research spend) is `design-owed` —
  the debit mechanism does not exist as a design, and NR-315 records that `condition_subject::science`
  is a *level*, picked by reading rather than by choice. Wants a design pass, not an implementer.
- **BL-188 (coastal ports), BL-374 (corp density), the corporation-KIND axis.** Riders on B2/B3
  that are separately-scoped items; two of the three are `design-owed`.
- **BL-443 (debt floor).** Still gated on Ben's NR-296 lever pick, and now collides with Lane D on
  `budget_system.cpp`. Unchanged from the previous batch's MC.
