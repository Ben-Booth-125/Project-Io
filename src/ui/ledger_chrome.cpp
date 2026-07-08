#include "ledger_chrome.hpp"

#include <algorithm>

namespace ui {

ImVec2 ledger_window_spawn(float disp_x)
{
    constexpr float margin = 16.0f;
    return {shell_column_width(disp_x) + margin, profile_panel_height + margin};
}

ImVec2 ledger_window_size(float disp_x, float disp_y)
{
    constexpr float margin = 8.0f;
    const ImVec2    spawn  = ledger_window_spawn(disp_x);
    return {std::min(820.0f, disp_x - spawn.x - margin),
            std::min(600.0f, disp_y - spawn.y - margin)};
}

} // namespace ui
