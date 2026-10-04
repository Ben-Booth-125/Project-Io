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

-- Toggled set + pie split (Ben, 2026-10-04): several extractable resources at once.
-- A tile carrying two or more toggled goods splits into equal wedges from the centre.
verify.set_lens_resource("iron_ore")
verify.toggle_lens_resource("coal")
verify.toggle_lens_resource("stone")
verify.toggle_lens_resource("timber")
local coal_at = verify.find_deposit_tile("coal")
verify.expect(coal_at.ok, "the home body carries coal to frame the split on")
if coal_at.ok then frame_tile(coal_at.x, coal_at.y + 3, 3) end
verify.capture("resource_lens_multi_split")

-- The legend's search box filters the checklist.
verify.set_lens_resource_filter("co")
verify.capture("resource_lens_search")
verify.set_lens_resource_filter("")
