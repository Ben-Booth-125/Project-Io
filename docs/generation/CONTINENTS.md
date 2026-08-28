# Project Io — Continents / Drift

The plate-drift pass answers "where did the land end up, and why?" by simulating a
small number of drifting plates rather than reading noise. Code:
`src/world/continents.{hpp,cpp}` (`run_continents`). This document is the design
authority for the pass; the fuller S1–S4 continents simulation (collision/rift
history replacing the remaining noise machinery) belongs to BL-210 (oral-history
pivot), of which this pass is the first slice.

Position in the pipeline: **after Planetology's S3 Engine** (which decides
`mobile_lid` and the thermal budget) and **before the six-pass tile pipeline** (its
height bias feeds Pass 1). Invoked per body from `plan()` in `hard_coded_world.cpp`.

---

## Consequence, not dice

The governing rule (Ben, 2026-07-28): every value here is a **deterministic
consequence of Planetology's already-computed scalars**, not an independent random
branch.

- **Plate count** — derived from the Engine's thermal budget: a `mobile_lid` body
  gets `clamp(round(4 + theta × 3), 4, 10)` plates; a stagnant lid drifts as **one
  immobile plate** (all-zero bias, one biography line: *"Interior locked into a
  single stagnant plate."*).
- **Drift speed** — the same budget: `clamp(theta × 0.6, 0.15, 1.2)` grid units per
  epoch. More heat, faster convection, bigger boundary effects.
- **Drift direction** — a seeded pick from a fixed 8-way compass table, *not*
  sin/cos: literal constants are bit-identical everywhere, where a runtime trig call
  is not (`PLANETOLOGY.md` § Determinism names the transcendental hazard).
- Each plate is oceanic with probability 0.4; oceanic plates bias the height field
  down.

The RNG is the same splitmix64 shape as `planetology.cpp`'s, seeded from
`(body_seed, stage tag 0xC017)` — duplicated rather than shared, per the convention
that each generation file owns its stream.

## The pass, in order

1. **Plate seeding.** Seed positions drawn on the tile grid's own axes (columns
   wrap, per the cylinder convention).
2. **Voronoi assignment.** Every tile is assigned to its nearest plate
   (wrapped-column distance); base bias −0.10 on oceanic plates, +0.05 on
   continental ones.
3. **Boundary classification.** For each pair of plates sharing ≥ 5 boundary tiles,
   the relative drift resolves the pair as **convergent** (closing), **divergent**
   (opening), or transform-dominant (no net bias, no line).
4. **Boundary bias.** Tiles touching a classified pair get +0.12 (convergent
   uplift) or −0.08 (divergent rift) added on top of the plate base bias.
