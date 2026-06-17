-- Visual verification for the Country lens (overlay_mode::country).
-- Confirms requirements.json § faction-to-country-rename:
--   R1 the lens is named Country (strip glyph = shield; tooltip "Countries")
--   R3 the renamed lens renders identical territory tint + owner borders to the
--      prior Faction lens (nation_colour tile fill + dark inter-owner borders)
--
-- The lens is the renamed BL-052 Faction lens; "faction" remains a legacy alias
-- in overlay_from_name, but this script drives it by its new name.
-- Run with: ProjectIo --verify scripts/verify/country_lens.lua

verify.goto_surface("home")

-- Whole-body political map: nation territory tinted by nation_colour, dark borders
-- on every edge between different owners (and the claimed/unclaimed boundary).
verify.set_overlay("country")
verify.capture("country_lens_full")

-- Zoomed onto a multi-nation border region so the per-edge borders read at scale.
frame_tile(42, 63, 8)
verify.capture("country_lens_zoom")
