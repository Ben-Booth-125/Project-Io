# Project Io — Map Rendering Strategies (research)

> **Settles:** nothing — research scaffolding. What comparable large-map strategy games do that
> Io's ground renderer does not, and a candidate path.
> **Not here:** how Io renders today (`../ui/RENDERING.md`) · settled technical decisions
> (`../tech/TECH_FOUNDATIONS.md`).
> **Confused with:** CANVAS_RENDERING.md, ../ui/RENDERING.md.

Gathered 2026-11-12 on Ben's question: other games with similarly large maps look far more capable
graphically. What should Io pursue?

## Where Io stands

The ground is painted on the CPU into 512 px chunks from one 96 px/hex master per body, with a
hand-built mip chain, and drawn as textured quads through SDL3's 2D `SDL_Renderer`
(`src/ui/ground_bake.cpp`, `src/core/ground_layer.cpp`). There is no per-pixel shader. Every
visible change is a re-bake by content hash. A lens re-bake can cost tens of seconds of worker
time. Zoom never adds detail, and animation is flipbook overlays.

## What separates the comparison maps

| Technique | Who | Needs |
|---|---|---|
| **Data textures and a shader.** Province or tile IDs live in a texture, and a palette lookup recolours them per pixel. | Paradox-style maps ([Silkroads write-up](https://thomas-slade.squarespace.com/development-blog/2019/6/4/silkroads-europa-universalis-iv-style-province-shader)) | Fragment shaders |
| **Borders as distance fields.** Jump flooding builds the field, and the shader draws smooth borders at any zoom. | Community re-implementations of the Paradox look | The above |
| **A zoom-dependent far view.** A paper or political map far out, terrain close in. | CK3 ([DD177](https://devtrackers.gg/crusader-kings/p/3c2f2816-dev-diary-177-a-fresh-coat-of-paint)), Victoria 3 ([DD49](https://www.paradoxinteractive.com/games/victoria-3/news/dev-diary-49-graphic-overview)) | A ruling on "zoom adds no detail" |
| **Shader water, lighting and atmosphere.** | Victoria 3, CK3, Factorio ([FFF #323](https://cf-www.factorio.com/blog/post/fff-323)) | Fragment shaders |
| **Erosion heightmaps and runtime material masks.** | CK3 (Gaea), Factorio ([FFF #214](https://cf-www.factorio.com/blog/post/fff-214)) | Mostly art pipeline |
| **True 3D terrain and camera.** | Civ, Humankind, Clausewitz | A renderer milestone; Io is ruled 2D |

## A candidate path

1. **Shaders inside the existing renderer.** SDL 3.4 adds GPU render states to `SDL_Renderer`
   ([SDL wiki](https://wiki.libsdl.org/SDL3/SDL_CreateGPURenderState)), so ImGui and the shell stay.
   Try the grade as a runtime pass, and animated water, first. Io pins SDL 3.2.0.
2. **Move lenses, borders, fog and ownership onto data textures.** The painterly bake stays as the
   base, and what changes at play speed leaves it. This removes the lens re-bakes.
3. **A far view drawn in its own style,** replacing the shrunk paintings at the farthest zooms.

**Costs:** visual goldens become driver-dependent, and shaders bring a cross-compile toolchain.
Adding shaders is a new technical decision against TECH_FOUNDATIONS' "baked on the 2D renderer".
Determinism is untouched, since rendering only reads the world.

**Gaps:** no primary source was found on Civ VI, Humankind, Old World, Shadow Empire or Songs of Syx
internals. The Paradox ID-map details come from community re-implementations.
