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

Ruled by Ben, 2026-10-09, after walking the sprint 51 build: the ground is viewed at **one
angle** (22.5°) at every rung, and **zooming does not add detail** — one 96 px/hex master
per body, pre-baked during generation and held in RAM, with every other zoom a downsample
of it (§ Level of detail; § One angle). This supersedes the never-magnify tier ladder and
the stepped tilt.

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

**The master carries feature stamps and sharpened relief** (Ben, 2026-09-01: *"we
should be able to render individual trees, and sharper hills"*). At bake resolutions
≥ 40 px per hex (the 96 px master — and so, downsampled, every level): forest and scrub tiles scatter **individual tree
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

- Chunks are fixed pixel windows (512 px) into one whole-body image per level
  of the master (§ Level of detail), so adjacent chunks are seamless by
  construction. The master's width and height are multiples of 16, so every
  master chunk halves exactly at every level and each level still spans one
  wrap period.
- **Resident set — two tiers of memory:**
  - **System RAM** holds each baked body's master and its mip chain, chunk by
    chunk (~3.8 GB for the 261×121 home body), plus its far page. A **RAM
    budget of 6 GB across bodies** drops the least-recently-visited body's
    master and chain — never the body on screen, never the pre-bake target —
    and keeps its far page. A background bake starts only where its whole
    master fits the budget without dropping anything.
  - **The GPU** holds only the drawn level's chunks in view, a one-chunk ring
    around them, and the same view at the two adjacent levels — an LRU of at
    most 320 textures (≤ 1 MB each), flushed on a body switch. Uploads are
    capped at **8 chunks (≤ 8 MB) a frame** for the view and its ring, nearest
    the centre first, and **2 a frame** for the adjacent levels — spent only
    once the view is whole (the coarser level's whole view, the finer level's
    central half) — so a pan reveals uploaded ground and a rung change finds
    its level already resident. The first view on a body (a body switch, play
    opening) uploads whole in one frame: it is a transition anyway.
  - The **far page** — one direct whole-body bake at 6 px per hex at the one
    angle, baked as 512 px pieces, single-sample — is all a body shows before
    its master covers the view; the vector fallback only before that.
- **All baking runs on a pool of worker threads** (Ben, 2026-09-01 — the
  smoothness ruling; the pool, Ben 2026-10-09 — one worker took 14-22 s to fill
  a zoom step): the pure bake executes against an immutable source snapshot,
  shared read-only by every worker — the bake holds no static or shared
  mutable state, so concurrent bakes are byte-identical to serial ones. The
  render thread only hashes, enqueues, uploads finished buffers and publishes
  the view.
  - **Size and priority:** N = max(1, hardware threads − 2) workers — the
    main/simulation thread and one spare are left free — at below-normal OS
    priority, so a full-viewport fill never starves the simulation or render
    threads.
  - **Order:** one shared queue, popped lowest-priority-value first: far-page
    pieces, then hash sweeps, then the Selection band's neighbourhood page,
    then the drawn body's master chunks under the view (nearest the centre
    first), then the rest of that body, then the pre-bake target, then — once
    play is open — one background body at a time (most-visited, then nearest
    the home body's orbit). The waiting set is capped at 2N and refilled every
    frame, so the queue follows the view.
  - **The mip chain is the worker's:** the worker that bakes a master chunk
    box-downsamples it into its pieces of the 48/24/12/6 levels before
    returning; the render thread only copies the pieces into place.
  - **Results are keyed by job:** each slot records the sequence number of
    its one outstanding job, and only that job's result can land in it, so
    arrival order cannot matter and a result that outlives its body (a drop,
    a new world) lands in nothing.

  **Under `--verify` everything a frame draws is complete before the frame
  returns:** the master chunks under the view bake on the pool and the main
  thread waits for them; every visible texture uploads with no budget; no
  pre-bake or background bake runs — a capture must never race the pool.
  (In a Debug build the home master is ~2 minutes of pool time, paid by the
  first whole-body framing of a run.)
- **Invalidation is content-hashed, per master chunk:** each chunk's hash covers the
  tile fields the bake reads (terrain, height, survey bits) and the installations
  standing on its tiles (building type, recipe identity, stack membership; settlement
  scale and razed state). The drawn body's source snapshot is re-taken every 30 frames —
  and on the next frame after the player places a building; when its whole-body digest
  moves, a sweep job hashes every master chunk against it, and a chunk whose hash moved
  re-bakes in the master and re-derives only its own mip pieces (an urban transform, a
  survey reveal, a build, a demolition, a settlement crossing a scale step). Nothing
  tick-rate enters the hash — staffing, output and ownership do not — so a re-bake
  follows a construction event, never a tick.
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
exactly as the ground does, deterministic and wrap-exact (`ground_bake_check` P13). An authored raster sheet may
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
- **Structures stand up.** At the one oblique angle (§ One angle) a structure bakes as a
  standing sprite like a tree — verticals pre-stretched by 1/cos(tilt), shadow left on
  the ground plane — so the camera squash returns it to true proportion.
- **Under a lens a structure is ground.** It takes the lens wash like the terrain around
  it; a lens is an analytic read, seen at the same one angle as the plain canvas.
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
- **LOD:** below a **20 px drawn radius** (`k_route_lod_radius_px` — between the
  second rung, ~13 px, and the third, ~27 px) a curve is **one stroke**, its two
  halves joined in a single path, with no round joint at the apex, and each half is
  tessellated at **two segments**; at the coarse fill (drawn radius ≤ 7 px) at one —
  its two chords. A curve whose halves fog differently is two strokes, still
  jointless. At these radii a curve spans a dozen pixels and the joint is under a
  pixel wide, so nothing is lost to the eye; six segments a half and the joint fan
  were most of the road layer's vertices, and the wide rungs must hold the 60 fps
  budget (TECH_FOUNDATIONS.md § Target hardware; Ben, 2026-10-09). The third rung
  and up draw the curve unchanged.
- Both are **always-on**, under every lens, and dim with the reach fog and stop at
  the survey mask as roads always have.

### Ambient animation

Motion is **overlay flipbook, not baked**: the ground bake is static, and animated
passes (water shimmer) draw as looping frame overlays on top of the baked chunks. No
animation keys on an installation's state — the art is static (§ Installations). The animation clock is **render-side real time** — never sim
state — so ambience continues while paused; under `--verify` the clock is **pinned to
phase 0** and a check advances it explicitly. The near-future grade applies over
animated overlays too, so motion cannot break the grade.

### Level of detail — one master, every zoom a downsample

**Planetary zoom is STEPPED** (Ben, 2026-09-01, judging the first bake: *"C-F is a
detailed image… we could approximate it with stepped zoom, rather than the continuous
zoom we currently have (2.5D)"*): the player's wheel and keyboard move through a fixed
**×2 ladder** — `kMinZoom × 2^k`, five rungs spanning the old continuous range
(`planetary_zoom_stepped`; verify's `set_zoom` stays free-form so scripted framings
are unaffected). The upper canvas rungs keep continuous zoom.

**Zooming does not add detail** (Ben, 2026-10-09, after walking the sprint 51 build:
*"the current lag is very off putting… zooming doesn't add more detail, arguably the
biggest fix even if we keep a high resolution"*). This supersedes the per-rung bake
tiers of 2026-09-01 and the never-magnify tier chooser of 2026-10-08: a zoom step had
been a fresh, expensive bake, and that bake was the lag.

- **One master per body.** The ground is baked ONCE per body, at **96 px per hex**, at
  the one camera angle (§ One angle), whole-body, in 512 px chunks. Everything the
  player sees at any rung is this image: the close-tier features (trees, crags, strata,
  structures) are always in it and simply grow small with distance.
- **Every other zoom is a downsample.** A **mip chain** — 48, 24, 12 and the 6 px far
  page — is box-downsampled from the master, chunk by chunk, in milliseconds. A rung
  draws the level at or above its drawn radius, minified by at most 2:1, so no step
  shimmers (`SDL_Renderer` has no mipmaps; the chain is ours). Detail never changes
  with zoom; only scale does.
- **The master is held in RAM; the GPU holds what is on screen.** The master and its
  chain live in system memory (about 4 GB for an Earth-sized body); each frame uploads
  the visible chunks of the level in use, within a per-frame upload budget so a pan
  never hitches, and keeps an LRU of uploaded textures.
- **Pre-baked during generation.** The home body's master bakes while the player watches
  the world generate (STARTUP.md), so the first frame of play is final at every zoom.
  Other bodies bake in the background after the home body, nearest and most-visited
  first; a first visit to an unbaked body bakes the area under the view first and the
  rest behind it. A RAM budget drops the masters of the least-recently-visited bodies,
  keeping their far pages.
- **The bake is pooled.** All baking runs on the worker pool (§ Chunks, cache and
  invalidation); the home body's master is about 3,000 chunks (2,975 at 261×121), work
  only a pool can carry. Its cost is a measurement, not an estimate:
  `ground_bake_check --master` times the whole master, surveyed and masked, at 1× and 2×,
  and with each pass switched off in turn.
- **Unsurveyed ground is a fill.** A chunk every tile in reach of which is survey-masked
  is the flat lock colour and nothing else, so it is filled directly rather than resolved
  pixel by pixel — byte-identical (`ground_bake_check` P23). An unsurveyed body's master
  is therefore nearly free, and a first visit to one is final at once.
- **Supersampling is a measured choice, not a rule.** The master bakes at 1× by default:
  the downsampled levels are anti-aliased by the downsample itself, and only the top rung
  — the one rung that reads the master near 1:1 — would gain from a 2× bake, at four
  times the bake cost. A 2× master is taken only if the pool's measured pre-bake still
  fits the target in TECH_FOUNDATIONS.md § Target hardware.
- **The top rung reads the master slightly magnified** at the reference window (~110 px
  drawn from 96: ~1.15×), and more on a 4K-height window (~2.3×). Accepted for the
  prototype's windows (Ben, 2026-10-09: 96 over 48, and over a separate 192 tier).

At the reference 1720×1080 window:

| Zoom rung (drawn hex radius) | Level drawn (px per hex) |
|---|---|
| ~7 px (whole grid) | 12, minified (~1.7:1) |
| ~14 px | 24, minified |
| ~28 px | 48, minified |
| ~55 px | 96 — the master, minified |
| ~110 px | 96 — the master, ~1.15× magnified |

The vector fill remains only as the fallback before a body's far page exists. What the
canvas strokes OVER the ground — washes, roads, lanes, the border rule — has its own
wide-rung level of detail and frame budget: PLANETARY.md § Draw-loop cost model, and
§ Roads and sea lanes above. The chunk images are submitted after those strokes on a
vertex offset of their own, drawn first: that ordering is part of the 60 fps budget, not
a style choice (the same section says why).

### One angle — the 2.5D seam at a single tilt

The planetary ground is viewed at **one fixed oblique angle, 22.5°, at every rung** (Ben,
2026-10-09, replacing the 2026-09-02 stepped tilt of 22.5° at rung 3 and 45° at rung 4):
axonometric — a y-squash by cos(22.5°) ≈ 0.924, no perspective convergence. One angle is
what makes one master possible: a second angle was a second bake.

Two halves make it:

- **The camera is one vertex squash, applied always.** The canvas draws everything in
  flat ground space; a single transform over the map-space vertex range squashes fills,
  strokes, images and labels alike about the canvas centre, and one inverse on the
  cursor keeps every hit test correct. It applies under every lens too, so switching a
  lens never moves the map; the lens key, which is screen chrome drawn on the same list,
  is cut out of the squash. Height displacement is deliberately ignored for hits — a
  few px on mountain tops.
- **The master bakes oblique:** the height field displaces content upward by
  `lift = 0.9·tan(22.5°)` canonical units per unit height, so hills grow real
  silhouettes (a masked re-resolve locks, so a peak truncates at the survey mask rather
  than leaking); trees and structures bake as **standing sprites** — upright, shadow
  left on the ground plane — with their verticals pre-stretched by 1/cos(22.5°) so the
  camera squash returns them to true proportion. The lift stays modest, so the
  projection is monotonic and needs no occlusion handling. Deterministic and
  wrap-exact (`ground_bake_check` P9).


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