5. **Biography lines.** Each notable pair emits one dated `history_event` —
   collision (*"→ mountain range and arc magmatism; porphyry copper where it
   persists"*) or rift (*"→ rift basin and new coastline"*) — with a deterministic
   synthetic timestamp hashed from the pair, stable-sorted oldest-first
   (`std::stable_sort`, because two boundaries can hash to the same year and a
   plain sort would leave tied order unspecified — a determinism hazard).

## Rift-basin sea

The enclosed sea is BL-276 (Mediterranean sea). Ben's call (2026-08-03): a
Mediterranean-like enclosed sea should be **almost inevitable (~90% of worlds) but
never guaranteed** — the rare world where forming a Rome is hard is a feature, and it
must never be *impossible* to try. Two levers deliver this, both in this pass plus one
gate in `hard_coded_world.cpp`:

- **Rift basin (the mechanism).** After boundary classification, the divergent boundary
  between two **continental** plates with the longest *land-interior segment* (per-tile
  inland-ness ≥ 0.75 against plate ownership) founders: a distance-falloff depression
  (depth 0.65) with an uplifted **rift-shoulder rim** (+0.50 out to +5 tiles) that seals
  the flooded basin off from the world ocean. Width is **adaptive** — short rifts flood
  wide (a Black-Sea oval), long rifts stay narrow — targeting a roughly constant arena
  area. Pass 2's ocean threshold is a percentile, so the basin **relocates** ocean rather
  than adding any.
- **Sag-basin fallback.** A world with no continental divergent pair gets an
  **intracratonic sag basin** (the Caspian shape) at the inland-ness argmax of its
  continental plates — still a pure consequence of the plate layout, no draw.
- **One biography line** per basin (`chain_stage::engine`): rift ("A continental rift
  foundered below the waterline.") or sag ("The craton's interior sagged into a broad
  basin."), on a local RNG tag so no other draw shifts.
- **Acceptance gate (the backstop, `hard_coded_world.cpp`).** The homeworld's tile seed is
  attempt-folded (attempt 0 = the unfolded seed): three attempts to find an **arena**
  (enclosed sea ≥ 300 tiles), any of six for the **floor** (≥ 30 tiles); attempt 0 kept
  honestly on exhaustion — no clamping, the `resolve_preferences` idiom.

Measured (`tools/verify/mediterranean_sweep.cpp`, 500 campaign seeds): floor 100%, arena
**89.6%** — the ~1-in-10 without an arena are the deliberately-hard tail. The sweep
asserts wide regression bars (floor ≥ 97%, arena 82–96%) and mirrors the gate loop; keep
it in sync with `hard_coded_world.cpp` when either changes.

## Outputs and contracts

`continent_state` carries **five** consumer-facing outputs:

- **`height_bias`** — per-tile float, roughly [−1, 1], always sized `gw×gh`
  (all-zero on a stagnant lid). Contract into tile Pass 1: **added to the raw noise
  heightmap *before* normalisation** (`generate_body_tiles`'s `continent_bias`
  parameter), so plate uplift shapes the same heightmap noise would otherwise
  produce alone. A null pointer reproduces the unbiased noise surface bit-for-bit.
- **`plate_id`** — per-tile plate index, **retained** on the generation report
  (`generation_report::body_entry::continents`, `hard_coded_world.hpp`) rather than
  discarded. The boundary that raised a mountain range is invisible once the bias
  folds into the heightmap; keeping the assignment lets the lens draw the plates
  the bias was derived *from* instead of inferring landmasses back out of finished
  terrain. Presentation data — never enters `world`, stays off the serialisation
  seam.
- **`convergent`** — per-tile `uint8_t` mask, 1 where the tile touches a **classified
  convergent** boundary (the pairs that earned the +0.12 uplift). **Empty on a stagnant
  lid**, which has no boundaries at all. Written in the same loop that applies the bias.

  It exists because a classification that survives only as prose is lost to the
  terrain: `run_continents` knows which boundaries collided, says so in a history line,
  and folds the uplift into the heightmap — after which the boundary is indistinguishable
  from any other high ground. Without the mask Pass 5 could only seed mountains as "high
  and rocky" — blobs on existing high ground rather than chains along the boundary that
  raised them. Consumed as the first tier of `pick_seeds` (TILE_GENERATION.md § Pass 5).

  **Only the homeworld receives it** — `hard_coded_world.cpp` passes the homeworld's
  mask into `generate_body_tiles`; the other bodies fall through to the
  height-preference path.
- **`divergent`** — per-tile `uint8_t` mask, 1 where the tile touches a **classified
  divergent** boundary (the pairs that earned the −0.08 subsidence). **Empty on a
  stagnant lid**, same as `convergent`, and written in the same loop from the same
  `sign` test.

  It exists for the reason its sibling does. The pass classifies every boundary
  *both* ways, writes a history line for each, folds uplift or subsidence into the
  height field — and the classification is then unrecoverable from the terrain, because
  −0.08 in a heightmap is indistinguishable from ground that was simply low to begin
  with. A rift is a legible read on a body — a rift valley, thinned crust, and where the
  basins are — and a consumer wanting any of that has to be *told* which tiles the rift
  ran through.

  **The two masks are not disjoint, and neither is the complement of the other.** Most
  tiles are in neither. A tile is walked against two neighbours (right and down) and
  marked per neighbour, so a tile at a junction between a closing pair and an opening
  pair carries both marks — rare, real, and already true of `height_bias`, which
  accumulates both deltas on that same tile. What *is* exclusive is the per-boundary
  classification: one `sign` decides one pair, once.
- **`history`** — dated `history_event` lines tagged `chain_stage::engine`. The
  caller (`plan()` in `hard_coded_world.cpp`) moves them into the body's biography
  (`planetology_state::history`) and re-sorts; they are not stored twice. This is
  the textual half of the "graphical + textual" rule every oral-history stage must
  carry (Ben, 2026-07-28).

## Surface — the Continent lens

`overlay_mode::continent` on the Planetary canvas: per-plate tint with boundary
emphasis, read from the retained `plate_id` field; glyph `icons::continent` (two
interlocking plates split by a jagged seam); on-canvas key via `draw_continent_key`.
The lens is BL-226 (Continent lens); full catalogue in `docs/ui/LENSES.md`
§ Continent lens; visual check `scripts/verify/continents_terrain.lua`.

## Boundary seeding and relief

Mountain clusters seed along the classified convergent boundaries (the `convergent`
mask above). Rift clusters and volcanic activity are **not** boundary-seeded: Pass 5
places rift seeds by composition and latitude, and the volcanic overlay in Pass 4 reads
only `geological_activity`.

One measured consequence is worth keeping because it was counter-intuitive:
boundary-seeding *lowers* relief on its own, 17.5% → 9.9% of land. Convergent
boundaries often run along coastlines, and cluster growth is blocked by ocean, so a
range seeded there loses most of its rings to water. The Pass 5 seed counts are sized
against this placement rule (recovering ~14.0% of land) — the mechanism is right; the
seeding budget has to be tuned against it, not against the older height rule.
