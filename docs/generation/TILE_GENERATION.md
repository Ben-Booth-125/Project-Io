# Project Io — Tile Generation

This document specifies the strategy and rules for procedural tile generation in
`hard_coded_world.cpp`. Generation is **deterministic**: every body has a fixed
seed and authored solar parameters, producing the same world on every run. The
same six-pass pipeline runs for every body; body character comes from the
parameters fed into it, not from body-specific code paths.

True procedural generation — randomised maps per campaign — is **deferred from
the prototype**. See § Deferred.

---

## Design principles

**Solar parameters drive body character.** Each body is described by a small set
of constants — temperature class, atmospheric class, hydrological state,
geological activity — expressing what kind of world it is. The generation passes
read these constants; they contain no body-specific branches.

**Fixed seed, authored constants.** The RNG is seeded deterministically per body.
Solar parameters are hard-coded in `make_hard_coded_world()`. The result is a
stable, authored world identical across every run.

**Same pipeline, all bodies.** Every body — planet, moon, asteroid — runs all six
passes. Passes that produce no output for a given body type (e.g. ocean placement
on an airless body) return immediately without side effects.

**Approximate geology.** Composition tables and deposit weights follow
`docs/economy/TILES.md` as a guide. Exact deposit amounts and secondary modifiers
are approximated; strict physical realism is deferred.

---

## Solar parameters

Each body carries a set of solar-level constants. For the prototype these are
authored by hand; a future generation layer could derive them from orbital
mechanics (see § Deferred).

| Parameter | Type | Values |
|---|---|---|
| `temperature_class` | enum | `scorching`, `hot`, `temperate`, `cold`, `frozen` |
| `atmosphere_class` | enum | `none`, `thin`, `moderate`, `thick` |
| `hydrological_state` | enum | `none`, `polar_frozen`, `liquid` |
| `geological_activity` | enum | `none`, `low`, `moderate`, `high` |
| `water_fraction` | float 0–1 | Target ocean coverage; only used when `hydrological_state == liquid` |
| `composition_bias` | enum | `default`, `metallic` | Override for bodies where surface composition is dominated by a single type |

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

These values are fixed for the prototype and hard-coded in `make_hard_coded_world()`.

| Body | `temperature_class` | `atmosphere_class` | `hydrological_state` | `geological_activity` | `water_fraction` | `composition_bias` |
|---|---|---|---|---|---|---|
| Cinder | `scorching` | `none` | `none` | `high` | 0.0 | `default` |
| Kepler | `temperate` | `thick` | `liquid` | `moderate` | 0.60 | `default` |
| Selene | `cold` | `none` | `polar_frozen` | `none` | 0.0 | `default` |
| Pallas | `cold` | `none` | `none` | `none` | 0.0 | `metallic` |

**Cinder** — hot inner planet (Mercury analogue). No liquid water; high volcanic
and barren coverage; rift zones and mountain ranges from high geological activity.

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
to `[0.0, 1.0]`. This single heightmap is shared across all subsequent passes:

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
| Polar | 0–10, 90–100 | — | 0–25, 75–100 |
| Subpolar | 10–22, 78–90 | — | 25–45, 55–75 |
| Temperate | 22–42, 58–78 | 0–20, 80–100 | 45–55 |
| Subtropical | 42–47, 53–58 | 20–40, 60–80 | — |
| Tropical | 47–53 | 40–60 | — |

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

| Effective band | Low moisture (0–0.35) | Mid moisture (0.35–0.65) | High moisture (0.65–1.0) |
|---|---|---|---|
| Polar | Icy | Icy | Icy |
| Subpolar | Rocky | Rocky / Tundra | Tundra |
| Temperate | Barren / Rocky | Rocky / Grassland | Grassland / Forest |
| Subtropical | Barren | Barren / Grassland | Grassland / Wetland |
| Tropical | Barren | Barren | Forest / Wetland |

Where two compositions are listed, the split is weighted approximately 60/40 and
resolved by a draw against `M[col][row]`.

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
composition is rocky or barren. BFS expansion uses a decay probability of 0.55
per step. The expanding frontier transitions:

