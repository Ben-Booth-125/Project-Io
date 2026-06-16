-- Visual verification for the Population lens (overlay_mode::population).
-- Confirms REQUIREMENTS.md § population-lens-render:
--   R1 the lens strip button/glyph (the figure) appears in every capture
--   R2 per-tile habitability tint (dark substrate -> liveable green; barren
--      land barely tints, hospitable land reads bright), composited over terrain
--   R3 the on-canvas habitability gradient key (low -> high bar)
--
-- Driver: direct state via the `verify` API (set_overlay). Captures land in
-- screenshots/<name>.png and diff against scripts/verify/golden/<name>.png.
-- Run with: ProjectIo --verify scripts/verify/population_lens.lua

verify.goto_surface("home")

-- Whole-body habitability surface: the climate/terrain gradient reads as a
-- dark->green wash, the key bottom-left.
verify.set_overlay("population")
verify.capture("population_lens_full")

-- Zoomed onto a varied region so the per-tile habitability tint reads at scale.
frame_tile(42, 63, 8)
verify.capture("population_lens_zoom")
