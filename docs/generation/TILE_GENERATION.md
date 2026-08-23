# Project Io — Tile Generation

This document specifies the strategy and rules for procedural tile generation in
`hard_coded_world.cpp`. Generation is **deterministic**: every body has a fixed
seed and a solar-parameter profile — **derived by the Planetology chain**
(`PLANETOLOGY.md`), not hand-authored — producing the same world on every run.
The same six-pass pipeline runs for every body; body character comes from the
parameters fed into it, not from body-specific code paths.

**Pipeline-shape convention (BL-051, sibling-pass architecture — settled 2026-07-21).**
The six-pass core below stays fixed. Every generation extension — rivers (BL-170), the
full-set deposit rarity scalar (BL-040), coastline/band smoothing, and body-level
Planetology (`PLANETOLOGY.md`, BL-167) — lands as its own **sibling pass**: a separate
file/function invoked around `generate_body_tiles`, reading the shared
`generation_record` rather than growing the six-pass function itself. Planetology in
particular runs *before* this pipeline (its atmosphere output feeds this pipeline's
solar-parameter input) — see `GENERATION_STRATEGY.md`.

---

## Design principles

**Solar parameters drive body character.** Each body is described by a small set
of constants — temperature class, atmospheric class, hydrological state,
geological activity — expressing what kind of world it is. The generation passes
read these constants; they contain no body-specific branches.

**Fixed seed, derived constants.** The RNG is seeded deterministically per body.
Each `body_profile` is the **return value of the Planetology pass** (`run_planetology`,
called from `plan()` in `hard_coded_world.cpp` — see `PLANETOLOGY.md`). What is
authored is the body's physical *inputs* (`prototype_body()` in `planetology.cpp`);
the chain derives the profile. The result is a stable world identical across every
run.

**Same pipeline, all bodies.** Every body — planet, moon, asteroid — runs all six
passes. Passes that produce no output for a given body type (e.g. ocean placement
on an airless body) return immediately without side effects.

**Approximate geology.** Composition tables and deposit weights follow
`docs/economy/TILES.md` as a guide. Exact deposit amounts and secondary modifiers
are approximated; strict physical realism is not a goal.

---

## Solar parameters

Each body carries a set of solar-level constants (`body_profile`,
`src/world/tile_generation.hpp`), **derived from physical inputs by the
Planetology chain** (`PLANETOLOGY.md`).

| Parameter | Type | Values |
|---|---|---|
| `temperature_class` | enum | `scorching`, `hot`, `temperate`, `cold`, `frozen` |
| `atmosphere_class` | enum | `none`, `thin`, `moderate`, `thick` |
| `hydrological_state` | enum | `none`, `polar_frozen`, `liquid` |
| `geological_activity` | enum | `none`, `low`, `moderate`, `high` |
| `water_fraction` | float 0–1 | Target ocean coverage; only used when `hydrological_state == liquid` |
| `composition_bias` | enum | `standard`, `metallic` | Override for bodies where surface composition is dominated by a single type |

**temperature_class** shifts the latitude band widths. A scorching body has no
polar band; a frozen body has no tropical band.

**atmosphere_class** gates organic compositions. `none` or `thin` suppresses
grassland, forest, and wetland — no liquid water cycle means no biology.

**hydrological_state** controls Pass 2 (ocean / ice placement). `liquid` runs the
noise-thresholded ocean pass. `polar_frozen` skips ocean generation and marks
polar rows as icy in Pass 4. `none` produces no water tiles at all; low-elevation
terrain from the heightmap becomes valley or canyon landforms in Pass 5.

**geological_activity** scales volcanic composition probability (Pass 4) and the
number of mountain range and rift zone cluster seeds (Pass 5).

---

## Prototype body profiles

**Derived by Planetology; the table is the regression baseline, not the source.**
Each profile below is what `run_planetology` derives from the body's authored
physical inputs, and the planetology harness asserts the derivation lands here.

> **The names in this table are slot labels, not in-game names (BL-257, body naming).**
> Body names are generated per seed — see `PLANETOLOGY.md` § Body naming. "Cinder" here
> means "the hot inner planet", the prototype slot; the world the player sees calls it
> something coined. Nothing in code may test a body's name to decide which body it is.

| Body | `temperature_class` | `atmosphere_class` | `hydrological_state` | `geological_activity` | `water_fraction` | `composition_bias` |
|---|---|---|---|---|---|---|
| Cinder | `scorching` | `none` | `none` | `low` | 0.0 | `standard` |
| Kepler | `temperate` | `thick` | `liquid` | `moderate` | 0.60 | `standard` |
| Selene | `cold` | `none` | `polar_frozen` | `none` | 0.0 | `standard` |
| Pallas | `cold` | `none` | `none` | `none` | 0.0 | `metallic` |

