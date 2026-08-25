# Project Io — REFINED (active worklist)

**Sprint 19 — the world reads lived-in (opened 2026-08-25).** Wave 1 below is fanned out to
three worktree agents; wave 2 promotes after the wave-1 merge. Batch: `sprint-19-wave-1`.

## Wave 1 — Agent G (generation-dev, worktree), sequential within the agent

- [ ] **T1 (BL-610, centres from demography)** — `src/world/population_generation.cpp`:
  derive centre count and scale from the settlement regions' populations
  (`src/world/settlement.hpp`), retiring the land-area divisor and the weighted scale draw on
  the campaign path. Target density: every land province (~7–20 tiles) can host an anchor.
  Measure and report centres/land-tile, scale histogram, per-province coverage.
  - provides: demography-derived centre set; density measurement numbers
  - consumes: `settlement_state` regions (landed)
- [ ] **T2 (BL-612, urban ground stamped)** — same file: stamp `land_use_component` urban
  footprints under each centre, sized by scale tier; grandfather standing extraction
  (TILES.md § Urban transform).
  - provides: urban footprints at generation
  - consumes: T1's centre set
- [ ] **T3 (BL-611, province centre anchor)** — `src/world/province.cpp`: land partition seeds
  from centres only (hinterland seeding survives for water domains); province nation = anchor
  centre's nation (agrees with ruling 5 at generation). New partition-harness rows.
  - provides: anchored land partition; anchor→nation derivation
  - consumes: T1's centre density

## Wave 1 — Agent E (economy-dev, worktree), sequential within the agent

- [x] **T4 (BL-613, qualification fraction)** — *code-complete on the agent branch (20b86c67), save v10→11; NR-633* — `src/world/components.hpp` +
  `src/world/economy_system.cpp` + nation serialisation seam: `nation_component::qualification`,
  seeded at generation from region industrialisation timing aggregated per nation; recipes gain
  a `qualified_workforce` requirement (data, default 0; first-cut values on deep methods);
  a building whose recipe needs qualified workers throttles against the nation's qualified
  pool. Education-building *effect* lands behind `is_education_building(...)` — a marked
  integration point, wired at merge against Agent P's roster entries.
  - provides: `nation_component::qualification` (+ serialisation), `qualified_workforce`
    recipe field, qualified-pool throttle, `is_education_building` seam
  - consumes: settlement industrialisation-timing scalar (landed)
- [x] **T5 (BL-614, wage competition)** — *code-complete on the agent branch (dd283070), save v11→12; live-Lua parse and SDL callers unverified in-worktree — integrating build covers both* — `src/world/economy_system.cpp`: contended allocation
  by offered wage. First cut (NR-600 idiom, flagged for overturn): per-building
  `wage_bid` dial (default 0, data-only, no UI), offered wage = `base_wage × (1 + wage_bid)`,
  allocation sorted (offered wage desc, building id asc), wages paid at offered rate on
  effective labour.
  - provides: wage-cleared allocation; `wage_bid` field
  - consumes: T4's pool split (qualified vs ordinary)

## Wave 1 — Agent P (economy-dev, worktree)

- [x] **T6 (BL-615, stratum placement gates)** — *code-complete on the agent branch (9ea61218), 16/16 harness rows; merge + owed UI wiring pending (NR-631/NR-632)* — `src/world/placement_rules.cpp` + building
  data: per-building placement fields (`requires_centre`, `min_centre_scale`,
  `centre_proximity_radius`), reason-coded refusals; **schooling** and **university** roster
  entries (university `min_centre_scale = 4`); heavy processors get proximity values.
  - provides: placement gate fields + reason codes; schooling/university building types
  - consumes: nothing new (existing centres suffice for the harness)

## Wave 2 — promoted after the wave-1 merge

- BL-616 (centre promotion/decline) · BL-617 (population migration) · BL-618 (roads scale with
  qualification). BL-618 consumes T4's field; BL-617 consumes T4 + T5 + stance.

## Wave 3 — main session

Merge G → P → E (P before E so `is_education_building` wires against real types), verifier-review
barrier, integrating build, harness sweep, NR-630 measured retunes, Population-lens capture,
BL-615's live click, one commit per item.
