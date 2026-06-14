# Project Io — Generation Ledger (design)

A tuning-and-analysis surface that explains **why a tile generated as it did**. It
reads the per-pass intermediates of the deterministic tile-generation pipeline and
presents them as a per-tile derivation breadcrumb and per-body summaries. Its
audience is the developer tuning the procedural passes, not (yet) the player.

Design authority for the generator it inspects: [`TILE_GENERATION.md`](TILE_GENERATION.md)
(the six-pass pipeline and the `generation_record` hook). This document is the
design authority for the ledger itself; it is **design only** — nothing here is
built yet.

See also: [`docs/ui/SELECTION.md`](../ui/SELECTION.md) and the deferred hover-card
item in [`docs/development/TODO.md`](../development/TODO.md) — the per-tile breadcrumb
is a natural section of a tile's rich card, so the content builder is shared (below).

---

## The data seam

The generator already exposes everything the ledger needs:
`generate_body_tiles(..., generation_record* record)` fills an optional
`generation_record` (`src/world/tile_generation.hpp`) with the per-pass
intermediates that are otherwise discarded on the common path:

| Field | Pass | Meaning |
|---|---|---|
| `height[row*gw+col]` | 1 | normalised heightmap |
| `ocean_threshold` | 2 | latitude-biased height percentile below which a tile is ocean |
| `ocean_tiles` | 2 | count assigned the ocean composition |
| `moisture[…]` | 3 | normalised moisture |
| `band[…]` | 3 | latitude band index |

Composition, landform, and deposits are not in the record because they are already
**on the tile** (`tile_component` in `src/world/components.hpp`) — the final state
the player sees. The ledger therefore joins the *record* (intermediate fields) with
the *tiles* (final fields) to tell the whole story height → ocean → band/moisture →
composition → landform → deposits.

---

## What the ledger presents

### Per-tile derivation breadcrumb

For one selected tile, the causal chain that produced it, one row per pass, each
row naming the input value and the rule that fired:

1. **Height** — `height` value, and whether it cleared `ocean_threshold` (→ ocean)
   or not (→ land). This is the first fork.
2. **Latitude band & moisture** — the `band` index (its `temperature_class`-shifted
   width) and the `moisture` value that select the climate row.
3. **Composition** — the resulting `terrain_composition`, and *which* branch chose
   it (organic gated by `atmosphere_class`; volcanic scaled by `geological_activity`;
   metallic under `composition_bias::metallic`).
4. **Landform** — the `terrain_landform`, and whether it came from a mountain-range
   or rift-zone cluster seed (and the seed it belonged to) or the default.
5. **Deposits** — the deposit profile rolled for that composition × landform, and
   the ambient-resource guarantee, with the RNG-derived magnitudes.

The breadcrumb is the per-tile content builder; the **hover card** and the
**Selection info element** call the same builder to render a tile's "why" section
(see SELECTION.md § Shared content builders). The ledger frames it full-height with
the pass rules spelled out; the card shows a condensed form.

### Per-body summaries

For a whole body, the aggregate shape of the generation:

- **Composition histogram** — tile count per `terrain_composition` (ocean / barren /
  grassland / forest / wetland / volcanic / metallic / icy …), with percentages. The
  surface for spotting "forest and wetland remain sparse on Kepler (~1% / ~0.5%)"
  (TODO § Tile generation — Kepler biome balance) without eyeballing the map.
- **Landform histogram** — tile count per `terrain_landform`.
- **Key thresholds** — `ocean_threshold`, `ocean_tiles` and the resulting water
  fraction vs. the profile's target `water_fraction`; band boundaries for the body's
  `temperature_class`.
- **Profile echo** — the `body_profile` that drove it (temperature / atmosphere /
  hydrology / geology / bias), so a surprising histogram can be traced to its inputs.

---

## Data lifetime — regenerate on demand, do not persist

Generation is **deterministic** in `(seed, profile, gw, gh)`: the same inputs always
reproduce the same surface *and* the same `generation_record`. Therefore the ledger
**regenerates the record on demand** for the body under inspection rather than
storing one. Consequences:

- **No per-tile storage cost.** Storing a record per tile for every body
  (Kepler alone is 180×84 = 15,120 tiles, each with height/moisture/band) is pure
  bloat when a single `generate_body_tiles(..., &record)` call rebuilds it exactly.
- **The record is a view, not state.** It is never serialised and never part of the
  save; it is scratch recomputed when the ledger opens for a body, cached only for as
  long as that body is the ledger's subject.
- **Cheap enough to recompute.** One body's six passes are fast; recomputing on
  ledger-open (or on a body switch) is imperceptible and keeps the world model free
  of analysis-only data.

This is the same principle the corporation/nation generators follow — derive, do not
store, when the derivation is deterministic and cheap.

---

## Surfacing

Two complementary presentations, because the two questions differ:

- **Per-body summaries → a dedicated Ledger window.** The histograms and thresholds
  are tabular and belong in a floating ledger alongside the Tile Ledger, reached from
  the navigation rail. Body selector defaults to the current view's main body (mirrors
  the Tile Ledger default, TODO § Ledger).
- **Per-tile breadcrumb → both the ledger and a Planetary overlay lens.** In the
  ledger it is the detail panel for the selected tile. As a **lens** (a new
  `overlay_mode` over the Planetary canvas, see [`docs/ui/LENSES.md`](../ui/LENSES.md))
  it paints the *fields themselves* across the surface — heightmap, moisture, or band
  as a gradient — so the spatial structure of a pass is visible at a glance. The lens
  is how you see "the equatorial ocean bias is too strong"; the breadcrumb is how you
  read one tile's exact numbers.

The per-tile breadcrumb **shares its content builder with the hover card and the
Selection info element** (SELECTION.md). The *frame* differs (ledger panel vs. tooltip
vs. pinned panel); the *content* — the five-step causal chain — does not. Build it
once as a tile-derivation builder and let the three callers wrap it.

---

## Open items

- **Lens field set.** Which intermediate fields earn a lens (height, moisture, band —
  and later any new pass output). Folds into the LENSES.md Resource-lens work, which
  also adds a gradient-legend palette the field lenses reuse.
- **Cluster seed visualisation.** Landform clusters (mountain ranges, rift zones) are
  seed-grown; showing the seed points and their growth is a richer view than a flat
  landform field, but needs the seed positions captured in `generation_record` (not
  there today — a small additive field when the lens is built).
- **Player-facing variant.** Whether any of this is ever surfaced to the player (a
  "survey" readout of a tile) or stays a developer tool. Out of scope here.
