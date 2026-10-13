#include "charts.hpp"

#include "format.hpp" // abbreviate (axis + readout labels)
#include "text_fit.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace ui::charts {

float legend_width(const bar* bars, std::size_t count, const char* fmt)
{
    float widest = 0.0f;
    for (std::size_t i = 0; i < count; ++i)
    {
        if (bars[i].label[0] == '\0')
            continue;
        char val[32];
        std::snprintf(val, sizeof val, fmt, static_cast<double>(bars[i].value));
        char buf[80];
        std::snprintf(buf, sizeof buf, "%s %s", bars[i].label, val);
        widest = std::max(widest, ui::fit_width(buf));
    }
    return (widest > 0.0f) ? 16.0f + widest + 8.0f : 0.0f;
}

namespace {

/// One below-plot legend entry's text ("label value") into @p buf.
void legend_entry(char* buf, std::size_t n, const bar& b, const char* fmt)
{
    char val[32];
    std::snprintf(val, sizeof val, fmt, static_cast<double>(b.value));
    std::snprintf(buf, n, "%s %s", b.label, val);
}

/// Tick-label text: fmt::abbreviate from 1000 up (the 5-digit overrun BL-215
/// fixes), %g below it — abbreviate's whole-number floor would turn a 2.5
/// gridline into "2" on the fractional-ceiling wizard charts.
std::string tick_label(float v)
{
    if (std::fabs(v) >= 1000.0f)
        return fmt::abbreviate(static_cast<double>(v));
    char buf[24];
    std::snprintf(buf, sizeof buf, "%g", static_cast<double>(v));
    return buf;
}

/// Rows a left-to-right below-plot legend flow needs at @p flow_w. Shared by
/// measure_chart (the reserve) and draw_bars (the draw) so they cannot disagree.
int legend_flow_rows(const bar* bars, std::size_t count, const char* fmt, float flow_w)
{
    int   rows = 0;
    float x    = 0.0f;
    for (std::size_t i = 0; i < count; ++i)
    {
        if (bars[i].label[0] == '\0')
            continue;
        char buf[80];
        legend_entry(buf, sizeof buf, bars[i], fmt);
        const float w = 16.0f + ImGui::CalcTextSize(buf).x + 18.0f;
        if (rows == 0 || x + w > flow_w)
        {
            ++rows;
            x = 0.0f;
        }
        x += w;
    }
    return rows;
}

} // namespace

chart_metrics measure_chart(float box_w, const char* title,
                            const bar* bars, std::size_t count,
                            const char* fmt, float chart_h)
{
    chart_metrics m;
    m.chart_h = chart_h;

    // The title is indented by the tick gutter, so it wraps against the rest of
    // the box width. More than two lines is elided at draw time, not counted.
    const float title_w = std::max(40.0f, box_w - gutter - 8.0f);
    const float line_h  = ImGui::GetTextLineHeight();
    const float wrapped = ImGui::CalcTextSize(title, nullptr, false, title_w).y;
    m.title_lines = (wrapped > line_h * 1.5f) ? 2 : 1;

    // The legend goes below when a beside placement would squeeze the column
    // band under a legible floor. This width test lives HERE, in the measure the
    // caller opts into — never inside draw_bars (the tile-selection goldens).
    const float lw = legend_width(bars, count, fmt);
    if (lw > 0.0f && (box_w - gutter - lw) < 120.0f)
    {
        m.place       = legend_place::below;
        m.legend_rows = legend_flow_rows(bars, count, fmt, std::max(60.0f, box_w - gutter));
    }
    return m;
}

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

float chart_row_height(const chart_metrics& m)
{
    const float line_h = ImGui::GetTextLineHeightWithSpacing();
    float h = chart_row_height(m.chart_h);
    h += static_cast<float>(m.title_lines - 1) * line_h;
    if (m.legend_rows > 0)
        h += static_cast<float>(m.legend_rows) * 20.0f + 6.0f;
    return h;
}

