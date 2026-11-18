#include "ground_slice_3d.hpp"

#include <SDL3/SDL.h>
#include <imgui.h>

#include "ui/ground_bake.hpp"
#include "ui/png_decode.hpp"
#include "ui/structure_stamps.hpp"

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
// vertex-shader rule). Without them the 3D ground reports that it cannot run.
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
constexpr float k_d2r = 3.14159265f / 180.0f;
constexpr int   k_grid = 48;          // quads per chunk side
constexpr int   k_far = 96;           // the backdrop's quads per side
constexpr int   k_shadow_n = 2048;    // the shadow map's side
constexpr int   k_mesh_budget_us = 4000; // chunk meshes built per frame, by time
constexpr int   k_mesh_keep_frames = 240; // a mesh unused this long is released
constexpr float k_building_max_chunk = 24.0f; // canonical width: coarser chunks stand nothing
// The bake's light (the ground shader's k_light), in world axes (x east, y up, z south):
// from the north-west, about 40 degrees up. The sun sliders turn and raise it from here.
constexpr float k_bake_light[3] = { -0.54167f, 0.64279f, -0.54167f };
// The ground shader's calibration (body_surface_canvas.cpp, BL-1658 (close-zoom detail
// layer)), so the mesh's bumps are the detail layer's own.
constexpr float k_bump[4]       = { 0.04f, 0.08f, 0.03f, 0.10f }; // vegetation, forest, arid, rock
constexpr float k_mountain_mul  = 2.5f;
constexpr float k_detail_period = 4.0f;
constexpr float k_rolling_swing = 0.25f;
constexpr float k_rolling_period = 12.0f;
constexpr float k_sky[3] = { 0.62f, 0.70f, 0.80f };

struct vertex
{
    float x, y, z;
    float u, v;
    float nx, ny, nz;
    std::uint8_t r, g, b, a; // a = 255 on a building (its own colour), 0 on the ground (the bake)
};

struct mat4 { float m[16]; }; // column-major: m[c * 4 + r]

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

struct settings
{
    float tilt_deg = 50.0f;  // from straight down, at the closest rung; the tilt follows zoom
    float fov_deg  = 35.0f;
    float height   = 4.0f;   // canonical units of rise per unit of tile height
    float bumps    = 2.0f;   // multiple of the detail layer's bump heights
    float rolling  = 1.0f;   // multiple of the rolling ground's swing
    float sun_deg  = 40.0f;  // the sun's height above the horizon (the bake's is 40)
    float sun_turn = 0.0f;   // its turn from the bake's north-west (degrees)
    float shadow   = 0.5f;   // how far a full shadow darkens (0 off)
    float buildings = 1.0f;  // multiple of the structures' authored heights
    float bump_light = 1.0f; // the detail's per-pixel relief on the mesh's light
    float trees = 1.0f;      // multiple of the trees a tile's cover density stands (0 none)
};

/// One chunk's mesh: its ground grid (drawn through the shared index buffer) and its
/// buildings (a plain triangle list), absolute canonical coordinates.
struct chunk_mesh
{
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    SDL_GPUBuffer* vbuf = nullptr;
    SDL_GPUBuffer* bbuf = nullptr;
    Uint32 n_building_vertices = 0;
    bool built = false;           // its buffers hold a mesh (perhaps a stale one)
    std::uint64_t built_key = 0;  // the heights it was built at: tiles and sliders
    std::uint64_t built_inst = 0; // the installations it stood
    std::uint64_t want_inst = 0;  // the installations the current source holds
    const void* src_seen = nullptr;
    int last_used = 0;
    SDL_Texture* tex = nullptr;   // this frame's bake
    float dx = 0;                 // this frame's wrap shift (canonical x)
};

struct camera
{
    bool ok = false;
    float ox = 0, oy = 0;      // canonical origin of the world frame
    float ex = 0, ey = 0, ez = 0; // eye
    float tx = 0, ty = 0, tz = 0; // target
    float fx = 0, fy = 0, fz = 0; // forward
    float sx = 0, sy = 0, sz = 0; // right
    float ux = 0, uy = 0, uz = 0; // up
    float tan_half = 0.3f, aspect = 1.0f, dist = 10.0f;
    float near_z = 0.1f, far_z = 100.0f;
    mat4 mvp{};
    view v;
};

struct state
{
    bool tried = false, ok = false;
    std::string why; // why it cannot run
    SDL_Renderer*  renderer = nullptr;
    SDL_GPUDevice* device   = nullptr;
    SDL_GPUGraphicsPipeline* pipe_chunk = nullptr; // writes the stencil
    SDL_GPUGraphicsPipeline* pipe_back  = nullptr; // draws only where no chunk drew
    SDL_GPUGraphicsPipeline* pipe_shadow = nullptr;
    SDL_GPUSampler* s_clamp = nullptr;
    SDL_GPUSampler* s_near  = nullptr;
    SDL_GPUSampler* s_wrap  = nullptr; // wraps both ways: the detail textures
    SDL_GPUSampler* s_far   = nullptr; // wraps east-west: the far page is the whole body
    SDL_GPUTextureFormat depth_fmt = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
    bool stencil = false;
    int target_w = 0, target_h = 0;
    bool rendered = false;
    bool active = false;
    int frame_no = 0;
    SDL_GPUTexture* shadow_map = nullptr;
    SDL_GPUTextureFormat shadow_fmt = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
    SDL_GPUTexture* colour  = nullptr;
    SDL_GPUTexture* depth   = nullptr;
    SDL_Texture*    shown   = nullptr; // `colour`, wrapped for ImGui
    SDL_GPUTexture* coord   = nullptr; // each pixel's place in the drape (drape.frag.hlsl)
    SDL_Texture*    coord_shown = nullptr; // `coord`, wrapped for the renderer
    SDL_Texture*    drape   = nullptr;
    int drape_w = 0, drape_h = 0;
    int drape_gen = 0; // bumped on every new drape texture: a binding holds the old one
    SDL_GPUBuffer*  grid_ibuf = nullptr; // the k_grid x k_grid index list every chunk shares
    std::vector<chunk_mesh> meshes;
    // The backdrop: its own vertices and indices, around the view.
    SDL_GPUBuffer* far_vbuf = nullptr;
    SDL_GPUBuffer* far_ibuf = nullptr;
    Uint32 far_n_index = 0;
    std::uint64_t far_key = 0;
    SDL_Texture* far_tex = nullptr;
    std::size_t n_vertices = 0, n_triangles = 0, n_building_tris = 0;
    double build_ms = 0, submit_ms = 0;
    int built_this_frame = 0, pending = 0;
    bool unbudgeted = false; // set_unbudgeted: no mesh time budget (--verify)
    // The textures the heights read: the coarse detail (one channel per family) and the
    // rolling ground, CPU copies.
    int det_n = 0, roll_n = 0;
    std::vector<float> det[4];
    std::vector<float> roll;
    // The tile table, for heights.
    std::vector<std::uint32_t> tiles;
    int gw = 0, gh = 0;
    std::uint64_t tiles_hash = 0;
    camera cam;
    settings s;
};

state g;

// The sliders persist in the working directory (Ben, 2026-11-16: "decide on the default
// settings") - read at start, written on every change, one "name value" per line.
constexpr const char* k_settings_file = "ground3d_settings.txt";

struct named { const char* name; float settings::* f; };
constexpr named k_named[] = {
    { "tilt_deg", &settings::tilt_deg }, { "fov_deg", &settings::fov_deg },
    { "height", &settings::height },     { "bumps", &settings::bumps },
    { "rolling", &settings::rolling },   { "sun_deg", &settings::sun_deg },
    { "sun_turn", &settings::sun_turn }, { "shadow", &settings::shadow },
    { "buildings", &settings::buildings }, { "bump_light", &settings::bump_light },
    { "trees", &settings::trees },
};

void load_settings()
{
    std::ifstream f(k_settings_file);
    std::string name;
    float v = 0;
    while (f >> name >> v)
        for (const named& n : k_named)
            if (name == n.name && std::isfinite(v))
                g.s.*(n.f) = v;
}

