#pragma once

// The one wait surface (BL-1072, a wizard wait says what it is doing).
//
// STARTUP.md § A wait never looks stopped, and it says what it is doing (Ben,
// 2026-09-24): a caption under the bar names the step under way, every long
// pass reports progress within itself so the bar moves for as long as the wait
// lasts, and an elapsed-seconds count under it shows the run is alive. The
// outer bar is weighted by what each step costs (`generation_progress::
// fraction`), never counted as equal steps.
//
// ONE WAIT, NOT TWO. Every wizard loading round ("Loading the X round") and the
// building screen (`app::draw_building_screen`) draw their bars through this
// one function, so the two surfaces cannot drift into two different waits.
// What either screen draws AROUND it (the round's title line, the building
// screen's carve) is that screen's own.

struct generation_progress;

namespace ui {

/// THE TOP BAND (STARTUP.md § Main menu; Ben, 2026-10-08): every loading bar
/// -- the cold build, the wait on a wizard round, the settle -- sits about 15%
/// of the window's height from the top, horizontally centred, never at the
/// window's vertical centre. The one position every wait surface reads; no
/// screen carries its own literal.
inline constexpr float k_loading_band_frac = 0.15f;

/// Screen-space y the outer loading bar's top edge sits on.
float loading_bar_y();

/// Pad the current window down (a Dummy, never a cursor jump) so the next
/// item lands @p lead_px above the loading band. A caller draws its title in
/// that lead; `draw_generation_wait` then pads the last stretch to the bar.
/// Does nothing when the cursor is already at or below that line.
void pad_to_loading_band(float lead_px);

/// Draw the wait's bars, caption and elapsed count, centred in @p pane_w.
///
/// @param prog          The run's sink. Read with atomic loads, except
///                      `wait_shown` (the bar's high-water mark, render-thread
///                      only) which this updates so the bar never steps back.
/// @param pane_w        Width to centre in; the bars are at most 420 px wide.
/// @param caption       Overrides the stage label when non-null (the building
///                      screen's validation run: "Proving the field").
/// @param live_clock    False under `--verify`: the elapsed count reads 0 so a
///                      capture never races the wall clock.
void draw_generation_wait(generation_progress& prog, float pane_w,
                          const char* caption = nullptr, bool live_clock = true);

} // namespace ui
