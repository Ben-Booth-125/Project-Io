# Project Io — Continents / Drift

The plate-drift pass — the **first landed slice of BL-210 (the oral-history pivot)**,
shipped 2026-07-28. It answers "where did the land end up, and why?" by simulating a
small number of drifting plates rather than reading noise. Code:
`src/world/continents.{hpp,cpp}` (`run_continents`). This document is the design
authority for the pass; `continents.hpp` still names `GENERATION_STRATEGY.md` as its
authority — this doc supersedes that pointer.

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

## Rift-basin sea (BL-276, landed 2026-08-03)

Ben's call (2026-08-03): a Mediterranean-like enclosed sea should be **almost inevitable
(~90% of worlds) but never guaranteed** — the rare world where forming a Rome is hard is a
feature, and it must never be *impossible* to try. Two levers deliver this, both in this
pass plus one gate in `hard_coded_world.cpp`:

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
  basin."), on a local RNG tag so no existing draw shifts.
- **Acceptance gate (the backstop, `hard_coded_world.cpp`).** Kepler's tile seed is
  attempt-folded (attempt 0 = the unfolded seed): three attempts to find an **arena**
  (enclosed sea ≥ 300 tiles), any of six for the **floor** (≥ 30 tiles); attempt 0 kept
  honestly on exhaustion — no clamping, the `resolve_preferences` idiom.

Measured (`tools/verify/mediterranean_sweep.cpp`, 500 campaign seeds): floor 100%, arena
**89.6%** — the ~1-in-10 without an arena are the deliberately-hard tail. The sweep
asserts wide regression bars (floor ≥ 97%, arena 82–96%) and mirrors the gate loop; keep
it in sync with `hard_coded_world.cpp` when either changes.

## Outputs and contracts

`continent_state` carries **four** consumer-facing outputs *(this read "three things" until
2026-08-04, when `convergent` was added)*:

- **`height_bias`** — per-tile float, roughly [−1, 1], always sized `gw×gh`
  (all-zero on a stagnant lid). Contract into tile Pass 1: **added to the raw noise
  heightmap *before* normalisation** (`generate_body_tiles`'s `continent_bias`
  parameter), so plate uplift shapes the same heightmap noise would otherwise
  produce alone. A null pointer reproduces the pre-BL-210 surface bit-for-bit.
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

  It exists because the classification used to survive only as prose: `run_continents` knew
  which boundaries collided, said so in a history line, folded the uplift into the heightmap
  and then forgot. Pass 5 could not seed mountains where mountains actually form, so it fell
  back to "high and rocky" — blobs on existing high ground rather than chains along the
  boundary that raised them. Consumed as the first tier of `pick_seeds`
  (TILE_GENERATION.md § Pass 5).

  **Only Kepler receives it today** — `hard_coded_world.cpp:146` passes `&kepler_convergent`
  and the other bodies fall through to the older height-preference path.
- **`history`** — dated `history_event` lines tagged `chain_stage::engine`. The
  caller (`plan()` in `hard_coded_world.cpp`) moves them into the body's biography
  (`planetology_state::history`) and re-sorts; they are not stored twice. This is
  the textual half of the "graphical + textual" rule every oral-history stage must
  carry (Ben, 2026-07-28).

## Surface — the Continent lens (BL-226, built 2026-07-30)

`overlay_mode::continent` on the Planetary canvas: per-plate tint with boundary
emphasis, read from the retained `plate_id` field; glyph `icons::continent` (two
interlocking plates split by a jagged seam); on-canvas key via `draw_continent_key`.
Full catalogue in `docs/ui/LENSES.md` § Continent lens; visual check
`scripts/verify/continents_terrain.lua`.

## Open follow-ons

- **BL-210 (oral-history pivot, design-owed)** — this pass is its first slice only;
  the fuller S1–S4 continents simulation (collision/rift history replacing the
  remaining noise machinery) stays with the parent item.
- ~~Seeding tile Pass 5's mountain/rift clusters along the classified boundaries~~
  **Mountains landed 2026-08-04** (`71e8a9b`) via the `convergent` mask above.
  **Rifts are still deferred**, as is concentrating volcanic activity on boundaries.

  Worth recording because it was counter-intuitive: boundary-seeding *lowered* relief
  on its own, 17.5% → 9.9% of land. Convergent boundaries often run along coastlines,
  and cluster growth is blocked by ocean, so a range seeded there loses most of its
  rings to water. Raising the seed counts recovered it to ~14.0%. The mechanism was
  right; the seeding budget had been tuned against a different placement rule.
