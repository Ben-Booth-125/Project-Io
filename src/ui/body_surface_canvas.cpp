#define IMGUI_DEFINE_MATH_OPERATORS
#include "body_surface_canvas.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <unordered_set>

namespace ui {

namespace {

constexpr float kSqrt3 = 1.7320508f;
constexpr float kPi    = 3.14159265f;

const char* terrain_name(terrain_type t)
{
    switch (t)
    {
        case terrain_type::barren:   return "Barren";
        case terrain_type::rocky:    return "Rocky";
        case terrain_type::icy:      return "Icy";
        case terrain_type::volcanic: return "Volcanic";
        case terrain_type::water:    return "Water";
        default:                     return "?";
    }
}

ImU32 terrain_colour(terrain_type t)
{
    switch (t)
    {
        case terrain_type::barren:   return IM_COL32(170, 145, 100, 255);
        case terrain_type::rocky:    return IM_COL32(112, 105,  95, 255);
        case terrain_type::icy:      return IM_COL32(160, 200, 220, 255);
        case terrain_type::volcanic: return IM_COL32(135,  55,  28, 255);
        case terrain_type::water:    return IM_COL32( 40,  80, 160, 255);
        default:                     return IM_COL32( 60,  60,  60, 255);
    }
}

const char* body_type_name(body_type t)
{
    switch (t)
    {
        case body_type::planet:   return "Planet";
        case body_type::moon:     return "Moon";
        case body_type::asteroid: return "Asteroid";
        case body_type::station:  return "Station";
        default:                  return "?";
    }
}

constexpr const char* resource_labels[resource_count] = {
    "Iron Ore", "Ice", "Silicates", "Rare Metals"
};

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

} // namespace

void draw_body_surface_canvas(const world& w, ui_state& state, ImVec2 origin, ImVec2 size, bool input_enabled)
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
    const float hex_size = fit_by_y * 0.95f; // 5% margin

    // Grid centre in local (unzoomed) world space — used to centre the grid
    // on the canvas at zoom=1 with no pan.
    const float col_step = kSqrt3 * hex_size;
    const float row_step = 1.5f * hex_size;
    const float grid_cx  = (static_cast<float>(gw) - 0.5f) * col_step * 0.5f;
    const float grid_cy  = static_cast<float>(gh - 1) * row_step * 0.5f;

    // --- View transform (pan/zoom) ---
    // The surface canvas is always primary, so pan and zoom always apply.
    const float  zoom        = std::max(0.1f, state.planetary_zoom);
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

    // Tiles that carry a building, for the building marker pass.
    std::unordered_set<entity_id> built_tiles;
    for (const auto& [id, bld] : w.buildings)
    {
        auto tile_it = w.tiles.find(bld.tile);
        if (tile_it != w.tiles.end() && tile_it->second.body == state.active_body)
            built_tiles.insert(bld.tile);
    }

    const ImVec2 mouse = ImGui::GetIO().MousePos;
    entity_id hovered_tile = null_entity;

    // Slightly shrink the drawn hex so the background shows through as a border.
    // Use the full circumradius for hit-testing so small hexes stay clickable.
    const float draw_r = hex_size * zoom - 1.0f;
    const float hit_r  = hex_size * zoom;

    for (const auto& [id, tile] : w.tiles)
    {
        if (tile.body != state.active_body)
            continue;

        const ImVec2 lc  = hex_local_centre(tile.grid_x, tile.grid_y, hex_size);
        const ImVec2 sc  = to_screen(lc);

        ImVec2 verts[6];
        hex_vertices(verts, sc.x, sc.y, draw_r);
        dl->AddConvexPolyFilled(verts, 6, terrain_colour(tile.terrain));

        if (built_tiles.count(id))
        {
            const float mh = std::max(2.0f, draw_r * 0.18f);
            dl->AddRectFilled(sc - ImVec2{mh, mh}, sc + ImVec2{mh, mh},
                              IM_COL32(255, 255, 255, 255));
        }

        if (id == state.active_tile)
        {
            ImVec2 outline[6];
            hex_vertices(outline, sc.x, sc.y, draw_r);
            dl->AddPolyline(outline, 6, IM_COL32(255, 255, 255, 255), ImDrawFlags_Closed, 2.0f);
        }

        // Hit-test: distance to hex centre < circumradius (approximate, sufficient for usability).
        if (input_enabled)
        {
            const float dx = mouse.x - sc.x;
            const float dy = mouse.y - sc.y;
            const bool in_area = mouse.x >= grid_area_origin.x &&
                                 mouse.x <= grid_area_origin.x + grid_area_size.x &&
                                 mouse.y >= grid_area_origin.y &&
                                 mouse.y <= grid_area_origin.y + grid_area_size.y;
            if (in_area && dx * dx + dy * dy <= hit_r * hit_r)
                hovered_tile = id;
        }
    }

    // TODO (horizontal wrap): When the view is panned past the left or right
    // edge of the grid, tiles should continue rendering seamlessly from the
    // opposite side (infinite horizontal scroll). This requires drawing each
    // tile at up to three x-offsets: nominal, +grid_width_px, and -grid_width_px,
    // clipping to the canvas area. Hit-testing needs the same offset logic.
    dl->PopClipRect();

    if (!input_enabled)
        return;

    // Hover tooltip.
    if (hovered_tile != null_entity)
    {
        const tile_component& tile = w.tiles.at(hovered_tile);
        ImGui::BeginTooltip();
        ImGui::Text("[%d, %d]", tile.grid_x, tile.grid_y);
        ImGui::Text("%s", terrain_name(tile.terrain));
        ImGui::Text("Hazard: %.2f", tile.hazard_level);
        ImGui::Text("Habitability: %.2f", tile.habitability);
        bool any_deposit = false;
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            if (tile.resource_deposit[r] > 0.0f)
            {
                ImGui::Text("%s: %.1f", resource_labels[r], tile.resource_deposit[r]);
                any_deposit = true;
            }
        }
        if (!any_deposit)
            ImGui::TextDisabled("No deposits");
        ImGui::EndTooltip();
    }

    // Click handling — select the hovered tile. The surface is the bottom rung,
    // so a tile click never changes the view; the player ascends via the minimap.
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && hovered_tile != null_entity)
        state.active_tile = hovered_tile;

    // Pan and zoom. Middle mouse button pans; scroll wheel zooms, anchored at
    // the cursor so the point under the mouse stays fixed.
    {
        ImGuiIO& io = ImGui::GetIO();

        if (ImGui::IsMouseDown(ImGuiMouseButton_Middle))
        {
            state.planetary_pan_x += io.MouseDelta.x;
            state.planetary_pan_y += io.MouseDelta.y;
        }

        if (io.MouseWheel != 0.0f)
        {
            const float new_zoom = std::clamp(zoom * std::pow(1.1f, io.MouseWheel), 0.2f, 20.0f);
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