```
core tile → mountain
first ring → highland (probability 0.7) or mountain (probability 0.3)
second ring → highland (probability 0.5) or plains (probability 0.5)
```

**Rift zone seed placement** — seeds prefer volcanic or barren tiles at subtropical
or tropical latitude. Growth is weakly directional (slightly biased east–west to
produce linear features). Frontier transitions:

```
core tile → rift
first ring → canyon (probability 0.6) or rift (probability 0.4)
second ring → canyon (probability 0.3) or plains
```

**Crater field seed placement** — seeds can land on any composition; preferred on
regolith and barren for airless bodies. Growth radius is tight (decay 0.45):

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

**Prototype deposit table** — only the seven-resource prototype subset is authored
with non-zero primary deposits. All other resource enum slots are defined but
receive zero. No generation code changes are needed when remaining resources are
authored; the deposit arrays already carry the full enum width.

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
| Regolith | (ambient only) | — | — | — | — |

Modifiers apply multiplicatively to the upper bound of the base range.

---

## Deferred

The following are noted here as future work. None are in scope for the prototype. **Each is a
separate v0.2 pass** — design its model (the orbital-derivation formula, the plate model, the
deposit-rarity profile) before promoting; do not take them as one Brief.

**Solar parameter derivation.** In the prototype, `temperature_class`,
`atmosphere_class`, and `geological_activity` are authored per body. A future
generation pass could derive these from orbital mechanics — distance from the
star, star luminosity, body mass, albedo — so that new bodies in a procedural
campaign need no manual parameter authoring.

**Smooth band transitions.** The composition table has hard boundaries between
moisture and temperature bands. A production pass would blend compositions at band
edges using noise-weighted mixing, eliminating any visible horizontal banding
artefact.

**Tectonic landforms.** Mountain range and rift seeds are currently placed by
weighted random sampling. A tectonic simulation pass would derive plate boundaries
as the structural input, concentrating volcanic activity and mountain chains along
boundary zones and producing more geographically coherent features.

**Full deposit authoring.** Copper ore, rare earth ore, silica, coal, and the
remaining Tier 1 resources receive zero deposits in the prototype. Authoring their
deposit ranges is a data pass that requires no generation architecture changes.

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
authors a `body_profile` per body and calls `generate_body_tiles()`; the passes
read only the profile, never the body's identity.

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
- **Colour table** — `terrain_colour()` in `body_surface_canvas.cpp` is keyed on
  `terrain_composition` (the 11-value enum). Landform is *not* tinted; it is
  surfaced in the tooltip and Tile Ledger and through deposit modifiers, and is
  reserved for overlay glyphs later.
- **`first_land_tiles()`** — moved into `tile_generation.cpp`; checks against
  `terrain_composition::ocean` to pick building attachment tiles.
- **Hazard / habitability** — not specified by the design tables but carried on
  `tile_component`, so they are *derived*: a per-composition base (the habitability
  ceiling from TILES.md, plus a hazard base) modified by landform (mountain/rift
  raise hazard and cut habitability; valley raises habitability), with light
  jitter.

### Deviations from the tables above

- **Seed counts scale with grid area.** The Pass 5 seed counts are authored for a
  reference-scale globe; taken as absolutes they collapse to near-zero coverage on
  the prototype's 180×84 grids. `scale_to_area()` scales the counts up with grid
  area (never below the authored count, so small bodies such as Pallas are
  unaffected). Absolute feature *size* still follows the doc's tight ring
  transitions — making ranges more prominent is a deliberate, separate tuning step.

### Generation history hook

`generate_body_tiles()` takes an optional `generation_record*`. When non-null it
captures the per-pass intermediates (heightmap, moisture, latitude bands, ocean
threshold). The common path passes `nullptr` and pays nothing. Generation is
deterministic, so this is the seam a future **Generation Ledger** (see
`docs/development/OPENS.md`) will read to explain *why* a tile turned out as it did —
the Ledger decides what to persist versus regenerate on demand.
