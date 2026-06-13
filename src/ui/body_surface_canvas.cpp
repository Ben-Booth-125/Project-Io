#define IMGUI_DEFINE_MATH_OPERATORS
#include "body_surface_canvas.hpp"

#include <algorithm>
#include <cstdio>
#include <unordered_set>

namespace ui {

namespace {

const char* terrain_name(terrain_type t)
{
    switch (t)
    {
        case terrain_type::barren:   return "Barren";
        case terrain_type::rocky:    return "Rocky";
        case terrain_type::icy:      return "Icy";
        case terrain_type::volcanic: return "Volcanic";
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

// Resource labels in resource_type order, for the hover tooltip.
constexpr const char* resource_labels[resource_count] = {
    "Iron Ore", "Ice", "Silicates", "Rare Metals"
};

} // namespace

void draw_body_surface_canvas(const world& w, ui_state& state, ImVec2 origin, ImVec2 size, bool input_enabled)
{
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    // Background fill.
    dl->AddRectFilled(origin, origin + size, IM_COL32(18, 18, 24, 255));

    // This canvas is the minimap when the solar canvas holds the primary slot.
    const bool is_minimap = !state.surface_is_primary;
    const float min_dim = std::min(size.x, size.y);
    const bool  draw_title = min_dim > 320.0f; // suppress the title bar on the minimap

    // Resolve the selected body. With no selection there is nothing to draw.
    auto body_it = w.bodies.find(state.active_body);
    if (body_it == w.bodies.end())
    {
        if (draw_title)
        {
            const char* msg = "No body selected";
            const ImVec2 ts = ImGui::CalcTextSize(msg);
            dl->AddText(origin + (size - ts) * 0.5f, IM_COL32(150, 150, 150, 255), msg);
        }
        // Even with no body, an empty minimap click still swaps to primary.
        if (input_enabled && is_minimap && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            state.surface_is_primary = true;
        return;
    }

    const body_component& body = body_it->second;

    // Optional title bar showing the selected body name and type.
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

    // Grid occupies the region below the title bar, centred in the remaining space.
    const ImVec2 grid_area_origin = origin + ImVec2{0.0f, title_h};
    const ImVec2 grid_area_size   = { size.x, size.y - title_h };

    const float cell_size = std::min(grid_area_size.x / gw, grid_area_size.y / gh);
    const ImVec2 grid_origin =
        grid_area_origin + (grid_area_size - ImVec2{gw * cell_size, gh * cell_size}) * 0.5f;

    // Tiles that carry a building, so we can mark them.
    std::unordered_set<entity_id> built_tiles;
    for (const auto& [id, bld] : w.buildings)
    {
        auto tile_it = w.tiles.find(bld.tile);
        if (tile_it != w.tiles.end() && tile_it->second.body == state.active_body)
            built_tiles.insert(bld.tile);
    }

    const ImVec2 mouse = ImGui::GetIO().MousePos;
    entity_id hovered_tile = null_entity;

    const float marker_half = std::max(2.0f, std::min(4.0f, cell_size * 0.18f));

    for (const auto& [id, tile] : w.tiles)
    {
        if (tile.body != state.active_body)
            continue;

        // 1 px gap on each side lets the background show through as a border.
        const ImVec2 cell_min = grid_origin + ImVec2{tile.grid_x * cell_size + 1.0f, tile.grid_y * cell_size + 1.0f};
        const ImVec2 cell_max = grid_origin + ImVec2{(tile.grid_x + 1) * cell_size - 1.0f, (tile.grid_y + 1) * cell_size - 1.0f};

        dl->AddRectFilled(cell_min, cell_max, terrain_colour(tile.terrain));

        if (built_tiles.count(id))
        {
            const ImVec2 cell_centre = (cell_min + cell_max) * 0.5f;
            dl->AddRectFilled(cell_centre - ImVec2{marker_half, marker_half},
                              cell_centre + ImVec2{marker_half, marker_half},
                              IM_COL32(255, 255, 255, 255));
        }

        if (id == state.active_tile)
            dl->AddRect(cell_min, cell_max, IM_COL32(255, 255, 255, 255), 0.0f, 0, 2.0f);

        if (input_enabled &&
            mouse.x >= cell_min.x && mouse.x <= cell_max.x &&
            mouse.y >= cell_min.y && mouse.y <= cell_max.y)
        {
            hovered_tile = id;
        }
    }

    if (!input_enabled)
        return;

    // Hover tooltip: coordinates, terrain, hazard, habitability, non-zero deposits.
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

    // Click handling.
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        // A click anywhere in the minimap brings the surface canvas forward.
        if (is_minimap)
            state.surface_is_primary = true;

        if (hovered_tile != null_entity)
            state.active_tile = hovered_tile;
    }
}

} // namespace ui
