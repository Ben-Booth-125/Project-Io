#pragma once

#include <functional>
#include <imgui.h>

namespace ui {

/// Frames of stable hover required before the card appears (BL-060).
inline constexpr int kHoverDelay = 20;

/// Dwell-to-open (BL-200). Frames of pointer stillness over an entity before the
/// Selection band auto-opens; the dwell bar fills across the span from
/// kHoverDelay (glance is up) to here. A single feel tunable — flag for the owner.
inline constexpr int kDwellOpenTicks = 48;

/// Dwell-to-open (BL-200). Pointer movement beyond this many pixels resets the
/// dwell timer — the anti-bombardment gate that keeps a sweep from auto-opening.
inline constexpr float kDwellJitterPx = 4.0f;

/// Render a transient floating hover card near `cursor` when `hover_ticks`
/// has reached `kHoverDelay`. The card is a semi-opaque ImGui child window
/// (no title bar, 4 px rounding, max width 200 px) positioned just above the
/// cursor. `content` is called inside the window to render the body. When
/// `hover_ticks < kHoverDelay` the call is a no-op.
///
/// When `dwell_fraction` is strictly between 0 and 1, a thin dwell-to-open
/// progress bar (BL-200) is drawn at the foot of the card — the pre-open
/// indicator that fills as the pointer holds still. At 0 or ≥ 1 no bar is drawn
/// (nothing to signal, or the card is about to open). The bar lives here, in the
/// transient tooltip, never in the opened card's header (Ben, 2026-07-23).
///
/// @param cursor         Current mouse position in screen pixels.
/// @param hover_ticks    Frames the cursor has rested over the same entity.
/// @param content        Caller-supplied content renderer (ImGui calls only).
/// @param dwell_fraction Dwell-to-open progress in [0,1]; the bar shows only on (0,1).
void draw_hover_card(ImVec2 cursor, int hover_ticks,
                     const std::function<void()>& content,
                     float dwell_fraction = 0.0f);

} // namespace ui
