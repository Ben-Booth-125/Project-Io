#include "balance_ledger.hpp"

#include "foldout_column.hpp" // shell fold-out column host (BL-122)
#include "header_panel.hpp"   // player_stockpile_value — shared Cargo Value (BL-171)
#include "presentation.hpp"

#include <imgui.h>

#include <algorithm> // std::min/max
#include <cmath>     // std::fabs, std::pow/log10/floor (nice axis ceiling)
#include <cstdio>    // std::snprintf (chart labels)
#include <string>    // header / label strings
#include <vector>

namespace ui {

namespace {

// Round v up to a 'nice' axis bound (1/2/5 x a power of ten): 450->500, 1800->2000.
float nice_ceil(float v)
{
    if (v <= 0.0f)
        return 0.0f;
    const float mag  = std::pow(10.0f, std::floor(std::log10(v)));
    const float n    = v / mag; // 1..10
    const float step = (n <= 1.0f) ? 1.0f : (n <= 2.0f) ? 2.0f : (n <= 5.0f) ? 5.0f : 10.0f;
    return step * mag;
}

// Short faint dashes across a chart gridline (ImDrawList has no dashed line).
void dotted_hline(ImDrawList* dl, float x0, float x1, float y, ImU32 col)
{
    for (float x = x0; x < x1; x += 6.0f)
        dl->AddLine({x, y}, {std::min(x + 3.0f, x1), y}, col, 1.0f);
}

// Profit line chart (BL-171): profit per econ tick over the recent series, in the
// rect [mn, mx]. A left gutter of value ticks (top / zero / bottom, K-formatted once
// the scale reaches thousands), a zero baseline, a gold polyline over evenly-spaced
// points, and 1..N tick labels along the bottom. Handles a negative early-game series
// (BL-112) by keeping zero in range so the climb to profit reads.
void draw_profit_chart(ImDrawList* dl, ImVec2 mn, ImVec2 mx, const std::vector<float>& profit)
{
    const ImU32 grid_col  = IM_COL32(120, 120, 120, 150);
    const ImU32 label_col = IM_COL32(150, 150, 150, 255);
    const ImU32 line_col  = IM_COL32(212, 175, 55, 255); // gold

    constexpr float gutter = 44.0f;
    const float plot_x0 = mn.x + gutter;
    const float plot_x1 = mx.x - 6.0f;
    const float y_top   = mn.y + 14.0f; // room for the "Profit" caption
    const float y_bot   = mx.y - 14.0f; // room for the X labels

    // "Profit" caption, top-left (the mockup's vertical axis title, kept horizontal).
    dl->AddText({mn.x, mn.y}, label_col, "Profit");

    if (profit.size() < 2)
    {
        dl->AddText({plot_x0, (y_top + y_bot) * 0.5f}, label_col,
                    "Run economy ticks to chart profit.");
        return;
    }

    // Value range with zero always in view.
    float vmin = 0.0f, vmax = 0.0f;
    for (const float p : profit) { vmin = std::min(vmin, p); vmax = std::max(vmax, p); }
    const float top = nice_ceil(vmax);
    const float bot = -nice_ceil(-vmin);
    const float span = (top - bot > 0.0f) ? (top - bot) : 1.0f;
    const bool  useK = (std::fabs(top) >= 1000.0f || std::fabs(bot) >= 1000.0f);

    const auto y_of = [&](float v) { return y_bot - (v - bot) / span * (y_bot - y_top); };
    const auto vlabel = [&](float v, char* buf, std::size_t n) {
        if (useK) std::snprintf(buf, n, "%gK", static_cast<double>(v / 1000.0f));
        else      std::snprintf(buf, n, "%g",  static_cast<double>(v));
    };

    // Value gridlines + labels: top, zero, and bottom (when negative).
    auto tick = [&](float v) {
        const float ty = y_of(v);
        dotted_hline(dl, plot_x0, plot_x1, ty, grid_col);
        char buf[24]; vlabel(v, buf, sizeof buf);
        const ImVec2 ts = ImGui::CalcTextSize(buf);
        dl->AddText({plot_x0 - 6.0f - ts.x, ty - ts.y * 0.5f}, label_col, buf);
    };
    tick(top);
    tick(0.0f);
    if (bot < 0.0f)
        tick(bot);

    // Gold polyline over evenly-spaced points, with a small node at each vertex.
    const int   n  = static_cast<int>(profit.size());
    const float dx = (n > 1) ? (plot_x1 - plot_x0) / static_cast<float>(n - 1) : 0.0f;
    const auto  px = [&](int i) { return plot_x0 + dx * static_cast<float>(i); };
    for (int i = 0; i + 1 < n; ++i)
        dl->AddLine({px(i), y_of(profit[i])}, {px(i + 1), y_of(profit[i + 1])}, line_col, 2.0f);
    for (int i = 0; i < n; ++i)
        dl->AddCircleFilled({px(i), y_of(profit[i])}, 2.5f, line_col);

    // X labels: 1..N under the axis (sparse when the series is long).
    const int stride = (n <= 8) ? 1 : (n + 5) / 6;
    for (int i = 0; i < n; i += stride)
    {
        char buf[8]; std::snprintf(buf, sizeof buf, "%d", i + 1);
        const ImVec2 ts = ImGui::CalcTextSize(buf);
        dl->AddText({px(i) - ts.x * 0.5f, y_bot + 2.0f}, label_col, buf);
    }
}

// A stubbed policy-tier selector (BL-171 UI): a centred label over a `– I II III IV V +`
// row. The active tier is highlighted green; the arrows step it, the numerals select
// it. Interactive so the control reads as real, but it has NO economic effect yet —
// the Tax/Wages mechanics are owed to BL-155 (a tooltip says so).
void draw_tier_control(const char* label, int& tier)
{
    const float avail = ImGui::GetContentRegionAvail().x;

    // Centred label.
    const ImVec2 lts = ImGui::CalcTextSize(label);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - lts.x) * 0.5f);
    ImGui::TextUnformatted(label);

