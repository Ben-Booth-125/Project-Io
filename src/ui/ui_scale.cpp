#include "ui_scale.hpp"

#include "fonts.hpp"

namespace ui {

void apply_ui_scale(float font_px)
{
    detail::g_ui_scale = font_px / k_ui_base_font_px;
    reload_ui_font(font_px);

    // A fresh base each time: ScaleAllSizes multiplies what is there, so scaling the
    // live style twice would compound.
    ImGuiStyle style;
    ImGui::StyleColorsDark(&style);
    style.ScaleAllSizes(detail::g_ui_scale);
    ImGui::GetStyle() = style;
}

} // namespace ui
