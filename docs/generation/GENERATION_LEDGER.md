# Project Io — Generation Ledger

A tuning-and-analysis surface that explains **why a tile generated as it did**. It
reads the per-pass intermediates of the deterministic tile-generation pipeline and
presents them as a per-tile derivation breadcrumb and per-body summaries. Its
audience is the developer tuning the procedural passes — plus the player, through
the History slot's Chain view.

Design authority for the generator it inspects: [`TILE_GENERATION.md`](TILE_GENERATION.md)
(the six-pass pipeline and the `generation_record` hook). This document is the
design authority for the ledger itself. The ledger is BL-303 (generation ledger);
its player-facing half is BL-211 (player-facing history ledger); the field lenses
are BL-304 (field overlay lenses).

The surface has three pieces:

| Piece | Where |
|---|---|
| **Chain half — player-facing** (History slot: the Chain view; stage charts redrawn from the persisted `generation_report`) | `src/ui/generation_charts.{hpp,cpp}` |
| **Per-body summary ledger** | `src/ui/generation_ledger.{hpp,cpp}`, nav rail slot 10 |
| **Field lenses** (heightmap / moisture / band painted over the Planetary canvas) | read the same `generation_record` seam (§ Surfacing) |

**How it is reached and what it holds.** Nav rail slot 10 (the plate glyph)
toggles it into the shell fold-out column, alongside every other ledger. It is **one
flat panel** — a body selector over six sections, each a collapsing header holding a
table: **Profile** (the body descriptor the passes were run from), **Thresholds** (the
ocean threshold and the resulting water fraction against the profile's target),
**Latitude bands** (the row ranges, read back off the record rather than restated from
the generator's table), and the three distributions over the live tiles — **Substrate**,
**Cover** and **Landform**. The record is regenerated into a scratch world on open and
on a body switch, cached on `(body, tile seed)`, and never stored — § Data lifetime.

There is **no tab strip**, so no active-tab press to close the ledger: the rail slot
toggles the surface and each header toggles its own section, which is the Toggle rule
satisfied by construction. Section disclosure lives in `ui_state` rather than ImGui's
storage so a verify script can drive it (`verify.section`).

**Every distribution names its denominator in its header, and they do not all share
one.** Substrate and Cover are taken over the whole grid; **Landform is taken over land
alone** (Ben, 2026-08-30). Water carries `landform::plains` — there is no water
landform — and a temperate body is more than half ocean, so a whole-grid denominator
reports the sea as flat ground: 95.27% plains and 0.72% mountain, where the answer over
land is 88.18% and 1.80%. Every share on that table was a share of a question nobody
asks.

**The asymmetry is deliberate and is why each header names its own denominator.** Ocean
is one of Substrate's *own categories*, so excluding water there would delete a real
row; Landform has no such category, so including water only dilutes it. `is_water()`
(`components.hpp`) is the single definition, and it settles the question the ruling
turned on: **coast is water**, with lake and ocean.

**There is no per-tile derivation, here or anywhere.** The breadcrumb that explained a
single tile pass by pass was retired with the Tile view (Ben, 2026-08-30). Its stated
destination — a condensed frame in the hover card or the Selection element — had been
written down since it was factored out and was never built, so the function outlived
both the caller it had and the callers it was factored out for. Removed rather than
kept dormant: a content builder nothing calls is a claim about a surface that does not
exist.

The **body-grain** question this ledger answers is unchanged. What is gone is the
tile-grain one, and if it comes back it comes back as a Selection subject — a tile is
already one — rather than as a ledger view.

Two additive taps make this possible, both pure captures that leave the generated
surface bit-for-bit identical:

- `generation_record::ocean_score` — the latitude-biased height Pass 2 actually
  compares against `ocean_threshold`. Without it the breadcrumb would have to
  re-derive the bias constant, and a second copy of a tuning constant is a copy that
  drifts. Filled only when a record is requested.
- `generation_report::body_entry::tiles` — the arguments `generate_body_tiles` was
  called with (seed, deposit scalar, grid, whether the convergent mask was passed).
  The homeworld's seed comes out of the enclosed-sea acceptance gate
  (`CONTINENTS.md` § Rift-basin sea), so it is not re-derivable from `world_params`;
  recording it at the call site is what makes the replay exact rather than
  approximate. Presentation data, off the save seam like the rest of the report.

**Player-facing relation — the History slot.** The nav rail's **History** slot
([`../ui/MENU.md`](../ui/MENU.md) § Menu set and ordering, slot 9) is the *player-facing*
counterpart of this developer surface: it presents the **procedural generation as a
number-crunch** plus **post-generation advisory** on the resource/workforce state, gated by
exploration. This ledger (developer tuning) and the History slot (player advisory) share
the same `generation_record` seam — see MENU.md for the History scope.

See also: [`docs/ui/SELECTION.md`](../ui/SELECTION.md) — the per-tile breadcrumb
is a natural section of a tile's rich card, so the content builder is shared (below).

**Why this stays separate from the world history log.** Both this ledger and
`src/world/history_log.{hpp,cpp}` (BL-208, world history log) answer the same instinct —
explain what happened and why — and it would be reasonable to ask why they are not one
mechanism. They differ on the axis that matters for storage: **lifetime**.

- **The ledger is DISPOSABLE.** Per-tile derivation breadcrumbs — band, moisture,
  composition, landform, deposits — regenerate on demand from `generate_body_tiles(..., &record)`
  (see § Data lifetime below); they are never persisted, scoped to **tuning**, and the developer
  (or the History slot's Chain view) recomputes them per body on open. Storing one per tile for
  every generated body would be pure bloat when a single deterministic call rebuilds it exactly.

  **HEIGHT IS THE ONE EXCEPTION, and the exception has a rule.** Height would be in the list
  above except that a downstream system reads it: the province partition (BL-515, province
  partition) grows borders against elevation difference, which makes height an INPUT to the
  partition rather than a breadcrumb explaining a past decision. A field a live system reads is
  world state, whatever pass first computed it — so height is retained on `tile_component` and
  serialised (BL-517, retained height), and it leaves this bullet.

  The rule that keeps this from becoming a loophole: an intermediate graduates out of the
  disposable set **only when a system outside the ledger reads it**, and it graduates by being
  named here. Nothing else in that list has such a reader. If a later reader finds a persisted
  height field and takes it for an oversight, this paragraph is the answer — do not delete it to
  restore the symmetry.
- **The log is DURABLE.** `world::history_log` is a persisted, append-only record — genesis
  chapter, checkpoint decisions, strategic AI commands, agency actions, and trade-route
  establishment — scoped to **narrative**. It is designed to be recited, told, and partially lost
  between agents (an oral history), which only works if it is real, serialised, communicated
  content, never something regenerable on demand from a seed.

Merging them would force one lifetime onto both: either persisting per-tile breadcrumbs for every
tile of every body (the ledger deliberately refuses this), or making the narrative log
regenerable-only (which defeats a history a player — or another agent — can actually be told
about, past the point where the original seed and engine are still at hand). Same instinct,
incompatible lifetimes — two mechanisms, not a duplication.

---

## The data seam

The generator exposes everything the ledger needs:
`generate_body_tiles(..., generation_record* record)` fills an optional
`generation_record` (`src/world/tile_generation.hpp`) with the per-pass
intermediates that are otherwise discarded on the common path:

| Field | Pass | Meaning |
|---|---|---|
| `height[row*gw+col]` | 1 | normalised heightmap |
| `ocean_score[…]` | 2 | the latitude-biased height actually compared against the threshold; empty when no ocean pass runs |
| `ocean_threshold` | 2 | latitude-biased height percentile below which a tile is ocean |
| `ocean_tiles` | 2 | count assigned the ocean composition |
| `moisture[…]` | 3 | normalised moisture |
| `band[…]` | 3 | latitude band index |

Composition, landform, and deposits are not in the record because they are already
**on the tile** (`tile_component` in `src/world/components.hpp`) — the final state
the player sees. The ledger therefore joins the *record* (intermediate fields) with
the *tiles* (final fields) to tell the whole story height → ocean → band/moisture →
composition → landform → deposits.

**Two contributions the record does not attribute.** The **continent height bias**
added into Pass 1 before normalisation (`run_continents` — `CONTINENTS.md`) is folded
into `height` rather than carried as its own field, so the breadcrumb shows a
heightmap that already contains the plate bias without separating it out. The
**Pass 6 post-multiplies** (abundance scalar, planetology endowment, ore-province
field — `TILE_GENERATION.md` § Post-multiplies) are likewise not in the record; the
breadcrumb reads the endowment and the scalar from the `generation_report` entry
instead and names them as known factors on the rolled figure. Both would be small
additive fields if a pass ever needs them attributed per tile.

---

## What the ledger presents

### Per-tile derivation breadcrumb

For one selected tile, the causal chain that produced it, one row per pass, each
row naming the input value and the rule that fired:

1. **Height** — `height` value, and whether its `ocean_score` cleared `ocean_threshold`
   (→ ocean) or not (→ land). This is the first fork.
2. **Latitude band & moisture** — the `band` index (its `temperature_class`-shifted
   width) and the `moisture` value that select the climate row.
3. **Substrate & cover** — the resulting `terrain_substrate`, the `terrain_cover`
   sitting on it and that cover's `cover_density`, and *which* branch chose each
   (organic gated by `atmosphere_class`; volcanic scaled by `geological_activity`;
   metallic under `composition_bias::metallic`).

   **Two sub-steps, and the ledger shows both.** Pass 4a picks a *biome* from the
   climate tables, and 4c/4d then DECOMPOSE that biome into the substrate/cover/density
   triple with no draws at all (`TILE_GENERATION.md` § Pass 4). So the breadcrumb has an
   intermediate worth surfacing: the biome the tables chose, then what it decomposed
   into. A tile whose cover looks wrong is far easier to diagnose when you can see
   whether the *table* or the *decomposition* put it there.
4. **Landform** — the `terrain_landform`, and whether it came from a mountain-range
   or rift-zone cluster seed (and the seed it belonged to) or the default.
5. **Deposits** — the deposit profile rolled for that composition × landform, and
   the ambient-resource guarantee, with the RNG-derived magnitudes and the
   post-multiply factors applied to them.

The breadcrumb is the per-tile content builder; the **hover card** and the
**Selection info element** call the same builder to render a tile's "why" section
(see SELECTION.md § Shared content builders). The ledger frames it full-height with
the pass rules spelled out; the card shows a condensed form.

### Per-body summaries

For a whole body, the aggregate shape of the generation:

- **Substrate and cover histograms** — two of them, not one: tile count per
  `terrain_substrate` (ocean / barren / rocky / sedimentary / volcanic / metallic /
  regolith / icy) and per `terrain_cover` (none / grass / scrub / forest / marsh /
  snow / dunes / ash / salt / urban), each with percentages, and cover
  weighted by `cover_density` as well as counted — a body that is 30% forest at
  density 20 is a different place from one that is 30% forest at density 200. The
  surface for spotting a sparse forest or wetland share on the homeworld without
  eyeballing the map.
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
  (the homeworld alone is 312×145 = 45,240 tiles, each with height/moisture/band) is
  pure bloat when a single `generate_body_tiles(..., &record)` call rebuilds it exactly.
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
  are tabular and belong in a ledger alongside the Tile Ledger, reached from the
  navigation rail. Body selector defaults to the current view's main body (mirrors
  the Tile Ledger default).
- **Per-tile breadcrumb → both the ledger and a Planetary overlay lens.** In the
  ledger it is the detail panel for the selected tile. As a **lens** (an
  `overlay_mode` over the Planetary canvas, see [`docs/ui/LENSES.md`](../ui/LENSES.md))
  it paints the *fields themselves* across the surface — heightmap, moisture, or band
  as a gradient — so the spatial structure of a pass is visible at a glance. The lens
  is how you see "the equatorial ocean bias is too strong"; the breadcrumb is how you
  read one tile's exact numbers. The field lenses are BL-304 (field overlay lenses).

The per-tile breadcrumb **shares its content builder with the hover card and the
Selection info element** (SELECTION.md). The *frame* differs (ledger panel vs. tooltip
vs. pinned panel); the *content* — the five-step causal chain — does not. It is built
once as a tile-derivation builder and the three callers wrap it.

---

## Open design questions

- **Lens field set.** Which intermediate fields earn a lens (height, moisture, band —
  and later any new pass output). Folds into BL-304 (field overlay lenses), which
  also adds a gradient-legend palette the field lenses reuse.
- **Cluster seed visualisation.** Landform clusters (mountain ranges, rift zones) are
  seed-grown; showing the seed points and their growth is a richer view than a flat
  landform field, but needs the seed positions captured in `generation_record` — a
  small additive field when such a lens is built.
- **The History slot's player-facing views.** The History nav-rail slot carries four
  views — **Story** (the body's dated oral-history biography), **Chain** (the generation
  stage charts, one collapsing accordion per chain stage, grouped by the wizard's three
  rounds), **Ages** (the Era −1 political time-lapse) and **Tectonics** (the plate
  field). The charts are the New World wizard's own plots, extracted into
  `src/ui/generation_charts.{hpp,cpp}` and redrawn from the persisted
  `generation_report`. The exploration gate on those views (MENU.md's History slot
  calls for it) is owned by BL-211 (player-facing history ledger).
- **Is `valley` reachable at all, and are Pass 5's clusters firing hard enough?** Read
  over land rather than over the grid, the default body is 88.18% plains, and `valley`
  never generates - zero tiles, on a landform the enum declares and the generator can
  apparently never produce. Mountain is 1.80% and rift 0.26%. This is a generation
  tuning question rather than a surface one, and it is the ledger doing exactly what it
  exists for: the numbers only became legible once the denominator was right.