**Cinder** — hot inner planet (Mercury analogue). No liquid water; high volcanic
and barren coverage. Its `geological_activity` derives `low` — Mercury is
genuinely geologically dead — which costs Cinder mountain and rift cluster seeds.

**Kepler** — temperate home planet (Earth analogue). Full climate gradient from
polar ice to tropical scrub; 60% ocean; grassland and forest belts; moderate
mountain chains.

**Selene** — Kepler's moon (Luna analogue). Airless regolith surface; icy at
polar rows only; crater-dominated landforms; no habitable compositions.

**Pallas** — metallic asteroid. Mostly metallic surface with rocky minority;
scattered craters; no organic matter; no water.

---

## Generation pipeline

Each body runs the same six passes in sequence. All random draws use the body's
seed with a prime offset per pass, so pass order does not affect other passes'
results.

---

### Pass 1 — Heightmap

Generate a simplex noise heightmap `H[col][row]` over the full grid, normalised
to `[0.0, 1.0]`.

**Continent height bias.** Before normalisation, the per-tile `height_bias` from
the Continents/Drift pass (`run_continents`, `src/world/continents.cpp` — see
`CONTINENTS.md`) is added into the raw noise field. Plate-boundary uplift and rift
subsidence therefore shape the *same* heightmap the noise would otherwise produce
alone — one terrain source, not two competing ones. A null bias reproduces the
unbiased noise surface bit-for-bit (`continent_bias` parameter of
`generate_body_tiles`).

This single heightmap is shared across all subsequent passes:

- **Ocean threshold** (Pass 2) — tiles below a water-fraction-derived threshold
  become ocean.
- **Landform base** (Pass 5) — high-`H` tiles are mountain/highland candidates;
  low-`H` non-ocean tiles are valley candidates; the middle band defaults to plains.

The same heightmap on an airless body still generates and drives landform
assignment — a crater-heavy moon still has elevation structure.

The normalised heightmap is **retained** on `tile_component::height` and serialised,
because the province partition reads it (§ Province partition); every other
intermediate is disposable (`GENERATION_LEDGER.md` § Data lifetime).

---

### Pass 2 — Ocean placement

*Skipped if `hydrological_state != liquid`.*

Apply a **latitude bias** to the heightmap before thresholding:

```
H'[col][row] = H[col][row] − latitude_bias(row, gh)
```

`latitude_bias` peaks at the equatorial rows and falls off toward the poles
(`bias_amp × (1 − lat²)`, with `bias_amp = 0.05` — a larger amplitude drowns most
tropical land), increasing ocean probability near the equator without enforcing a
uniform band. The noise in `H` breaks the coastline into an irregular noise-banded
shape rather than a smooth oval.

The threshold is set at the percentile of `H'` that matches `water_fraction`. All
tiles below threshold receive `ocean` composition. Tiles above threshold proceed
to Pass 4 for land composition.

*If `hydrological_state == polar_frozen`*: no ocean placement. Polar-latitude
tiles receive `icy` composition in Pass 4.

*If `hydrological_state == none`*: no water tiles at all. Low-elevation areas
from `H` remain land and receive valley or canyon landforms in Pass 5.

---

### Pass 3 — Latitude band assignment

Divide the grid rows into named temperature bands. Band boundaries are shifted by
`temperature_class`:

| Band | Temperate row % | Scorching row % | Cold row % |
|---|---|---|---|
| Polar | 0–10, 90–100 | — | 0–15, 85–100 |
| Subpolar | 10–22, 78–90 | — | 15–35, 65–85 |
| Temperate | 22–42, 58–78 | 0–20, 80–100 | 35–65 |
| Subtropical | 42–47, 53–58 | 20–40, 60–80 | — |
| Tropical | 47–53 | 40–60 | — |

The cold column's polar band is the outer 30% of rows, not the outer 50%: a
`polar_frozen` body with half its rows polar reads half-icy (see the comment in
`band_for_row`, `tile_generation.cpp`; Selene's `polar_frozen` + `cold` is
load-bearing there).

A scorching body collapses to temperate/subtropical/tropical only. A cold body
collapses to polar/subpolar/temperate only. A frozen body is all polar.

Also compute a **moisture value** `M[col][row]` from a second simplex noise pass
at a different seed offset. Moisture is independent of latitude, normalised to
`[0.0, 1.0]`. It acts as the second axis of the composition lookup in Pass 4 and
ensures band boundaries are irregular rather than sharp horizontal lines.

---

### Pass 4 — Biome, then the axes it decomposes into

What this pass produces is not one enum but three axes — `terrain_substrate` ×
`terrain_cover` (+ `cover_density`) × the `terrain_landform` Pass 5 sets. See
`docs/economy/TILES.md` § Three-axis terrain model; the split is BL-519 (three-axis
terrain).

