#pragma once
// gpu_effects - the fragment-shader seam on SDL's 2D renderer (BL-1655 (shader seam);
// TECH_FOUNDATIONS.md § Framework, "SDL 3.4, and fragment shaders on the 2D renderer").
//
// An EFFECT is one DXIL fragment shader wrapped in an SDL_GPURenderState. A canvas
// switches an effect on for the draws that follow it on an ImGui draw list with
// begin(), and off with end(); both record draw-list callbacks, so the switch lands
// exactly between the draws it brackets, in draw order.
//
// Every effect is OPTIONAL. available() is false when the renderer is not SDL's GPU
// renderer (the --verify software path, a machine without one), when the build had no
// shader compiler, or when the shader failed to load - and every caller then draws its
// no-shader path. Nothing in world/* ever reads what an effect draws.

#include <cstddef>
#include <cstdint>

struct SDL_Renderer;
struct SDL_Texture;
struct ImDrawList;

namespace gfx {

enum class effect : int
{
    identity = 0, ///< the texture times the vertex colour: the seam's self-check
    lens_wash,    ///< a lens wash from a per-tile colour table (BL-1656; uniforms: float4, x = edge band)
    border_band,  ///< the national border rule from an owner table (BL-1657; uniforms: float4)
    water_glint,  ///< moving light on open water from a water table (BL-1660; uniforms: float4)
    count
};

/// Creates every effect the renderer and the build can carry. Call once, after the
/// renderer exists, on its thread. Returns how many effects are available.
int init(SDL_Renderer* renderer);
/// Destroys every effect. Call before the renderer is destroyed.
void shutdown();

/// True when the renderer is SDL's GPU renderer (so effects are possible at all).
bool gpu_renderer();
/// True when effect `e` loaded and may be used this frame.
bool available(effect e);

/// Record on `dl`: the draws after this use effect `e` until end(). No-op if
/// unavailable (the caller should have drawn its fallback instead).
void begin(ImDrawList* dl, effect e);
/// Record on `dl`: the draws after this use SDL's own shaders again.
void end(ImDrawList* dl);

/// Fragment uniform block 0 of effect `e` (copied; pushed at each draw that uses it).
void set_uniforms(effect e, const void* data, std::size_t len);

/// Binds `n` extra textures at sampler slots 1..n of effect `e` (slot 0 is always the
/// draw's own texture). `linear` picks bilinear over nearest sampling for all of them.
/// Rebuilds the effect's render state; call when the textures change, not per frame.
/// Returns false (and leaves the effect unavailable) if a texture has no GPU texture.
bool bind_textures(effect e, SDL_Texture* const* textures, int n, bool linear);

/// A small RGBA32 texture (nearest, alpha-blended) an effect reads as its DRAW
/// texture - a lookup table, one texel per tile. `slot` (0-3) names the table; it is
/// re-uploaded only when `px` differs from the last upload, so a table that does not
/// change costs one compare a frame. Null without the GPU renderer.
SDL_Texture* table_texture(int slot, int w, int h, const std::uint32_t* px);

} // namespace gfx
