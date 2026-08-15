#include "construction_panel.hpp"

#include <algorithm>
#include <cmath>   // std::ceil (construction ETA, BL-095)
#include <string>  // status strings (BL-095)

#include "foldout_column.hpp" // shell fold-out column host (BL-122)
#include "ui_state.hpp"

#include "world/budget_system.hpp"     // building_opex / compute_building_opex, body_mean_habitability (BL-143)
#include "world/economy_system.hpp"    // economy_report (BL-143 status column)
#include "world/market_clearing.hpp"   // market_for_tile (build-rate read, BL-095)
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

// Compact quarter-count label; a Tick is ~3 months, so 4 make a year (BL-095).
//
// The DISPLAY word is "qtr", not "tick" (NR-002, Ben 2026-08-01): Tick is the internal
// term for the economy step and reads as engineering to a player. It is also the less
// accurate of the two here — the step is literally a calendar quarter, which is why this
// function could already gloss it in years.
std::string ticks_label(int ticks)
{
    std::string s = std::to_string(ticks) + (ticks == 1 ? " qtr" : " qtrs");
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
    ImGui::TableSetupColumn("Est. qtrs left",  ImGuiTableColumnFlags_WidthStretch, 1.5f);
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

// Construction-only, single view (2026-08-15 rework): the former Buildings
// tab — and its inline detail (recipe, workforce, close/dismantle) — moved
// onto the building Selection card as accordion pages (Method already lived
// there; Workforce and Lifecycle joined it, selection_panel.cpp). What's
// left here answers one question only, "what's building?" — the queue view,
// so the tab switcher (Construction/Buildings) and its selection-driven
// auto-focus (the old panel_view snap-to-Buildings edge trigger) are both
// gone with it; there is nothing left to switch between.
void draw_construction_panel(world& w,
                             const recipe_registry& reg,
                             const economy_report&,
                             ui_state&,
                             bool* p_open)
{
    if (p_open && !*p_open)
        return;

    // Re-hosted into the shell fold-out column (BL-122): pinned + borderless, no floating
    // spawn. The BL-082 height-cap that kept the old floating window clear of the
    // bottom-left Selection element is gone — the column sits entirely left of Selection.
    ui::foldout_begin("Construction");
    draw_queue_section(w, reg);
    ui::foldout_end();
}

} // namespace ui
