#pragma once

#include <functional>
#include <imgui.h>

namespace ui {

/// Seconds of stable hover before the card first APPEARS — a "glance" that
/// still tracks the live cursor (BL-230). Seconds, not frames (BL-362): the
/// frame-count form was 0.5 s at 60 Hz but 0.2 s at 144 Hz, so the dwell the
/// player felt depended on their monitor. Under `--verify` the caller advances
/// this clock by a fixed 1/60 s per presentation frame, so verify.frames(30)
/// still lands exactly on the appear threshold and captures stay deterministic.
inline constexpr float kHoverAppearDelaySec = 0.5f;

/// Total seconds of stable hover (from the SAME start as kHoverAppearDelaySec)
/// before the card STICKS — freezes in place and starts the leave-to-dismiss
/// rule below. 2.5 s total, i.e. 2 s after it appears.
inline constexpr float kHoverStickDelaySec = 2.5f;

/// The fixed presentation-frame step used in place of wall-clock delta under
/// `--verify` (ui_state::fixed_frame_clock). Golden captures are driven by
/// frame COUNT, and verify frames run on a vsynced wall clock with real jitter,
/// so accumulating io.DeltaTime there would put the thresholds inside the
/// jitter band and flake the hover goldens.
inline constexpr float kVerifyFrameSeconds = 1.0f / 60.0f;

/// Largest per-frame delta the interactive dwell clock will accept. A stall
/// (world generation, alt-tab, a hitch) otherwise contributes its whole
/// wall-clock length in one frame and snaps a card straight to stuck.
inline constexpr float kHoverMaxFrameSeconds = 1.0f / 30.0f;

/// Half a verify frame, subtracted when testing a dwell threshold.
///
/// Accumulating `1/60` in float32 does not reach the threshold on the frame the
/// integer count would: after 150 additions the sum is 2.49999833, just under
/// 2.5, so the card stuck at frame 151 where the old integer counter fired at
/// 150 — and the appear threshold cleared by barely one ulp, so any change in
/// accumulation order or FP contraction could have moved it too. Comparing
/// against `threshold - kHoverThresholdEpsilon` restores the exact frame the
/// verify scripts and their goldens were authored against, with half a frame of
/// margin on either side rather than an ulp.
inline constexpr float kHoverThresholdEpsilon = kVerifyFrameSeconds * 0.5f;

/// How far outside the card's own rect the pointer may stray before the STUCK
/// card is dismissed (BL-228/230). The card is drawn *above* the cursor with a
/// gap, so the cursor that froze it starts outside the rect; this pad spans
/// that gap and leaves a forgiving margin for the trip up into the card.
/// Only applies once the card is stuck — see kHoverStickDelaySec.
inline constexpr float kHoverCardExitPadPx = 26.0f;

/// Render the floating hover card at `anchor` and report the rect it occupied.
///
/// The card is a semi-opaque ImGui window (no title bar, 4 px rounding, max
/// width 200 px) positioned just above `anchor`. `content` is called inside the
/// window to render the body.
///
/// **Two phases (BL-230).** Between `kHoverAppearDelaySec` and `kHoverStickDelaySec`
/// the card is a GLANCE: the caller re-passes the live cursor position each
/// frame, so it tracks the pointer like an ordinary tooltip and does not yet
/// block dismissal-by-distance. Past `kHoverStickDelaySec` the caller freezes
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