- **4a — biome.** The `(temperature_band, moisture_value)` table below returns an
  internal `biome` — a twelve-valued vocabulary that is not itself an axis, only the
  table's row.
- **4b — drainage** (BL-338, drainage). Operates on the biome.
- **4c — decompose.** `decompose_biome` maps each biome to
  (substrate, cover, density). **Consumes no RNG stream** — density varies through
  a stateless fold.
- **4d — cover refinement.** Dresses ground the biome table left **bare** (a forest
  on a wet rocky upland, snow on cold high ground, dunes on dry barren) and never
  rewrites a substrate. Also no stream.
- **4e — water kinds** (BL-516, water provinces). Classifies water structurally into
  lake / coast / open ocean by flood fill. Also no stream, and it writes a separate
  reported substrate so Passes 5 and 6 still see the coarse `ocean` they are written
  against.

That structure is what makes the pass auditable: only 4a draws, so every downstream
pass sees the same draws whatever the cover axis does, and anything that moves is
attributable to the decomposition rather than to stream drift. The 120-seed
`earthlike_tile_census` harness is the instrument that pins the biome draws.

Each non-ocean land tile receives a biome by looking up
`(temperature_band, moisture_value)` in the table below.

**Bodies with `atmosphere_class == none` or `thin`** skip habitable biomes
(grassland, forest, wetland). The moisture axis still applies to choose among the
available inorganic types.

**Bodies with `composition_bias == metallic`** override this table: most tiles
receive `metallic`, minority `rocky` and `regolith`. See the airless table below.

#### Atmosphere-present composition table

| Effective band | Low moisture (0–0.35) | Mid moisture (0.35–0.55) | High moisture (0.55–1.0) |
|---|---|---|---|
| Polar | Icy | Icy | Icy |
| Subpolar | Rocky | Rocky / Tundra | Tundra |
| Temperate | Barren / Rocky | Rocky / Grassland | Grassland / Forest |
| Subtropical | Barren | Barren / Grassland | Grassland / Wetland |
| Tropical | Barren | Barren | Forest / Wetland |

Where two compositions are listed, the split is weighted approximately 60/40 and
resolved by a draw against `M[col][row]`. The high-moisture cutoff sits at 0.55
(`moisture_column`) so that enough tiles reach the wet branches that produce forest
and wetland.

#### Abiotic fallback table

A body that **held an atmosphere but whose biosphere never reached land**
(`life_stage < land`) cannot carry grassland, forest, wetland or tundra. Pass 4
routes such bodies to `composition_abiotic()` — a second (band × moisture) table
in which each cell falls back to its **own inorganic member** (rocky in the cool
bands, barren in the warm ones), deliberately *not* a blanket substitution table:
tundra's abiotic partner is rocky, not icy, so "replace tundra with icy" would
repaint every subpolar band.

| Effective band | Low moisture (0–0.35) | Mid moisture (0.35–0.55) | High moisture (0.55–1.0) |
|---|---|---|---|
| Polar | Icy | Icy | Icy |
| Subpolar | Rocky | Rocky | Rocky |
| Temperate | Barren / Rocky | Rocky / Barren | Rocky / Barren |
| Subtropical | Barren | Barren / Rocky | Barren / Rocky |
| Tropical | Barren | Barren | Barren / Rocky |

The abiotic branch mirrors `composition_atmospheric()`'s RNG consumption
draw-for-draw, so the two branches stay stream-aligned and switching between them
cannot shift any downstream pass. It only bites on a world that *has* an atmosphere
yet never reached land; in the prototype body set the homeworld is the only
atmospheric body and always lives (`PLANETOLOGY.md` § Verification), so the branch
is exercised by a synthetic harness case.

**Volcanic overlay** — `geological_activity` injects volcanic composition over the
subtropical and tropical bands before the main lookup. Activity levels:

| `geological_activity` | Volcanic injection probability (subtropical/tropical) |
|---|---|
| `high` | 35–40% |
| `moderate` | 10–15% |
| `low` | 3–5% |
| `none` | 0% |

#### Airless body composition tables

| Body type | Composition distribution |
|---|---|
| Airless rocky moon (Selene) | Regolith 65%, Rocky 30%, Icy at polar rows |
| Metallic asteroid (Pallas) | Metallic 55%, Rocky 30%, Regolith 15% |
| Airless scorching planet (Cinder) | Volcanic 45%, Barren 40%, Rocky 15% |

Polar rows for `polar_frozen` bodies (Selene) always receive `icy` regardless of
the table, overriding any other assignment.

---

### Pass 5 — Landform clusters

