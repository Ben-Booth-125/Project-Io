#include "overlay.hpp"

namespace ui {

const char* overlay_mode_name(overlay_mode m)
{
    switch (m)
    {
        case overlay_mode::supply:  return "Supply routes";
        case overlay_mode::market:  return "Market";
        case overlay_mode::faction: return "Faction presence";
        default:                    return "None";
    }
}

const char* overlay_mode_short_name(overlay_mode m)
{
    switch (m)
    {
        case overlay_mode::supply:  return "Supply";
        case overlay_mode::market:  return "Market";
        case overlay_mode::faction: return "Faction";
        default:                    return "None";
    }
}

void toggle_overlay(ui_state& ui, overlay_mode m)
{
    ui.overlay = (ui.overlay == m) ? overlay_mode::none : m;
}

void draw_overlay_controls(ui_state& ui, float left_x, float bottom_y)
{
    // The three selectable lenses, in mode-bar order. overlay_mode::none is not a
    // button — clicking the active lens clears back to it (toggle_overlay).
    constexpr overlay_mode modes[3] = {
        overlay_mode::supply, overlay_mode::market, overlay_mode::faction };

    ImGui::SetNextWindowPos({left_x, bottom_y}, ImGuiCond_Always, {0.0f, 1.0f});
    ImGui::SetNextWindowBgAlpha(0.65f);
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar          |
        ImGuiWindowFlags_NoResize            |
        ImGuiWindowFlags_NoMove              |
        ImGuiWindowFlags_NoCollapse          |
        ImGuiWindowFlags_NoScrollbar         |
        ImGuiWindowFlags_NoNav               |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoSavedSettings     |
        ImGuiWindowFlags_AlwaysAutoResize;
    ImGui::Begin("##overlay_controls", nullptr, flags);

    ImGui::TextUnformatted("Lens:");
    for (overlay_mode m : modes)
    {
        ImGui::SameLine();
        const bool active = (ui.overlay == m);
        if (active)
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        if (ImGui::Button(overlay_mode_short_name(m)))
            toggle_overlay(ui, m);
        if (active)
            ImGui::PopStyleColor();
    }

    ImGui::End();
}

void draw_canvas_overlay(const world& w, const ui_state& ui, canvas_level level,
                         ImVec2 origin, ImVec2 size, ImDrawList* dl)
{
    (void)w;      // overlay data source — used once lenses draw real geometry
    (void)level;  // lenses differ per rung — switched on here by later layers
    (void)origin; // lens geometry is positioned in canvas space here by later layers
    (void)size;
    (void)dl;

    if (ui.overlay == overlay_mode::none)
        return;

    // No on-canvas geometry yet. The active lens is named by the bottom overlay
    // control strip (draw_overlay_controls); this pass is the extension point
    // where Layer 5 supply routes and later lenses draw real geometry, keyed by
    // ui.overlay and level.
}

} // namespace ui
