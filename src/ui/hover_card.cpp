#include "hover_card.hpp"

#include <imgui_internal.h>

namespace ui {

void draw_hover_card(ImVec2 anchor,
                     const std::function<void()>& content,
                     ImVec2* out_min,
                     ImVec2* out_max)
{
    constexpr float kMaxWidth  = 200.0f;
    constexpr float kPad       = 6.0f;
    constexpr float kRounding  = 4.0f;
    constexpr float kYOffset   = 18.0f; // above the anchor point

    // Pinned to the anchor the card was summoned at, NOT the live cursor
    // (BL-228): a card that slides with the pointer cannot be read to the end of
    // a line, and offers nowhere to move the pointer TO in order to dismiss it.
    const ImVec2 pos = { anchor.x + 4.0f, anchor.y - kYOffset };

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

    // Report the rect the card actually occupied so the caller can hit-test the
    // pointer against it next frame and dismiss on exit. Read while the window is
    // still current — the auto-resized height is only known here, after the
    // content has been laid out.
    const ImVec2 wmin = ImGui::GetWindowPos();
    const ImVec2 wsz  = ImGui::GetWindowSize();
    if (out_min) *out_min = wmin;
    if (out_max) *out_max = { wmin.x + wsz.x, wmin.y + wsz.y };

    ImGui::End();

    ImGui::PopStyleVar(2);
}

} // namespace ui
