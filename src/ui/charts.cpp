#include "charts.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ui::charts {

float nice_ceil(float v)
{
    if (v <= 0.0f)
        return 1.0f;
    const float mag  = std::pow(10.0f, std::floor(std::log10(v)));
    const float n    = v / mag; // 1..10
    const float step = (n <= 1.0f) ? 1.0f : (n <= 2.0f) ? 2.0f : (n <= 5.0f) ? 5.0f : 10.0f;
    return step * mag;
}

float tight_ceil(float v)
{
    if (v <= 0.0f)
        return 1.0f;
    // A finer ladder than nice_ceil's 1/2/5. The coarse one wastes up to half the
    // plot height (a peak of 2.45 rounds to 5, so the tallest bar fills 49% of the
    // box); these steps keep the tallest column above ~70% while still landing on
    // a readable gridline. Kept separate from nice_ceil so the tile-selection
    // graphs, which are goldened against the coarse ladder, are untouched.
    static constexpr float steps[] = { 1.0f, 1.5f, 2.0f, 2.5f, 3.0f, 4.0f, 5.0f, 6.0f, 8.0f, 10.0f };
    const float mag = std::pow(10.0f, std::floor(std::log10(v)));
    const float n   = v / mag; // 1..10
    for (float s : steps)
        if (n <= s + 1e-4f)
            return s * mag;
    return 10.0f * mag;
}

void dotted_hline(ImDrawList* dl, float x0, float x1, float y, ImU32 col)
{
    for (float x = x0; x < x1; x += 6.0f)
        dl->AddLine({x, y}, {std::min(x + 3.0f, x1), y}, col, 1.0f);
}

float chart_row_height(float chart_h)
{
    const ImGuiStyle& style = ImGui::GetStyle();
    const float frame_h = ImGui::GetFrameHeight();
    return frame_h + chart_h + style.ItemSpacing.y + style.WindowPadding.y * 2.0f + 4.0f;
}

void draw_bars(ImDrawList* dl, ImVec2 mn, ImVec2 mx,
               const bar* bars, std::size_t count,
               float ceiling, const char* fmt, float bar_cap)
{
    if (count == 0 || ceiling <= 0.0f)
        return;

    const float plot_x0 = mn.x + gutter;
    const float y0 = mn.y, y1 = mx.y;
    const ImU32 grid_col  = IM_COL32(120, 120, 120, 150);
    const ImU32 label_col = IM_COL32(150, 150, 150, 255);

    const auto y_of = [&](float v) { return y1 - (v / ceiling) * (y1 - y0); };

    // Tick gutter: floor, top, and a mid tick once the ceiling is large enough to
    // make one worth reading.
    const auto tick = [&](float v) {
        const float ty = y_of(v);
        dotted_hline(dl, plot_x0, mx.x, ty, grid_col);
        char buf[24];
        std::snprintf(buf, sizeof buf, "%g", static_cast<double>(v));
        const ImVec2 ts = ImGui::CalcTextSize(buf);
        dl->AddText({plot_x0 - 6.0f - ts.x, ty - ts.y * 0.5f}, label_col, buf);
    };
    tick(0.0f);
    if (ceiling >= 100.0f)
        tick(ceiling * 0.5f);
    tick(ceiling);

    // Clustered columns sharing the baseline. By default the columns auto-fit the
    // plot, so a wide chart fills its box rather than huddling a row of thin bars
    // against the left edge. A caller whose width is fixed by a mockup passes an
    // explicit bar_cap — the tile-selection graphs pass 34.0f, which is what keeps
    // them pixel-identical to the version this was extracted from.
    constexpr float legend_w = 190.0f;
    constexpr float gap      = 10.0f;
    const float cap  = (bar_cap > 0.0f) ? bar_cap : 96.0f;
    const float band = std::max(60.0f, (mx.x - plot_x0) - legend_w);
    const float bar_w = std::min(cap,
        std::max(6.0f, (band - gap * static_cast<float>(count - 1)) / static_cast<float>(count)));

    for (std::size_t i = 0; i < count; ++i)
    {
        const float bx = plot_x0 + 6.0f + static_cast<float>(i) * (bar_w + gap);
        const ImVec2 a{bx, y_of(bars[i].value)};
        const ImVec2 b{bx + bar_w, y1};
        if (bars[i].hollow)
            dl->AddRect(a, b, bars[i].colour, 0.0f, 0, 1.5f);
        else
            dl->AddRectFilled(a, b, bars[i].colour);
    }

    // Legend to the right of the columns: swatch + label + value, one row each.
    const float lx = plot_x0 + 6.0f + static_cast<float>(count) * (bar_w + gap) + 4.0f;
    float ly = y0 + 2.0f;
    for (std::size_t i = 0; i < count; ++i)
    {
        if (bars[i].label[0] == '\0')
            continue;
        if (bars[i].hollow)
            dl->AddRect({lx, ly + 2.0f}, {lx + 10.0f, ly + 12.0f}, bars[i].colour, 0.0f, 0, 1.5f);
        else
            dl->AddRectFilled({lx, ly + 2.0f}, {lx + 10.0f, ly + 12.0f}, bars[i].colour);

        char val[32];
        std::snprintf(val, sizeof val, fmt, static_cast<double>(bars[i].value));
        char buf[80];
        std::snprintf(buf, sizeof buf, "%s %s", bars[i].label, val);
        dl->AddText({lx + 16.0f, ly}, IM_COL32(210, 210, 210, 255), buf);
        ly += 20.0f;
    }
}

void threshold_line(ImDrawList* dl, ImVec2 mn, ImVec2 mx,
                    float value, float ceiling, ImU32 colour, const char* caption)
{
    if (ceiling <= 0.0f || value <= 0.0f || value > ceiling)
        return;
    const float plot_x0 = mn.x + gutter;
    const float ty = mx.y - (value / ceiling) * (mx.y - mn.y);

    // Solid rather than dotted, so a gate reads as different in kind from a
    // gridline — this line is a rule the values are being judged against.
    dl->AddLine({plot_x0, ty}, {mx.x, ty}, colour, 1.0f);
    if (caption && caption[0])
    {
        const ImVec2 ts = ImGui::CalcTextSize(caption);
        dl->AddText({mx.x - ts.x - 2.0f, ty - ts.y - 1.0f}, colour, caption);
    }
}

} // namespace ui::charts
