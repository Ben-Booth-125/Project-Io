#pragma once

struct ImFont;

namespace ui {

/// Load the UI font atlas with sub-pixel oversampling.
///
/// Loads the first available system TTF at @p size_px with horizontal
/// oversampling and pixel-snapping disabled, so on-canvas labels stay crisp at
/// the fractional screen positions that moving bodies produce — the durable fix
/// for the label-shimmer bug (an oversampled atlas has the sub-pixel glyph
/// variants a bitmap font lacks). Falls back to the built-in ProggyClean font
/// (also oversampled) when no candidate file is present.
///
/// The candidate list tries a bundled assets/fonts/*.ttf first (copied next to
/// the executable at build time, loaded cwd-relative like scripts/) so text
/// renders identically across OSes and machines, then Linux and Windows system
/// fonts as fallbacks. Cross-platform per BACKLOG BL-057 / TECH_FOUNDATIONS.
///
/// Call once after the ImGui context and renderer backend are initialised and
/// before the first frame; the renderer backend uploads the atlas texture on
/// the next NewFrame.
///
/// @param size_px Font size in pixels.
/// @return        The loaded font (also set as ImGui's default font).
ImFont* load_ui_font(float size_px = 16.0f);

/// Re-load the UI font atlas at a new @p size_px (BL-063 discrete UI-scale
/// steps — 1.0 / 1.25 / 1.5 — re-render crisper than ImGui's FontGlobalScale).
/// Clears the existing atlas and calls load_ui_font(size_px); the caller is
/// responsible for destroying the renderer backend's fonts texture afterwards
/// so it is rebuilt from the new atlas on the next frame.
///
/// @param size_px Font size in pixels.
/// @return        The loaded font.
ImFont* reload_ui_font(float size_px);

} // namespace ui
