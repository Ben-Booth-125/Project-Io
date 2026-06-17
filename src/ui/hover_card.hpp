#pragma once

#include <functional>
#include <imgui.h>

namespace ui {

/// Frames of stable hover required before the card appears (BL-060).
inline constexpr int kHoverDelay = 20;

/// Render a transient floating hover card near `cursor` when `hover_ticks`
/// has reached `kHoverDelay`. The card is a semi-opaque ImGui child window
/// (no title bar, 4 px rounding, max width 200 px) positioned just above the
/// cursor. `content` is called inside the window to render the body. When
/// `hover_ticks < kHoverDelay` the call is a no-op.
///
/// @param cursor      Current mouse position in screen pixels.
/// @param hover_ticks Frames the cursor has rested over the same entity.
/// @param content     Caller-supplied content renderer (ImGui calls only).
void draw_hover_card(ImVec2 cursor, int hover_ticks,
                     const std::function<void()>& content);

} // namespace ui
