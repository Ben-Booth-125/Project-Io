#include "canvas_scale.hpp"

#include <algorithm>
#include <cstdio>

namespace ui {

void draw_scale_zoom_overlay(const char* id_suffix, ImVec2 origin, ImVec2 size,
                             float px_per_au, float& zoom, float zoom_min, float zoom_max)
{
    // Fixed-width scale bar (8% of the canvas width); the AU distance it spans is
    // dynamic, derived from the live pixels-per-AU. Guard against a zero/negative
    // px_per_au (degenerate framing) so the label stays finite.
    const float bar_px = size.x * 0.08f;
    const float bar_au = (px_per_au > 0.0f) ? bar_px / px_per_au : 0.0f;

    const float slider_w = std::clamp(size.x * 0.12f, 120.0f, 240.0f);

    // Centre the scale bar on the canvas; the slider sits to its right. The
    // window's left edge is placed so the bar (the leading element) is
    // screen-centred, with the slider offset rightward beyond it.
    char window_id[64];
    std::snprintf(window_id, sizeof(window_id), "##scale_%s", id_suffix);

    ImGui::SetNextWindowPos({origin.x + size.x * 0.5f - bar_px * 0.5f,
                             origin.y + size.y - 8.0f},
                            ImGuiCond_Always, {0.0f, 1.0f});
    ImGui::SetNextWindowBgAlpha(0.0f); // no fill
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0.0f, 0.0f});
    constexpr ImGuiWindowFlags scale_flags =
        ImGuiWindowFlags_NoTitleBar          |
        ImGuiWindowFlags_NoResize            |
        ImGuiWindowFlags_NoMove              |
        ImGuiWindowFlags_NoCollapse          |
        ImGuiWindowFlags_NoScrollbar         |
        ImGuiWindowFlags_NoNav               |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoSavedSettings     |
        ImGuiWindowFlags_AlwaysAutoResize;
    ImGui::Begin(window_id, nullptr, scale_flags);

    // --- Scale bar: a horizontal rule with end ticks and a centred label.
    const ImVec2 bar_origin = ImGui::GetCursorScreenPos();
    const float  bar_h      = ImGui::GetTextLineHeight() + 8.0f;
    ImGui::Dummy({bar_px, bar_h});

    ImDrawList* wdl = ImGui::GetWindowDrawList();
    const ImU32 bar_col = IM_COL32(200, 205, 220, 220);
    const float by  = bar_origin.y + bar_h - 3.0f; // bar baseline
    const float bx0 = bar_origin.x;
    const float bx1 = bar_origin.x + bar_px;
    wdl->AddLine({bx0, by}, {bx1, by}, bar_col, 1.5f);
    wdl->AddLine({bx0, by - 5.0f}, {bx0, by}, bar_col, 1.5f);
    wdl->AddLine({bx1, by - 5.0f}, {bx1, by}, bar_col, 1.5f);

    char bar_label[32];
    std::snprintf(bar_label, sizeof(bar_label), "%.2f AU", bar_au);
    const ImVec2 lsz = ImGui::CalcTextSize(bar_label);
    wdl->AddText({bar_origin.x + (bar_px - lsz.x) * 0.5f, bar_origin.y}, bar_col, bar_label);

    // --- Zoom slider: the zoom factor itself, logarithmic so the range feels
    // even across the track. Left end is zoom_min (zoomed out), right end is
    // zoom_max (zoomed in) — dragging right zooms in. No value text; the scale
    // bar already reports the distance.
    ImGui::SameLine();
    ImGui::SetNextItemWidth(slider_w);
    float z = std::clamp(zoom, zoom_min, zoom_max);
    if (ImGui::SliderFloat("##zoom", &z, zoom_min, zoom_max, "",
                           ImGuiSliderFlags_Logarithmic))
    {
        zoom = z;
    }

    ImGui::End();
    ImGui::PopStyleVar(2);
}

} // namespace ui
