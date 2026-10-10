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
angle** (22.5°) at every rung, and **zooming does not add detail** — one 128 px/hex master
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
| **Base ground** | `substrate`, `height` | Each tile's own material colour and texture across its body, blending into a neighbour only in a narrow band at the shared edge (§ Tiles hold their own ground) |
| **Border sets** | the terrain family on each side of a shared edge | A natural transition along every edge between DIFFERENT families — forest fringe, rock lip, shore shelf, field edge (§ Tiles hold their own ground) |
| **Hillshade relief** | `height` (BL-517's continuous field) | Slope lighting from a fixed sun azimuth — the exaggerated topographic read that carried panel C |
| **Biome brushes** | `cover` × `cover_density` | Authored painterly stamps (forest canopy, scrub, marsh…) scattered by density, hash-seeded from grid coordinates |
| **Landform relief** | `landform`, `height` | The dramatic landforms' own forms — massif and ridge, canyon cut, crater bowl, rift fissure (§ Mountains, rivers and terrain variety) |
| **Water & rivers** | water substrates, `river_edges` + flow | Sea, lakes, and carved, curved river courses that widen downstream, with bank treatment (§ Mountains, rivers and terrain variety) |
| **Roads and sea lanes** | `road_level`, `lane_level`, the installations' road plan | The road lattice and the stamped lanes, smooth curves along their own tiles at the named tier widths, a surface per tier — painted in the installation pass's slot, after its ground parts and before its shadows and standing structures (§ Roads and sea lanes) |
| **Installations** | buildings (type, active recipe, stack membership), settlements (scale, razed) | Structure stamps — § Installations below |
| **Cast shadows** | `height`, the relief fields, the landform forms | The low NW sun's shadow over everything the ground carries, behind ridges and hills (§ Art direction and palette) |
| **Near-future grade** | — (a colour pass) | Desaturation, an S-curve on luminance, a split tone (cool shadows, warm lights), haze — **a separable final pass**, tunable without re-authoring any brush |

**Brush placement is hashed from tile grid coordinates**, never screen position — the
established rule (wrap copies of one tile must agree; no crawl under pan).

**The master carries feature stamps and sharpened relief** (Ben, 2026-09-01: *"we
should be able to render individual trees, and sharper hills"*). At bake resolutions
≥ 40 px per hex (the 128 px master — and so, downsampled, every level): forest and scrub tiles scatter **individual tree
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
    chunk (~6.7 GB for the 261×121 home body at 128 px per hex), plus its far page. **The home body's master is pinned in RAM** and never dropped (Ben, 2026-10-10: at
    128 px a single visit elsewhere had evicted it, and coming home re-baked for ~71 s).
    Other bodies share the rest of a **RAM budget that scales with the machine** — the ground as a
    whole may hold **45% of installed RAM**, home included, so other bodies get what the pinned
    home leaves of it (Ben, 2026-10-10; ~0.5 GB on a 16 GB PC, so other bodies there stream
    from the disk cache on each visit, ~7.7 GB on 32 GB): when a body's master must
    leave RAM it **spills to a disk cache** — its finished chunks written to local disk —
    and streams back from there on the next visit, in seconds from an SSD instead of a
    re-bake; only chunks whose content hash moved meanwhile re-bake. The far page stays
    resident. The disk cache is a cache: deleting it costs a re-bake, never correctness. It lives
    in the user's local app data (`%LOCALAPPDATA%\ProjectIo\ground_cache` on Windows), not beside
    the saves — a regenerable per-machine cache does not belong in a folder that may sync. One
    file per master chunk (its master piece, its mip pieces and the content hash it was baked
    against), written by one background writer thread as a temp file renamed into place, so a
    crash never leaves a half chunk that loads; each file is checked on load and a bad one
    re-bakes. Files are packed losslessly (the alpha plane run-length coded, RGB raw: about 0.75
    of raw). The cache is scoped to the run that wrote it — deleted at exit and on a new world —
    and capped at 32 GB, the least recently visited body's files evicted first. A background bake starts only where its whole
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
  first whole-body framing of a run.) So the master **never fills by itself**
  under `--verify`: it holds what the views so far have covered. A script
  that needs the whole master asks for it (`verify.ground_complete_master`,
  which bakes the rest and returns once the master is complete and current,
  or fails); polling the ready count waits forever. `IO_GROUND_BENCH=1` runs
  the live pool path under `--verify` instead, where the master fills in the
  background from any state.
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
- **A building's change re-bakes a window around it, not its whole chunk** (Ben,
  2026-10-09: the settle adds about 1,300 installations at once, and re-baking every
  chunk they touch whole did not fit the wait after the seat). The chunk hash has two
  halves, **terrain** and **installations**. When only the installation half moved, the
  chunk re-bakes only the union of small windows around each installation that changed,
  blits them into the stored chunk, and re-derives only those windows' mip pieces. The
  window rule:
  - **A window covers a changed installation's whole reach** — its tile plus every pixel
    its structures can touch (overhang, standing height, the SE shadow, the pad, the
    trees its cleared ground removes or restores) — padded for the post passes that read
    across a pixel (the unsharp mask, anti-aliased rims).
  - It is **aligned outward to 16 px**, so every mip level's piece of it is whole pixels
    and derives from the window alone, and clipped to the chunk. A structure that
    straddles chunks, or the wrap seam, is a window in each chunk it reaches.
  - Overlapping or touching windows **merge** into their bounding box where that box
    costs no more pixels than the two apart; otherwise both stand, and their overlap
    bakes twice, identically (a road crossing a chunk is a chain of windows, never the
    chunk's whole bounding box).
  - Past **40 % of the chunk**, or on **any terrain change**, the chunk re-bakes whole.
  This rests on the bake being **window-invariant**: any window bakes byte-identical to
  the same pixels of a larger bake, structures, trees, post passes and all
  (`ground_bake_check` P24, with the mip pieces P25).
- **Before play there is no cadence.** While a generation round's worker is moving the
  world forward, nothing reads that world: the pre-bake target's source snapshot is
  re-taken only at a **round boundary** — a round's world landed and owned by the main
  thread — and once more at adoption (STARTUP.md § Handoff). The same sweep then re-bakes
  only the chunks that boundary moved. The home body is snapshotted as play shows it,
  fully surveyed, though the wizard's worlds carry no survey state until the finish.
- The cylinder wrap draws the same chunk at multiple offsets, exactly as tiles do
  today; the seam-crossing chunk bakes with wrapped neighbour reads.

### The grid rule

**No hex grid renders on the ground** — with one amendment (Ben, 2026-10-09, below). No
1 px gap, no cell borders, no per-tile fill boundary is BAKED into the ground. The grid
surfaces as interaction feedback:

- **Selection:** a single hex outline on the selected tile in the house amber
  `#E8A33D` (the treatment confirmed against all five it2 panels).
- **Hover:** the same shape in the highlight convention's hover tint, yielding to
  selection (`highlight.hpp` precedence unchanged).
- **The close-zoom seam (amended, Ben 2026-10-09):** at the two closest rungs only, a
  **faint seam** is drawn between every pair of neighbouring tiles — a thin line in a
  darkened tone of the ground under it, never a colour of its own — so a player can see
  where one tile of grass ends and the next begins. It is drawn over the ground at those
  rungs, not baked: the master is one image for every zoom, and a baked seam would darken
  the far zooms where it would read as a grid. Below the close rungs the seam is absent
  and the ground is continuous, as before. **The values:** drawn where the drawn hex
  radius is **at least 40 px** (between rung 2, ~27 px, and rung 3, ~55 px, at the
  reference 1720×1080 window); a **1 px anti-aliased black stroke at 16% alpha**, so it is
  exactly the ground beneath darkened by a sixth and has no hue of its own. It lies over
  the ground and under every lens wash and stroke, one segment per shared edge.

Tiles remain fully instrumental — hit-testing, placement, deposits, ownership are
tile-keyed exactly as before (Ben, 2026-08-21: tiles are "rendered differently, but
still instrumental unit values"). The province selection outline and the structure
hit-zones (national border corridor) are unaffected as *interaction* geometry; their
visual weight over painterly ground is BL-734's to settle
(ground/chrome layer contract).

### Tiles hold their own ground

Ben, 2026-10-09, after walking the sprint 51 build: *"there's still quite a blur over each
tile, and it can be hard to see where one tile begins and ends."* The ground was
interpolated between tile centres by design, and the terrain variants widened that blend to
about a tile and a half, so neighbouring tiles melted together. Three changes answer it:

- **Per-tile tone stays** (Ben, 2026-10-10, confirming after the it3 C-F review — the
  references show none, but the honeycomb tone is wanted). The palette moves toward the
  target (§ Art direction and palette) while each tile keeps its own tone:
- **Each tile keeps its own ground.** A tile's body carries its own material colour, its
  own variant and its own texture undiluted; it blends into a neighbour only in a **narrow
  band at the shared edge** — **0.13 canonical units each side of it**, 15% of the hex's
  0.866 inradius, the two tiles a 50/50 mix on the edge itself. The variant cross-fade
  (§ Mountains, rivers and terrain variety) narrows to the same band, the dramatic forms'
  variants included. The material edge sits on the hex — frayed by at most 0.07 — so a tile's
  body is where its seam is drawn; the land|water boundary keeps the full domain warp, so
  coasts stay organic. Terrain *shape* (height, slope, the oblique lift) is continuous ground,
  not material, and keeps its smooth interpolation; water keeps its wide blend too, so
  shallows never tile into hexes. The edge is still never a drawn line between different
  terrains — that is the border sets' job, and the cover-boundary ink line retires with them.
- **Border sets between different terrains.** Every edge between two DIFFERENT terrain
  families bakes a natural **transition set** chosen by the pair: a forest edge as a
  fringe of scattered trees and undergrowth stepping into the open ground; rock against
  soil as a lip of broken scree; ground against water as a shore shelf (a pale wet margin
  and shallows); cultivated or grassland against another family as a field edge (a hedge or
  furrow line). The pair decides the set, so the transition reads as terrain and the edge
  stays crisp without an outline. Same-family edges carry no set; the close-zoom seam
  (§ The grid rule) marks them at the close rungs. The families group into border classes,
  ranked; the higher-ranked class of a pair is the **source** and bakes its own set into
  the other, the **receiving** tile:

  | Rank | Class (variant families) | Its set, baked into the lower-ranked neighbour |
  |---|---|---|
  | 1 | forest | **Forest fringe** — trees and dark undergrowth stepping out, thinning over ~0.5 |
  | 2 | scrub | **Scrub fringe** — bushes and undergrowth stepping out, over ~0.3 |
  | 3 | rock (bare, volcanic) | **Scree lip** — broken stones and gravel spilling onto the soil, downhill only |
  | 4 | grass (cultivated and grassland) | **Field edge** — a hedge or a furrowed headland on the grass side |
  | 5 | marsh | **Reed fringe** — damp ground and reed flecks |
  | 6 | sand (dunes, salt, regolith) | **Drift lip** — tongues of pale sand |
  | 7 | snow and ice | **Snow drift** — white patches |
  | 8 | urban | none: its neighbours' sets carry its edges |

  Two families of one class (bare against volcanic) carry no set. **Any land against water**
  bakes the **shore shelf** — a pale wet margin darkening to a waterline on the land, paling
  shallows and a thin foam line on the water — on the land|water boundary itself. Scattered
  items exist by a hash of their own lattice point against the density its own tile's edges
  give it, gated along the edge by a low-frequency field, so a tile ringed by its source
  reads as ground, never as a dotted outline.
- **Crisper texture inside a tile** (Ben, 2026-10-09: the ground read softer than the
  buildings). Relief shading is sharper, the fine grain stronger, and relief creases and the
  family patterns carry a light **contact ink** of their own, the way structures do, so the
  ground holds the same crispness as what stands on it.
- **More texture per family.** Each terrain family carries finer, higher-contrast grain
  and its own **pattern**: furrows on cultivated ground, scree on rock, ripples on dunes,
  tussocks on grass and scrub, with forest read through its canopy as now. Grass is both
  cultivated and grassland: half its variants are furrowed fields (one furrow direction per
  tile), the rest tussock sward. Patterns are procedural on the stamp seam, hash-placed from
  grid coordinates, keyed on the nominal scale like every pass (close rungs only — the far
  levels are downsamples, so they cannot alias). **The master bakes at 2×** where the pre-bake still fits its
  budget (TECH_FOUNDATIONS.md § Target hardware); otherwise at 1×.

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
  plain does not read as wallpaper. Variants **cross-fade** across the tile boundary within
  the base ground's edge band (§ Tiles hold their own ground), so no variant edge is ever
  drawn (the grid rule). The count per family is set by eye against captures, not
  here; an authored raster set replaces a family's procedural variants on the same seam
  structures use.

### Roads and sea lanes — smooth curves on their own tiles

**Roads are part of the ground's tile sets, not threads laid on it** (Ben, 2026-10-10, walking
the C-F build: *"various tile sets with roads, highways, and railroads on them, so the roads don't
appear just painted on top … roads curving around hills / buildings, and connecting at various
points on the tile edge, not just the centre"*). This revises the geometry rule below:

- **A road crosses a tile edge at its own point, not the midpoint.** Each shared edge a road
  crosses carries a **crossing point** placed by a hash of the edge (within the inner 70% of the
  edge), computed identically from both tiles so the road is continuous; a road of a higher tier
  and a lower one crossing the same edge take separate points.
- **Inside the tile the road finds its way.** The path from crossing to crossing (or to a
  junction, or to a building's forecourt) is a smooth curve that **bends around higher ground** —
  it keeps to the lower side of the tile's relief rather than climbing over a hill — and **around
  the tile's structures** (the shared road plan). It passes through the centre only where the
  ground lets it.
- **Each terrain family carries its own road treatment** — the road's tile set: on a slope the
  road is **cut into the hillside** with a shaded bank on the uphill side; across wet or low ground
  it runs on a low **embankment**; through forest it runs in a **cleared corridor**; on open
  ground a verge of worn grass; in a town it is the street. The road reads as built into the land,
  in the same light and grade.
- **Rail** follows the same routing with **gentler curvature** (a railway cannot turn as tightly
  as a road) and its own treatment — ballast, sleepers, twin rails, cuttings and embankments
  deeper than a road's — painted wherever the world lays the rail rung (BL-1255 rail rung).
- **No drawn road network on the plain canvas at any rung** (Ben, 2026-10-10: roads drew too much
  attention when zoomed out, because the drawn web kept a minimum width). Roads are the painted
  ground at every zoom and shrink with it like the ground; **the network as logistics is read on
  the Throughput lens** (LENSES.md § Throughput lens).

**Roads and sea lanes are painted into the ground** (Ben, 2026-10-09, after walking the
sprint 51 build: *"roads go over buildings, rather than being painted as an optional part of
the building tile sets. Roads are also so simplified, we want to paint a texture on roads"*).
The geometry rule below — a route along its own tiles, a smooth curve through the chain, one
width per tier — is unchanged; what changes is that the route is now a **pass in the bake**,
painted into the master after the base ground and border sets and before structures, so it
takes the light, the grade and the lens wash like the ground does:

- **Each tier has its own surface**, read by colour and value at a thread's width (a few
  master pixels — detail finer than that aliases, so a surface carries none). **Track:** pale
  packed dirt, a little translucent, its edge worn soft. **Road:** paler, greyer gravel with a
  faint verge. **Highway:** a slightly darker asphalt between pale shoulders, with a painted
  centre line only where it can resolve (a surface at least 8 nominal pixels across).
  **Rail:** a ballast bed with sleepers and twin rails — painted on the
  rail rung of the road ladder (INDUSTRIALISATION.md, the rail sink), wherever the world lays
  it; its bed is **0.10** wide. Textures are procedural, hash-placed along
  the curve, nominal-keyed, wrap-exact. A dash pattern (the centre line, the sleepers) runs a
  whole number of dashes along each half of a curve, pinned at the shared-edge midpoint, so it
  meets its neighbour tile's without a break.
- **A road meets a built tile through the building's set.** Each structure form carries a
  **roaded variant**: where a road enters a built tile it arrives at that structure's
  forecourt, yard or loading apron; a road that continues through the tile **bends around the
  cluster** along the tile's free side. A road is never painted across a roof or a pad.
- **One plan, both passes.** How a road meets a built tile is decided once, by a plan both the
  route pass and the installation pass read, so they cannot disagree. A tile standing works
  (stack structures, no settlement) with a road link is **roaded**: its cluster and pad step
  **0.30** toward the tile's **free side** — of 24 headings, the one farthest by angle from
  every road link, north on a tie, so the road runs past the cluster's front — and shrink to
  **0.80 / 0.70 / 0.60** for one / two / three or more links; a through-road **bows** away
  from it (a sin² bow, which leaves its ends and their tangents where they were, so it still
  meets its neighbours smoothly at the midpoints) until the road and its verges clear the
  cluster; a road that ends here ends at a **forecourt apron** standing just outside the
  cluster on the road side — packed dirt for extraction, concrete for works that load and
  ship, paving otherwise — and a short spur joins a through-road to it. A **town's** tile is
  not re-planned: the road runs through as its street, and the town's blocks keep off it.
- **The route pass paints in the installation pass's slot** — after its ground parts (pads,
  yards, aprons, a town's paving), before its shadows and standing structures — so a road lies
  on the yard it arrives at and on a town's paving, structures' shadows fall across it, and no
  roof is ever under it. No tree stands on a road or its verges; a tree whose canopy stands
  over one is drawn after the road, so it hides the road behind it.
- **Sea lanes are a faint wake** painted on the water — a pale, broken trail along the lane's
  sea tiles — not a stroke over it.
- **The drawn network is the Throughput lens's** (the bullet above): on the plain canvas,
  over baked ground, roads and lanes are painted only, at every rung. A tile whose ground is
  not yet baked (the vector fallback) has no painted road, so it keeps the drawn network.
- A built or upgraded road dirties only the window around its tiles (§ Chunks, cache and
  invalidation, the partial re-bake), and the survey mask and the reach fog treat a painted
  road as ground.

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
- **One named width per tier**, as a fraction of the hex radius: **Track 0.015, Road
  0.022, Highway 0.03** — thin pale threads, as in the references (Ben, 2026-10-10: roads
  about 2-3% of a hex; they were 0.06 / 0.09 / 0.12) — and the **sea lane 0.025**. On the
  ground a road is a detail of the land, not the logistics map: **the network is read
  through a lens** (LENSES.md § Throughput lens draws the road network at full weight, at
  the old 1 : 1.5 : 2 weights).
- **LOD** (the drawn curve — the Throughput lens's network): below a **20 px drawn radius** (`k_route_lod_radius_px` — between the
  second rung, ~13 px, and the third, ~27 px) a curve is **one stroke**, its two
  halves joined in a single path, with no round joint at the apex, and each half is
  tessellated at **two segments**; at the coarse fill (drawn radius ≤ 7 px) at one —
  its two chords. A curve whose halves fog differently is two strokes, still
  jointless. At these radii a curve spans a dozen pixels and the joint is under a
  pixel wide, so nothing is lost to the eye; six segments a half and the joint fan
  were most of the road layer's vertices, and the wide rungs must hold the 60 fps
  budget (TECH_FOUNDATIONS.md § Target hardware; Ben, 2026-10-09). The third rung
  and up draw the curve unchanged.
- Both are **always-on** as ground — painted, under every lens, which washes over them —
  and dim with the reach fog and stop at the survey mask as roads always have.

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

- **One master per body.** The ground is baked ONCE per body, at **128 px per hex** (Ben,
  2026-10-10: "simply up the resolution in each tile"; it was 96), at
  the one camera angle (§ One angle), whole-body, in 512 px chunks. Everything the
  player sees at any rung is this image: the close-tier features (trees, crags, strata,
  structures) are always in it and simply grow small with distance.
- **Every other zoom is a downsample.** A **mip chain** — 64, 32, 16 and 8 px per hex
  — is box-downsampled from the master, chunk by chunk, in milliseconds. A rung
  draws the level at or above its drawn radius, minified by at most 2:1, so no step
  shimmers (`SDL_Renderer` has no mipmaps; the chain is ours). Detail never changes
  with zoom; only scale does.
- **The master is held in RAM; the GPU holds what is on screen.** The master and its
  chain live in system memory (about 6.7 GB for an Earth-sized body); each frame uploads
  the visible chunks of the level in use, within a per-frame upload budget so a pan
  never hitches, and keeps an LRU of uploaded textures.
- **Pre-baked during generation.** The home body's master bakes while the player watches
  the world generate (STARTUP.md), so the first frame of play is final at every zoom.
  Other bodies bake in the background after the home body, nearest and most-visited
  first; a first visit to an unbaked body bakes the area under the view first and the
  rest behind it. A RAM budget drops the masters of the least-recently-visited bodies,
  keeping their far pages.
- **The bake is pooled.** All baking runs on the worker pool (§ Chunks, cache and
  invalidation); the home body's master is about 5,200 chunks (5,244 at 261×121), work
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
- **No rung magnifies at the reference window**: the top rung draws ~110 px from the
  128 px master (minified ~1.16:1). A 4K-height window magnifies the top rung ~1.7×,
  accepted for the prototype (Ben, 2026-10-10: 128 over 96, and over 192, which would
  break the 16 GB minimum).

At the reference 1720×1080 window:

| Zoom rung (drawn hex radius) | Level drawn (px per hex) |
|---|---|
| ~7 px (whole grid) | 8, minified (~1.14:1) |
| ~14 px | 16, minified |
| ~28 px | 32, minified |
| ~55 px | 64, minified |
| ~110 px | 128 — the master, minified (~1.16:1) |

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
| Ground | C-F: painterly relief base, near-future grade over it — **detail from it3 C-F, colour from it1** (below) |

**Two references, two jobs** (Ben, 2026-10-10). **Detail and drama come from it3's C-F panel**
(relief on every hex, the low-sun shadows, the wide light-to-dark range, the water forms, the
town density); **colour comes from it1** — warm olive and khaki fields, rich green forest, white
snowy peaks, a blue-green sea (Ben, the same day: *"we want more colour than the current render
suggests … the key element I wanted to bring was the resolution / detail, and less the colour
scheme"*). The C-F pass first took both from it3 and came out too grey; the palette below is it1's.
Judged against `docs/ui/design/renders/map/`. What it asks of the bake, procedurally, in this order:

- **Palette and value.** Base hues take **it1's colour** — warm olive and khaki, rich green
  forest, white peaks, blue-green sea — not C-F's grey (the grade's desaturation eases to
  match); from it3 the value structure stays:
  the value range widens — **lit slopes near a warm off-white, shadows near black** — where
  the build sat in a narrow mid-dark band; the broad dark mottle that belonged to no
  landform is cut. The grade's S-curve strengthens to hold the range. Sampled from the
  reference (sRGB): lit slopes ~(168, 153, 127), at their brightest ~(180, 163, 135);
  flat lit ground ~(75, 70, 60); the median ground ~(55, 56, 51); shadow ~(11-25, 12-27,
  12-25); stands of forest ~(30-36, 35-38, 34); water ~(50, 56, 56), deep ~(30, 35, 35).
  The value targets the bake is held to are the reference's luminance percentiles — p10
  ~26, p50 ~55, p90 ~106, p99 ~155. **The hues are it1's**, sampled from its top-down panel
  (sRGB): fields khaki-ochre ~(100, 83, 53), the warm ground as a whole ~(79, 70, 47);
  olive grassland ~(61, 67, 54); forest a deep olive-green ~(41-51, 43-54, 27-35); bare rock
  a warm grey ~(62, 61, 56); snow ~(179, 175, 161); deep sea a teal ~(21, 42, 50), shallows
  ~(64, 83, 81). The chroma target (mean max-minus-min channel, sRGB) is it1's **~22** over
  the ground, ~32 on the warm ground, ~19 on vegetation, ~24 on water. Each baked base entry
  keeps the luminance the C-F pass tuned and takes it1's hue and chroma, and the grade's
  desaturation eases (0.15 toward luma) so the chroma survives it. **Snowy peaks:** white
  caps on the mountain form's high ground, where the form rises past a snow line that falls
  as the massif's tile height (a weighted mean, continuous across tiles) stands higher —
  broken by the form's jag so snow runs down gullies. The baked ground has its own palette for this
  (`palette::ground_tile_colour`); the identity fill the vector fallback, minimap and
  generation preview draw keeps its own hues.
- **Relief everywhere.** Every hex carries terrain shape: **plains roll** as low hills with
  folds ~0.3-0.5 hex apart (they were held calm), ranges rise above them. The landform
  forms keep their drama on top. The folds are a field of two sheared octaves 0.46 and
  0.29 canonical units across (a hex is 1.73 wide), turned ridged so they crease along
  meandering crests, swelling and easing over ~2.4 units so some country lies nearly flat;
  under them, broad smooth hills (~1.15 units) give the long lit and shaded faces.
  Relief is terrain shape: its amplitude follows the wide interpolation, never a tile's
  edge band, so no relief stops at a hex.
- **Cast shadows.** A **low-sun shadow pass** on the height field throws shadow across the
  ground behind ridges and hills, from the same NW light, so relief reads as mass, not only
  as slope shading. The sun stands 33° high; a point looks at most **1.5 canonical units**
  upsun for a caster, so a pixel reads tiles at most ~5.3 units away — inside the chunk
  hash's margin (6 units), and the shadow is a pure function of position: any window
  bakes it identically.
- **Water.** Rivers and sea take it1's **blue-green** — a teal, neither royal blue nor
  grey — with specular glints, white **rapids** where a river falls, and **rocky banks**.
  Sea ~(22-36, 50-66, 58-70) before the grade, shallows paling toward ~(62, 98, 94); a river
  shelves from ~(58, 86, 86) at the bank to ~(22, 46, 54) mid-channel. Rapids show where a reach drops more than ~0.02 of the height range to its
  downstream neighbour, full white water by ~0.06.
- **Towns.** Settlements are **denser and larger**: compact blocks of multi-storey buildings
  with lit roofs, shaded sides, stacks and a street grid, reading as a town from the mid rungs.
- **Forests** keep their current procedural form for now (Ben did not take the conifer change).

Truly painterly trees and towns need authored brushes and stamps; that is a later item, on
the stamp seam that already exists. What the reference owes to its low perspective camera —
silhouetted ridges, foreshortening, distance haze — is out of reach of the top-down bake
(§ The staged end-state).

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
- **Master completion:** `scripts/verify/ground_master_complete.lua` — the home
  master reaches complete and current from a first visit, after every rung, after a
  body switch and after a burst of dirty chunks; on the pool path
  (`IO_GROUND_BENCH=1`) the background fill alone does so within a time bound.
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
