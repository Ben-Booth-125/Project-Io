/// @file time_panel.cpp
/// The time panel and the in-app system menu — extracted verbatim from
/// app::render (BL-361); behaviour unchanged.

#include "ui/time_panel.hpp"

#include "core/sim_loop.hpp"
#include "ui/format.hpp"
#include "ui/shell_metrics.hpp"
#include "ui/ui_state.hpp"

#include <imgui.h>

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

void draw_time_panel(sim_loop& sim, int& prev_speed, const ImVec2& disp)
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
    const float ctrl_w   = tick_w - prog_w - col_gap;
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
    const float time_h         = time_content_h + ImGui::GetStyle().WindowPadding.y * 2.0f;
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
            // Narrowed off the exact fill-width division (Ben, playtest: the "V"
            // button was riding the screen edge) — a small margin per button so the
            // row sits comfortably inside ctrl_w instead of exactly matching it.
            const float bw       = (ctrl_w - spacing * (n - 1)) / n * 0.92f;

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

            if (!sim.paused() && ImGui::CalcTextSize(rate).x > ctrl_w)
                std::snprintf(rate, sizeof(rate), "%s",
                              speed_rate_label(sim.speed()));
            ImGui::TextDisabled("%s", rate);
        }
        ImGui::EndGroup();

        ImGui::End();
    }
}

void draw_system_menu(ui_state& st, sim_loop& sim, int& prev_speed,
                      bool& quit_requested, const ImVec2& disp)
{
    constexpr float margin = ui::shell_margin;
    // In-app system menu (BL-070): a corner gear button opening a small popup with
    // session controls — Pause/Resume (mirroring the Space hotkey via the shared
    // pause_toggle path) and Exit Game (inline "Really quit?" confirm, since there
    // is no save). Sits at the top-right, just left of the time column; Esc toggles
    // the same popup (handle_key_down). See docs/ui/MENU.md.
    {
        constexpr float gear = 26.0f;
        // One margin left of the right chrome column (BL-216).
        const ImVec2 gear_pos{ ui::right_chrome_left(disp) - margin - gear, margin };

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0f, 0.0f});
        ImGui::SetNextWindowPos(gear_pos);
        ImGui::SetNextWindowSize({gear, gear});
        constexpr ImGuiWindowFlags gear_flags =
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings;
        ImGui::Begin("##sysmenu_gear", nullptr, gear_flags);
        {
            ImDrawList*  gdl = ImGui::GetWindowDrawList();
            const ImVec2 g0  = ImGui::GetCursorScreenPos();
            if (ImGui::InvisibleButton("##sysmenu_btn", {gear, gear}))
            {
                st.show_system_menu = !st.show_system_menu;
                if (!st.show_system_menu)
                    st.confirm_exit_pending = false;
            }
            const bool  hov = ImGui::IsItemHovered() || st.show_system_menu;
            const ImU32 gc  = hov ? IM_COL32(232, 236, 246, 255) : IM_COL32(170, 178, 192, 255);
            const float cx  = g0.x + gear * 0.5f;
            const float cy  = g0.y + gear * 0.5f;
            const float hw  = gear * 0.28f;
            for (int i = -1; i <= 1; ++i)
                gdl->AddLine({cx - hw, cy + static_cast<float>(i) * 6.0f},
                             {cx + hw, cy + static_cast<float>(i) * 6.0f}, gc, 2.0f);
        }
        ImGui::End();
        ImGui::PopStyleVar();

        if (st.show_system_menu)
        {
            ImGui::SetNextWindowPos({gear_pos.x + gear, gear_pos.y + gear + 4.0f},
                                    ImGuiCond_Always, {1.0f, 0.0f});
            constexpr ImGuiWindowFlags menu_flags =
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoNav |
                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;
            if (ImGui::Begin("##system_menu", nullptr, menu_flags))
            {
                constexpr float bw = 150.0f;
                if (ImGui::Button(sim.paused() ? "Resume" : "Pause", {bw, 0.0f}))
                {
                    // Same toggle app::dispatch_action performs for pause_toggle
                    // (the Space hotkey); inlined here because dispatch_action is
                    // an app member and this surface no longer lives in app.cpp.
                    if (sim.paused())
                        sim.set_speed(prev_speed);
                    else
                    {
                        prev_speed = sim.speed();
                        sim.set_speed(0);
                    }
                }

                ImGui::Separator();

                if (!st.confirm_exit_pending)
                {
                    if (ImGui::Button("Exit Game", {bw, 0.0f}))
                        st.confirm_exit_pending = true;
                }
                else
                {
                    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(IM_COL32(232, 150, 90, 255)),
                                       "Really quit?");
                    if (ImGui::Button("Yes, quit", {bw, 0.0f}))
                        quit_requested = true;
                    if (ImGui::Button("Cancel", {bw, 0.0f}))
                        st.confirm_exit_pending = false;
                }
            }
            ImGui::End();
        }
    }
}

} // namespace ui
