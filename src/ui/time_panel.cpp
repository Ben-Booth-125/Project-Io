/// @file time_panel.cpp
/// The time panel and the escape menu (BL-361, BL-1519).

#include "ui/time_panel.hpp"

#include "core/sim_loop.hpp"
#include "ui/foldout_column.hpp" // canvas_rect, what the escape menu takes
#include "ui/format.hpp"
#include "ui/icons.hpp"
#include "ui/presentation.hpp"
#include "ui/shell_metrics.hpp"
#include "ui/ui_state.hpp"

#include <imgui.h>
#include <imgui_internal.h> // BringWindowToDisplayFront

#include "profile_panel.hpp" // profile_panel_height — the top band's shared height
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace {


/// The speed tier's rate as a multiplier string ("0.25×" … "16×"), derived from
/// `sim_loop::speed_multiplier` so the label can never drift from the curve it
/// describes (BL-178). Returns "Paused" for tier 0.
const char* speed_rate_label(int speed)
{
    switch (speed)
    {
    case 1:  return "0.25×";
    case 2:  return "0.5×";
    case 3:  return "1×";
    case 4:  return "4×";
    case 5:  return "16×";
    default: return "Paused";
    }
}

/// The speed tier's real-time cost of one economic quarter, as a compact human
/// string (BL-178). Computed from the sim-loop constants rather than authored, so
/// retuning `seconds_per_day_1x` or the multiplier curve updates the label:
/// one quarter is `econ_tick_days` in-game days, and one in-game day costs
/// `seconds_per_day_1x / multiplier` real seconds.
const char* speed_quarter_label(int speed)
{
    const double mult = sim_loop::speed_multiplier(speed);
    if (mult <= 0.0) return "—";

    const double secs = (sim_loop::econ_tick_days * sim_loop::seconds_per_day_1x) / mult;

    // Static buffers per tier: the caller treats the result as a stable literal,
    // and there are only five tiers, so this stays trivially safe.
    static char buf[sim_loop::max_speed + 1][24] = {};
    char* out = buf[speed < 0 ? 0 : (speed > sim_loop::max_speed ? 0 : speed)];
    if (secs < 60.0)
        std::snprintf(out, 24, "~%ds", static_cast<int>(std::lround(secs)));
    else
        std::snprintf(out, 24, "~%dm", static_cast<int>(std::lround(secs / 60.0)));
    return out;
}

} // namespace

