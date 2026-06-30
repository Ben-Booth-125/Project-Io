#pragma once

#include "world/components.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <unordered_map>
#include <vector>

namespace ui {

// Colourblind-safe plot colours (Okabe-Ito palette).
inline constexpr ImVec4 plot_col_blue   = {0.000f, 0.447f, 0.698f, 1.0f}; // #0072B2
inline constexpr ImVec4 plot_col_orange = {0.902f, 0.624f, 0.000f, 1.0f}; // #E69F00
inline constexpr ImVec4 plot_col_green  = {0.000f, 0.620f, 0.451f, 1.0f}; // #009E73

/// Maximum retained samples across all plot series (coordinates the retention
/// window for the v0.0.9 data-creep audit — one place to tighten or widen).
inline constexpr std::size_t plot_history_cap = 64;

/// Push v onto series, capping at plot_history_cap by dropping the oldest entry.
inline void push_capped(std::vector<float>& series, float v)
{
    series.push_back(v);
    if (series.size() > plot_history_cap)
        series.erase(series.begin());
}

/// Per-resource price / supply / demand time series for one market.
struct resource_plot_series
{
    std::vector<float> price;
    std::vector<float> supply;
    std::vector<float> demand;
};

/// Market history: one resource_plot_series per resource type, keyed by market entity_id.
using market_plot_history =
    std::unordered_map<entity_id, std::array<resource_plot_series, resource_count>>;

/// Draw an ImGui::PlotLines for series with auto y-range (flat-line guarded) and
/// an optional colour pushed around the call.
///
/// @param label   ImGui widget label (use "##..." for hidden labels).
/// @param series  Data points, oldest→newest.
/// @param size    Plot size; -1 width = full available width.
/// @param colour  Optional Okabe-Ito (or other) colour to push around the call.
/// @param overlay Text rendered in the centre of the plot (e.g. a current value).
inline void draw_plot(const char*          label,
                      const std::vector<float>& series,
                      ImVec2               size    = {-1.0f, 60.0f},
                      const ImVec4*        colour  = nullptr,
                      const char*          overlay = nullptr)
{
    if (series.empty())
    {
        ImGui::TextDisabled("(no data yet)");
        return;
    }
    float lo = *std::min_element(series.begin(), series.end());
    float hi = *std::max_element(series.begin(), series.end());
    if (lo == hi) { lo -= 1.0f; hi += 1.0f; } // flat-line guard
    if (colour)
        ImGui::PushStyleColor(ImGuiCol_PlotLines, *colour);
    ImGui::PlotLines(label, series.data(), static_cast<int>(series.size()),
                     0, overlay, lo, hi, size);
    if (colour)
        ImGui::PopStyleColor();
}

} // namespace ui
