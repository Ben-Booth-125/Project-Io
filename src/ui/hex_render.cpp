#include "hex_render.hpp"

#include "icons.hpp" // landform glyphs (BL-231)
#include "world/logistics.hpp" // body_tile_grid — O(1) neighbour lookup (BL-077 raster)
#include "world/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_set>

namespace ui {

namespace {
constexpr float kSqrt3 = 1.7320508f;
constexpr float kPi    = 3.14159265f;

ImU32 blend(ImU32 a, ImU32 b, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    const float ar = static_cast<float>((a >> IM_COL32_R_SHIFT) & 0xFF);
    const float ag = static_cast<float>((a >> IM_COL32_G_SHIFT) & 0xFF);
    const float ab = static_cast<float>((a >> IM_COL32_B_SHIFT) & 0xFF);
    const float aa = static_cast<float>((a >> IM_COL32_A_SHIFT) & 0xFF);
    const float br = static_cast<float>((b >> IM_COL32_R_SHIFT) & 0xFF);
    const float bg = static_cast<float>((b >> IM_COL32_G_SHIFT) & 0xFF);
    const float bb = static_cast<float>((b >> IM_COL32_B_SHIFT) & 0xFF);
    return IM_COL32(static_cast<int>(ar + (br - ar) * t),
                    static_cast<int>(ag + (bg - ag) * t),
                    static_cast<int>(ab + (bb - ab) * t),
                    static_cast<int>(aa));
}

// Signed elevation, plains = 0. Ordered as topography, not as the enum: mountain
// stands highest, then highland; valley, crater, rift and canyon sink in turn.
// Magnitudes stay small on purpose — see landform_relief's contract.
float relief_amount(terrain_landform lf)
{
    switch (lf)
    {
        case terrain_landform::mountain: return  0.22f;
        case terrain_landform::highland: return  0.12f;
        case terrain_landform::plains:   return  0.00f;
        case terrain_landform::crater:   return -0.08f;
        case terrain_landform::valley:   return -0.10f;
        case terrain_landform::rift:     return -0.14f;
        case terrain_landform::canyon:   return -0.16f;
    }
    return 0.0f;
}
} // namespace

ImU32 landform_relief(ImU32 base, terrain_landform lf)
{
    const float a = relief_amount(lf);
    if (a == 0.0f)
        return base; // plains is the untouched baseline

    // Warm sun above, cool shadow below — the convention that makes a shaded map
    // read as topography rather than as two arbitrary tints.
    constexpr ImU32 highlight = IM_COL32(255, 248, 225, 255);
    constexpr ImU32 shadow    = IM_COL32( 18,  24,  40, 255);
    return blend(base, a > 0.0f ? highlight : shadow, a > 0.0f ? a : -a);
}

ImU32 contrast_ink(ImU32 bg)
{
    const float r = static_cast<float>((bg >> IM_COL32_R_SHIFT) & 0xFF);
    const float g = static_cast<float>((bg >> IM_COL32_G_SHIFT) & 0xFF);
    const float b = static_cast<float>((bg >> IM_COL32_B_SHIFT) & 0xFF);
    const float luma = 0.2126f * r + 0.7152f * g + 0.0722f * b; // Rec. 709
    return luma > 140.0f ? IM_COL32(24, 26, 32, 225)
                         : IM_COL32(238, 240, 246, 225);
}

ImU32 substrate_colour(terrain_substrate sub)
{
    switch (sub)
    {
        case terrain_substrate::barren:   return IM_COL32(170, 145, 100, 255);
        case terrain_substrate::rocky:    return IM_COL32(112, 105,  95, 255);
        case terrain_substrate::volcanic: return IM_COL32(135,  55,  28, 255);
        case terrain_substrate::icy:      return IM_COL32(200, 224, 236, 255);
        case terrain_substrate::ocean:    return IM_COL32( 40,  80, 160, 255);
        case terrain_substrate::regolith: return IM_COL32(138, 130, 120, 255);
        case terrain_substrate::metallic: return IM_COL32(158, 150, 140, 255);
        // BARE SOIL — a tan that no pre-split composition had, because the old
        // model could not draw sedimentary ground without something growing on
        // it. It is the base every biotic cover below is blended INTO, and the
        // four canonical blends are calibrated against exactly this value.
        case terrain_substrate::sedimentary: return IM_COL32(150, 138, 110, 255);
    }
    return IM_COL32( 60,  60,  60, 255);
}

namespace {

/// The colour a cover tends toward at FULL density. Deliberately more saturated
/// than anything that reaches the screen: the drawn colour is the substrate
/// blended this far by the tile's density, so these are endpoints, not samples.
///
/// CALIBRATED (BL-519). The four biotic endpoints are solved so that each old
/// composition, at the density decompose_biome grades it to, reproduces its
/// pre-split colour EXACTLY on sedimentary ground:
///
///   grassland = sedimentary + grass  @150 -> (96,150,72)   the old grassland
///   forest    = sedimentary + forest @205 -> (48,102,56)   the old forest
///   wetland   = sedimentary + marsh  @170 -> (78,120,92)   the old wetland
///   tundra    = sedimentary + scrub  @ 75 -> (140,140,118) the old tundra
///
/// So the map a player already knows is pixel-identical, and what is NEW is that
/// a forested crag now reads as rock tinted green rather than as flat woodland.
/// Blend that ROUNDS. `blend` above truncates, and the four calibrated cover
/// endpoints land a channel short of their pre-split colour under truncation
/// (grassland came out (95,149,71) instead of (96,150,72)). Rounding here rather
/// than fixing `blend` keeps every OTHER colour path in this file bit-identical,
/// which matters because the visual goldens compare pixels.
ImU32 blend_round(ImU32 a, ImU32 b, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    auto ch = [&](int shift)
    {
        const float av = static_cast<float>((a >> shift) & 0xFF);
        const float bv = static_cast<float>((b >> shift) & 0xFF);
        return static_cast<int>(std::lround(av + (bv - av) * t));
    };
    return IM_COL32(ch(IM_COL32_R_SHIFT), ch(IM_COL32_G_SHIFT), ch(IM_COL32_B_SHIFT),
                    (a >> IM_COL32_A_SHIFT) & 0xFF);
}

ImU32 cover_endpoint(terrain_cover c)
{
    switch (c)
    {
        case terrain_cover::grass:  return IM_COL32( 58, 158,  45, 255);
        case terrain_cover::forest: return IM_COL32( 23,  93,  43, 255);
        case terrain_cover::marsh:  return IM_COL32( 42, 111,  83, 255);
        case terrain_cover::scrub:  return IM_COL32(116, 145, 137, 255);
        case terrain_cover::snow:   return IM_COL32(240, 246, 252, 255);
        case terrain_cover::dunes:  return IM_COL32(214, 190, 138, 255);
        case terrain_cover::ash:    return IM_COL32( 78,  72,  70, 255);
        case terrain_cover::salt:   return IM_COL32(232, 228, 216, 255);
        case terrain_cover::urban:  return IM_COL32(120, 118, 128, 255); // BL-366: built-over grey.
        case terrain_cover::none:   break;
    }
    return IM_COL32(0, 0, 0, 0);
}

} // namespace

ImU32 terrain_colour(terrain_substrate sub, terrain_cover cov, std::uint8_t density)
{
    const ImU32 base = substrate_colour(sub);
    if (cov == terrain_cover::none || density == 0)
        return base;
    // Urban is opaque, not a tint: a paved tile is not "grey-ish ground", and
    // letting the geology show through it would misread as a lens artefact.
    if (cov == terrain_cover::urban)
        return cover_endpoint(terrain_cover::urban);
    return blend_round(base, cover_endpoint(cov), cover_fraction(density));
}

// ---------------------------------------------------------------------------
// Terrain texture (BL-520) - substrate grain + cover pattern
// ---------------------------------------------------------------------------
namespace {

/// Deterministic 32-bit hash of a tile's GRID coordinate plus a salt.
///
/// Grid coordinates, never screen position: the cylinder draws several wrap
/// copies of the same tile and the canvas pans continuously, so a screen-space
/// hash would make the ground crawl under the cursor and would disagree between
/// two copies of one tile. FNV-1a with the usual spatial odd-prime mix.
std::uint32_t tex_hash(int x, int y, std::uint32_t salt)
{
    std::uint32_t h = 2166136261u;
    auto mix = [&h](std::uint32_t v) { h ^= v; h *= 16777619u; h ^= h >> 13; };
    mix(static_cast<std::uint32_t>(x) * 73856093u);
    mix(static_cast<std::uint32_t>(y) * 19349663u);
    mix(salt * 83492791u);
    return h ^ (h >> 16);
}

/// Hash bits as a float in [0, 1).
float hash_unit(std::uint32_t h)
{
    return static_cast<float>(h & 0x00FFFFFFu) / 16777216.0f;
}

/// A mark position inside the hex. Polar, with a sqrt-distributed radius so marks
/// spread evenly over the AREA rather than clumping at the centre. @p spread is a
/// fraction of the circumradius and stays under the inradius (0.866), so no mark
/// crosses the tile's own edge - a cover pattern that bled over the seam would
/// undo the per-tile read the pattern exists to give.
ImVec2 mark_pos(ImVec2 c, float r, int gx, int gy, std::uint32_t salt, float spread)
{
    const float ang = hash_unit(tex_hash(gx, gy, salt * 2u + 1u)) * 2.0f * kPi;
    const float rad = spread * r * std::sqrt(hash_unit(tex_hash(gx, gy, salt * 2u + 2u)));
    return { c.x + rad * std::cos(ang), c.y + rad * std::sin(ang) };
}

ImU32 with_alpha(ImU32 c, float a)
{
    const int ai = std::clamp(static_cast<int>(std::lround(a * 255.0f)), 0, 255);
    return (c & ~IM_COL32_A_MASK) | (static_cast<ImU32>(ai) << IM_COL32_A_SHIFT);
}

/// Ink for one texture mark: the tile's own drawn fill pushed 55% toward
/// @p target, at alpha @p a.
///
/// Deriving from the FILL rather than from a fixed palette colour is the whole
/// lens answer. Under the Country lens the tile is a nation colour, and a mark
/// that is that nation colour darkened reads as shading ON the nation block; a
/// fixed forest-green mark over it would read as dirt. It also inherits the fog
/// wash and the survey dim for free - as the ground darkens so does its grain,
/// and the texture recedes with it instead of floating above it.
ImU32 texture_ink(ImU32 fill, ImU32 target, float a)
{
    return with_alpha(blend(fill, target, 0.55f), a);
}

constexpr ImU32 k_tex_dark = IM_COL32( 16,  20,  26, 255);
constexpr ImU32 k_tex_pale = IM_COL32(248, 250, 255, 255);
constexpr ImU32 k_tex_leaf = IM_COL32( 12,  26,  14, 255); // biotic marks read green-black
constexpr ImU32 k_tex_sand = IM_COL32( 96,  74,  40, 255);

enum class grain_kind : std::uint8_t { none, dot, bed, chip };

struct grain_spec
{
    grain_kind kind;
    int        count;
    ImU32      target;
    float      alpha;
};

/// The ground's own material, drawn FAINT and SMALL on purpose.
///
/// BL-511's province blend averages a province's fills across its member tiles;
/// the substrate is the axis that blend already smooths, so its texture must not
/// assert a tile boundary. Low alpha and small marks give material character
/// without drawing a seam - which is exactly the split BL-520 turns on (texture
/// the substrate, pattern the cover).
///
/// Ocean draws NOTHING. A flat sea is a correct reading of open water, and any
/// mark on it would be read as the animated water this item explicitly excludes.
grain_spec substrate_grain(terrain_substrate s)
{
    switch (s)
    {
        case terrain_substrate::barren:      return { grain_kind::dot,  4, k_tex_dark, 0.16f };
        case terrain_substrate::rocky:       return { grain_kind::chip, 3, k_tex_dark, 0.26f };
        case terrain_substrate::sedimentary: return { grain_kind::bed,  3, k_tex_dark, 0.14f };
        case terrain_substrate::volcanic:    return { grain_kind::chip, 3, k_tex_dark, 0.28f };
        case terrain_substrate::metallic:    return { grain_kind::chip, 3, k_tex_pale, 0.26f };
        case terrain_substrate::regolith:    return { grain_kind::dot,  5, k_tex_dark, 0.15f };
        case terrain_substrate::icy:         return { grain_kind::bed,  2, k_tex_pale, 0.30f };
        case terrain_substrate::ocean:       return { grain_kind::none, 0, k_tex_dark, 0.00f };
    }
    return { grain_kind::none, 0, k_tex_dark, 0.00f };
}

/// The ink a cover's marks tend toward. Biotic covers read as green-black; snow
/// and salt as pale; dunes as a warm sand shadow; ash and urban as dark.
ImU32 cover_ink_target(terrain_cover c)
{
    switch (c)
    {
        case terrain_cover::grass:
        case terrain_cover::scrub:
        case terrain_cover::forest:
        case terrain_cover::marsh:  return k_tex_leaf;
        case terrain_cover::snow:
        case terrain_cover::salt:   return k_tex_pale;
        case terrain_cover::dunes:  return k_tex_sand;
        case terrain_cover::ash:
        case terrain_cover::urban:  return k_tex_dark;
        case terrain_cover::none:   break;
    }
    return k_tex_dark;
}

void draw_substrate_grain(ImDrawList* dl, ImVec2 c, float r, int gx, int gy,
                          terrain_substrate sub, ImU32 fill, float strength)
{
    const grain_spec g = substrate_grain(sub);
    if (g.kind == grain_kind::none || g.count <= 0)
        return;
    const float a = g.alpha * strength;
    if (a <= 0.012f) // below ~3/255 the mark is not a colour, it is rounding
        return;

    const ImU32 ink = texture_ink(fill, g.target, a);
    const float th  = std::max(1.0f, r * 0.045f);
    for (int i = 0; i < g.count; ++i)
    {
        const std::uint32_t salt = 101u + static_cast<std::uint32_t>(i);
        const ImVec2        p    = mark_pos(c, r, gx, gy, salt, 0.60f);
        switch (g.kind)
        {
            case grain_kind::dot:
                dl->AddCircleFilled(p, std::max(0.6f, r * 0.045f), ink, 4);
                break;
            case grain_kind::bed: // bedding / stratification: a short level stroke
            {
                const float w = r * 0.26f;
                dl->AddLine({ p.x - w, p.y }, { p.x + w, p.y }, ink, th);
                break;
            }
            case grain_kind::chip: // fracture: a short stroke at a hashed angle
            {
                const float ang = hash_unit(tex_hash(gx, gy, salt + 811u)) * kPi;
                const float w   = r * 0.20f;
                const ImVec2 d  = { w * std::cos(ang), w * std::sin(ang) };
                dl->AddLine({ p.x - d.x, p.y - d.y }, { p.x + d.x, p.y + d.y }, ink, th);
                break;
            }
            case grain_kind::none:
                break;
        }
    }
}

/// The cover pattern - per tile, and deliberately more emphatic than the grain.
///
/// SCALED BY `cover_density`, which is that field's whole reason for existing
/// (components.hpp: "one number, two consumers"). Both channels move with it:
///
///     f     = cover_fraction(density)
///     marks = clamp(1 + round(f * 4), 1, 5)
///     alpha = (0.35 + 0.45 * f) * strength
///
/// So at BL-519's four calibration densities a sparse cover both LOOKS thin and,
/// through the same scalar, CUTS thin in the economy:
///
///     scrub  @ 75 -> f 0.294 -> 2 marks, alpha 0.482 (123/255)
///     grass  @150 -> f 0.588 -> 3 marks, alpha 0.615 (157/255)
///     forest @205 -> f 0.804 -> 4 marks, alpha 0.712 (182/255)
///     full   @255 -> f 1.000 -> 5 marks, alpha 0.800 (204/255)
void draw_cover_pattern(ImDrawList* dl, ImVec2 c, float r, int gx, int gy,
                        terrain_cover cov, std::uint8_t density, ImU32 fill, float strength)
{
    // BL-519's invariant (density == 0 iff cover == none) means either test
    // suffices; both are written so the pass stays a no-op if it ever slips.
    if (cov == terrain_cover::none || density == 0)
        return;

    const float f = cover_fraction(density);
    const int   n = std::clamp(1 + static_cast<int>(std::lround(f * 4.0f)), 1, 5);
    const float a = (0.35f + 0.45f * f) * strength;
    if (a <= 0.012f)
        return;

    const ImU32 ink = texture_ink(fill, cover_ink_target(cov), a);
    const float m   = r * 0.20f;                 // mark half-extent
    const float th  = std::max(1.0f, r * 0.055f);

    for (int i = 0; i < n; ++i)
    {
        const ImVec2 p = mark_pos(c, r, gx, gy, 7u + static_cast<std::uint32_t>(i), 0.56f);
        switch (cov)
        {
            case terrain_cover::forest: // a canopy tick - the densest silhouette
                dl->AddTriangleFilled({ p.x, p.y - m },
                                      { p.x - m * 0.62f, p.y + m * 0.55f },
                                      { p.x + m * 0.62f, p.y + m * 0.55f }, ink);
                break;
            case terrain_cover::scrub: // the same silhouette at ~60%, so a forest
            {                          // line grades instead of snapping to an edge
                const float s = m * 0.62f;
                dl->AddTriangleFilled({ p.x, p.y - s },
                                      { p.x - s * 0.70f, p.y + s * 0.60f },
                                      { p.x + s * 0.70f, p.y + s * 0.60f }, ink);
                break;
            }
            case terrain_cover::grass: // a three-blade tuft
            {
                const float s = m * 0.85f;
                dl->AddLine({ p.x, p.y + s * 0.5f }, { p.x, p.y - s * 0.5f }, ink, th);
                dl->AddLine({ p.x - s * 0.45f, p.y + s * 0.5f },
                            { p.x - s * 0.15f, p.y - s * 0.4f }, ink, th);
                dl->AddLine({ p.x + s * 0.45f, p.y + s * 0.5f },
                            { p.x + s * 0.15f, p.y - s * 0.4f }, ink, th);
                break;
            }
            case terrain_cover::marsh: // stacked level dashes - standing water
                dl->AddLine({ p.x - m, p.y }, { p.x + m, p.y }, ink, th);
                dl->AddLine({ p.x - m * 0.60f, p.y + m * 0.55f },
                            { p.x + m * 0.60f, p.y + m * 0.55f }, ink, th);
                break;
            case terrain_cover::snow:
            case terrain_cover::ash: // both are FALL: a stipple, not a growth form
                dl->AddCircleFilled(p, std::max(0.7f, m * 0.35f), ink, 5);
                break;
            case terrain_cover::salt: // dry-basin crust: a crossed tick
                dl->AddLine({ p.x - m * 0.7f, p.y - m * 0.7f },
                            { p.x + m * 0.7f, p.y + m * 0.7f }, ink, th);
                dl->AddLine({ p.x - m * 0.7f, p.y + m * 0.7f },
                            { p.x + m * 0.7f, p.y - m * 0.7f }, ink, th);
                break;
            case terrain_cover::dunes: // a windward crest
            {
                ImVec2 arc[3] = { { p.x - m, p.y + m * 0.35f },
                                  { p.x,     p.y - m * 0.45f },
                                  { p.x + m, p.y + m * 0.35f } };
                dl->AddPolyline(arc, 3, ink, ImDrawFlags_None, th);
                break;
            }
            case terrain_cover::urban: // a block plan: built form, not growth
                dl->AddRectFilled({ p.x - m * 0.70f, p.y - m * 0.70f },
                                  { p.x + m * 0.70f, p.y + m * 0.70f }, ink);
                break;
            case terrain_cover::none:
                break;
        }
    }
}

} // namespace

float texture_lod_scale(float draw_radius_px)
{
    constexpr float k_texture_min_radius_px  = 14.0f;
    constexpr float k_texture_full_radius_px = 22.0f;
    if (draw_radius_px <= k_texture_min_radius_px)
        return 0.0f;
    return std::clamp((draw_radius_px - k_texture_min_radius_px)
                          / (k_texture_full_radius_px - k_texture_min_radius_px),
                      0.0f, 1.0f);
}

void draw_tile_texture(ImDrawList* dl, ImVec2 centre, float r, int grid_x, int grid_y,
                       terrain_substrate sub, terrain_cover cov,
                       std::uint8_t density, ImU32 fill, float strength)
{
    if (dl == nullptr || strength <= 0.0f || r <= 0.0f)
        return;
    // Grain first, pattern over it: the cover sits ON the ground, and drawing it
    // that way round matches the compositing order the colour model already uses
    // (terrain_colour blends the substrate TOWARD the cover, not the reverse).
    draw_substrate_grain(dl, centre, r, grid_x, grid_y, sub, fill, strength);
    draw_cover_pattern(dl, centre, r, grid_x, grid_y, cov, density, fill, strength);
}

void hex_vertices(ImVec2 out[6], float cx, float cy, float r)
{
    for (int i = 0; i < 6; ++i)
    {
        const float angle = kPi / 6.0f + kPi / 3.0f * static_cast<float>(i);
        out[i] = { cx + r * std::cos(angle), cy + r * std::sin(angle) };
    }
}

ImVec2 hex_local_centre(int col, int row, float hex_size)
{
    const float col_step = kSqrt3 * hex_size;
    const float row_step = 1.5f * hex_size;
    return {
        col_step * static_cast<float>(col) + ((row & 1) ? col_step * 0.5f : 0.0f),
        row_step * static_cast<float>(row),
    };
}

void draw_tile_neighbourhood(ImDrawList* dl, world& w, entity_id centre_tile,
                             ImVec2 origin, ImVec2 size, int radius)
{
    if (size.x <= 4.0f || size.y <= 4.0f || radius < 0)
        return;

    const auto cit = w.tiles.find(centre_tile);
    if (cit == w.tiles.end())
        return;
    const int       cx0  = cit->second.grid_x;
    const int       cy0  = cit->second.grid_y;
    const entity_id body = cit->second.body;

    const auto bit = w.bodies.find(body);
    if (bit == w.bodies.end())
        return;
    const int gw = bit->second.grid_width;
    const int gh = bit->second.grid_height;
    if (gw <= 0 || gh <= 0)
        return;

    // Raster index (grid_y*gw + grid_x -> tile id), lazily built once per body.
    const std::vector<entity_id>& grid = body_tile_grid(w, body);
    if (grid.size() < static_cast<std::size_t>(gw) * static_cast<std::size_t>(gh))
        return; // index not populated (e.g. no tiles) — nothing to draw

    // Scale so a (2·radius+1) block fills the box, with a little margin.
    const float span   = static_cast<float>(2 * radius + 1) + 0.75f;
    const float hex_sz = std::max(4.0f, std::min(size.x / (span * kSqrt3),
                                                 size.y / (span * 1.5f)));

    // Centre the clicked tile in the box.
    const ImVec2 centre_local = hex_local_centre(cx0, cy0, hex_sz);
    const ImVec2 region_ctr   = { origin.x + size.x * 0.5f, origin.y + size.y * 0.5f };
    const ImVec2 off          = { region_ctr.x - centre_local.x,
                                  region_ctr.y - centre_local.y };

    // Tiles that carry a building (rendered as installations, not ground).
    std::unordered_set<entity_id> built;
    built.reserve(w.buildings.size());
    for (const auto& [bid, bc] : w.buildings)
        built.insert(bc.tile);

    dl->PushClipRect(origin, {origin.x + size.x, origin.y + size.y}, true);

    constexpr ImU32 outline_col = IM_COL32(24, 26, 32, 200);
    constexpr ImU32 built_plate = IM_COL32(50, 52, 60, 255);
    constexpr ImU32 built_mark  = IM_COL32(150, 160, 190, 255);
    constexpr ImU32 hl_col      = IM_COL32(250, 235, 140, 255); // centre-tile highlight ring

    ImVec2 hl_verts[6];
    bool   have_hl = false;

    for (int dr = -radius; dr <= radius; ++dr)
    {
        const int row = cy0 + dr;
        if (row < 0 || row >= gh)
            continue;
        for (int dc = -radius; dc <= radius; ++dc)
        {
            const int col = cx0 + dc;
            // Wrap the column for the LOOKUP (the grid wraps), but position with the
            // unwrapped col so the block stays contiguous around the centre.
            const int wc  = ((col % gw) + gw) % gw;
            const entity_id tid =
                grid[static_cast<std::size_t>(row) * static_cast<std::size_t>(gw)
                     + static_cast<std::size_t>(wc)];
            if (tid == null_entity)
                continue;

            const ImVec2 lc = { hex_local_centre(col, row, hex_sz).x + off.x,
                                hex_local_centre(col, row, hex_sz).y + off.y };
            ImVec2 verts[6];
            hex_vertices(verts, lc.x, lc.y, hex_sz * 0.94f);

            const auto tt        = w.tiles.find(tid);
            const bool have_tile = (tt != w.tiles.end());
            const bool is_built  = built.count(tid) != 0;
            ImU32 fill = built_plate;
            if (!is_built)
            {
                fill = have_tile ? terrain_colour(tt->second.substrate, tt->second.cover,
                                                 tt->second.cover_density)
                                 : IM_COL32(60, 60, 60, 255);
                // Same landform channel the Planetary canvas draws (BL-231) — this
                // view exists so the two surfaces cannot drift apart.
                if (have_tile)
                    fill = landform_relief(fill, tt->second.landform);
            }
            dl->AddConvexPolyFilled(verts, 6, fill);
            // Terrain texture (BL-520). Drawn here as well as on the Planetary
            // canvas for the reason this whole file exists: the two surfaces must
            // not drift. No lens attenuation applies — this view has no lens.
            if (!is_built && have_tile)
            {
                const float tr = hex_sz * 0.94f;
                draw_tile_texture(dl, lc, tr, tt->second.grid_x, tt->second.grid_y,
                                  tt->second.substrate, tt->second.cover,
                                  tt->second.cover_density, fill, texture_lod_scale(tr));
            }
            dl->AddPolyline(verts, 6, outline_col, ImDrawFlags_Closed, 1.0f);
            if (is_built)
            {
                const float m = hex_sz * 0.30f;
                dl->AddRectFilled({lc.x - m, lc.y - m}, {lc.x + m, lc.y + m}, built_mark, 1.0f);
            }
            else if (have_tile)
            {
                icons::landform(dl, lc, hex_sz * 0.42f, tt->second.landform,
                                contrast_ink(fill));
            }
            if (tid == centre_tile)
            {
                std::copy(std::begin(verts), std::end(verts), std::begin(hl_verts));
                have_hl = true;
            }
        }
    }

    // Highlight ring last, so it sits above neighbouring fills.
    if (have_hl)
        dl->AddPolyline(hl_verts, 6, hl_col, ImDrawFlags_Closed, 2.5f);

    dl->PopClipRect();
}

} // namespace ui
