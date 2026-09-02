#include "construction_panel.hpp"

#include <algorithm>
#include <cmath>   // std::ceil (construction ETA, BL-095)
#include <string>  // status strings (BL-095)

#include "foldout_column.hpp"  // shell fold-out column host (BL-122) + nav_button (the tab strip)
#include "format.hpp"          // fmt::signed_delta / sign_of - the roster's profit column
#include "presentation.hpp"    // building_type_name / extraction_building_name — the shared roster vocabulary
#include "selection_panel.hpp" // the build bar and the two relocated levers
#include "ui_state.hpp"

#include "world/budget_system.hpp"     // building_opex / compute_building_opex, body_mean_habitability (BL-143)
#include "world/building_profit.hpp"   // estimate_building_profit — the per-building figure the roster sums
#include "world/economy_system.hpp"    // economy_report (BL-143 status column)
#include "world/market_clearing.hpp"   // market_for_tile (build-rate read, BL-095)
#include "world/recipe_registry.hpp"

#include <cstdio>
#include <map>
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
                        building_type type, resource_type target, uint16_t recipe,
                        entity_id tile)
{
    const building_economics& econ = reg.economics(type);
    const float duration = econ.build_duration_ticks;
    if (duration <= 0.0f)
        return 1.0f; // instant build — never material-gated
    // BL-590: the material cost specific to THIS named building.
    const auto& material_cost_row = reg.resource_build_cost_for(type, target, recipe);

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
        const float need = material_cost_row[r] / duration;
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
        const building_opex opex = compute_building_opex(b, reg.economics(b.type), 1.0f, mean_hab,
                                                         reg.idle_maintenance_floor());
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
void draw_queue_section(const world& w, const recipe_registry& reg, ui_state& ui)
{
    // COLLAPSIBLE, and collapsed by default: the queue is empty most of the time,
    // and an empty queue that owns the surface is the empty room BL-176 diagnosed.
    // Closed it costs one line, and the build bar below it is what the player meets.
    // A CollapsingHeader is a toggle by construction, so the standing Toggle rule is
    // satisfied without a second control. Its open state is held in ui_state rather
    // than ImGui's own storage so it is stable and drivable by a check; the label
    // carries a `###` id so the live cost figure cannot churn the widget's identity.
    ImGui::SetNextItemOpen(ui.construction.queue_open, ImGuiCond_Always);

    const float est_cost = estimated_quarterly_construction_cost(w, reg);
    char hdr[96];
    std::snprintf(hdr, sizeof hdr, "Queue - %.0f cr / qtr###queue_section",
                  static_cast<double>(est_cost));
    const bool open = ImGui::CollapsingHeader(hdr);
    if (open != ui.construction.queue_open)
        ui.construction.queue_open = open;
    if (!open)
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


// --- The Buildings view: the player's estate, grouped by type -----------------

// One line per building inside an expanded group: its tile and its own net, with a
// press that SELECTS it. Returns true when pressed.
//
// Selecting from a row is the existing idiom, not a new one — the corporations table
// sets `selected_entity` from a row press in exactly this way. It is also the reason
// this view's empty state can be better than the Construction view's: listing the
// buildings behind a count the header has already asserted is NAVIGATION, and decides
// nothing. Ranking tiles the player could build on would have been the other thing.
bool draw_group_member_row(const world& w, entity_id id, float net, bool known,
                           bool selected, float name_w)
{
    const auto bit = w.buildings.find(id);
    if (bit == w.buildings.end())
        return false;

    char label[96];
    const auto tit = w.tiles.find(bit->second.tile);
    if (tit != w.tiles.end())
        std::snprintf(label, sizeof label, "[%d, %d]###m%u",
                      tit->second.grid_x, tit->second.grid_y, static_cast<unsigned>(id));
    else
        std::snprintf(label, sizeof label, "(off map)###m%u", static_cast<unsigned>(id));

    const bool pressed = ImGui::Selectable(label, selected, 0, {name_w, 0.0f});

    // The member's own net, right-aligned against the group total above it: the
    // column exists so the header's total is VISIBLY the sum of these, not a figure
    // the player has to take on trust.
    ImGui::SameLine();
    if (known)
    {
        const auto sg = fmt::sign_of(static_cast<double>(net));
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(value_colour(sg)),
                           "%s", fmt::signed_delta(static_cast<double>(net)).c_str());
    }
    else if (bit->second.ticks_remaining > 0)
        ImGui::TextDisabled("building");
    else
        ImGui::TextDisabled("-");

    return pressed;
}

// The levers, drawn for whichever building is selected. THE SAME FUNCTION BODIES the
// Selection card's centre used to host (declared in selection_panel.hpp) — relocated,
// never reimplemented, because two sets of controls writing `workforce_target` and
// `try_switch_recipe` is how two surfaces come to disagree.
//
// The two selectors COMPOSE: this reads `ui.selected_entity`, so a building selected
// from a row here and a building selected on the map both land on the same levers.
void draw_building_levers(world& w, const recipe_registry& reg, ui_state& ui)
{
    const entity_id sel = ui.selected_entity;
    const auto bit = w.buildings.find(sel);
    if (bit == w.buildings.end() || !is_player_owned(w, sel))
        return;

    ui.construction_ui.levers_for = sel; // what was actually drawn for, not what was asked

    ImGui::Separator();
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s",
                       building_group_name(reg, bit->second).c_str());
    if (const auto tit = w.tiles.find(bit->second.tile); tit != w.tiles.end())
    {
        ImGui::SameLine();
        ImGui::TextDisabled("[%d, %d]", tit->second.grid_x, tit->second.grid_y);
    }

    if (bit->second.ticks_remaining > 0)
    {
        // Under construction: there is no method to switch and no workforce to set
        // yet. Saying so beats drawing dials that do nothing.
        ImGui::TextDisabled("Under construction.");
        return;
    }

    draw_production_method_section(w, reg, sel); // draws nothing for a non-processing type
    ImGui::Spacing();
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "Workforce");
    draw_building_workforce_page(bit->second);
}

