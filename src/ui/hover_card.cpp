#include "hover_card.hpp"

#include <imgui_internal.h>

namespace ui {

void draw_hover_card(ImVec2 cursor, int hover_ticks,
                     const std::function<void()>& content)
{
    if (hover_ticks < kHoverDelay)
        return;

    constexpr float kMaxWidth  = 200.0f;
    constexpr float kPad       = 6.0f;
    constexpr float kRounding  = 4.0f;
    constexpr float kYOffset   = 18.0f; // above the cursor tip

    // Measure the content to size the window before rendering.
    // ImGui child windows auto-size to their content when using
    // ImGuiWindowFlags_AlwaysAutoResize; we position after measuring.
    const ImVec2 pos = { cursor.x + 4.0f, cursor.y - kYOffset };

    ImGui::SetNextWindowPos(pos, ImGuiCond_Always, {0.0f, 1.0f});
    ImGui::SetNextWindowSize({ kMaxWidth, 0.0f }); // 0 height = auto
    ImGui::SetNextWindowBgAlpha(0.82f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar
                           | ImGuiWindowFlags_NoResize
                           | ImGuiWindowFlags_NoScrollbar
                           | ImGuiWindowFlags_NoInputs
                           | ImGuiWindowFlags_NoMove
                           | ImGuiWindowFlags_NoSavedSettings
                           | ImGuiWindowFlags_NoFocusOnAppearing
                           | ImGuiWindowFlags_NoNav
                           | ImGuiWindowFlags_AlwaysAutoResize
                           | ImGuiWindowFlags_Tooltip; // keeps it above other windows

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, kRounding);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  { kPad, kPad });

    if (ImGui::Begin("##hover_card", nullptr, flags))
        content();
    ImGui::End();

    ImGui::PopStyleVar(2);
}

} // namespace ui
