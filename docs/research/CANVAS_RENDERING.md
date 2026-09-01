# Research — Detailed Canvas Rendering (Sprint 29)

**Research scaffolding, not authority.** This note is the feasibility study behind the
rendering push: can the Planetary canvas (and the ladder above it) move from the minimal
vector style to a much more detailed render, and by which mechanism? The settled outcome
lives in the rendering authority doc once Ben rules; this file keeps the option space and
the measurements that decided it.

Date: 2026-09-01. State of the code at time of writing: everything on every canvas is
untextured `ImDrawList` vector geometry through ImGui's SDLRenderer3 backend
(`src/core/app.cpp` — `SDL_CreateRenderer` + `ImGui_ImplSDLRenderer3_Init`). No
`SDL_Texture` exists anywhere outside the font atlas; captures read the finished frame
with `SDL_RenderReadPixels` (`src/core/app_capture.cpp`).

---

## 1. What a detailed renderer has to eat — the data is already generated

No new generation work is needed. Per tile, live on `tile_component`
(`src/world/components.hpp`):

| Field | Feeds |
|---|---|
| `substrate` (10 values) | Base material — ground colour/texture family |
| `cover` × `cover_density` (10 values × 0–255) | Vegetation/overlay layer, graded |
| `landform` (7 values) | Dramatic feature placement (mountain, canyon, crater, rift) |
| `height` [0, 1] (BL-517, continuous) | **Relief shading, slope lighting, coastal shelf** |
| `river_edges` + `river_down` bitfields | River courses with flow direction |
| `road_level` (0–3) | Road rendering by tier |
| `habitability`, `hazard_level` | Optional ambience (colour grading) |

The continuous `height` scalar is the load-bearing one: hillshading (light from a fixed
azimuth, slope from the height difference to neighbours) is the single technique that
most moves a map from "diagram" to "terrain", and the field for it is already persisted
per tile — the same number `generation_record::height` reports.

Body-level: `planetology` gives atmosphere/chemistry (sky/tint per body), and the
Continents pass gives plate/ocean structure. Bodies on the Solar/Circumplanetary rungs
are currently flat discs (`body_style` in both canvas files) — any baked surface could
replace them.

## 2. What the current backend can and cannot do

SDL3's 2D renderer (D3D11/12 on Windows, GL/Vulkan on Linux) supports, today, without
any platform change:

- **Textures** — `SDL_Texture`, usable from inside ImGui draw lists
  (`ImTextureID` = `SDL_Texture*` in the SDLRenderer3 backend, `AddImage*`), so a
  textured terrain layer slots into the existing canvas draw pass unchanged.
- **Render targets** — `SDL_SetRenderTarget`: bake arbitrarily rich terrain into
  textures once (or when dirty), then draw the baked result per frame at ~zero cost.
- **Textured triangle geometry with per-vertex colour** — `SDL_RenderGeometry`: enough
  for gradient lighting, sphere-warping a baked surface onto a globe disc, and
  soft-edged compositing. This is the same primitive ImGui itself renders through.

What it cannot do: per-pixel shaders. No real-time lighting from a normal map, no
animated water/cloud shaders, no palette-lookup post-processing. Anything per-pixel has
to be **baked** (computed CPU-side or composed from pre-made layers into a texture) or
faked with vertex colour.

The **SDL3 GPU API** removes that ceiling — custom shader pipelines, stable since SDL
3.2.0, with an existing ImGui backend (`ImGui_ImplSDLGPU3`). `TECH_FOUNDATIONS.md`
§ Framework chose SDL3 partly to keep this door open. The cost: the whole app's render
path migrates backends (every panel still renders, but init/present/capture all change),
shaders enter the toolchain (HLSL/SPIR-V cross-compile), and **pixel captures stop being
machine-portable** — GPU rasterisation differs across drivers, which touches the visual
verification harness (see § 5).

## 3. The option ladder

Four rungs, in ascending ambition. B and C compose; D can arrive later *underneath* the
same layer contract.

### A. Richer procedural vector (status quo, pushed)
More `ImDrawList` passes: denser texture marks, more relief bands, vector shore/foam.
**Verdict: bounded.** The whole-grid view is already vertex-emission-bound (155k verts,
14.25 ms submit before the rect LOD — PLANETARY.md § Fill LOD), and every added pass
scales per-tile-per-frame. This rung cannot reach "much more detail"; it is the style
we are moving away from.

