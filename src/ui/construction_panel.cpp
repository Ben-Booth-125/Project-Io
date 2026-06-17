#include "construction_panel.hpp"

#include <algorithm>

#include "ledger_chrome.hpp" // shared ledger-window size + spawn anchor
#include "presentation.hpp"
#include "ui_state.hpp"

#include "world/components.hpp"
#include "world/placement_rules.hpp"
#include "world/recipe_registry.hpp"

#include <imgui.h>

namespace ui {

namespace {

// --- Queue overview table ----------------------------------------------------
// Section 1: all active construction items across all bodies. Progress cell is
// colour-coded: dim (<25%) grey, mid (25–75%) yellow, high (>75%) green.
// The construction queue data model is not yet authored (BL-029 precedes the
// queue backend); until that lands, this always shows the empty-state message.
void draw_queue_section(const world& /*w*/)
{
    if (!ImGui::CollapsingHeader("Queue", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    // When the queue backend lands, iterate the queue here.
    // For now the queue is always empty — show the placeholder.
    const bool any_items = false;

    if (!any_items)
    {
        ImGui::TextDisabled("No active construction.");
        return;
    }

    constexpr ImGuiTableFlags table_flags =
        ImGuiTableFlags_BordersOuter |
        ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_SizingStretchProp;

    if (!ImGui::BeginTable("##construction_queue", 5, table_flags))
        return;

    ImGui::TableSetupColumn("Body",             ImGuiTableColumnFlags_WidthStretch, 1.5f);
    ImGui::TableSetupColumn("Building",         ImGuiTableColumnFlags_WidthStretch, 2.0f);
    ImGui::TableSetupColumn("Progress",         ImGuiTableColumnFlags_WidthStretch, 1.5f);
    ImGui::TableSetupColumn("Est. ticks left",  ImGuiTableColumnFlags_WidthStretch, 1.5f);
    ImGui::TableSetupColumn("Cost remaining",   ImGuiTableColumnFlags_WidthStretch, 1.5f);
    ImGui::TableHeadersRow();

    // Placeholder loop — replace with real queue iteration when the backend lands.
    // Example shape:
    //   for (const auto& item : w.construction_queue) {
    //       const float pct = item.progress_0_to_1;
    //       ImGui::TableNextRow();
    //       ImGui::TableSetColumnIndex(0);
    //       ImGui::TextUnformatted(w.bodies.at(item.body).name.c_str());
    //       ImGui::TableSetColumnIndex(1);
    //       ImGui::TextUnformatted(building_type_name(item.type));
    //       ImGui::TableSetColumnIndex(2);
    //       ImU32 cell_colour;
    //       if      (pct < 0.25f) cell_colour = IM_COL32(80, 80, 80, 160);
    //       else if (pct < 0.75f) cell_colour = IM_COL32(200, 180, 0, 160);
    //       else                  cell_colour = IM_COL32(0, 160, 80, 160);
    //       ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, cell_colour);
    //       ImGui::Text("%.0f%%", pct * 100.0f);
    //       ImGui::TableSetColumnIndex(3);
    //       ImGui::Text("%d", item.ticks_remaining);
    //       ImGui::TableSetColumnIndex(4);
    //       ImGui::Text("%.1f", item.cost_remaining);
    //   }

    ImGui::EndTable();
}

// --- Build section -----------------------------------------------------------
void draw_build_section(const recipe_registry& reg, ui_state& state)
{
    if (!ImGui::CollapsingHeader("Build", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    auto arm = [&state](building_type type) {
        state.construction.active = true;
        state.construction.type   = type;
    };

    if (ImGui::Button("Extraction Site"))
        arm(building_type::extraction_site);
    ImGui::SameLine();
    if (ImGui::Button("Processing Facility"))
        arm(building_type::processing_facility);
    ImGui::SameLine();
    if (ImGui::Button("Port"))
        arm(building_type::port);

    if (state.construction.type == building_type::extraction_site)
    {
        ImGui::Spacing();
        ImGui::TextDisabled("Extraction target");
        for (const resource_type r : placement_rules::k_extractable)
        {
            const bool selected = (state.construction.target == r);
            if (ImGui::Selectable(resource_name(r), selected, 0, ImVec2(160.0f, 0.0f)))
                state.construction.target = r;
        }
    }

    ImGui::Spacing();

    if (state.construction.active)
    {
        if (state.construction.type == building_type::extraction_site)
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::pinned),
                "Placing: %s -> %s",
                building_type_name(state.construction.type),
                resource_name(state.construction.target));
        else
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::pinned),
                "Placing: %s", building_type_name(state.construction.type));

        // Show cost: cash + any resource materials (BL-044).
        const building_economics& eco = reg.economics(state.construction.type);
        ImGui::Text("Cost: %.0f cr", static_cast<double>(eco.build_cost));
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            if (eco.resource_build_cost[r] > 0.0f)
                ImGui::Text("  + %.0f %s",
                            static_cast<double>(eco.resource_build_cost[r]),
                            resource_name(static_cast<resource_type>(r)));
        }
        if (!state.construction.last_message.empty())
            ImGui::TextColored({1.0f, 0.4f, 0.4f, 1.0f}, "%s",
                               state.construction.last_message.c_str());

        if (ImGui::Button("Cancel"))
            state.construction.active = false;

        ImGui::TextDisabled("Click a tile on the surface to build it.");
        ImGui::TextDisabled("(Or use 'Build here' on a selected tile.)");
    }
    else
    {
        ImGui::TextDisabled("Choose a building above to begin placing.");
    }
}

// --- Selected-building section -----------------------------------------------
// Resolves the building whose tile is the current selection and shows its config,
// plus live management controls: workforce slider, recipe selector, decommission.
void draw_selected_section(world& w, const recipe_registry& reg, const ui_state& state)
{
    if (!ImGui::CollapsingHeader("Selected building", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    // Find the building sitting on the selected tile (buildings key on their own
    // entity id, not their tile, so we scan for tile == selected_entity).
    building_component* found = nullptr;
    for (auto& [id, bld] : w.buildings)
    {
        if (bld.tile == state.selected_entity)
        {
            found = &bld;
            break;
        }
    }

    if (found == nullptr)
    {
        ImGui::TextDisabled("Select a building's tile to manage it.");
        return;
    }

    building_component&        b   = *found;
    const building_economics&  eco = reg.economics(b.type);
    const recipe*              rcp = reg.get_recipe(b.recipe);

    ImGui::Text("Type: %s", building_type_name(b.type));

    if (b.type == building_type::extraction_site)
        ImGui::Text("Target: %s", resource_name(b.target_resource));
    else
        ImGui::TextDisabled("Target: -");

    if (rcp != nullptr && !rcp->name.empty())
        ImGui::Text("Recipe: %s", rcp->name.c_str());
    else
        ImGui::TextDisabled("Recipe: -");

    ImGui::Text("Build cost: %.1f", eco.build_cost);
    ImGui::Text("Maintenance: %.1f / tick", eco.maintenance);

    ImGui::Spacing();
    ImGui::SeparatorText("Management");

    // --- Workforce slider ---------------------------------------------------
    // Shows and edits the player's workforce target (0–200 % of nominal).
    ImGui::Text("Workforce: %d%%", b.workforce_target);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160.0f);
    ImGui::SliderInt("Workforce##bld", &b.workforce_target, 0, 200);
    b.workforce_target = std::clamp(b.workforce_target, 0, 200);

    // --- Recipe selector ----------------------------------------------------
    // Shown only for buildings that have more than one recipe available.
    const int n_recipes = reg.recipe_count(b.type);
    if (n_recipes > 1)
    {
        b.active_recipe_index = std::clamp(b.active_recipe_index, 0, n_recipes - 1);
        const recipe& cur_rcp = reg.recipe_at(b.type, b.active_recipe_index);
        const char* combo_preview = cur_rcp.name.empty() ? "-" : cur_rcp.name.c_str();
        if (ImGui::BeginCombo("Recipe##bld", combo_preview))
        {
            for (int i = 0; i < n_recipes; ++i)
            {
                const recipe& r = reg.recipe_at(b.type, i);
                const bool sel  = (i == b.active_recipe_index);
                if (ImGui::Selectable(r.name.empty() ? "-" : r.name.c_str(), sel))
                {
                    b.active_recipe_index = i;
                    b.recipe = reg.recipe_id(r.name);
                }
                if (sel)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }

    // --- Decommission button ------------------------------------------------
    ImGui::Spacing();
    if (b.decommissioned)
    {
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::negative),
                           "DECOMMISSIONED");
    }
    else
    {
        if (ImGui::Button("Decommission"))
            b.decommissioned = true;
        ImGui::SameLine();
        ImGui::TextDisabled("(stops production; material cost only)");
    }
}

// --- Sell orders (player) ----------------------------------------------------
void draw_sell_orders_section(const world& w, const recipe_registry& reg, ui_state& state)
{
    (void)reg;
    if (!ImGui::CollapsingHeader("Sell orders", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    const entity_id corp = w.player_entity;
    const entity_id body = (w.bodies.count(state.active_body) != 0) ? state.active_body
                                                                     : w.home_body;
    if (body == null_entity)
    {
        ImGui::TextDisabled("No body in view to trade on.");
        return;
    }

    const market_component* market = nullptr;
    for (const auto& [mid, mc] : w.markets)
        if (mc.body == body) { market = &mc; break; }

    ImGui::TextDisabled("Body: %s", w.bodies.at(body).name.c_str());

    bool any = false;
    for (std::size_t i = 0; i < state.sell_orders.size(); ++i)
    {
        const sell_order& o = state.sell_orders[i];
        if (o.corp != corp || o.body != body)
            continue;
        any = true;
        ImGui::PushID(static_cast<int>(i));
        ImGui::Text("%s  x%.0f  >= %.1f",
            resource_name(o.resource), o.quantity, o.floor_price);
        ImGui::SameLine();
        if (ImGui::SmallButton("Remove"))
        {
            state.sell_orders.erase(state.sell_orders.begin() + static_cast<long>(i));
            ImGui::PopID();
            break;
        }
        ImGui::PopID();
    }
    if (!any)
        ImGui::TextDisabled("No sell orders on this body.");

    if (market == nullptr)
    {
        ImGui::TextDisabled("This body has no market.");
        return;
    }

    ImGui::Separator();
    static int   add_resource = -1;
    static float add_quantity = 10.0f;
    static float add_floor    = 0.0f;

    if (add_resource < 0 || market->base_price[static_cast<std::size_t>(add_resource)] <= 0.0f)
    {
        for (std::size_t r = 0; r < resource_count; ++r)
            if (market->base_price[r] > 0.0f) { add_resource = static_cast<int>(r); break; }
    }

    const char* preview = (add_resource >= 0)
        ? resource_name(static_cast<resource_type>(add_resource)) : "-";
    if (ImGui::BeginCombo("Resource", preview))
    {
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            if (market->base_price[r] <= 0.0f)
                continue;
            const bool sel = (add_resource == static_cast<int>(r));
            if (ImGui::Selectable(resource_name(static_cast<resource_type>(r)), sel))
                add_resource = static_cast<int>(r);
        }
        ImGui::EndCombo();
    }
    ImGui::InputFloat("Quantity / tick", &add_quantity, 1.0f, 10.0f, "%.0f");
    ImGui::InputFloat("Floor price",     &add_floor,    0.1f, 1.0f,  "%.1f");
    if (add_quantity < 0.0f) add_quantity = 0.0f;
    if (add_floor    < 0.0f) add_floor    = 0.0f;

    ImGui::BeginDisabled(add_resource < 0 || add_quantity <= 0.0f);
    if (ImGui::Button("Add sell order"))
    {
        sell_order o;
        o.corp        = corp;
        o.body        = body;
        o.resource    = static_cast<resource_type>(add_resource);
        o.quantity    = add_quantity;
        o.floor_price = add_floor;
        state.sell_orders.push_back(o);
    }
    ImGui::EndDisabled();
}

} // namespace

void draw_construction_panel(world& w,
                             const recipe_registry& reg,
                             ui_state& state,
                             bool* p_open)
{
    if (p_open && !*p_open)
        return;

    ImGui::SetNextWindowPos(ledger_window_spawn, ImGuiCond_Once);
    ImGui::SetNextWindowSize(ledger_window_size, ImGuiCond_Once);
    ImGui::Begin("Construction", p_open);

    draw_queue_section(w);
    ImGui::Spacing();
    draw_build_section(reg, state);
    ImGui::Spacing();
    draw_selected_section(w, reg, state);
    ImGui::Spacing();
    draw_sell_orders_section(w, reg, state);

    ImGui::End();
}

} // namespace ui
