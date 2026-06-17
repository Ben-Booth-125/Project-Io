-- Visual verification for the Resource lens (overlay_mode::resource).
-- Confirms requirements.json § resource-density-flat:
--   R1 flat, uniform fill over the contiguous deposit of the selected resource
--      (deposit *shape*, no magnitude gradient) + the strip button/glyph
--   R2 the lens is always single-resource (the selector picks the good; no
--      highest-value mode / Single checkbox)
--   R3 the selector re-skins the surface to a different resource + the on-canvas
--      key (selected resource swatch/name)
--
-- Driver: direct state via the `verify` API (set_overlay / set_lens_resource).
-- Captures land in screenshots/<name>.png and diff against the blessed goldens.
-- Run with: ProjectIo --verify scripts/verify/resource_lens.lua

verify.goto_surface("home")

-- Flat fill over iron-ore deposits: every tile carrying iron reads the iron hue at
-- a fixed opacity; the key shows the iron swatch + name.
verify.set_overlay("resource")
verify.set_lens_resource("iron_ore")
verify.capture("resource_lens_full_iron")

-- A second good proves the selector re-skins the surface to a different resource.
verify.set_lens_resource("coal")
verify.capture("resource_lens_full_coal")

-- Zoomed onto a varied land/deposit region so the flat deposit shape + the
-- on-canvas key read clearly at scale.
verify.set_lens_resource("iron_ore")
frame_tile(42, 63, 8)
verify.capture("resource_lens_zoom_iron")
