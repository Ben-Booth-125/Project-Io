-- Visual verification for the Production lens (overlay_mode::production).
-- Confirms requirements.json § production-output-lens:
--   R1 per producing tile, intensity = sum(output qty x resolved price) read from
--      the economy report + market prices; built producing tiles tint
--   R2 log scale relative to the body's producing-tile mean (above mean warm,
--      below cool); idle/exhausted/unbuilt tiles read cold + strip glyph + key
--
-- Driver: run the economy so buildings produce and the report populates, close the
-- panel econ_step opens, then capture.
-- Run with: ProjectIo --verify scripts/verify/production_lens.lua

verify.goto_surface("home")

verify.econ_step(12)
verify.show_panel("economy", false)

-- Whole-body production surface: producing tiles tint warm/cool by output value;
-- the key shows the low -> high diverging gradient.
verify.set_overlay("production")
verify.capture("production_lens_full")

-- Zoomed onto a known extraction/processing cluster so producing-tile tints read.
frame_tile(42, 63, 8)
verify.capture("production_lens_zoom")
