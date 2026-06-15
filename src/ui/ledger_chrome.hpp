#pragma once

#include "profile_panel.hpp" // profile_panel_width/height — anchor clear of the profile

#include <imgui.h>

namespace ui {

/// Shared chrome for the ledger-window family (Tile Ledger, Economy, and the
/// future Market / Balance / Construction ledgers). Every ledger window opens at
/// the **same size** and the **same spawn anchor** so the family reads as one
/// consistent surface, per docs/ui/LAYOUT.md § Uniform ledger-window chrome.
/// Both are applied with `ImGuiCond_Once`, so the window stays movable/resizable
/// thereafter.

/// One shared initial size for every ledger window.
inline constexpr ImVec2 ledger_window_size{820.0f, 600.0f};

/// One shared spawn anchor — clear of the top-left profile panel (to its right
/// and below the header strip). The window is movable after first appearance.
inline const ImVec2 ledger_window_spawn{profile_panel_width + 16.0f,
                                        profile_panel_height + 16.0f};

} // namespace ui
