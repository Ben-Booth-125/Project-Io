#pragma once

#include "imgui.h"

#include <cstddef>
#include <cstdint>

// ---------------------------------------------------------------------------
// Shared chart primitives
//
// Extracted from selection_panel.cpp's tile-deposit graphs (BL-123, Ben's
// mockup) so every chart in the app is drawn by the same code rather than by
// imitation. The visual language is:
//
//   * one bordered child container per chart, its header reading as the chart's
//     title (the caller owns the container; see chart_row_height);
//   * a left gutter of tick labels with faint dotted gridlines;
//   * clustered columns sharing a baseline, so heights compare directly;
//   * a small swatch legend naming each value.
//
// Anything drawing bars should call in here, not roll its own.
// ---------------------------------------------------------------------------

namespace ui::charts {

/// Width of the tick-label gutter. Callers indent chart headers by this so the
/// title sits above the plot origin rather than above the labels.
inline constexpr float gutter = 40.0f;

/// Round @p v up to a 'nice' axis ceiling (1 / 2 / 5 x a power of ten): 45->50,
/// 18->20, 130->200. Gives the bar a legible top gridline instead of a ragged max.
float nice_ceil(float v);

/// A finer ladder than nice_ceil (1 / 1.5 / 2 / 2.5 / 3 / 4 / 5 / 6 / 8 / 10).
/// The coarse one can waste half the plot height — a peak of 2.45 rounds to 5 —
/// which matters on a chart whose whole job is to show a value moving. Separate
/// from nice_ceil so the goldened tile-selection graphs stay put.
float tight_ceil(float v);

/// A faint dotted horizontal rule across a chart's gridline, drawn as short
/// dashes (ImDrawList has no native dashed line).
void dotted_hline(ImDrawList* dl, float x0, float x1, float y, ImU32 col);

/// One column in a clustered bar chart.
struct bar
{
    float       value = 0.0f;
    ImU32       colour = IM_COL32(150, 160, 190, 255);
    const char* label = "";     ///< Legend row text. Empty suppresses the legend row.
    bool        hollow = false; ///< Draw as an outline — used for "before" / reference values.
};

/// Where a chart's swatch legend sits (BL-215). The narrow-host mode is a CALLER
/// opt-in, never a width test inside draw_bars — an internal test would move the
/// goldened tile-selection graphs (host width ~238 px at 1280).
enum class legend_place : std::uint8_t { beside, below, none };

/// Width the swatch legend needs: 16 (swatch + gap) + the widest "label value" + 8.
/// Replaces the file-local `constexpr float legend_w = 190.0f`, which was a guess
/// unreconciled with the `lx` the legend actually draws at (BL-215).
float legend_width(const bar* bars, std::size_t count, const char* fmt);

/// Everything a chart row's geometry needs, answered from the box width the host
/// already knows — so the reserve and the draw cannot disagree (BL-215).
struct chart_metrics
{
    float        chart_h     = 0.0f;
    int          title_lines = 1;                    ///< 1 or 2; beyond 2 the title elides.
    int          legend_rows = 0;                    ///< 0 when the legend sits beside.
    legend_place place       = legend_place::beside;
};

/// Measure a chart row against @p box_w: how many lines the title wraps to, and
/// whether the legend fits beside the plot or must reflow below it.
chart_metrics measure_chart(float box_w, const char* title,
                            const bar* bars, std::size_t count,
                            const char* fmt, float chart_h);

/// Draw a clustered bar chart inside [@p mn, @p mx].
///
/// Generalises selection_panel.cpp's two-column tile-vs-benchmark graph to N
/// columns. At two bars with `bar_cap = 34.0f` it is pixel-identical to the
/// original, which is what lets the tile graphs share this code.
///
/// @param ceiling Axis top; spans the tallest column. Pass nice_ceil or tight_ceil.
/// @param fmt     printf format for the legend value (e.g. "%.1f", "%.0f%%").
/// @param bar_cap Maximum column width. 0 (the default) auto-fits the columns to
///                the plot, which is what a wide chart wants. The tile-selection
///                graphs pass 34.0f explicitly because their width is fixed by
///                the mockup they were drawn from.
/// @param place   Caller opt-in (from measure_chart): below reflows the legend
///                under the plot and frees the whole width for the columns.
void draw_bars(ImDrawList* dl, ImVec2 mn, ImVec2 mx,
               const bar* bars, std::size_t count,
               float ceiling, const char* fmt = "%.1f", float bar_cap = 0.0f,
               legend_place place = legend_place::beside);

/// One horizontal magnitude bar inside [@p mn, @p mx], for a narrow list where a
/// clustered vertical chart cannot fit (draw_bars reserves gutter + a 190px legend,
/// exceeding the fold-out column's ~278px row width). Length = |value| / ceiling;
/// colour is the caller's; the signed value prints right-aligned and is NEVER elided.
///
/// The reusable narrow-list idiom (BL-162's construction ledger is the first caller;
/// BL-176's building table wants the same shape). draw_bars cannot serve on two counts:
/// its gutter + legend exceed the row width outright, and its y_of() inverts the rect
/// for a negative value, drawing outside the clip box.
void draw_value_bar(ImDrawList* dl, ImVec2 mn, ImVec2 mx,
                    float value, float ceiling, ImU32 colour, const char* fmt = "%+.0f");

/// A horizontal threshold marker across the plot, with a right-aligned caption.
/// Used to show a gate on the same axis as the values it judges — the retention
/// shoreline, the mobile-lid band, the combustion floor.
///
/// @param legend_reserve The resolved legend reserve the host's draw_bars used
///        (legend_width(...) when beside, 0 when below/none), so caption and
///        column band cannot disagree by construction (BL-215). Degrades:
///        right-aligned within the band → left-aligned below the line →
///        elide-with-record at the plot width.
void threshold_line(ImDrawList* dl, ImVec2 mn, ImVec2 mx,
                    float value, float ceiling, ImU32 colour, const char* caption,
                    float legend_reserve);

/// Height of one chart row: a bordered child holding a single-line header plus a
/// @p chart_h -tall plot. Matches the tile-graph row metric exactly.
/// Kept VERBATIM — the tile-selection golden depends on it (BL-215).
float chart_row_height(float chart_h);

/// Measured-row overload: adds the extra title lines and the below-plot legend
/// rows measure_chart resolved for the host's width (BL-215).
float chart_row_height(const chart_metrics& m);

// ---------------------------------------------------------------------------
// Dual-axis time-series chart (BL-197)
//
// A reusable time-series plot the Selection band and the ledgers can host: an
// inline legend row, up to two INDEPENDENT Y axes (so a ~100 000-scale aggregate
// and a ~100-scale single series read on one chart, the Power BI reference Ben
// gave), a hierarchical Year / Quarter X axis derived from each sample's in-game
// day, an optional log scale, and an optional hover crosshair. Hand-drawn via
// ImDrawList in screen space, matching the house idiom (dotted_hline gridlines,
// nice_ceil axis tops, ASCII tick text). Bars vs line and left vs right axis are
// per-series, so the canonical "aggregate bars + this-item line" is two series.
// ---------------------------------------------------------------------------

/// One plotted series in a dual-axis time-series chart.
struct time_series
{
    const float* values     = nullptr;                    ///< count contiguous samples, oldest→newest.
    std::size_t  count       = 0;                          ///< Valid samples in `values` (≤ point_count).
    std::size_t  offset      = 0;                          ///< First X slot this series occupies. A series that began recording late (a lazily-tracked tile) sets offset = point_count - count so it aligns to the recent (right) end of the shared axis, not slot 0.
    ImU32        colour      = IM_COL32(120, 160, 230, 255);
    const char*  label       = "";                        ///< Legend text. Empty suppresses the legend entry.
    bool         line        = false;                     ///< false = columns (the aggregate), true = a line drawn over them (the single item).
    bool         right_axis  = false;                     ///< false = left Y axis, true = right Y axis; each axis auto-scales to its own series.
};

/// Draw a dual-axis time-series chart inside [@p mn, @p mx] (screen space).
///
/// Each Y axis auto-scales to the peak of the series bound to it (nice_ceil, or
/// decade steps under @p log_scale). @p day_ticks[i] is the in-game day of sample
/// i (sim_loop day count) — used only to label the hierarchical Year/Quarter X
/// axis via fmt::date_from_day; pass nullptr for a bare 0..N index axis.
///
/// @param series       Series array (bars and/or lines, left and/or right axis).
/// @param series_count Number of series.
/// @param day_ticks    Per-sample in-game day (point_count entries), or nullptr.
/// @param point_count  Number of X slots (the longest series' length).
/// @param log_scale    Log-scale both axes (decade gridlines) instead of linear.
/// @param hover        Mouse position (screen px) for the crosshair, or {-1,-1}
///                     for none. When inside the plot it snaps a vertical rule +
///                     per-series markers + a value readout to the nearest sample.
void draw_time_series(ImDrawList* dl, ImVec2 mn, ImVec2 mx,
                      const time_series* series, std::size_t series_count,
                      const std::uint64_t* day_ticks, std::size_t point_count,
                      bool log_scale = false, ImVec2 hover = ImVec2{-1.0f, -1.0f});

} // namespace ui::charts