### B. Textured hex art (atlas on the current backend)
An atlas of tile variants (per substrate/cover/density band, plus transition edges),
each hex drawn as a textured quad. The standard hex-game look (Civ-like at the tile
grain). ~4 verts/tile — *cheaper* than today's 6-gon fill. Needs a transition-edge
scheme (a blend mask per neighbouring-terrain edge, or dual-grid corner tiles) or the
tile boundaries read as a checkerboard.
**Lift: moderate.** New: atlas loader, art pipeline, transition logic. The open question
is where the art comes from (§ 4).

### C. Baked terrain chunks (render-target bake) — the interesting rung
Terrain is rendered **once** into per-chunk textures (e.g. 16×16-tile chunks) at one or
two resolutions, using unlimited draw passes — hillshade from `height`, soft biome
blending, river carving, coastal shelf gradients, per-substrate grain at any density —
because bake cost is off the frame path. The frame then draws N visible chunk quads.
Re-bake only dirty chunks (terrain changes are rare: urban transform, roads, builds).

- **Frame cost collapses**: the whole-grid view becomes ~dozens of quads instead of
  15,120 fills — the current LOD machinery (7 px rect fill, 14 px texture gate) becomes
  unnecessary for the base layer.
- **Memory is the constraint to engineer**: 180×84 at 48 px hex radius full-res is
  ~15,000×6,000 px ≈ 360 MB RGBA — so a **chunk cache around the viewport** at full
  res plus one low-res whole-body mip (~8 px/hex, ~16 MB) for far zoom. Texture max
  size (typically 8192–16384) forces chunking anyway.
- Composes with B: the bake can *stamp authored brushes* (raster art) or draw
  procedural vector marks, or both — the bake mechanism is agnostic about its brush.

**Lift: the chunk cache + bake scheduler is real engineering, but it is plumbing, not
research risk.** Everything it needs is in the current backend.

### D. SDL3 GPU pipeline (shaders)
Real-time lit relief (normal map from the height field), animated water, day/night
terminator, palette post-grades. The highest ceiling and the only rung with **motion**
in the terrain itself.
**Lift: large** — backend migration, shader toolchain, capture portability (§ 5). Not a
sprint; a milestone. The layer contract (§ 6) is designed so D can replace C's bake
later without the chrome above noticing.

### The globe (Circumplanetary/Solar discs)
Independent of the rung chosen: bake the body surface to a low-res equirect texture
(the cylinder grid *is* equirect-ish already), warp it onto the disc with a precomputed
`SDL_RenderGeometry` mesh, add a vertex-colour terminator/limb shade. A planet on the
upper rungs then shows its actual generated face. Cheap once any bake exists; almost
free under option C.

## 4. The doctrinal collisions (the real questions)

1. **`TECH_FOUNDATIONS.md` § Rendering dimension**: *"Detailed surface visualisation is
   not a prototype concern."* Sprint 29 overturns this sentence; the doc must be
   amended, not worked around.
2. **The no-asset idiom**: ICONS.md's hand-drawn-vector vocabulary and PLANETARY.md's
   texture pass ("no atlas, no art assets") are a deliberate style. Options B/C admit
   raster assets for the first time — authored (hand or AI-generated from the reference
   images) or procedurally baked. This is an *asset pipeline* decision as much as a
   style one: authored art needs sourcing, versioning, and a licence story; procedural
   bake keeps everything in code but caps fidelity at what code can draw.
3. **The analytic layer**: the lens system, land blend, landform channels, borders,
   fog, and markers are a designed, verified reading surface. A detailed base render
   should **replace the base fill/texture/relief channels and leave every analytic
   channel drawn as vector chrome on top** — otherwise every lens question reopens.
4. **Production-UI alignment**: canvases currently draw inside ImGui. A terrain layer
   drawn by direct SDL calls *beneath* the ImGui frame would decouple the map from
   ImGui — a step toward the planned post-prototype retained UI, and it makes backend
   option D a swap of one layer rather than of everything.

## 5. Determinism and verification

Rendering never writes sim state, so the `world/*` determinism rule is untouched by any
option. The verification story differs by rung:

| Rung | Capture behaviour |
|---|---|
| A/B/C | CPU-deterministic given the same code+data on one machine; portable in practice (same rasteriser path as today) |
| D | Shader output varies by GPU/driver — per-pixel goldens break across machines; checks move to structural assertions + tolerance-based image compare |

The golden policy (curated world-independent set only) already anticipates this; a bake
also adds one new class of check — "chunk re-bake fires on terrain change" — which is a
headless-testable cache invariant, not a visual one.

## 6. The end-state choice — fixed-oblique 2.5D vs full 3D

Written 2026-09-01 at Ben's request, after the staged ruling: the C-F painterly bake
ships under the current camera, and the project then moves toward one of these two.
This section is the decision's briefing paper.

### What each one is

