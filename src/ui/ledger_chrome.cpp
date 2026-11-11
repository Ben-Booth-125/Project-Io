#include "ledger_chrome.hpp"
#include "ui_scale.hpp" // BL-1495: px, px_const

#include <algorithm>

namespace ui {

ImVec2 ledger_window_spawn(float disp_x)
{
    const float margin = ui::px(16.0f);
    return {shell_column_width(disp_x) + margin, profile_panel_height + margin};
}

ImVec2 ledger_window_size(float disp_x, float disp_y)
{
    const float margin = ui::px(8.0f);
    const ImVec2    spawn  = ledger_window_spawn(disp_x);
    return {std::min(820.0f, disp_x - spawn.x - margin),
            std::min(ui::px(600.0f), disp_y - spawn.y - margin)};
}

} // namespace ui
