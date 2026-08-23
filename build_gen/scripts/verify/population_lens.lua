-- Visual verification for the Population lens (overlay_mode::population).
-- Re-keyed by BL-069 to tint by WORKFORCE EFFICIENCY (workforce.hpp), not raw
-- habitability — so the lens shows the 0.6 efficiency cliff (the siting signal).
-- Confirms REQUIREMENTS.md § population-lens-render:
--   R1 the lens strip button/glyph (the figure) appears in every capture
--   R2 per-tile workforce-efficiency tint (zero-habitability land untinted,
--      full-labour land reads bright), composited over terrain
--   R3 the on-canvas workforce-efficiency gradient key (0.5x -> 1.0x bar)
--
-- Driver: direct state via the `verify` API (set_overlay). Captures land in
-- screenshots/<name>.png and diff against scripts/verify/golden/<name>.png.
-- Run with: ProjectIo --verify scripts/verify/population_lens.lua

verify.goto_surface("home")

-- Whole-body workforce-efficiency surface: the cliff at habitability 0.6 reads as
-- a flat-bright plateau over a sharper falloff, the key bottom-left.
verify.set_overlay("population")
verify.capture("population_lens_full")

-- Zoomed onto a varied region so the per-tile efficiency tint reads at scale.
frame_tile(42, 63, 8)
verify.capture("population_lens_zoom")