After composition is settled, run a seeded cluster pass to place non-plains
landforms. Tiles not claimed by a cluster default to `plains`. Tiles where
`H[col][row] < 0.35` and not claimed by any cluster receive `valley`.

**Seed counts** are derived from `geological_activity` and body type (`seed_count`,
`tile_generation.cpp`). Crater field seeds are elevated for airless bodies
(`atmosphere_class == none`) regardless of geological activity.

| Feature | `none` | `low` | `moderate` | `high` |
|---|---|---|---|---|
| Mountain range seeds | 0 | 5 | 11 | 13 |
| Rift zone seeds | 0 | 0 | 1 | 3 |
| Crater field seeds (atmospheric) | 0 | 1 | 2 | 1 |
| Crater field seeds (airless bonus) | +4 | +3 | +3 | +2 |

**Mountain range seed placement — a three-tier pool, boundary-first.** `pick_seeds` tries,
in order:

1. **Tiles on a classified convergent plate boundary** — where mountains actually form.
   This tier exists only for mountains, only when the body was given a `convergent` mask
   (CONTINENTS.md § Outputs), and it is tried *before* the height rule.
2. **High and rocky** — `H > 0.65` and composition rocky or barren.
3. **Anywhere on land** — so a body with no classified boundaries still gets ranges.

BFS expansion uses `shape_of(mountain) = {5 rings, 0.72 decay}`. The frontier transitions:

```
core tile  → mountain
ring 1     → mountain 55% / highland 45%
ring 2     → mountain 25% / highland 75%
ring 3+    → highland 65% / plains 35%
```

**Why the seed counts and cluster shape are sized as they are.** Boundary-seeding
*lowers* relief on its own — 17.5% → 9.9% of land — because convergent boundaries often
run along coastlines and `grow_cluster` is blocked by ocean, so a range seeded there loses
most of its rings to water. The seed counts above recover relief to ~14.0% of land,
against Earth's ~24%. Clusters are grown large rather than seeded more thickly because
Earth's relief is a few long chains, not many small blobs — and the remaining shortfall is
range **count**, not range size: pushing `max_ring` further produces blobs rather than
chains.

**Rift zone seed placement** — seeds prefer volcanic or barren tiles at subtropical
or tropical latitude. Growth uses `shape_of(rift) = {3 rings, 0.60 decay}` and is
weakly directional — the two purely vertical neighbours are damped ×0.5, biasing
growth east–west to produce linear features. Frontier transitions:

```
core tile → rift
first ring → canyon (probability 0.6) or rift (probability 0.4)
second ring → canyon (probability 0.3) or plains
```

**Crater field seed placement** — seeds can land on any composition; preferred on
regolith and barren for airless bodies. Growth radius is tight
(`shape_of(crater) = {2 rings, 0.55 decay}`) so craters read as impact features
rather than single-tile stamps:

```
centre tile → crater
first ring → crater (probability 0.5) or highland (probability 0.5)
beyond → plains
```

Mountain ranges steer toward high-`H` tiles during BFS expansion; craters
distribute more uniformly.

---

### Pass 6 — Deposit generation

Each tile receives resource deposits based on `(composition, landform)` following
the approximate profiles in `docs/economy/TILES.md`. Amounts are randomised in a
per-tile draw seeded from the body seed plus tile index, ensuring the same body
always produces the same deposits.

**Ambient resources** are always generated on eligible compositions at a low fixed
baseline before the main deposit draw. This guarantees every tile has at least one
extractable resource.

| Ambient resource | Eligible compositions | Base deposit |
|---|---|---|
| Stone | All non-ocean, non-icy | 10–30 |
| Timber | Forest, Wetland | 15–40 |
| Sand | Barren (plains/canyon landform) | 10–25 |
| Clay | Wetland, any valley landform | 8–20 |
| Peat | Tundra (plains/valley landform) | 5–15 |

**Calibrated subset deposit table** — the seven-resource subset, authored on the
per-tile `tile_rng` stream. These values are hand-calibrated and the economy is
tuned on them; the full-set pass below leaves them bit-for-bit unchanged.

| Composition | Resource | Base range | Mountain mod | Rift mod | Valley mod |
|---|---|---|---|---|---|
| Barren | Iron ore | 0–150 | ×1.4 | ×1.2 | — |
| Barren | Petroleum | 0–120 | — | — | ×1.2 |
| Rocky | Iron ore | 0–200 | ×1.5 | — | — |
| Volcanic | Iron ore | 0–150 | — | ×1.3 | — |
| Icy | Water | 0–400 | — | — | — |
| Grassland | Agricultural produce | 40–180 | — | — | ×1.3 |
| Forest | Agricultural produce | 10–80 | — | — | ×1.15 |
| Wetland | Agricultural produce | 40–200 | — | — | — |
| Tundra | Iron ore | 0–60 | ×1.3 | — | — |
| Metallic | Iron ore | 50–250 | — | — | — |
| Metallic | Regolith | 20–50 | — | — | — |
| Regolith | Regolith | 20–50 | — | — | — |

