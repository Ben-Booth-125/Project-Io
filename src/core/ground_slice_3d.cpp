#include "ground_slice_3d.hpp"

#include <SDL3/SDL.h>
#include <imgui.h>

#include "ui/png_decode.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

// The DXIL headers DXC writes at build time (CMakeLists.txt § Fragment shaders, and its
// vertex-shader rule). Without them the slice reports that it cannot run.
#if __has_include("shaders/slice3d_vert.dxil.h") && __has_include("shaders/slice3d.dxil.h") \
    && __has_include("shaders/slice3d_shadow.dxil.h")
#define IO_HAVE_SLICE_DXIL 1
#include "shaders/slice3d_vert.dxil.h"
#include "shaders/slice3d.dxil.h"
#include "shaders/slice3d_shadow.dxil.h"
#endif

namespace gfx::slice {
namespace {

constexpr float k_s3 = 1.7320508f;
constexpr int   k_grid = 48;          // quads per chunk side
constexpr int   k_target_w = 1280;    // the rendered frame
constexpr int   k_target_h = 720;
constexpr int   k_shadow_n = 2048;    // the shadow map's side
// The bake's light (the ground shader's k_light), in world axes (x east, y up, z south):
// from the north-west, about 40 degrees up. The sun sliders turn and raise it from here.
constexpr float k_bake_light[3] = { -0.54167f, 0.64279f, -0.54167f };
// The ground shader's calibration (body_surface_canvas.cpp, BL-1658 (close-zoom detail
// layer)), so the slice's bumps are the fixed-angle relief's own.
constexpr float k_bump[4]       = { 0.04f, 0.08f, 0.03f, 0.10f }; // vegetation, forest, arid, rock
constexpr float k_mountain_mul  = 2.5f;
constexpr float k_detail_period = 4.0f;
constexpr float k_rolling_swing = 0.25f;
constexpr float k_rolling_period = 12.0f;

struct vertex
{
    float x, y, z;
    float u, v;
    float nx, ny, nz;
};

struct mesh_chunk
{
    SDL_Texture* tex = nullptr;
    Uint32 first_index = 0, n_index = 0;
    Sint32 vertex_offset = 0;
};

struct settings
{
    bool  render   = false;
    bool  canvas   = true;   // the 3D ground on the canvas (RENDERING.md § The 3D ground)
    float tilt_deg = 50.0f;  // from straight down, at the closest rung; the tilt follows zoom
    float yaw_deg  = 0.0f;   // 0: looking north from the south
    float dist     = 0.0f;   // 0: fit the region on the first build
    float fov_deg  = 35.0f;
    float height   = 4.0f;   // canonical units of rise per unit of tile height
    float bumps    = 2.0f;   // multiple of the ground shader's bump heights
    float rolling  = 1.0f;   // multiple of the rolling ground's swing
    float sun_deg  = 40.0f;  // the sun's height above the horizon (the bake's is 40)
    float sun_turn = 0.0f;   // its turn from the bake's north-west (degrees)
    float shadow   = 0.5f;   // how far a full shadow darkens (0 off)
};

struct state
{
    bool tried = false, ok = false;
    std::string why; // why it cannot run
    SDL_Renderer*  renderer = nullptr;
    SDL_GPUDevice* device   = nullptr;
    SDL_GPUGraphicsPipeline* pipeline = nullptr;
    SDL_GPUGraphicsPipeline* shadow_pipeline = nullptr;
    SDL_GPUSampler* sampler = nullptr;
    SDL_GPUSampler* shadow_sampler = nullptr;
    SDL_GPUSampler* far_sampler = nullptr; // wraps east-west: the far page is the whole body
    int target_w = 0, target_h = 0;
    bool rendered = false; // a frame was rendered this frame
    SDL_GPUTexture* shadow_map = nullptr;
    SDL_GPUTextureFormat shadow_fmt = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
    SDL_GPUTexture* colour  = nullptr;
    SDL_GPUTexture* depth   = nullptr;
    SDL_Texture*    shown   = nullptr; // `colour`, wrapped for ImGui
    SDL_GPUBuffer*  vbuf = nullptr;
    SDL_GPUBuffer*  ibuf = nullptr;
    Uint32 vbuf_bytes = 0, ibuf_bytes = 0;
    std::vector<mesh_chunk> chunks;
    std::uint64_t mesh_key = 0;
    mesh_chunk far_mesh; // the backdrop
    float centre_x = 0, centre_y = 0, centre_h = 0, extent = 1;
    std::size_t n_vertices = 0, n_triangles = 0;
    double build_ms = 0, submit_ms = 0;
    // The textures the heights read: the coarse detail (one channel per family) and the
    // rolling ground, CPU copies.
    int det_n = 0, roll_n = 0;
    std::vector<float> det[4];
    std::vector<float> roll;
    settings s;
};

state g;

// The sliders persist in the working directory (Ben, 2026-11-16: "decide on the default
// settings") - read at start, written on every change, one "name value" per line.
constexpr const char* k_settings_file = "ground3d_settings.txt";

struct named { const char* name; float settings::* f; };
constexpr named k_named[] = {
    { "tilt_deg", &settings::tilt_deg }, { "yaw_deg", &settings::yaw_deg },
    { "dist", &settings::dist },         { "fov_deg", &settings::fov_deg },
    { "height", &settings::height },     { "bumps", &settings::bumps },
    { "rolling", &settings::rolling },   { "sun_deg", &settings::sun_deg },
    { "sun_turn", &settings::sun_turn }, { "shadow", &settings::shadow },
};

void load_settings()
{
    std::ifstream f(k_settings_file);
    std::string name;
    float v = 0;
    while (f >> name >> v)
    {
        if (name == "canvas") { g.s.canvas = v != 0.0f; continue; }
        if (name == "render") { g.s.render = v != 0.0f; continue; }
        for (const named& n : k_named)
            if (name == n.name && std::isfinite(v))
                g.s.*(n.f) = v;
    }
}

void save_settings()
{
    std::ofstream f(k_settings_file, std::ios::trunc);
    f << "canvas " << (g.s.canvas ? 1 : 0) << std::endl << "render " << (g.s.render ? 1 : 0) << std::endl;
    for (const named& n : k_named)
        f << n.name << ' ' << g.s.*(n.f) << std::endl;
}

bool load_grey(const char* path, int& n, std::vector<float>& out)
{
    std::ifstream f(path, std::ios::binary);
    if (!f)
        return false;
    const std::vector<std::uint8_t> file{ std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>() };
    int w = 0, h = 0;
    std::vector<std::uint8_t> rgba;
    std::string err;
    if (!ui::png_decode_rgba(file, w, h, rgba, err) || w != h || w <= 0)
        return false;
    n = w;
    out.resize(static_cast<std::size_t>(w) * h);
    for (std::size_t i = 0; i < out.size(); ++i)
        out[i] = rgba[i * 4] / 255.0f;
    return true;
}

float sample_wrap(const std::vector<float>& t, int n, float u, float v) // bilinear, wrapped
{
    u = u - std::floor(u);
    v = v - std::floor(v);
    const float fx = u * n - 0.5f, fy = v * n - 0.5f;
    const int x0 = static_cast<int>(std::floor(fx)), y0 = static_cast<int>(std::floor(fy));
    const float ax = fx - x0, ay = fy - y0;
    auto at = [&](int x, int y) {
        x = ((x % n) + n) % n;
        y = ((y % n) + n) % n;
        return t[static_cast<std::size_t>(y) * n + x];
    };
    return (at(x0, y0) * (1 - ax) + at(x0 + 1, y0) * ax) * (1 - ay)
         + (at(x0, y0 + 1) * (1 - ax) + at(x0 + 1, y0 + 1) * ax) * ay;
}

void centre_of(int c, int r, float& x, float& y)
{
    x = k_s3 * (c + 0.5f * (r & 1));
    y = 1.5f * r;
}

void nearest_cell(float px, float py, int& bc, int& br)
{
    const int r0 = static_cast<int>(std::floor(py / 1.5f));
    float bd = 1e30f;
    bc = 0; br = r0;
    for (int k = 0; k < 2; ++k)
    {
        const int r = r0 + k;
        const int c = static_cast<int>(std::lround(px / k_s3 - 0.5f * (r & 1)));
        float cx, cy;
        centre_of(c, r, cx, cy);
        const float dd = (px - cx) * (px - cx) + (py - cy) * (py - cy);
        if (dd < bd) { bd = dd; bc = c; br = r; }
    }
}

std::uint32_t tile_at(const inputs& in, int c, int r)
{
    if (r < 0 || r >= in.gh || in.gw <= 0)
        return 0u;
    c = ((c % in.gw) + in.gw) % in.gw;
    return in.tiles[static_cast<std::size_t>(r) * in.gw + c];
}

float tile_height(std::uint32_t t) { return static_cast<float>(t >> 24) / 255.0f; }

// The height field at canonical (px, py): the plane through the three nearest tile
// centres, the rolling ground and the nearest tile's coarse bumps, at the sliders' scales.
float height_at(const inputs& in, float px, float py, bool bumps = true)
{
    int c0, r0;
    nearest_cell(px, py, c0, r0);
    float x0, y0;
    centre_of(c0, r0, x0, y0);
    const float vx = px - x0, vy = py - y0;
    const float sixth = 1.0471976f;
    const float ka = std::atan2(vy, vx) / sixth;
    const float k = std::round(ka);
    const float k2 = (ka - k) >= 0.0f ? k + 1.0f : k - 1.0f;
    int c1, r1, c2, r2;
    nearest_cell(x0 + k_s3 * std::cos(k * sixth), y0 + k_s3 * std::sin(k * sixth), c1, r1);
    nearest_cell(x0 + k_s3 * std::cos(k2 * sixth), y0 + k_s3 * std::sin(k2 * sixth), c2, r2);
    const std::uint32_t t0 = tile_at(in, c0, r0);
    const float hs = g.s.height;
    const float h0 = tile_height(t0) * hs, h1 = tile_height(tile_at(in, c1, r1)) * hs,
                h2 = tile_height(tile_at(in, c2, r2)) * hs;
    float x1, y1, x2, y2;
    centre_of(c1, r1, x1, y1);
    centre_of(c2, r2, x2, y2);
    const float e1x = x1 - x0, e1y = y1 - y0, e2x = x2 - x0, e2y = y2 - y0;
    const float det = e1x * e2y - e1y * e2x;
    float h = h0;
    if (std::fabs(det) > 1e-4f)
    {
        const float gx = ((h1 - h0) * e2y - (h2 - h0) * e1y) / det;
        const float gy = ((h2 - h0) * e1x - (h1 - h0) * e2x) / det;
        h += gx * vx + gy * vy;
    }
    if (g.roll_n > 0)
        h += (sample_wrap(g.roll, g.roll_n, px / k_rolling_period, py / k_rolling_period) - 0.5f)
           * k_rolling_swing * g.s.rolling;
    if (bumps && g.det_n > 0 && (t0 & 0xFFu) != 0u) // land only
    {
        const int fam = std::clamp(static_cast<int>((t0 >> 8) & 0xFFu), 0, 3);
        const bool mountain = ((t0 >> 16) & 0xFFu) != 0u;
        const float amp = k_bump[fam] * (mountain ? k_mountain_mul : 1.0f) * g.s.bumps;
        h += (sample_wrap(g.det[fam], g.det_n, px / k_detail_period, py / k_detail_period) - 0.5f) * amp;
    }
    return h;
}

bool fail(const char* what)
{
    g.why = std::string(what) + ": " + SDL_GetError();
    SDL_Log("ground_slice_3d: %s", g.why.c_str());
    return false;
}

// The colour and depth targets at w x h, remade when the size changes, the colour one
// wrapped as an SDL_Texture for ImGui.
bool ensure_target(int w, int h)
{
    w = std::clamp(w, 64, 3840);
    h = std::clamp(h, 64, 2160);
    if (g.colour && w == g.target_w && h == g.target_h)
        return true;
    if (g.shown)  { SDL_DestroyTexture(g.shown); g.shown = nullptr; }
    if (g.colour) { SDL_ReleaseGPUTexture(g.device, g.colour); g.colour = nullptr; }
    if (g.depth)  { SDL_ReleaseGPUTexture(g.device, g.depth); g.depth = nullptr; }
    g.target_w = g.target_h = 0;
    SDL_GPUTextureCreateInfo tci{};
    tci.type = SDL_GPU_TEXTURETYPE_2D;
    tci.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    tci.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    tci.width = static_cast<Uint32>(w);
    tci.height = static_cast<Uint32>(h);
    tci.layer_count_or_depth = 1;
    tci.num_levels = 1;
    g.colour = SDL_CreateGPUTexture(g.device, &tci);
    if (!g.colour)
        return fail("colour target");
    tci.format = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
    tci.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    g.depth = SDL_CreateGPUTexture(g.device, &tci);
    if (!g.depth)
        return fail("depth target");
    const SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_FORMAT_NUMBER, SDL_PIXELFORMAT_ABGR8888);
    SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_ACCESS_NUMBER, SDL_TEXTUREACCESS_STATIC);
    SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_WIDTH_NUMBER, w);
    SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_HEIGHT_NUMBER, h);
    SDL_SetPointerProperty(props, SDL_PROP_TEXTURE_CREATE_GPU_TEXTURE_POINTER, g.colour);
    g.shown = SDL_CreateTextureWithProperties(g.renderer, props);
    SDL_DestroyProperties(props);
    if (!g.shown)
        return fail("wrapping the frame for ImGui");
    g.target_w = w;
    g.target_h = h;
    return true;
}

