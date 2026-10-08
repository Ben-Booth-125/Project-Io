# Project Io — Canvas Ground Rendering

> **Settles:** by what mechanism the Planetary ground is drawn and when that work
> is done · which art direction the ground is held to, and how the grade relates to
> the terrain under it · whether a hex grid is ever on screen and what the grid
> serves instead · how an installation, a settlement, a mountain and a river appear
> if not as a glyph or a stroke · what moves, what changes with zoom, how sharp each
> rung is held, and how the result is checked.
> **Not here:** what the canvas communicates above the ground (PLANETARY) · the
> ladder and the shared state (CANVASES) · the unsettled style exploration
> (design/GLOBAL_STYLE_SHEET).
> **Confused with:** PLANETARY.md, design/GLOBAL_STYLE_SHEET.md, ICONS.md.

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

Ruled by Ben, 2026-10-08, in the sprint 51 form (the visibility pass): the installation pass
is **procedural now, authored raster later on the same stamp seam**; a stacked tile stands a
**cluster**; the art is **static** — running state is the Selection element's; settlements,
dramatic landforms and rivers leave the canvas's vector layer for the bake too; and the ground
is **never magnified** at any rung (§ Level of detail).

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
| **Landform relief** | `landform`, `height` | The dramatic landforms' own forms — massif and ridge, canyon cut, crater bowl, rift fissure (§ Mountains, rivers and terrain variety) |
| **Water & rivers** | water substrates, `river_edges` + flow | Sea, lakes, and carved, curved river courses that widen downstream, with bank treatment (§ Mountains, rivers and terrain variety) |
| **Roads and sea lanes** | `road_level`, `lane_level` | The road lattice and the stamped lanes, smooth curves along their own tiles at the named tier widths (§ Roads and sea lanes) |
| **Installations** | buildings (type, active recipe, stack membership), settlements (scale, razed) | Structure stamps — § Installations below |
| **Near-future grade** | — (a colour pass) | Desaturation, cool cast, distance haze — **a separable final pass**, tunable without re-authoring any brush |

**Brush placement is hashed from tile grid coordinates**, never screen position — the
established rule (wrap copies of one tile must agree; no crawl under pan).

