# Project Io — Canvas Ground Rendering

This document owns **how the Planetary canvas renders its ground**: the baked-chunk
mechanism, the settled art direction, the grid rule, installations-as-geometry,
animation, level-of-detail and the verification story. [PLANETARY.md](PLANETARY.md)
keeps ownership of *what* the canvas communicates and of the analytic channels above
the ground; [CANVASES.md](CANVASES.md) keeps the ladder, layout and shared state. The
feasibility study behind these decisions is `docs/research/CANVAS_RENDERING.md`
(research tier, not authority); the style exploration that produced the reference
renders is `docs/ui/design/GLOBAL_STYLE_SHEET.md` and `docs/ui/design/renders/map/`.

Ruled by Ben, 2026-09-01, across two design forms — the second against the `map/it1–it3`
reference renders:

1. **Art direction: C-F — painterly relief + near-future grade** (ratifying the
   provisional round-2 verdict in `renders/map/PROMPTS.txt`). Semi-realistic,
   representational terrain; exaggerated hillshade for at-a-glance topography; a
   desaturated, cool, hazed grade over it. The **grade is a separable runtime pass**
   over any biome, never baked into the terrain content (round 2's confirmed finding).
2. **Mechanism: baked terrain chunks** — hillshade from the height field plus
   **authored biome brushes**, composed at bake time (superseding the same-day atlas
   ruling once the references showed grid-free continuous ground).
3. **Camera: staged.** The bake ships under the **current top-down camera**; the
   tilting oblique camera of the references ("as if standing on the planet") is a
   **future milestone of its own** — `docs/development/ROADMAP.md` owns when, and
   § The staged end-state below owns what carries forward.
4. **The grid rule: no hex grid on screen.** The grid exists only as interaction
   feedback — a single amber hex on the selected tile, the hover treatment likewise.
5. **Installations are rendered geometry, not glyphs.** Building markers retire from
   the canvas; what stands on a tile is drawn as real-looking structures in the art.

---

## The mechanism — baked terrain chunks

The ground is rendered **once** into per-body chunk textures (render targets on the
existing `SDL_Renderer`; no platform change), then drawn per frame as a handful of
textured quads. Bake cost is off the frame path, which is the whole point: the bake
may run unlimited passes — hillshade, biome brushing, river carving, coastal
shelving — because it happens on world load and on terrain change, not at 60 Hz.

### The bake, pass by pass

Composed per chunk from `tile_component` fields — no new generation work, and the
bake **reads** world state, never writes it (the `world/*` determinism rule is
untouched by construction):

| Pass | Reads | Produces |
|---|---|---|
| **Base ground** | `substrate`, `height` | Continuous material colour, smoothly interpolated between tile centres — no cell boundary is ever drawn |
| **Hillshade relief** | `height` (BL-517's continuous field) | Slope lighting from a fixed sun azimuth — the exaggerated topographic read that carried panel C |
| **Biome brushes** | `cover` × `cover_density` | Authored painterly stamps (forest canopy, scrub, marsh…) scattered by density, hash-seeded from grid coordinates |
| **Water & rivers** | water substrates, `river_edges` + flow | Sea, lakes, and carved river courses with bank treatment |
| **Roads** | `road_level` | The road lattice painted into the ground at tier weight |
| **Installations** | buildings, settlements | § Installations below |
| **Near-future grade** | — (a colour pass) | Desaturation, cool cast, distance haze — **a separable final pass**, tunable without re-authoring any brush |

**Brush placement is hashed from tile grid coordinates**, never screen position — the
established rule (wrap copies of one tile must agree; no crawl under pan).

**Fallback by coverage.** A pass whose brushes are not yet authored bakes from the
current vector-fill logic instead (`ui::terrain_colour` becomes the base-ground
fallback). The app runs and every capture passes with a partial brush set; the vector
ground retires only as coverage arrives.

### Chunks, cache and invalidation

- Chunks are fixed tile rectangles (order 16×16 tiles); pages stay ≤ 4096².
- **Resident set:** full-resolution chunks around the viewport, plus one low-res
  whole-body page for far zoom. Bounded memory; no streaming subsystem.
- **Invalidation is event-driven and narrow:** a build, a road placement, an urban
  transform, a settlement change or a survey reveal dirties the chunks its tiles
  intersect; dirty chunks re-bake off the critical path (budgeted per frame). Terrain
  changes are rare by design, so re-bakes are rare.
- The cylinder wrap draws the same chunk at multiple offsets, exactly as tiles do
  today; the seam-crossing chunk bakes with wrapped neighbour reads.

### The grid rule

**No hex grid renders on the ground.** No 1 px gap, no cell borders, no per-tile
fill boundary — the ground is one continuous surface. The grid surfaces only as
interaction feedback:

- **Selection:** a single hex outline on the selected tile in the house amber
  `#E8A33D` (the treatment confirmed against all five it2 panels).
- **Hover:** the same shape in the highlight convention's hover tint, yielding to
  selection (`highlight.hpp` precedence unchanged).

Tiles remain fully instrumental — hit-testing, placement, deposits, ownership are
tile-keyed exactly as before (Ben, 2026-08-21: tiles are "rendered differently, but
still instrumental unit values"). The province selection outline and the structure
hit-zones (national border corridor) are unaffected as *interaction* geometry; their
visual weight over painterly ground is BL-734's to settle
(ground/chrome layer contract).

### Installations — rendered geometry, no glyphs

**What stands on a tile is drawn as structures in the art**: buildings, settlements
and works render as real-looking painterly geometry stamped in the installation
pass, in the same perspective and light as the ground. The vector building
silhouette, the stacked-tile ring and the settlement skyline glyphs **retire from
the canvas** (the glyph vocabulary survives everywhere else — panels, ledgers,
chrome; [ICONS.md](ICONS.md) is narrowed, not retired).

Consequences the design accepts and answers:

- **Far-zoom legibility lives in the art**, not in a glyph fallback — an installation
  must be authored to read at distance (footprint contrast, smoke, light), the way
  the it3 settlement reads. Ben chose this against a geometry-close/glyphs-far
  ladder, deliberately.
- **Ownership, stack contents and counts** — the three questions the silhouette,
  ring and badge answered — move to hover/selection surfaces and the layer-contract
  work; how much ownership colour touches the ground art is BL-734's
  (ground/chrome layer contract) sharpest open call.
- A structure stamp may **overhang its tile** (chimneys, towers); stamps compose in
  row order like every other pass.

### Ambient animation

Motion is **overlay flipbook, not baked**: the ground bake is static, and animated
passes (water shimmer first; stack smoke with it) draw as looping frame overlays on
top of the baked chunks. The animation clock is **render-side real time** — never sim
state — so ambience continues while paused; under `--verify` the clock is **pinned to
phase 0** and a check advances it explicitly. The near-future grade applies over
animated overlays too, so motion cannot break the grade.

### Level of detail

Two rungs, replacing the per-tile fill LOD for the ground layer:

- **Far zoom** (at and below the whole-grid band): the low-res whole-body page —
  one quad, mip-baked from the full-res bake so the two agree.
- **Play zoom**: the full-res chunk set.

The crossover sits where a full-res texel falls below screen resolution; there is no
per-tile geometry to LOD any more. The existing 7 px vector pivot remains only in
the fallback path.

---

## Art direction and palette

The settled house values this doc consumes (authority for their settlement:
`docs/ui/design/GLOBAL_STYLE_SHEET.md`):

| Role | Value |
|---|---|
| Canvas background | `#0F0F14` — matches the app clear colour |
| Selection / EARNED accent | Amber `#E8A33D` |
| Secondary accent | Saturated cyan `#3FC9E8` |
| Ground | C-F: painterly relief base, near-future grade over it |

**Assets.** Authored raster brushes and structure stamps — the project's first
shipped raster art beyond fonts — live under **`assets/brushes/`**, resolved
cwd-relative like fonts, with a provenance line per sheet in the brush manifest
(Lua, per the data-definitions boundary). Reference renders and prompt logs live in
`docs/ui/design/renders/` and are design material, never shipped.

---

## The staged end-state

The references express a camera this stage does not ship: continuous tilt from
near-orbital to a low oblique, with terrain and structures as true geometry. That is
**a renderer milestone of its own** (the SDL3 GPU door TECH_FOUNDATIONS holds open —
or its 2.5D pre-rendered alternative; the trade-off analysis lives in
`docs/research/CANVAS_RENDERING.md` § The end-state choice). What this stage
guarantees about it:

- **Nothing authored here dead-ends.** Biome brushes, structure stamps, the grade
  recipe and the palette carry into any successor as textures and materials; the
  bake's pass structure is the material definition a 3D ground would consume.
- **The ground is one swappable layer.** Everything above it (chrome, lenses,
  selection) composes onto "a ground layer" — the successor replaces the layer, not
  the canvas.

---

## Verification

- **Visual:** `scripts/verify/ground_bake.lua` (verifier-visual) — baked ground at
  play zoom and far zoom, the grade pass on/off, a river course, an installation
  stamp, selection hex on terrain, an animation overlay frame advanced via the
  pinned clock, and the vector fallback on an unauthored brush key.
- **Headless:** chunk invalidation (a build dirties exactly the intersecting
  chunks; nothing else re-bakes) and brush-manifest integrity are display-free
  checks.
- Captures stay machine-portable: same rasteriser path, bundled cwd-relative assets.

Design owners: BL-732 (ground bake renderer) — the mechanism; BL-733 (biome brush &
structure art pipeline) — the authored assets; BL-734 (ground/chrome layer contract)
— the fate of each analytic channel over painterly ground.
