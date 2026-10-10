#include "tech_tree_panel.hpp"

#include "foldout_column.hpp"

#include <imgui.h>

namespace ui {

// A pending state, not a design: the Digitisation tree panel is BL-1276's.

void draw_tech_tree_menu(ui_state& s)
{
    if (!s.show_tech_tree)
        return;
    if (ui::foldout_begin("Tech Tree"))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextWrapped("Research panel pending.");
        ImGui::PopStyleColor();
    }
    ui::foldout_end();
}

void draw_tech_tree_panel(const world& /*w*/, bool& open)
{
    if (!open)
        return;

    const foldout_rect canvas = ui::canvas_rect();
    ImGui::SetNextWindowPos({canvas.x, canvas.y}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({canvas.w, canvas.h}, ImGuiCond_Always);
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::Begin("##tech_tree_canvas", nullptr, flags);
    ImGui::TextDisabled("Research panel pending.");
    ImGui::End();
}

} // namespace ui
