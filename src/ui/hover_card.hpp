#pragma once

#include <functional>
#include <imgui.h>

namespace ui {

/// Frames of stable hover before the card first APPEARS — a "glance" that still
/// tracks the live cursor (BL-230). Assumes a 60 Hz frame, matching the
/// existing frame-count convention (verify.frames() steps presentation frames
/// deterministically rather than real wall time). 30 frames @ 60 Hz = 0.5 s.
inline constexpr int kHoverAppearDelay = 30;

/// Total frames of stable hover (from the SAME start as kHoverAppearDelay)
/// before the card STICKS — freezes in place and starts the leave-to-dismiss
/// rule below. 150 frames @ 60 Hz = 2.5 s total, i.e. 2 s after it appears.
inline constexpr int kHoverStickDelay = 150;

/// How far outside the card's own rect the pointer may stray before the STUCK
/// card is dismissed (BL-228/230). The card is drawn *above* the cursor with a
/// gap, so the cursor that froze it starts outside the rect; this pad spans
/// that gap and leaves a forgiving margin for the trip up into the card.
/// Only applies once the card is stuck — see kHoverStickDelay.
inline constexpr float kHoverCardExitPadPx = 26.0f;

/// Render the floating hover card at `anchor` and report the rect it occupied.
///
/// The card is a semi-opaque ImGui window (no title bar, 4 px rounding, max
/// width 200 px) positioned just above `anchor`. `content` is called inside the
/// window to render the body.
///
/// **Two phases (BL-230).** Between `kHoverAppearDelay` and `kHoverStickDelay`
/// the card is a GLANCE: the caller re-passes the live cursor position each
/// frame, so it tracks the pointer like an ordinary tooltip and does not yet
/// block dismissal-by-distance. Past `kHoverStickDelay` the caller freezes
/// `anchor` at the position it stuck at and the card becomes read-stable —
/// it stops tracking the cursor and is dismissed only once the pointer leaves
/// the reported rect inflated by `kHoverCardExitPadPx`, so a long line can be
/// read to its end without the card sliding away.
///
/// **Z-order stays constant across both phases.** The window always carries
/// `ImGuiWindowFlags_NoInputs` and the `Tooltip` flag: it draws above every
/// other window (rendering pov) but never captures the pointer (cursor pov),
/// so canvas hover/click always resolves to whatever tile or marker is
/// beneath it, in either phase. Hovering never opens the Selection band —
/// that is the click's job alone. This retires BL-200's dwell-to-open and the
/// dwell progress bar that advertised it.
///
/// @param anchor   Screen position the card is drawn at — the live cursor
///                 during the glance phase, frozen once stuck (BL-230).
/// @param content  Caller-supplied content renderer (ImGui calls only).
/// @param out_min  Receives the card's top-left in screen pixels, if non-null.
/// @param out_max  Receives the card's bottom-right in screen pixels, if non-null.
void draw_hover_card(ImVec2 anchor,
                     const std::function<void()>& content,
                     ImVec2* out_min = nullptr,
                     ImVec2* out_max = nullptr);

} // namespace ui