Modifiers apply multiplicatively to the upper bound of the base range. (The
metallic row also authors regolith 20–50, same as the regolith composition — a
metallic surface still carries impact debris; see `generate_deposits`.)

**Full raw-set additions (BL-040, deposit rarity)** — the remaining Tier 1 raw
resources, authored on an **independent per-tile rng stream** (`rare_rng`) so they
cannot perturb the calibrated subset above or the derived environment. Each is gated
by its seeded rarity scalar (§ deposit rarity in [RESOURCES.md](../economy/RESOURCES.md)):
the scalar both gates presence (frequency) and scales the rolled magnitude, so rare
goods are sparse *and* small. Base ranges below are pre-scalar.

| Composition | Resource | Base range (pre-scalar) |
|---|---|---|
| Barren | Coal | 30–140 |
| Barren | Silica | 20–90 |
| Rocky | Silica | 20–100 |
| Rocky | Copper ore | 30–160 |
| Rocky | Rare earth ore | 10–70 |
| Volcanic | Copper ore | 30–180 |
| Volcanic | Rare earth ore | 20–100 |
| Metallic | Iron-nickel ore | 60–260 |
| Metallic | Platinum group metals | 20–120 |

### Post-multiplies and endemic additions

After the deposit array is filled, **three** pure post-multiplies run in sequence.
None draws RNG, so each reproduces the unscaled surface bit-for-bit at its
identity value:

1. **Abundance scalar (BL-114, world descriptor).** `deposit_scalar` — sparse
   0.40 / lean 0.65 / standard 1.00 (`GENERATION_STRATEGY.md` § The world
   descriptor). Applied before the finite-reserve seeding so both scale together.
2. **Planetology endowment (BL-167, planetology).** The body's per-resource
   `endowment[r]`. A channel at 0.0 removes the resource outright rather than
   thinning it — "no life, no coal" lands here. A null planetology state skips
   the multiply entirely.
3. **Ore-province field.** `provinces_for` seeds 2–3 provinces per
   province-bearing resource and `province_field` redistributes a large share of
   the world total into them — copper 65%, petroleum 60%, iron 55%, coal 45%.
   Skipped entirely when `pl == nullptr`, which preserves the identity contract for
   bodies with no planetology.

   **Conservation is over the resource's bearing set, not over all land.** That
   distinction is load-bearing rather than pedantic: conserving across land drains
   petroleum by 47%, because the province takes its share from tiles that never
   bore any in the first place.

Then **endemic trade goods (BL-191, endemic goods)** *add* deposits rather than scale
them — an endemic good has no base distribution to scale; it exists only where it
evolved. Each `endemic_good` from the Planetology state places its resource only
where latitude band ∩ wrapped longitude sector ∩ suitable composition all hold
(tobacco → grassland; spices → wetland/forest; coffee → forest; furs → tundra),
densest at the sector centre and thinning toward its edge. The amount rides
`deposit_scalar` like every other deposit. Distance-from-origin pricing of these
goods happens in `hard_coded_world.cpp`'s market authoring, not here.

---

## Rivers

Rivers are BL-170 (rivers). A **sibling pass** (BL-051 convention), `generate_rivers`
(`src/world/river_generation.{hpp,cpp}`), runs after `generate_body_tiles` returns for the
homeworld — it needs Pass 2's ocean placement and the Pass 5 landform set for source-tile
candidacy, so it is not spliced into the six-pass core.

**A river is an EDGE, never a tile** — no lake or other tile-occupying water feature is implied.
Each tile carries two `std::uint8_t` bitmasks (`tile_component::river_edges` /
`river_downstream`, `src/world/components.hpp`): bit *i* of `river_edges` marks that the tile's
hex side *i* carries a river crossing (side order 0=E, 1=NE, 2=NW, 3=W, 4=SW, 5=SE, odd-r offset);
the same bit in `river_downstream` marks whether that side is the outflow (set) or inflow (clear)
direction. `tile_borders_river(tc)` reads `river_edges != 0` — the water-adjacency signal a
farming predicate reads; the pass only records it.