void save_settings()
{
    std::ofstream f(k_settings_file, std::ios::trunc);
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

std::uint32_t tile_at(int c, int r)
{
    if (r < 0 || r >= g.gh || g.gw <= 0)
        return 0u;
    c = ((c % g.gw) + g.gw) % g.gw;
    return g.tiles[static_cast<std::size_t>(r) * g.gw + c];
}

float tile_height(std::uint32_t t) { return static_cast<float>(t >> 24) / 255.0f; }

// The height field at canonical (px, py): the plane through the three nearest tile
// centres, the rolling ground and the nearest tile's coarse bumps, at the sliders' scales.
float height_at(float px, float py, bool bumps = true)
{
    if (g.gw <= 0)
        return 0.0f;
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
    const std::uint32_t t0 = tile_at(c0, r0);
    const float hs = g.s.height;
    const float h0 = tile_height(t0) * hs, h1 = tile_height(tile_at(c1, r1)) * hs,
                h2 = tile_height(tile_at(c2, r2)) * hs;
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

float max_height() { return g.s.height + 0.5f * k_rolling_swing * g.s.rolling + 0.5f * 0.25f * g.s.bumps + 0.05f; }
float min_height() { return -0.5f * k_rolling_swing * g.s.rolling - 0.5f * 0.25f * g.s.bumps - 0.05f; }

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
    w = std::clamp(w, 64, 7680);
    h = std::clamp(h, 64, 4320);
    if (g.colour && w == g.target_w && h == g.target_h)
        return true;
    if (g.shown)  { SDL_DestroyTexture(g.shown); g.shown = nullptr; }
    if (g.colour) { SDL_ReleaseGPUTexture(g.device, g.colour); g.colour = nullptr; }
    if (g.coord_shown) { SDL_DestroyTexture(g.coord_shown); g.coord_shown = nullptr; }
    if (g.coord)  { SDL_ReleaseGPUTexture(g.device, g.coord); g.coord = nullptr; }
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
    g.coord = SDL_CreateGPUTexture(g.device, &tci);
    if (!g.coord)
        return fail("drape coordinate target");
    tci.format = g.depth_fmt;
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
    SDL_SetPointerProperty(props, SDL_PROP_TEXTURE_CREATE_GPU_TEXTURE_POINTER, g.coord);
    g.coord_shown = SDL_CreateTextureWithProperties(g.renderer, props);
    SDL_DestroyProperties(props);
    if (!g.shown)
        return fail("wrapping the frame for ImGui");
    if (!g.coord_shown)
        return fail("wrapping the drape coordinates");
    // Read texel for texel, and the drape it fetches is premultiplied.
    SDL_SetTextureScaleMode(g.coord_shown, SDL_SCALEMODE_NEAREST);
    SDL_SetTextureBlendMode(g.coord_shown, SDL_BLENDMODE_BLEND_PREMULTIPLIED);
    g.target_w = w;
    g.target_h = h;
    return true;
}

SDL_GPUBuffer* make_buffer(SDL_GPUBufferUsageFlags usage, Uint32 bytes)
{
    SDL_GPUBufferCreateInfo bci{};
    bci.usage = usage;
    bci.size = std::max<Uint32>(bytes, 16);
    return SDL_CreateGPUBuffer(g.device, &bci);
}

// Uploads @p n blocks into their buffers in one copy pass.
struct upload_item { SDL_GPUBuffer* buf; const void* data; Uint32 bytes; };
bool upload(const upload_item* items, int n)
{
    Uint32 total = 0;
    for (int i = 0; i < n; ++i)
        total += items[i].bytes;
    if (total == 0)
        return true;
    SDL_GPUTransferBufferCreateInfo tbi{};
    tbi.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tbi.size = total;
    SDL_GPUTransferBuffer* tb = SDL_CreateGPUTransferBuffer(g.device, &tbi);
    if (!tb)
        return fail("transfer buffer");
    auto* dst = static_cast<std::uint8_t*>(SDL_MapGPUTransferBuffer(g.device, tb, false));
    if (!dst)
    {
        SDL_ReleaseGPUTransferBuffer(g.device, tb);
        return fail("mapping the transfer buffer");
    }
    Uint32 off = 0;
    for (int i = 0; i < n; ++i)
    {
        std::memcpy(dst + off, items[i].data, items[i].bytes);
        off += items[i].bytes;
    }
    SDL_UnmapGPUTransferBuffer(g.device, tb);
    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(g.device);
    if (!cmd)
    {
        SDL_ReleaseGPUTransferBuffer(g.device, tb);
        return fail("command buffer");
    }
    SDL_GPUCopyPass* cp = SDL_BeginGPUCopyPass(cmd);
    off = 0;
    for (int i = 0; i < n; ++i)
    {
        if (items[i].bytes == 0)
            continue;
        SDL_GPUTransferBufferLocation src{ tb, off };
        SDL_GPUBufferRegion reg{ items[i].buf, 0, items[i].bytes };
        SDL_UploadToGPUBuffer(cp, &src, &reg, false);
        off += items[i].bytes;
    }
    SDL_EndGPUCopyPass(cp);
    SDL_SubmitGPUCommandBuffer(cmd);
    SDL_ReleaseGPUTransferBuffer(g.device, tb);
    return true;
}

SDL_GPUGraphicsPipeline* make_pipeline(SDL_GPUShader* vsh, SDL_GPUShader* fsh, bool colour, int stencil_mode)
{
    SDL_GPUVertexBufferDescription vbd{};
    vbd.slot = 0;
    vbd.pitch = sizeof(vertex);
    vbd.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    SDL_GPUVertexAttribute attrs[4]{};
    attrs[0].location = 0; attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3; attrs[0].offset = offsetof(vertex, x);
    attrs[1].location = 1; attrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2; attrs[1].offset = offsetof(vertex, u);
    attrs[2].location = 2; attrs[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3; attrs[2].offset = offsetof(vertex, nx);
    attrs[3].location = 3; attrs[3].format = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM; attrs[3].offset = offsetof(vertex, r);
    SDL_GPUColorTargetDescription ctd[2]{};
    ctd[0].format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    ctd[1].format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM; // the drape coordinates
    SDL_GPUGraphicsPipelineCreateInfo pci{};
    pci.vertex_shader = vsh;
    pci.fragment_shader = fsh;
    pci.vertex_input_state.vertex_buffer_descriptions = &vbd;
    pci.vertex_input_state.num_vertex_buffers = 1;
    pci.vertex_input_state.vertex_attributes = attrs;
    pci.vertex_input_state.num_vertex_attributes = 4;
    pci.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pci.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    pci.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    pci.depth_stencil_state.enable_depth_test = true;
    pci.depth_stencil_state.enable_depth_write = true;
    pci.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
    if (colour)
    {
        pci.target_info.color_target_descriptions = ctd;
        pci.target_info.num_color_targets = 2;
        pci.target_info.depth_stencil_format = g.depth_fmt;
        if (g.stencil && stencil_mode != 0)
        {
            SDL_GPUStencilOpState so{};
            so.fail_op = SDL_GPU_STENCILOP_KEEP;
            so.depth_fail_op = SDL_GPU_STENCILOP_KEEP;
            if (stencil_mode == 1) // the chunks mark where they drew
            {
                so.pass_op = SDL_GPU_STENCILOP_REPLACE;
                so.compare_op = SDL_GPU_COMPAREOP_ALWAYS;
                so.depth_fail_op = SDL_GPU_STENCILOP_REPLACE;
            }
            else // the backdrop draws only where none did
            {
                so.pass_op = SDL_GPU_STENCILOP_KEEP;
                so.compare_op = SDL_GPU_COMPAREOP_NOT_EQUAL;
            }
            pci.depth_stencil_state.enable_stencil_test = true;
            pci.depth_stencil_state.front_stencil_state = so;
            pci.depth_stencil_state.back_stencil_state = so;
            pci.depth_stencil_state.compare_mask = 0xFF;
            pci.depth_stencil_state.write_mask = 0xFF;
        }
    }
    else
        pci.target_info.depth_stencil_format = g.shadow_fmt;
    pci.target_info.has_depth_stencil_target = true;
    return SDL_CreateGPUGraphicsPipeline(g.device, &pci);
}

bool init(SDL_Renderer* renderer)
{
    g.tried = true;
    load_settings();
    g.renderer = renderer;
    g.device = renderer ? SDL_GetGPURendererDevice(renderer) : nullptr;
    if (!g.device)
    {
        g.why = "not the GPU renderer";
        return false;
    }
#if !defined(IO_HAVE_SLICE_DXIL)
    g.why = "built without the 3D ground's shaders (no DXC)";
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
    fs.num_samplers = 8;
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
    // The frame's depth: with a stencil where the device has one, so the backdrop draws
    // only where no chunk did (it never rises through the loaded ground).
    const SDL_GPUTextureUsageFlags du = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    if (SDL_GPUTextureSupportsFormat(g.device, SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT, SDL_GPU_TEXTURETYPE_2D, du))
    { g.depth_fmt = SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT; g.stencil = true; }
    else if (SDL_GPUTextureSupportsFormat(g.device, SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT, SDL_GPU_TEXTURETYPE_2D, du))
    { g.depth_fmt = SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT; g.stencil = true; }
    else
    { g.depth_fmt = SDL_GPU_TEXTUREFORMAT_D16_UNORM; g.stencil = false; }

    g.pipe_chunk  = make_pipeline(vsh, fsh, true, 1);
    g.pipe_back   = make_pipeline(vsh, fsh, true, 2);
    g.pipe_shadow = make_pipeline(vsh, ssh, false, 0);
    SDL_ReleaseGPUShader(g.device, vsh);
    SDL_ReleaseGPUShader(g.device, fsh);
    SDL_ReleaseGPUShader(g.device, ssh);
    if (!g.pipe_chunk || !g.pipe_back)
        return fail("pipeline");
    if (!g.pipe_shadow)
        return fail("shadow pipeline");

    SDL_GPUSamplerCreateInfo sci{};
    sci.min_filter = SDL_GPU_FILTER_LINEAR;
    sci.mag_filter = SDL_GPU_FILTER_LINEAR;
    sci.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
    sci.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sci.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sci.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    g.s_clamp = SDL_CreateGPUSampler(g.device, &sci);
    sci.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    g.s_far = SDL_CreateGPUSampler(g.device, &sci);
    sci.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    g.s_wrap = SDL_CreateGPUSampler(g.device, &sci);
    sci.min_filter = SDL_GPU_FILTER_NEAREST;
    sci.mag_filter = SDL_GPU_FILTER_NEAREST;
    sci.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    sci.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sci.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    g.s_near = SDL_CreateGPUSampler(g.device, &sci);
    if (!g.s_clamp || !g.s_far || !g.s_wrap || !g.s_near)
        return fail("samplers");

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

    // The index list every chunk's grid shares.
    {
        const int n = k_grid + 1;
        std::vector<Uint32> idx;
        idx.reserve(static_cast<std::size_t>(k_grid) * k_grid * 6);
        for (int y = 0; y < k_grid; ++y)
            for (int x = 0; x < k_grid; ++x)
            {
                const Uint32 a = static_cast<Uint32>(y * n + x), b = a + 1, c = a + n, d = c + 1;
                idx.insert(idx.end(), { a, c, b, b, c, d });
            }
        g.grid_ibuf = make_buffer(SDL_GPU_BUFFERUSAGE_INDEX, static_cast<Uint32>(idx.size() * sizeof(Uint32)));
        const upload_item it{ g.grid_ibuf, idx.data(), static_cast<Uint32>(idx.size() * sizeof(Uint32)) };
        if (!g.grid_ibuf || !upload(&it, 1))
            return fail("grid indices");
    }

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

std::uint64_t fnv(std::uint64_t h, const void* p, std::size_t n)
{
    const auto* b = static_cast<const std::uint8_t*>(p);
    for (std::size_t i = 0; i < n; ++i) { h ^= b[i]; h *= 1099511628211ull; }
    return h;
}

// What a chunk's heights depend on, beyond its rectangle: the tiles and the sliders.
std::uint64_t height_key()
{
    std::uint64_t h = 1469598103934665603ull;
    h = fnv(h, &g.tiles_hash, sizeof g.tiles_hash);
    h = fnv(h, &g.s.height, sizeof(float));
    h = fnv(h, &g.s.bumps, sizeof(float));
    h = fnv(h, &g.s.rolling, sizeof(float));
    h = fnv(h, &g.s.buildings, sizeof(float));
    h = fnv(h, &g.s.trees, sizeof(float));
    return h;
}

// The tiles whose centres lie in [x0, x1) x [y0, y1): their raster cells, columns
// unwrapped as the rectangle has them.
template <class F> void for_tiles_in(float x0, float y0, float x1, float y1, F&& f)
{
    const int r_lo = std::max(0, static_cast<int>(std::ceil(y0 / 1.5f)));
    const int r_hi = std::min(g.gh - 1, static_cast<int>(std::ceil(y1 / 1.5f)) - 1);
    for (int r = r_lo; r <= r_hi; ++r)
    {
        const float odd = (r & 1) ? 0.5f : 0.0f;
        const int c_lo = static_cast<int>(std::ceil(x0 / k_s3 - odd));
        const int c_hi = static_cast<int>(std::ceil(x1 / k_s3 - odd)) - 1;
        for (int c = c_lo; c <= c_hi; ++c)
            f(c, r);
    }
}

std::uint64_t installations_hash(const ui::ground::bake_source* src, float x0, float y0, float x1, float y1)
{
    std::uint64_t h = 1469598103934665603ull;
    if (!src || src->gw <= 0)
        return h;
    for_tiles_in(x0, y0, x1, y1, [&](int c, int r) {
        const int cw = ((c % src->gw) + src->gw) % src->gw;
        const std::size_t i = static_cast<std::size_t>(r) * src->gw + cw;
        // The trees' inputs (BL-1672): cover, density, roads, rivers.
        if (i < src->cover.size())
        {
            const std::uint8_t cv[2] = { src->cover[i], i < src->density.size() ? src->density[i] : std::uint8_t{0} };
            h = fnv(h, cv, sizeof cv);
            if (cv[0] == 2u || cv[0] == 3u)
            {
                const std::uint16_t rs = i < src->road_sides.size() ? src->road_sides[i] : 0u;
                const std::uint8_t rv = static_cast<std::uint8_t>(
                    (i < src->river_in.size() ? src->river_in[i] : 0u) | (i < src->river_out.size() ? src->river_out[i] : 0u));
                h = fnv(h, &rs, sizeof rs);
                h = fnv(h, &rv, sizeof rv);
            }
        }
        if (i >= src->inst.of_tile.size())
            return;
        const std::int32_t li = src->inst.of_tile[i];
        if (li < 0)
            return;
        const ui::ground::tile_installation& ti = src->inst.list[static_cast<std::size_t>(li)];
        h = fnv(h, &i, sizeof i);
        for (int j = 0; j < ti.n_stacks; ++j)
        {
            const std::uint32_t p = ti.stacks[j].packed();
            h = fnv(h, &p, sizeof p);
        }
        const std::uint32_t p = ti.settlement.packed();
        h = fnv(h, &p, sizeof p);
    });
    return h;
}

// ---- Buildings (BL-1670) --------------------------------------------------------
// Low-poly solids from the installation pass's own parts, flat-shaded, standing on the
// lowest ground under their footprint. World axes: x east, y up, z south (canonical y).

struct solid_builder
{
    std::vector<vertex>& out;
    std::uint8_t col[3] = { 200, 200, 200 };

    void tri(const float* a, const float* b, const float* c)
    {
        const float e1[3] = { b[0] - a[0], b[1] - a[1], b[2] - a[2] };
        const float e2[3] = { c[0] - a[0], c[1] - a[1], c[2] - a[2] };
        float n[3] = { e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0] };
        const float l = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        if (l < 1e-9f)
            return;
        n[0] /= l; n[1] /= l; n[2] /= l;
        if (n[1] < -0.2f) { n[0] = -n[0]; n[1] = -n[1]; n[2] = -n[2]; } // no culling: face up/out
        for (const float* p : { a, b, c })
        {
            vertex v{};
            v.x = p[0]; v.y = p[1]; v.z = p[2];
            v.nx = n[0]; v.ny = n[1]; v.nz = n[2];
            v.r = col[0]; v.g = col[1]; v.b = col[2]; v.a = 255;
            out.push_back(v);
        }
    }
    void quad(const float* a, const float* b, const float* c, const float* d) { tri(a, b, c); tri(a, c, d); }
    void set(const float* c)
    {
        for (int i = 0; i < 3; ++i)
            col[i] = static_cast<std::uint8_t>(std::clamp(c[i], 0.0f, 1.0f) * 255.0f + 0.5f);
    }
    // An outward normal for a wall: the face's own, flipped away from (cx, cz).
    void wall(const float* a, const float* b, const float* c, const float* d, float cx, float cz)
    {
        const std::size_t first = out.size();
        quad(a, b, c, d);
        const float mx = (a[0] + c[0]) * 0.5f - cx, mz = (a[2] + c[2]) * 0.5f - cz;
        for (std::size_t i = first; i < out.size(); ++i)
            if (out[i].nx * mx + out[i].nz * mz < 0.0f)
            { out[i].nx = -out[i].nx; out[i].ny = -out[i].ny; out[i].nz = -out[i].nz; }
    }
    void box(float cx, float cz, float hw, float hd, float y0, float y1, const float* roof, const float* wallc)
    {
        const float p[8][3] = {
            { cx - hw, y0, cz - hd }, { cx + hw, y0, cz - hd }, { cx + hw, y0, cz + hd }, { cx - hw, y0, cz + hd },
            { cx - hw, y1, cz - hd }, { cx + hw, y1, cz - hd }, { cx + hw, y1, cz + hd }, { cx - hw, y1, cz + hd } };
        set(wallc);
        wall(p[0], p[1], p[5], p[4], cx, cz);
        wall(p[1], p[2], p[6], p[5], cx, cz);
        wall(p[2], p[3], p[7], p[6], cx, cz);
        wall(p[3], p[0], p[4], p[7], cx, cz);
        set(roof);
        quad(p[4], p[5], p[6], p[7]);
    }
    void gable(float cx, float cz, float hw, float hd, float y0, float y1, float rise, const float* roof, const float* wallc)
    {
        box(cx, cz, hw, hd, y0, y1, wallc, wallc);
        const float a[3] = { cx - hw, y1, cz - hd }, b[3] = { cx + hw, y1, cz - hd };
        const float c[3] = { cx + hw, y1, cz + hd }, d[3] = { cx - hw, y1, cz + hd };
        const float r0[3] = { cx - hw, y1 + rise, cz }, r1[3] = { cx + hw, y1 + rise, cz };
        set(roof);
        quad(a, b, r1, r0);
        quad(d, c, r1, r0);
        set(wallc);
        tri(a, d, r0);
        tri(b, c, r1);
    }
    void frustum(float cx, float cz, float rb, float rt, float y0, float y1, int cap, float rise,
                 const float* roof, const float* wallc, int n = 12)
    {
        set(wallc);
        for (int i = 0; i < n; ++i)
        {
            const float a0 = 6.2831853f * i / n, a1 = 6.2831853f * (i + 1) / n;
            const float p0[3] = { cx + rb * std::cos(a0), y0, cz + rb * std::sin(a0) };
            const float p1[3] = { cx + rb * std::cos(a1), y0, cz + rb * std::sin(a1) };
            const float q1[3] = { cx + rt * std::cos(a1), y1, cz + rt * std::sin(a1) };
            const float q0[3] = { cx + rt * std::cos(a0), y1, cz + rt * std::sin(a0) };
            wall(p0, p1, q1, q0, cx, cz);
        }
        if (cap == 3) // open
            return;
        set(roof);
        const float apex = cap == 0 ? y1 : y1 + std::max(rise, cap == 1 ? rt * 0.6f : rt * 0.8f);
        for (int i = 0; i < n; ++i)
        {
            const float a0 = 6.2831853f * i / n, a1 = 6.2831853f * (i + 1) / n;
            const float q0[3] = { cx + rt * std::cos(a0), y1, cz + rt * std::sin(a0) };
            const float q1[3] = { cx + rt * std::cos(a1), y1, cz + rt * std::sin(a1) };
            const float t[3] = { cx, apex, cz };
            if (cap == 1) // a dome: one ring between
            {
                const float mr = rt * 0.72f, my = y1 + (apex - y1) * 0.7f;
                const float m0[3] = { cx + mr * std::cos(a0), my, cz + mr * std::sin(a0) };
                const float m1[3] = { cx + mr * std::cos(a1), my, cz + mr * std::sin(a1) };
                quad(q0, q1, m1, m0);
                tri(m0, m1, t);
            }
            else
                tri(q0, q1, t);
        }
    }
    void sphere(float cx, float cy, float cz, float r, const float* c, int n = 10)
    {
        set(c);
        for (int j = 0; j < n / 2; ++j)
        {
            const float b0 = -1.5707963f + 3.1415927f * j / (n / 2), b1 = -1.5707963f + 3.1415927f * (j + 1) / (n / 2);
            for (int i = 0; i < n; ++i)
            {
                const float a0 = 6.2831853f * i / n, a1 = 6.2831853f * (i + 1) / n;
                const float p00[3] = { cx + r * std::cos(b0) * std::cos(a0), cy + r * std::sin(b0), cz + r * std::cos(b0) * std::sin(a0) };
                const float p10[3] = { cx + r * std::cos(b0) * std::cos(a1), cy + r * std::sin(b0), cz + r * std::cos(b0) * std::sin(a1) };
                const float p11[3] = { cx + r * std::cos(b1) * std::cos(a1), cy + r * std::sin(b1), cz + r * std::cos(b1) * std::sin(a1) };
                const float p01[3] = { cx + r * std::cos(b1) * std::cos(a0), cy + r * std::sin(b1), cz + r * std::cos(b1) * std::sin(a0) };
                const std::size_t first = out.size();
                quad(p00, p10, p11, p01);
                for (std::size_t k = first; k < out.size(); ++k) // smooth: the radial normal
                {
                    float nx = out[k].x - cx, ny = out[k].y - cy, nz = out[k].z - cz;
                    const float l = std::max(1e-6f, std::sqrt(nx * nx + ny * ny + nz * nz));
                    out[k].nx = nx / l; out[k].ny = ny / l; out[k].nz = nz / l;
                }
            }
        }
    }
    void beam(const float* a, const float* b, float w, const float* c)
    {
        // A square prism along a-b, w across.
        float d[3] = { b[0] - a[0], b[1] - a[1], b[2] - a[2] };
        const float l = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        if (l < 1e-5f)
            return;
        d[0] /= l; d[1] /= l; d[2] /= l;
        float u[3] = { 0, 1, 0 };
        if (std::fabs(d[1]) > 0.9f) { u[0] = 1; u[1] = 0; }
        float s1[3] = { d[1] * u[2] - d[2] * u[1], d[2] * u[0] - d[0] * u[2], d[0] * u[1] - d[1] * u[0] };
        const float sl = std::sqrt(s1[0] * s1[0] + s1[1] * s1[1] + s1[2] * s1[2]);
        for (float& v : s1) v /= sl;
        const float s2[3] = { d[1] * s1[2] - d[2] * s1[1], d[2] * s1[0] - d[0] * s1[2], d[0] * s1[1] - d[1] * s1[0] };
        const float hw = std::max(0.004f, w * 0.5f);
        float ca[4][3], cb[4][3];
        const float sg[4][2] = { { 1, 1 }, { -1, 1 }, { -1, -1 }, { 1, -1 } };
        for (int k = 0; k < 4; ++k)
            for (int i = 0; i < 3; ++i)
            {
                ca[k][i] = a[i] + (s1[i] * sg[k][0] + s2[i] * sg[k][1]) * hw;
                cb[k][i] = b[i] + (s1[i] * sg[k][0] + s2[i] * sg[k][1]) * hw;
            }
        set(c);
        const float mx = (a[0] + b[0]) * 0.5f, mz = (a[2] + b[2]) * 0.5f;
        for (int k = 0; k < 4; ++k)
            wall(ca[k], ca[(k + 1) % 4], cb[(k + 1) % 4], cb[k], mx, mz);
    }
};

void build_buildings(const ui::ground::bake_source& src, float x0, float y0, float x1, float y1,
                     std::vector<vertex>& out)
{
    std::vector<ui::ground::structure_solid> solids;
    for_tiles_in(x0, y0, x1, y1, [&](int c, int r) { ui::ground::installation_solids(src, c, r, solids); });
    solid_builder b{ out };
    const float hs = std::max(0.05f, g.s.buildings);
    for (const ui::ground::structure_solid& q : solids)
    {
        // Ground: the lowest under the footprint, so nothing floats on a slope.
        const float rx = std::max(q.hw, 0.02f), rz = std::max(q.kind == ui::ground::structure_solid::cyl ? q.hw : q.hd, 0.02f);
        float base = height_at(q.x, q.y);
        base = std::min(base, height_at(q.x - rx, q.y - rz));
        base = std::min(base, height_at(q.x + rx, q.y - rz));
        base = std::min(base, height_at(q.x - rx, q.y + rz));
        base = std::min(base, height_at(q.x + rx, q.y + rz));
        const float y0b = base + q.z0 * hs, y1b = base + (q.z0 + q.h) * hs;
        using K = ui::ground::structure_solid;
        switch (q.kind)
        {
        case K::box:
            b.box(q.x, q.y, q.hw, q.hd, base - 0.02f, y1b, q.roof, q.wall);
            break;
        case K::gable:
            b.gable(q.x, q.y, q.hw, q.hd, base - 0.02f, y1b, q.rh * hs, q.roof, q.wall);
            break;
        case K::saw:
            b.gable(q.x, q.y, q.hw, q.hd, base - 0.02f, y1b, q.h * 0.25f * hs, q.roof, q.wall);
            break;
        case K::cyl:
            b.frustum(q.x, q.y, q.hw, q.hd, y0b - (q.z0 > 0 ? 0.0f : 0.02f), y1b, q.cap, q.rh * hs, q.roof, q.wall);
            break;
        case K::dome:
            b.frustum(q.x, q.y, q.hw, q.hw * 0.98f, base - 0.02f, base + 0.01f, 1, q.h * hs, q.roof, q.roof);
            break;
        case K::heap:
            b.frustum(q.x, q.y, q.hw, 0.001f, base - 0.02f, base + q.h * hs, 0, 0.0f, q.roof, q.roof, 10);
            break;
        case K::sphere:
            b.sphere(q.x, base + (q.z0 + q.hw) * hs, q.y, q.hw, q.roof);
            break;
        case K::beam:
        {
            const float a[3] = { q.x, y0b, q.y };
            const float c[3] = { q.x2, base + q.z2 * hs, q.y2 };
            b.beam(a, c, q.w, q.wall);
            break;
        }
        case K::ring:
        {
            constexpr int n = 10;
            for (int i = 0; i < n; ++i)
            {
                const float a0 = 6.2831853f * i / n, a1 = 6.2831853f * (i + 1) / n;
                const float p0[3] = { q.x + q.hw * std::cos(a0), y0b + q.hw * std::sin(a0) * hs, q.y };
                const float p1[3] = { q.x + q.hw * std::cos(a1), y0b + q.hw * std::sin(a1) * hs, q.y };
                b.beam(p0, p1, q.w, q.wall);
            }
            break;
        }
        }
    }
}

// ---- Trees (BL-1672) -------------------------------------------------------------
// Forest and scrub stand as low-poly trees, scattered by a seed of the tile: conifer cones
// and broadleaf crowns (a double cone), bushes on scrub. Off roads, rivers and installations;
// coloured from the tile's own ground colour toward a canopy green, so they match the bake.

std::uint32_t tree_seed(int c, int r)
{
    std::uint32_t h = static_cast<std::uint32_t>(c) * 73856093u ^ static_cast<std::uint32_t>(r) * 19349663u ^ 0x9e3779b9u;
    h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
    return h;
}

float rnd01(std::uint32_t& s)
{
    s = s * 1664525u + 1013904223u;
    return static_cast<float>(s >> 8) / 16777216.0f;
}

float seg_dist2(float px, float py, float ax, float ay, float bx, float by)
{
    const float vx = bx - ax, vy = by - ay;
    const float l2 = vx * vx + vy * vy;
    const float t = l2 > 0.0f ? std::clamp(((px - ax) * vx + (py - ay) * vy) / l2, 0.0f, 1.0f) : 0.0f;
    const float dx = px - (ax + t * vx), dy = py - (ay + t * vy);
    return dx * dx + dy * dy;
}

void build_trees(const ui::ground::bake_source& src, float x0, float y0, float x1, float y1,
                 std::vector<vertex>& out)
{
    if (g.s.trees <= 0.0f || src.gw <= 0)
        return;
    // Side i's unit direction, y south (side order 0=E, 1=NE, 2=NW, 3=W, 4=SW, 5=SE).
    constexpr float k_dir[6][2] = { { 1.0f, 0.0f }, { 0.5f, -0.8660254f }, { -0.5f, -0.8660254f },
                                    { -1.0f, 0.0f }, { -0.5f, 0.8660254f }, { 0.5f, 0.8660254f } };
    constexpr float k_road_clear  = 0.20f; // canonical: half a road's clearing
    constexpr float k_river_clear = 0.16f;
    constexpr float k_canopy[3]   = { 0.16f, 0.27f, 0.12f };
    solid_builder b{ out };
    for_tiles_in(x0, y0, x1, y1, [&](int c, int r) {
        const int cw = ((c % src.gw) + src.gw) % src.gw;
        const std::size_t i = static_cast<std::size_t>(r) * src.gw + cw;
        if (i >= src.cls.size() || i >= src.cover.size()
            || src.cls[i] != static_cast<std::uint8_t>(ui::ground::bake_source::tile_class::land))
            return;
        const std::uint8_t cov = src.cover[i];
        const bool forest = cov == 3u, scrub = cov == 2u;
        if (!forest && !scrub)
            return;
        if (i < src.inst.of_tile.size() && src.inst.of_tile[i] >= 0)
            return; // the installation owns the tile's ground
        const float dens = i < src.density.size() ? src.density[i] / 255.0f : 1.0f;
        const int n = static_cast<int>(std::lround((forest ? 18.0f : 5.0f) * std::max(0.25f, dens) * g.s.trees));
        if (n <= 0)
            return;
        float cx, cy;
        centre_of(c, r, cx, cy);
        const std::uint16_t rs = i < src.road_sides.size() ? src.road_sides[i] : 0u;
        const bool road_here = (i < src.road.size() && src.road[i] != 0u) || rs != 0u;
        const std::uint8_t rv = static_cast<std::uint8_t>(
            (i < src.river_in.size() ? src.river_in[i] : 0u) | (i < src.river_out.size() ? src.river_out[i] : 0u));
        // The tile's ground colour (ABGR), pulled toward the canopy green.
        float base[3] = { k_canopy[0], k_canopy[1], k_canopy[2] };
        if (i < src.colour.size())
        {
            const std::uint32_t col = src.colour[i];
            const float tc[3] = { (col & 0xFFu) / 255.0f, ((col >> 8) & 0xFFu) / 255.0f, ((col >> 16) & 0xFFu) / 255.0f };
            for (int k = 0; k < 3; ++k)
                base[k] = tc[k] * 0.45f + k_canopy[k] * 0.55f;
        }
        const float jit = i < src.jitter.size() ? src.jitter[i] : 0.0f;
        const float conifer_share = std::clamp(0.45f + 0.35f * jit, 0.1f, 0.9f);
        std::uint32_t s = tree_seed(cw, r);
        int placed = 0;
        for (int t = 0; t < n * 3 && placed < n; ++t)
        {
            // A point in the hex's inscribed circle, a little in from its edge.
            const float a = rnd01(s) * 6.2831853f, d = std::sqrt(rnd01(s)) * 0.80f;
            const float px = cx + d * std::cos(a), py = cy + d * std::sin(a);
            bool clear = true;
            for (int k = 0; k < 6 && clear; ++k)
            {
                const float mx = cx + 0.8660254f * k_dir[k][0], my = cy + 0.8660254f * k_dir[k][1];
                if (((rs >> (2 * k)) & 3u) != 0u && seg_dist2(px, py, cx, cy, mx, my) < k_road_clear * k_road_clear)
                    clear = false;
                if (((rv >> k) & 1u) != 0u && seg_dist2(px, py, cx, cy, mx, my) < k_river_clear * k_river_clear)
                    clear = false;
            }
            if (clear && road_here && rs == 0u && (px - cx) * (px - cx) + (py - cy) * (py - cy) < 0.09f)
                clear = false; // a road's end at the centre
            const float u0 = rnd01(s), u1 = rnd01(s), u2 = rnd01(s);
            if (!clear)
                continue;
            ++placed;
            const float ground = height_at(px, py) - 0.015f;
            const float tone = 0.82f + 0.30f * u1;
            const bool conifer = !scrub && u0 < conifer_share;
            float crown[3];
            for (int k = 0; k < 3; ++k)
                crown[k] = std::clamp(base[k] * tone * (conifer ? 0.80f : 1.0f), 0.0f, 1.0f);
            const float hgt = (scrub ? 0.05f : 0.10f) + (scrub ? 0.03f : 0.08f) * u2;
            if (conifer)
            {
                const float rad = hgt * 0.34f;
                b.frustum(px, py, rad, 0.0f, ground + hgt * 0.12f, ground + hgt, 3, 0.0f, crown, crown, 6);
            }
            else
            {
                // A broadleaf crown: a double cone, widest two-thirds up.
                const float rad = hgt * (scrub ? 0.55f : 0.42f);
                const float ym = ground + hgt * 0.55f;
                b.frustum(px, py, rad * 0.35f, rad, ground + hgt * 0.15f, ym, 3, 0.0f, crown, crown, 6);
                b.frustum(px, py, rad, 0.0f, ym, ground + hgt, 3, 0.0f, crown, crown, 6);
            }
        }
    });
}

// Builds one chunk's ground grid (and its buildings), absolute canonical coordinates.
bool build_chunk(chunk_mesh& m, const ui::ground::bake_source* src)
{
    const int n = k_grid + 1;
    const float dx = (m.x1 - m.x0) / k_grid, dy = (m.y1 - m.y0) / k_grid;
    std::vector<float> hgt(static_cast<std::size_t>(n + 2) * (n + 2));
    for (int y = -1; y <= n; ++y)
        for (int x = -1; x <= n; ++x)
            hgt[static_cast<std::size_t>(y + 1) * (n + 2) + (x + 1)] = height_at(m.x0 + x * dx, m.y0 + y * dy);
    auto H = [&](int x, int y) { return hgt[static_cast<std::size_t>(y + 1) * (n + 2) + (x + 1)]; };
    std::vector<vertex> verts;
    verts.reserve(static_cast<std::size_t>(n) * n);
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x)
        {
            vertex v{};
            v.x = m.x0 + x * dx;
            v.z = m.y0 + y * dy;
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
    std::vector<vertex> bverts;
    if (src && m.x1 - m.x0 <= k_building_max_chunk)
    {
        build_buildings(*src, m.x0, m.y0, m.x1, m.y1, bverts);
        build_trees(*src, m.x0, m.y0, m.x1, m.y1, bverts);
    }
    if (!m.vbuf)
        m.vbuf = make_buffer(SDL_GPU_BUFFERUSAGE_VERTEX, static_cast<Uint32>(verts.size() * sizeof(vertex)));
    if (m.bbuf)
    {
        SDL_ReleaseGPUBuffer(g.device, m.bbuf);
        m.bbuf = nullptr;
    }
    m.n_building_vertices = 0;
    if (!bverts.empty())
        m.bbuf = make_buffer(SDL_GPU_BUFFERUSAGE_VERTEX, static_cast<Uint32>(bverts.size() * sizeof(vertex)));
    if (!m.vbuf)
        return fail("chunk mesh");
    upload_item items[2] = {
        { m.vbuf, verts.data(), static_cast<Uint32>(verts.size() * sizeof(vertex)) },
        { m.bbuf, bverts.data(), m.bbuf ? static_cast<Uint32>(bverts.size() * sizeof(vertex)) : 0u } };
    if (!upload(items, m.bbuf ? 2 : 1))
        return false;
    if (m.bbuf)
        m.n_building_vertices = static_cast<Uint32>(bverts.size());
    return true;
}

// The backdrop: the far page as a coarse mesh around the view, without the bumps, a little
// below the chunks. Where the stencil runs it draws only where no chunk did.
void build_far(const chunk& far, float cx, float cy, float half)
{
    const float fx0 = cx - half, fy0 = std::max(far.y0, cy - half);
    const float fy1 = std::min(far.y1, cy + half);
    if (fy1 <= fy0)
    {
        g.far_n_index = 0;
        return;
    }
    const float dx = 2.0f * half / k_far, dy = (fy1 - fy0) / k_far;
    const int nf = k_far + 1;
    std::vector<float> fh(static_cast<std::size_t>(nf + 2) * (nf + 2));
    for (int y = -1; y <= nf; ++y)
        for (int x = -1; x <= nf; ++x)
            fh[static_cast<std::size_t>(y + 1) * (nf + 2) + (x + 1)] = height_at(fx0 + x * dx, fy0 + y * dy, false);
    auto F = [&](int x, int y) { return fh[static_cast<std::size_t>(y + 1) * (nf + 2) + (x + 1)]; };
    std::vector<vertex> verts;
    verts.reserve(static_cast<std::size_t>(nf) * nf);
    for (int y = 0; y < nf; ++y)
        for (int x = 0; x < nf; ++x)
        {
            vertex v{};
            const float px = fx0 + x * dx, py = fy0 + y * dy;
            v.x = px;
            v.z = py;
            v.y = F(x, y) - (g.stencil ? 0.0f : 0.15f);
            v.u = (px - far.x0) / (far.x1 - far.x0);
            v.v = (py - far.y0) / (far.y1 - far.y0);
            const float gx = (F(x + 1, y) - F(x - 1, y)) / (2.0f * dx);
            const float gz = (F(x, y + 1) - F(x, y - 1)) / (2.0f * dy);
            const float len = std::sqrt(gx * gx + 1.0f + gz * gz);
            v.nx = -gx / len; v.ny = 1.0f / len; v.nz = -gz / len;
            verts.push_back(v);
        }
    std::vector<Uint32> idx;
    idx.reserve(static_cast<std::size_t>(k_far) * k_far * 6);
    for (int y = 0; y < k_far; ++y)
        for (int x = 0; x < k_far; ++x)
        {
            const Uint32 a = static_cast<Uint32>(y * nf + x), b = a + 1, c = a + nf, d = c + 1;
            idx.insert(idx.end(), { a, c, b, b, c, d });
        }
    if (!g.far_vbuf)
        g.far_vbuf = make_buffer(SDL_GPU_BUFFERUSAGE_VERTEX, static_cast<Uint32>(verts.size() * sizeof(vertex)));
    if (!g.far_ibuf)
        g.far_ibuf = make_buffer(SDL_GPU_BUFFERUSAGE_INDEX, static_cast<Uint32>(idx.size() * sizeof(Uint32)));
    const upload_item items[2] = {
        { g.far_vbuf, verts.data(), static_cast<Uint32>(verts.size() * sizeof(vertex)) },
        { g.far_ibuf, idx.data(), static_cast<Uint32>(idx.size() * sizeof(Uint32)) } };
    g.far_n_index = (g.far_vbuf && g.far_ibuf && upload(items, 2)) ? static_cast<Uint32>(idx.size()) : 0u;
}

// ---- The camera (BL-1669) ---------------------------------------------------------

void set_camera(const view& v)
{
    camera& c = g.cam;
    c.v = v;
    c.ox = v.cx;
    c.oy = v.cy;
    // The tilt follows zoom (RENDERING.md § The camera): straight down at the 12 px rung and
    // wider, rising on a log scale to the setting at the 96 px rung. No turn.
    const float k = std::clamp(std::log2(std::max(v.draw_r, 1.0f) / 12.0f) / 3.0f, 0.0f, 1.0f);
    const float tilt = g.s.tilt_deg * k * k * (3.0f - 2.0f * k) * k_d2r;
    c.tan_half = std::tan(0.5f * g.s.fov_deg * k_d2r);
    c.aspect = std::max(0.1f, v.w / std::max(1.0f, v.h));
    // The distance at which the view's height fills the frame looking straight down, so the
    // zoom is the canvas's.
    c.dist = std::max(0.5f, v.view_h / (2.0f * c.tan_half));
    c.tx = 0.0f;
    c.tz = 0.0f;
    c.ty = height_at(v.cx, v.cy, false);
    c.ex = c.tx;
    c.ey = c.ty + c.dist * std::cos(tilt);
    c.ez = c.tz + c.dist * std::sin(tilt);
    float fx = c.tx - c.ex, fy = c.ty - c.ey, fz = c.tz - c.ez;
    const float fl = std::sqrt(fx * fx + fy * fy + fz * fz);
    fx /= fl; fy /= fl; fz /= fl;
    // Up: world up, or north when looking straight down.
    float ux = 0, uy = 1, uz = 0;
    if (std::fabs(fy) > 0.999f) { ux = 0; uy = 0; uz = -1; }
    float sx = fy * uz - fz * uy, sy = fz * ux - fx * uz, sz = fx * uy - fy * ux;
    const float sl = std::sqrt(sx * sx + sy * sy + sz * sz);
    sx /= sl; sy /= sl; sz /= sl;
    const float vx = sy * fz - sz * fy, vy = sz * fx - sx * fz, vz = sx * fy - sy * fx;
    c.fx = fx; c.fy = fy; c.fz = fz;
    c.sx = sx; c.sy = sy; c.sz = sz;
    c.ux = vx; c.uy = vy; c.uz = vz;
    mat4 vw{};
    vw.m[0] = sx;  vw.m[4] = sy;  vw.m[8]  = sz;  vw.m[12] = -(sx * c.ex + sy * c.ey + sz * c.ez);
    vw.m[1] = vx;  vw.m[5] = vy;  vw.m[9]  = vz;  vw.m[13] = -(vx * c.ex + vy * c.ey + vz * c.ez);
    vw.m[2] = -fx; vw.m[6] = -fy; vw.m[10] = -fz; vw.m[14] = (fx * c.ex + fy * c.ey + fz * c.ez);
    vw.m[15] = 1.0f;
    c.near_z = std::max(0.05f, c.dist * 0.02f);
    c.far_z = c.dist * 12.0f + 60.0f;
    const float f = 1.0f / c.tan_half;
    mat4 pr{};
    pr.m[0] = f / c.aspect;
    pr.m[5] = f;
    pr.m[10] = c.far_z / (c.near_z - c.far_z);
    pr.m[11] = -1.0f;
    pr.m[14] = c.near_z * c.far_z / (c.near_z - c.far_z);
    c.mvp = mul(pr, vw);
    c.ok = true;
}

// The view ray through a screen point (logical px): origin the eye, world axes.
void ray_of(float sx, float sy, float& dx, float& dy, float& dz)
{
    const camera& c = g.cam;
    const float nx = ((sx - c.v.x) / c.v.w) * 2.0f - 1.0f;
    const float ny = 1.0f - ((sy - c.v.y) / c.v.h) * 2.0f;
    dx = c.fx + (nx * c.tan_half * c.aspect) * c.sx + (ny * c.tan_half) * c.ux;
    dy = c.fy + (nx * c.tan_half * c.aspect) * c.sy + (ny * c.tan_half) * c.uy;
    dz = c.fz + (nx * c.tan_half * c.aspect) * c.sz + (ny * c.tan_half) * c.uz;
    const float l = std::sqrt(dx * dx + dy * dy + dz * dz);
    dx /= l; dy /= l; dz /= l;
}

// Where the ray through a screen point meets the plane y = h (world), canonical. False when
// it never does.
bool ray_plane(float sx, float sy, float h, float& cx, float& cy)
{
    const camera& c = g.cam;
    float dx, dy, dz;
    ray_of(sx, sy, dx, dy, dz);
    if (dy > -1e-4f)
        return false;
    const float t = (h - c.ey) / dy;
    cx = c.ex + dx * t + c.ox;
    cy = c.ez + dz * t + c.oy;
    return true;
}

} // namespace

void set_tiles(const std::uint32_t* tiles, int gw, int gh)
{
    if (!tiles || gw <= 0 || gh <= 0)
        return;
    const std::size_t n = static_cast<std::size_t>(gw) * gh;
    g.gw = gw;
    g.gh = gh;
    g.tiles.assign(tiles, tiles + n);
    g.tiles_hash = fnv(1469598103934665603ull, g.tiles.data(), n * sizeof(std::uint32_t));
}

bool begin_frame(SDL_Renderer* renderer, const view& v)
{
    g.active = false;
    g.rendered = false;
    if (!renderer)
        return false;
    if (!g.tried)
        g.ok = init(renderer);
    if (!g.ok)
        return false;
    set_camera(v);
    if (!ensure_target(static_cast<int>(std::lround(v.w * v.fb)), static_cast<int>(std::lround(v.h * v.fb))))
        return false;
    g.active = true;
    ++g.frame_no;
    return true;
}

bool active() { return g.active; }
void set_unbudgeted(bool on) { g.unbudgeted = on; }

bool project(float cx, float cy, float& sx, float& sy)
{
    const camera& c = g.cam;
    if (!c.ok)
        return false;
    const float x = cx - c.ox, z = cy - c.oy, y = height_at(cx, cy);
    const float* m = c.mvp.m;
    const float px = m[0] * x + m[4] * y + m[8] * z + m[12];
    const float py = m[1] * x + m[5] * y + m[9] * z + m[13];
    const float pw = m[3] * x + m[7] * y + m[11] * z + m[15];
    if (pw <= 1e-4f)
        return false;
    sx = c.v.x + (px / pw * 0.5f + 0.5f) * c.v.w;
    sy = c.v.y + (0.5f - py / pw * 0.5f) * c.v.h;
    return true;
}

bool in_sight(float cx, float cy)
{
    const camera& c = g.cam;
    if (!c.ok)
        return true;
    const float x = cx - c.ox, z = cy - c.oy, y = height_at(cx, cy) + 0.03f;
    float dx = c.ex - x, dy = c.ey - y, dz = c.ez - z;
    const float l = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (l < 1e-4f)
        return true;
    dx /= l; dy /= l; dz /= l;
    // Out to where the ray clears the highest ground.
    const float top = max_height();
    const float reach = dy > 1e-4f ? std::min(l, (top - y) / dy) : l;
    constexpr int k_steps = 24;
    for (int i = 1; i <= k_steps; ++i)
    {
        const float t = reach * static_cast<float>(i) / k_steps;
        const float px = x + dx * t, py = y + dy * t, pz = z + dz * t;
        if (height_at(px + c.ox, pz + c.oy) > py + 0.01f)
            return false;
    }
    return true;
}

bool pick(float sx, float sy, float& cx, float& cy)
{
    const camera& c = g.cam;
    if (!c.ok)
        return false;
    float dx, dy, dz;
    ray_of(sx, sy, dx, dy, dz);
    if (dy > -1e-4f)
        return false;
    // From where the ray drops below the highest ground to where it drops below the lowest.
    const float top = max_height(), bot = min_height();
    const float t0 = std::max(0.0f, (top - c.ey) / dy);
    const float t1 = (bot - c.ey) / dy;
    const float step = std::max(0.01f, (t1 - t0) / 160.0f);
    float prev_t = t0, prev_d = 0.0f;
    bool have_prev = false;
    for (float t = t0; t <= t1 + step; t += step)
    {
        const float px = c.ex + dx * t, py = c.ey + dy * t, pz = c.ez + dz * t;
        const float d = py - height_at(px + c.ox, pz + c.oy);
        if (d <= 0.0f)
        {
            float lo = have_prev ? prev_t : t, hi = t;
            for (int i = 0; i < 12 && have_prev; ++i)
            {
                const float mid = 0.5f * (lo + hi);
                const float qx = c.ex + dx * mid, qy = c.ey + dy * mid, qz = c.ez + dz * mid;
                if (qy - height_at(qx + c.ox, qz + c.oy) > 0.0f) lo = mid; else hi = mid;
            }
            cx = c.ex + dx * hi + c.ox;
            cy = c.ez + dz * hi + c.oy;
            return true;
        }
        prev_t = t;
        prev_d = d;
        have_prev = true;
    }
    (void)prev_d;
    return ray_plane(sx, sy, 0.0f, cx, cy);
}

void footprint(float& x0, float& y0, float& x1, float& y1)
{
    const camera& c = g.cam;
    const view& v = c.v;
    x0 = y0 = 1e30f;
    x1 = y1 = -1e30f;
    const float lo = min_height(), hi = max_height();
    // The corners and edge midpoints, met with the lowest and the highest ground: whatever
    // the terrain, what is seen lies between.
    const float pts[8][2] = { { v.x, v.y }, { v.x + v.w * 0.5f, v.y }, { v.x + v.w, v.y },
                              { v.x + v.w, v.y + v.h * 0.5f }, { v.x + v.w, v.y + v.h },
                              { v.x + v.w * 0.5f, v.y + v.h }, { v.x, v.y + v.h }, { v.x, v.y + v.h * 0.5f } };
    const float reach = c.dist * 6.0f;
    for (const auto& p : pts)
        for (const float h : { lo, hi })
        {
            float cx, cy;
            if (!ray_plane(p[0], p[1], h, cx, cy))
            {
                // Above the horizon: as far as the frame reaches.
                float dx, dy, dz;
                ray_of(p[0], p[1], dx, dy, dz);
                const float l = std::max(1e-4f, std::sqrt(dx * dx + dz * dz));
                cx = c.ex + dx / l * reach + c.ox;
                cy = c.ez + dz / l * reach + c.oy;
            }
            const float ddx = cx - (c.ex + c.ox), ddy = cy - (c.ez + c.oy);
            const float d = std::sqrt(ddx * ddx + ddy * ddy);
            if (d > reach)
            {
                cx = c.ex + c.ox + ddx / d * reach;
                cy = c.ez + c.oy + ddy / d * reach;
            }
            x0 = std::min(x0, cx); x1 = std::max(x1, cx);
            y0 = std::min(y0, cy); y1 = std::max(y1, cy);
        }
}

float height(float cx, float cy) { return height_at(cx, cy); }

SDL_Texture* frame() { return g.active ? g.shown : nullptr; }

SDL_Texture* drape_coords() { return g.active ? g.coord_shown : nullptr; }

int drape_generation() { return g.drape_gen; }

SDL_Texture* drape_target(int w, int h)
{
    if (!g.ok || !g.renderer)
        return nullptr;
    w = std::clamp(w, 16, 8192);
    h = std::clamp(h, 16, 8192);
    if (g.drape && g.drape_w >= w && g.drape_h >= h)
        return g.drape;
    if (g.drape)
        SDL_DestroyTexture(g.drape);
    const int nw = std::max(w, g.drape_w), nh = std::max(h, g.drape_h);
    g.drape = SDL_CreateTexture(g.renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_TARGET, nw, nh);
    ++g.drape_gen;
    g.drape_w = g.drape ? nw : 0;
    g.drape_h = g.drape ? nh : 0;
    if (g.drape)
        SDL_SetTextureBlendMode(g.drape, SDL_BLENDMODE_NONE);
    return g.drape;
}

namespace {

SDL_GPUTexture* gpu_of(SDL_Texture* t)
{
    if (!t)
        return nullptr;
    return static_cast<SDL_GPUTexture*>(SDL_GetPointerProperty(
        SDL_GetTextureProperties(t), SDL_PROP_TEXTURE_GPU_TEXTURE_POINTER, nullptr));
}

// Keeps a mesh for every chunk in view (building within the frame's budget, nearest the
// view first), releases what has gone unused.
void update_meshes(const inputs& in)
{
    const Uint64 t0 = SDL_GetPerformanceCounter();
    const double freq = static_cast<double>(SDL_GetPerformanceFrequency());
    const std::uint64_t hk = height_key();
    const float period = k_s3 * static_cast<float>(std::max(1, g.gw));
    const camera& c = g.cam;
    struct want { int i; float d; };
    std::vector<want> todo;
    g.built_this_frame = 0;
    for (chunk_mesh& m : g.meshes)
        m.tex = nullptr;
    for (int i = 0; i < in.n_chunks; ++i)
    {
        const chunk& ch = in.chunks[i];
        if (!ch.tex || ch.x1 <= ch.x0 || ch.y1 <= ch.y0)
            continue;
        chunk_mesh* m = nullptr;
        for (chunk_mesh& q : g.meshes)
            if (q.x0 == ch.x0 && q.y0 == ch.y0 && q.x1 == ch.x1 && q.y1 == ch.y1)
            { m = &q; break; }
        if (!m)
        {
            g.meshes.push_back(chunk_mesh{});
            m = &g.meshes.back();
            m->x0 = ch.x0; m->y0 = ch.y0; m->x1 = ch.x1; m->y1 = ch.y1;
        }
        m->tex = ch.tex;
        m->last_used = g.frame_no;
        // The wrap copy nearest the view.
        const float mid = 0.5f * (ch.x0 + ch.x1);
        m->dx = period * std::round((c.ox - mid) / period);
        const void* sp = in.src.get();
        if (m->src_seen != sp)
        {
            m->src_seen = sp;
            m->want_inst = (m->x1 - m->x0 <= k_building_max_chunk)
                ? installations_hash(in.src.get(), m->x0, m->y0, m->x1, m->y1) : 0u;
        }
        if (!m->built || m->built_key != hk || m->built_inst != m->want_inst)
        {
            const float ddx = mid + m->dx - c.ox, ddy = 0.5f * (ch.y0 + ch.y1) - c.oy;
            // A chunk with nothing to show yet goes first; a stale one keeps drawing.
            todo.push_back({ static_cast<int>(&*m - g.meshes.data()),
                             (m->built ? 1e6f : 0.0f) + ddx * ddx + ddy * ddy });
        }
    }
    std::sort(todo.begin(), todo.end(), [](const want& a, const want& b) { return a.d < b.d; });
    for (const want& w : todo)
    {
        const double used_us = static_cast<double>(SDL_GetPerformanceCounter() - t0) * 1e6 / freq;
        if (!g.unbudgeted && g.built_this_frame > 0 && used_us > k_mesh_budget_us)
            break;
        chunk_mesh& m = g.meshes[static_cast<std::size_t>(w.i)];
        if (build_chunk(m, in.src.get()))
        {
            m.built = true;
            m.built_key = hk;
            m.built_inst = m.want_inst;
        }
        ++g.built_this_frame;
    }
    g.pending = 0;
    for (const chunk_mesh& m : g.meshes)
        if (m.tex && (!m.built || m.built_key != hk || m.built_inst != m.want_inst))
            ++g.pending;
    // Release what has gone unused.
    for (std::size_t i = 0; i < g.meshes.size();)
    {
        chunk_mesh& m = g.meshes[i];
        if (g.frame_no - m.last_used > k_mesh_keep_frames)
        {
            if (m.vbuf) SDL_ReleaseGPUBuffer(g.device, m.vbuf);
            if (m.bbuf) SDL_ReleaseGPUBuffer(g.device, m.bbuf);
            m = g.meshes.back();
            g.meshes.pop_back();
            continue;
        }
        ++i;
    }
    // The backdrop, around the view in half-view steps.
    if (in.far.tex && in.far.x1 > in.far.x0 && in.far.y1 > in.far.y0)
    {
        const float half = std::max(40.0f, c.dist * 6.0f);
        const float step = std::max(4.0f, c.v.view_h * 0.5f);
        const float qx = step * std::floor(c.ox / step), qy = step * std::floor(c.oy / step);
        std::uint64_t fk = fnv(hk, &qx, sizeof qx);
        fk = fnv(fk, &qy, sizeof qy);
        fk = fnv(fk, &half, sizeof half);
        fk = fnv(fk, &in.far, sizeof(chunk));
        if (fk != g.far_key)
        {
            g.far_key = fk;
            build_far(in.far, qx, qy, half);
        }
        g.far_tex = in.far.tex;
    }
    else
        g.far_tex = nullptr;
    g.build_ms = static_cast<double>(SDL_GetPerformanceCounter() - t0) * 1000.0 / freq;
}

} // namespace

void render(const inputs& in)
{
    if (!g.active)
        return;
    update_meshes(in);
    const Uint64 t0 = SDL_GetPerformanceCounter();
    const camera& c = g.cam;

    // The sun: the bake's light turned and raised by the sliders.
    const float az0 = std::atan2(k_bake_light[2], k_bake_light[0]);
    const float az = az0 + g.s.sun_turn * k_d2r, el = g.s.sun_deg * k_d2r;
    const float lx = std::cos(el) * std::cos(az), ly = std::sin(el), lz = std::cos(el) * std::sin(az);
    // An orthographic box from the sun over the ground in view.
    float fx0, fy0, fx1, fy1;
    footprint(fx0, fy0, fx1, fy1);
    const float half = std::clamp(std::max({ std::fabs(fx0 - c.ox), std::fabs(fx1 - c.ox),
                                             std::fabs(fy0 - c.oy), std::fabs(fy1 - c.oy) }) + 1.0f, 2.0f, 400.0f);
    const float reach = half + g.s.height * 2.0f + 10.0f;
    const float sx_ = c.tx + lx * reach, sy_ = c.ty + ly * reach, sz_ = c.tz + lz * reach;
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

    // The renderer's pending work (chunk uploads, the tables, the drape) goes first, so the
    // mesh samples it.
    SDL_FlushRenderer(g.renderer);
    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(g.device);
    if (!cmd)
    {
        fail("command buffer");
        return;
    }
    const float bl = std::sqrt(k_bake_light[0] * k_bake_light[0] + k_bake_light[1] * k_bake_light[1]
                               + k_bake_light[2] * k_bake_light[2]);
    const detail& d = in.det;
    // The drape is composited after this pass, in SDL's queue (drape.frag.hlsl); this pass
    // only needs its placement, so it writes each pixel's drape coordinate.
    const bool has_drape = in.drape_ax != 0.0f && in.drape_ay != 0.0f;
    struct
    {
        float light[4], sky[4], eye[4], shade[4], org[4], det[4], detr[4], ink[4];
        float bump[4], famk[4], wind[4], anim[4], drape[4], misc[4];
    } fsu = {
        { lx, ly, lz, in.lift_gamma },
        { k_sky[0], k_sky[1], k_sky[2], std::max(4.0f, c.dist) },
        { c.ex, c.ey, c.ez, k_bake_light[1] / bl },
        { g.s.shadow, 0.0015f, 1.0f / k_shadow_n, 0.0f },
        { c.ox, c.oy, d.time, 1.0f / std::max(0.25f, c.v.fb) },
        { d.coarse ? d.strength : 0.0f, d.period, d.fine ? d.fine_strength : 0.0f, d.fine_period },
        { d.r_none, d.r_full, d.map ? d.map_r_none : 0.0f, d.map_r_full },
        { d.coast_px, d.river_px, d.edge_band, d.tone_swing },
        { d.bump[0], d.bump[1], d.bump[2], d.bump[3] },
        { d.famk[0], d.famk[1], d.famk[2], d.famk[3] },
        { d.wind[0], d.wind[1], d.wind[2], d.wind[3] },
        { d.mountain_mul, d.gust, d.flutter, d.creep },
        { in.drape_ax, in.drape_ay, in.drape_bx, in.drape_by },
        { has_drape ? 1.0f : 0.0f, d.fine_bump_share, g.s.bump_light, d.tiles ? 1.0f : 0.0f } };

    struct { float mvp[16]; float sun[16]; float off[4]; } vsu{};
    std::memcpy(vsu.sun, sun.m, sizeof sun.m);

    // The shadow pass: depth from the sun - the chunks and their buildings.
    std::size_t n_tris = 0, n_btris = 0;
    if (g.s.shadow > 0.0f)
    {
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
            SDL_BindGPUGraphicsPipeline(sp, g.pipe_shadow);
            std::memcpy(vsu.mvp, sun.m, sizeof sun.m);
            for (const chunk_mesh& m : g.meshes)
            {
                if (!m.tex || !m.vbuf || !m.built)
                    continue;
                vsu.off[0] = m.dx - c.ox; vsu.off[1] = 0.0f; vsu.off[2] = -c.oy; vsu.off[3] = 0.0f;
                SDL_PushGPUVertexUniformData(cmd, 0, &vsu, sizeof vsu);
                const SDL_GPUBufferBinding vb{ m.vbuf, 0 };
                SDL_BindGPUVertexBuffers(sp, 0, &vb, 1);
                const SDL_GPUBufferBinding ib{ g.grid_ibuf, 0 };
                SDL_BindGPUIndexBuffer(sp, &ib, SDL_GPU_INDEXELEMENTSIZE_32BIT);
                SDL_DrawGPUIndexedPrimitives(sp, k_grid * k_grid * 6, 1, 0, 0, 0);
                if (m.bbuf && m.n_building_vertices)
                {
                    const SDL_GPUBufferBinding bb{ m.bbuf, 0 };
                    SDL_BindGPUVertexBuffers(sp, 0, &bb, 1);
                    SDL_DrawGPUPrimitives(sp, m.n_building_vertices, 1, 0, 0);
                }
            }
            SDL_EndGPURenderPass(sp);
        }
    }

    std::memcpy(vsu.mvp, c.mvp.m, sizeof c.mvp.m);
    SDL_GPUColorTargetInfo cts[2]{};
    SDL_GPUColorTargetInfo& ct = cts[0];
    ct.texture = g.colour;
    ct.clear_color = SDL_FColor{ k_sky[0], k_sky[1], k_sky[2], 1.0f };
    ct.load_op = SDL_GPU_LOADOP_CLEAR;
    ct.store_op = SDL_GPU_STOREOP_STORE;
    cts[1].texture = g.coord;
    cts[1].clear_color = SDL_FColor{ 0.0f, 0.0f, 0.0f, 0.0f };
    cts[1].load_op = SDL_GPU_LOADOP_CLEAR;
    cts[1].store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPUDepthStencilTargetInfo dt{};
    dt.texture = g.depth;
    dt.clear_depth = 1.0f;
    dt.clear_stencil = 0;
    dt.load_op = SDL_GPU_LOADOP_CLEAR;
    dt.store_op = SDL_GPU_STOREOP_DONT_CARE;
    dt.stencil_load_op = SDL_GPU_LOADOP_CLEAR;
    dt.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cmd, cts, 2, &dt);
    if (pass)
    {
        // Samplers: 0 the bake, 1 the shadow map, 2 the tile table, 3 coarse detail, 4 fine
        // detail, 5 the rolling ground, 6 the map table, 7 the drape. A missing table binds
        // the shadow map in its place (the shader leaves it unread).
        SDL_GPUTexture* t_tiles = gpu_of(d.tiles);
        SDL_GPUTexture* t_coarse = gpu_of(d.coarse);
        SDL_GPUTexture* t_fine = gpu_of(d.fine);
        SDL_GPUTexture* t_roll = gpu_of(d.rolling);
        SDL_GPUTexture* t_map = gpu_of(d.map);
        SDL_GPUTexture* t_drape = nullptr; // read by the drape shader, not here
        SDL_GPUTextureSamplerBinding sb[8] = {
            { nullptr, g.s_clamp }, { g.shadow_map, g.s_near },
            { t_tiles ? t_tiles : g.shadow_map, g.s_near },
            { t_coarse ? t_coarse : g.shadow_map, g.s_wrap },
            { t_fine ? t_fine : g.shadow_map, g.s_wrap },
            { t_roll ? t_roll : g.shadow_map, g.s_wrap },
            { t_map ? t_map : g.shadow_map, g.s_near },
            { t_drape ? t_drape : g.shadow_map, g.s_clamp } };
        if (!t_coarse || !t_fine || !t_roll) fsu.det[0] = fsu.det[2] = 0.0f;
        if (!t_map) fsu.detr[2] = 0.0f;
        if (!t_tiles) { fsu.misc[3] = 0.0f; fsu.det[0] = fsu.det[2] = 0.0f; fsu.detr[2] = 0.0f; }
        SDL_PushGPUFragmentUniformData(cmd, 0, &fsu, sizeof fsu);
        SDL_BindGPUGraphicsPipeline(pass, g.pipe_chunk);
        if (g.stencil)
            SDL_SetGPUStencilReference(pass, 1);
        for (const chunk_mesh& m : g.meshes)
        {
            if (!m.tex || !m.vbuf || !m.built)
                continue;
            SDL_GPUTexture* gt = gpu_of(m.tex);
            if (!gt)
                continue;
            sb[0] = { gt, g.s_clamp };
            SDL_BindGPUFragmentSamplers(pass, 0, sb, 8);
            vsu.off[0] = m.dx - c.ox; vsu.off[1] = 0.0f; vsu.off[2] = -c.oy; vsu.off[3] = 0.0f;
            SDL_PushGPUVertexUniformData(cmd, 0, &vsu, sizeof vsu);
            const SDL_GPUBufferBinding vb{ m.vbuf, 0 };
            SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);
            const SDL_GPUBufferBinding ib{ g.grid_ibuf, 0 };
            SDL_BindGPUIndexBuffer(pass, &ib, SDL_GPU_INDEXELEMENTSIZE_32BIT);
            SDL_DrawGPUIndexedPrimitives(pass, k_grid * k_grid * 6, 1, 0, 0, 0);
            n_tris += k_grid * k_grid * 2;
            if (m.bbuf && m.n_building_vertices)
            {
                const SDL_GPUBufferBinding bb{ m.bbuf, 0 };
                SDL_BindGPUVertexBuffers(pass, 0, &bb, 1);
                SDL_DrawGPUPrimitives(pass, m.n_building_vertices, 1, 0, 0);
                n_btris += m.n_building_vertices / 3;
            }
        }
        if (g.far_n_index && g.far_tex)
            if (SDL_GPUTexture* ft = gpu_of(g.far_tex))
            {
                SDL_BindGPUGraphicsPipeline(pass, g.pipe_back);
                if (g.stencil)
                    SDL_SetGPUStencilReference(pass, 1);
                sb[0] = { ft, g.s_far };
                SDL_BindGPUFragmentSamplers(pass, 0, sb, 8);
                fsu.shade[3] = 1.0f; // the backdrop: no cast shadows reach it
                SDL_PushGPUFragmentUniformData(cmd, 0, &fsu, sizeof fsu);
                vsu.off[0] = -c.ox; vsu.off[1] = 0.0f; vsu.off[2] = -c.oy; vsu.off[3] = 0.0f;
                SDL_PushGPUVertexUniformData(cmd, 0, &vsu, sizeof vsu);
                const SDL_GPUBufferBinding vb{ g.far_vbuf, 0 };
                SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);
                const SDL_GPUBufferBinding ib{ g.far_ibuf, 0 };
                SDL_BindGPUIndexBuffer(pass, &ib, SDL_GPU_INDEXELEMENTSIZE_32BIT);
                SDL_DrawGPUIndexedPrimitives(pass, g.far_n_index, 1, 0, 0, 0);
                n_tris += g.far_n_index / 3;
            }
        SDL_EndGPURenderPass(pass);
    }
    SDL_SubmitGPUCommandBuffer(cmd);
    g.n_triangles = n_tris;
    g.n_building_tris = n_btris;
    g.submit_ms = static_cast<double>(SDL_GetPerformanceCounter() - t0) * 1000.0
                / static_cast<double>(SDL_GetPerformanceFrequency());
    g.rendered = true;
}

