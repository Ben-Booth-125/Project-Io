#pragma once

#include "../world/tech_tree.hpp"
#include "../world/world.hpp" // BL-344: earned-tech state the viewer reports
#include "ui_state.hpp"

namespace ui {

/// Draws the era-selector menu (BL-310 round 4) — a nav-rail fold-out ledger
/// in the shell column, same idiom as every other menu (ui::foldout_begin).
/// One row per era: a real thumbnail of that era's own node graph AND its
/// name, both independently clickable, either selects the era (Ben,
/// 2026-08-06: "a button click for the name, or the map click ... both of
/// these should be in menu space"). Call every frame alongside
/// draw_tech_tree_panel; each guards its own visibility off
/// ui_state::show_tech_tree.
///
/// @param tree The startup-loaded mock tree.
/// @param s     UI state — reads/writes show_tech_tree and tech_tree_view.
void draw_tech_tree_menu(const tech_tree_registry& tree, ui_state& s);

/// Draws the read-only mock tech-tree viewer's CANVAS (BL-087, BL-310). A
/// full-canvas takeover (ui::canvas_rect(), BL-265's settled geometry) with
/// no chrome of its own — no title, no era tabs, no return control (BL-310
/// round 4, Ben: "this is just tech constellation"). Era selection lives
/// entirely in draw_tech_tree_menu's shell-column ledger. Display only — no
/// research state exists; the tech system is post-prototype (BL-087
/// resolution 6).
///
/// @param tree The startup-loaded mock tree.
/// @param open Visibility flag (ui_state::show_tech_tree) — closing happens
///              via the menu's toggle-rule re-click, not from this window.
/// @param view Selected era view (ui_state::tech_tree_view).
/// @param pan_x Radial-canvas pan offset, screen px (ui_state::tech_tree_pan_x).
/// @param pan_y Radial-canvas pan offset, screen px (ui_state::tech_tree_pan_y).
/// @param zoom  Radial-canvas zoom factor (ui_state::tech_tree_zoom).
/// @param w    Read-only world — BL-344: the viewer now reports whether a tech
///             is EARNED, and by whom, so it stops being a picture of a system.
/// @param corp Corporation whose earned set to read (the player's).
void draw_tech_tree_panel(const tech_tree_registry& tree, const world& w, entity_id corp,
                           bool& open, int& view,
                           float& pan_x, float& pan_y, float& zoom);

} // namespace ui