void draw_buildings_view(world& w, const recipe_registry& reg,
                         const economy_report& report, ui_state& ui)
{
    const std::vector<building_group> groups = player_building_groups(w, reg, report);
    if (groups.empty())
    {
        ImGui::TextDisabled("You own no buildings.");
        return;
    }

    // AT MOST ~9 rows visible, the rest reached by scrolling (Ben's "roughly 9
    // buildings with a vertical scroll"), and no more than there is to show — a
    // roster of four types should not reserve nine rows of empty column. Measured
    // off the live frame height rather than a pixel constant, so the row count
    // survives a font or display that is not the one this was written on.
    //
    // The levers sit BELOW the scroller. The scroller's height therefore depends on
    // the roster's row count, which changes when a group expands — but never on
    // which building is SELECTED, so the press that selects one cannot move the
    // thing the player was reaching for.
    int rows_shown = static_cast<int>(groups.size());
    for (const building_group& g : groups)
        if (ui.construction.buildings_expanded == g.name)
            rows_shown += static_cast<int>(g.members.size());

    const float row_h  = ImGui::GetFrameHeightWithSpacing();
    const float want_h = row_h * static_cast<float>(std::min(rows_shown, 9)) + 4.0f;
    const float roster_h = std::min(want_h,
                                    std::max(row_h * 3.0f,
                                             ImGui::GetContentRegionAvail().y * 0.60f));

    ImGui::BeginChild("##buildings_roster", {0.0f, roster_h}, false,
                      ImGuiWindowFlags_NoSavedSettings);

    // Width budget, measured off the host column rather than assumed (NR-709): the
    // net figure takes what it needs and the NAME takes the rest, so the name can
    // never be squeezed out by a fixed sibling.
    const float avail  = ImGui::GetContentRegionAvail().x;
    const float net_w  = ImGui::CalcTextSize("-000.0k").x + ImGui::GetStyle().ItemSpacing.x;
    const float name_w = std::max(40.0f, avail - net_w);

    bool published_group = false;
    for (const building_group& g : groups)
    {
        ImGui::PushID(g.name.c_str());

        const bool want_open = (ui.construction.buildings_expanded == g.name);
        ImGui::SetNextItemOpen(want_open, ImGuiCond_Always);

        // The count lives in the label; the TOTAL is drawn beside it so it can carry
        // its own sign colour. `###` keeps the widget id stable while the count moves.
        char hdr[128];
        std::snprintf(hdr, sizeof hdr, "%s  (%d)###grp", g.name.c_str(), g.count);

        const ImVec2 hp = ImGui::GetCursorScreenPos();
        const bool open = ImGui::CollapsingHeader(hdr);
        if (!published_group)
        {
            // First header's press point, published for a check to click the REAL
            // control (ui_state.hpp § construction_controls).
            ui.construction_ui.group_x = hp.x + ImGui::GetFrameHeight() * 0.5f;
            ui.construction_ui.group_y = hp.y + ImGui::GetFrameHeight() * 0.5f;
            published_group = true;
        }

        // One group open at a time — an accordion. Re-pressing the open header
        // closes it, which is the Toggle rule a header obeys by construction.
        if (open != want_open)
            ui.construction.buildings_expanded = open ? g.name : std::string{};

        ImGui::SameLine(name_w);
        {
            const auto sg = fmt::sign_of(static_cast<double>(g.total));
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(value_colour(sg)),
                               "%s", fmt::signed_delta(static_cast<double>(g.total)).c_str());
        }

        if (open)
        {
            ImGui::Indent();
            bool published_member = false;
            for (const entity_id id : g.members)
            {
                const building_profit p = estimate_building_profit(w, reg, report, id);
                const ImVec2 mp = ImGui::GetCursorScreenPos();
                const bool pressed = draw_group_member_row(
                    w, id, p.has_data ? p.net() : 0.0f, p.has_data,
                    ui.selected_entity == id,
                    std::max(40.0f, name_w - ImGui::GetStyle().IndentSpacing));
                if (!published_member)
                {
                    ui.construction_ui.member_x = mp.x + 20.0f;
                    ui.construction_ui.member_y = mp.y + ImGui::GetTextLineHeight() * 0.5f;
                    ui.construction_ui.member   = id;
                    published_member = true;
                }
                if (pressed)
                    ui.selected_entity = id;
            }
            ImGui::Unindent();
        }
        ImGui::PopID();
    }
    ImGui::EndChild();

    draw_building_levers(w, reg, ui);
}

} // namespace