    // Centred button row: [–] I II III IV V [+].
    const char* nums[] = {"I", "II", "III", "IV", "V"};
    const float bw      = ImGui::GetFrameHeight();
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const int   count   = 7; // '-' + 5 tiers + '+'
    const float total   = count * bw + (count - 1) * spacing;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (avail - total) * 0.5f));

    const auto note = [] {
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Policy lever - not yet wired (BL-155)");
    };

    if (ImGui::Button("-", {bw, bw})) tier = std::max(1, tier - 1);
    note();
    for (int t = 1; t <= 5; ++t)
    {
        ImGui::SameLine();
        const bool active = (tier == t);
        if (active)
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(palette::positive));
        if (ImGui::Button(nums[t - 1], {bw, bw})) tier = t;
        if (active)
            ImGui::PopStyleColor();
        note();
    }
    ImGui::SameLine();
    if (ImGui::Button("+", {bw, bw})) tier = std::min(5, tier + 1);
    note();
}

} // namespace

void draw_balance_ledger(const world& w, const economy_report& report,
                         const player_plot_history& history, ui_state& ui, bool& open)
{
    if (!open)
        return;

    // Re-hosted into the shell fold-out column (BL-122); closed via the nav rail.
    if (!ui::foldout_begin("Balance Ledger"))
    {
        ui::foldout_end();
        return;
    }

    // Player-only budget (BL-142): pinned to the player's own corp. Rival treasuries
    // stay private per the competitor-visibility rule (BL-068).
    const auto cit = w.corporations.find(w.player_entity);
    if (cit == w.corporations.end())
    {
        ImGui::TextDisabled("No corporation data.");
        ui::foldout_end();
        return;
    }
    const corporation_component& cc = cit->second;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float content_w = ImGui::GetContentRegionAvail().x;

    // --- Corp name header (centred). ---
    {
        const ImVec2 ts = ImGui::CalcTextSize(cc.name.c_str());
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (content_w - ts.x) * 0.5f));
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s", cc.name.c_str());
    }
    ImGui::Spacing();

    // --- Profit chart: profit per econ tick = income − expenditure, most recent
    // window. Replaces the former itemised cashflow table (Ben's call — too much
    // detail here; it returns in a dedicated breakdown menu later). ---
    std::vector<float> profit;
    {
        const std::size_t n = std::min(history.income.size(), history.expenditure.size());
        const std::size_t window = 12;
        const std::size_t start  = (n > window) ? (n - window) : 0;
        for (std::size_t i = start; i < n; ++i)
            profit.push_back(history.income[i] - history.expenditure[i]);
    }
    {
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const float  h = 130.0f;
        ImGui::Dummy({content_w, h});
        draw_profit_chart(dl, p, {p.x + content_w, p.y + h}, profit);
    }
    ImGui::Spacing();

    // --- Policy levers (stubbed UI, mechanics owed to BL-155). ---
    draw_tier_control("Taxes", ui.budget_tax_tier);
    ImGui::Spacing();
    draw_tier_control("Wages", ui.budget_wage_tier);
    ImGui::Spacing();

    // --- Assets. ---
    ImGui::SeparatorText("Assets");
    ImGui::Text("Buildings Owned : %d", static_cast<int>(cc.assets.size()));

    float income = 0.0f;
    if (const auto bit = report.budgets.find(w.player_entity); bit != report.budgets.end())
        income = bit->second.income;
    ImGui::Text("Income: Cr %.0f", static_cast<double>(income));
    ImGui::Text("Cargo Value: Cr %.0f", static_cast<double>(player_stockpile_value(w)));
    ImGui::Spacing();

    // --- Top-buildings-by-profit rank table: placeholder box (BL-171). Per-building
    // profit exists (BL-074); the rank-change-over-4-ticks history is owed. ---
    {
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const float  h = std::max(120.0f, ImGui::GetContentRegionAvail().y - 4.0f);
        const ImVec2 mx = {p.x + content_w, p.y + h};
        dl->AddRect(p, mx, IM_COL32(110, 110, 110, 255), 3.0f);

        const char* title = "BUILDING_RANK_TABLE";
        const ImVec2 tts = ImGui::CalcTextSize(title);
        dl->AddText({p.x + (content_w - tts.x) * 0.5f, p.y + h * 0.42f},
                    IM_COL32(200, 200, 200, 255), title);

        ImGui::Dummy({content_w, h * 0.62f});
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("Shows the top 8 buildings by profit, their profit, and change "
                            "in rank from the last year (4 econ ticks).");
        ImGui::PopTextWrapPos();
    }

    ui::foldout_end();
}

} // namespace ui
