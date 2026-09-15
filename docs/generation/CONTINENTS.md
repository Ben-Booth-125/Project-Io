# Project Io — Continents / Drift

> **Settles:** how plates are derived from Planetology's Engine result · how drift is
> simulated, and on what clock · how a plate's drift history sets the `height_bias` handed
> to the tile pipeline · how a rift basin becomes sea · what the pass retains for downstream
> code, `plate_id` included · what the Continent lens is asked to show.
> **Not here:** what the biased heightmap becomes (TILE_GENERATION) · how tiles are grouped
> into the unit of consequence (PROVINCES) · where the thermal budget came from
> (PLANETOLOGY).
> **Confused with:** TILE_GENERATION.md, PROVINCES.md, PLANETOLOGY.md.

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
  terrain. Presentation data — it never enters `world` — but it **is serialised**: the
  generation report is written whole by `src/core/save_game.cpp` (`w_continents` carries the
  plate set, `plate_id`, `height_bias` and both boundary masks), because a loaded campaign has
  no generation to consult — the Continent lens draws the plates from the saved report, and the
  Generation Ledger replays a body's tiles from the report's recorded pass inputs, which need
  the same `convergent` mask generation used. Two seams, then: a field added to
  `continent_state` is a `save_game_version` bump exactly as a field on the world is.
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

## The drift clock

Drift is *per epoch*, and the epoch is a **stated constant**: **5 My**, to a depth of
**20 epochs (100 My)**. Both numbers are design, not implementation detail, and the
statement is the point — a "per-epoch" drift vector with no epoch length defined
anywhere is unfalsifiable, and nothing can integrate it.

- **5 My per epoch.** One grid column on an Earth-sized body is roughly 150 km, and
  Earth-like plate motion covers that in about 3–6 My, so 5 My puts the clamped
  0.15–1.2 columns per epoch at a plausible rate rather than an arbitrary one.
- **20 epochs of depth.** Deep enough to reach a carboniferous-analogue coal window;
  shallow enough that extrapolating a single straight-line drift vector is still a
  defensible reconstruction. Past it, the plates' linear motion stops being one.

**Past configurations are DERIVED, never stored.** The plate set is five floats per
plate, and twenty per-tile plate rasters would be megabytes of save per body for
data that is a pure function of those floats — so the ordered sequence is
reconstructed on demand by winding the seeds back along their drift vectors. The
reconstruction re-runs the Voronoi assignment and **nothing else**: not the boundary
classification, not the rift-basin search. Those describe the *present* surface, and
the basin search is the expensive half of the pass, so re-running it per epoch would
cost more than the snapshot and mean less.

The contract that makes the whole axis usable: **epoch 0 reproduces the present
exactly** — same seeds, same comparison, same tie-break. If the reconstruction
disagreed with the present at zero offset, every deeper epoch would be fiction and
the shipped world would move the moment anything read it.

## The Lagrangian frame

`plate_id` answers "which plate seed is nearest this **fixed grid cell**". Under
drifting seeds that is a Voronoi partition reshuffling over stationary ground — the
boundaries move and the ground does not — so "this tile was at the equator in the
carboniferous" is not a question that partition can answer. Latitude is the same
problem in its sharpest form: a tile's climate band is derived from its **raster
row**, which is fixed for all time, so there is no representation in which a tile
*had* a different latitude. That is exactly what a coal-forming swamp at a tropical
palaeolatitude requires.

The frame that fixes it: **a tile is a material point on the plate it sits on
today.** A plate translates rigidly along its own drift vector, so a tile's offset
from its plate's seed is a *constant of the tile*, and its position at a past epoch
is simply its present position minus drift × epochs. That is the same winding
applied to the ground that the drift clock applies to the seeds, which is what makes
the two one model rather than two: a tile rides **one** plate at every epoch and
never appears to hop between them as the partition reshuffles underneath it.

**What a tile can be asked, at a given age:**

- **Where it was** — a past column (wrapping, as the surface does) and a past row.
  The row is deliberately **not clamped**: ground carried past a pole reports a row
  off the grid, which is the honest answer, and the answer says so rather than
  quietly pretending otherwise.
- **What latitude it sat at** — distance from the equator in [0, 1], **folded over
  the poles**, since latitude as a function of the row fraction is a triangle wave:
  ground carried past a pole comes back *down* in latitude on the far side. Folding
  rather than clamping is what keeps a deep epoch honest — clamped, every polar
  drifter would read as sitting exactly on the pole forever.
- **Which climate belt that was** — the same band boundaries Pass 3 uses, evaluated
  at the past latitude. One table, not a second copy of it.
- **What the moisture was** — the body's moisture field sampled at the **past**
  position. This is the Lagrangian premise stated in one line: ground moves *through*
  a climate rather than carrying one with it.

**A stagnant lid never moved.** Its single plate carries a drift vector that nothing
consumes — the pass never runs the Voronoi, never classifies a boundary, never
applies a bias — and the palaeo frame honours that: the ground of a stagnant body sat
where it sits, at every epoch. A world whose whole characterisation is that it has no
drift history is not given one.

**The frame has a raster form, and the generator reads through it.** The per-tile
question above, asked of every tile at one epoch, is a raster of latitude, band,
moisture and an on-grid flag — and that raster is what the tile pipeline consumes.
Pass 3's present band raster *is* the frame at epoch 0; the Life phase's coal and
petroleum rasters are the frame at the two fossil epochs
([TILE_GENERATION.md](TILE_GENERATION.md) § The Life phase). One implementation,
indexed by epoch: the present is a member of the family, not a separate lookup the
query had to be proved equal to. The raster form is built *by* the per-tile query,
tile for tile, so it cannot be a third answer.

### The boundary of the frame

The frame's safety is the **epoch-0 identity**, not a separation between the
generator and the query. At epoch 0 the frame returns the present exactly — same
position, same band, same moisture cell — so a generator reading the present through
it produces the world it produced before, bit for bit, and only the fossil epochs move
anything. Banding the *present* by a past row would change every world, and nothing
does; the present reads the frame at zero.

Four things it deliberately does **not** answer, each because answering it would be a
guess rather than a reconstruction:

- **Past height, cover or ocean.** Only position and climate are wound back. Terrain
  at a past epoch is a re-run of the pipeline in the past, not a lookup.
- **Longitude across a pole.** Ground that crosses a pole physically comes down the
  far side, half a wrap away. The latitude folds; the column does not.
- **A body-global palaeo-thermal term.** Over the 100 My the drift record spans, the
  radiogenic budget moves by well under a percent — latitude is the whole story at
  this depth, and a term that cannot change an answer is a term that only looks
  rigorous.
- **Boundary classification at a past epoch.** Convergent and divergent describe the
  present surface, for the reason the drift clock gives above.

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