std::string building_group_name(const recipe_registry& reg, const building_component& b)
{
    if (b.type == building_type::extraction_site)
    {
        // The Build door names this candidate "Farm" rather than by its resource, so
        // the roster does too — one vocabulary (presentation.hpp).
        if (b.target_resource == resource_type::agricultural_produce)
            return "Farm";
        return extraction_building_name(b.target_resource);
    }
    if (b.type == building_type::processing_facility)
    {
        // The recipe GROUP, not the recipe: "Metal Foundry" once, rather than a row
        // per method — BL-434's grouping, which is also the axis the Build door
        // collapses its processing candidates onto.
        if (const recipe* rc = reg.get_recipe(b.recipe); rc != nullptr && !rc->group.empty())
            return rc->group;
        return building_type_name(b.type);
    }
    return building_type_name(b.type);
}

std::vector<building_group> player_building_groups(const world& w,
                                                   const recipe_registry& reg,
                                                   const economy_report& report)
{
    // std::map, walked in name order — the source container (`world::buildings`) is
    // unordered, so an ordered accumulator is what makes the roster the same roster
    // on every platform and every run.
    std::map<std::string, building_group> by_name;
    for (const auto& [id, b] : w.buildings)
    {
        if (!is_player_owned(w, id))
            continue;
        const std::string name = building_group_name(reg, b);
        building_group& g = by_name[name];
        g.name = name;
        ++g.count;
        g.members.push_back(id);
        // A building the report has not priced contributes ZERO, never a guess: the
        // total must be the visible sum of the rows behind it.
        const building_profit p = estimate_building_profit(w, reg, report, id);
        if (p.has_data)
            g.total += p.net();
    }

    std::vector<building_group> out;
    out.reserve(by_name.size());
    for (auto& [key, g] : by_name)
    {
        std::sort(g.members.begin(), g.members.end(),
                  [&](entity_id a, entity_id b)
                  {
                      const building_profit pa = estimate_building_profit(w, reg, report, a);
                      const building_profit pb = estimate_building_profit(w, reg, report, b);
                      const float na = pa.has_data ? pa.net() : 0.0f;
                      const float nb = pb.has_data ? pb.net() : 0.0f;
                      if (na != nb)
                          return na > nb;
                      return a < b; // total order, so the walk is reproducible
                  });
        out.push_back(std::move(g));
    }
    return out;
}

