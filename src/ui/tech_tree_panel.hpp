#pragma once

#include "../world/world.hpp"
#include "ui_state.hpp"

namespace ui {

// The research surface — PENDING. The BL-087/BL-310 constellation viewer drew
// the mock Lua tree (scripts/tech_tree.lua), which retired with BL-1273 when
// the nation came to hold technology (RESEARCH.md; NR-1022). The Digitisation
// tree panel — ring render, inspect, plan mode — is BL-1276's (tech tree
// panel). Until it lands, both halves below draw a pending state so the nav
// slot, F9 and the verify panel names keep working.

/// The nav-rail fold-out for slot 4 (Research). Guards its own visibility off
/// ui_state::show_tech_tree.
void draw_tech_tree_menu(ui_state& s);

/// The research canvas (a full-canvas takeover, ui::canvas_rect()). Guards its
/// own visibility off @p open.
void draw_tech_tree_panel(const world& w, bool& open);

} // namespace ui
