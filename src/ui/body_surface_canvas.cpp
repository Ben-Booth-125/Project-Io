#define IMGUI_DEFINE_MATH_OPERATORS
#include "body_surface_canvas.hpp"

#include "entity_summary.hpp"
#include "highlight.hpp"
#include "icons.hpp"
#include "nav_pane.hpp"
#include "presentation.hpp"
#include "world/market_clearing.hpp" // market_for_tile (Scarcity catchment, prices)
#include "world/placement_rules.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <limits>
#include <unordered_map>
#include <vector>

namespace ui {

namespace {

constexpr float kSqrt3 = 1.7320508f;
constexpr float kPi    = 3.14159265f;

// Zoom is normalised to grid height: at zoom = 1 the grid fills kFitMargin of the
// canvas height. The zoom floor must therefore be derived from these constants,
// not guessed — at the minimum the viewport spans kMinZoomHeadroom of the grid
// height (the full grid plus ~20% headroom), i.e. zoom = 1 / (headroom * margin).
constexpr float kFitMargin       = 0.95f; // grid fills 95% of the canvas height at zoom = 1
constexpr float kMinZoomHeadroom = 1.2f;  // at min zoom the viewport shows ~120% of the grid height
constexpr float kMaxZoom         = 20.0f;
constexpr float kMinZoom         = 1.0f / (kMinZoomHeadroom * kFitMargin); // ~0.877

// Identity fill colour for a tile's composition. The single source of truth for
// surface tinting; landform is conveyed by overlay glyphs (deferred), not hue.
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

/// Fills `out[6]` with the screen-space vertices of a pointy-top hexagon
/// centred at (cx, cy) with circumradius r.
void hex_vertices(ImVec2 out[6], float cx, float cy, float r)
{
    for (int i = 0; i < 6; ++i)
    {
        const float angle = kPi / 6.0f + kPi / 3.0f * static_cast<float>(i);
        out[i] = { cx + r * std::cos(angle), cy + r * std::sin(angle) };
    }
}

/// World-space centre of a hex at (col, row) in odd-r offset coordinates,
/// relative to the grid top-left, using the given circumradius.
ImVec2 hex_local_centre(int col, int row, float hex_size)
{
    const float col_step = kSqrt3 * hex_size;
    const float row_step = 1.5f * hex_size;
    return {
        col_step * static_cast<float>(col) + ((row & 1) ? col_step * 0.5f : 0.0f),
        row_step * static_cast<float>(row),
    };
}

/// Per-channel linear blend of two opaque colours: result = a·(1−t) + b·t, alpha
/// forced opaque. Used by the Resource lens to composite a deposit hue over terrain
/// at a magnitude-driven opacity, and by the Market lens's diverging wash.
ImU32 lerp_colour(ImU32 a, ImU32 b, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    const float u = 1.0f - t;
    auto ch = [](ImU32 c, int shift) -> float { return static_cast<float>((c >> shift) & 0xFFu); };
    const int r = static_cast<int>(ch(a, IM_COL32_R_SHIFT) * u + ch(b, IM_COL32_R_SHIFT) * t);
    const int g = static_cast<int>(ch(a, IM_COL32_G_SHIFT) * u + ch(b, IM_COL32_G_SHIFT) * t);
    const int bl= static_cast<int>(ch(a, IM_COL32_B_SHIFT) * u + ch(b, IM_COL32_B_SHIFT) * t);
    return IM_COL32(r, g, bl, 255);
}

/// Diverging warm↔cool colour for a ratio relative to 1.0 (defined below); forward
/// declared so the Production key (above its definition) can sample the same band.
ImU32 diverging_colour(float ratio);

/// Shared chrome for an on-canvas lens key: a rounded dark panel of @p box_w ×
/// @p body_h at the left edge (inset past the nav rail), vertically centred —
/// clear of the Selection panel, the header/Explorer, and the lens control strip.
/// Returns the inner top-left and the inner content width via @p out_x/@p out_y/
/// @p out_w. Pure ImDrawList — no ImGui widget state.
void begin_lens_key(ImDrawList* dl, ImVec2 area_origin, ImVec2 area_size, float box_w,
                    float body_h, float pad, float& out_x, float& out_y, float& out_w)
{
    const ImVec2 p0 = { area_origin.x + nav_pane_width + pad,
                        area_origin.y + std::max(pad, (area_size.y - body_h) * 0.5f) };
    const ImVec2 p1 = { p0.x + box_w, p0.y + body_h };
    dl->AddRectFilled(p0, p1, IM_COL32(18, 18, 24, 210), 4.0f);
    dl->AddRect      (p0, p1, IM_COL32(80, 80, 90, 255), 4.0f);
    out_x = p0.x + pad;
    out_y = p0.y + pad * 0.5f;
    out_w = box_w - 2.0f * pad;
}

/// On-canvas legend for the Resource lens (BL-019): the selected resource's name
/// and identity swatch, plus a note that the fill marks the contiguous deposit.
/// Flat, not a gradient — the lens shows deposit *shape*, not magnitude.
void draw_resource_key(ImDrawList* dl, ImVec2 area_origin, ImVec2 area_size,
                       const ui_state& state)
{
    const float pad    = 8.0f;
    const float line_h = ImGui::GetTextLineHeight();
    const float body_h = pad + line_h + 4.0f + line_h + 4.0f + line_h + pad;
    float x, y, bar_w;
    begin_lens_key(dl, area_origin, area_size, 168.0f, body_h, pad, x, y, bar_w);

    dl->AddText({x, y}, IM_COL32(235, 235, 235, 255), "Resource deposit");
    y += line_h + 4.0f;
    dl->AddRectFilled({x, y + 2.0f}, {x + 10.0f, y + 12.0f},
                      presentation_of(state.lens_resource).colour);
    dl->AddText({x + 14.0f, y}, IM_COL32(235, 235, 235, 255),
                presentation_of(state.lens_resource).name);
    y += line_h + 4.0f;
    dl->AddText({x, y}, IM_COL32(170, 175, 185, 255), "filled = deposit present");
}

/// On-canvas legend for the Opportunity lens (BL-017): a diverging loss→profit
/// gradient bar over the best-building net-margin surface.
void draw_opportunity_key(ImDrawList* dl, ImVec2 area_origin, ImVec2 area_size)
{
    const float pad    = 8.0f;
    const float line_h = ImGui::GetTextLineHeight();
    const float bar_h  = 10.0f;
    const float body_h = pad + line_h + 4.0f + bar_h + 2.0f + line_h + pad;
    float x, y, bar_w;
    begin_lens_key(dl, area_origin, area_size, 168.0f, body_h, pad, x, y, bar_w);

    dl->AddText({x, y}, IM_COL32(235, 235, 235, 255), "Opportunity (margin)");
    y += line_h + 4.0f;
    constexpr ImU32 loss   = IM_COL32(216, 100,  96, 255);
    constexpr ImU32 profit = IM_COL32(110, 200, 120, 255);
    constexpr int segs = 24;
    for (int i = 0; i < segs; ++i)
    {
        const float t = static_cast<float>(i) / (segs - 1);
        const ImU32 c = t < 0.5f ? lerp_colour(loss, IM_COL32(40, 40, 48, 255), t * 2.0f)
                                 : lerp_colour(IM_COL32(40, 40, 48, 255), profit, (t - 0.5f) * 2.0f);
        dl->AddRectFilled({ x + bar_w * static_cast<float>(i) / segs, y },
                          { x + bar_w * static_cast<float>(i + 1) / segs, y + bar_h }, c);
    }
    y += bar_h + 2.0f;
    dl->AddText({x, y}, IM_COL32(170, 175, 185, 255), "loss");
    const ImVec2 ts = ImGui::CalcTextSize("profit");
    dl->AddText({x + bar_w - ts.x, y}, IM_COL32(170, 175, 185, 255), "profit");
}

/// On-canvas legend for the Production lens (BL-009): a diverging cool→warm bar
/// (below/above the body's mean output value) over the production-intensity surface.
void draw_production_key(ImDrawList* dl, ImVec2 area_origin, ImVec2 area_size)
{
    const float pad    = 8.0f;
    const float line_h = ImGui::GetTextLineHeight();
    const float bar_h  = 10.0f;
    const float body_h = pad + line_h + 4.0f + bar_h + 2.0f + line_h + pad;
    float x, y, bar_w;
    begin_lens_key(dl, area_origin, area_size, 168.0f, body_h, pad, x, y, bar_w);

    dl->AddText({x, y}, IM_COL32(235, 235, 235, 255), "Production intensity");
    y += line_h + 4.0f;
    constexpr int segs = 24;
    for (int i = 0; i < segs; ++i)
    {
        const float t = static_cast<float>(i) / (segs - 1);
        const ImU32 c = diverging_colour(std::pow(4.0f, t * 2.0f - 1.0f));
        dl->AddRectFilled({ x + bar_w * static_cast<float>(i) / segs, y },
                          { x + bar_w * static_cast<float>(i + 1) / segs, y + bar_h }, c);
    }
    y += bar_h + 2.0f;
    dl->AddText({x, y}, IM_COL32(170, 175, 185, 255), "low");
    const ImVec2 ts = ImGui::CalcTextSize("high");
    dl->AddText({x + bar_w - ts.x, y}, IM_COL32(170, 175, 185, 255), "high");
}

/// Diverging warm↔cool colour for a price relative to its base (floor) price.
/// `ratio = price / base_price`: 1.0 is the neutral mid-tone, < 1 (cheap) trends
/// cool, > 1 (dear) trends warm. Centred on the log of the ratio so the symmetric
/// price band `[0.25×, 4×]` (the market clamp) maps to the full [cool, warm] span.
ImU32 diverging_colour(float ratio)
{
    ratio = std::clamp(ratio, 0.25f, 4.0f);
    const float d = std::log(ratio) / std::log(4.0f); // [-1, 1]
    constexpr ImU32 neutral = IM_COL32(205, 205, 210, 255);
    constexpr ImU32 cool    = IM_COL32( 70, 140, 225, 255);
    constexpr ImU32 warm    = IM_COL32(232, 120,  60, 255);
    return d < 0.0f ? lerp_colour(neutral, cool, -d) : lerp_colour(neutral, warm, d);
}

/// On-canvas legend for the Market lens: a diverging cheap↔dear gradient bar plus
/// the selected good's name and its current price ratio (or an "untraded" note when
/// the body's market has no entry for it). Same left-edge placement as the Resource key.
void draw_market_key(ImDrawList* dl, ImVec2 area_origin, ImVec2 area_size,
                     const ui_state& state, bool active, float ratio)
{
    const float pad    = 8.0f;
    const float box_w  = 172.0f;
    const float line_h = ImGui::GetTextLineHeight();
    const float bar_h  = 10.0f;

    const float body_h = pad + line_h + 4.0f + bar_h + 2.0f + line_h + 4.0f + line_h + pad;
    const ImVec2 p0 = { area_origin.x + nav_pane_width + pad,
                        area_origin.y + std::max(pad, (area_size.y - body_h) * 0.5f) };
    const ImVec2 p1 = { p0.x + box_w, p0.y + body_h };
    dl->AddRectFilled(p0, p1, IM_COL32(18, 18, 24, 210), 4.0f);
    dl->AddRect      (p0, p1, IM_COL32(80, 80, 90, 255), 4.0f);

    const float x     = p0.x + pad;
    const float bar_w = box_w - 2.0f * pad;
    float       y     = p0.y + pad * 0.5f;

    dl->AddText({x, y}, IM_COL32(235, 235, 235, 255), "Market price");
    y += line_h + 4.0f;

    // Diverging bar: cheap (cool) at the left, dear (warm) at the right, sampling
    // the same band the wash uses (ratio 0.25 → 4).
    constexpr int segs = 24;
    for (int i = 0; i < segs; ++i)
    {
        const float t = static_cast<float>(i) / (segs - 1);  // 0..1
        const ImU32 c = diverging_colour(std::pow(4.0f, t * 2.0f - 1.0f));
        dl->AddRectFilled({ x + bar_w * static_cast<float>(i) / segs, y },
                          { x + bar_w * static_cast<float>(i + 1) / segs, y + bar_h }, c);
    }
    y += bar_h + 2.0f;
    dl->AddText({x, y}, IM_COL32(170, 175, 185, 255), "cheap");
    const ImVec2 dts = ImGui::CalcTextSize("dear");
    dl->AddText({x + bar_w - dts.x, y}, IM_COL32(170, 175, 185, 255), "dear");
    y += line_h + 4.0f;

    char buf[96];
    if (active)
        std::snprintf(buf, sizeof(buf), "%s  x%.2f",
                      presentation_of(state.lens_resource).name, static_cast<double>(ratio));
    else
        std::snprintf(buf, sizeof(buf), "%s  (untraded)",
                      presentation_of(state.lens_resource).name);
    dl->AddText({x, y}, IM_COL32(235, 235, 235, 255), buf);
}

/// On-canvas legend for the Population lens: a low→high habitability gradient bar
/// (dark substrate → liveable green). Same left-edge placement as the other keys.
void draw_population_key(ImDrawList* dl, ImVec2 area_origin, ImVec2 area_size)
{
    const float pad    = 8.0f;
    const float box_w  = 156.0f;
    const float line_h = ImGui::GetTextLineHeight();
    const float bar_h  = 10.0f;

    const float body_h = pad + line_h + 4.0f + bar_h + 2.0f + line_h + pad;
    const ImVec2 p0 = { area_origin.x + nav_pane_width + pad,
                        area_origin.y + std::max(pad, (area_size.y - body_h) * 0.5f) };
    const ImVec2 p1 = { p0.x + box_w, p0.y + body_h };
    dl->AddRectFilled(p0, p1, IM_COL32(18, 18, 24, 210), 4.0f);
    dl->AddRect      (p0, p1, IM_COL32(80, 80, 90, 255), 4.0f);

    const float x     = p0.x + pad;
    const float bar_w = box_w - 2.0f * pad;
    float       y     = p0.y + pad * 0.5f;

    dl->AddText({x, y}, IM_COL32(235, 235, 235, 255), "Habitability");
    y += line_h + 4.0f;

    constexpr ImU32 live = IM_COL32(80, 200, 110, 255);
    constexpr int segs = 24;
    for (int i = 0; i < segs; ++i)
    {
        const float t0 = static_cast<float>(i) / segs;
        const ImU32 c  = lerp_colour(IM_COL32(40, 40, 48, 255), live, 0.15f + 0.7f * t0);
        dl->AddRectFilled({ x + bar_w * t0, y },
                          { x + bar_w * static_cast<float>(i + 1) / segs, y + bar_h }, c);
    }
    y += bar_h + 2.0f;
    dl->AddText({x, y}, IM_COL32(170, 175, 185, 255), "low");
    const ImVec2 hts = ImGui::CalcTextSize("high");
    dl->AddText({x + bar_w - hts.x, y}, IM_COL32(170, 175, 185, 255), "high");
}

/// On-canvas legend for the Scarcity lens: an abundant→scarce gradient bar (no tint
/// → hot) plus the selected resource's name and swatch. Same placement as the others.
void draw_scarcity_key(ImDrawList* dl, ImVec2 area_origin, ImVec2 area_size,
                       const ui_state& state)
{
    const float pad    = 8.0f;
    const float box_w  = 156.0f;
    const float line_h = ImGui::GetTextLineHeight();
    const float bar_h  = 10.0f;

    const float body_h = pad + line_h + 4.0f + bar_h + 2.0f + line_h + 4.0f + line_h + pad;
    const ImVec2 p0 = { area_origin.x + nav_pane_width + pad,
                        area_origin.y + std::max(pad, (area_size.y - body_h) * 0.5f) };
    const ImVec2 p1 = { p0.x + box_w, p0.y + body_h };
    dl->AddRectFilled(p0, p1, IM_COL32(18, 18, 24, 210), 4.0f);
    dl->AddRect      (p0, p1, IM_COL32(80, 80, 90, 255), 4.0f);

    const float x     = p0.x + pad;
    const float bar_w = box_w - 2.0f * pad;
    float       y     = p0.y + pad * 0.5f;

    dl->AddText({x, y}, IM_COL32(235, 235, 235, 255), "Market scarcity");
    y += line_h + 4.0f;

    // Met (substrate, no tint) → scarce (hot). Mirrors the per-market composite.
    constexpr ImU32 substrate = IM_COL32(40, 40, 48, 255);
    constexpr ImU32 hot       = IM_COL32(220, 70, 55, 255);
    constexpr int segs = 24;
    for (int i = 0; i < segs; ++i)
    {
        const float t0 = static_cast<float>(i) / segs;
        const ImU32 c  = lerp_colour(substrate, hot, t0);
        dl->AddRectFilled({ x + bar_w * t0, y },
                          { x + bar_w * static_cast<float>(i + 1) / segs, y + bar_h }, c);
    }
    y += bar_h + 2.0f;
    dl->AddText({x, y}, IM_COL32(170, 175, 185, 255), "met");
    const ImVec2 sts = ImGui::CalcTextSize("scarce");
    dl->AddText({x + bar_w - sts.x, y}, IM_COL32(170, 175, 185, 255), "scarce");
    y += line_h + 4.0f;

    dl->AddRectFilled({x, y + 2.0f}, {x + 10.0f, y + 12.0f},
                      presentation_of(state.lens_resource).colour);
    dl->AddText({x + 14.0f, y}, IM_COL32(235, 235, 235, 255),
                presentation_of(state.lens_resource).name);
}

} // namespace

void draw_body_surface_canvas(const world& w, ui_state& state, const recipe_registry& reg,
                              const economy_report& report, ImVec2 origin, ImVec2 size,
                              bool input_enabled)
{
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    dl->AddRectFilled(origin, origin + size, IM_COL32(18, 18, 24, 255));

    // The surface canvas is the bottom rung of the ladder, so it is only ever
    // drawn as the primary view — never the minimap.
    const float min_dim    = std::min(size.x, size.y);
    const bool  draw_title = min_dim > 320.0f;

    auto body_it = w.bodies.find(state.active_body);
    if (body_it == w.bodies.end())
    {
        if (draw_title)
        {
            const char* msg = "No body selected";
            const ImVec2 ts = ImGui::CalcTextSize(msg);
            dl->AddText(origin + (size - ts) * 0.5f, IM_COL32(150, 150, 150, 255), msg);
        }
        return;
    }

    const body_component& body = body_it->second;

    float title_h = 0.0f;
    if (draw_title)
    {
        title_h = ImGui::GetTextLineHeightWithSpacing();
        char title[128];
        std::snprintf(title, sizeof(title), "%s  -  %s  (%dx%d)",
            body.name.c_str(), body_type_name(body.type), body.grid_width, body.grid_height);
        dl->AddText(origin + ImVec2{4.0f, 2.0f}, IM_COL32(235, 235, 235, 255), title);
    }

    const int gw = std::max(1, body.grid_width);
    const int gh = std::max(1, body.grid_height);

    // Grid occupies the region below the title bar.
    const ImVec2 grid_area_origin = origin + ImVec2{0.0f, title_h};
    const ImVec2 grid_area_size   = { size.x, size.y - title_h };
    const ImVec2 canvas_centre    = grid_area_origin + grid_area_size * 0.5f;

    // --- Hex size at zoom=1 ---
    // zoom=1 is defined as "the full grid height fills the canvas height." The
    // grid width will typically exceed the canvas at this scale — the player pans
    // horizontally. This definition makes zoom=4/3 mean exactly "3/4 of the grid
    // height is visible" regardless of canvas size or grid aspect ratio, so the
    // same zoom value reads correctly on both the primary view and the minimap.
    const float fit_by_y = grid_area_size.y / (1.5f * static_cast<float>(gh) + 0.5f);
    const float hex_size = fit_by_y * kFitMargin;

    // Grid centre in local (unzoomed) world space — used to centre the grid
    // on the canvas at zoom=1 with no pan.
    const float col_step = kSqrt3 * hex_size;
    const float row_step = 1.5f * hex_size;
    const float grid_cx  = (static_cast<float>(gw) - 0.5f) * col_step * 0.5f;
    const float grid_cy  = static_cast<float>(gh - 1) * row_step * 0.5f;

    // --- View transform (pan/zoom) ---
    // The surface canvas is always primary, so pan and zoom always apply.
    const float  zoom        = std::clamp(state.planetary_zoom, kMinZoom, kMaxZoom);

    // Pending centre request (verify.center_tile): now that the exact grid
    // transform is in hand, set the pan that places the requested tile's local
    // centre on the canvas centre — pan = (grid_centre − tile_local) · zoom. This
    // is the one place the centring math lives; Lua callers never replicate it.
    if (state.planetary_center_pending)
    {
        const ImVec2 lc = hex_local_centre(state.planetary_center_col,
                                           state.planetary_center_row, hex_size);
        state.planetary_pan_x = (grid_cx - lc.x) * zoom;
        state.planetary_pan_y = (grid_cy - lc.y) * zoom;
        state.planetary_center_pending = false;
    }

    const ImVec2 view_origin = ImVec2{ canvas_centre.x + state.planetary_pan_x,
                                       canvas_centre.y + state.planetary_pan_y };

    // Local world space → screen space.
    auto to_screen = [&](ImVec2 lp) -> ImVec2 {
        return {
            view_origin.x + (lp.x - grid_cx) * zoom,
            view_origin.y + (lp.y - grid_cy) * zoom,
        };
    };

    // Clip to the grid area so hexes don't overdraw the title bar or the solar canvas.
    dl->PushClipRect(grid_area_origin, grid_area_origin + grid_area_size, true);

    // Tiles that carry a building, mapped to their type so the marker pass can
    // draw the type-specific glyph.
    std::unordered_map<entity_id, building_type> built_tiles;
    for (const auto& [id, bld] : w.buildings)
    {
        auto tile_it = w.tiles.find(bld.tile);
        if (tile_it != w.tiles.end() && tile_it->second.body == state.active_body)
            built_tiles[bld.tile] = bld.type;
    }

    // Spatial index: tile id keyed by its packed (col, row) on the active body,
    // so the Faction-lens border pass can find a tile's grid neighbours without
    // scanning w.tiles. Key packs row-major: row * gw + col (col < gw always).
    std::unordered_map<long long, entity_id> tile_at;
    for (const auto& [id, tile] : w.tiles)
    {
        if (tile.body == state.active_body)
            tile_at[static_cast<long long>(tile.grid_y) * gw + tile.grid_x] = id;
    }

    // Nation owner of a tile, or null_entity when the tile is absent from
    // tile_to_nation (unclaimed). Used by the border pass to compare adjacent
    // owners; "unclaimed" is treated as its own distinct owner.
    auto nation_of = [&](entity_id tile_id) -> entity_id {
        const auto it = w.tile_to_nation.find(tile_id);
        return it != w.tile_to_nation.end() ? it->second : null_entity;
    };

    // Corporation owner of a tile, keyed by tile id. Built only when corporations
    // exist; drives both the building-marker colour and the Corporation lens tint.
    std::unordered_map<entity_id, entity_id> tile_to_corp;
    if (!w.corporations.empty())
    {
        for (const auto& [corp_id, corp] : w.corporations)
        {
            for (entity_id bld_id : corp.assets)
            {
                const auto bld_it = w.buildings.find(bld_id);
                if (bld_it != w.buildings.end())
                    tile_to_corp[bld_it->second.tile] = corp_id;
            }
        }
    }

    // Identity colour for a corporation: the player's corp is corp slot 0;
    // rivals get a stable per-corp slot via a multiplicative hash, kept off slot 0
    // so a rival never collides with the player's colour. Shared by the marker
    // pass and the Corporation lens so the two always agree.
    auto corp_identity = [&](entity_id corp_id) -> ImU32 {
        if (corp_id == w.player_entity)
            return palette::corp_colour(0);
        int slot = static_cast<int>(
            (static_cast<uint32_t>(corp_id) * 2654435761u) % palette::corp_slot_count);
        if (slot == 0)
            slot = 1;
        return palette::corp_colour(slot);
    };

    // Resource lens (BL-019): always single-resource. The lens fills the whole
    // contiguous deposit of the selected resource as a flat, uniform colour — the
    // *shape* of the deposit, not a magnitude gradient. A tile is part of the
    // deposit when it carries any of the resource (deposit > 0); contiguous tiles
    // form one blob (8-connected), but since the fill is uniform the per-tile
    // threshold is visually identical, so no flood-fill grouping is needed. No
    // normalisation maxima — the only state the pass needs is the selected index.

    // Market lens pre-pass: the active body's market is a single per-body exchange,
    // so the Planetary tint is a body-wide wash (not per-tile). Resolve the selected
    // good's price relative to its base (floor) price once; an untraded good (no base
    // price) leaves the wash off. See LENSES.md § Market lens.
    bool  market_active = false;
    float market_ratio  = 1.0f;
    ImU32 market_wash   = 0;
    if (state.overlay == overlay_mode::market)
    {
        const std::size_t g = static_cast<std::size_t>(state.lens_resource);
        for (const auto& [mid, mk] : w.markets)
        {
            if (mk.body != state.active_body)
                continue;
            if (mk.base_price[g] > 0.0f)
            {
                market_ratio  = mk.price[g] / mk.base_price[g];
                market_wash   = diverging_colour(market_ratio);
                market_active = true;
            }
            break;
        }
    }

    // Scarcity lens pre-pass (BL-018): a market-level field, not a per-tile one.
    // Scarcity is driven by **supply shortfall** — how much demand outran supply
    // last tick for the selected good, per market (independent of price). Each
    // market's catchment tiles read as one chunky block (market_for_tile). We
    // collect per-market shortfall and the body-max for normalisation; a tile then
    // looks up its market's shortfall and tints uniformly across the catchment.
    std::unordered_map<entity_id, float> scar_shortfall; // market id → shortfall of sel
    float scar_max_shortfall = 0.0f;
    if (state.overlay == overlay_mode::scarcity)
    {
        const std::size_t sel = static_cast<std::size_t>(state.lens_resource);
        for (const auto& [mid, mk] : w.markets)
        {
            if (mk.body != state.active_body)
                continue;
            const float shortfall = std::max(0.0f, mk.demand[sel] - mk.supply[sel]);
            scar_shortfall[mid] = shortfall;
            scar_max_shortfall  = std::max(scar_max_shortfall, shortfall);
        }
    }

    // Production lens pre-pass (BL-009): per producing tile, the sell value of its
    // building's outputs this tick = Σ(output qty × resolved price). Read from the
    // economy report (output_quantity) + the tile's market prices; a processor's
    // total output is split across its recipe's products by their batch
    // proportions. Idle / exhausted buildings produce nothing → no entry → cold.
    // The geometric-style mean of producing tiles anchors the log scale.
    std::unordered_map<entity_id, float> prod_value; // tile id → output sell value
    float prod_log_sum = 0.0f;
    int   prod_count   = 0;
    if (state.overlay == overlay_mode::production)
    {
        for (const building_report& br : report.buildings)
        {
            if (br.body != state.active_body || br.output_quantity <= 0.0f || !br.active)
                continue;
            const auto bld_it = w.buildings.find(br.building);
            if (bld_it == w.buildings.end())
                continue;
            const entity_id tile_id = bld_it->second.tile;
            const entity_id mid     = market_for_tile(w, tile_id);
            const auto      mk_it   = w.markets.find(mid);
            if (mk_it == w.markets.end())
                continue;
            const auto& price = mk_it->second.price;

            float value = 0.0f;
            if (br.type == building_type::extraction_site)
            {
                value = br.output_quantity * price[static_cast<std::size_t>(br.target_resource)];
            }
            else if (const recipe* rec = reg.get_recipe(br.recipe))
            {
                float out_total = 0.0f, weighted = 0.0f;
                for (std::size_t r = 0; r < resource_count; ++r)
                {
                    out_total += rec->outputs[r];
                    weighted  += rec->outputs[r] * price[r];
                }
                if (out_total > 0.0f)
                    value = br.output_quantity * weighted / out_total;
            }
            if (value > 0.0f)
            {
                prod_value[tile_id] += value;
                prod_log_sum += std::log(value);
                ++prod_count;
            }
        }
    }
    const float prod_mean = prod_count > 0 ? std::exp(prod_log_sum / static_cast<float>(prod_count)) : 0.0f;

    // Opportunity lens pre-pass (BL-017): per tile, the estimated net margin of the
    // best valid building on that terrain — output value minus input value and
    // upkeep — evaluated *without* regard to what is currently built or to
    // logistics. A siting signal: where could value be made? Diverging red(loss)→
    // green(profit), normalised against the body's largest absolute margin.
    std::unordered_map<entity_id, float> opp_margin; // tile id → best net margin
    float opp_max_abs = 0.0f;
    if (state.overlay == overlay_mode::opportunity)
    {
        const building_economics& ext_e  = reg.economics(building_type::extraction_site);
        const building_economics& proc_e = reg.economics(building_type::processing_facility);
        for (const auto& [id, tile] : w.tiles)
        {
            if (tile.body != state.active_body)
                continue;
            const auto mk_it = w.markets.find(market_for_tile(w, id));
            if (mk_it == w.markets.end())
                continue;
            const auto& price = mk_it->second.price;

            float best = 0.0f;
            bool  have = false;

            // Extraction: best single deposit on the tile (valid terrain only).
            for (std::size_t r = 0; r < resource_count; ++r)
            {
                if (tile.resource_deposit[r] <= 0.0f)
                    continue;
                const resource_type rt = static_cast<resource_type>(r);
                if (!placement_rules::can_place(tile, building_type::extraction_site, rt))
                    continue;
                const float gross = ext_e.base_rate * tile.resource_deposit[r] * price[r];
                const float net   = gross - ext_e.maintenance - ext_e.base_wage;
                best = have ? std::max(best, net) : net;
                have = true;
            }

            // Processing: best recipe (valid on any non-ocean terrain).
            if (placement_rules::can_place(tile, building_type::processing_facility, resource_type::iron_ore))
            {
                for (std::size_t i = 0; i < reg.recipe_count(); ++i)
                {
                    const recipe* rec = reg.get_recipe(static_cast<uint16_t>(i));
                    if (!rec)
                        continue;
                    float gross = 0.0f, cost = 0.0f;
                    for (std::size_t r = 0; r < resource_count; ++r)
                    {
                        gross += rec->outputs[r] * price[r];
                        cost  += rec->inputs[r]  * price[r];
                    }
                    const float net = proc_e.base_rate * (gross - cost) - proc_e.maintenance - proc_e.base_wage;
                    best = have ? std::max(best, net) : net;
                    have = true;
                }
            }

            if (have)
            {
                opp_margin[id] = best;
                opp_max_abs    = std::max(opp_max_abs, std::abs(best));
            }
        }
    }

    // Supply lens pre-pass: check whether the active body has any player convoys
    // (source or destination). Used inside the tile loop to gate the per-tile glyph.
    // w.convoys is empty until the dispatch system lands; supply_active stays false.
    bool supply_active = false;
    if (state.overlay == overlay_mode::supply)
    {
        for (const auto& cv : w.convoys)
        {
            if (cv.corp != w.player_entity)
                continue;
            entity_id src_body = null_entity;
            entity_id dst_body = null_entity;
            const auto smk = w.markets.find(cv.source_market);
            const auto dmk = w.markets.find(cv.dest_market);
            if (smk != w.markets.end()) src_body = smk->second.body;
            if (dmk != w.markets.end()) dst_body = dmk->second.body;
            if (src_body == state.active_body || dst_body == state.active_body)
            {
                supply_active = true;
                break;
            }
        }
    }

    const ImVec2 mouse = ImGui::GetIO().MousePos;

    // Hover resolves to a single tile copy. Adjacent hexes' circular hit-tests
    // overlap, and the cylinder draws several wrap copies of each tile, so more
    // than one (tile, copy) can satisfy the hover test at once. The nearest hex
    // centre to the cursor wins; its outline is drawn after the grid pass so only
    // one tile highlights (see the highlight tie convention in highlight.hpp).
    entity_id hovered_tile     = null_entity;
    bool      hovered_selected = false;
    bool      have_hover       = false;
    float     best_d2          = std::numeric_limits<float>::max();
    ImVec2    hover_verts[6]   = {};

    // Slightly shrink the drawn hex so the background shows through as a border.
    // Use the full circumradius for hit-testing so small hexes stay clickable.
    const float draw_r = hex_size * zoom - 1.0f;
    const float hit_r  = hex_size * zoom;

    // --- Infinite horizontal scroll ---
    // The grid is a cylinder: column gw wraps onto column 0. In screen space the
    // grid repeats every `period_px = gw * col_step * zoom`. Each tile is drawn
    // (and hit-tested) at every integer wrap offset k whose copy falls within the
    // canvas, so panning past either edge continues seamlessly from the far side.
    const float period_px = static_cast<float>(gw) * col_step * zoom;
    const float visible_left  = grid_area_origin.x - hit_r;
    const float visible_right = grid_area_origin.x + grid_area_size.x + hit_r;

    // Draw the active body's tiles in a deterministic order. `w.tiles` is an
    // unordered_map, whose iteration order varies between process runs (hash
    // seeding) — that reorders overlapping antialiased hex edges and markers and
    // makes full-body golden captures flake by ~1–2%. Sorting the tile ids fixes
    // the draw order so a capture is reproducible run-to-run. (`tile_at` already
    // holds exactly the active body's tiles, keyed by packed grid coordinate.)
    std::vector<entity_id> draw_order;
    draw_order.reserve(tile_at.size());
    for (const auto& [key, tid] : tile_at)
        draw_order.push_back(tid);
    std::sort(draw_order.begin(), draw_order.end());

    for (const entity_id id : draw_order)
    {
        const tile_component& tile = w.tiles.at(id);
        if (tile.body != state.active_body)
            continue;

        const ImVec2 lc   = hex_local_centre(tile.grid_x, tile.grid_y, hex_size);
        const ImVec2 sc   = to_screen(lc);

        // Fill is the tile's terrain colour, except under the Faction lens where
        // a tile owned by a nation is tinted that nation's identity colour. The
        // tint is a direct replacement (no blend); unclaimed tiles — absent from
        // tile_to_nation, e.g. ocean — keep their terrain hue so the political
        // map still reads as terrain underneath.
        ImU32 fill = terrain_colour(tile.composition);
        if (state.overlay == overlay_mode::country)
        {
            const auto nat_it = w.tile_to_nation.find(id);
            if (nat_it != w.tile_to_nation.end())
                fill = palette::nation_colour(nat_it->second);
        }
        // Corporation lens: tint a tile that carries a corporate building with its
        // owning corp's colour (direct replacement, like the faction tint). Tiles
        // with no corporate building keep their terrain hue — no nation underlay.
        else if (state.overlay == overlay_mode::corporation)
        {
            const auto corp_it = tile_to_corp.find(id);
            if (corp_it != tile_to_corp.end())
                fill = corp_identity(corp_it->second);
        }
        // Resource lens (BL-019): flat, uniform fill over the contiguous deposit of
        // the selected resource — the *shape* of the deposit, no magnitude gradient.
        // Any tile carrying the resource (deposit > 0) is part of the deposit and
        // takes the resource's identity colour at a fixed opacity; a tile without it
        // keeps its terrain hue. Intensity lives in tile detail, not the lens.
        else if (state.overlay == overlay_mode::resource)
        {
            const std::size_t sel = static_cast<std::size_t>(state.lens_resource);
            if (tile.resource_deposit[sel] > 0.0f)
                fill = lerp_colour(fill, presentation_of(state.lens_resource).colour, 0.8f);
        }
        // Market lens: a body-wide diverging wash for the selected good's price
        // relative to its floor (warm = dear, cool = cheap). Uniform across the body
        // because the market is per-body; composited over terrain so the surface still
        // reads. Off when the good is untraded here (no base price).
        else if (state.overlay == overlay_mode::market)
        {
            if (market_active)
                fill = lerp_colour(fill, market_wash, 0.55f);
        }
        // Population lens: tint a tile by its habitability (0–1, already normalised) —
        // a sequential dark→liveable-green gradient composited over terrain, so
        // hospitable land reads bright and barren land barely tints.
        else if (state.overlay == overlay_mode::population)
        {
            const float h = std::clamp(tile.habitability, 0.0f, 1.0f);
            if (h > 0.0f)
            {
                constexpr ImU32 live = IM_COL32(80, 200, 110, 255);
                fill = lerp_colour(fill, live, 0.15f + 0.7f * h);
            }
        }
        // Opportunity lens (BL-017): tint a tile by the net margin of the best valid
        // building on its terrain — a diverging red(loss)→green(profit) surface
        // normalised against the body's largest absolute margin. A siting signal:
        // where could value be made? Tiles with no valid building keep terrain hue.
        else if (state.overlay == overlay_mode::opportunity)
        {
            const auto it = opp_margin.find(id);
            if (it != opp_margin.end() && opp_max_abs > 0.0f)
            {
                const float d = std::clamp(it->second / opp_max_abs, -1.0f, 1.0f);
                constexpr ImU32 loss   = IM_COL32(216, 100,  96, 255);
                constexpr ImU32 profit = IM_COL32(110, 200, 120, 255);
                fill = d < 0.0f ? lerp_colour(fill, loss, -d * 0.75f)
                                : lerp_colour(fill, profit, d * 0.75f);
            }
        }
        // Production lens (BL-009): tint a producing tile by output sell value this
        // tick, log-scaled relative to the body's producing-tile mean (above mean
        // warm, below cool). Idle / exhausted / unbuilt tiles read cold (no tint).
        else if (state.overlay == overlay_mode::production)
        {
            const auto it = prod_value.find(id);
            if (it != prod_value.end() && prod_mean > 0.0f)
                fill = lerp_colour(fill, diverging_colour(it->second / prod_mean), 0.6f);
        }
        // Scarcity lens (BL-018): a market-level shortfall field. Every tile in a
        // market's catchment reads as one chunky block tinted by that market's
        // supply shortfall of the selected good (demand outran supply last tick),
        // normalised across the body's markets. Hot where scarce; no tint where met.
        else if (state.overlay == overlay_mode::scarcity)
        {
            const auto sf_it = scar_shortfall.find(market_for_tile(w, id));
            if (sf_it != scar_shortfall.end() && scar_max_shortfall > 0.0f)
            {
                const float scar = std::clamp(sf_it->second / scar_max_shortfall, 0.0f, 1.0f);
                constexpr ImU32 hot = IM_COL32(220, 70, 55, 255);
                fill = lerp_colour(fill, hot, 0.6f * scar);
            }
        }
        const auto   built_it  = built_tiles.find(id);
        const bool   built     = built_it != built_tiles.end();
        const building_type built_type = built ? built_it->second : building_type::none;
        const bool   selected  = (id == state.selected_entity);

        // Range of wrap copies that land inside the canvas horizontally.
        const int k_min = (period_px > 0.0f)
            ? static_cast<int>(std::ceil((visible_left  - sc.x) / period_px)) : 0;
        const int k_max = (period_px > 0.0f)
            ? static_cast<int>(std::floor((visible_right - sc.x) / period_px)) : 0;

        for (int k = k_min; k <= k_max; ++k)
        {
            const float cx = sc.x + static_cast<float>(k) * period_px;
            const float cy = sc.y;

            ImVec2 verts[6];
            hex_vertices(verts, cx, cy, draw_r);
            dl->AddConvexPolyFilled(verts, 6, fill);

            // Nation borders (Country lens only). Draw a dark line on every hex
            // edge shared with a neighbour of a different owner — including the
            // claimed/unclaimed boundary. The grid is odd-r offset, so the six
            // neighbour offsets differ between even and odd rows.
            if (state.overlay == overlay_mode::country)
            {
                const entity_id own_nation = nation_of(id);

                // Standard odd-r neighbour offsets (col, row deltas).
                static const int even_off[6][2] =
                    {{+1, 0}, {0, -1}, {-1, -1}, {-1, 0}, {-1, +1}, {0, +1}};
                static const int odd_off[6][2] =
                    {{+1, 0}, {+1, -1}, {0, -1}, {-1, 0}, {0, +1}, {+1, +1}};
                const int (*off)[2] = (tile.grid_y & 1) ? odd_off : even_off;

                for (int n = 0; n < 6; ++n)
                {
                    const int nrow = tile.grid_y + off[n][1];
                    if (nrow < 0 || nrow >= gh)
                        continue; // Off the top/bottom edge: no neighbour tile.

                    // Columns wrap on the horizontal cylinder.
                    int ncol = (tile.grid_x + off[n][0]) % gw;
                    if (ncol < 0)
                        ncol += gw;

                    const auto nb_it = tile_at.find(static_cast<long long>(nrow) * gw + ncol);
                    if (nb_it == tile_at.end())
                        continue;
                    if (nation_of(nb_it->second) == own_nation)
                        continue; // Same owner: interior edge, no border.

                    // Draw the shared edge via the midpoint-perpendicular method:
                    // place the segment at the midpoint of the centre-to-centre
                    // line, perpendicular to it, with length equal to one hex side
                    // (== circumradius draw_r for a regular hexagon). This avoids
                    // mapping neighbour directions to per-vertex pairs, which the
                    // offset-row vertex ordering makes error-prone. The neighbour's
                    // screen centre is taken at the SAME wrap offset k as this tile.
                    const ImVec2 nb_lc = hex_local_centre(ncol, nrow, hex_size);
                    ImVec2 nb_sc = to_screen(nb_lc);
                    nb_sc.x += static_cast<float>(k) * period_px;

                    float dirx = nb_sc.x - cx;
                    float diry = nb_sc.y - cy;
                    const float len = std::sqrt(dirx * dirx + diry * diry);
                    if (len <= 0.0f)
                        continue;
                    dirx /= len;
                    diry /= len;

                    const float mx = (cx + nb_sc.x) * 0.5f;
                    const float my = (cy + nb_sc.y) * 0.5f;
                    const float px = -diry; // perpendicular to the centre line
                    const float py =  dirx;
                    const float half = draw_r * 0.5f;

                    dl->AddLine({mx - px * half, my - py * half},
                                {mx + px * half, my + py * half},
                                IM_COL32(20, 20, 20, 200), 1.5f);
                }
            }

            // Corporation lens: outline the player's own tiles so the player's
            // footprint reads at a glance against rivals' flat tints. Rival tiles
            // are distinguished by their fill colour alone (see the fill pass).
            if (state.overlay == overlay_mode::corporation)
            {
                const auto corp_it = tile_to_corp.find(id);
                if (corp_it != tile_to_corp.end() && corp_it->second == w.player_entity)
                    dl->AddPolyline(verts, 6, palette::selection,
                                    ImDrawFlags_Closed, 2.0f);
            }

            if (built)
            {
                const float mr = std::max(2.0f, draw_r * 0.22f);

                // Building markers carry their owning corporation's colour (the
                // player's corp gets faction slot 0). Always on, independent of
                // the lens. Tiles with no corporate owner stay white.
                ImU32 marker_col = IM_COL32(255, 255, 255, 255);
                const auto corp_it = tile_to_corp.find(id);
                if (corp_it != tile_to_corp.end())
                    marker_col = corp_identity(corp_it->second);

                icons::building(dl, {cx, cy}, mr, built_type, marker_col);
            }

            // Supply lens: draw a convoy glyph on every tile when the active body
            // has a player convoy passing through it. supply_active is false when
            // w.convoys is empty, so this is a no-op until dispatch is wired.
            if (supply_active)
            {
                constexpr ImU32 supply_col = IM_COL32(80, 200, 255, 200);
                const float gr = std::max(2.0f, draw_r * 0.28f);
                icons::convoy(dl, {cx, cy}, gr, supply_col);
            }

            // Selection outline is drawn on every visible copy of the selected
            // tile; hover is deferred to a single nearest copy, resolved below.
            draw_hex_highlight(dl, verts,
                resolve_highlight(selected, /*hovered=*/false, /*pinned=*/false));

            // Hit-test: distance to hex centre < circumradius (approximate,
            // sufficient for usability). Scoped per wrap copy so the highlight
            // lands on the copy actually under the cursor; nearest centre wins.
            if (input_enabled)
            {
                const float dx = mouse.x - cx;
                const float dy = mouse.y - cy;
                const float d2 = dx * dx + dy * dy;
                const bool in_area = mouse.x >= grid_area_origin.x &&
                                     mouse.x <= grid_area_origin.x + grid_area_size.x &&
                                     mouse.y >= grid_area_origin.y &&
                                     mouse.y <= grid_area_origin.y + grid_area_size.y;
                if (in_area && d2 <= hit_r * hit_r && d2 < best_d2)
                {
                    best_d2          = d2;
                    hovered_tile     = id;
                    hovered_selected = selected;
                    have_hover       = true;
                    std::copy(std::begin(verts), std::end(verts), std::begin(hover_verts));
                }
            }
        }
    }

    // Hover outline for the single resolved copy. Skipped when the hovered tile is
    // also the selection — selection outranks hover, and its ring is already drawn.
    if (have_hover && !hovered_selected)
        draw_hex_highlight(dl, hover_verts, highlight::hovered);

    // Building-placement ghost preview. When construction mode is active and a tile
    // is hovered, draw a translucent-intent marker of the chosen building type at the
    // hovered copy's centre, tinted green when the placement-rules seam accepts the
    // tile and red when it rejects it. The marker reads placement_rules::can_place
    // and mutates nothing — the click below is what enqueues the build. The
    // hovered copy's centre is the centroid of its six vertices (the wrap copy the
    // cursor actually resolved to); the marker radius mirrors the built-marker pass.
    if (have_hover && state.construction.active)
    {
        float gx = 0.0f, gy = 0.0f;
        for (const ImVec2& v : hover_verts)
        {
            gx += v.x;
            gy += v.y;
        }
        gx /= 6.0f;
        gy /= 6.0f;

        const float mr = std::max(2.0f, draw_r * 0.22f);

        const tile_component& hovered = w.tiles.at(hovered_tile);
        const bool placeable = placement_rules::can_place(
            hovered, state.construction.type, state.construction.target);
        const ImU32 ghost_col = placeable ? palette::positive : palette::negative;

        icons::building(dl, {gx, gy}, mr, state.construction.type, ghost_col);
    }

    dl->PopClipRect();

    // On-canvas lens key (drawn unclipped, top-right of the grid area, before the
    // input early-out so it shows in headless captures too). Resource is the first
    // lens to carry a colour key; Market's diverging key is added alongside.
    if (state.overlay == overlay_mode::resource)
        draw_resource_key(dl, grid_area_origin, grid_area_size, state);
    else if (state.overlay == overlay_mode::market)
        draw_market_key(dl, grid_area_origin, grid_area_size, state, market_active, market_ratio);
    else if (state.overlay == overlay_mode::population)
        draw_population_key(dl, grid_area_origin, grid_area_size);
    else if (state.overlay == overlay_mode::opportunity)
        draw_opportunity_key(dl, grid_area_origin, grid_area_size);
    else if (state.overlay == overlay_mode::production)
        draw_production_key(dl, grid_area_origin, grid_area_size);
    else if (state.overlay == overlay_mode::scarcity)
        draw_scarcity_key(dl, grid_area_origin, grid_area_size, state);

    if (!input_enabled)
        return;

    // Hover tooltip.
    if (hovered_tile != null_entity)
        draw_hover_card(dl, w, state, hovered_tile);

    // Click handling. The surface is the bottom rung, so there is nothing to
    // descend into: a single left-click simply selects the hovered tile (null
    // clears the selection on empty space) and fills the Selection info element.
    // No view change; the player ascends via the minimap. See SELECTION.md.
    // Construction mode suppresses selection: in placement mode a left-click is a
    // construction gesture, not a selection one, so it must not retarget the
    // Selection info element.
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        if (!state.construction.active)
        {
            state.selected_entity = hovered_tile;
        }
        else if (hovered_tile != null_entity)
        {
            // Placement mode: queue a construction request on the hovered tile.
            // app::render executes it against the mutable world (this canvas holds
            // only const world&) and clears it. See SELECTION.md / construction.hpp.
            state.construction.pending_tile   = hovered_tile;
            state.construction.pending_type   = state.construction.type;
            state.construction.pending_target = state.construction.target;
        }
    }

