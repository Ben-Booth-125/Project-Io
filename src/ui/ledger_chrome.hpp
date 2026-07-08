#pragma once

#include "profile_panel.hpp"  // profile_panel_height — anchor clear of the profile
#include "foldout_column.hpp" // shell_column_width — live, resolution-scaled column edge

#include <imgui.h>
#include <algorithm> // std::min (spawn-size clamp)

namespace ui {

/// Shared chrome for the ledger-window family (Tile Ledger, Economy, and the
/// future Market / Balance / Construction ledgers). Every ledger window opens at
/// the **same size** and the **same spawn anchor** so the family reads as one
/// consistent surface, per docs/ui/LAYOUT.md § Uniform ledger-window chrome.
/// Both are applied with `ImGuiCond_Once`, so the window stays movable/resizable
/// thereafter.

/// One shared spawn anchor — clear of the shell fold-out column (`shell_column_width`,
/// BL-122), not the stale compile-time `profile_panel_width` (superseded since BL-122;
/// see profile_panel.hpp). The window is movable after first appearance.
///
/// @param disp_x Current display width (ImGui::GetIO().DisplaySize.x).
ImVec2 ledger_window_spawn(float disp_x);

/// Shared initial size for a ledger window, clamped so it can't first-open larger
/// than the live viewport at @p disp_x / @p disp_y.
///
/// @param disp_x Current display width.
/// @param disp_y Current display height.
ImVec2 ledger_window_size(float disp_x, float disp_y);

} // namespace ui
