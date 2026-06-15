-- Visual verification for the Resource lens (overlay_mode::resource).
-- Confirms REQUIREMENTS.md § resource-lens-render R1-R5:
--   R2/R3 highest-value deposit-density tint (richest deposit's hue, magnitude
--         opacity) + the resource strip button/glyph (in every capture)
--   R4    single-resource heatmap driven by the lens-local selector
--   R5    the on-canvas gradient key (sparse->dense bar + swatch/name)
--
-- Driver: direct state manipulation via the `verify` API (set_overlay /
-- set_resource_mode / set_lens_resource). Captures land in screenshots/<name>.png
-- and diff against scripts/verify/golden/<name>.png. Uses the shared lib.lua helpers.
-- Run with: ProjectIo --verify scripts/verify/resource_lens.lua

verify.goto_surface("home")

-- Highest-value mode (default): each tile tints to its single richest deposit's
-- identity hue at a magnitude-scaled opacity; the key lists the body's resources.
verify.set_overlay("resource")
verify.set_resource_mode(false)
verify.capture("resource_lens_full_highest")

-- Single-resource heatmap for iron ore: density of one good across the body, with
-- the key showing the selected resource's name + swatch.
verify.set_resource_mode(true)
verify.set_lens_resource("iron_ore")
verify.capture("resource_lens_full_iron")

-- A second good proves the selector re-skins the surface to a different resource.
verify.set_lens_resource("coal")
verify.capture("resource_lens_full_coal")

-- Zoomed onto a varied land/deposit region (a known rival extraction tile) in
-- highest-value mode so the per-tile tint + the on-canvas key read clearly at scale.
verify.set_resource_mode(false)
frame_tile(42, 63, 8)
verify.capture("resource_lens_zoom_highest")