**Trace algorithm.** Source candidates are non-ocean tiles carrying a `mountain` or `highland`
landform, sorted by (height descending, raster index ascending) so candidate order is a pure
function of the height field and grid shape. A seeded `std::mt19937` (`seed ^ 0x52495645u`,
"RIVE") picks `clamp(gw*gh / 1800, 3, 24)` sources by striding the sorted list with a jittered
offset per stride. Each source then walks its 6 hex neighbours, always stepping to the
strictly-lowest-height neighbour available; a step onto an `ocean` tile ends the trace there, and
a tile with no lower neighbour ends the trace as a **basin** (no lake tile is created). Height
strictly decreases every step, so termination and cycle-freedom follow by construction — no
explicit visited-set is needed.

**Logistics discount.** `river_edge_discount(from, side)` (`river_generation.hpp`) returns a
per-edge multiplier — 1.0 (no discount) if that side of `from` carries no river, else 0.75
downstream / 0.85 upstream — folded into the intra-body A* edge cost in
`src/world/logistics.cpp` alongside the road-tier discount (stacking **multiplicatively**, not a
new resource or market good). See `docs/economy/LOGISTICS.md` for the stacking rule against the
road ladder. `hex_side_for_offset(dc, dr, odd_row)` is the shared mapping from a cardinal
A*-grid step to the hex side river tracing recorded, since the A* neighbour walk is 4-cardinal
on the raster grid while river sides are the full 6-neighbour hex.

Verified by `tools/verify/river_generation_harness.cpp` (determinism, mutual edge consistency,
monotonic descent / no cycles, discount ordering) and a bitmask-identity check folded into
`tools/verify/determinism_harness.cpp`.

---

## Province partition

> **[`PROVINCES.md`](PROVINCES.md) is the authority for the province as a game object** — what
> it is, the three size constants and why there are three, the three domains, what reads it, and
> the rulings behind all of it. This section keeps the **generation-pass** view: where the
> partition sits in the pipeline and what it consumes. The partition is BL-515 (province
> partition); the water domains are BL-516 (water provinces).

The last generation pass over a body's land: `build_province_partition`
(`src/world/province.{hpp,cpp}`), run from `make_hard_coded_world` after nations,
corporations, population centres, rivers and roads exist — because it reads all of
them. It is organic rather than a regular block partition by ruling (Ben, 2026-08-21):
*"packing each province perfectly looks nice, but it is scarcely how borders were
defined in history."*

**Provinces grow from settlement and are stopped by terrain.** Four rulings
(Ben, 2026-08-21) define it, and `province.hpp` is the authority for the mechanics:

| Ruling | How the pass realises it |
|---|---|
| Seeds are population centres, strength scaling with scale 1–5 | Each centre seeds one region with a growth budget of 7 tiles (village) to 12 (metropolis); all centres grow simultaneously as one multi-source fill |
| Boundaries are rivers, elevation difference, and sometimes roads — but **a road binds** | Integer edge cost `base 10 + river 40 + round(\|Δheight\| × 683) + jitter 0–4`, the whole sum divided by 4 when both tiles are roaded |
| Identity is the **lowest-id member tile** | Derived, never allocated — so an id cannot be handed out in the wrong order, and nothing new is serialised |
| Country no centre reaches becomes **hinterland** | Seeds chosen from the least-accessible tile onward at a minimum spacing of 3, all chosen before any grows |
| 7–12 soft, 3–12 hard, boundaries win ties; **tiny provinces are kept** | A region takes its first 3 tiles at any cost, then grows to its budget, then annexes only ground no harder to reach than what it already holds. Nothing is ever merged away |
| **12 is a PREFERENCE; 20 is the hard cap** (Ben, 2026-08-21, NR-438) | Growth clamps at 12, but singleton absorption can carry a full region past it. The bound that is *asserted* is 20 (`k_province_hard_cap_tiles`); the over-12 share is **reported**, never asserted |

**Why the cap is 20 and not 12.** Pass 3 absorbs a one-tile province into its
**cheapest** neighbour, and that neighbour may already hold 12. The three ways out
are clamping (dishonest), preferring a roomier neighbour (which contradicts the
cheapest-edge rule the growth model is *expressed in*), or a higher bound. Ben chose
the bound — *"we prefer up to 12 tiles, but up to 20 is permitted in rare cases"* — so
the cheapest-edge rule survives intact, which is what the ruling protects. (The
prefer-room variant was measured at 241 over the preference, max 14, and rejected;
the breach was its only justification.)

**Elevation is read from the retained heightmap.** The pass reads
`tile_component::height` — Pass 1's normalised heightmap, retained for this
consumer (BL-517, retained height) — not the seven landform classes, whose numeric
order means nothing (`GENERATION_LEDGER.md` § Data lifetime).

### Provinces over water

Ben: *"We can also draw provinces over the ocean, using 3-12 size coastal tile
provinces. Ocean provinces should be much larger, but not larger than say 80
tiles."* The partition runs **the same algorithm three times**, over three
exclusive tile sets, and a province never spans two of them:

