#pragma once
// ground_slice_3d - THE 3D GROUND (RENDERING.md § The 3D ground and § The camera; Ben,
// 2026-11-16: "work with just 3D from now on"). The planetary canvas's ground drawn as a
// lit heightmesh with a depth buffer, on the GPU renderer's own device: one grid per
// visible ground chunk (the chunk's straight-down bake as its colour), a coarse backdrop
// from the far page where no chunk is loaded, installations as low-poly geometry
// (BL-1670 (procedural 3D buildings)), the close-zoom detail layer and the far rungs' flat
// map in the fragment shader (BL-1667 (3D detail layer)), and the canvas's ground marks
// draped over it from a texture the canvas draws each frame (BL-1668 (draped ground
// marks)).
//
// ONE CAMERA (BL-1669 (upright markers, ray-pick)): the canvas sets it at the top of each
// frame from its view (begin_frame), then projects every marker through it and picks the
// terrain under the cursor with it, so what is drawn and what is pressed agree.
//
// Heights are the tile height field (the plane through the three nearest tile centres),
// the rolling ground and the coarse detail bumps, at the tuning panel's scales.

#include <cstdint>
#include <memory>

struct SDL_Renderer;
struct SDL_Texture;

namespace ui::ground { struct bake_source; }

namespace gfx::slice {

struct chunk
{
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0; ///< canonical units (hex circumradius = 1)
    SDL_Texture* tex = nullptr;            ///< the chunk's bake
};

/// The canvas view the camera is set from, before the frame draws anything.
struct view
{
    float cx = 0, cy = 0; ///< the canonical point at the grid area's centre
    float view_h = 1;     ///< the grid area's height in canonical units, looking straight down
    float draw_r = 96;    ///< the drawn hex radius in px - the zoom the tilt follows
    float x = 0, y = 0, w = 1, h = 1; ///< the grid area, logical px
    float fb = 1;         ///< framebuffer px per logical px
};

/// The ground's tile table: one texel per tile (IM_COL32: r 255 on land, g the detail
/// family, b 255 on a mountain, a height x 255); zero off the survey. Copied.
void set_tiles(const std::uint32_t* tiles, int gw, int gh);

/// Sets this frame's camera. False when the 3D ground cannot run (not the GPU renderer, no
/// shaders): the canvas then draws as it did before the 3D ground.
bool begin_frame(SDL_Renderer* renderer, const view& v);
/// Whether the 3D ground ran this frame (begin_frame returned true).
bool active();

/// A canonical ground point to a screen point (logical px), at the ground's height there.
/// False behind the camera.
bool project(float cx, float cy, float& sx, float& sy);
/// Whether the ground point is in sight (no terrain between it and the eye).
bool in_sight(float cx, float cy);
/// The canonical ground point under a screen point: the view ray met with the height field.
/// False when the ray meets no ground (the sky).
bool pick(float sx, float sy, float& cx, float& cy);
/// The canonical bounding box of the ground in view.
void footprint(float& x0, float& y0, float& x1, float& y1);
/// The ground's height (canonical units) at a canonical point.
float height(float cx, float cy);

/// The draw-time detail and flat map (BL-1667): the canvas's tables, textures and dials.
struct detail
{
    SDL_Texture* tiles = nullptr;   ///< the tile table (set_tiles' packing), by Load
    SDL_Texture* map = nullptr;     ///< the flat map's table (rgb colour, a 128 + river sides)
    SDL_Texture* coarse = nullptr;  ///< gfx::detail_texture(0)
    SDL_Texture* fine = nullptr;    ///< gfx::detail_texture(1)
    SDL_Texture* rolling = nullptr; ///< gfx::detail_texture(2)
    float strength = 0, period = 4, fine_strength = 0, fine_period = 2;
    float r_none = 24, r_full = 48;          ///< drawn radii the detail fades in over
    float map_r_none = 16, map_r_full = 6;   ///< ... and the flat map
    float coast_px = 0.75f, river_px = 0.5f; ///< ink half-widths, screen px
    float edge_band = 0.10f, tone_swing = 0.10f, mountain_mul = 2.5f, fine_bump_share = 0.3f;
    float gust = 0.06f, flutter = 0.12f, creep = 0.008f;
    float bump[4] = { 0.04f, 0.08f, 0.03f, 0.10f }; ///< vegetation, forest, arid, rock
    float famk[4] = { 1, 1, 1, 1 };
    float wind[4] = { 1, 0.6f, 0, 0 };
    float time = 0;
};

struct inputs
{
    const chunk* chunks = nullptr;
    int n_chunks = 0;
    chunk far;               ///< the far page (the whole body, low resolution): the backdrop
    float lift_gamma = 1.0f; ///< the land lift's gamma (1 = off)
    detail det;
    /// The canvas's ground marks, drawn this frame (premultiplied alpha); uv = canonical *
    /// (ax, ay) + (bx, by). Null: no marks.
    SDL_Texture* drape = nullptr;
    float drape_ax = 0, drape_ay = 0, drape_bx = 0, drape_by = 0;
    /// The bake's source snapshot: the installations stood up as geometry. Null: none.
    std::shared_ptr<const ui::ground::bake_source> src;
};

/// Renders this frame's ground into the frame texture. Call after begin_frame, once the
/// drape is drawn; the renderer's queued work is flushed first.
void render(const inputs& in);
/// The frame texture, sized to the grid area (valid after begin_frame returned true).
SDL_Texture* frame();
/// This frame's drape coordinates, one texel per frame pixel (drape.frag.hlsl reads them):
/// valid once render() ran. The canvas draws them over the frame through effect::drape.
SDL_Texture* drape_coords();
/// Bumped whenever drape_target makes a new texture. A new texture can land at the address
/// of the one it replaced, so a binding is checked by this, never by the pointer.
int drape_generation();
/// A render-target texture of at least w x h for the canvas to draw the drape into.
SDL_Texture* drape_target(int w, int h);

/// The tuning panel (beside the F11 frame HUD). Call inside an ImGui frame.
void draw_panel();
/// Releases every GPU object. Call before the renderer is destroyed.
void shutdown();

} // namespace gfx::slice