void draw_panel()
{
    ImGui::SetNextWindowSize({ 460.0f, 400.0f }, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("3D ground"))
    {
        ImGui::End();
        return;
    }
    if (!g.ok)
    {
        ImGui::TextWrapped("The 3D ground cannot run: %s.", g.why.empty() ? "not started" : g.why.c_str());
        ImGui::End();
        return;
    }
    ImGui::SliderFloat("Tilt at the closest zoom", &g.s.tilt_deg, 0.0f, 75.0f, "%.0f");
    ImGui::SliderFloat("Field of view", &g.s.fov_deg, 15.0f, 70.0f, "%.0f");
    ImGui::SliderFloat("Height", &g.s.height, 0.0f, 16.0f, "%.1f");
    ImGui::SliderFloat("Bumps", &g.s.bumps, 0.0f, 8.0f, "%.1f");
    ImGui::SliderFloat("Rolling", &g.s.rolling, 0.0f, 4.0f, "%.1f");
    ImGui::SliderFloat("Detail relief", &g.s.bump_light, 0.0f, 4.0f, "%.1f");
    ImGui::SliderFloat("Building height", &g.s.buildings, 0.2f, 3.0f, "%.1f");
    ImGui::SliderFloat("Trees", &g.s.trees, 0.0f, 3.0f, "%.2f");
    ImGui::SliderFloat("Sun height (deg)", &g.s.sun_deg, 3.0f, 85.0f, "%.0f");
    ImGui::SliderFloat("Sun turn (deg)", &g.s.sun_turn, -180.0f, 180.0f, "%.0f");
    ImGui::SliderFloat("Shadow", &g.s.shadow, 0.0f, 0.9f, "%.2f");
    ImGui::TextDisabled("The bake's own shading keeps its light at 40 deg from the north-west.");
    ImGui::Text("%d meshes (%d pending, %d built this frame)  ·  %zu ground + %zu building triangles",
                static_cast<int>(g.meshes.size()), g.pending, g.built_this_frame, g.n_triangles, g.n_building_tris);
    ImGui::Text("mesh work %.1f ms  ·  record + submit %.2f ms (CPU)  ·  stencil %s",
                g.build_ms, g.submit_ms, g.stencil ? "on" : "off");
    static settings saved_s = g.s;
    if (std::memcmp(&saved_s, &g.s, sizeof(settings)) != 0)
    {
        saved_s = g.s;
        save_settings();
    }
    ImGui::End();
}

