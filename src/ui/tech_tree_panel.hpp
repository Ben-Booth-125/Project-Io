#pragma once

#include "../world/tech_tree.hpp"

namespace ui {

/// Draws the read-only mock tech-tree viewer (F9). A floating window in the F1/F10
/// family, not a nav-rail ledger: it visualises the BL-087 design data in
/// scripts/tech_tree.lua so the tree's shape can be judged in a build. One tab per
/// era (each era carries its own tree; unauthored eras hold a placeholder — the
/// Era -1 antiquity tab points at the BL-307 ladder store) plus a standing-lines
/// tab. Display only — no research state exists; the tech system is
/// post-prototype (BL-087 resolution 6).
///
/// @param tree The startup-loaded mock tree.
/// @param open Visibility flag; the window's close button writes false.
/// @param view Selected era view (ui_state::tech_tree_view).
void draw_tech_tree_panel(const tech_tree_registry& tree, bool& open, int& view);

} // namespace ui
