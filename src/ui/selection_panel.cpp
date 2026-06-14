#include "selection_panel.hpp"

#include "entity_summary.hpp"
#include "presentation.hpp"
#include "selection.hpp"
#include "view_nav.hpp"

#include <imgui.h>

namespace ui {

namespace {

// Headline label for a selected entity. Bodies carry a name; the other kinds
// have none, so their kind doubles as the title and the stat block supplies the
// distinguishing detail (coordinates, host body, etc.).
const char* selection_title(const world& w, selection_kind kind, entity_id id)
{
    switch (kind)
    {
        case selection_kind::body:     return w.bodies.at(id).name.c_str();
        case selection_kind::building: return building_type_name(w.buildings.at(id).type);
        case selection_kind::tile:     return "Tile";
        case selection_kind::market:   return "Market";
        case selection_kind::unit:     return "Unit";
        case selection_kind::none:     return "Nothing";
    }
    return "?";
}

// Dispatch to the matching shared content builder (entity_summary.hpp).
void draw_summary(const world& w, selection_kind kind, entity_id id)
{
    switch (kind)
    {
        case selection_kind::body:     draw_body_summary(w, id);     break;
        case selection_kind::tile:     draw_tile_summary(w, id);     break;
        case selection_kind::building: draw_building_summary(w, id); break;
        case selection_kind::market:   draw_market_summary(w, id);   break;
        case selection_kind::unit:     draw_unit_summary(w, id);     break;
        case selection_kind::none:     break;
    }
}

} // namespace

void draw_selection_panel(const world& w, ui_state& ui, float left_x, float bottom_y)
{
    const selection_kind kind = selection_kind_of(w, ui.selected_entity);

    // Hidden when nothing valid is selected, or when the player dismissed this
    // exact selection (close hides until the next selection re-shows it).
    if (kind == selection_kind::none)
        return;
    if (ui.selected_entity == ui.selection_hidden_for)
        return;

    ImGui::SetNextWindowPos({left_x, bottom_y}, ImGuiCond_Always, {0.0f, 1.0f});
    ImGui::SetNextWindowSize({320.0f, 0.0f}, ImGuiCond_Always); // fixed width, auto height
    ImGui::SetNextWindowBgAlpha(0.85f);
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar            |
        ImGuiWindowFlags_NoResize              |
        ImGuiWindowFlags_NoMove                |
        ImGuiWindowFlags_NoCollapse            |
        ImGuiWindowFlags_NoNav                 |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoSavedSettings;
    ImGui::Begin("##selection_info", nullptr, flags);

    // --- Header: title + kind on the left, 'go to' and close on the right ---
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection),
                       "%s", selection_title(w, kind, ui.selected_entity));
    ImGui::SameLine();
    ImGui::TextDisabled("%s", selection_kind_name(kind));

    // Right-align the two square buttons against the inner edge.
    const float        btn   = ImGui::GetFrameHeight();
    const ImGuiStyle&  style = ImGui::GetStyle();
    const float        right = ImGui::GetWindowWidth() - style.WindowPadding.x;
    ImGui::SameLine(right - 2.0f * btn - style.ItemSpacing.x);

    // 'Go to' — same effect as a double-click on the selection. focus_on_entity
    // resolves it to the most informative view (or, later, a ledger).
    if (ImGui::Button(">", {btn, btn}))
        focus_on_entity(w, ui, ui.selected_entity);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Go to");
    ImGui::SameLine();

    // Close — hides until the next selection; the panel is not destroyed.
    if (ImGui::Button("x", {btn, btn}))
        ui.selection_hidden_for = ui.selected_entity;
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Close");

    ImGui::Separator();

    // --- Body: polymorphic content for the selected kind ---
    draw_summary(w, kind, ui.selected_entity);

    ImGui::End();
}

} // namespace ui
