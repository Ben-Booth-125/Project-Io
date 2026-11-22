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
   **future milestone of its own** (since ruled: § The 3D ground).
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
angle** (22.5°) at every rung, and **zooming bakes no new detail** — one 96 px/hex master
per body, pre-baked during generation and held in RAM, with every other zoom a downsample
of it (§ Level of detail; the angle since ruled 3D, § The camera). This supersedes the never-magnify tier ladder and
the stepped tilt. The two closest rungs add a draw-time detail texture over it (Ben,
2026-11-15, § Level of detail).

Ruled by Ben, 2026-11-16, after the 3D slice: **the ground is 3D**, seen through a perspective
camera whose tilt follows zoom, with no turn; the flat ground retires entirely; ground marks
drape over the terrain, markers stand upright, clicks pick the terrain, and buildings are
procedural geometry (§ The 3D ground; § The camera). This supersedes the one-angle ruling.

---


## Roads draw across the edge they cross (Ben, 2026-10-16)

A road is an edge, not a tile (`../economy/LOGISTICS.md` § 4. A road is an edge), and it draws as
one: a run crossing the boundary between two tiles, meeting the boundary where the two tile centres
would. It is never a fill, a blob or a stamp inside one tile's footprint. Tier reads as the run's
own weight, and a junction is what several runs meeting at one centre already look like — nothing
draws a junction as a separate object.

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
| **Land lift** | the land weight (water and a river's water carry none) | The player's lightness level on land only — a hue-keeping gamma on luminance, the last pass (§ Art direction and palette, "The land lift") |

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

**Forest trees are pointed (Ben, 2026-11-05)**: a tapering crown, not a bushy round head, rising
from a short trunk in three bough tiers, its west flank lit. The forest fringe's trees take the same
form. The form stands 3.5 crown radii above its root before the camera stretch, and its half-width
at the foot is 1.08 radii. Default for Ben's review: scrub keeps its round form, and a flat plan view
(the Selection band's neighbourhood page) looks straight down, so its crowns stay round.

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
    chunk, plus its far page. **The game process peaks under 6 GB on every machine** (Ben,
    2026-10-17). A master chunk that is fully opaque is held **losslessly as RGB**, its alpha
    implied, and only a chunk with any transparency keeps RGBA; the pixels drawn are the same
    either way. **The home body's master is pinned in RAM** and never dropped (Ben, 2026-10-10: at
    128 px a single visit elsewhere had evicted it, and coming home re-baked for ~71 s).
    Other bodies share a **fixed ground budget** — what the pinned home leaves under the 6 GB
    process ceiling, the same on every machine rather than a share of installed RAM (Ben,
    2026-10-17, replacing 2026-10-10's 45%) — so they usually stream from the disk cache on
    each visit: when a body's master must
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
  - **Optimised in every build** (BL-1311, ground paint perf): the bake's
    pure translation units build as their own library at full optimisation
    in a Debug build too (`io_ground_bake` in CMakeLists.txt; the
    `IO_OPTIMISE_GROUND_BAKE_IN_DEBUG` option turns it off to single-step
    the bake). Unoptimised, a Debug build's home master cost ~5x the CPU of
    the optimised one (`scripts/bench/ground_paint_bench.lua`: 6,089 s
    against 1,248 s; Release 897 s), and its pixels are the same byte for byte.
  - **Results are keyed by job:** each slot records the sequence number of
    its one outstanding job, and only that job's result can land in it, so
    arrival order cannot matter and a result that outlives its body (a drop,
    a new world) lands in nothing.

  **Under `--verify` everything a frame draws is complete before the frame
  returns:** the master chunks under the view bake on the pool and the main
  thread waits for them; every visible texture uploads, and every chunk mesh
  of the 3D ground builds, with no budget; no
  pre-bake or background bake runs — a capture must never race the pool.
  The canvas asks for the next frame's ground, so a capture first renders until
  the canvas asks for nothing the bake has not covered and every chunk under the
  view is on the GPU, then one frame more (the 3D pass draws from the last frame's
  camera): a capture taken straight after a zoom, a pan or a lens shows its own
  view's ground, not the last one's.
  (The home master is ~1-3 minutes of pool time, paid by the first
  whole-body framing of a run.) So the master **never fills by itself**
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
  radius is **at least 40 px** (between rung 2, ~24 px, and rung 3, ~48 px, at the
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
silhouette, the stacked-tile ring, the `+N` count badge, the owner emblem tag and the
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
rule), so a rival's structure draws exactly as the player's. **No identity wash** tints the player's
structures or ground (Ben, 2026-10-26): ownership on the plain canvas is the national border
band (`PLANETARY.md` § The national border band), and under a lens the player footprint
outline (`LENSES.md` § Rung applicability).

**A stacked tile stands a cluster** (Ben, 2026-10-08). One structure per **stack** —
`(type, target)`, `placement_rules::stack_members` — placed by a fixed in-tile layout,
the dominant (lowest-id) stack largest and in front, **up to three**; a fourth stack and
beyond adds nothing to the ground. Two buildings of one stack are one structure: the art
says *what stands here*; the Selection element's tile card says *how many*, on the stack's button
(SELECTION.md § The tile element's layout).

**Settlements are structures too.** A population centre bakes as a settlement whose
footprint and height step with its scale, and a razed centre as a ruin. A small centre may
vanish into the ground at the far page where the old density dot stood, but a scale ≥ 3 centre
must read as a city at every rung — the obligation the skyline LOD ladder carried, which the
art now takes over.

**Each size band draws a different group, on its one tile (Ben, 2026-11-19).** His reason:
*"render different population centre sizes with different building groups, to show that a city
is larger than a town."* He chose a sharper form per band over spreading a city across its urban
footprint: the settlement stays on its centre's tile, and the bands are told apart by **what
stands**, not by how many hexes it covers. The bands are POPULATION.md's scale rungs. Each
band's tallest standing thing is well above the band below's, so the bands part by silhouette at
any camera, and roofs turn from pitched to flat as the place stops being rural:

| Scale | Band | Field (of the hex) | What stands | Storeys | The heart |
|---|---|---|---|---|---|
| 1 | Village | small, ~0.30 | a handful of pitched-roof houses and a barn round a yard; no street grid | 1 | — |
| 2 | Town | ~0.46 | a loose street grid of pitched-roof lots with gardens between | 1–2 | a church spire |
| 3 | City | ~0.62 | a full grid of flat-roofed blocks, a works quarter with stacks | 2–4 | a domed civic hall |
| 4 | Metropolis | ~0.80 | dense mid-rise to near the hex edge, two works quarters' stacks | 3–6 | a cluster of towers, ~14 storeys |
| 5 | Megacity | ~0.92, to the edge | every lot built, mid-rise throughout | 4–8 | a skyline of towers, ~24 storeys, over a wide core |

The field may overhang its hex a little at the top band, as any structure may. The constants are
`k_town` in `src/ui/structure_stamps.cpp`; the capture check is
`scripts/verify/settlement_bands.lua`, one frame per band.

**The art is static** (Ben, 2026-10-08). No smoke, light, flag or animation distinguishes
a running building from an idle, starved or mothballed one; running state is read in the
Selection element alone. Two reasons: a state cue on the ground is a glyph by another
name, and a stamp keyed on tick-rate state would re-bake chunks every tick (§ Chunks,
cache and invalidation). **Under construction is the one exception** — a site bakes as
scaffolding, because construction start and completion are events, not ticks.

**Ownership never touches the ground art.** This settles what was the ground/chrome layer
contract's sharpest open call: a structure carries no owner colour. Ownership is read from
the hover card, the Selection element, the player footprint outline under a lens (none on the plain canvas: Ben, 2026-10-24), and the ground:
a building's owner is always its ground's nation (Ben, 2026-10-23), so the national border band
and the Nations lens ([LENSES.md](LENSES.md)) say whose it is.

Consequences the design accepts and answers:

- **Far-zoom legibility lives in the art**, not in a glyph fallback — an installation
  must be authored to read at distance (footprint contrast, cleared ground, a road
  stub), the way the it3 settlement reads. Ben chose this against a
  geometry-close/glyphs-far ladder, deliberately.
- **Structures stand up as geometry.** A structure is procedural low-poly 3D shapes on its
  footprint, lit and shadowed with the ground (§ The 3D ground); the bake keeps only its
  ground parts - pads, yards, aprons, a town's paving - and the roads.
- **Under a lens a structure is ground.** It takes the lens wash like the terrain around
  it; a lens is an analytic read, seen under the same camera as the plain canvas.
- A structure stamp may **overhang its tile** (chimneys, towers); stamps compose in
  row order like every other pass. **Hit-testing is unchanged**: the hex, not the stamp,
  is what a press lands on (SELECTION.md § Multi-building tiles).

### Mountains, rivers and terrain variety

Ben, 2026-10-08: mountains and rivers **blend into the render**, with **more tile sets**.
Both were vector chrome drawn over the bake — a stroke-only landform glyph on the hex
centre, a straight river line from tile centre to tile centre with downstream chevrons —
and both read as annotation laid on a painting. They move into the bake.

- **A mountain's flanks are broken, not smooth (Ben, 2026-11-16: mountains looked "very smooth
  compared to the detail", "even when zooming out a bit").** Into the massif the bake cuts
  **erosion gullies** - sharp, narrow channels running straight down each flank from the
  ridge, a field sampled at the ridge point a pixel drains from, so every gully runs downhill -
  and its **crags** stand twice as tall as before. Both are in the 96 px master, so they reach
  every rung the mips carry; the draw-time relief adds taller rock bumps at the closest two.
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

**A bridge is the run carried over the water, and nothing more (Ben, 2026-10-17).** A crossing with
a water end draws as a **deck**: the same tier width, the same colour and the same crossing points
as the road on either shore, continuous with it. What a deck does *not* carry is any treatment that
belongs to ground — no verge, ditch, embankment, cut or spoil, and none of the water's own colour,
because the ground-tint term is dust off the land a road is cut into and a bridge is not made of the
strait. Its one mark on the water is the narrow shadow the shared light throws. No parapet and no
ornament: at play zooms a deck is a sub-pixel thread like any other road, and anything added for
decoration is something a restyle must later undo.

**Roads are part of the ground's tile sets, not threads laid on it** (Ben, 2026-10-10, walking
the C-F build: *"various tile sets with roads, highways, and railroads on them, so the roads don't
appear just painted on top … roads curving around hills / buildings, and connecting at various
points on the tile edge, not just the centre"*). This revises the geometry rule below:

- **A road crosses a tile edge at its own point, not the midpoint.** Each shared edge a road
  crosses carries a **crossing point** placed by a hash of the edge (within the inner 70% of the
  edge), computed identically from both tiles so the road is continuous; a road of a higher tier
  and a lower one crossing the same edge take separate points. **The values:** the point lies
  within **0.35** of the edge's midpoint (the edge is one hex radius long), keyed on the edge's
  west or north tile — its wrapped column, so the cylinder seam agrees — and the road leaves it
  straight across the edge, so the two tiles' pieces meet in one direction; a rail point stands
  half the band (0.35) along the edge from the road's.
- **Inside the tile the road finds its way.** The path from crossing to crossing (or to a
  junction, or to a building's forecourt) is a smooth curve that **bends around higher ground** —
  it keeps to the lower side of the tile's relief rather than climbing over a hill — and **around
  the tile's structures** (the shared road plan). It passes through the centre only where the
  ground lets it. **The values:** a cubic between its two ends, bowed sideways by a profile that
  leaves each end, its heading and its curvature where they were; the bow is chosen from a
  bounded grid by the ground under the curve (the tile heights and the hills field, in the
  hillshade's own units), its length and its bends, kept inside the hex, clear of the plan's
  cluster, and no tighter than the tier's least turning radius — **0.22** of a hex for a road,
  **0.60** for rail — wherever any candidate allows it.
- **Each terrain family carries its own road treatment** — the road's tile set: on a slope the
  road is **cut into the hillside** with a shaded bank on the uphill side; across wet or low ground
  it runs on a low **embankment**; through forest it runs in a **cleared corridor**; on open
  ground a verge of worn grass; in a town it is the street. The road reads as built into the land,
  in the same light and grade. **The values** (past a Road's surface, as a fraction of the hex
  radius; a Track takes 0.7 of each, a Highway 1.2, rail 1.7): the cut's uphill bank up to
  **0.032**, rising with the slope across the road, and a downhill spoil edge up to **0.014**; the
  forest corridor **0.09**, with the tree line's shade at its edge; the embankment's side slope
  **0.024**; the worn verge **0.014**; a town's kerb **0.006**. The surface itself is lit by the
  ground's own hillshade and takes a share of the ground's tone.
- **Rail** follows the same routing with **gentler curvature** (a railway cannot turn as tightly
  as a road) and its own treatment — ballast, sleepers, twin rails, cuttings and embankments
  deeper than a road's — painted wherever track is laid (BL-1255 railways; `../economy/LOGISTICS.md` § 4c — rail is its own network, not a road rung).
- **No drawn road network on the plain canvas at any rung** (Ben, 2026-10-10: roads drew too much
  attention when zoomed out, because the drawn web kept a minimum width). Roads are the painted
  ground at every zoom and shrink with it like the ground; **the network as logistics is read on
  the Logistics lens** (LENSES.md § Logistics lens).

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
  rail network (`../economy/LOGISTICS.md` § 4c; INDUSTRIALISATION.md, the rail sink), wherever
  track is laid, and a station building where the line meets a city; its bed is **0.10** wide. Textures are procedural, hash-placed along
  the curve, nominal-keyed, wrap-exact. A dash pattern (the centre line, the sleepers) runs a
  whole number of dashes along each half of a curve, pinned at the edge's crossing point, so it
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
  **0.80 / 0.70 / 0.60** for one / two / three or more links; a through-road
  **bows** away from it (the route's own bow, which leaves its ends and their headings where
  they were, so it still meets its neighbours smoothly at the crossing points) until the road
  and its verges clear the cluster; a road that ends here ends at a **forecourt apron** standing just outside the
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
- **The drawn network is the Logistics lens's** (the bullet above): on the plain canvas,
  over baked ground, roads and lanes are painted only, at every rung. A tile whose ground is
  not yet baked (the vector fallback) has no painted road, so it keeps the drawn network.
- **The Logistics lens takes the routes off the ground** (Ben, 2026-10-25: *"remove the
  sharper road rendering when the logistics lens is active; currently you can see roads
  twice"*). The painted route and the lens's drawn stroke follow different geometry — the
  paint runs between hashed crossing points, bows around ground and clusters and rides the
  oblique lift; the stroke runs edge-midpoint to edge-midpoint — so under the lens the two
  read as two roads. While the lens is up the bake paints **no route surface**: no road
  ladder, no rail bed, no lane wake. Everything else stays: the routes are still derived, so
  a tree still keeps off a road's line, a town's blocks off its street, a roaded cluster
  still steps toward its road — only the surface is gone, and the corridor it ran in is the
  ground's. The canvas then draws the whole network itself — every road and lane as ever,
  and every **rail** track tile too (the four-cardinal walk, a dark ballast tone at a Road's
  weight), with the player's chain in the lens's colours (LENSES.md § Logistics lens).
  **Mechanism:** the canvas sets `ground_request::hide_routes`; `ground_layer` re-takes the
  active body's source at once with `strip_painted_routes` (one flag on the source), which
  moves `route_hash` for exactly the windows a route reaches; a chunk no route reaches keeps
  its hash and is not touched. The flipped chunks re-bake as **route-only partial re-bakes**:
  a box round each piece's painted reach and nothing more — the routes did not move, so no
  tree, town block or cluster is re-boxed (§ Chunks, cache and invalidation).
  **Only what the lens shows close is re-baked.** While the lens is up, a ready chunk the
  flip dirtied re-bakes only when it is **under the view at a drawn level of 24 px a hex or
  finer** (`k_unpaint_max_level`); coarser, a painted road is under a pixel and the chunk
  keeps its pixels until the view comes close, and the far page is not re-baked at all.
  A chunk never re-baked keeps the painted hash, so disarming the lens re-bakes only what
  arming it did. **Measured** (`scripts/verify/logistics_chain.lua`, the `ROAD_HIDE` lines,
  1720 × 1080, the shipped 1960 start): at a close rung (51 px drawn radius) a toggle
  re-bakes **80** chunks, about **25 s of worker time**; at a mid rung (18 px) **311** chunks,
  about **54 s**; on the whole grid **none**; the whole home master, before the view rule,
  was **2,992** chunks and **~540 s**. On the play path that work runs on the pool behind
  the frame — the main thread pays only the snapshot it already takes on its cadence — and
  a chunk shows its previous pixels until it lands. `scripts/verify/logistics_unpaint_perf.lua`
  (the `UNPAINT_PERF` lines, pool path, a Market toggle as the control) measured on a Debug
  build with the software renderer: at a close rung (77 px) arming re-bakes **42** chunks and
  the view settles in **~2.2 s**, its frames no slower than the control's (p95 212 ms against
  242 ms — that build's frames are the renderer's, not 60 fps figures); at a mid rung (28 px)
  **311** chunks settle in **~5.5 s** (p95 291 ms against 220 ms), and disarming there took
  **~39 s** behind the frame with one 2 s frame — the cost a Release measure should confirm. **Why not a second, road-free master:** a second master per body would
  double the ground's resident set against the fixed budget (§ Resident set); and a
  ground-coloured underlay cannot hide a road it does not lie on — the painted route and
  the drawn stroke do not share a line, and the paint rides the oblique lift.
- A built or upgraded road dirties only the window around its tiles (§ Chunks, cache and
  invalidation, the partial re-bake), and the survey mask and the reach fog treat a painted
  road as ground.

Ben, 2026-10-03, playing the build: *"render [roads] as curves rather than lines,
and make them thinner"*; *"sea lanes should always go over ocean, never over
ground"*. Roads (`road_level`) and sea lanes (`lane_level`, [LOGISTICS.md](../economy/LOGISTICS.md)
§ 4b) are both tile fields on the four-cardinal grid the traversal walk uses, and
both draw by one rule:

- **A route is drawn along its own tiles, never between its ends.** A lane is drawn
  only on the sea tiles it was stamped on, so it can never cross open land; the wizard's
  lapse draws a lane along the same water-only walk the stamp lays, port to port
  ([STARTUP.md](STARTUP.md) § Round 5, "The lane").
- **A lane ends in its port** (Ben, 2026-10-26: *"draw a sea lane with its ends going into
  land, where the port tiles are"*) — the one place a lane's line leaves the water. A port lays
  its lanes from the water on its six sides (LOGISTICS.md § 4b, "Every port lays lanes"), so the
  stamped lane stops on the sea tile **beside** the port, and drawn to its own tiles alone it read
  as a lane that never reached the port. So a lane tile beside a **standing** port draws its curve
  on across the shared edge **into the port tile's centre**, as though the port were one more link:
  the curve bends to the edge midpoint like any other link, and the half on the port's side runs
  from that midpoint to the port's centre, capped there. **Which lane tiles join:** every lane
  **end** beside the port (one cardinal lane neighbour or none); a port no lane ends beside but a
  lane runs past joins the **lowest-index** lane tile beside it, so a lane hugging the coast is not
  fanned into the port from every side. Any of the six hex sides joins, not only the four the
  walk steps on, because a port's mouths are its six sides. No other land is ever touched. This
  is the drawn network's rule (the Logistics lens, and the vector fallback where the ground is not
  yet baked); the baked wake is the water's own mark and stays on the water.
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
  through a lens** (LENSES.md § Logistics lens draws the road network at full weight, at
  the old 1 : 1.5 : 2 weights).
- **LOD** (the drawn curve — the Logistics lens's network): below a **20 px drawn radius** (`k_route_lod_radius_px` — between the
  second rung, ~12 px, and the third, ~24 px) a curve is **one stroke**, its two
  halves joined in a single path, with no round joint at the apex, and each half is
  tessellated at **two segments**; at the coarse fill (drawn radius ≤ 7 px) at one —
  its two chords. A curve whose halves fog differently is two strokes, still
  jointless. At these radii a curve spans a dozen pixels and the joint is under a
  pixel wide, so nothing is lost to the eye; six segments a half and the joint fan
  were most of the road layer's vertices, and the wide rungs must hold the 60 fps
  budget (TECH_FOUNDATIONS.md § Target hardware; Ben, 2026-10-09). The third rung
  and up draw the curve unchanged.
- Both are **always-on** as ground — painted, under every lens but Logistics (which draws
  them itself, above), and washed over by the lens — and dim with the reach fog and stop at
  the survey mask as roads always have.

### Ambient animation

Motion is **an overlay, not baked**: the ground bake is static, and animated
passes draw over the baked chunks.

**Water moves by shader (BL-1660, shader water).** Where the renderer runs Io's fragment
shaders ([TECH_FOUNDATIONS.md](../tech/TECH_FOUNDATIONS.md) § Framework), open water carries a
**glint**: two crossing height fields, three and five waves a hex, scrolling slowly and lit by
the bake's low NW sun, add a small specular highlight over the baked water — the bake stays the
water's colour, the shader only adds light. It reads one table, which tiles are visible water,
and fades to nothing within half a hex of any side facing a non-water tile, so the painted
coastline, the shore shelf and a river mouth never move; and below ~24 px a hex, where a wave
would be under a pixel. Shore foam is not part of it: foam must follow the painted coast,
which a per-tile table cannot place. **No-shader path:** the static bake. The flipbook
overlay this replaces is retired for water. No
animation keys on an installation's state — the art is static (§ Installations). The animation clock is **render-side real time** — never sim
state — so ambience continues while paused; under `--verify` the clock is **pinned to
phase 0** and a check advances it explicitly. The near-future grade applies over
animated overlays too, so motion cannot break the grade.

### Level of detail — one master, every zoom a downsample

**Rungs halve from the 96 px master, and the wider view stands (Ben, 2026-11-07).** The widest view is about
0.82x the old extent, past the 0.7x lag cap of 2026-08-12: Ben chose the wider view over the old floor.

**Planetary zoom is STEPPED** (Ben, 2026-09-01, judging the first bake: *"C-F is a
detailed image… we could approximate it with stepped zoom, rather than the continuous
zoom we currently have (2.5D)"*): the player's wheel and keyboard move through a fixed
**×2 ladder** — `kMinZoom × 2^k`, five rungs spanning the old continuous range
(`planetary_zoom_stepped`; verify's `set_zoom` stays free-form so scripted framings
are unaffected). The upper canvas rungs keep continuous zoom.

**A step glides** (Ben, 2026-11-14, BL-1653 (animated zoom step)). The ladder is the
state — the saved zoom and every reader of it see the rung — but the zoom DRAWN moves to
the new rung over **0.18 s**, eased out, geometric in zoom, holding fixed the point under
the cursor (the wheel) or the view's centre (the keyboard). In between, the level drawn
follows the drawn radius by the rule below (minified at most 2:1), so a glide asks for no
level the ladder's two ends do not. A step taken mid-glide starts from the zoom drawn. A
middle-button pan ends the glide at its rung. Under `--verify` (the fixed frame clock) a
step is instant, so a capture never lands mid-glide.

**Zooming does not add detail** (Ben, 2026-10-09, after walking the sprint 51 build:
*"the current lag is very off putting… zooming doesn't add more detail, arguably the
biggest fix even if we keep a high resolution"*). This supersedes the per-rung bake
tiers of 2026-09-01 and the never-magnify tier chooser of 2026-10-08: a zoom step had
been a fresh, expensive bake, and that bake was the lag. The BAKE still adds nothing with
zoom; the two closest rungs add a draw-time detail texture (Ben, 2026-11-15, below).

- **One master per body.** The ground is baked ONCE per body, at **96 px per hex** (Ben, 2026-11-05, overturning 128 px, Ben,
  2026-10-10: "simply up the resolution in each tile"), its levels halving from there, at
  straight down (§ The camera), whole-body, in 512 px chunks. Everything the
  player sees at any rung is this image: the close-tier features (trees, crags, strata,
  structures) are always in it and simply grow small with distance.
- **Every other zoom is a downsample.** A **mip chain** — 48, 24, 12 and 6 px per hex
  — is box-downsampled from the master, chunk by chunk, in milliseconds. A rung
  draws the level at or above its drawn radius, minified by at most 2:1, so no step
  shimmers (`SDL_Renderer` has no mipmaps; the chain is ours). The baked detail never changes
  with zoom; only scale does. **The downsample averages in linear light** (Ben,
  2026-11-14, BL-1652 (gamma-correct mips)): each sRGB byte is decoded through a fixed
  table, the four samples averaged by alpha weight, and the mean re-encoded through the
  inverse table. An sRGB-byte mean darkens fine contrast — a lit crown beside its
  shadow averages to a darker grey than the eye sees — so each level would read
  muddier than the master it came from. Integer tables, so the chain stays
  deterministic; a flat run stays exact.
- **The master is held in RAM; the GPU holds what is on screen.** The master and its
  chain live in system memory (about 2.9 GB for an Earth-sized body, opaque chunks as RGB); each frame uploads
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
- **The two closest rungs add detail at draw time (Ben, 2026-11-15, BL-1658 (close-zoom
  detail layer); overturning "zooming does not add detail" for these two rungs, whose
  ground for that ruling was the lag of a fresh bake per rung).** Where the renderer runs
  Io's fragment shaders, the ground shader modulates the land's **luminance only**, by a
  tiling **detail texture** chosen per terrain family, so close zoom stays crisp past the
  master's 96 px per hex (and past the ~2x magnification of a 4K-height window). Nothing is
  baked: the bake and its chain are unchanged. The 3D pass applies it (§ The 3D ground).
  - **Four families**, one greyscale texture each, packed into the four channels of one
    texture: **vegetation** (grass, scrub, marsh; bare sedimentary ground), **forest**,
    **arid** (dunes, ash, salt; bare barren ground) and **rock** (snow, urban; bare rocky,
    volcanic, metallic, regolith and icy ground). A tile's family is its cover first, its substrate
    when bare. Water and the survey lock fill take none.
  - **Two scales (Ben, 2026-11-15: "more detail than this", answered "two scales").** A
    **coarse** layer gives the patches; a **fine** layer over it gives grain inside them.
    Each family has a texture at each scale.
  - **The textures are generated files**: the coarse `assets/detail/<family>.png`, 256 px
    square, and the fine `assets/detail/fine/<family>.png`, also 256 px square (Ben,
    2026-11-16: "increase the resolution of the fine grain textures, and make them larger"),
    from its own seed. Both are seamless, written by `tools/art/gen_detail_textures.py` from fixed
    seeds. They are ordinary art files: repainting one by hand replaces the generator's
    output for it.
  - **The textures are hard-edged where the ground is:** forest is small round crowns of
    varied size in shadowed gaps (Ben, 2026-11-16: whole-cell crowns read "too jagged"), arid ground is steep-faced ripples with pebbles, rock is cracked plates. Soft noise
    there reads as blur, not detail. **Grass is the exception** (Ben, 2026-11-16: cellular
    tufts "just look rocky"): soft clumps with a short, bent blade grain, no cells or seams.
  - **Strength (Ben, 2026-11-16: doubled, the first cut was "not really seeing" it): ±18% of
    luminance for the coarse layer, ±14% for the fine**, at each
    texture's extremes (each is normalised so its 99th-percentile deviation is the full
    swing); the two add. Both apply at the **48 and 96 px rungs**, fading to none from 48
    down to 24 px drawn radius, so a zoom glide brings them in smoothly.
  - **The fine layer shows only where it resolves:** it fades out as its texels shrink
    from 1.5 to 3 per screen pixel, past which it would shimmer.
  - **World-locked:** the coarse texture repeats every 4 canonical units (one hex
    circumradius is 1), the fine one every 2, so a grass tuft is about 10 px across at the
    96 px rung, anchored to the ground through the same inverse camera as the land lift, so
    it pans and zooms with the map. The family is per tile, softened across a hex side
    over the lift's band. It touches what is drawn on land - trees, crags - which reads as
    surface texture, not pattern.
  - **The detail ends at the painted coast (Ben, 2026-11-22: "fine grain textures drawn over the
    ocean, or land with no texture where the hexagon spills onto the ocean").** The bake's coast
    is warped and organic; the hex side is not. So the detail's extent is per pixel, not per
    tile: the bake **marks every pixel it paints as land** (land weight at least half; the sea,
    a lake and a river's water are not land) in the **lowest bit of its red channel**, as the
    structure mark uses blue's, and the shader filters that bit over the four texels round the
    point, so the detail's edge follows the painted shore softly. The family is still per tile:
    the hex's own, and where the painted land runs past the hex side into a water tile, the
    nearest land tile beside it. A land tile does not fade toward a water neighbour at the hex
    side; the mark is the coast. The draw-time land lift reads the same mark (below).
  - **Each tile reads on its own (Ben, 2026-11-16: the style "doesn't differentiate between
    different tiles very well" - both the families looking alike and neighbours running on as
    wallpaper).** Every tile reads the textures **at its own offset**, a hash of its grid cell,
    so no two neighbours continue one pattern, and takes its **own tone**, a small lift or dip of
    the whole tile (±10% of luminance at the extremes). Each **family carries its own strength**:
    forest and rock strongest, then arid ground, grass weakest. Across a hex side the two tiles'
    readings blend over the lift's band, as the family does. No rotation or mirroring: the
    textures carry the bake's light direction in them.
  - **Buildings and roads draw over the detail (Ben, 2026-11-16: "draw buildings and roads over
    the detail layer, so it can still be seen in the space between buildings").** A per-tile
    rule left whole hexes plain; this one is per pixel. The bake **marks every pixel the
    road-and-installation pass paints** (a road, a pad, a yard, a roof, a town's paving), and
    **every pixel of a river's water** (Ben, 2026-11-16: "rivers should be painted over the fine
    grain textures"), in
    the **lowest bit of its blue channel**, and clears that bit on every other pixel; the
    shader reads the bit, unfiltered, and applies no detail and no relief there. So the
    ground between buildings carries its texture and the buildings and roads stay as baked.
    The bit costs no memory and moves no colour by more than one step in 255. Each mip level
    keeps it where at least two of the four pixels it averages carry it. A compressed chunk
    format would not keep it (BL-1662 (compressed ground textures) would need its own mask).
  - **The ground has relief (Ben, 2026-11-16: "draw-time relief: lit bumps on a smooth height
    field, with short shadows").** A brightness ripple alone reads faint; light and shadow read.
    So the detail is also a surface, lit at draw time:
    - **A smooth height field.** Each tile's generation `height` is a sample at its centre; a
      pixel's height is the plane through the three nearest tile centres (barycentric), so the
      ground slopes continuously from tile to tile and never steps at a hex side.
    - **Rolling ground at draw time (Ben, 2026-11-16: flat tiles showed "no elevation or
      shading").** A broad, gentle height noise (`assets/detail/rolling.png`, swells about
      three hex radii across) adds to the height field on all land, so flat country rises
      and falls under the light at the two closest rungs.
    - **Bumps on it.** Each family's coarse detail texture is also a height map: the tufts,
      crowns, ripples and plates stand up from the slope, rock and forest tallest, arid ground
      lowest; the fine layer adds a smaller roughness over it. **A mountain tile's bumps stand
      two and a half times taller** (Ben, 2026-11-16: mountains looked "very smooth compared
      to the detail").
    - **Lit from the bake's light** (the upper left, about 40° up). The bumps apply what they
      add: the light on the bumped surface over the light on the bare slope. **The height field
      is shaded too (Ben, 2026-11-16: "I can't see the height map being rendered at all")**: the
      bake's own slope shading is too soft to read under the detail, so the shader restates it,
      the light on the bare slope over the light on flat ground. A tile's height rises 16
      canonical units per unit of generation height, so a step between neighbours is a slope
      the light finds. A bump on a slope facing the light lights more
      than one facing away, so the roughness follows the height map.
    - **Short shadows.** Each pixel looks toward the light over a short distance (half a hex
      radius, in four steps) through the slope and the bumps; where they rise above its line
      of sight it darkens. A crag throws its shadow downhill, and a steep slope facing away
      from the light lies in its own bumps' shade.
    - Weighted as the detail is: land only, none at 24 px rising to full at 48 px, none on a
      structure's or a river's pixel. The values (height scale, bump heights, shadow depth,
      family strengths, tile tone) are calibration,
      tuned by eye.
  - *Superseded the same day by § The 3D ground, which keeps its wind and sand and retires its
    parallax and shadow marches.* **Depth at the fixed angle (Ben, 2026-11-16: "more elevation, shadows, and possibly
    animations on the texture", answered "Shader now, 3D slice"; BL-1664 (fixed-angle depth)).**
    The relief above gains depth the camera can see, still at the one angle and still at draw
    time:
    - **Parallax relief.** The bumps are a surface the view ray meets, not only a light. A pixel
      steps its ray down through the bump heights along the view's tilt and reads the detail
      where the ray first meets the surface, so a raised crown or crag stands in front of the
      ground behind it and hides it. The tile-scale slope is not stepped: the bake already lifts
      it. The parallax runs at a depth of its own, deeper than the bumps' lighting height, tuned
      by eye.
    - **Long shadows.** Past the short march through the bumps, a second march runs **three hex
      radii** toward the light over the height field and the rolling ground, so a high tile
      shades the low ground beside it and a ridge throws its shadow across a valley. It is
      softer and lighter than the bumps' own shade.
    - **Wind and sand.** Grass and forest move: **gusts** - broad bands of the rolling texture
      drifting downwind - brighten the detail they cross, grass more than forest, and inside a
      gust **each blade flutters on its own**: the texture's raised parts swing in brightness,
      each blade-sized cell at its own phase and pace. Nothing moves the texture itself (Ben,
      2026-11-16: a moving lookup "looks more like the ground is warping than individual blades
      of grass are waving").
      Arid ground's ripples **creep downwind**, a pixel or so a second at the closest rung. The
      clock is § Ambient animation's: render-side real time, never sim state. Rock and snow do
      not move.
    - **No-shader path:** the static bake, as for every draw-time pass.
- **The farthest rung is a flat map (Ben, 2026-11-16: "flat map, cross-fade from 12 px";
  it shows national colour, inked coastlines and rivers as lines).** At ~6 px a hex the painted
  ground reads as noise: every feature the bake carries is a pixel or two. So the whole-grid
  rung trades the painting for a cartographic read, as a strategy map's far zoom does.
  - **What it shows.** Land is flat **paper**, tinted toward its **nation's colour**
    (unclaimed land stays paper; under the **Market lens** all land stays paper, so the
    catchment wash is the only colour — Ben, 2026-11-20); all water — sea, coast and lake tiles — is one flat, deep
    blue. **Coastlines are inked**: a dark line on the hex side between a land tile and a
    water tile. **Rivers are lines**: each land tile draws its half of the course, centre to
    the midpoint of each river side, in a darker blue. No terrain colours, relief or paper
    grain: the far map answers *whose* and *where the water is*, nothing else.
  - **Widths are in screen pixels**, about 1.5 px for a coast and 1 px for a river, so the
    ink does not thicken or thin through the fade.
  - **It fades in from the 12 px rung.** The map's weight rises from none at a drawn hex
    radius of 16 px to full at 6 px, smoothly: a third at the 12 px rung, so that rung
    previews the map, and full at the 6 px rung. A zoom glide passes through the fade.
  - **The survey still hides.** A tile the player has not surveyed keeps the bake's lock fill;
    no ink is drawn against it, so the map does not leak a coast the player has not seen.
  - **Drawn by the ground shader, over the images.** A per-tile map table carries each tile's
    colour and river sides; the canvas's washes, border rule and strokes draw over the map as
    over the painting.
- **No rung is wider than the whole grid (Ben, 2026-11-16).** A ~3 px rung was tried and
  cancelled the same day: it shows the same land several times across the cylinder's wrap,
  which confuses more than the wider view helps.
- **No rung magnifies at the reference window (Ben, 2026-11-05)**: the top rung is lowered to
  ~96 px, drawn from the 96 px master at about 1:1. A 4K-height window magnifies the top rung
  ~2× (the 2026-10-10 ruling accepted ~1.7× for the prototype, choosing 128 over 96 and over
  192, which would break the 16 GB minimum; the 2026-11-05 ruling overturns its 128).

At the reference 1720×1080 window:

| Zoom rung (drawn hex radius) | Level drawn (px per hex) |
|---|---|
| ~6 px (whole grid) | 6, about 1:1 |
| ~12 px | 12, about 1:1 |
| ~24 px | 24, about 1:1 |
| ~48 px | 48, about 1:1 |
| ~96 px | 96 — the master, about 1:1 |

The vector fill remains only as the fallback before a body's far page exists. What the
canvas strokes OVER the ground — washes, roads, lanes, the border rule — has its own
wide-rung level of detail and frame budget: PLANETARY.md § Draw-loop cost model, and
§ Roads and sea lanes above. The chunk images are submitted after those strokes on a
vertex offset of their own, drawn first: that ordering is part of the 60 fps budget, not
a style choice (the same section says why).

### The camera — a perspective view, tilt following zoom

The planetary ground is a **3D surface seen through a perspective camera** (Ben, 2026-11-16,
§ The 3D ground, replacing the one fixed 22.5° angle of 2026-10-09). The camera looks at the
centre of the canvas view, north up, and **never turns**. Its **tilt follows zoom**: straight
down at the far rungs, where the map reads as a map, rising with each closer rung to its
steepest - about 50° from straight down - at the closest. Its distance is set by zoom, so the
view's height fills the canvas as it does today; panning moves what it looks at. The player
does not tilt or turn it.

The master is still baked once per body and every rung still reads one level of it (§ Level of
detail). It is now baked **straight down**: with real heights the 3D ground lifts the hills
itself, so the bake's own oblique lift and its standing sprites retire. Trees stay painted on
the ground; structures leave the bake for geometry (§ The 3D ground).
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

- **A lighter tileset (Ben, 2026-11-05).** The palette across the tileset lifts lighter. The
  figures below are the base it lifts from; how far it lifts is read against the live map.
- **The land lift (Ben, 2026-11-10).** The lighter tileset is a **grade pass**, not new base
  colours: a separable final pass over the finished bake, after the near-future grade and the
  sharpening, so a level can be tried without re-authoring any palette entry. **Only land lifts.**
  Water — sea, ocean, lakes, and a river's water — keeps its colour exactly; a shore or bank
  pixel lifts by the share of it that is land. The shadow and relief shading on land lift with
  the land. The pass is a **gamma curve on luminance** (y' = 255 · (y / 255)^γ), applied as a
  ratio of the pixel's own channels so hue and saturation hold, never a flat add that greys the
  colour; the curve leaves the top of the range where it is, and a ratio that would clip a
  channel is held to the largest that does not, so highlights never clip and the land's p99 stays
  under ~235. It is a **player setting with three trial levels and off** — about +15%, +30% and
  +45% on the median land luminance — chosen live by Ben; changing it re-bakes the land, and
  ground holding no land (open sea, the survey lock fill) is not re-baked. The default is the
  **middle level (+30%)** until Ben picks. The percentiles each level gives are
  `ground_bake_check` P35's reading (`--lift`), which also holds the water byte-identical to the
  lift off. The player sees the setting as **Brightness** (Ben, 2026-11-15); "land lift" stays
  the design's name for the pass.
- **The lift at draw time (BL-1659, live grade shader; NR-1232 option D, Ben 2026-11-15).**
  The canvas's ground bakes **without** the lift and the 3D pass (§ The 3D ground) applies the
  same curve as it draws, so a change of level is instant and re-bakes nothing. **The land
  weight there is the bake's land mark (Ben, 2026-11-22, overturning the per-tile weight of
  2026-11-15):** red's lowest bit, set where the bake painted land (§ Level of detail, "The
  detail ends at the painted coast"), filtered over the four texels round the point. So the
  lifted shore is the **painted coast**, as in the baked lift, and a river's water keeps its
  colour. The mark costs no memory: the opaque chunks stay RGB (§ Resident set), and the
  half-resolution land mask per chunk (NR-1232 option B) is not needed. The Selection band's
  neighbourhood page, which no shader draws, still bakes the lift.
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
- **Flat land rolls gently (Ben, 2026-11-16, revising 2026-10-10's "flat land renders flat"):
  flat tiles showed "no elevation or shading" once the close rungs carried relief.** Plains and
  valleys take a **roll of 0.25** where they took 0, so the fold field, the hills and the slope
  shading lift them a quarter as much as highland - gentle swells, still plainly flatter than
  rolling country. The draw-time rolling ground (§ Level of detail) adds to it up close.
- **Flat land renders flat** (Ben, 2026-10-10, closing sprint 51: *"render flat land as flat -
  currently the whole world seems very rugged and hilly"* — this revises the bullet below;
  its "0 on plains and valleys" is revised in turn by the bullet above). Relief
  follows the tile's **landform**: **plains and valleys are flat** (only the faint grain and patterns
  move their light), **highland rolls**, and the dramatic forms — mountain, canyon, crater, rift —
  keep their drama; the rolling fold field below applies to highland only, and cast shadows fall
  only where there is relief to cast them. The transition from rolling to flat ground is smooth
  across tiles, never hex-shaped. The weight is a per-tile **roll** — 0 on plains and valleys, 1
  on highland and the dramatic forms — interpolated as the relief bias is (the wide
  interpolation, never the edge band). It scales the fold field's floor and altitude share, the
  hills, and the tile height field's slope shading (flat ground keeps none of it); the landform
  accent reads only rolling tiles' bias, so a valley adds none. The cast-shadow height field, the
  crease ink and a road's hillside cut read the same weight, so flat ground casts, inks and cuts
  nothing. The 2026-10-10 rule it revises:
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
- **Forests** stand as pointed crowns (§ The bake, Ben 2026-11-05, overturning the earlier call not to take the conifer change).

Truly painterly trees and towns need authored brushes and stamps; that is a later item, on
the stamp seam that already exists. What the reference owes to its low perspective camera —
silhouetted ridges, foreshortening, distance haze — comes from the 3D ground, not the bake
(§ The 3D ground).

**Assets.** Authored raster brushes and structure stamps — the project's first
shipped raster art beyond fonts — live under **`assets/brushes/`**, resolved
cwd-relative like fonts, with a provenance line per sheet in the brush manifest
(Lua, per the data-definitions boundary). Reference renders and prompt logs live in
`docs/ui/design/renders/` and are design material, never shipped.

---

## The 3D ground (Ben, 2026-11-16)

Ruled after the 3D slice (BL-1665 (3D slice)) stood in for the ground on the canvas: "I much
prefer the 3D look ... work with just 3D from now on". The planetary ground is **true 3D on
the GPU device** behind SDL's GPU renderer (TECH_FOUNDATIONS § Rendering dimension); the flat
ground is retired, with no flat fallback.

- **The terrain is a heightmesh** per ground chunk, coloured by that chunk's bake. A vertex's
  height is the tile height field (the plane through the three nearest tile centres), the
  rolling ground, and each family's detail bumps - mountains' standing taller. The far page is
  a coarse backdrop **only where no chunk is loaded**, without bumps and a little below, so a
  tilted view never runs off the ground and never shows it through the detail.
- **It is lit and shadowed in 3D:** the bake's light, now a sun casting a shadow map over the
  whole view. The bake keeps its own painted shading; the sun applies as a ratio against flat
  ground under the same light, so the two agree.
- **The close-zoom detail layer draws on the mesh** (§ Level of detail): the per-family detail
  textures, each tile's own offset and tone, the bumps as lit surface, wind and creeping sand,
  and the far rungs' flat map - each with the weight by drawn radius it had on the flat
  ground. Parallax and the draw-time shadow marches retire: the mesh and the shadow map are the
  depth now.
- **Ground marks are draped.** Everything the canvas draws flat on the ground - lens washes,
  borders, fronts, a province's selection, roads' highlights, the hover and selection hex - is
  drawn each frame into a texture in ground space and laid over the terrain, so it follows the
  hills and is hidden behind them where they rise.
- **Markers stand upright.** Units, labels, icons and the tile card's anchor stand at their 3D
  point on the ground, facing the screen, at a constant screen size; behind a hill they hide.
- **Clicks pick the terrain.** A click casts a ray from the camera and lands on the first
  ground it meets; hover and every hit test use the same pick.
- **Buildings are procedural geometry.** Each installation's footprint, as the installation
  pass lays it out today (pads, yards, sheds, tanks, towers, a town's blocks), stands as
  low-poly 3D shapes - boxes, pitched roofs, cylinders - lit and shadowed with the ground.
  Roads and paving stay painted on the ground.
- **Trees stand in 3D** (Ben, 2026-11-16: "We can render the trees in 3D as well"). Forest
  and scrub tiles carry low-poly trees - dark conifer cones and rounder broadleaf crowns,
  smaller bushes on scrub - in the number the tile's cover density gives, scattered by a
  seed of the tile so the same forest always stands the same. They keep off roads, rivers
  and installations, stand only where buildings do (the close rungs), and are lit and
  shadowed with everything else. The painted canopy stays as the forest floor, and the
  trees' own shadows darken it. Clicks still pick the terrain: a tree never hides its tile.
- **Nothing in world/\* reads any of it.** The ground is presentation.

The default calibration (proposed by Claude at Ben's request, 2026-11-16): tilt rising from
straight down at the 12 px rung to 50° from straight down at the 96 px rung; a 35° field of
view; tile height ×4 canonical units; bumps at twice the detail layer's; the sun at the bake's
40° from the north-west; a full shadow darkening by half. Calibration, tuned by eye.
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
- **Benchmark:** `ProjectIo --bench-ground` — the five target-hardware budgets
  (TECH_FOUNDATIONS.md § Target hardware) measured on the live pool path, with
  the total paint time, the per-chunk bake cost and the peak process memory.
  `scripts/bench/ground_paint_bench.lua` (under `--verify`, kept out of the
  suite) is the paint in miniature: a whole master, then the partial re-bake
  a round boundary's new installations cause, with the worker CPU per pass —
  a reading that starts in seconds, for comparing builds and bake changes.
- **Headless:** chunk invalidation (a build dirties exactly the intersecting
  chunks; nothing else re-bakes) and brush-manifest integrity are display-free
  checks.
- Captures use bundled cwd-relative assets, but the ground is rasterised by the GPU
  device: a ground golden holds on the machine that blessed it.

Design owners: BL-732 (ground bake renderer) — the mechanism; BL-733 (biome brush &
structure art pipeline) — the authored assets; BL-734 (ground/chrome layer contract)
— the fate of each analytic channel over painterly ground. The 2026-10-08 visibility
pass: BL-1241 (structures baked) — § Installations; BL-1242 (landforms and rivers baked)
and BL-1243 (terrain variant families) — § Mountains, rivers and terrain variety; BL-1244
(never magnify the ground) — § Level of detail.