namespace {

/// A stacked column's total — the sum of the segments that will actually be
/// drawn, so the axis ceiling and the stack cannot disagree about the top.
float column_total(const stack_segment* segs, std::size_t count)
{
    float t = 0.0f;
    for (std::size_t i = 0; i < count; ++i)
        if (segs[i].value > 0.0f)
            t += segs[i].value;
    return t;
}

/// One stacked column, bottom-up in array order, each drawn segment carrying an
/// InvisibleButton so it can name itself on hover.
///
/// A LONE segment is a plain bar and keeps a plain bar's manners: a rounded cap,
/// a 2 px floor, and it draws even at zero. A stack of several skips its zeros —
/// that is what gives the corp grain its variable segment count without a second
/// code path (see the header note on the solvency boundary).
void draw_stack(ImDrawList* dl, float cx, float base_y, float box_h, float bw,
                float ceiling, const stack_segment* segs, std::size_t count, int column)
{
    const bool  lone     = (count == 1);
    const float rounding = lone ? 1.5f : 0.0f;
    const float floor_h  = lone ? 2.0f : 1.0f;

    float y = base_y;
    for (std::size_t i = 0; i < count; ++i)
    {
        if (!lone && segs[i].value <= 0.0f)
            continue;
        const float  bh = std::max(floor_h, box_h * (segs[i].value / ceiling));
        const ImVec2 b0{cx - bw * 0.5f, y - bh};
        const ImVec2 b1{cx + bw * 0.5f, y};
        dl->AddRectFilled(b0, b1, segs[i].colour, rounding);

        char btn_id[32];
        std::snprintf(btn_id, sizeof btn_id, "##stack_%d_%d", column, static_cast<int>(i));
        ImGui::SetCursorScreenPos(b0);
        ImGui::InvisibleButton(btn_id, {bw, bh});
        if (ImGui::IsItemHovered())
        {
            if (segs[i].detail != nullptr && segs[i].detail[0] != '\0')
                ImGui::SetTooltip("%s: %.1f\n%s", segs[i].label,
                                  static_cast<double>(segs[i].value), segs[i].detail);
            else
                ImGui::SetTooltip("%s: %.1f", segs[i].label,
                                  static_cast<double>(segs[i].value));
        }
        y -= bh;
    }
}

} // namespace

void draw_stacked_columns(ImDrawList* dl, ImVec2 mn, ImVec2 mx,
                          const stack_segment* left,  std::size_t left_count,
                          const stack_segment* right, std::size_t right_count,
                          float bar_cap,
                          const char* left_caption, const char* right_caption,
                          bool tight_axis)
{
    const float peak = std::max(column_total(left, left_count),
                                column_total(right, right_count));
    const float top  = (peak > 0.0f) ? peak : 1.0f;
    const float ceiling = tight_axis ? tight_ceil(top) : nice_ceil(top);

    const bool  captioned = (left_caption != nullptr) || (right_caption != nullptr);
    // The caption strip is carved out of the plot, never added to it: the host
    // budgeted this rectangle and the chart must stay inside it.
    const float cap_h  = captioned ? ImGui::GetTextLineHeight() + 2.0f : 0.0f;
    const float base_y = mx.y - cap_h;
    const float box_w  = mx.x - mn.x;
    const float box_h  = base_y - mn.y;
    const float gap    = box_w / 2.0f;
    // Auto-fit (bar_cap 0) is held under a fifth of the plot's height as well as
    // under half its width: on a full-canvas host gap * 0.6 is several hundred
    // pixels, and two columns that wide read as blocks rather than as a chart.
    const float bw     = (bar_cap > 0.0f) ? std::min(bar_cap, gap * 0.6f)
                                          : std::min(gap * 0.6f, box_h * 0.2f);
    if (box_h <= 0.0f || box_w <= 0.0f)
        return;

    dl->AddRect(mn, mx, IM_COL32(70, 70, 78, 255));

    const float lx = mn.x + gap * 0.5f;
    const float rx = mn.x + gap * 1.5f;
    draw_stack(dl, lx, base_y, box_h, bw, ceiling, left,  left_count,  0);
    draw_stack(dl, rx, base_y, box_h, bw, ceiling, right, right_count, 1);

    if (captioned)
    {
        const auto caption = [&](float cx, const char* text) {
            if (text == nullptr || text[0] == '\0')
                return;
            // fit-exempt: a two-word baseline caption under a column whose width
            // is the host's, degraded by centring rather than by elision.
            const float w = ImGui::CalcTextSize(text).x;
            dl->AddText({cx - w * 0.5f, base_y + 1.0f}, IM_COL32(190, 190, 198, 255), text);
        };
        caption(lx, left_caption);
        caption(rx, right_caption);
    }
}

