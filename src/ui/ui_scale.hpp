#pragma once

#include <imgui.h>

namespace ui {

/// **The UI scale — one factor (BL-1495; LAYOUT.md § UI scale).**
///
/// Every hard-coded pixel in the chrome was authored against a 16 px font. The factor
/// is the active font size over that base, so 12 px text draws at 0.75 and every
/// padding, spacing, item size and px constant shrinks with it. ImGui's own style is
/// rebuilt from a fresh base and ScaleAllSizes'd on each apply (`apply_ui_scale`); the
/// px the UI hard-codes go through `px()` or a `px_const`.
///
/// Canvas ground (the map bake, tile sizes, zoom) does NOT use this: it is chrome only.
inline constexpr float k_ui_base_font_px = 16.0f;

namespace detail {
inline float g_ui_scale = 1.0f;
} // namespace detail

/// The active factor: font px / 16.
inline float ui_scale() { return detail::g_ui_scale; }

/// A chrome pixel length at the active scale.
inline float  px(float v) { return v * detail::g_ui_scale; }
inline ImVec2 px(float x, float y) { return {x * detail::g_ui_scale, y * detail::g_ui_scale}; }

/// A named chrome constant: declared at its authored 16 px value, read at the active
/// scale wherever it converts to float. `inline constexpr px_const k{48.0f};`
struct px_const
{
    float base;
    constexpr explicit px_const(float b) : base(b) {}
    operator float() const { return base * detail::g_ui_scale; }
};

/// Set the factor from @p font_px, reload the font at that size, and rebuild ImGui's
/// style from a fresh dark base scaled by the factor. The caller destroys the
/// renderer backend's font texture afterwards so the new atlas uploads next frame.
void apply_ui_scale(float font_px);

} // namespace ui
