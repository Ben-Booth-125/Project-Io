#include "tile_inspector.hpp"

#include "presentation.hpp"
#include "profile_panel.hpp" // profile_panel_width/height — spawn clear of the profile

#include <imgui.h>

#include <algorithm>
#include <vector>

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

const char* building_type_name(building_type t)
{
    switch (t)
    {
        case building_type::extraction_site:     return "Extraction Site";
        case building_type::processing_facility: return "Processing Facility";
        case building_type::port:                return "Port";
        default:                                 return "None";
    }
}

} // namespace

void draw_tile_inspector(const world& w, bool* p_open)
{
    // Honour the open flag; when closed the window draws nothing at all.
    if (p_open && !*p_open)
        return;

    // Collect body IDs into a stable order for the combo box.
    std::vector<entity_id> body_ids;
    body_ids.reserve(w.bodies.size());
    for (const auto& [id, _] : w.bodies)
        body_ids.push_back(id);
    std::sort(body_ids.begin(), body_ids.end());

    // Open clear of the profile (top-left) on first appearance — to its right and
    // below the header strip. The window is movable thereafter.
    ImGui::SetNextWindowPos({profile_panel_width + 10.0f, profile_panel_height + 10.0f},
                            ImGuiCond_Once);
    ImGui::SetNextWindowSize({820, 560}, ImGuiCond_Once);
    // Passing p_open gives the window a close button that clears the flag.
    ImGui::Begin("Tile Ledger", p_open);

    if (body_ids.empty())
    {
        ImGui::TextDisabled("No bodies in world.");
        ImGui::End();
        return;
    }

    // --- Body selector ---
    static entity_id selected_body = 0;
    if (selected_body == 0)
        selected_body = body_ids.front();

    const body_component& sel_body = w.bodies.at(selected_body);
    if (ImGui::BeginCombo("Body", sel_body.name.c_str()))
    {
        for (entity_id id : body_ids)
        {
            const body_component& b = w.bodies.at(id);
            const bool is_sel = (id == selected_body);
            if (ImGui::Selectable(b.name.c_str(), is_sel))
                selected_body = id;
            if (is_sel)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    // Body summary line
    ImGui::SameLine();
    ImGui::TextDisabled("%s  |  %.2f AU  |  %dx%d tiles",
        body_type_name(sel_body.type),
        sel_body.orbital_radius_au,
        sel_body.grid_width,
        sel_body.grid_height);

    ImGui::Separator();

    // --- Tile table ---
    // Section comment: columns map 1:1 to tile_component fields so this
    // table doubles as the specification for the production tile canvas.
    constexpr ImGuiTableFlags table_flags =
        ImGuiTableFlags_BordersOuter  |
        ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_RowBg         |
        ImGuiTableFlags_ScrollY       |
        ImGuiTableFlags_SizingFixedFit;

    const float row_height    = ImGui::GetTextLineHeightWithSpacing();
    const float table_height  = row_height * 12.0f; // show ~11 tiles before scroll

    // Columns: x, y, terrain, hazard, habitability, then one per resource type.
    const int col_count = 5 + static_cast<int>(resource_count);
    if (ImGui::BeginTable("tiles", col_count, table_flags, {0.0f, table_height}))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("X",    ImGuiTableColumnFlags_WidthFixed, 28.0f);
        ImGui::TableSetupColumn("Y",    ImGuiTableColumnFlags_WidthFixed, 28.0f);
        ImGui::TableSetupColumn("Terrain",     ImGuiTableColumnFlags_WidthFixed,  80.0f);
        ImGui::TableSetupColumn("Hazard",      ImGuiTableColumnFlags_WidthFixed,  60.0f);
        ImGui::TableSetupColumn("Habitability",ImGuiTableColumnFlags_WidthFixed,  90.0f);
        for (std::size_t r = 0; r < resource_count; ++r)
            ImGui::TableSetupColumn(resource_name(static_cast<resource_type>(r)),
                                    ImGuiTableColumnFlags_WidthFixed, 88.0f);
        ImGui::TableHeadersRow();

        // Collect and sort tiles belonging to the selected body.
        std::vector<const tile_component*> body_tiles;
        for (const auto& [id, tile] : w.tiles)
        {
            if (tile.body == selected_body)
                body_tiles.push_back(&tile);
        }
        std::sort(body_tiles.begin(), body_tiles.end(),
            [](const tile_component* a, const tile_component* b) {
                return (a->grid_y != b->grid_y) ? (a->grid_y < b->grid_y)
                                                : (a->grid_x < b->grid_x);
            });

        for (const tile_component* tile : body_tiles)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::Text("%d", tile->grid_x);
            ImGui::TableSetColumnIndex(1); ImGui::Text("%d", tile->grid_y);
            ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(terrain_name(tile->terrain));
            ImGui::TableSetColumnIndex(3); ImGui::Text("%.2f", tile->hazard_level);
            ImGui::TableSetColumnIndex(4); ImGui::Text("%.2f", tile->habitability);
            for (std::size_t r = 0; r < resource_count; ++r)
            {
                ImGui::TableSetColumnIndex(static_cast<int>(5 + r));
                const float deposit = tile->resource_deposit[r];
                if (deposit > 0.0f)
                    ImGui::Text("%.1f", deposit);
                else
                    ImGui::TextDisabled("—");
            }
        }

        ImGui::EndTable();
    }

    // --- Buildings on this body ---
    ImGui::Spacing();
    ImGui::SeparatorText("Buildings");

    bool any_buildings = false;
    for (const auto& [id, bld] : w.buildings)
    {
        // Resolve the building's tile to check body ownership.
        auto tile_it = w.tiles.find(bld.tile);
        if (tile_it == w.tiles.end() || tile_it->second.body != selected_body)
            continue;

        any_buildings = true;
        const tile_component& t = tile_it->second;
        ImGui::BulletText("%s  at [%d,%d]  workforce %.0f%%",
            building_type_name(bld.type),
            t.grid_x, t.grid_y,
            bld.workforce_assigned * 100.0f);
    }
    if (!any_buildings)
        ImGui::TextDisabled("None");

    // --- Market state for this body ---
    ImGui::Spacing();
    ImGui::SeparatorText("Market");

    bool any_market = false;
    for (const auto& [id, mkt] : w.markets)
    {
        if (mkt.body != selected_body)
            continue;

        any_market = true;

        // Section comment: columns mirror market_component arrays so this
        // panel is the functional specification for the production market ledger.
        if (ImGui::BeginTable("market", 5,
            ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV |
            ImGuiTableFlags_RowBg        | ImGuiTableFlags_SizingFixedFit))
        {
            ImGui::TableSetupColumn("Resource",   ImGuiTableColumnFlags_WidthFixed, 100.0f);
            ImGui::TableSetupColumn("Supply",     ImGuiTableColumnFlags_WidthFixed,  70.0f);
            ImGui::TableSetupColumn("Demand",     ImGuiTableColumnFlags_WidthFixed,  70.0f);
            ImGui::TableSetupColumn("Price",      ImGuiTableColumnFlags_WidthFixed,  60.0f);
            ImGui::TableSetupColumn("Base Price", ImGuiTableColumnFlags_WidthFixed,  80.0f);
            ImGui::TableHeadersRow();

            for (std::size_t r = 0; r < resource_count; ++r)
            {
                const resource_presentation& rp = presentation_of(static_cast<resource_type>(r));
                ImGui::TableNextRow();

                // Resource: identity colour swatch + name, drawn from the shared
                // presentation metadata so the colour reads the same everywhere.
                ImGui::TableSetColumnIndex(0);
                const float    sw   = ImGui::GetTextLineHeight();
                const ImVec2   swp  = ImGui::GetCursorScreenPos();
                ImGui::GetWindowDrawList()->AddRectFilled(swp, {swp.x + sw, swp.y + sw}, rp.colour);
                ImGui::Dummy({sw, sw});
                ImGui::SameLine();
                ImGui::TextUnformatted(rp.name);

                ImGui::TableSetColumnIndex(1); ImGui::Text("%.1f", mkt.supply[r]);
                ImGui::TableSetColumnIndex(2); ImGui::Text("%.1f", mkt.demand[r]);

                // Price coloured by its move against the base: above base reads as
                // a gain (sold dear), below as a loss. Neutral until the first
                // economy tick moves prices off base_price.
                ImGui::TableSetColumnIndex(3);
                const float move = mkt.price[r] - mkt.base_price[r];
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(value_colour(move)),
                                   "%.2f", mkt.price[r]);

                ImGui::TableSetColumnIndex(4); ImGui::Text("%.2f", mkt.base_price[r]);
            }
            ImGui::EndTable();
        }
        break; // one market per body
    }
    if (!any_market)
        ImGui::TextDisabled("No market.");

    ImGui::End();
}

} // namespace ui
