#pragma once
// ground_slice_3d - BL-1665 (3D slice): the visible ground drawn as a heightmesh with a
// depth buffer on the GPU renderer's own device, in a window of its own, so true 3D can be
// judged beside the fixed-angle ground (TECH_FOUNDATIONS.md § Rendering dimension, "Toward
// 3D, in two steps"). A MEASUREMENT, not a renderer: it replaces no layer, takes no input
// from the map, and nothing reads what it draws. It reports what it cost.
//
// The mesh: one grid per visible ground chunk, the chunk's own bake as its colour; heights
// are the tile height field (the plane through the three nearest tile centres, as the
// ground shader's relief), the rolling ground and the coarse detail bumps, each at a
// window slider's scale. The window opens beside the F11 frame HUD.

#include <cstdint>

struct SDL_Renderer;
struct SDL_Texture;

namespace gfx::slice {

struct chunk
{
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0; ///< canonical units (hex circumradius = 1)
    SDL_Texture* tex = nullptr;            ///< the chunk's bake
};

struct inputs
{
    SDL_Renderer* renderer = nullptr;
    /// One texel per tile, the ground shader's tile table packing (IM_COL32: r land,
    /// g detail family, b 255 on a mountain, a height x 255); zero off the survey.
    const std::uint32_t* tiles = nullptr;
    int gw = 0, gh = 0;
    const chunk* chunks = nullptr;
    int n_chunks = 0;
    float lift_gamma = 1.0f; ///< the land lift's gamma (1 = off)
    /// The far page (the whole body, low resolution): a backdrop mesh under the chunks, so
    /// a tilted view does not run off the loaded ground.
    chunk far;
    /// The canvas view, for "Show on the canvas": its centre and height in canonical units,
    /// and its size in pixels.
    float view_cx = 0, view_cy = 0, view_h = 1;
    float draw_r = 96.0f; ///< the drawn hex radius in px - the zoom the tilt follows
    int   view_px_w = 0, view_px_h = 0;
    /// Whether the slice's window is drawn this frame (the F11 HUD is open).
    bool  window = false;
};

/// The slice's frame: renders when switched on, and draws its window when in.window.
/// Call inside an ImGui frame. Does nothing without the GPU renderer.
void draw_window(const inputs& in);
/// True while "Show on the canvas" is on (Ben, 2026-11-16: "wire that into the main canvas
/// so I can comment more accurately on it") - the canvas should call draw_window every
/// frame then, window or not.
bool on_canvas();
/// The frame rendered this frame for the canvas, sized to the canvas view; null when
/// "Show on the canvas" is off or nothing rendered.
SDL_Texture* canvas_frame();
/// Releases every GPU object. Call before the renderer is destroyed.
void shutdown();

} // namespace gfx::slice