void shutdown()
{
    if (g.device)
    {
        if (g.shown)    SDL_DestroyTexture(g.shown);
        if (g.coord_shown) SDL_DestroyTexture(g.coord_shown);
        if (g.coord)    SDL_ReleaseGPUTexture(g.device, g.coord);
        if (g.drape)    SDL_DestroyTexture(g.drape);
        if (g.pipe_chunk) SDL_ReleaseGPUGraphicsPipeline(g.device, g.pipe_chunk);
        if (g.pipe_back)  SDL_ReleaseGPUGraphicsPipeline(g.device, g.pipe_back);
        if (g.pipe_shadow) SDL_ReleaseGPUGraphicsPipeline(g.device, g.pipe_shadow);
        for (SDL_GPUSampler* s : { g.s_clamp, g.s_near, g.s_wrap, g.s_far })
            if (s) SDL_ReleaseGPUSampler(g.device, s);
        if (g.shadow_map) SDL_ReleaseGPUTexture(g.device, g.shadow_map);
        if (g.colour)   SDL_ReleaseGPUTexture(g.device, g.colour);
        if (g.depth)    SDL_ReleaseGPUTexture(g.device, g.depth);
        if (g.grid_ibuf) SDL_ReleaseGPUBuffer(g.device, g.grid_ibuf);
        if (g.far_vbuf) SDL_ReleaseGPUBuffer(g.device, g.far_vbuf);
        if (g.far_ibuf) SDL_ReleaseGPUBuffer(g.device, g.far_ibuf);
        for (chunk_mesh& m : g.meshes)
        {
            if (m.vbuf) SDL_ReleaseGPUBuffer(g.device, m.vbuf);
            if (m.bbuf) SDL_ReleaseGPUBuffer(g.device, m.bbuf);
        }
    }
    g = state{};
}

} // namespace gfx::slice
