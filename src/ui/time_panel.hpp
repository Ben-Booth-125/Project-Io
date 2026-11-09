#pragma once

#include <imgui.h>

#include <string>

class sim_loop;
struct ui_state;

namespace ui {

/// Draw the time panel — top-right, same width as the minimap (BL-138/BL-313):
/// the date/quarter line, the quarter-progress bar (left third) and the
/// pause/speed-tier controls with the active rate label (right two-thirds).
/// Extracted from app::render (BL-361). The gear that opens the escape menu is the
/// row's right end (BL-1519).
///
/// @param sim        The sim clock (read for the date; written by the buttons).
/// @param prev_speed Speed remembered across a pause, so unpausing restores it
///                   (app's m_prev_speed, shared with the keyboard bindings).
/// @param disp       ImGui display size for the frame.
/// @param state  Receives the panel's measured height (state.time_panel_h), which
///               is derived from font metrics here and exists nowhere else. The
///               right chrome column below needs it to find its own ceiling.
void draw_time_panel(ui_state& state, sim_loop& sim, int& prev_speed, const ImVec2& disp);

/// What the escape menu's player pressed this frame. The app acts on them: saving,
/// options, the session teardown and the quit are all app members.
struct escape_menu_press
{
    std::string save_name;        ///< Non-empty: write the save under this checked name.
    bool options      = false;
    bool exit_to_menu = false; ///< Confirmed past the unsaved-progress warning.
    bool exit_game    = false; ///< Confirmed past the unsaved-progress warning.
};

/// Draw the escape menu (BL-1519; MENU.md § Application / system menu) over the
/// whole window: Save (with its name form), Options, Exit to Menu, Exit Game. No-op unless
/// st.show_system_menu.
///
/// @param st           UI state (show_system_menu and the exit warning).
/// @param options_open Whether the Options window is open, so its press lights.
/// @param last_saved   The session's last save, already worded.
escape_menu_press draw_escape_menu(ui_state& st, bool options_open, const char* last_saved);

} // namespace ui
