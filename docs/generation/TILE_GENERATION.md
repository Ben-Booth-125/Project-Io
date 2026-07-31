# Project Io — Tile Generation

This document specifies the strategy and rules for procedural tile generation in
`hard_coded_world.cpp`. Generation is **deterministic**: every body has a fixed
seed and a solar-parameter profile — since BL-167 (planetology) **derived by the
Planetology chain**, not hand-authored — producing the same world on every run.
The same six-pass pipeline runs for every body; body character comes from the
parameters fed into it, not from body-specific code paths.

True procedural generation — randomised maps per campaign — is **deferred from
the prototype**. See § Deferred.

**Pipeline-shape convention (settled 2026-07-21, BL-051).** The six-pass core below stays fixed.
Every generation extension — rivers ([[BL-170]]), the full-set deposit rarity scalar ([[BL-040]],
shipped), future coastline/band smoothing, and body-level Planetology ([[PLANETOLOGY.md]],
[[BL-167]]) — lands as its own **sibling pass**: a separate file/function invoked around
`generate_body_tiles`, reading the shared `generation_record` rather than growing the six-pass
function itself. Planetology in particular runs *before* this pipeline (its atmosphere output
feeds this pipeline's solar-parameter input) — see `GENERATION_STRATEGY.md`.

---

## Design principles

**Solar parameters drive body character.** Each body is described by a small set
of constants — temperature class, atmospheric class, hydrological state,
geological activity — expressing what kind of world it is. The generation passes
read these constants; they contain no body-specific branches.

**Fixed seed, derived constants** *(updated 2026-07-31)*. The RNG is seeded
deterministically per body. Solar parameters are no longer hard-coded: each
`body_profile` is the **return value of the Planetology pass** (`run_planetology`,
called from `plan()` in `hard_coded_world.cpp` — see `PLANETOLOGY.md`). What is
authored is the body's physical *inputs* (`prototype_body()` in `planetology.cpp`);
the chain derives the profile. The result is still a stable world identical
across every run.

**Same pipeline, all bodies.** Every body — planet, moon, asteroid — runs all six
passes. Passes that produce no output for a given body type (e.g. ocean placement
on an airless body) return immediately without side effects.

**Approximate geology.** Composition tables and deposit weights follow
`docs/economy/TILES.md` as a guide. Exact deposit amounts and secondary modifiers
are approximated; strict physical realism is deferred.

---

## Solar parameters

Each body carries a set of solar-level constants (`body_profile`,
`src/world/tile_generation.hpp`). These are **derived from physical inputs by the
Planetology chain** (landed 2026-07-21, BL-167 — `PLANETOLOGY.md`), realising what
this doc used to defer as "solar parameter derivation".

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

**Derived by Planetology; the table is the regression baseline, not the source**
*(updated 2026-07-31)*. Each profile below is what `run_planetology` derives from
the body's authored physical inputs. The derivation was checked against the old
hand-authored values — 23 of 24 fields reproduce exactly (`PLANETOLOGY.md`
§ Implementation) — so this table now serves as the regression reference the
harness asserts against, not as authored constants.

| Body | `temperature_class` | `atmosphere_class` | `hydrological_state` | `geological_activity` | `water_fraction` | `composition_bias` |
|---|---|---|---|---|---|---|
| Cinder | `scorching` | `none` | `none` | `low` | 0.0 | `standard` |
| Kepler | `temperate` | `thick` | `liquid` | `moderate` | 0.60 | `standard` |
| Selene | `cold` | `none` | `polar_frozen` | `none` | 0.0 | `standard` |
| Pallas | `cold` | `none` | `none` | `none` | 0.0 | `metallic` |

**Cinder** — hot inner planet (Mercury analogue). No liquid water; high volcanic
and barren coverage. Its `geological_activity` is the one field the derivation
*changed*: authored `high`, derived `low` — Mercury is genuinely geologically
dead, and the authored value was flavour. It costs Cinder some mountain and rift
cluster seeds.

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

**Continent height bias (landed 2026-07-28, BL-210 first slice).** Before
normalisation, the per-tile `height_bias` from the Continents/Drift pass
(`run_continents`, `src/world/continents.cpp` — see `CONTINENTS.md`) is added
into the raw noise field. Plate-boundary uplift and rift subsidence therefore
shape the *same* heightmap the noise would otherwise produce alone — one terrain
source, not two competing ones. A null bias reproduces the pre-BL-210 surface
bit-for-bit (`continent_bias` parameter of `generate_body_tiles`).

This single heightmap is shared across all subsequent passes:

- **Ocean threshold** (Pass 2) — tiles below a water-fraction-derived threshold
  become ocean.
- **Landform base** (Pass 5) — high-`H` tiles are mountain/highland candidates;
  low-`H` non-ocean tiles are valley candidates; the middle band defaults to plains.

The same heightmap on an airless body still generates and drives landform
assignment — a crater-heavy moon still has elevation structure.

---

### Pass 2 — Ocean placement

*Skipped if `hydrological_state != liquid`.*

Apply a **latitude bias** to the heightmap before thresholding:

```
H'[col][row] = H[col][row] − latitude_bias(row, gh)
```

`latitude_bias` peaks at the equatorial rows and falls off toward the poles,
increasing ocean probability near the equator without enforcing a uniform band.
The noise in `H` breaks the coastline into an irregular noise-banded shape rather
than a smooth oval.

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

*(Cold column retuned 2026-06-14: the polar band was tightened from the outer 50%
of rows to the outer 30%, so `polar_frozen` bodies stop reading half-icy — see
§ Deviations and the dated comment in `band_for_row`, `tile_generation.cpp`.)*

A scorching body collapses to temperate/subtropical/tropical only. A cold body
collapses to polar/subpolar/temperate only. A frozen body is all polar.

Also compute a **moisture value** `M[col][row]` from a second simplex noise pass
at a different seed offset. Moisture is independent of latitude, normalised to
`[0.0, 1.0]`. It acts as the second axis of the composition lookup in Pass 4 and
ensures band boundaries are irregular rather than sharp horizontal lines.

---

### Pass 4 — Composition assignment

Each non-ocean land tile receives a `terrain_composition` by looking up
`(temperature_band, moisture_value)` in the table below.

**Bodies with `atmosphere_class == none` or `thin`** skip habitable compositions
(grassland, forest, wetland). The moisture axis still applies to choose among the
available inorganic types.

**Bodies with `composition_bias == metallic`** override this table: most tiles
receive `metallic`, minority `rocky` and `regolith`. See the airless table below.

#### Atmosphere-present composition table

*(High-moisture cutoff retuned 2026-06-14, 0.65 → 0.55, so more tiles reach the
wet branches that produce forest and wetland — see § Deviations.)*

| Effective band | Low moisture (0–0.35) | Mid moisture (0.35–0.55) | High moisture (0.55–1.0) |
|---|---|---|---|
| Polar | Icy | Icy | Icy |
| Subpolar | Rocky | Rocky / Tundra | Tundra |
| Temperate | Barren / Rocky | Rocky / Grassland | Grassland / Forest |
| Subtropical | Barren | Barren / Grassland | Grassland / Wetland |
| Tropical | Barren | Barren | Forest / Wetland |

Where two compositions are listed, the split is weighted approximately 60/40 and
resolved by a draw against `M[col][row]`.

#### Abiotic fallback table (landed 2026-07-21, BL-167)

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
cannot shift any downstream pass. Currently dormant on the shipped body set —
Kepler is the only atmospheric body and always lives (`PLANETOLOGY.md` § Known
dormancy) — but exercised by a synthetic harness case.

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

**Seed counts** are derived from `geological_activity` and body type. Crater
field seeds are elevated for airless bodies (`atmosphere_class == none`)
regardless of geological activity.

| Feature | `none` | `low` | `moderate` | `high` |
|---|---|---|---|---|
| Mountain range seeds | 0 | 2 | 4 | 5 |
| Rift zone seeds | 0 | 0 | 1 | 3 |
| Crater field seeds (atmospheric) | 0 | 1 | 2 | 1 |
| Crater field seeds (airless bonus) | +4 | +3 | +3 | +2 |

**Mountain range seed placement** — seeds prefer tiles where `H > 0.65` and
composition is rocky or barren. BFS expansion uses a decay probability of 0.65
per step, out to a fourth ring (retuned 2026-06-14 from `{2 rings, 0.55}` — see
§ Deviations). The expanding frontier transitions:

```
core tile → mountain
first ring → highland (probability 0.7) or mountain (probability 0.3)
second ring → highland (probability 0.5) or plains (probability 0.5)
```

**Rift zone seed placement** — seeds prefer volcanic or barren tiles at subtropical
or tropical latitude. Growth uses decay 0.60 to a fourth ring (retuned 2026-06-14
from `{2, 0.55}`) and is weakly directional — the two purely vertical neighbours
are damped ×0.5, biasing growth east–west to produce linear features. Frontier
transitions:

```
core tile → rift
first ring → canyon (probability 0.6) or rift (probability 0.4)
second ring → canyon (probability 0.3) or plains
```

**Crater field seed placement** — seeds can land on any composition; preferred on
regolith and barren for airless bodies. Growth radius is tight (decay 0.55 to a
third ring — retuned 2026-06-14 from `{1, 0.45}` so craters read as impact
features rather than single-tile stamps):

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

**Calibrated subset deposit table** — the original seven-resource subset, authored
on the per-tile `tile_rng` stream. These values are hand-calibrated and the economy
is tuned on them; the full-set pass (BL-040, below) leaves them bit-for-bit
unchanged.

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

**Full raw-set additions (BL-040)** — the remaining Tier 1 raw resources, authored
on an **independent per-tile rng stream** (`rare_rng`) so they cannot perturb the
calibrated subset above or the derived environment. Each is gated by its seeded
rarity scalar (§ deposit rarity in [RESOURCES.md](../economy/RESOURCES.md)): the
scalar both gates presence (frequency) and scales the rolled magnitude, so rare
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

After the deposit array is filled, two **pure post-multiplies** run in sequence.
Neither draws RNG, so each reproduces the unscaled surface bit-for-bit at its
identity value:

1. **Abundance scalar (BL-114, world descriptor).** `deposit_scalar` — sparse
   0.40 / lean 0.65 / standard 1.00 (`GENERATION_STRATEGY.md` § The world
   descriptor). Applied before the finite-reserve seeding so both scale together.
2. **Planetology endowment (landed 2026-07-21, BL-167).** The body's per-resource
   `endowment[r]`. A channel at 0.0 removes the resource outright rather than
   thinning it — "no life, no coal" lands here. A null planetology state skips
   the multiply entirely.

Then **endemic trade goods (BL-191, complete)** *add* deposits rather than scale
them — an endemic good has no base distribution to scale; it exists only where it
evolved. Each `endemic_good` from the Planetology state places its resource only
where latitude band ∩ wrapped longitude sector ∩ suitable composition all hold
(tobacco → grassland; spices → wetland/forest; coffee → forest; furs → tundra),
densest at the sector centre and thinning toward its edge. The amount rides
`deposit_scalar` like every other deposit. Distance-from-origin pricing of these
goods happens in `hard_coded_world.cpp`'s market authoring, not here.

---

## Deferred

The following are noted here as future work. None are in scope for the prototype. **Each is a
separate v0.2 pass** — design its model (the orbital-derivation formula, the plate model, the
deposit-rarity profile) before promoting; do not take them as one Brief.

**Solar parameter derivation.** *Done (BL-167, planetology — landed 2026-07-21).*
`body_profile` is now the Planetology chain's return value, derived from star
mass, orbit, body mass, and the rest of the physical inputs. See § Solar
parameters above and `PLANETOLOGY.md`.

**Smooth band transitions.** The composition table has hard boundaries between
moisture and temperature bands. A production pass would blend compositions at band
edges using noise-weighted mixing, eliminating any visible horizontal banding
artefact.

**Tectonic landforms.** *First slice landed (2026-07-28, BL-210 Continents/Drift).*
`run_continents` (`src/world/continents.cpp` — `CONTINENTS.md`) derives drifting
plates from Planetology's Engine output and feeds a plate-boundary height bias
into Pass 1, so mountain candidacy already correlates with convergent boundaries
through the heightmap. Still deferred: seeding Pass 5's mountain/rift *clusters*
along the boundaries directly (they still sample by weighted preference over the
biased heightmap) and concentrating volcanic activity there.

**Full deposit authoring.** *Done (BL-040).* Copper ore, rare earth ore, silica,
coal, iron-nickel ore, and platinum-group metals are now authored via the seeded
rarity-scalar pass — see the full raw-set additions table above and
[RESOURCES.md](../economy/RESOURCES.md) § Deposit rarity & scarcity.

**Coastline refinement.** The noise-thresholded ocean produces plausible
coastlines, but lacks features like enclosed seas, archipelagos, or large lakes.
These could be produced by multi-scale noise layering or post-processing the
coastline with additional BFS passes.

**Additional body types.** The pipeline's parameter set is already adequate to
model gas giant moons, outer ice worlds, and terrestrial bodies with thin reducing
atmospheres. Adding them requires only new entries in `make_hard_coded_world()`
with appropriate solar parameters.

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
- **Colour table** — `terrain_colour()` now lives in `src/ui/hex_render.hpp`
  (moved out of `body_surface_canvas.cpp`), keyed on `terrain_composition` (the
  11-value enum). **Superseded (2026-07-31, BL-231/BL-232):** landform *is* now
  rendered — a subtle relief tint (`landform_relief`, `src/ui/hex_render.cpp`)
  over the composition colour, plus always-on glyphs for the four dramatic
  landforms (mountain, canyon, crater, rift — `ui::icons::landform`,
  `src/ui/icons.cpp`), with contiguous same-landform runs bridged into spanning
  markers (`landform_span`, BL-232). The old "reserved for overlay glyphs later"
  note is dead.
- **`first_land_tiles()`** — moved into `tile_generation.cpp`; checks against
  `terrain_composition::ocean` to pick building attachment tiles.
- **Hazard / habitability** — not specified by the design tables but carried on
  `tile_component`, so they are *derived*: a per-composition base (the habitability
  ceiling from TILES.md, plus a hazard base) modified by landform (mountain/rift
  raise hazard and cut habitability; valley raises habitability), with light
  jitter.
- **`road_level` is stamped outside this pipeline.** The six-pass tile generation
  leaves `tile_component.road_level` at 0; it is populated *after* nation and
  population-centre generation by the **road pass** (BL-146, `generate_roads` in
  `src/world/road_generation.cpp`, run from `hard_coded_world.cpp`), which needs the
  cities the pipeline does not yet know about. It is deterministic from the same
  world state — no new seed. See SUPPLY.md for the network shape and the A* cost
  effect; the on-canvas rendering is the BL-147 follow-on.

### Deviations from the tables above

- **Seed counts scale with grid area.** The Pass 5 seed counts are authored for a
  reference-scale globe; taken as absolutes they collapse to near-zero coverage on
  the prototype's 180×84 grids. `scale_to_area()` scales the counts up with grid
  area (never below the authored count, so small bodies such as Pallas are
  unaffected). Reference lowered 1800 → 1200 tiles in the 2026-06-14 retune, so
  the 180×84 grids get a ~12.6× scale factor.
- **Kepler biome-balance retune (2026-06-14).** Three co-ordinated changes,
  now reflected in the tables above and recorded here as deliberate deviations
  from the original authored values: the **cold polar band** tightened from the
  outer 50% of rows to the outer 30% (`band_for_row` — Selene's icy cap fell from
  ~52% to ~30% coverage); the **high-moisture cutoff** lowered 0.65 → 0.55
  (`moisture_column` — forest/wetland were stuck near ~1% / ~0.5% on Kepler); the
  **Pass 2 equatorial ocean bias** lowered 0.15 → 0.05 (`bias_amp` — the old
  value drowned most tropical land).
- **Cluster shape retune (2026-06-14).** `shape_of()` grew the clusters:
  mountain `{2 rings, 0.55}` → `{3, 0.65}`, rift `{2, 0.55}` → `{3, 0.60}`,
  crater `{1, 0.45}` → `{2, 0.55}`. Pairs with the reference-tiles change above
  to make landform features more prominent and more numerous.

### Generation history hook

`generate_body_tiles()` takes an optional `generation_record*`. When non-null it
captures the per-pass intermediates (heightmap, moisture, latitude bands, ocean
threshold). The common path passes `nullptr` and pays nothing. Generation is
deterministic, so this is the seam the **Generation Ledger**
(`GENERATION_LEDGER.md` — Chain half built, breadcrumb still owed) reads to
explain *why* a tile turned out as it did. Note two gaps the breadcrumb will hit:
the continent height bias and the planetology endowment/endemic contributions are
**not** captured in `generation_record` today (GENERATION_LEDGER.md § The data
seam).