**The close tiers carry feature stamps and sharpened relief** (Ben, 2026-09-01: *"we
should be able to render individual trees, and sharper hills"*). At bake resolutions
≥ 40 px per hex (the 48/96 tiers): forest and scrub tiles scatter **individual tree
canopies** — hash-positioned, density-counted, NW-lit with an SE drop shadow, drawn
before the grade so they take it exactly as the ground does, and never painted over
the survey lock fill; mountain-biased detail noise folds toward a **ridged** variant
whose creases shade as ridge lines; and steep slopes shed cover colour toward bare
**rock**. All deterministic and wrap-exact (`ground_bake_check` P8). These are the
procedural stand-ins the authored brushes of the asset pipeline will replace or
augment, on the same stamp seam.

**Fallback by coverage.** A pass whose brushes are not yet authored bakes from the
current vector-fill logic instead (`ui::terrain_colour` becomes the base-ground
fallback). The app runs and every capture passes with a partial brush set; the vector
ground retires only as coverage arrives.

### Chunks, cache and invalidation

- Chunks are fixed pixel windows (512 px) into one whole-body bake space per
  tier, so adjacent chunks are seamless by construction; pages stay ≤ 4096².
- **Resident set:** the ACTIVE zoom tier's chunks around the viewport
  (LRU-capped per tier), plus one low-res whole-body far page. Bounded memory;
  no streaming subsystem.
- **All baking runs on a worker thread** (Ben, 2026-09-01 — the smoothness
  ruling): the pure bake executes against an immutable source snapshot; the
  render thread only hashes, enqueues, uploads finished buffers and publishes
  the view. A generation counter discards results that outlive their source or
  body. Until a chunk lands, the far page carries the frame; until the far page
  lands (a body switch), the vector fallback does. **Under `--verify` every
  bake is synchronous on the main thread** — a capture must never race a
  worker.
- **Invalidation is content-hashed:** each chunk's job carries a hash of the
  tile fields the bake reads (terrain, height, survey bits) and of the installations
  standing on its tiles (building type, recipe identity, stack membership; settlement
  scale and razed state); an urban transform, a survey reveal, a build, a demolition or
  a settlement crossing a scale step changes the hash and the chunk re-bakes on its next
  sweep. Nothing tick-rate enters the hash — staffing, output and ownership do not — so a
  re-bake follows a construction event, never a tick.
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
silhouette, the stacked-tile ring, the `+N` count badge, the corp emblem tag and the
settlement skyline and ruin glyphs **retire from the canvas** (the glyph vocabulary
survives everywhere else — panels, ledgers, chrome; [ICONS.md](ICONS.md) is narrowed,
not retired).

**Procedural now, raster later, one seam** (Ben, 2026-10-08). The structures are
procedural stamps of the same kind as the close tiers' tree canopies: hash-placed from
grid coordinates, NW-lit with an SE drop shadow, drawn before the grade so they take it
exactly as the ground does, deterministic and wrap-exact (`ground_bake_check` P10). An authored raster sheet may
later replace a type's procedural stamp **on the same stamp seam** — a stamp is keyed by
what it depicts, and the bake asks the seam for it — so nothing authored procedurally is
thrown away when art arrives.

**One structure per type, read by silhouette.** Each placeable building type carries its
own form, distinct at its tier's scale — an extraction head-frame, a processing hall with
stacks, a port's quay, a well's housing, a wharf's jetty — and a processing facility's
form keys on its active recipe's family the way the retired glyph did, so *what is made
here* reads before a hover. The type is public (DISCOVERY.md, the competitor-visibility
rule), so a rival's structure draws exactly as the player's.

**A stacked tile stands a cluster** (Ben, 2026-10-08). One structure per **stack** —
`(type, target)`, `placement_rules::stack_members` — placed by a fixed in-tile layout,
the dominant (lowest-id) stack largest and in front, **up to three**; a fourth stack and
beyond adds nothing to the ground. Two buildings of one stack are one structure: the art
says *what stands here*; the Selection element's Production section says *how many*
(SELECTION.md § The tile element's layout).

**Settlements are structures too.** A population centre bakes as a settlement whose
footprint and height step with its scale (Outpost → Metropolis), and a razed centre as a
ruin. A small centre may vanish into the ground at the far page where the old density dot
stood, but a scale ≥ 3 centre must read as a city at every rung — the obligation the
skyline LOD ladder carried, which the art now takes over.

**The art is static** (Ben, 2026-10-08). No smoke, light, flag or animation distinguishes
a running building from an idle, starved or mothballed one; running state is read in the
Selection element alone. Two reasons: a state cue on the ground is a glyph by another
name, and a stamp keyed on tick-rate state would re-bake chunks every tick (§ Chunks,
cache and invalidation). **Under construction is the one exception** — a site bakes as
scaffolding, because construction start and completion are events, not ticks.

**Ownership never touches the ground art.** This settles what was the ground/chrome layer
contract's sharpest open call: a structure carries no owner colour. Ownership is read from
the hover card, the Selection element, the always-on player footprint outline, and the
Corporation and Company lenses' owner multi-select ([LENSES.md](LENSES.md)).

Consequences the design accepts and answers:

- **Far-zoom legibility lives in the art**, not in a glyph fallback — an installation
  must be authored to read at distance (footprint contrast, cleared ground, a road
  stub), the way the it3 settlement reads. Ben chose this against a
  geometry-close/glyphs-far ladder, deliberately.
- **The tilted rungs stand structures up.** On the oblique tiers a structure bakes as a
  standing sprite like a tree — verticals pre-stretched by 1/cos(tilt), shadow left on
  the ground plane — so the camera squash returns it to true proportion.
- **Under a lens a structure is ground.** It takes the lens wash like the terrain around
  it; a lens is an analytic read and stays flat.
- A structure stamp may **overhang its tile** (chimneys, towers); stamps compose in
  row order like every other pass. **Hit-testing is unchanged**: the hex, not the stamp,
  is what a press lands on (SELECTION.md § Multi-building tiles).

### Mountains, rivers and terrain variety

Ben, 2026-10-08: mountains and rivers **blend into the render**, with **more tile sets**.
Both were vector chrome drawn over the bake — a stroke-only landform glyph on the hex
centre, a straight river line from tile centre to tile centre with downstream chevrons —
and both read as annotation laid on a painting. They move into the bake.

- **Dramatic landforms bake their own forms.** Mountain bakes as massif and ridge (the
  ridged relief the close tiers already fold toward, now carrying the read at every
  tier); canyon as a cut between paired rims; crater as a raised-rim bowl; rift as a dark
  fissure. A contiguous run bakes as **one** form — a range, one cut, one fissure — each
  tile baking its half of the shared edge so the halves meet at the midpoint
  (PLANETARY.md § Terrain channels). The obligation the glyph carried is the form's now:
  these are the ×1.3-or-worse movement tiles, so the form must read at the far page. The
  hover card keeps naming the landform and its cost.
- **Rivers are carved courses.** A river bakes as water along its `river_edges` chain,
  drawn as a **smooth curve** through the chain by the quadratic B-spline rule roads use
  (§ Roads and sea lanes), **widening downstream** with accumulated flow, with bank
  shelving and a wet margin blended into the ground either side. The canvas stroke and
  its chevrons retire: the width gradient says which way the water flows. Province
  borders drawn against a river edge keep following it (TILES.md, edge features).
- **More tile sets — variant families.** Every terrain family — a substrate × cover pair,
  and each dramatic landform — carries **several procedural variants** (different noise
  seeds, brush scatter, rock and erosion patterns), and each tile picks one by a hash of
  its grid coordinates, so neighbouring tiles of one terrain do not repeat and a wide
  plain does not read as wallpaper. Variants **cross-fade** across the tile boundary the
  way the base ground already interpolates between tile centres, so no variant edge is
  ever drawn (the grid rule). The count per family is set by eye against captures, not
  here; an authored raster set replaces a family's procedural variants on the same seam
  structures use.

### Roads and sea lanes — smooth curves on their own tiles

Ben, 2026-10-03, playing the build: *"render [roads] as curves rather than lines,
and make them thinner"*; *"sea lanes should always go over ocean, never over
ground"*. Roads (`road_level`) and sea lanes (`lane_level`, [LOGISTICS.md](../economy/LOGISTICS.md)
§ 4b) are both tile fields on the four-cardinal grid the traversal walk uses, and
both draw by one rule:

- **A route is drawn along its own tiles, never between its ends.** A lane is drawn
  only on the sea tiles it was stamped on, so it can never cross land; the wizard's
  lapse draws a lane along the same water-only walk the stamp lays, port to port
  ([STARTUP.md](STARTUP.md) § Round 5, "The lane").
- **The line is a smooth curve through the tile chain.** A tile joined to two
  neighbours draws one quadratic from one shared-edge midpoint to the other, with
  its own centre as the control point: consecutive tiles meet tangent-continuous at
  the midpoints (the quadratic B-spline of the tile-centre chain), and each curve
  stays inside its own tile's corner. A junction pairs its neighbours into
  through-curves, most opposite first; an end, or a three-way junction's odd branch,
  is a straight spoke, so a fork still reads as a fork. Two lanes laid on adjacent
  rows touch tile to tile; a link between two tiles that both run straight through
  along the other axis is a **rung**, not a route, and a lane does not draw it (a
  road lattice's rungs are real roads and are drawn). On the lapse's small map
  the walk's staircase is first string-pulled over the water, then corner-cut, every
  cut checked against the sea.
- **One named width per tier**, as a fraction of the drawn hex radius (floored at a
  10 px radius so the tiers stay apart on the whole-grid view): **Track 0.06, Road
  0.09, Highway 0.12** — the 1 : 1.5 : 2 ladder — and the **sea lane 0.10**, told
  from the road ladder by its sea blue rather than its weight.
- **LOD:** at the coarse fill (drawn radius ≤ 7 px) a curve is drawn as its two
  chords; the shape is invisible at a few pixels a tile.
- Both are **always-on**, under every lens, and dim with the reach fog and stop at
  the survey mask as roads always have.

### Ambient animation

Motion is **overlay flipbook, not baked**: the ground bake is static, and animated
passes (water shimmer) draw as looping frame overlays on top of the baked chunks. No
animation keys on an installation's state — the art is static (§ Installations). The animation clock is **render-side real time** — never sim
state — so ambience continues while paused; under `--verify` the clock is **pinned to
phase 0** and a check advances it explicitly. The near-future grade applies over
animated overlays too, so motion cannot break the grade.

### Level of detail — the stepped zoom pairing

**Planetary zoom is STEPPED** (Ben, 2026-09-01, judging the first bake: *"C-F is a
detailed image… we could approximate it with stepped zoom, rather than the continuous
zoom we currently have (2.5D)"*): the player's wheel and keyboard move through a fixed
**×2 ladder** — `kMinZoom × 2^k`, five rungs spanning the old continuous range
(`planetary_zoom_stepped`; verify's `set_zoom` stays free-form so scripted framings
are unaffected). The upper canvas rungs keep continuous zoom.

Each zoom rung pairs with a **bake tier**, so the ground is near-1:1 texels at every
step — the stepped ladder's whole point. "Near", not exact: the drawn hex radius is
**fit-derived** (window height over grid rows, then the rung's ×2 factor), so where a
rung lands relative to its tier moves with the window.

**The ground is never magnified** (Ben, 2026-10-08, the sprint 51 form: sharpen the
tiles across all zooms). The tier chooser takes the **smallest tier at or above the
drawn radius** — no magnification headroom — and draws it **minified**, never past 2:1
(the ×2 spacing; `SDL_Renderer` has no mipmaps, so that bound is what keeps a step
transition shimmer-free). Minification under linear filtering softens nothing;
magnification is what read as blur, at the 4–14% the earlier 1.2× headroom allowed at
the reference window. While a tier's chunks fill, the stand-in is the next tier down
where it is resident, and the far page only where nothing closer is.

**Every tier is supersampled.** A tier bakes at **2×** its nominal pixels per hex and is
box-downsampled to the nominal size before upload, so stamp edges, ridge creases and
river banks are anti-aliased in the bake rather than stair-stepped. The cost is bake
time (×4 pixels through the worker), not frame time or resident memory, which see only
the downsampled texture; the bake's own unsharp pass is re-tuned against the
supersampled result rather than stacked on it.

At the reference 1720×1080 window:

| Zoom rung (drawn hex radius, reference window) | Bake tier (px per hex circumradius) |
|---|---|
| ~6 px (whole grid) | The far page, 6 — whole-body, one texture (drawn at or below 1:1) |
| ~13 px | 24 — chunked, minified |
| ~27 px | 48 — chunked, minified; the close-grain octave joins the bake |
| ~55 px | 96 — chunked, minified, close-grain |
| ~110 px | 192 — chunked, minified, close-grain |

The 12 px tier stays in the ladder for windows where a rung lands at or under 12 px.
**The 192 px tier** exists so the top rung is never magnified at the reference window or
a taller one, up to a 4K-height canvas. It is chunked like every tier and resident only
while its rung is active, so its memory cost is the viewport's chunks, not the body's. The existing 7 px vector pivot remains only in
the fallback path.

### The stepped tilt — the 2.5D seam, taken

The top two rungs **view the land obliquely** (Ben, 2026-09-02, from the reference
mock: *"a stepped tilt… the land at roughly 45 degrees at max zoom"*): rung 3 at
22.5°, rung 4 at 45°, axonometric — a y-squash by cos(tilt), no perspective
convergence — snapping with the zoom step. Tilt is a **pure function of zoom**
(`planetary_tilt_sy`; thresholds at the rung midpoints), so verify's free-form zooms
stay deterministic, and it applies on the **plain canvas only**: a lens is an
analytic read and stays flat.

Two halves make it:

- **The camera is one vertex squash.** The canvas draws everything in flat ground
  space; a single transform over the map-space vertex range squashes fills, strokes,
  images and labels alike about the canvas centre, and one inverse on the cursor
  keeps every hit test correct. Height displacement is deliberately ignored for
  hits — a few px on mountain tops.
- **The tilted rungs bake OBLIQUE tiers** (a tier is keyed by resolution *and*
  tilt): the height field displaces content upward by `lift = 0.9·tan(tilt)`
  canonical units per unit height, so hills grow real silhouettes (a masked
  re-resolve locks, so a peak truncates at the survey mask rather than leaking);
  trees bake as **standing sprites** — trunk, upright canopy, shadow left on the
  ground plane — with their verticals pre-stretched by 1/cos(tilt) so the camera
  squash returns them to true proportion. The lift stays modest, so the projection
  is monotonic and needs no occlusion handling.

The far page and the low tiers stay flat; while a tilted tier fills, the squashed
flat bake stands in — geometry aligns, only the relief displacement arrives with the
chunks. Deterministic and wrap-exact (`ground_bake_check` P9).

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
— the fate of each analytic channel over painterly ground. The 2026-10-08 visibility
pass: BL-1241 (structures baked) — § Installations; BL-1242 (landforms and rivers baked)
and BL-1243 (terrain variant families) — § Mountains, rivers and terrain variety; BL-1244
(never magnify the ground) — § Level of detail.
