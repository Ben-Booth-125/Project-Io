#pragma once

#include <imgui.h>

class sim_loop;
struct ui_state;

namespace ui {

/// Draw the time panel — top-right, same width as the minimap (BL-138/BL-313):
/// the date/quarter line, the quarter-progress bar (left third) and the
/// pause/speed-tier controls with the active rate label (right two-thirds).
/// Extracted from app::render (BL-361); behaviour unchanged.
///
/// @param sim        The sim clock (read for the date; written by the buttons).
/// @param prev_speed Speed remembered across a pause, so unpausing restores it
///                   (app's m_prev_speed, shared with the keyboard bindings).
/// @param disp       ImGui display size for the frame.
void draw_time_panel(sim_loop& sim, int& prev_speed, const ImVec2& disp);

/// Draw the in-app system menu (BL-070) — the corner gear button and its popup
/// (Pause/Resume + Exit Game with inline confirm). Esc toggles the same popup
/// via st.show_system_menu (app::handle_key_down). Extracted from app::render
/// (BL-361); behaviour unchanged.
///
/// @param st             UI state (show_system_menu / confirm_exit_pending).
/// @param sim            The sim clock, for the Pause/Resume toggle.
/// @param prev_speed     Speed remembered across a pause (see draw_time_panel).
/// @param quit_requested Set true by "Yes, quit"; breaks app::run's loop.
/// @param disp           ImGui display size for the frame.
void draw_system_menu(ui_state& st, sim_loop& sim, int& prev_speed,
                      bool& quit_requested, const ImVec2& disp);

} // namespace ui
