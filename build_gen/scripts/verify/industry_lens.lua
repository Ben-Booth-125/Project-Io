-- Visual verification for the Industry-density lens (BL-084).
-- Confirms requirements.json § industry-lens R1 (item-spanning):
--   overlay_mode::industry tints the Planetary surface by the nation-substrate
--   throughput field (occupation weighted by terrain richness), with a distinct
--   strip glyph. Reads "where the existing industry is", distinct from the
--   Population markers/lens. Pure rendering — no economy change.
--
-- Run with: ProjectIo --verify scripts/verify/industry_lens.lua

verify.econ_step(4)                     -- settle the substrate injection
verify.show_panel("economy", false)
verify.goto_surface("home")

verify.set_overlay("industry")
verify.capture("industry_lens_full")

frame_tile(66, 6, 9)
verify.capture("industry_lens_zoom")