| Domain | Tiles | Growth clamp | Hard cap | Seed spacing | Seeded by |
|---|---|---|---|---|---|
| Land | everything not water | 12 (preferred) | 20 | 3 | population centres, then hinterland |
| Coastal water | `coast` + `lake` | 12 (preferred) | 20 | 3 | hinterland only |
| Open ocean | `ocean` | **80** | — (see below) | 7 | hinterland only |

**The land-only invariant is narrowed, not deleted** (NR-428). Land provinces are
hex-connected land that never spans water; the general claim — asserted by the
harness as P2b — is that **a province holds exactly one domain**, which is strictly
stronger, since it also forbids a lake joining the sea.

**Open ocean has no separate hard cap, deliberately.** Land needed one because Ben
ruled a preference (12) and a bound (20) as two different numbers; for the sea he
named one number. Inventing a second would invent a threshold nobody chose, so
what carries the 80 instead is the exact identity the harness asserts: *every tile
above it arrived by singleton absorption, never by growth.*

**The sea spacing is measurement-pinned, and the pin rule is "the cap must stay a
guard, not a clamp."** Seeds at separation *d* tile a plane in cells of area
(√3/2)·*d*², so the lattice predicts a mean size; where growth is running into the
ceiling instead of meeting its neighbours, the measured mean falls away from that
prediction and provinces pile up on the clamp exactly:

| d | ideal cell | measured mean | max | exactly on the 80 | provinces |
|---|---|---|---|---|---|
| 6 | 31.2 | 32.17 | 75 | 0 (0.0%) | 2,901 |
| **7** | **42.4** | **41.07** | **82** | **26 (1.1%)** | **2,272** |
| 8 | 55.4 | 49.29 | 83 | 207 (10.9%) | 1,893 |
| 9 | 70.1 | 55.25 | 83 | 507 (30.0%) | 1,689 |

At *d* = 8 one province in nine sits exactly on 80 — the clamp is drawing the size
rather than guarding it. At *d* = 7 the measured mean still matches its lattice
prediction, which is the evidence that terrain and spacing set the size. 41 tiles
against land's 8.6 is also "much larger" by nearly five times.

**Sea provinces are addressable empty space.** Units are land-bound (`march_unit`
refuses a water destination outright), buildings refuse water, and a sea province
sustains zero buildings. They exist without a naval model to justify them.

**Open question for Ben:** a **lake** is partitioned on the coastal band, as its own
province. Ben named lakes as a tile kind but did not rule what province a lake
belongs to (its own, the surrounding land province, or a coastal one). Its own was
chosen because it invents no new size rule and keeps the one-domain invariant.

**The measured distribution, 6 seeds** (`tools/verify/province_partition_harness.cpp`,
sections C and D — which is also the re-pinning instrument for the two
measurement-pinned coefficients):

| Partition | provinces | min | max | mean | < 7 | < 3 | > 12 | % in 7–12 |
|---|---|---|---|---|---|---|---|---|
| Organic, pre-absorption | 24,498 | 1 | 12 | 7.87 | 6,195 | 3,008 | 0 | 74.71% |
| Organic, **with absorption** | 22,390 | 1 | **16** | 8.61 | 4,098 | 913 | 1,096 | 76.80% |

The spread is wide **on purpose** and is reported rather than tuned: organic
borders are irregular, and the sub-floor tail is the pockets a ceiling leaves
behind — kept by ruling, not repaired.

