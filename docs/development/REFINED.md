# Project Io — REFINED (active worklist)

## Sprint P1 — the province redesign (promoted 2026-08-21)

Requirements: requirements.json § `retain-heightmap`, § `organic-province-borders`.

Sprint P1 in `sprints.json` carries the arc and the method note. This is the
remaining half: the partition gets rebuilt a third time, this time as shapes grown
from settlement and stopped by terrain rather than a lattice packed over the map.

**Main session owns** the board files and `question_log.json` — no agent touches them.

### [2] H1 — BL-517, retain the generation heightmap. **IN FLIGHT.**

Files: `src/world/components.hpp`, `src/world/tile_generation.cpp`, the tile
serialisation seam, harness. Height is computed in Pass 1 and discarded; BL-515
needs it as a boundary signal, because `terrain_landform` is seven classes whose
numeric order means nothing (`mountain = 2` and `canyon = 3` are not one step apart)
and a watershed is a shape, not a class.

Narrow by design: capture the value, append it to the tile record, expose it to the
Generation Ledger overlay. Do not re-derive landform from it, do not change a
terrain rule, and do not perturb an RNG stream — if capturing it changes draw order
that is a defect, not a cost.

### [5] O1 — BL-515, organic province borders. **BLOCKED on H1.**

Files: `src/world/province.{hpp,cpp}`, `tools/verify/province_partition_harness.cpp`,
`tools/verify/province_capacity_probe.cpp`, `docs/generation/TILE_GENERATION.md`.

Grow from population centres (seed strength scaling with centre scale 1–5), by
cost-weighted flood fill where a **river edge** is expensive (`river_edges` is
already a per-side bitmask — the right shape for a border), an **elevation
difference** is expensive in proportion to gradient, and a **road link is cheap,
because a road binds**. Hinterland provinces for country no centre reaches, seeded
from the least-accessible tile so they are shaped by terrain rather than left over.

- **Identity: the lowest-id member tile.** Derived, so determinism is free and
  nothing new is serialised. Safe only because borders move during generation ONLY.
- **Size: 7–12 soft, 3–12 hard, and tiny provinces are KEPT, not merged away.**
- **The land-only invariant is NARROWED, never deleted** (NR-428) — ocean is a
  special case and BL-516 owns it.
- The partition walk stays explicitly sorted: `world::bodies` is an `unordered_map`
  and iterating it unsorted already fed province ids in container order once.

Report the size distribution against the 3×3 partition it replaces — 21,161
provinces, mean 9.11, 97.90% in the 7–12 band — as a comparison, not a target. The
organic partition will not match that spread and is not meant to.

### Expected breakage, so it is not mistaken for regression

- `province_partition_harness`'s land-only and block-derived rows break **by design**.
- **Every province id in the world moves.** Anything holding one across the change
  is stale — including `province_capacity_probe`'s reported ids and any fixture that
  names a province by id rather than by a member tile.
- `unit_march_harness`'s grid fixtures assume provinces carved from 3×3 blocks
  (ids 40/48/72/80 on a 9×9 body). They will need re-anchoring, and the last time
  this happened it also surfaced a segfault that had hidden two whole sections.

### Held, with reasons

- **BL-516** (water kinds + sea provinces) — wants BL-515 landed; land-only first.
- **BL-518** (war redraws borders) — no point redrawing borders about to be replaced.
- **BL-514** (blend all tiles) — held at Ben's instruction until he sees the organic
  borders drawn.
- **BL-512** (firm cap tunables) — the per-province cap is inert by orders of
  magnitude (NR-421); pin it against whatever BL-515 produces, not before.
