-- Visual verification for the Opportunity lens (overlay_mode::opportunity).
-- Confirms requirements.json § population-opportunity-lens:
--   R1 per-tile best-building net-margin surface (output value - input value -
--      upkeep), evaluated ignoring what is built and logistics; diverging
--      red(loss) -> green(profit) normalised per body, + strip glyph + key
--
-- Driver: run the economy a few ticks so market prices have resolved (the margin
-- reads prices), close the panel econ_step opens, then capture.
-- Run with: ProjectIo --verify scripts/verify/opportunity_lens.lua

verify.goto_surface("home")

verify.econ_step(12)
verify.show_panel("economy", false)

-- Whole-body opportunity surface: each tile tinted by the best valid building's
-- net margin; the key shows the loss -> profit gradient.
verify.set_overlay("opportunity")
verify.capture("opportunity_lens_full")

-- Zoomed onto a varied land region so the per-tile margin gradient reads at scale.
frame_tile(42, 63, 8)
verify.capture("opportunity_lens_zoom")