bool init(SDL_Renderer* renderer)
{
    g.tried = true;
    g.renderer = renderer;
    g.device = renderer ? SDL_GetGPURendererDevice(renderer) : nullptr;
    if (!g.device)
    {
        g.why = "not the GPU renderer";
        return false;
    }
#if !defined(IO_HAVE_SLICE_DXIL)
    g.why = "built without the slice's shaders (no DXC)";
    return false;
#else
    if (!(SDL_GetGPUShaderFormats(g.device) & SDL_GPU_SHADERFORMAT_DXIL))
    {
        g.why = "the device takes no DXIL";
        return false;
    }
    SDL_GPUShaderCreateInfo vs{};
    vs.code = reinterpret_cast<const Uint8*>(g_slice3d_vert_dxil);
    vs.code_size = sizeof(g_slice3d_vert_dxil);
    vs.entrypoint = "main";
    vs.format = SDL_GPU_SHADERFORMAT_DXIL;
    vs.stage = SDL_GPU_SHADERSTAGE_VERTEX;
    vs.num_uniform_buffers = 1;
    SDL_GPUShader* vsh = SDL_CreateGPUShader(g.device, &vs);
    if (!vsh)
        return fail("vertex shader");
    SDL_GPUShaderCreateInfo fs{};
    fs.code = reinterpret_cast<const Uint8*>(g_slice3d_dxil);
    fs.code_size = sizeof(g_slice3d_dxil);
    fs.entrypoint = "main";
    fs.format = SDL_GPU_SHADERFORMAT_DXIL;
    fs.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
    fs.num_samplers = 2;
    fs.num_uniform_buffers = 1;
    SDL_GPUShader* fsh = SDL_CreateGPUShader(g.device, &fs);
    if (!fsh)
    {
        SDL_ReleaseGPUShader(g.device, vsh);
        return fail("fragment shader");
    }
    SDL_GPUShaderCreateInfo ss{};
    ss.code = reinterpret_cast<const Uint8*>(g_slice3d_shadow_dxil);
    ss.code_size = sizeof(g_slice3d_shadow_dxil);
    ss.entrypoint = "main";
    ss.format = SDL_GPU_SHADERFORMAT_DXIL;
    ss.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
    SDL_GPUShader* ssh = SDL_CreateGPUShader(g.device, &ss);
    if (!ssh)
    {
        SDL_ReleaseGPUShader(g.device, vsh);
        SDL_ReleaseGPUShader(g.device, fsh);
        return fail("shadow fragment shader");
    }
    // The shadow map: 32-bit float depth where the device can sample it, 16-bit otherwise.
    const SDL_GPUTextureUsageFlags su = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    g.shadow_fmt = SDL_GPUTextureSupportsFormat(g.device, SDL_GPU_TEXTUREFORMAT_D32_FLOAT, SDL_GPU_TEXTURETYPE_2D, su)
                 ? SDL_GPU_TEXTUREFORMAT_D32_FLOAT : SDL_GPU_TEXTUREFORMAT_D16_UNORM;

    SDL_GPUVertexBufferDescription vbd{};
    vbd.slot = 0;
    vbd.pitch = sizeof(vertex);
    vbd.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    SDL_GPUVertexAttribute attrs[3]{};
    attrs[0].location = 0; attrs[0].buffer_slot = 0; attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3; attrs[0].offset = offsetof(vertex, x);
    attrs[1].location = 1; attrs[1].buffer_slot = 0; attrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2; attrs[1].offset = offsetof(vertex, u);
    attrs[2].location = 2; attrs[2].buffer_slot = 0; attrs[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3; attrs[2].offset = offsetof(vertex, nx);
    SDL_GPUColorTargetDescription ctd{};
    ctd.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    SDL_GPUGraphicsPipelineCreateInfo pci{};
    pci.vertex_shader = vsh;
    pci.fragment_shader = fsh;
    pci.vertex_input_state.vertex_buffer_descriptions = &vbd;
    pci.vertex_input_state.num_vertex_buffers = 1;
    pci.vertex_input_state.vertex_attributes = attrs;
    pci.vertex_input_state.num_vertex_attributes = 3;
    pci.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pci.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    pci.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    pci.depth_stencil_state.enable_depth_test = true;
    pci.depth_stencil_state.enable_depth_write = true;
    pci.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
    pci.target_info.color_target_descriptions = &ctd;
    pci.target_info.num_color_targets = 1;
    pci.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
    pci.target_info.has_depth_stencil_target = true;
    g.pipeline = SDL_CreateGPUGraphicsPipeline(g.device, &pci);
    // The shadow pass: the same vertices from the sun, depth only.
    pci.fragment_shader = ssh;
    pci.target_info.color_target_descriptions = nullptr;
    pci.target_info.num_color_targets = 0;
    pci.target_info.depth_stencil_format = g.shadow_fmt;
    g.shadow_pipeline = SDL_CreateGPUGraphicsPipeline(g.device, &pci);
    SDL_ReleaseGPUShader(g.device, vsh);
    SDL_ReleaseGPUShader(g.device, fsh);
    SDL_ReleaseGPUShader(g.device, ssh);
    if (!g.pipeline)
        return fail("pipeline");
    if (!g.shadow_pipeline)
        return fail("shadow pipeline");

    SDL_GPUSamplerCreateInfo sci{};
    sci.min_filter = SDL_GPU_FILTER_LINEAR;
    sci.mag_filter = SDL_GPU_FILTER_LINEAR;
    sci.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
    sci.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sci.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sci.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    g.sampler = SDL_CreateGPUSampler(g.device, &sci);
    if (!g.sampler)
        return fail("sampler");
    sci.min_filter = SDL_GPU_FILTER_NEAREST;
    sci.mag_filter = SDL_GPU_FILTER_NEAREST;
    sci.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    g.shadow_sampler = SDL_CreateGPUSampler(g.device, &sci);
    if (!g.shadow_sampler)
        return fail("shadow sampler");
    sci.min_filter = SDL_GPU_FILTER_LINEAR;
    sci.mag_filter = SDL_GPU_FILTER_LINEAR;
    sci.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    g.far_sampler = SDL_CreateGPUSampler(g.device, &sci);
    if (!g.far_sampler)
        return fail("far sampler");

    SDL_GPUTextureCreateInfo tci{};
    tci.type = SDL_GPU_TEXTURETYPE_2D;
    tci.layer_count_or_depth = 1;
    tci.num_levels = 1;
    tci.format = g.shadow_fmt;
    tci.usage = su;
    tci.width = k_shadow_n;
    tci.height = k_shadow_n;
    g.shadow_map = SDL_CreateGPUTexture(g.device, &tci);
    if (!g.shadow_map)
        return fail("shadow map");


    static const char* const k_names[4] = { "vegetation", "forest", "arid", "rock" };
    for (int f = 0; f < 4; ++f)
    {
        const std::string path = std::string("assets/detail/") + k_names[f] + ".png";
        int n = 0;
        if (!load_grey(path.c_str(), n, g.det[f]) || (f > 0 && n != g.det_n))
        {
            g.det_n = 0;
            break;
        }
        g.det_n = n;
    }
    if (!load_grey("assets/detail/rolling.png", g.roll_n, g.roll))
        g.roll_n = 0;
    return true;
#endif
}

// A hash of what the mesh depends on: the chunks (texture and rectangle) and the sliders
// that move heights. The mesh is rebuilt only when it changes.
std::uint64_t mesh_key(const inputs& in)
{
    std::uint64_t h = 1469598103934665603ull;
    auto mix = [&](const void* p, std::size_t n) {
        const auto* b = static_cast<const std::uint8_t*>(p);
        for (std::size_t i = 0; i < n; ++i) { h ^= b[i]; h *= 1099511628211ull; }
    };
    for (int i = 0; i < in.n_chunks; ++i)
        mix(&in.chunks[i], sizeof(chunk));
    mix(&g.s.height, sizeof(float));
    mix(&g.s.bumps, sizeof(float));
    mix(&g.s.rolling, sizeof(float));
    mix(&in.gw, sizeof(int));
    mix(&in.far, sizeof(chunk));
    if (in.n_chunks == 0) // the backdrop alone follows the view, in steps
    {
        const float step = std::max(4.0f, in.view_h * 0.5f);
        const int qx = static_cast<int>(std::floor(in.view_cx / step)), qy = static_cast<int>(std::floor(in.view_cy / step));
        mix(&qx, sizeof qx);
        mix(&qy, sizeof qy);
        mix(&step, sizeof step);
    }
    return h;
}

bool upload(const std::vector<vertex>& verts, const std::vector<Uint32>& idx)
{
    const Uint32 vb = static_cast<Uint32>(verts.size() * sizeof(vertex));
    const Uint32 ib = static_cast<Uint32>(idx.size() * sizeof(Uint32));
    if (vb > g.vbuf_bytes)
    {
        if (g.vbuf) SDL_ReleaseGPUBuffer(g.device, g.vbuf);
        SDL_GPUBufferCreateInfo bci{};
        bci.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
        bci.size = vb;
        g.vbuf = SDL_CreateGPUBuffer(g.device, &bci);
        g.vbuf_bytes = g.vbuf ? vb : 0;
    }
    if (ib > g.ibuf_bytes)
    {
        if (g.ibuf) SDL_ReleaseGPUBuffer(g.device, g.ibuf);
        SDL_GPUBufferCreateInfo bci{};
        bci.usage = SDL_GPU_BUFFERUSAGE_INDEX;
        bci.size = ib;
        g.ibuf = SDL_CreateGPUBuffer(g.device, &bci);
        g.ibuf_bytes = g.ibuf ? ib : 0;
    }
    if (!g.vbuf || !g.ibuf)
        return fail("mesh buffers");
    SDL_GPUTransferBufferCreateInfo tbi{};
    tbi.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tbi.size = vb + ib;
    SDL_GPUTransferBuffer* tb = SDL_CreateGPUTransferBuffer(g.device, &tbi);
    if (!tb)
        return fail("transfer buffer");
    auto* dst = static_cast<std::uint8_t*>(SDL_MapGPUTransferBuffer(g.device, tb, false));
    if (!dst)
    {
        SDL_ReleaseGPUTransferBuffer(g.device, tb);
        return fail("mapping the transfer buffer");
    }
    std::memcpy(dst, verts.data(), vb);
    std::memcpy(dst + vb, idx.data(), ib);
    SDL_UnmapGPUTransferBuffer(g.device, tb);
    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(g.device);
    if (!cmd)
    {
        SDL_ReleaseGPUTransferBuffer(g.device, tb);
        return fail("command buffer");
    }
    SDL_GPUCopyPass* cp = SDL_BeginGPUCopyPass(cmd);
    SDL_GPUTransferBufferLocation src{ tb, 0 };
    SDL_GPUBufferRegion vreg{ g.vbuf, 0, vb };
    SDL_UploadToGPUBuffer(cp, &src, &vreg, false);
    src.offset = vb;
    SDL_GPUBufferRegion ireg{ g.ibuf, 0, ib };
    SDL_UploadToGPUBuffer(cp, &src, &ireg, false);
    SDL_EndGPUCopyPass(cp);
    SDL_SubmitGPUCommandBuffer(cmd);
    SDL_ReleaseGPUTransferBuffer(g.device, tb);
    return true;
}

void build_mesh(const inputs& in)
{
    const Uint64 t0 = SDL_GetPerformanceCounter();
    g.chunks.clear();
    float lo_x = 1e30f, lo_y = 1e30f, hi_x = -1e30f, hi_y = -1e30f;
    for (int i = 0; i < in.n_chunks; ++i)
    {
        lo_x = std::min(lo_x, in.chunks[i].x0); hi_x = std::max(hi_x, in.chunks[i].x1);
        lo_y = std::min(lo_y, in.chunks[i].y0); hi_y = std::max(hi_y, in.chunks[i].y1);
    }
    if (in.n_chunks > 0)
    {
        g.centre_x = 0.5f * (lo_x + hi_x);
        g.centre_y = 0.5f * (lo_y + hi_y);
        g.extent = std::max(hi_x - lo_x, hi_y - lo_y);
    }
    else // the far rungs load no chunks: the backdrop alone, around the view
    {
        g.centre_x = in.view_cx;
        g.centre_y = in.view_cy;
        g.extent = std::max(4.0f, in.view_h * 1.8f);
    }
    g.centre_h = height_at(in, g.centre_x, g.centre_y);
    std::vector<vertex> verts;
    std::vector<Uint32> idx;
    const int n = k_grid + 1;
    std::vector<float> hgt(static_cast<std::size_t>(n + 2) * (n + 2));
    for (int i = 0; i < in.n_chunks; ++i)
    {
        const chunk& c = in.chunks[i];
        if (!c.tex || c.x1 <= c.x0 || c.y1 <= c.y0)
            continue;
        SDL_PropertiesID tp = SDL_GetTextureProperties(c.tex);
        if (!SDL_GetPointerProperty(tp, SDL_PROP_TEXTURE_GPU_TEXTURE_POINTER, nullptr))
            continue;
        const float dx = (c.x1 - c.x0) / k_grid, dy = (c.y1 - c.y0) / k_grid;
        // Heights with a one-sample border, for the normals' central differences.
        for (int y = -1; y <= n; ++y)
            for (int x = -1; x <= n; ++x)
                hgt[static_cast<std::size_t>(y + 1) * (n + 2) + (x + 1)] =
                    height_at(in, c.x0 + x * dx, c.y0 + y * dy);
        auto H = [&](int x, int y) { return hgt[static_cast<std::size_t>(y + 1) * (n + 2) + (x + 1)]; };
        mesh_chunk mc;
        mc.tex = c.tex;
        mc.vertex_offset = static_cast<Sint32>(verts.size());
        mc.first_index = static_cast<Uint32>(idx.size());
        for (int y = 0; y < n; ++y)
            for (int x = 0; x < n; ++x)
            {
                vertex v{};
                v.x = c.x0 + x * dx - g.centre_x;
                v.z = c.y0 + y * dy - g.centre_y;
                v.y = H(x, y);
                v.u = static_cast<float>(x) / k_grid;
                v.v = static_cast<float>(y) / k_grid;
                // Normal of the height surface y = h(x, z): (-dh/dx, 1, -dh/dz).
                const float gx = (H(x + 1, y) - H(x - 1, y)) / (2.0f * dx);
                const float gz = (H(x, y + 1) - H(x, y - 1)) / (2.0f * dy);
                const float len = std::sqrt(gx * gx + 1.0f + gz * gz);
                v.nx = -gx / len; v.ny = 1.0f / len; v.nz = -gz / len;
                verts.push_back(v);
            }
        for (int y = 0; y < k_grid; ++y)
            for (int x = 0; x < k_grid; ++x)
            {
                const Uint32 a = static_cast<Uint32>(y * n + x), b = a + 1, cc = a + n, d = cc + 1;
                idx.insert(idx.end(), { a, cc, b, b, cc, d });
            }
        mc.n_index = static_cast<Uint32>(idx.size()) - mc.first_index;
        g.chunks.push_back(mc);
    }
    // The backdrop: the far page as a coarse mesh around the region, a little below the
    // chunks so they win the depth test where both lie.
    g.far_mesh = mesh_chunk{};
    if (in.far.tex && in.far.x1 > in.far.x0 && in.far.y1 > in.far.y0)
    {
        constexpr int k_far = 160;
        const float half = std::max(3.0f * g.extent, 40.0f);
        const float fx0 = g.centre_x - half, fy0 = std::max(in.far.y0, g.centre_y - half);
        const float fy1 = std::min(in.far.y1, g.centre_y + half);
        const float dx = 2.0f * half / k_far, dy = (fy1 - fy0) / k_far;
        const int nf = k_far + 1;
        mesh_chunk& fm = g.far_mesh;
        fm.tex = in.far.tex;
        fm.vertex_offset = static_cast<Sint32>(verts.size());
        fm.first_index = static_cast<Uint32>(idx.size());
        std::vector<float> fh(static_cast<std::size_t>(nf + 2) * (nf + 2));
        for (int y = -1; y <= nf; ++y)
            for (int x = -1; x <= nf; ++x)
                fh[static_cast<std::size_t>(y + 1) * (nf + 2) + (x + 1)] = height_at(in, fx0 + x * dx, fy0 + y * dy, false);
        auto F = [&](int x, int y) { return fh[static_cast<std::size_t>(y + 1) * (nf + 2) + (x + 1)]; };
        for (int y = 0; y < nf; ++y)
            for (int x = 0; x < nf; ++x)
            {
                vertex v{};
                const float cx = fx0 + x * dx, cy = fy0 + y * dy;
                v.x = cx - g.centre_x;
                v.z = cy - g.centre_y;
                v.y = F(x, y) - 0.15f; // below the chunks, without their bumps
                v.u = (cx - in.far.x0) / (in.far.x1 - in.far.x0);
                v.v = (cy - in.far.y0) / (in.far.y1 - in.far.y0);
                const float gx = (F(x + 1, y) - F(x - 1, y)) / (2.0f * dx);
                const float gz = (F(x, y + 1) - F(x, y - 1)) / (2.0f * dy);
                const float len = std::sqrt(gx * gx + 1.0f + gz * gz);
                v.nx = -gx / len; v.ny = 1.0f / len; v.nz = -gz / len;
                verts.push_back(v);
            }
        // No backdrop under a loaded chunk: where both lay, the coarse backdrop rose through
        // the detailed ground in places (Ben, 2026-11-16: the ground "peeks over the
        // fine-grained textures"). A quad is dropped when its four corners lie in one chunk.
        auto in_chunk = [&](float x0q, float y0q, float x1q, float y1q) {
            for (int i = 0; i < in.n_chunks; ++i)
            {
                const chunk& c = in.chunks[i];
                if (c.tex && x0q >= c.x0 && x1q <= c.x1 && y0q >= c.y0 && y1q <= c.y1)
                    return true;
            }
            return false;
        };
        for (int y = 0; y < k_far; ++y)
            for (int x = 0; x < k_far; ++x)
            {
                if (in_chunk(fx0 + x * dx, fy0 + y * dy, fx0 + (x + 1) * dx, fy0 + (y + 1) * dy))
                    continue;
                const Uint32 a = static_cast<Uint32>(y * nf + x), b = a + 1, cc = a + nf, d = cc + 1;
                idx.insert(idx.end(), { a, cc, b, b, cc, d });
            }
        fm.n_index = static_cast<Uint32>(idx.size()) - fm.first_index;
    }
    g.n_vertices = verts.size();
    g.n_triangles = idx.size() / 3;
    if (!verts.empty() && !upload(verts, idx))
        g.chunks.clear();
    g.build_ms = static_cast<double>(SDL_GetPerformanceCounter() - t0) * 1000.0
               / static_cast<double>(SDL_GetPerformanceFrequency());
    if (g.s.dist <= 0.0f)
        g.s.dist = std::max(4.0f, g.extent * 0.9f);
}

// Column-major 4x4: m[c * 4 + r].
struct mat4 { float m[16]; };

mat4 mul(const mat4& a, const mat4& b)
{
    mat4 o{};
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
        {
            float s = 0;
            for (int k = 0; k < 4; ++k)
                s += a.m[k * 4 + r] * b.m[c * 4 + k];
            o.m[c * 4 + r] = s;
        }
    return o;
}

void render(const inputs& in, float lift_gamma)
{
    const Uint64 t0 = SDL_GetPerformanceCounter();
    const float d2r = 3.14159265f / 180.0f;
    // The tilt follows zoom (RENDERING.md § The camera): straight down at the 12 px rung and
    // wider, rising on a log scale to the setting at the 96 px rung. No turn on the canvas.
    float tilt_deg = g.s.tilt_deg, yaw_deg = g.s.yaw_deg;
    if (g.s.canvas)
    {
        const float k = std::clamp(std::log2(std::max(in.draw_r, 1.0f) / 12.0f) / 3.0f, 0.0f, 1.0f);
        tilt_deg = g.s.tilt_deg * k * k * (3.0f - 2.0f * k);
        yaw_deg = 0.0f;
    }
    const float tilt = tilt_deg * d2r, yaw = yaw_deg * d2r;
    // On the canvas the camera looks at the canvas view's centre from the distance at which
    // the view's height fills the frame when looking straight down, so zoom is the canvas's.
    float dist = g.s.dist;
    float tx = 0.0f, ty = g.centre_h, tz = 0.0f;
    if (g.s.canvas)
    {
        tx = in.view_cx - g.centre_x;
        tz = in.view_cy - g.centre_y;
        ty = height_at(in, in.view_cx, in.view_cy);
        dist = in.view_h / (2.0f * std::tan(0.5f * g.s.fov_deg * d2r));
    }
    const float ex = tx + dist * std::sin(tilt) * std::sin(yaw);
    const float ey = ty + dist * std::cos(tilt);
    const float ez = tz + dist * std::sin(tilt) * std::cos(yaw);
    // View (right-handed look-at).
    float fx = tx - ex, fy = ty - ey, fz = tz - ez;
    float fl = std::sqrt(fx * fx + fy * fy + fz * fz);
    fx /= fl; fy /= fl; fz /= fl;
    // Up: world up, or north when looking straight down.
    float ux = 0, uy = 1, uz = 0;
    if (std::fabs(fy) > 0.999f) { ux = 0; uy = 0; uz = -1; }
    float sx = fy * uz - fz * uy, sy = fz * ux - fx * uz, sz = fx * uy - fy * ux;
    const float sl = std::sqrt(sx * sx + sy * sy + sz * sz);
    sx /= sl; sy /= sl; sz /= sl;
    const float vx = sy * fz - sz * fy, vy = sz * fx - sx * fz, vz = sx * fy - sy * fx;
    mat4 view{};
    view.m[0] = sx;  view.m[4] = sy;  view.m[8]  = sz;  view.m[12] = -(sx * ex + sy * ey + sz * ez);
    view.m[1] = vx;  view.m[5] = vy;  view.m[9]  = vz;  view.m[13] = -(vx * ex + vy * ey + vz * ez);
    view.m[2] = -fx; view.m[6] = -fy; view.m[10] = -fz; view.m[14] = (fx * ex + fy * ey + fz * ez);
    view.m[15] = 1.0f;
    // Projection: right-handed, depth 0..1.
    const float nearz = std::max(0.05f, dist * 0.02f), farz = dist * 6.0f + g.extent * 8.0f;
    const float f = 1.0f / std::tan(0.5f * g.s.fov_deg * d2r);
    const float aspect = static_cast<float>(g.target_w) / static_cast<float>(g.target_h);
    mat4 proj{};
    proj.m[0] = f / aspect;
    proj.m[5] = f;
    proj.m[10] = farz / (nearz - farz);
    proj.m[11] = -1.0f;
    proj.m[14] = nearz * farz / (nearz - farz);
    const mat4 mvp = mul(proj, view);

    // The sun: the bake's light turned and raised by the sliders.
    const float az0 = std::atan2(k_bake_light[2], k_bake_light[0]);
    const float az = az0 + g.s.sun_turn * d2r, el = g.s.sun_deg * d2r;
    const float lx = std::cos(el) * std::cos(az), ly = std::sin(el), lz = std::cos(el) * std::sin(az);
    // An orthographic box from the sun over the whole region.
    const float half = g.extent * 0.75f + 1.0f + (g.s.canvas ? std::max(std::fabs(tx), std::fabs(tz)) : 0.0f);
    const float reach = g.extent + g.s.height * 2.0f + 10.0f;
    const float sx_ = tx + lx * reach, sy_ = ty + ly * reach, sz_ = tz + lz * reach;
    float bfx = -lx, bfy = -ly, bfz = -lz;
    float bux = 0, buy = 1, buz = 0;
    if (std::fabs(bfy) > 0.999f) { bux = 0; buy = 0; buz = -1; }
    float bsx = bfy * buz - bfz * buy, bsy = bfz * bux - bfx * buz, bsz = bfx * buy - bfy * bux;
    const float bsl = std::sqrt(bsx * bsx + bsy * bsy + bsz * bsz);
    bsx /= bsl; bsy /= bsl; bsz /= bsl;
    const float bvx = bsy * bfz - bsz * bfy, bvy = bsz * bfx - bsx * bfz, bvz = bsx * bfy - bsy * bfx;
    mat4 sview{};
    sview.m[0] = bsx;  sview.m[4] = bsy;  sview.m[8]  = bsz;  sview.m[12] = -(bsx * sx_ + bsy * sy_ + bsz * sz_);
    sview.m[1] = bvx;  sview.m[5] = bvy;  sview.m[9]  = bvz;  sview.m[13] = -(bvx * sx_ + bvy * sy_ + bvz * sz_);
    sview.m[2] = -bfx; sview.m[6] = -bfy; sview.m[10] = -bfz; sview.m[14] = (bfx * sx_ + bfy * sy_ + bfz * sz_);
    sview.m[15] = 1.0f;
    const float snear = 0.0f, sfar = reach * 2.0f;
    mat4 sproj{};
    sproj.m[0] = 1.0f / half;
    sproj.m[5] = 1.0f / half;
    sproj.m[10] = 1.0f / (snear - sfar);
    sproj.m[14] = snear / (snear - sfar);
    sproj.m[15] = 1.0f;
    const mat4 sun = mul(sproj, sview);

    // The renderer's pending work (chunk uploads) goes first, so the mesh samples it.
    SDL_FlushRenderer(g.renderer);
    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(g.device);
    if (!cmd)
    {
        fail("command buffer");
        return;
    }
    const float bl = std::sqrt(k_bake_light[0] * k_bake_light[0] + k_bake_light[1] * k_bake_light[1]
                               + k_bake_light[2] * k_bake_light[2]);
    struct { float light[4]; float sky[4]; float eye[4]; float shade[4]; } fsu = {
        { lx, ly, lz, lift_gamma },
        { 0.62f, 0.70f, 0.80f, std::max(4.0f, dist) },
        { ex, ey, ez, k_bake_light[1] / bl },
        { g.s.shadow, 0.0015f, 1.0f / k_shadow_n, 0.0f } };

    // The shadow pass: depth from the sun.
    if (g.s.shadow > 0.0f && !g.chunks.empty())
    {
        float vsu[32];
        std::memcpy(vsu, sun.m, sizeof sun.m);
        std::memcpy(vsu + 16, sun.m, sizeof sun.m);
        SDL_PushGPUVertexUniformData(cmd, 0, vsu, sizeof vsu);
        SDL_GPUDepthStencilTargetInfo sdt{};
        sdt.texture = g.shadow_map;
        sdt.clear_depth = 1.0f;
        sdt.load_op = SDL_GPU_LOADOP_CLEAR;
        sdt.store_op = SDL_GPU_STOREOP_STORE;
        sdt.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
        sdt.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
        SDL_GPURenderPass* sp = SDL_BeginGPURenderPass(cmd, nullptr, 0, &sdt);
        if (sp)
        {
            SDL_BindGPUGraphicsPipeline(sp, g.shadow_pipeline);
            const SDL_GPUBufferBinding svb{ g.vbuf, 0 };
            SDL_BindGPUVertexBuffers(sp, 0, &svb, 1);
            const SDL_GPUBufferBinding sib{ g.ibuf, 0 };
            SDL_BindGPUIndexBuffer(sp, &sib, SDL_GPU_INDEXELEMENTSIZE_32BIT);
            for (const mesh_chunk& mc : g.chunks)
                SDL_DrawGPUIndexedPrimitives(sp, mc.n_index, 1, mc.first_index, mc.vertex_offset, 0);
            if (g.far_mesh.n_index)
                SDL_DrawGPUIndexedPrimitives(sp, g.far_mesh.n_index, 1, g.far_mesh.first_index,
                                             g.far_mesh.vertex_offset, 0);
            SDL_EndGPURenderPass(sp);
        }
    }

    float vmu[32];
    std::memcpy(vmu, mvp.m, sizeof mvp.m);
    std::memcpy(vmu + 16, sun.m, sizeof sun.m);
    SDL_PushGPUVertexUniformData(cmd, 0, vmu, sizeof vmu);
    SDL_PushGPUFragmentUniformData(cmd, 0, &fsu, sizeof fsu);
    SDL_GPUColorTargetInfo ct{};
    ct.texture = g.colour;
    ct.clear_color = SDL_FColor{ 0.62f, 0.70f, 0.80f, 1.0f };
    ct.load_op = SDL_GPU_LOADOP_CLEAR;
    ct.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPUDepthStencilTargetInfo dt{};
    dt.texture = g.depth;
    dt.clear_depth = 1.0f;
    dt.load_op = SDL_GPU_LOADOP_CLEAR;
    dt.store_op = SDL_GPU_STOREOP_DONT_CARE;
    dt.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
    dt.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cmd, &ct, 1, &dt);
    if (pass && !g.chunks.empty())
    {
        SDL_BindGPUGraphicsPipeline(pass, g.pipeline);
        const SDL_GPUBufferBinding vbb{ g.vbuf, 0 };
        SDL_BindGPUVertexBuffers(pass, 0, &vbb, 1);
        const SDL_GPUBufferBinding ibb{ g.ibuf, 0 };
        SDL_BindGPUIndexBuffer(pass, &ibb, SDL_GPU_INDEXELEMENTSIZE_32BIT);
        for (const mesh_chunk& mc : g.chunks)
        {
            auto* gt = static_cast<SDL_GPUTexture*>(SDL_GetPointerProperty(
                SDL_GetTextureProperties(mc.tex), SDL_PROP_TEXTURE_GPU_TEXTURE_POINTER, nullptr));
            if (!gt)
                continue;
            const SDL_GPUTextureSamplerBinding sb[2] = { { gt, g.sampler }, { g.shadow_map, g.shadow_sampler } };
            SDL_BindGPUFragmentSamplers(pass, 0, sb, 2);
            SDL_DrawGPUIndexedPrimitives(pass, mc.n_index, 1, mc.first_index, mc.vertex_offset, 0);
        }
        if (g.far_mesh.n_index)
            if (auto* ft = static_cast<SDL_GPUTexture*>(SDL_GetPointerProperty(
                    SDL_GetTextureProperties(g.far_mesh.tex), SDL_PROP_TEXTURE_GPU_TEXTURE_POINTER, nullptr)))
            {
                const SDL_GPUTextureSamplerBinding sb[2] = { { ft, g.far_sampler }, { g.shadow_map, g.shadow_sampler } };
                SDL_BindGPUFragmentSamplers(pass, 0, sb, 2);
                SDL_DrawGPUIndexedPrimitives(pass, g.far_mesh.n_index, 1, g.far_mesh.first_index,
                                             g.far_mesh.vertex_offset, 0);
            }
    }
    if (pass)
        SDL_EndGPURenderPass(pass);
    SDL_SubmitGPUCommandBuffer(cmd);
    g.submit_ms = static_cast<double>(SDL_GetPerformanceCounter() - t0) * 1000.0
                / static_cast<double>(SDL_GetPerformanceFrequency());
    g.rendered = true;
}

} // namespace

