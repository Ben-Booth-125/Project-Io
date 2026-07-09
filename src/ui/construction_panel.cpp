#include "construction_panel.hpp"

#include <algorithm>
#include <cmath>   // std::ceil (construction ETA, BL-095)
#include <cstdio>  // std::snprintf (buildings-table row ids, BL-143)
#include <string>  // status strings (BL-095)

#include "foldout_column.hpp" // shell fold-out column host (BL-122)
#include "presentation.hpp"
#include "ui_state.hpp"

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

// --- Analog construction status (BL-095 task E) ------------------------------
// Construction is durative *and* material-gated: each economy tick a building under
// construction advances by a RATE in [0,1] set by how much of its per-tick material
// need the local market can supply (economy_system.cpp § run_construction). These
// mirror that formula (const-read only) so the management view can show the same
// analog rate / ETA / paused status the Selection front door does. (Kept a local copy
// rather than shared — the UI has no common header seam to hang it on.)
float construction_rate(const world& w, const recipe_registry& reg,
                        building_type type, entity_id tile)
{
    const building_economics& econ = reg.economics(type);
    const float duration = econ.build_duration_ticks;
    if (duration <= 0.0f)
        return 1.0f; // instant build — never material-gated

    const market_component* m = nullptr;
    const entity_id mid = market_for_tile(w, tile);
    if (mid != null_entity)
    {
        const auto mit = w.markets.find(mid);
        if (mit != w.markets.end())
            m = &mit->second;
    }

    float rate = 1.0f;
    for (std::size_t r = 0; r < resource_count; ++r)
    {
        const float need = econ.resource_build_cost[r] / duration;
        if (need <= 0.0f)
            continue;
        const float avail = m ? m->supply[r] : 0.0f;
        rate = std::min(rate, avail / need);
    }
    rate = std::clamp(rate, 0.0f, 1.0f);

    const float max_stretch = reg.construction().max_stretch;
    const float pause_below  = (max_stretch > 1.0f) ? (1.0f / max_stretch) : 0.0f;
    if (rate < pause_below)
        rate = 0.0f; // paused: even the max-stretched rate can't be supplied
    return rate;
}

// Compact tick-count label; a Tick is ~3 months, so 4 ticks make a year (BL-095).
std::string ticks_label(int ticks)
{
    std::string s = std::to_string(ticks) + (ticks == 1 ? " tick" : " ticks");
    if (ticks >= 4)
        s += " (~" + std::to_string(ticks / 4) + " yr)";
    return s;
}

// Human status for a build rate + the whole ticks of work left.
std::string construction_status(float rate, int ticks_left)
{
    if (rate <= 0.0f)
        return "Paused - market can't supply materials";
    if (rate >= 1.0f)
        return "Building... ~" + ticks_label(ticks_left);
    const int eta = static_cast<int>(std::ceil(static_cast<float>(ticks_left) / rate));
    return "Building... ~" + ticks_label(eta) + " (materials scarce)";
}