    // Pan and zoom. Middle mouse button pans; scroll wheel zooms, anchored at
    // the cursor so the point under the mouse stays fixed.
    {
        ImGuiIO& io = ImGui::GetIO();

        if (ImGui::IsMouseDown(ImGuiMouseButton_Middle))
        {
            state.planetary_pan_x += io.MouseDelta.x;
            state.planetary_pan_y += io.MouseDelta.y;
        }

        // Keep horizontal pan bounded to one grid period. The grid wraps, so
        // this is visually identical but stops pan_x growing without limit.
        if (period_px > 0.0f)
            state.planetary_pan_x = std::fmod(state.planetary_pan_x, period_px);

        if (io.MouseWheel != 0.0f)
        {
            const float new_zoom = std::clamp(zoom * std::pow(1.1f, io.MouseWheel), kMinZoom, kMaxZoom);
            // World point under the cursor, kept fixed across the zoom change.
            const ImVec2 wp = { (mouse.x - view_origin.x) / zoom + grid_cx,
                                (mouse.y - view_origin.y) / zoom + grid_cy };
            state.planetary_pan_x = mouse.x - (wp.x - grid_cx) * new_zoom - canvas_centre.x;
            state.planetary_pan_y = mouse.y - (wp.y - grid_cy) * new_zoom - canvas_centre.y;
            state.planetary_zoom  = new_zoom;
        }
    }
}

} // namespace ui