void draw_bars(ImDrawList* dl, ImVec2 mn, ImVec2 mx,
               const bar* bars, std::size_t count,
               float ceiling, const char* fmt, float bar_cap,
               legend_place place)
{
    if (count == 0 || ceiling <= 0.0f)
        return;

    // Tick gutter measured against the widest label it will hold — the fixed
    // 40 px overran left of the box on 5-digit ceilings (BL-215). Labels format
    // through fmt::abbreviate so the gutter stays narrow on large ceilings.
    const std::string top_label = tick_label(ceiling);
    const float g = std::max(gutter, ui::fit_width(top_label.c_str()) + 6.0f);

    const float plot_x0 = mn.x + g;
    const float y0 = mn.y, y1 = mx.y;
    const ImU32 grid_col  = IM_COL32(120, 120, 120, 150);
    const ImU32 label_col = IM_COL32(150, 150, 150, 255);

    const auto y_of = [&](float v) { return y1 - (v / ceiling) * (y1 - y0); };

    // Tick gutter: floor, top, and a mid tick once the ceiling is large enough to
    // make one worth reading. The top tick's y is clamped into the box so it
    // stops printing into the title row.
    const auto tick = [&](float v) {
        const float ty = y_of(v);
        dotted_hline(dl, plot_x0, mx.x, ty, grid_col);
        const std::string s  = tick_label(v);
        const float       th = ImGui::GetTextLineHeight();
        const float       ly = std::max(mn.y, ty - th * 0.5f);
        ui::add_fit_text(dl, ui::text_box::chart_plot, "charts.bars.tick",
                         {plot_x0 - 6.0f, ly}, g - 6.0f, label_col, s.c_str(),
                         ui::text_align::right);
    };
    tick(0.0f);
    if (ceiling >= 100.0f)
        tick(ceiling * 0.5f);
    tick(ceiling);

    // Legend reserve, measured (BL-215): the columns are kept out of it, as are
    // threshold captions. A below/none placement frees the whole plot width.
    const float reserve = (place == legend_place::beside)
                          ? legend_width(bars, count, fmt) : 0.0f;

    // Clustered columns sharing the baseline. By default the columns auto-fit the
    // plot, so a wide chart fills its box rather than huddling a row of thin bars
    // against the left edge. A caller whose width is fixed by a mockup passes an
    // explicit bar_cap — the tile-selection graphs pass 34.0f, which is what keeps
    // them pixel-identical to the version this was extracted from.
    constexpr float gap      = 10.0f;
    const float cap  = (bar_cap > 0.0f) ? bar_cap : 96.0f;
    // The 12 px covers the fixed offsets between band and legend text (6 px bar
    // inset + trailing 4 px + final gap slack), so a full-band bar row still
    // leaves the measured legend its whole reserve.
    const float band = std::max(60.0f, (mx.x - plot_x0) - reserve
                                       - (reserve > 0.0f ? 12.0f : 0.0f));
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

    if (place == legend_place::none)
        return;

    const auto swatch = [&](float sx, float sy, const bar& b) {
        if (b.hollow)
            dl->AddRect({sx, sy + 2.0f}, {sx + 10.0f, sy + 12.0f}, b.colour, 0.0f, 0, 1.5f);
        else
            dl->AddRectFilled({sx, sy + 2.0f}, {sx + 10.0f, sy + 12.0f}, b.colour);
    };

    if (place == legend_place::below)
    {
        // Entries flow left-to-right under the plot, wrapping by the same
        // measurement legend_flow_rows counted the reserve from.
        const float flow_w = std::max(60.0f, mx.x - plot_x0);
        float lx = plot_x0;
        float ly = y1 + 4.0f;
        for (std::size_t i = 0; i < count; ++i)
        {
            if (bars[i].label[0] == '\0')
                continue;
            char buf[80];
            legend_entry(buf, sizeof buf, bars[i], fmt);
            const float w = 16.0f + ImGui::CalcTextSize(buf).x + 18.0f;
            if (lx > plot_x0 && lx + w > plot_x0 + flow_w)
            {
                lx  = plot_x0;
                ly += 20.0f;
            }
            swatch(lx, ly, bars[i]);
            ui::add_fit_text(dl, ui::text_box::chart_plot, "charts.bars.legend_below",
                             {lx + 16.0f, ly}, flow_w - 16.0f,
                             IM_COL32(210, 210, 210, 255), buf);
            lx += w;
        }
        return;
    }

    // Legend to the right of the columns: swatch + label + value, one row each.
    const float lx = plot_x0 + 6.0f + static_cast<float>(count) * (bar_w + gap) + 4.0f;
    float ly = y0 + 2.0f;
    for (std::size_t i = 0; i < count; ++i)
    {
        if (bars[i].label[0] == '\0')
            continue;
        swatch(lx, ly, bars[i]);
        char buf[80];
        legend_entry(buf, sizeof buf, bars[i], fmt);
        ui::add_fit_text(dl, ui::text_box::chart_plot, "charts.bars.legend",
                         {lx + 16.0f, ly}, mx.x - (lx + 16.0f),
                         IM_COL32(210, 210, 210, 255), buf);
        ly += 20.0f;
    }
}

