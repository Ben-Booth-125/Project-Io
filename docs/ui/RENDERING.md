# Project Io — Canvas Ground Rendering

This document owns **how the Planetary canvas renders its ground**: the tile-atlas
mechanism, the asset policy it introduces, animation, level-of-detail and the
verification story. It is the authority the sprint-29 rendering push builds against.
[PLANETARY.md](PLANETARY.md) keeps ownership of *what* the canvas communicates and of
every analytic channel above the ground; [CANVASES.md](CANVASES.md) keeps the ladder,
layout and shared state. The feasibility study behind these decisions is
`docs/research/CANVAS_RENDERING.md` (research tier, not authority).

Ruled by Ben, 2026-09-01 (the sprint-29 design form): mechanism **authored hex-tile
atlas**; scope **the Planetary tile grid** (the upper rungs are unchanged); assets
**authored raster art**, worked from his reference images; **ambient animation in
scope**. The fate of the analytic channels over the new ground is a separate owned
call (§ The layer contract).

---

## The mechanism — an authored hex-tile atlas

The ground is drawn from **atlas pages of authored raster hex sprites**, one textured
quad per visible tile, on the **existing backend**. No platform change: pages load as
`SDL_Texture`s, register with ImGui's SDLRenderer3 backend (`ImTextureID`), and draw
via `AddImageQuad` inside the same culled, wrap-windowed, row-banded tile loop the
canvas runs today (PLANETARY.md § Draw-loop cost model — that machinery is unchanged
and this doc does not restate it).

A textured quad is ~4 vertices against the flat hex fill's ~10, so the mechanism is
*cheaper* per tile than the render it replaces; the whole-grid view stays within the
budget the rect LOD established.

**The SDL3 GPU API remains the deferred ceiling**, exactly as
`docs/tech/TECH_FOUNDATIONS.md` § Framework holds the door open. Nothing in this
design reaches for it: every capability the atlas needs — textures, alpha
compositing, linear scaling — is in the 2D renderer.

### Sprite geometry and draw order

- **Pointy-top hex sprites** matching the canvas's odd-r grid, transparent outside
  the hex bounds. A sprite may carry an **overdraw apron** — art extending above its
  hex (relief, canopy) — because the existing row-major iteration is already
  top-to-bottom painter's order: a southern tile correctly overlaps its northern
  neighbour's apron.
- The atlas draw lives in **`hex_render`**, beside `ui::terrain_colour`, for the
  standing reason: the Planetary canvas and the Selection band's neighbourhood view
  share one ground implementation so the two surfaces cannot drift.

### The manifest — data-driven, with a vector fallback

Atlas pages ship with a **manifest** (Lua, per the data-definitions boundary in
TECH_FOUNDATIONS) mapping terrain keys to cells: `substrate × cover × density band →
{page, cell, variants, frames, frame_rate}`. Each sheet entry also carries a
**provenance line** (source reference image / author) — authored assets get the
versioning discipline code already has.

**A terrain key the manifest does not cover falls back to the existing vector fill.**
This is load-bearing, not a courtesy: the art arrives incrementally from the
reference images, and the app must run — and every capture must pass — with a partial
atlas. The vector ground is therefore *retired only by coverage*, never by a switch.

### Variants and placement

Per-tile variant selection is **hashed from the tile's grid coordinate**, never from
screen position — the same rule the procedural texture pass established, for the same
two reasons: the cylinder draws several wrap copies of one tile and they must agree,
and a screen-space hash makes the ground crawl under pan.

### Transitions

Terrain boundaries draw **per-edge transition fringes**: the higher-priority terrain
draws its fringe onto the lower-priority neighbour, priority being a fixed authored
order in the manifest, so any two terrains meet the same way everywhere. Fringes obey
the road-span rules: drawn only toward **survey-revealed** neighbours (no leaking the
shape of unsurveyed ground), and short-path across the cylinder seam.

### Ambient animation — flipbook, render-side

Motion is **flipbook animation in the atlas**: a cell may carry N frames at a
manifest-authored rate (water shimmer first; anything a frame sequence can carry).
The frame index derives from a **render-side real-time clock** — never from sim
state, so the `world/*` determinism rule is untouched and animation continues while
the sim is paused, as ambience should.

**Under `--verify` the animation clock is pinned to phase 0.** A capture is a
deterministic function of world state, as today; animation is checked by advancing
the pinned clock explicitly from the script, not by racing it.

### Level of detail

The atlas carries a **far page** — pre-downscaled cells — selected below the existing
**7 px** coarse pivot (`k_lod_radius_px`), because `SDL_Renderer` textures have no
automatic mipmaps and minification past ~2:1 shimmers. Linear scale mode covers the
range between. One pivot, shared with the fill LOD, so there is exactly one zoom
seam to tune.

### Sizing envelope

Pages are capped at **4096²** (safe on every driver the renderer targets; the max is
queryable but never assumed). At a 128 px hex cell that is ~1,000 cells per page —
the full substrate × cover × density × variant × frame roster fits in a handful of
pages, resident always; no streaming.

---

## Asset policy

This is the project's **first shipped raster art beyond fonts**, and it follows the
font precedent (`src/ui/fonts.cpp`): assets live under **`assets/tiles/`**, resolved
cwd-relative first, identical on both OSes so captures agree across machines.

The vector-glyph vocabulary of [ICONS.md](ICONS.md) is **not** displaced: icons,
markers and chrome remain hand-drawn vector. The atlas grant covers the *ground*.

---

## The layer contract

The ground layer sits at the **bottom of the composite**; everything analytic —
lens tints, the land blend, landform glyphs, borders, fog washes, markers, texture
marks — is chrome *above* it. Which of those channels survive unchanged over
authored ground, which retire as redundant (the procedural texture pass and relief
tint are the obvious candidates — the art itself now carries material and relief),
and which restyle, is owned by **BL-734 (ground/chrome layer contract)**, to be
settled against the reference images. Until that settles, every channel keeps its
current spec in PLANETARY.md / CANVASES.md.

---

## What this design deliberately excludes

- **Upper rungs.** Solar and Circumplanetary keep their current styles; the baked
  globe idea stays in the research note.
- **Baked terrain chunks and the GPU pipeline.** Both remain open doors (the research
  note's options C and D); the atlas neither needs nor precludes them.
- **Per-pixel effects** — real-time lighting, shader water. Motion in this design is
  flipbook frames, full stop.

---

## Verification

- **Visual:** a `scripts/verify/tile_atlas.lua` check (verifier-visual) — atlas
  ground at play zoom, the far page below the pivot, a transition boundary, the
  fallback on an uncovered key, and an animation frame advanced via the pinned clock.
- **Headless:** manifest integrity (every referenced page/cell exists; variant and
  frame counts in range) is a data check, testable without a display.
- Captures remain machine-portable: same rasteriser path as today, assets bundled
  and cwd-relative like fonts.

Design owners: BL-732 (hex tile atlas renderer) — the mechanism above; BL-733 (tile
art asset pipeline) — the sheets, manifest and authoring flow from the reference
images; BL-734 (ground/chrome layer contract) — the fate of each analytic channel.