namespace ui {

void draw_time_panel(ui_state& state, sim_loop& sim, int& prev_speed, const ImVec2& disp)
{
    // Time panel — top-right, same width as the minimap (BL-138 compact redesign,
    // proportions revised on Ben's 2026-07-10 review). REFLOWED (BL-313, Ben
    // 2026-08-06: "make sure the time bar is within the header bound ... left
    // 1/3 for the progress, right 2/3 for time controls") — was four stacked
    // full-width rows whose total height exceeded header_panel_height and
    // overflowed the header strip. Now two COLUMNS sharing one row-band: left
    // third is "the progress" (date line + quarter-progress bar), right two
    // thirds is "time controls" (the pause/speed-tier buttons + active-rate
    // label) — half the row count, so it fits. The panel takes input (the
    // speed buttons), so it is not flagged NoInputs.
    const float tick_w = ui::right_chrome_width(disp);
    const float col_gap  = ImGui::GetStyle().ItemSpacing.x;
    const float prog_w   = tick_w / 3.0f - col_gap * 0.5f;
    const float time_line_h    = ImGui::GetTextLineHeightWithSpacing();
    // BL-178: the 10 px bar was easy to miss and carried no label. Tall enough to
    // seat a centred overlay ("58 d to Q2"), which is what actually makes the
    // next-resolution distance read.
    const float time_prog_h    = ImGui::GetTextLineHeight() + 4.0f;
    const float time_spacing   = ImGui::GetStyle().ItemSpacing.y;
    // BL-178: one always-visible line under the speed row naming the ACTIVE
    // tier's real rate. A tooltip cannot be seen without hovering (the same
    // critique BL-174 made of the nav rail), so the current speed's meaning is
    // stated on screen; the per-button tooltips carry the full ladder.
    const float time_rate_h    = ImGui::GetTextLineHeightWithSpacing();
    // BL-313: one frame height, not two — the two-column layout gives the
    // button row half the width it used to have, and a half-width row of six
    // buttons reads fine at the ordinary control height; the extra height the
    // old single-column layout spent here is exactly what put the panel over
    // header_panel_height.
    const float time_btn_h     = ImGui::GetFrameHeight();
    // Date line full-width, then one shared row where the left third (progress
    // bar) and right two-thirds (buttons + rate label) are measured
    // independently and the taller of the two sets the row's height.
    const float prog_col_h     = time_prog_h;
    const float ctrl_col_h     = time_btn_h + time_rate_h + time_spacing;
    const float time_content_h = time_line_h + std::max(prog_col_h, ctrl_col_h);
    // FLOORED at the header's height (Ben, 2026-08-24). The three tiles of the top
    // band — profile, balance bar, time controls — now share a bottom edge, the way
    // the three tiles of the bottom band already share a top one.
    //
    // This does not undo BL-313's two-column layout, which exists so the CONTENT
    // fits under the header height rather than overflowing it. That still holds and
    // is still why the row is one frame tall; the panel simply stops ending early
    // when its content happens to be shorter than its neighbours.
    const float time_h         = std::max(time_content_h + ImGui::GetStyle().WindowPadding.y * 2.0f,
                                          ui::profile_panel_height);
    // Publish it: the right chrome column below this panel needs to know where
    // its ceiling is, and this is the only place the number exists (BL-533).
    state.time_panel_h = time_h;
    {
        // Head of the right chrome column (BL-216: shell_metrics owns the edge).
        const ui::shell_rect tp = ui::time_panel_rect(disp, time_h);
        ImGui::SetNextWindowPos({tp.x, tp.y});
        ImGui::SetNextWindowSize({tp.w, tp.h});
        constexpr ImGuiWindowFlags time_flags =
            ImGuiWindowFlags_NoTitleBar          |
            ImGuiWindowFlags_NoResize            |
            ImGuiWindowFlags_NoMove              |
            ImGuiWindowFlags_NoCollapse          |
            ImGuiWindowFlags_NoScrollbar         |
            ImGuiWindowFlags_NoNav               |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoSavedSettings;
        ImGui::Begin("##time_panel", nullptr, time_flags);

        const uint64_t day = sim.day_tick();
        const ui::fmt::calendar_date date = ui::fmt::date_from_day(day);

        // --- Date/quarter line, full width — NOT confined to the left third:
        // "1960 Jan 1st [Q1]" runs wider than a 1/3 column at this panel's
        // width, and a fixed-width column has no wrap/clip of its own (a
        // BeginGroup is not a clip region), so an early version of this
        // reflow let the text bleed into the button row below it. The
        // 1/3-vs-2/3 split applies to the row underneath, where both sides
        // ARE sized elements (a progress bar, a button row) that actually
        // respect a width argument.
        ImGui::Text("%s %s %s [Q%d]", std::to_string(date.year).c_str(),
                    ui::fmt::month_abbrev(date.month),
                    ui::fmt::ordinal_day(date.day).c_str(), date.quarter);

        const ImVec2 row_start = ImGui::GetCursorPos();

        // --- LEFT THIRD: "the progress" — the quarter-progress bar. BL-178:
        // text-height, carries a centred overlay naming the distance to the
        // next resolution ("how close am I to the economy resolving").
        ImGui::BeginGroup();
        const float quarter_frac = ui::fmt::quarter_progress(day);
        char         prog_label[32];
        const int    days_left = static_cast<int>(
            std::lround((1.0f - quarter_frac) * static_cast<float>(sim_loop::econ_tick_days)));
        std::snprintf(prog_label, sizeof(prog_label), "%d d to Q%d",
                      days_left, (date.quarter % 4) + 1);
        ImGui::ProgressBar(quarter_frac, {prog_w, time_prog_h}, prog_label);
        ImGui::SetItemTooltip(
            "Quarter progress. The economy resolves on the quarter boundary:\n"
            "prices clear, production banks, and the budget settles.");
        ImGui::EndGroup();

        // --- RIGHT TWO-THIRDS: "time controls" — pause/speed-tier buttons +
        // the active-rate label, top-aligned with the progress bar (explicit
        // cursor, not SameLine — SameLine would anchor to the left group's
        // LAST item, not its top; harmless here since the bar is the only
        // item, but explicit stays correct if that ever changes).
        ImGui::SetCursorPos({row_start.x + prog_w + col_gap, row_start.y});
        const float ctrl_avail = ImGui::GetWindowContentRegionMax().x - ImGui::GetCursorPosX();
        ImGui::BeginGroup();

        // Speed controls: pause + speed-tier buttons, sized to the control
        // column's own width (NOT GetContentRegionAvail — a group is not a
        // clipping region, so that would still measure the whole window).
        // The active speed is highlighted. When running, the pause slot is a
        // blank button carrying a filled square glyph (drawn below); when
        // paused it flips to a play ">" so it reflects the toggle state.
        // Speed tiers use Roman numerals (I–V); the square avoids "||"
        // reading as II.
        {
            const char* labels[] = {sim.paused() ? ">" : "##pause", "I", "II", "III", "IV", "V"};
            const int   speeds[] = { 0,    1,   2,   3,   4,   5 };
            const int   n        = 6;
            const float spacing  = ImGui::GetStyle().ItemSpacing.x;
            // The escape menu's gear closes the row, inside the panel's right end
            // (TIME_CONTROLS.md § The escape menu): the panel is flush to the screen
            // edge, so there is no room right of it. The row is measured to the
            // panel's content edge.
            const float gear_w   = time_btn_h;
            const float bw       = (ctrl_avail - gear_w - spacing * n) / n;

            // The escape menu is the pause screen: the clock stays stopped under it.
            ImGui::BeginDisabled(state.show_system_menu);

            for (int i = 0; i < n; ++i)
            {
                const bool active = (sim.speed() == speeds[i]);
                if (active)
                    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
                if (ImGui::Button(labels[i], {bw, time_btn_h}))
                {
                    if (speeds[i] == 0)
                    {
                        // Pause button toggles: if already paused, restore the
                        // previous speed; otherwise save the current speed and pause.
                        if (sim.paused())
                            sim.set_speed(prev_speed);
                        else
                        {
                            prev_speed = sim.speed();
                            sim.set_speed(0);
                        }
                    }
                    else
                    {
                        // Speed button: remember it so pause can restore it later.
                        prev_speed = speeds[i];
                        sim.set_speed(speeds[i]);
                    }
                }
                // Pause glyph: a filled square centred on the blank "##pause" button —
                // clearer than "||", which read as the numeral II beside the tiers.
                if (speeds[i] == 0 && !sim.paused())
                {
                    const ImVec2 mn = ImGui::GetItemRectMin();
                    const ImVec2 mx = ImGui::GetItemRectMax();
                    const ImVec2 c  = {(mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f};
                    const float  h  = ImGui::GetFontSize() * 0.31f;
                    ImGui::GetWindowDrawList()->AddRectFilled(
                        {c.x - h, c.y - h}, {c.x + h, c.y + h},
                        ImGui::GetColorU32(ImGuiCol_Text), 1.0f);
                }
                if (active)
                    ImGui::PopStyleColor();
                // BL-178: name each tier's REAL rate on hover. The multipliers were
                // already documented in the F1 hotkey sheet but never on the control
                // itself, so the ladder was unreadable where the player uses it.
                if (speeds[i] == 0)
                    ImGui::SetItemTooltip("%s (Space)", sim.paused() ? "Resume" : "Pause");
                else
                    ImGui::SetItemTooltip("Speed %s — %s\n%s per quarter (%d)",
                                          labels[i],
                                          speed_rate_label(speeds[i]),
                                          speed_quarter_label(speeds[i]),
                                          speeds[i]);
                if (i + 1 < n)
                    ImGui::SameLine();
            }
            ImGui::EndDisabled();

            // The gear (BL-1519): toggles the escape menu, lit while it is open.
            ImGui::SameLine();
            const ImVec2 g0 = ImGui::GetCursorScreenPos();
            if (ImGui::InvisibleButton("##escape_gear", {gear_w, time_btn_h}))
            {
                state.show_system_menu = !state.show_system_menu;
                state.confirm_exit_pending = false;
            }
            const bool  gear_hot = ImGui::IsItemHovered();
            const ImU32 gear_col = state.show_system_menu ? palette::selection
                                 : gear_hot ? IM_COL32(232, 236, 246, 255)
                                            : IM_COL32(170, 178, 192, 255);
            state.escape_drawn[ui_state::esc_gear].x = g0.x + gear_w * 0.5f;
            state.escape_drawn[ui_state::esc_gear].y = g0.y + time_btn_h * 0.5f;
            icons::gear(ImGui::GetWindowDrawList(),
                        {g0.x + gear_w * 0.5f, g0.y + time_btn_h * 0.5f},
                        time_btn_h * 0.36f, gear_col);
            ImGui::SetItemTooltip("Menu (Esc)");
        }

        // --- BL-178: the active tier's rate, always visible. Guaranteed-fit per
        // LAYOUT.md container 5 (the time panel is authored to fit): the string is
        // measured against the control column's own width — a group is not a
        // clipping region, so GetContentRegionAvail() would still measure the
        // whole window (the BL-313 bug this whole block was rewritten to avoid).
        {
            char rate[64];
            if (sim.paused())
                std::snprintf(rate, sizeof(rate), "Paused");
            else
                std::snprintf(rate, sizeof(rate), "%s  ·  %s per quarter",
                              speed_rate_label(sim.speed()),
                              speed_quarter_label(sim.speed()));

            if (!sim.paused() && ImGui::CalcTextSize(rate).x > ctrl_avail)
                std::snprintf(rate, sizeof(rate), "%s",
                              speed_rate_label(sim.speed()));
            ImGui::TextDisabled("%s", rate);
        }
        ImGui::EndGroup();

        ImGui::End();
    }
}

escape_menu_press draw_escape_menu(ui_state& st, bool options_open, const char* last_saved)
{
    escape_menu_press out;
    for (int i = 0; i < ui_state::esc_gear; ++i)
        st.escape_drawn[i] = {};
    if (!st.show_system_menu)
        return out;

    // The whole canvas region (LAYOUT.md § Canvas area), the shell standing around
    // it: the gear in the time panel stays pressable, so the control that opened
    // this closes it (the toggle rule).
    const foldout_rect canvas = canvas_rect();
    ImGui::SetNextWindowPos({canvas.x, canvas.y});
    ImGui::SetNextWindowSize({canvas.w, canvas.h});
    ImGui::SetNextWindowBgAlpha(0.94f);
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::Begin("##escape_menu", nullptr, flags);
    // Over any takeover when it opens, but a press on it must not bury the Options
    // window it just opened: hence no bring-to-front on focus.
    if (ImGui::IsWindowAppearing())
        ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());

    const ImVec2 btn{std::min(280.0f, canvas.w - 32.0f), 40.0f};
    const float  line = ImGui::GetTextLineHeightWithSpacing();
    const float  gap  = 6.0f;
    // Title, last-saved line, then four presses or the warning; the hint beneath.
    const float  body_h = line * 3.0f + (btn.y + gap) * 4.0f
                        + (st.spectating ? ImGui::GetFrameHeightWithSpacing() : 0.0f);
    ImGui::SetCursorPosY(std::max(8.0f, (canvas.h - body_h) * 0.5f));

    const auto centred_text = [&](const char* text, ImU32 col) {
        ImGui::SetCursorPosX((canvas.w - ImGui::CalcTextSize(text).x) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, col);
        ImGui::TextUnformatted(text);
        ImGui::PopStyleColor();
    };
    const auto press = [&](const char* label, ui_state::escape_press which, bool lit = false) {
        ImGui::SetCursorPosX((canvas.w - btn.x) * 0.5f);
        if (lit)
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        const bool pressed = ImGui::Button(label, btn);
        if (lit)
            ImGui::PopStyleColor();
        const ImVec2 mn = ImGui::GetItemRectMin();
        st.escape_drawn[which].x = mn.x + btn.x * 0.5f;
        st.escape_drawn[which].y = mn.y + btn.y * 0.5f;
        return pressed;
    };

    centred_text("Paused", ImGui::GetColorU32(ImGuiCol_Text));
    centred_text(last_saved, palette::text_secondary);
    ImGui::Dummy({0.0f, 6.0f});

    if (!st.confirm_exit_pending)
    {
        out.save = press("Save", ui_state::esc_save);
        out.options = press("Options", ui_state::esc_options, options_open);
        ImGui::SetItemTooltip("Display and UI scale (F10).");
        if (press("Exit to Menu", ui_state::esc_exit_menu))
        {
            st.confirm_exit_pending = true;
            st.confirm_exit_to_menu = true;
        }
        if (press("Exit Game", ui_state::esc_exit_game))
        {
            st.confirm_exit_pending = true;
            st.confirm_exit_to_menu = false;
        }
    }
    else
    {
        // MENU.md: both exits warn about unsaved progress before they act. Always,
        // rather than only when the world moved: a paused player can still place a
        // build, so "nothing changed" is a claim this surface cannot make.
        centred_text("Unsaved progress will be lost.", palette::negative);
        const char* confirm = st.confirm_exit_to_menu ? "Exit to Menu" : "Exit Game";
        if (press(confirm, ui_state::esc_warn_confirm))
        {
            out.exit_to_menu = st.confirm_exit_to_menu;
            out.exit_game    = !st.confirm_exit_to_menu;
        }
        if (press("Cancel", ui_state::esc_warn_cancel))
            st.confirm_exit_pending = false;
    }

    // Spectator god view (BL-408): offered ONLY while spectating — a played session
    // never renders the control, so the BL-068 asymmetry it lifts is unreachable
    // outside spectate by construction, not by convention. A checkbox is inherently
    // a toggle (Toggle rule, io-standing-rules.md): its visible checked state is
    // exactly what re-clicking undoes.
    if (st.spectating)
    {
        ImGui::SetCursorPosX((canvas.w - btn.x) * 0.5f);
        ImGui::Checkbox("God view", &st.god_view);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Spectator only: read every nation's internals and see\n"
                              "through the survey mask. Presentation-side - the AI's\n"
                              "own view of the world is untouched.");
    }

    centred_text("Esc to resume", palette::neutral);
    ImGui::End();
    return out;
}

} // namespace ui