Read the absorption row against the hard cap, not against 12. **Max 16 against a cap
of 20**, so the bound holds with four tiles of headroom, and **4.90% sit above the
preferred 12**. Absorption is what moves every one of those numbers: it converts
2,098 one-tile provinces into member tiles of their cheapest neighbour, which is
why the count falls, the mean rises, and the sub-floor tail more than halves. The
harness asserts the cap and the accounting identity (every tile above 12 arrived
by absorption, so growth's own clamp is still proven separately) and **reports**
the 4.90% — whether that counts as "rare" is Ben's judgement against a number, and
no threshold for it has been chosen.

---

## Further passes

Each of the following is a separate sibling pass under the BL-051 convention — design
its model before promoting; do not take them as one brief.

**Smooth band transitions.** The composition table has hard boundaries between
moisture and temperature bands. A smoothing pass would blend compositions at band
edges using noise-weighted mixing, eliminating any visible horizontal banding
artefact.

**Tectonic landforms beyond mountains.** `run_continents` (`CONTINENTS.md`) feeds a
plate-boundary height bias into Pass 1 and a `convergent` mask that Pass 5 seeds
mountain clusters from. The same treatment for **rift** clusters (seeding along
divergent boundaries rather than by composition and latitude) and concentrating
**volcanic** activity along boundaries are the remaining halves of that idea.

**Coastline refinement.** Enclosed seas come from the Continents/Drift pass's
rift-basin depression and the enclosed-sea acceptance gate (`CONTINENTS.md`
§ Rift-basin sea). Archipelagos and large lakes are not produced by any pass; they
could come from multi-scale noise layering or post-processing the coastline with
additional BFS passes.

**Additional body types.** The pipeline's parameter set is adequate to model gas
giant moons, outer ice worlds, and terrestrial bodies with thin reducing
atmospheres. Adding them requires only new entries in the prototype body set with
appropriate physical inputs.

---

## Implementation notes

The pipeline lives in `src/world/tile_generation.{hpp,cpp}`. `make_hard_coded_world()`
obtains each `body_profile` from `run_planetology`, runs `run_continents` for the
height bias, and calls `generate_body_tiles()`; the passes read only the profile
(plus the optional planetology state and continent bias), never the body's
identity.

- **Simplex noise** — a self-contained seedable 3D simplex implementation. The
  heightmap and moisture are sampled on a *cylinder* (the column axis is wrapped
  around a circle and fed to 3D noise), so the field is seamless across the
  horizontal wrap — no antimeridian seam. `base_cycles` sets feature scale as a
  number of cycles around the equator, so it is independent of grid width.
- **Seed management** — the heightmap, moisture noise, composition draws, cluster
  pass, and per-tile deposit draws each consume the body seed XOR'd with a
  distinct prime offset. Deposits additionally fold in the tile index, so a tile's
  deposits are independent of its neighbours. No separate seed bookkeeping.
- **Pass order** — ocean tiles (Pass 2) are finalised before composition (Pass 4);
  composition before deposits (Pass 6). Landform (Pass 5) runs before deposits,
  since deposit modifiers are landform-dependent.
- **Colour table** — `terrain_colour()` lives in `src/ui/hex_render.hpp`, keyed on
  the `(substrate, cover, density)` triple: a substrate base blended toward a cover
  endpoint by density, calibrated so the four canonical covers render at their
  canonical RGB. Landform *is* rendered — a subtle relief tint (`landform_relief`,
  `src/ui/hex_render.cpp`) over the terrain colour, plus always-on glyphs for the
  four dramatic landforms (mountain, canyon, crater, rift — `ui::icons::landform`,
  `src/ui/icons.cpp`), with contiguous same-landform runs bridged into spanning
  markers (`landform_span`, BL-232 landform spans).
- **`first_land_tiles()`** — lives in `tile_generation.cpp`; checks against
  water (`is_water`) to pick building attachment tiles.
- **Hazard / habitability** — not specified by the design tables but carried on
  `tile_component`, so they are *derived*: a per-substrate base (the habitability
  ceiling from TILES.md, plus a hazard base), modified by the cover and then by landform (mountain/rift
  raise hazard and cut habitability; valley raises habitability), with light
  jitter.
- **`road_level` is stamped outside this pipeline.** The six-pass tile generation
  leaves `tile_component.road_level` at 0; it is populated *after* nation and
  population-centre generation by the **road pass** (BL-146 roads, `generate_roads` in
  `src/world/road_generation.cpp`, run from `hard_coded_world.cpp`), which needs the
  cities the pipeline does not yet know about. It is deterministic from the same
  world state — no new seed. See LOGISTICS.md for the network shape and the A* cost
  effect; the on-canvas rendering is in `body_surface_canvas.cpp`.

### Scale

- **Seed counts scale with grid area.** The Pass 5 seed counts are authored for a
  reference-scale globe (`reference_tiles = 1200`); taken as absolutes they collapse
  to near-zero coverage on a larger grid. `scale_to_area()` scales the counts up with
  grid area (never below the authored count, so small bodies such as Pallas are
  unaffected).

  **The homeworld grid is 312×145 = 45,240 tiles (Ben, 2026-08-12)**, which puts the
  scale factor at ~37.7×. Because `scale_to_area()` derives the factor from the actual
  grid, the seed tables above are authored once and never re-tuned for grid size.

  The authority for the dimensions is `home_grid_width` / `home_grid_height`
  (`src/world/hard_coded_world.hpp`); the homeworld construction site and the wizard
  preview both read those constants, so changing one cannot move the preview without
  moving the world.

### Generation record hook

`generate_body_tiles()` takes an optional `generation_record*`. When non-null it
captures the per-pass intermediates (heightmap, ocean score and threshold, moisture,
latitude bands). The common path passes `nullptr` and pays nothing. Generation is
deterministic, so this is the seam the **Generation Ledger**
(`GENERATION_LEDGER.md`) reads to explain *why* a tile turned out as it did; what the
record does and does not attribute is in GENERATION_LEDGER.md § The data seam.