void draw_window(const inputs& in)
{
    if (!in.renderer)
        return;
    if (!g.tried)
    {
        load_settings();
        g.ok = init(in.renderer);
    }
    g.rendered = false;
    // On the canvas, the frame renders whether or not the window is open.
    const bool want = g.ok && (g.s.render || g.s.canvas);
    if (want)
    {
        const int w = g.s.canvas && in.view_px_w > 0 ? in.view_px_w : k_target_w;
        const int h = g.s.canvas && in.view_px_h > 0 ? in.view_px_h : k_target_h;
        if (ensure_target(w, h))
        {
            const std::uint64_t key = mesh_key(in);
            if (key != g.mesh_key)
            {
                g.mesh_key = key;
                build_mesh(in);
            }
            render(in, in.lift_gamma);
        }
    }
    if (!in.window)
        return;
    ImGui::SetNextWindowSize({ 700.0f, 560.0f }, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("3D slice (BL-1665)"))
    {
        ImGui::End();
        return;
    }
    if (!g.ok)
    {
        ImGui::TextWrapped("The 3D slice cannot run: %s.", g.why.c_str());
        ImGui::End();
        return;
    }
    ImGui::Checkbox("3D ground on the canvas", &g.s.canvas);
    ImGui::SameLine();
    ImGui::Checkbox("Render here", &g.s.render);
    ImGui::SameLine();
    if (ImGui::Button("Fit"))
        g.s.dist = std::max(4.0f, g.extent * 0.9f);
    ImGui::SliderFloat(g.s.canvas ? "Tilt at the closest zoom" : "Tilt (deg from down)", &g.s.tilt_deg, 0.0f, 85.0f, "%.0f");
    if (!g.s.canvas)
        ImGui::SliderFloat("Turn (deg)", &g.s.yaw_deg, -180.0f, 180.0f, "%.0f");
    ImGui::SliderFloat("Distance", &g.s.dist, 1.0f, std::max(20.0f, g.extent * 3.0f), "%.1f");
    ImGui::SliderFloat("Field of view", &g.s.fov_deg, 15.0f, 80.0f, "%.0f");
    ImGui::SliderFloat("Height", &g.s.height, 0.0f, 16.0f, "%.1f");
    ImGui::SliderFloat("Bumps", &g.s.bumps, 0.0f, 8.0f, "%.1f");
    ImGui::SliderFloat("Rolling", &g.s.rolling, 0.0f, 4.0f, "%.1f");
    ImGui::SliderFloat("Sun height (deg)", &g.s.sun_deg, 3.0f, 85.0f, "%.0f");
    ImGui::SliderFloat("Sun turn (deg)", &g.s.sun_turn, -180.0f, 180.0f, "%.0f");
    ImGui::SliderFloat("Shadow", &g.s.shadow, 0.0f, 0.9f, "%.2f");
    ImGui::TextDisabled("The bake's own shading keeps its light at 40 deg from the north-west.");
    if (g.s.canvas)
        ImGui::TextDisabled("On the canvas: distance follows the zoom; the map's marks and clicks stay flat.");
    if (g.rendered)
    {
        ImGui::Text("%d chunks  ·  %zu vertices  ·  %zu triangles", static_cast<int>(g.chunks.size()),
                    g.n_vertices, g.n_triangles);
        ImGui::Text("mesh build %.1f ms (on change)  ·  record + submit %.2f ms (CPU, every frame)",
                    g.build_ms, g.submit_ms);
        ImGui::TextDisabled("GPU cost: compare the F11 frame time with Render on and off.");
        if (!g.s.canvas)
        {
            const float w = ImGui::GetContentRegionAvail().x;
            ImGui::Image((ImTextureID)(intptr_t)g.shown, { w, w * static_cast<float>(g.target_h) / g.target_w });
        }
    }
    static settings saved_s = g.s;
    if (std::memcmp(&saved_s, &g.s, sizeof(settings)) != 0)
    {
        saved_s = g.s;
        save_settings();
    }
    ImGui::End();
}

