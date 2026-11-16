#include "gpu_effects.hpp"

#include <SDL3/SDL.h>
#include <imgui.h>

#include "ui/png_decode.hpp"
#include "core/ground_slice_3d.hpp"

#include <array>
#include <fstream>
#include <iterator>
#include <string>
#include <cstdint>
#include <cstring>
#include <vector>

// The DXIL headers DXC writes at build time (CMakeLists.txt § Fragment shaders).
// They exist only when DXC ran; __has_include keeps this TU's defines equal to the PCH's.
#if __has_include("shaders/identity.dxil.h")
#define IO_HAVE_DXIL 1
#include "shaders/identity.dxil.h"
#include "shaders/lens_wash.dxil.h"
#include "shaders/border_band.dxil.h"
#include "shaders/water_glint.dxil.h"
#include "shaders/ground.dxil.h"
#include "shaders/drape.dxil.h"
#endif

namespace gfx {
namespace {

struct effect_def
{
    const char*          name;
    const unsigned char* dxil;
    std::size_t          dxil_size;
    int                  extra_samplers;  ///< bound by bind_textures, slots 1..n
    int                  uniform_buffers; ///< fragment uniform blocks
};

#if defined(IO_HAVE_DXIL)
#define IO_DXIL(name) reinterpret_cast<const unsigned char*>(g_##name##_dxil), sizeof(g_##name##_dxil)
#else
#define IO_DXIL(name) nullptr, 0
#endif

const std::array<effect_def, static_cast<std::size_t>(effect::count)> k_defs = {{
    { "identity",  IO_DXIL(identity),  0, 0 },
    { "lens_wash", IO_DXIL(lens_wash), 0, 1 },
    { "border_band", IO_DXIL(border_band), 0, 1 },
    { "water_glint", IO_DXIL(water_glint), 0, 1 },
    { "ground", IO_DXIL(ground), 5, 1 },
    { "drape", IO_DXIL(drape), 1, 0 },
}};

struct effect_slot
{
    SDL_GPUShader*      shader  = nullptr;
    SDL_GPURenderState* state   = nullptr;
    SDL_GPUSampler*     sampler = nullptr;
    bool                ok      = false;
    std::vector<std::uint8_t> uniforms; ///< re-applied when the state is rebuilt
};

struct table_slot
{
    SDL_Texture* tex = nullptr;
    int w = 0, h = 0;
    std::vector<std::uint32_t> last; ///< what was last uploaded (skip an equal upload)
};
std::array<table_slot, 5> g_tables;

/// BL-1658 (close-zoom detail layer): the coarse and fine detail textures.
struct detail_slot
{
    SDL_Texture* tex   = nullptr;
    bool         tried = false;
};
std::array<detail_slot, 3> g_detail;

SDL_Renderer*  g_renderer = nullptr;
SDL_GPUDevice* g_device   = nullptr;
std::array<effect_slot, static_cast<std::size_t>(effect::count)> g_slots;

effect_slot& slot(effect e) { return g_slots[static_cast<std::size_t>(e)]; }
const effect_def& def(effect e) { return k_defs[static_cast<std::size_t>(e)]; }

bool make_state(effect e, const SDL_GPUTextureSamplerBinding* bindings, int n)
{
    effect_slot& s = slot(e);
    if (s.state)
    {
        SDL_DestroyGPURenderState(s.state);
        s.state = nullptr;
    }
    SDL_GPURenderStateCreateInfo ci{};
    ci.fragment_shader      = s.shader;
    ci.num_sampler_bindings = n;
    ci.sampler_bindings     = bindings;
    s.state = SDL_CreateGPURenderState(g_renderer, &ci);
    if (!s.state)
    {
        SDL_Log("gpu_effects: %s: render state failed: %s", def(e).name, SDL_GetError());
        return false;
    }
    if (!s.uniforms.empty()
        && !SDL_SetGPURenderStateFragmentUniforms(s.state, 0, s.uniforms.data(),
                                                  static_cast<Uint32>(s.uniforms.size())))
    {
        SDL_Log("gpu_effects: %s: uniforms failed: %s", def(e).name, SDL_GetError());
        return false;
    }
    return true;
}

void cb_begin(const ImDrawList*, const ImDrawCmd* cmd)
{
    const auto e = static_cast<effect>(reinterpret_cast<std::intptr_t>(cmd->UserCallbackData));
    if (g_renderer && available(e))
        SDL_SetGPURenderState(g_renderer, slot(e).state);
}

void cb_end(const ImDrawList*, const ImDrawCmd*)
{
    if (g_renderer && g_device)
        SDL_SetGPURenderState(g_renderer, nullptr);
}

} // namespace

int init(SDL_Renderer* renderer)
{
    g_renderer = renderer;
    g_device   = renderer ? SDL_GetGPURendererDevice(renderer) : nullptr;
    SDL_ClearError();
    if (!g_device)
    {
        SDL_Log("gpu_effects: renderer '%s' is not the GPU renderer - no-shader paths",
                renderer ? SDL_GetRendererName(renderer) : "(none)");
        return 0;
    }
    // The lever: every effect off, the GPU renderer kept (a shader under suspicion).
    if (SDL_getenv("IO_NO_GPU_EFFECTS"))
    {
        SDL_Log("gpu_effects: IO_NO_GPU_EFFECTS is set - no-shader paths");
        g_device = nullptr;
        return 0;
    }
    const SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(g_device);
    int n_ok = 0;
    for (int i = 0; i < static_cast<int>(effect::count); ++i)
    {
        const auto e = static_cast<effect>(i);
        const effect_def& d = def(e);
        effect_slot& s = slot(e);
        if (!d.dxil || !(formats & SDL_GPU_SHADERFORMAT_DXIL))
        {
            SDL_Log("gpu_effects: %s: no DXIL shader for this device - no-shader path", d.name);
            continue;
        }
        SDL_GPUShaderCreateInfo si{};
        si.code_size           = d.dxil_size;
        si.code                = d.dxil;
        si.entrypoint          = "main";
        si.format              = SDL_GPU_SHADERFORMAT_DXIL;
        si.stage               = SDL_GPU_SHADERSTAGE_FRAGMENT;
        si.num_samplers        = static_cast<Uint32>(1 + d.extra_samplers);
        si.num_uniform_buffers = static_cast<Uint32>(d.uniform_buffers);
        s.shader = SDL_CreateGPUShader(g_device, &si);
        if (!s.shader)
        {
            SDL_Log("gpu_effects: %s: shader failed: %s", d.name, SDL_GetError());
            continue;
        }
        // An effect with extra samplers waits for bind_textures; one without is ready.
        if (d.extra_samplers == 0)
            s.ok = make_state(e, nullptr, 0);
        n_ok += s.ok ? 1 : 0;
    }
    SDL_Log("gpu_effects: GPU renderer, %d of %d effects ready", n_ok, static_cast<int>(effect::count));
    return n_ok;
}

void shutdown()
{
    slice::shutdown(); // BL-1665 (3D slice): its GPU objects, before the device goes
    for (effect_slot& s : g_slots)
    {
        if (s.state)   SDL_DestroyGPURenderState(s.state);
        if (s.shader)  SDL_ReleaseGPUShader(g_device, s.shader);
        if (s.sampler) SDL_ReleaseGPUSampler(g_device, s.sampler);
        s = effect_slot{};
    }
    for (table_slot& t : g_tables)
    {
        if (t.tex) SDL_DestroyTexture(t.tex);
        t = table_slot{};
    }
    for (detail_slot& d : g_detail)
    {
        if (d.tex) SDL_DestroyTexture(d.tex);
        d = detail_slot{};
    }
    g_renderer = nullptr;
    g_device   = nullptr;
}

bool gpu_renderer() { return g_device != nullptr; }

SDL_Renderer* renderer() { return g_renderer; }

bool available(effect e) { return g_device && slot(e).ok && slot(e).state; }

bool loaded(effect e) { return g_device && slot(e).shader; }

void begin(ImDrawList* dl, effect e)
{
    if (!available(e))
        return;
    dl->AddCallback(&cb_begin, reinterpret_cast<void*>(static_cast<std::intptr_t>(e)));
}

void end(ImDrawList* dl)
{
    if (!g_device)
        return;
    dl->AddCallback(&cb_end, nullptr);
}

void set_uniforms(effect e, const void* data, std::size_t len)
{
    effect_slot& s = slot(e);
    const auto* p = static_cast<const std::uint8_t*>(data);
    s.uniforms.assign(p, p + len);
    if (s.state)
        SDL_SetGPURenderStateFragmentUniforms(s.state, 0, data, static_cast<Uint32>(len));
}

bool bind_textures(effect e, SDL_Texture* const* textures, int n, bool linear)
{
    effect_slot& s = slot(e);
    if (!g_device || !s.shader)
        return false;
    s.ok = false;
    if (n != def(e).extra_samplers)
    {
        SDL_Log("gpu_effects: %s: %d textures bound, shader takes %d", def(e).name, n,
                def(e).extra_samplers);
        return false;
    }
    if (s.sampler)
    {
        SDL_ReleaseGPUSampler(g_device, s.sampler);
        s.sampler = nullptr;
    }
    SDL_GPUSamplerCreateInfo sci{};
    sci.min_filter     = linear ? SDL_GPU_FILTER_LINEAR : SDL_GPU_FILTER_NEAREST;
    sci.mag_filter     = sci.min_filter;
    sci.mipmap_mode    = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    sci.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT; // the cylinder wraps in x
    sci.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sci.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    s.sampler = SDL_CreateGPUSampler(g_device, &sci);
    if (!s.sampler)
        return false;
    std::vector<SDL_GPUTextureSamplerBinding> b(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i)
    {
        SDL_PropertiesID props = SDL_GetTextureProperties(textures[i]);
        auto* gt = static_cast<SDL_GPUTexture*>(
            SDL_GetPointerProperty(props, SDL_PROP_TEXTURE_GPU_TEXTURE_POINTER, nullptr));
        if (!gt)
        {
            SDL_Log("gpu_effects: %s: texture %d has no GPU texture", def(e).name, i);
            return false;
        }
        b[static_cast<std::size_t>(i)].texture = gt;
        b[static_cast<std::size_t>(i)].sampler = s.sampler;
    }
    s.ok = make_state(e, b.data(), n);
    return s.ok;
}

SDL_Texture* table_texture(int slot_i, int w, int h, const std::uint32_t* px)
{
    if (!g_device || slot_i < 0 || slot_i >= static_cast<int>(g_tables.size()) || w <= 0 || h <= 0)
        return nullptr;
    table_slot& t = g_tables[static_cast<std::size_t>(slot_i)];
    const std::size_t n = static_cast<std::size_t>(w) * static_cast<std::size_t>(h);
    if (!t.tex || t.w != w || t.h != h)
    {
        if (t.tex)
            SDL_DestroyTexture(t.tex);
        t = table_slot{};
        t.tex = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, w, h);
        if (!t.tex)
        {
            SDL_Log("gpu_effects: table %d (%dx%d) failed: %s", slot_i, w, h, SDL_GetError());
            return nullptr;
        }
        SDL_SetTextureScaleMode(t.tex, SDL_SCALEMODE_NEAREST);
        SDL_SetTextureBlendMode(t.tex, SDL_BLENDMODE_BLEND);
        t.w = w;
        t.h = h;
    }
    if (t.last.size() != n || std::memcmp(t.last.data(), px, n * sizeof(std::uint32_t)) != 0)
    {
        SDL_UpdateTexture(t.tex, nullptr, px, w * static_cast<int>(sizeof(std::uint32_t)));
        t.last.assign(px, px + n);
    }
    return t.tex;
}

