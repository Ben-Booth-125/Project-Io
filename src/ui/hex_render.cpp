#include "hex_render.hpp"

#include "icons.hpp" // landform glyphs (BL-231)
#include "world/logistics.hpp" // body_tile_grid — O(1) neighbour lookup (BL-077 raster)
#include "world/world.hpp"

#include <algorithm>
#include <cmath>
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

ImU32 terrain_colour(terrain_composition t)
{
    switch (t)
    {
        case terrain_composition::barren:    return IM_COL32(170, 145, 100, 255);
        case terrain_composition::rocky:     return IM_COL32(112, 105,  95, 255);
        case terrain_composition::volcanic:  return IM_COL32(135,  55,  28, 255);
        case terrain_composition::icy:       return IM_COL32(200, 224, 236, 255);
        case terrain_composition::tundra:    return IM_COL32(140, 140, 118, 255);
        case terrain_composition::grassland: return IM_COL32( 96, 150,  72, 255);
        case terrain_composition::forest:    return IM_COL32( 48, 102,  56, 255);
        case terrain_composition::wetland:   return IM_COL32( 78, 120,  92, 255);
        case terrain_composition::ocean:     return IM_COL32( 40,  80, 160, 255);
        case terrain_composition::regolith:  return IM_COL32(138, 130, 120, 255);
        case terrain_composition::metallic:  return IM_COL32(158, 150, 140, 255);
    }
    return IM_COL32( 60,  60,  60, 255);
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
                fill = have_tile ? terrain_colour(tt->second.composition)
                                 : IM_COL32(60, 60, 60, 255);
                // Same landform channel the Planetary canvas draws (BL-231) — this
                // view exists so the two surfaces cannot drift apart.
                if (have_tile)
                    fill = landform_relief(fill, tt->second.landform);
            }
            dl->AddConvexPolyFilled(verts, 6, fill);
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