// Two views, one construction element (Ben, 2026-08-29). Buildings is the default —
// the queue is empty most of the time and the player always owns buildings, so
// opening on the queue makes this ledger's front door an empty room (the fix BL-176
// made and the 2026-08-15 rework deleted along with the tab it lived on).
//
// The Construction view draws the collapsible queue and then the SAME build bar the
// tile Selection element's Construct button opens — that button opens this view
// rather than a second surface, so there is one build bar with two doors.
void draw_construction_panel(world& w,
                             const recipe_registry& reg,
                             const economy_report& report,
                             ui_state& ui,
                             bool* p_open)
{
    // Probe fields describe THIS frame; a frame that draws nothing publishes nothing,
    // so a stale position cannot be clicked after the surface is gone.
    ui.construction_ui.tab_x[0] = ui.construction_ui.tab_x[1] = -1.0f;
    ui.construction_ui.tab_y[0] = ui.construction_ui.tab_y[1] = -1.0f;
    ui.construction_ui.group_x  = ui.construction_ui.group_y  = -1.0f;
    ui.construction_ui.member_x = ui.construction_ui.member_y = -1.0f;
    ui.construction_ui.member   = null_entity;
    ui.construction_ui.levers_for = null_entity;

    if (p_open && !*p_open)
        return;

    // Re-hosted into the shell fold-out column (BL-122): pinned + borderless, no floating
    // spawn. The BL-082 height-cap that kept the old floating window clear of the
    // bottom-left Selection element is gone — the column sits entirely left of Selection.
    ui::foldout_begin("Construction");

    // The tab strip. `nav_button` carries the standing Toggle rule for us: re-clicking
    // the ACTIVE tab closes the whole ledger (it does not collapse to an overview),
    // and switching to the other tab is an ordinary view change.
    int& view = ui.construction.panel_view;
    {
        const float  h  = ImGui::GetFrameHeight();
        const ImVec2 p0 = ImGui::GetCursorScreenPos();
        ui::nav_button("Buildings", 1, view, p_open);
        ui.construction_ui.tab_x[1] = p0.x + ImGui::GetItemRectSize().x * 0.5f;
        ui.construction_ui.tab_y[1] = p0.y + h * 0.5f;
        ImGui::SameLine();
        const ImVec2 p1 = ImGui::GetCursorScreenPos();
        ui::nav_button("Construction", 0, view, p_open);
        ui.construction_ui.tab_x[0] = p1.x + ImGui::GetItemRectSize().x * 0.5f;
        ui.construction_ui.tab_y[0] = p1.y + h * 0.5f;
    }
    ImGui::Separator();

    // nav_button may have closed the ledger under us (the active-tab re-click). Draw
    // no body in that case — the column releases this frame, not the next.
    if (p_open == nullptr || *p_open)
    {
        if (view == 0)
        {
            draw_queue_section(w, reg, ui);
            ImGui::Spacing();
            draw_construction_ledger_body(w, reg, ui);
        }
        else
            draw_buildings_view(w, reg, report, ui);
    }

    ui::foldout_end();
}

} // namespace ui
