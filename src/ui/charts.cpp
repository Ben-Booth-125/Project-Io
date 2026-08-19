#include "charts.hpp"

#include "format.hpp" // date_from_day (Year/Quarter axis), abbreviate (axis + readout labels)
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

namespace {

// Peak (max) of the samples one axis carries; 0 when the axis has no series.
float axis_peak(const time_series* series, std::size_t n, bool right_axis)
{
    float peak = 0.0f;
    for (std::size_t s = 0; s < n; ++s)
    {
        if (series[s].right_axis != right_axis || series[s].values == nullptr)
            continue;
        for (std::size_t i = 0; i < series[s].count; ++i)
            peak = std::max(peak, series[s].values[i]);
    }
    return peak;
}

// Whether any series is bound to the given axis.
bool axis_used(const time_series* series, std::size_t n, bool right_axis)
{
    for (std::size_t s = 0; s < n; ++s)
        if (series[s].right_axis == right_axis && series[s].values != nullptr)
            return true;
    return false;
}

} // namespace

void draw_time_series(ImDrawList* dl, ImVec2 mn, ImVec2 mx,
                      const time_series* series, std::size_t series_count,
                      const std::uint64_t* day_ticks, std::size_t point_count,
                      bool log_scale, ImVec2 hover)
{
    if (series_count == 0 || point_count == 0 || mx.x <= mn.x || mx.y <= mn.y)
        return;

    const float line_h = ImGui::GetTextLineHeight();

    // --- Plot geometry ---
    // A legend row caps the top; a two-line Year/Quarter axis floors the bottom; a
    // tick gutter flanks each side that carries an axis (the right gutter collapses
    // when no series is bound to the right axis).
    const bool  has_left  = axis_used(series, series_count, false);
    const bool  has_right = axis_used(series, series_count, true);
    const float legend_h  = line_h + 6.0f;
    const float axis_h    = day_ticks ? (line_h * 2.0f + 6.0f) : (line_h + 4.0f);

    const ImU32 grid_col  = IM_COL32(120, 120, 120, 130);
    const ImU32 label_col = IM_COL32(150, 150, 150, 255);
    const ImU32 axis_col  = IM_COL32(170, 170, 170, 255);

    // --- Per-axis scaling. Linear: [0, nice_ceil(peak)]. Log: [decade_lo, decade_hi]. ---
    // Resolved BEFORE the plot geometry so each gutter can be measured against
    // the widest label its axis will carry (BL-215 — the fixed 40 px gutter lost
    // the right axis's tick at px1 + 5 and overran on 5-digit ceilings).
    const float peak_l = axis_peak(series, series_count, false);
    const float peak_r = axis_peak(series, series_count, true);

    const auto decade_lo = [](float peak) {
        if (peak <= 0.0f) return 1.0f;
        // One decade below the peak's decade, floored at 1 — a readable log window.
        const float hi = std::pow(10.0f, std::ceil(std::log10(peak)));
        return std::max(1.0f, hi / 1000.0f);
    };
    const auto decade_hi = [](float peak) {
        if (peak <= 0.0f) return 10.0f;
        return std::pow(10.0f, std::ceil(std::log10(std::max(peak, 1.0f))));
    };

    const float ceil_l = log_scale ? decade_hi(peak_l) : nice_ceil(peak_l);
    const float ceil_r = log_scale ? decade_hi(peak_r) : nice_ceil(peak_r);
    const float lo_l   = log_scale ? decade_lo(peak_l) : 0.0f;
    const float lo_r   = log_scale ? decade_lo(peak_r) : 0.0f;

    const auto gutter_for = [&](float ceil) {
        const std::string s = fmt::abbreviate(static_cast<double>(ceil));
        return std::max(gutter, ui::fit_width(s.c_str()) + 6.0f);
    };
    const float l_gutter  = has_left  ? gutter_for(ceil_l) : 6.0f;
    const float r_gutter  = has_right ? gutter_for(ceil_r) : 6.0f;

    const float px0 = mn.x + l_gutter;
    const float px1 = mx.x - r_gutter;
    const float py0 = mn.y + legend_h;
    const float py1 = mx.y - axis_h;
    if (px1 <= px0 + 8.0f || py1 <= py0 + 8.0f)
        return; // too small to plot legibly

    // Value → y, per axis. Log maps in log-space between the decade window.
    const auto y_of = [&](float v, bool right) {
        const float ceil = right ? ceil_r : ceil_l;
        const float lo   = right ? lo_r   : lo_l;
        if (ceil <= 0.0f) return py1;
        float t;
        if (log_scale)
        {
            const float vv = std::max(v, lo);
            t = (std::log10(vv) - std::log10(lo)) / (std::log10(ceil) - std::log10(lo));
        }
        else
        {
            t = v / ceil;
        }
        t = std::clamp(t, 0.0f, 1.0f);
        return py1 - t * (py1 - py0);
    };

    // --- Y gridlines + axis ticks ---
    const auto axis_label = [&](float v) {
        return fmt::abbreviate(static_cast<double>(v));
    };
    const auto draw_axis = [&](bool right) {
        if (right ? !has_right : !has_left) return;
        const float ceil = right ? ceil_r : ceil_l;
        const float lo   = right ? lo_r   : lo_l;
        auto put = [&](float v, bool grid) {
            const float ty = y_of(v, right);
            if (grid) dotted_hline(dl, px0, px1, ty, grid_col);
            const std::string s = axis_label(v);
            const float ly = ty - ImGui::GetTextLineHeight() * 0.5f;
            if (right)
                ui::add_fit_text(dl, ui::text_box::chart_plot, "charts.series.tick_r",
                                 {px1 + 5.0f, ly}, r_gutter - 5.0f, label_col, s.c_str());
            else
                ui::add_fit_text(dl, ui::text_box::chart_plot, "charts.series.tick_l",
                                 {px0 - 5.0f, ly}, l_gutter - 5.0f, label_col, s.c_str(),
                                 ui::text_align::right);
        };
        if (log_scale)
        {
            for (float v = lo; v <= ceil * 1.0001f; v *= 10.0f) // one label per decade
                put(v, true);
        }
        else
        {
            put(0.0f, true);
            if (ceil >= 100.0f) put(ceil * 0.5f, true);
            put(ceil, true);
        }
    };
    draw_axis(false);
    draw_axis(true);

    // Plot frame baseline + right rule (so the two axes read as bounding the plot).
    dl->AddLine({px0, py1}, {px1, py1}, axis_col, 1.0f);

    // --- X slot geometry: one centred slot per sample ---
    const float step  = (px1 - px0) / static_cast<float>(point_count);
    const auto  xc    = [&](std::size_t i) { return px0 + (static_cast<float>(i) + 0.5f) * step; };

    // --- Bars first (background), then lines over them ---
    std::size_t bar_series = 0;
    for (std::size_t s = 0; s < series_count; ++s)
        if (!series[s].line && series[s].values) ++bar_series;

    // Thick bars, tight gaps (Ben's 2026-07-23): the slot is filled ~0.88 wide and the
    // bar ~0.96 of its share of the slot — chunkier columns with less air between them
    // than the earlier 0.72/0.90.
    std::size_t bar_ord = 0;
    const float slot_w  = step * 0.88f;
    for (std::size_t s = 0; s < series_count; ++s)
    {
        const time_series& ts = series[s];
        if (ts.line || ts.values == nullptr) continue;
        const float bw = (bar_series > 0) ? slot_w / static_cast<float>(bar_series) : slot_w;
        for (std::size_t i = 0; i < ts.count; ++i)
        {
            const float cx = xc(ts.offset + i) - slot_w * 0.5f + (static_cast<float>(bar_ord) + 0.5f) * bw;
            const ImVec2 a{cx - bw * 0.48f, y_of(ts.values[i], ts.right_axis)};
            const ImVec2 b{cx + bw * 0.48f, py1};
            dl->AddRectFilled(a, b, ts.colour);
        }
        ++bar_ord;
    }
    for (std::size_t s = 0; s < series_count; ++s)
    {
        const time_series& ts = series[s];
        if (!ts.line || ts.values == nullptr || ts.count == 0) continue;
        for (std::size_t i = 1; i < ts.count; ++i)
            dl->AddLine({xc(ts.offset + i - 1), y_of(ts.values[i - 1], ts.right_axis)},
                        {xc(ts.offset + i),     y_of(ts.values[i],     ts.right_axis)},
                        ts.colour, 2.0f);
        for (std::size_t i = 0; i < ts.count; ++i)
            dl->AddCircleFilled({xc(ts.offset + i), y_of(ts.values[i], ts.right_axis)}, 2.5f, ts.colour);
    }

    // --- Hierarchical Year / Quarter X axis ---
    if (day_ticks)
    {
        // Quarter labels are only drawn when the slots are wide enough to hold "Qn"
        // without overlapping; the year band is always drawn, centred under its run.
        const bool show_q = step >= ImGui::CalcTextSize("Q4").x + 4.0f;
        int   run_year    = -100000;
        float run_x0      = px0;
        float last_yr_x1  = -1e9f; // right edge of the last drawn year label (collision guard)
        const float q_y   = py1 + 2.0f;
        const float yr_y  = py1 + 2.0f + line_h;

        for (std::size_t i = 0; i < point_count; ++i)
        {
            const fmt::calendar_date d = fmt::date_from_day(day_ticks[i]);
            if (show_q)
            {
                char qb[4];
                std::snprintf(qb, sizeof qb, "Q%d", d.quarter);
                const ImVec2 ts = ImGui::CalcTextSize(qb);
                dl->AddText({xc(i) - ts.x * 0.5f, q_y}, label_col, qb); // fit-exempt: show_q gates on measured slot width
            }
            // Close a year run when the year changes or at the last sample; label it
            // centred under its run, but only when it clears the previous label — so
            // a 16-year span thins to every Nth year instead of an unreadable smear.
            const bool last = (i + 1 == point_count);
            const int  next_year = last ? run_year - 1
                                        : fmt::date_from_day(day_ticks[i + 1]).year;
            if (i == 0) { run_year = d.year; run_x0 = xc(i); }
            if (d.year != run_year || last)
            {
                const float run_x1 = xc(i);
                char yb[8];
                std::snprintf(yb, sizeof yb, "%d", run_year);
                const ImVec2 ts = ImGui::CalcTextSize(yb);
                const float mid = (run_x0 + run_x1) * 0.5f;
                if (mid - ts.x * 0.5f >= last_yr_x1 + 6.0f)
                {
                    dl->AddText({mid - ts.x * 0.5f, yr_y}, axis_col, yb); // fit-exempt: collision guard thins labels by measurement
                    last_yr_x1 = mid + ts.x * 0.5f;
                }
                run_year = last ? next_year : d.year;
                run_x0   = xc(i);
            }
        }
    }

    // --- Inline legend row (dot + label, left→right) at the top ---
    {
        float lx = px0;
        const float ly = mn.y + 2.0f;
        for (std::size_t s = 0; s < series_count; ++s)
        {
            if (series[s].label[0] == '\0') continue;
            dl->AddCircleFilled({lx + 5.0f, ly + line_h * 0.5f}, 4.0f, series[s].colour);
            ui::add_fit_text(dl, ui::text_box::chart_plot, "charts.series.legend",
                             {lx + 13.0f, ly}, px1 - (lx + 13.0f),
                             IM_COL32(210, 210, 210, 255), series[s].label);
            lx += 13.0f + ImGui::CalcTextSize(series[s].label).x + 18.0f;
        }
    }

    // --- Hover crosshair: snap to the nearest sample, mark each series, read out ---
    if (hover.x >= px0 && hover.x <= px1 && hover.y >= py0 && hover.y <= py1)
    {
        std::size_t idx = 0;
        if (step > 0.0f)
            idx = std::min(point_count - 1,
                           static_cast<std::size_t>(std::max(0.0f, (hover.x - px0) / step)));
        const float cx = xc(idx);
        dl->AddLine({cx, py0}, {cx, py1}, IM_COL32(230, 230, 230, 120), 1.0f);

        // Readout box: the sample's date + each series' value at idx.
        std::string lines[8];
        std::size_t nlines = 0;
        if (day_ticks && nlines < 8)
        {
            const fmt::calendar_date d = fmt::date_from_day(day_ticks[idx]);
            char db[24];
            std::snprintf(db, sizeof db, "%d Q%d", d.year, d.quarter);
            lines[nlines++] = db;
        }
        for (std::size_t s = 0; s < series_count && nlines < 8; ++s)
        {
            const time_series& ts = series[s];
            if (ts.values == nullptr || idx < ts.offset || idx - ts.offset >= ts.count) continue;
            const std::size_t li = idx - ts.offset;
            dl->AddCircleFilled({cx, y_of(ts.values[li], ts.right_axis)}, 3.5f, ts.colour);
            const char* lbl = ts.label[0] ? ts.label : "value";
            lines[nlines++] = std::string(lbl) + ": " + fmt::abbreviate(static_cast<double>(ts.values[li]));
        }

        float bw = 0.0f;
        for (std::size_t i = 0; i < nlines; ++i)
            bw = std::max(bw, ImGui::CalcTextSize(lines[i].c_str()).x);
        const float bh = static_cast<float>(nlines) * line_h + 8.0f;
        bw += 12.0f;
        // Place the box to the side of the crosshair with more room, clamped to the
        // plot. Guard the clamp against an inverted range: a readout taller/wider
        // than the plot (small card) would make hi < lo, which is UB in std::clamp —
        // pin to the origin edge instead.
        float bx = (cx + 8.0f + bw <= px1) ? cx + 8.0f : cx - 8.0f - bw;
        bx = (px1 - bw < px0) ? px0 : std::clamp(bx, px0, px1 - bw);
        const float by = (py1 - bh < py0) ? py0
                                          : std::clamp(hover.y - bh * 0.5f, py0, py1 - bh);
        dl->AddRectFilled({bx, by}, {bx + bw, by + bh}, IM_COL32(20, 20, 24, 235), 3.0f);
        dl->AddRect({bx, by}, {bx + bw, by + bh}, IM_COL32(90, 90, 100, 255), 3.0f);
        for (std::size_t i = 0; i < nlines; ++i)
            dl->AddText({bx + 6.0f, by + 4.0f + static_cast<float>(i) * line_h}, // fit-exempt: readout box is sized to its own measured lines
                        IM_COL32(220, 220, 220, 255), lines[i].c_str());
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
            // (points - count + j), the same "began recording late" convention
            // as time_series::offset.
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
