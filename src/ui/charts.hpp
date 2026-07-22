#pragma once

#include "imgui.h"

#include <cstddef>

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
void draw_bars(ImDrawList* dl, ImVec2 mn, ImVec2 mx,
               const bar* bars, std::size_t count,
               float ceiling, const char* fmt = "%.1f", float bar_cap = 0.0f);

/// A horizontal threshold marker across the plot, with a right-aligned caption.
/// Used to show a gate on the same axis as the values it judges — the retention
/// shoreline, the mobile-lid band, the combustion floor.
void threshold_line(ImDrawList* dl, ImVec2 mn, ImVec2 mx,
                    float value, float ceiling, ImU32 colour, const char* caption);

/// Height of one chart row: a bordered child holding a single-line header plus a
/// @p chart_h -tall plot. Matches the tile-graph row metric exactly.
float chart_row_height(float chart_h);

} // namespace ui::charts
