#include "hover_card.hpp"

#include <imgui_internal.h>

namespace ui {

void draw_hover_card(ImVec2 cursor, int hover_ticks,
                     const std::function<void()>& content,
                     float dwell_fraction)
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
    {
        content();

        // Dwell-to-open indicator (BL-200): a thin bar that fills as the pointer
        // holds still, signalling "keep holding to open the card". Shown only
        // mid-dwell — not before it starts (0) and not once it completes (≥1, the
        // frame the card opens). It lives here in the tooltip, never in the card
        // header (Ben, 2026-07-23).
        if (dwell_fraction > 0.0f && dwell_fraction < 1.0f)
        {
            ImGui::Spacing();
            ImGui::ProgressBar(dwell_fraction, { -1.0f, 3.0f }, "");
        }
    }
    ImGui::End();

    ImGui::PopStyleVar(2);
}

} // namespace ui
