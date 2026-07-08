#pragma once

#include "../world/tech_tree.hpp"

namespace ui {

/// Draws the read-only mock tech-tree viewer (F9). A floating window in the F1/F10
/// family, not a nav-rail ledger: it visualises the BL-087 design data in
/// scripts/tech_tree.lua (gate quests by Era, then standing lines, each a tech
/// table) so the tree's shape can be judged in a build. Display only — no research
/// state exists; the tech system is post-prototype (BL-087 resolution 6).
///
/// @param tree The startup-loaded mock tree.
/// @param open Visibility flag; the window's close button writes false.
void draw_tech_tree_panel(const tech_tree_registry& tree, bool& open);

} // namespace ui