bool on_canvas()
{
    if (!g.tried) // the saved "Show on the canvas" holds from launch, before the window opens
    {
        static bool loaded = false;
        if (!loaded) { loaded = true; load_settings(); }
        return g.s.canvas;
    }
    return g.ok && g.s.canvas;
}

SDL_Texture* canvas_frame() { return g.s.canvas && g.rendered ? g.shown : nullptr; }

void shutdown()
{
    if (g.device)
    {
        if (g.shown)    SDL_DestroyTexture(g.shown);
        if (g.pipeline) SDL_ReleaseGPUGraphicsPipeline(g.device, g.pipeline);
        if (g.shadow_pipeline) SDL_ReleaseGPUGraphicsPipeline(g.device, g.shadow_pipeline);
        if (g.sampler)  SDL_ReleaseGPUSampler(g.device, g.sampler);
        if (g.shadow_sampler) SDL_ReleaseGPUSampler(g.device, g.shadow_sampler);
        if (g.far_sampler) SDL_ReleaseGPUSampler(g.device, g.far_sampler);
        if (g.shadow_map) SDL_ReleaseGPUTexture(g.device, g.shadow_map);
        if (g.colour)   SDL_ReleaseGPUTexture(g.device, g.colour);
        if (g.depth)    SDL_ReleaseGPUTexture(g.device, g.depth);
        if (g.vbuf)     SDL_ReleaseGPUBuffer(g.device, g.vbuf);
        if (g.ibuf)     SDL_ReleaseGPUBuffer(g.device, g.ibuf);
    }
    g = state{};
}

} // namespace gfx::slice