void draw_value_bar(ImDrawList* dl, ImVec2 mn, ImVec2 mx,
                    float value, float ceiling, ImU32 colour, const char* fmt)
{
    dl->AddRectFilled(mn, mx, IM_COL32(48, 50, 58, 255), 2.0f); // track

    char buf[32];
    std::snprintf(buf, sizeof(buf), fmt, static_cast<double>(value));
    const ImVec2 ts = ImGui::CalcTextSize(buf);

    // Reserve the figure's measured width (never under 52 px) so it can be neither
    // truncated nor collided with by the bar.
    const float plot_w = (mx.x - mn.x) - std::max(52.0f, ts.x) - 6.0f;
    const float frac   = (ceiling > 0.0f)
                         ? std::clamp(std::fabs(value) / ceiling, 0.0f, 1.0f) : 0.0f;
    // A 2 px stub at the origin when the value (or the ceiling) is zero, so zero reads
    // as zero rather than as an absent bar.
    const float fill_w = std::min(std::max(plot_w * frac, 2.0f), std::max(plot_w, 0.0f));
    if (plot_w > 0.0f)
        dl->AddRectFilled(mn, {mn.x + fill_w, mx.y}, colour, 2.0f);

    dl->AddText({mx.x - ts.x, mn.y + (mx.y - mn.y - ts.y) * 0.5f}, // fit-exempt: the figure's measured width is reserved above, never truncated
                IM_COL32(225, 228, 235, 255), buf);
}

void draw_stacked_band(ImDrawList* dl, ImVec2 mn, ImVec2 mx,
                       const band_layer* layers, std::size_t layer_count)
{
    if (!layers || layer_count == 0)
        return;

    std::size_t points = 0;
    for (std::size_t l = 0; l < layer_count; ++l)
        points = std::max(points, layers[l].count);
    if (points == 0)
        return;

    const float w      = mx.x - mn.x;
    const float h      = mx.y - mn.y;
    const float col_w  = w / static_cast<float>(points);
    // A hairline gap between columns keeps adjacent samples countable; below
    // ~3 px per column the gap would eat the fill, so it degrades to none.
    const float gap = (col_w >= 3.0f) ? 1.0f : 0.0f;

    for (std::size_t i = 0; i < points; ++i)
    {
        float sum = 0.0f;
        for (std::size_t l = 0; l < layer_count; ++l)
        {
            // Right-align short layers: layer sample j sits at column
            // (points - count + j): a layer that began recording late aligns to
            // the recent end.
            const std::size_t off = points - layers[l].count;
            if (i >= off && layers[l].values)
                sum += std::max(0.0f, layers[l].values[i - off]);
        }
        if (sum <= 0.0f)
            continue; // an honest gap — no decisions this sample

        const float x0 = mn.x + col_w * static_cast<float>(i);
        const float x1 = x0 + col_w - gap;

        float y = mx.y; // stack upward from the baseline
        for (std::size_t l = 0; l < layer_count; ++l)
        {
            const std::size_t off = points - layers[l].count;
            const float v = (i >= off && layers[l].values)
                                ? std::max(0.0f, layers[l].values[i - off]) : 0.0f;
            if (v <= 0.0f)
                continue;
            const float seg = (v / sum) * h;
            dl->AddRectFilled({x0, y - seg}, {x1, y}, layers[l].colour);
            y -= seg;
        }
    }
}

void threshold_line(ImDrawList* dl, ImVec2 mn, ImVec2 mx,
                    float value, float ceiling, ImU32 colour, const char* caption,
                    float legend_reserve)
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
        // Right-aligned to the COLUMN band, not the box: the right of the box is
        // the legend's. The caller passes the RESOLVED reserve its draw_bars
        // used, so caption and band cannot disagree by construction (BL-215).
        // Degrade ladder: in-band → below the line at plot_x0 → elide-with-record.
        const float band_x1 = std::max(plot_x0 + 60.0f,
                                       mx.x - std::max(0.0f, legend_reserve));
        if (ts.x + 2.0f <= band_x1 - plot_x0)
        {
            dl->AddText({band_x1 - ts.x - 2.0f, ty - ts.y - 1.0f}, colour, caption); // fit-exempt: measured to fit the band two lines up
        }
        else if (ts.x <= mx.x - plot_x0)
        {
            dl->AddText({plot_x0, ty + 1.0f}, colour, caption); // fit-exempt: measured to fit the plot width one line up
        }
        else
        {
            ui::add_fit_text(dl, ui::text_box::chart_plot, "charts.threshold.caption",
                             {plot_x0, ty + 1.0f}, mx.x - plot_x0, colour, caption);
        }
    }
}

} // namespace ui::charts