**Fixed-oblique 2.5D (pre-rendered).** The ground is baked in an oblique projection —
tile positions displaced vertically by the height field, rows composited back-to-front
— and structures/trees are pre-rendered sprites at the same fixed angle. Zoom is
scale; the "rotate on zoom" of the references becomes a **transition between two fixed
states** (near-orbital top-down page ↔ oblique page), a crossfade, never a free
camera. The lineage is AoE2 / Songs of Conquest.

**Full 3D (SDL3 GPU path).** Heightmesh terrain chunks, structure models (or
billboard impostors), a real perspective camera. Continuous tilt from orbital to
low-oblique — the it1 behaviour literally — plus real lighting, shadow, day/night,
shader water, and the near-future grade as a post-process.

### The trade-offs

| Axis | 2.5D pre-rendered | Full 3D |
|---|---|---|
| **Camera** | Frozen angle(s); tilt is an illusion between two states; no rotation ever | The reference behaviour, continuous and free |
| **Backend** | Current `SDL_Renderer` + ImGui unchanged | GPU API migration: new init/present/capture, ImGui backend swap (`ImGui_ImplSDLGPU3`), shader toolchain (HLSL/SPIR-V cross-compile) |
| **Engineering scale** | Sprint-to-sprints: oblique bake + sprite compositor over the existing chunk machinery | A renderer milestone: mesh LOD, frustum, materials, shadows, NPR shading — months solo |
| **Art burden** | High and angle-locked: every structure × owner × zoom pre-rendered at the fixed angle; terrain brushes re-authored for oblique | Different, not smaller: models or impostors + materials; but ONE authoring per asset serves every angle and zoom |
| **Reuse from the staged bake** | Partial — top-down brushes re-drawn for oblique; the grade recipe and palette carry | Strong — brushes become textures/materials; the bake's pass structure is the material definition; grade becomes a post pass |
| **Reuse OF it, later** | Mostly discarded if 3D follows — the oblique bake pipeline and angle-locked sprites are the throwaway | Terminal — nothing after it |
| **Style risk** | Low: painterly is native to pre-rendering | Real: C-F painterly in 3D is deliberate NPR work (stylised shading, painted textures); the default gravity is photoreal, which round 2 already rejected (A-F "a tad too realistic") |
| **Interaction** | Hit-testing on height-displaced ground needs an inverse-displacement walk; tractable, mildly fiddly | Standard ray-pick; cleaner than 2.5D once built |
| **Perf envelope** | Trivial (quads) | Modest for this world size, but a real envelope to engineer on low-spec |
| **Verification** | Captures stay machine-portable | GPU rasterisation varies by driver: pixel goldens break cross-machine; checks move to structural asserts + tolerance compare |
| **Animation ceiling** | Flipbook overlays only | Native: shader water, particles, lit smoke, terminator |

### The advisor's read

**If the end-state is ever 3D, go to 3D directly and skip 2.5D.** The two share almost
no work: 2.5D's distinctive costs (the oblique bake pipeline, angle-locked sprite
sets) are exactly the parts 3D throws away, so taking 2.5D first pays a large
art-and-engineering bill for a waypoint. The staged top-down bake already banks
everything transferable (brushes, grade, palette, the swappable ground-layer seam).

Choose **2.5D as the destination** only if the frozen camera is judged *sufficient
forever* — it delivers the close-up look at the lowest engine risk, on the current
backend, with portable captures. Choose **3D** if the reference camera behaviour
(continuous tilt, "standing on the planet") is part of the game's identity — the
prompts and it1 suggest it is. The one 3D risk to respect is style drift toward
photoreal; the mitigation is authoring the NPR look first (a vertical slice judged
against it3's C-F panel before any systems work).

## 7. Recommended shape (advisor's view — superseded)

*Ruled past, 2026-09-01: Ben's first form picked the hex atlas; the reference renders
(`docs/ui/design/renders/map/`) then falsified per-tile sprites for grid-free
continuous ground, and the second form settled on the baked-chunk mechanism this
section originally argued for, with authored biome brushes and the C-F direction.
`docs/ui/RENDERING.md` is the authority; kept below as the original reasoning.*

**C as the sprint-29 target, B's brushes optional inside it, D explicitly deferred with
the door held open.** Concretely: a `terrain_layer` that owns baked chunk textures per
body, drawn as the base of the Planetary canvas; the existing hue/blend/texture/relief
code becomes the *first* bake brush (so day one looks identical, de-risked), then detail
brushes are added bake-side where frame cost no longer exists; every analytic channel
stays vector on top under the layer contract; the globe warp on the upper rungs comes
from the same bake. The authority doc then owns the layer contract, the bake/invalidate
model, the LOD story, and the asset policy Ben rules.
