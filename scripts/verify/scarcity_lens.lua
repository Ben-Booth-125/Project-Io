-- Visual verification for the Scarcity lens (overlay_mode::scarcity).
-- Confirms REQUIREMENTS.md § scarcity-lens-render:
--   R1 the lens strip button/glyph (hollow inverted triangle) in every capture
--   R2 per-tile translucent scarcity heatmap for the selected resource
--      (hot where absent/sparse, fading to no tint where abundant)
--   R3 the resource selector re-skins the surface to a different resource
--   R4 the on-canvas key (abundant -> scarce bar + selected resource swatch)
--
-- Driver: direct state via the `verify` API (set_overlay / set_lens_resource).
-- Captures land in screenshots/<name>.png and diff against the goldens.
-- Run with: ProjectIo --verify scripts/verify/scarcity_lens.lua

verify.goto_surface("home")

-- Scarcity of iron ore: tiles poor in iron read hot, rich tiles stay near
-- terrain; the key shows the iron swatch.
verify.set_overlay("scarcity")
verify.set_lens_resource("iron_ore")
verify.capture("scarcity_lens_full_iron")

-- A second good proves the selector re-skins the scarcity surface.
verify.set_lens_resource("coal")
verify.capture("scarcity_lens_full_coal")

-- Zoomed so the translucent per-tile heatmap reads against terrain at scale.
verify.set_lens_resource("iron_ore")
frame_tile(42, 63, 8)
verify.capture("scarcity_lens_zoom_iron")
