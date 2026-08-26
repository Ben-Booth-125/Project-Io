#pragma once

#include "../world/standing.hpp"
#include "../world/world.hpp"
#include "ui_state.hpp"

#include <vector>

namespace ui {

/// Draws the Corporation Overview Dashboard. open controls visibility (toggled by nav rail).
///
/// @param w         The world to read corporations from (const; selection mutation is via s).
/// @param standings This tick's per-corp standing profile (BL-262, re-specified by BL-633),
///                   precomputed by the caller via compute_corp_standings. Reach and share print
///                   exactly for EVERY row (both derive from already-public facts); capital prints
///                   only where the firm files (`capital_disclosed`), a dash otherwise
///                   (FINANCE.md § Disclosure).
/// @param s         Shared UI state; clicking a row sets s.selected_entity.
/// @param open      Visibility flag; the window's close button writes false.
void draw_corporation_panel(const world& w, const std::vector<corp_standing>& standings,
                             ui_state& s, bool& open);

} // namespace ui
