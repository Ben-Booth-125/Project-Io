#include "construction_panel.hpp"

#include <algorithm>

#include "foldout_column.hpp" // shell fold-out column host (BL-122)
#include "presentation.hpp"
#include "ui_state.hpp"

#include "world/components.hpp"
#include "world/placement_rules.hpp"
#include "world/recipe_registry.hpp"

#include <imgui.h>

namespace ui {

namespace {

// --- Queue overview table ----------------------------------------------------
// All active construction items across all bodies. Progress cell is
// colour-coded: dim (<25%) grey, mid (25–75%) yellow, high (>75%) green.
// The construction queue data model is not yet authored (BL-029 precedes the
// queue backend); until that lands, this always shows the empty-state message.
// Lives under the "Build" tab (same question as placing something new: "what's
// happening with my construction?"), not its own top-level section — see the
// 2026-07-06 tabbed redesign note on draw_construction_panel.
void draw_queue_section(const world& /*w*/)
{
    ImGui::SeparatorText("Queue");

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
// Answers one question: "what can I place, and what's it cost?" (the Queue
// table above answers the closely-related "what's already in flight?").
void draw_build_section(const recipe_registry& reg, ui_state& state)
{
    ImGui::SeparatorText("Place a building");

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
// Resolves the building the current selection refers to and shows its config,
// plus live management controls: workforce slider, recipe selector, decommission.
// `selected_entity` is a generic entity reference, not always a tile: normal
// tile-click selection (SELECTION.md) sets it to the tile, but committing a
// build (both the interactive click-to-build path, app.cpp's pending_tile
// handler, and the verify harness's build_first_valid) sets it directly to the
// newly-built building's own id so its Selection-panel card is right there.
// Only matching on tile == selected_entity (the prior behaviour) missed that
// second case entirely — right after placing a building, this section showed
// "Select a building's tile to manage it." even though one was just built and
// selected, forcing the player to click away and reselect the tile before they
// could reach the recipe combo (found while investigating why processing-facility
// management felt clunky, 2026-07-06 — every processor defaults to the steel
// recipe at construction, so this was blocking the very first thing a player
// would want to change).
//
// Own tab ("Manage", 2026-07-06 redesign): answers "how do I configure the
// thing I have," a different question from "what can I place" (Build tab).
void draw_selected_section(world& w, const recipe_registry& reg, const ui_state& state)
{
    building_component* found = nullptr;
    if (const auto bit = w.buildings.find(state.selected_entity); bit != w.buildings.end())
        found = &bit->second;
    else
    {
        // Fall back to tile-match for the normal "select a tile, see its building" case.
        for (auto& [id, bld] : w.buildings)
        {
            if (bld.tile == state.selected_entity)
            {
                found = &bld;
                break;
            }
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
// Own tab ("Sell Orders", 2026-07-06 redesign): "how do I sell what I make?" —
// a market question that happens to live in this window today. Candidate to
// move onto the market ledger in a later pass (flagged, not done here — moving
// it is a cross-panel question, out of scope for this session's Construction
// tabbing).
void draw_sell_orders_section(const world& w, const recipe_registry& reg, ui_state& state)
{
    (void)reg;
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

// Tabbed redesign (2026-07-06, Ben's steer): "each view must address one
// question; a menu allows navigation between views." The Construction window
// is the bounded space for every construction question, but the four sections
// used to sit stacked in one scrolling column, mixing "what can I build"
// (Build+Queue), "how do I configure what I have" (Manage), and "how do I
// sell" (Sell Orders) into a single undifferentiated view. Each now gets its
// own bounded view (draws exclusively, nothing else stacked below it); a small
// button-strip nav is the menu that switches between them (`state.construction.
// panel_view`). Built with a plain Selectable-style button row rather than
// ImGui::BeginTabBar/TabItem — the tab-bar header mysteriously never rendered
// in this build/environment (BeginTabBar/BeginTabItem reported success and the
// active tab's body drew correctly, but the clickable header strip itself was
// never visible in any capture, even in an isolated two-tab test with no other
// content) while ordinary buttons render reliably everywhere else in this
// codebase, so this sidesteps the mystery rather than chasing it further.
void draw_construction_panel(world& w,
                             const recipe_registry& reg,
                             ui_state& state,
                             bool* p_open)
{
    if (p_open && !*p_open)
        return;

    // Re-hosted into the shell fold-out column (BL-122): pinned + borderless, no floating
    // spawn. The BL-082 height-cap that kept the old floating window clear of the
    // bottom-left Selection element is gone — the column sits entirely left of Selection.
    ui::foldout_begin("Construction");

    int& view = state.construction.panel_view;
    auto nav_button = [&](const char* label, int id) {
        const bool active = (view == id);
        if (active)
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        if (ImGui::Button(label))
            view = id;
        if (active)
            ImGui::PopStyleColor();
    };
    nav_button("Build", 0);
    ImGui::SameLine();
    nav_button("Manage", 1);
    ImGui::SameLine();
    nav_button("Sell Orders", 2);
    ImGui::Separator();
    ImGui::Spacing();

    switch (view)
    {
        case 0:
            draw_build_section(reg, state);
            ImGui::Spacing();
            draw_queue_section(w);
            break;
        case 1:
            draw_selected_section(w, reg, state);
            break;
        case 2:
            draw_sell_orders_section(w, reg, state);
            break;
        default:
            view = 0;
            break;
    }

    ui::foldout_end();
}

} // namespace ui
