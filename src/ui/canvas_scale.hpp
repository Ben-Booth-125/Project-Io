#pragma once

#include <imgui.h>

namespace ui {

/// Draw the shared scale-bar + zoom-slider overlay pinned to the bottom-centre of
/// a primary canvas region. Both the Solar and Circumplanetary canvases call it
/// so the scale/zoom control reads and behaves identically on each rung.
///
/// The scale bar is a fixed fraction of the canvas width; the AU distance it
/// spans is derived from @p px_per_au and shown to two decimals at the current
/// zoom. The zoom slider runs **left = zoomed out** (@p zoom_min) to **right =
/// zoomed in** (@p zoom_max), logarithmically, and writes the new factor back to
/// @p zoom when the player drags it. The slider carries no value text — the scale
/// bar already reports the distance.
///
/// The overlay is itself an ImGui window, so it handles its own input regardless
/// of the canvas's input gating; call it once per frame for the primary canvas.
///
/// @param id_suffix Unique ImGui id suffix for the overlay window (one per canvas).
/// @param origin    Canvas top-left, screen pixels.
/// @param size      Canvas size, screen pixels.
/// @param px_per_au Current pixels-per-AU (the auto-fit AU->pixel scale times zoom).
/// @param zoom      [in,out] Current zoom factor; updated when the slider moves.
/// @param zoom_min  Minimum zoom (slider left end; most zoomed out).
/// @param zoom_max  Maximum zoom (slider right end; most zoomed in).
void draw_scale_zoom_overlay(const char* id_suffix, ImVec2 origin, ImVec2 size,
                             float px_per_au, float& zoom, float zoom_min, float zoom_max);

} // namespace ui