SDL_Texture* detail_texture(int scale)
{
    if (scale < 0 || scale > 2 || !g_device)
        return nullptr;
    detail_slot& d = g_detail[static_cast<std::size_t>(scale)];
    if (d.tex || d.tried)
        return d.tex;
    d.tried = true;
    // Scale 2 is the rolling ground (Ben, 2026-11-16): one height texture in every channel.
    static const char* const k_family_files[4]  = { "vegetation", "forest", "arid", "rock" };
    static const char* const k_rolling_files[4] = { "rolling", "rolling", "rolling", "rolling" };
    const char* const* const k_files = scale == 2 ? k_rolling_files : k_family_files;
    const char* const dir = scale == 1 ? "assets/detail/fine/" : "assets/detail/";
    int w = 0, h = 0;
    std::vector<std::uint8_t> packed;
    for (int ch = 0; ch < 4; ++ch)
    {
        const std::string path = std::string(dir) + k_files[ch] + ".png";
        std::ifstream f(path, std::ios::binary);
        if (!f)
        {
            SDL_Log("gpu_effects: no detail texture at %s - no close-zoom detail", path.c_str());
            return nullptr;
        }
        const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(f),
                                              std::istreambuf_iterator<char>()};
        int fw = 0, fh = 0;
        std::vector<std::uint8_t> rgba;
        std::string err;
        if (!ui::png_decode_rgba(bytes, fw, fh, rgba, err) || (ch > 0 && (fw != w || fh != h)))
        {
            SDL_Log("gpu_effects: %s not used: %s", path.c_str(),
                    err.empty() ? "its size differs from the others" : err.c_str());
            return nullptr;
        }
        if (ch == 0)
        {
            w = fw;
            h = fh;
            packed.assign(static_cast<std::size_t>(w) * h * 4u, 0u);
        }
        for (std::size_t i = 0; i < static_cast<std::size_t>(w) * h; ++i)
            packed[i * 4u + static_cast<std::size_t>(ch)] = rgba[i * 4u]; // grey: r = g = b
    }
    SDL_Texture* t = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, w, h);
    if (!t)
    {
        SDL_Log("gpu_effects: detail texture (%dx%d) failed: %s", w, h, SDL_GetError());
        return nullptr;
    }
    SDL_SetTextureScaleMode(t, SDL_SCALEMODE_LINEAR);
    SDL_SetTextureBlendMode(t, SDL_BLENDMODE_NONE);
    SDL_UpdateTexture(t, nullptr, packed.data(), w * 4);
    d.tex = t;
    return d.tex;
}

} // namespace gfx
