#include "charts.hpp"

#include "format.hpp" // date_from_day (Year/Quarter axis), abbreviate (axis + readout labels)

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

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
    const float l_gutter  = has_left  ? gutter : 6.0f;
    const float r_gutter  = has_right ? gutter : 6.0f;

    const float px0 = mn.x + l_gutter;
    const float px1 = mx.x - r_gutter;
    const float py0 = mn.y + legend_h;
    const float py1 = mx.y - axis_h;
    if (px1 <= px0 + 8.0f || py1 <= py0 + 8.0f)
        return; // too small to plot legibly

    const ImU32 grid_col  = IM_COL32(120, 120, 120, 130);
    const ImU32 label_col = IM_COL32(150, 150, 150, 255);
    const ImU32 axis_col  = IM_COL32(170, 170, 170, 255);

    // --- Per-axis scaling. Linear: [0, nice_ceil(peak)]. Log: [decade_lo, decade_hi]. ---
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
            const ImVec2 ts = ImGui::CalcTextSize(s.c_str());
            const float tx = right ? (px1 + 5.0f) : (px0 - 5.0f - ts.x);
            dl->AddText({tx, ty - ts.y * 0.5f}, label_col, s.c_str());
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
                dl->AddText({xc(i) - ts.x * 0.5f, q_y}, label_col, qb);
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
                    dl->AddText({mid - ts.x * 0.5f, yr_y}, axis_col, yb);
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
            dl->AddText({lx + 13.0f, ly}, IM_COL32(210, 210, 210, 255), series[s].label);
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
            dl->AddText({bx + 6.0f, by + 4.0f + static_cast<float>(i) * line_h},
                        IM_COL32(220, 220, 220, 255), lines[i].c_str());
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