// Colour for an analog build status: red paused, amber scarce, green on-schedule.
ImVec4 construction_status_colour(float rate)
{
    return (rate <= 0.0f) ? ImVec4{0.90f, 0.55f, 0.55f, 1.0f}   // paused
         : (rate < 1.0f)  ? ImVec4{0.85f, 0.75f, 0.45f, 1.0f}   // materials scarce
                          : ImVec4{0.55f, 0.80f, 0.55f, 1.0f};  // on schedule
}

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
// Inline detail under the Buildings tab (BL-143 redesign, folded in from the
// former standalone "Manage" tab): answers "how do I configure the thing I
// have" for whichever row the player selected in the buildings table above.
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

    // BL-095 task E: while under construction, the analog build status — rate / ETA /
    // paused — recomputed the same way economy_system.cpp § run_construction paces it
    // (per-tick material need vs the local market's recent supply). Replaces the old
    // "instant or nothing" read now that construction is durative + material-gated.
    if (b.ticks_remaining > 0)
    {
        const float rate = construction_rate(w, reg, b.type, b.tile);
        ImGui::Spacing();
        ImGui::SeparatorText("Under construction");
        ImGui::TextColored(construction_status_colour(rate), "%s",
                           construction_status(rate, b.ticks_remaining).c_str());
    }

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
// --- Buildings tab (BL-143) ---------------------------------------------------
// Answers "what do I own, and how's it doing?" — every player building in one
// table (name/type, body/tile, workforce, per-quarter profit, status), row
// selection driving the same selected-entity idiom used elsewhere (e.g.
// corporation_panel.cpp's roster table); the inline detail below reuses
// draw_selected_section for whichever building that leaves selected. "Group
// policies" is a stub only — same disabled-TODO treatment as BL-142.
void draw_buildings_tab(world& w, const recipe_registry& reg,
                        const economy_report& report, ui_state& state)
{
    ImGui::SeparatorText("Buildings");

    ImGui::TextDisabled("Group policies");
    ImGui::TextDisabled("(not implemented)");
    ImGui::Spacing();

    constexpr ImGuiTableFlags table_flags =
        ImGuiTableFlags_BordersOuter |
        ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_SizingStretchProp |
        ImGuiTableFlags_ScrollY;

    const bool has_table = ImGui::BeginTable("##buildings", 5, table_flags, {0.0f, 220.0f});
    if (has_table)
    {
        ImGui::TableSetupColumn("Building",   ImGuiTableColumnFlags_WidthStretch, 2.0f);
        ImGui::TableSetupColumn("Body/Tile",  ImGuiTableColumnFlags_WidthStretch, 1.8f);
        ImGui::TableSetupColumn("Workforce",  ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Profit/qtr", ImGuiTableColumnFlags_WidthStretch, 1.2f);
        ImGui::TableSetupColumn("Status",     ImGuiTableColumnFlags_WidthStretch, 1.2f);
        ImGui::TableHeadersRow();

        bool any = false;
        for (const auto& [id, b] : w.buildings)
        {
            if (!is_player_owned(w, id))
                continue;
            any = true;

            const building_profit profit = estimate_building_profit(w, reg, report, id);

            bool idle = b.decommissioned;
            if (!idle)
            {
                for (const building_report& br : report.buildings)
                {
                    if (br.building == id) { idle = br.idle; break; }
                }
            }
            const char* status = b.decommissioned ? "Decommissioned" : (idle ? "Idle" : "Active");

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            const bool selected = (state.selected_entity == id);
            char row_label[64];
            std::snprintf(row_label, sizeof(row_label), "%s##bld%llu",
                          building_type_name(b.type), static_cast<unsigned long long>(id));
            if (ImGui::Selectable(row_label, selected, ImGuiSelectableFlags_SpanAllColumns))
                state.selected_entity = id;

            ImGui::TableSetColumnIndex(1);
            const auto tit = w.tiles.find(b.tile);
            if (tit != w.tiles.end())
            {
                const auto bit = w.bodies.find(tit->second.body);
                const char* body_name = (bit != w.bodies.end()) ? bit->second.name.c_str() : "?";
                ImGui::Text("%s [%d, %d]", body_name, tit->second.grid_x, tit->second.grid_y);
            }
            else
                ImGui::TextDisabled("-");

            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%d%%", b.workforce_target);

            ImGui::TableSetColumnIndex(3);
            if (profit.has_data)
                ImGui::Text("%.1f", static_cast<double>(profit.net()));
            else
                ImGui::TextDisabled("-");

            ImGui::TableSetColumnIndex(4);
            ImGui::TextUnformatted(status);
        }
        if (!any)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextDisabled("No buildings yet.");
        }
        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    draw_selected_section(w, reg, state);
}

} // namespace

// Tabbed redesign (BL-143, following the 2026-07-06 Ben's-steer precedent):
// "each view must address one question; a menu allows navigation between
// views." Slimmed from three tabs to two — the Build front door moved to the
// tile Selection element (BL-139, draw_build_front_door in selection_panel.cpp)
// and Sell Orders moved to the Market Ledger (BL-159, a market question, not a
// building one). What remains: "what's building?" (Construction) and "what do
// I own?" (Buildings, which also folds in the former Manage detail for
// whichever row is selected). Still plain Selectable-style buttons rather than
// ImGui::BeginTabBar/TabItem — the tab-bar header mysteriously never rendered
// in this build/environment (see the prior redesign note this replaces).
void draw_construction_panel(world& w,
                             const recipe_registry& reg,
                             const economy_report& report,
                             ui_state& state,
                             bool* p_open)
{
    if (p_open && !*p_open)
        return;

    // Re-hosted into the shell fold-out column (BL-122): pinned + borderless, no floating
    // spawn. The BL-082 height-cap that kept the old floating window clear of the
    // bottom-left Selection element is gone — the column sits entirely left of Selection.
    ui::foldout_begin("Building");

    int& view = state.construction.panel_view;
    ui::nav_button("Construction", 0, view, p_open);
    ImGui::SameLine();
    ui::nav_button("Buildings", 1, view, p_open);
    ImGui::Separator();
    ImGui::Spacing();

    switch (view)
    {
        case 0:
            draw_queue_section(w, reg);
            break;
        case 1:
            draw_buildings_tab(w, reg, report, state);
            break;
        default:
            view = 0;
            break;
    }

    ui::foldout_end();
}

} // namespace ui
