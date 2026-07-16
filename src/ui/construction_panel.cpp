#include "construction_panel.hpp"

#include <algorithm>
#include <cmath>   // std::ceil (construction ETA, BL-095)
#include <cstdio>  // std::snprintf (buildings-table row ids, BL-143)
#include <string>  // status strings (BL-095)

#include "foldout_column.hpp" // shell fold-out column host (BL-122)
#include "icons.hpp"          // production-method pip glyph (building-management shell)
#include "presentation.hpp"
#include "ui_state.hpp"

#include <cfloat>  // FLT_MAX (PlotLines auto-scale)

#include "world/budget_system.hpp"     // building_opex / compute_building_opex, body_mean_habitability (BL-143)
#include "world/building_profit.hpp"   // estimate_building_profit (BL-143 Buildings tab)
#include "world/components.hpp"
#include "world/economy_system.hpp"    // economy_report (BL-143 status column)
#include "world/market_clearing.hpp"   // market_for_tile (build-rate read, BL-095)
#include "world/placement_rules.hpp"
#include "world/recipe_registry.hpp"

#include <imgui.h>

namespace ui {

namespace {

// --- Estimated quarterly cost of in-progress construction (BL-143) ----------
// Sums compute_building_opex's maintenance + wages (budget_system.hpp — the
// same formula the live budget loop and building_profit use, so no drift)
// over every player building still under construction. One economy tick is
// one quarter (k_ticks_per_year = 4), so a single tick's opex *is* the
// per-quarter figure — no further scaling needed.
float estimated_quarterly_construction_cost(const world& w, const recipe_registry& reg)
{
    float total = 0.0f;
    for (const auto& [id, b] : w.buildings)
    {
        if (b.ticks_remaining <= 0)
            continue; // completed — not "in progress"
        if (!is_player_owned(w, id))
            continue;

        const entity_id body = (w.tiles.count(b.tile) != 0) ? w.tiles.at(b.tile).body : null_entity;
        const float mean_hab = (body != null_entity) ? body_mean_habitability(w, body) : 1.0f;
        const building_opex opex = compute_building_opex(b, reg.economics(b.type), 1.0f, mean_hab);
        total += opex.maintenance + opex.wages;
    }
    return total;
}

// --- Queue overview table ----------------------------------------------------
// All active construction items across all bodies. Progress cell is
// colour-coded: dim (<25%) grey, mid (25–75%) yellow, high (>75%) green.
// The construction queue data model is not yet authored (BL-029 precedes the
// queue backend); until that lands, this always shows the empty-state message.
// Own tab ("Construction", BL-143 rename): "what's happening with my
// construction?" — now paired with an estimated per-quarter cost of every
// in-progress build.
void draw_queue_section(const world& w, const recipe_registry& reg)
{
    ImGui::SeparatorText("Queue");

    const float est_cost = estimated_quarterly_construction_cost(w, reg);
    ImGui::Text("Estimated cost: %.1f / quarter", static_cast<double>(est_cost));
    ImGui::Spacing();

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


} // namespace

// The Construction ledger — ONE view, one question: "what's building?" (Ben's
// 2026-07-16 steer). It was a two-tab panel; the "Buildings" tab (a roster table plus
// the selected building's management detail) is gone — a single building's detail is a
// *selection* question, so it now lives in the Selection element
// (draw_building_selection in selection_panel.cpp), reached by selecting the building
// itself rather than hunting a table row. The Build front door moved to the tile
// Selection element earlier (BL-139) and Sell Orders to the Market Ledger (BL-159).
//
// The single tab is kept as a nav_button rather than dropped: it carries the toggle
// rule (io-standing-rules.md) — clicking the active tab closes the ledger. Still plain
// Selectable-style buttons rather than ImGui::BeginTabBar/TabItem — the tab-bar header
// mysteriously never rendered in this build/environment.
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
    view = 0; // single view; the field remains so nav_button's toggle rule still reads
    ui::nav_button("Construction", 0, view, p_open);
    ImGui::Separator();
    ImGui::Spacing();

    draw_queue_section(w, reg);

    ui::foldout_end();
}

} // namespace ui
