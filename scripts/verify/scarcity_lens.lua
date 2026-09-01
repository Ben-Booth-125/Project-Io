-- Visual verification for the Scarcity lens (overlay_mode::scarcity).
-- Confirms requirements.json § scarcity-market-heatmap:
--   R1 scarcity reads market supply shortfall (max(0, demand - supply)) of the
--      selected good, not tile deposits
--   R2 chunky per-market blocks: every tile in a market's catchment shares one
--      tint (via market_for_tile), not crisp per-tile cells
--   R3 the on-canvas key (met -> scarce bar + selected resource swatch) + the
--      strip button/glyph; the selector re-skins to a different good
--
-- Driver: run the economy a fixed number of ticks so market supply/demand (and
-- thus shortfall) populate, close the panel econ_step opens, then capture.
-- Run with: ProjectIo --verify scripts/verify/scarcity_lens.lua

verify.goto_surface("home")

-- Populate market supply/demand so shortfall is meaningful, then clear the economy
-- panel that econ_step opens so it does not obscure the capture.
verify.econ_step(12)

-- Scarcity of iron ore: each market's catchment tints by its iron shortfall; the
-- key shows the iron swatch.
verify.set_overlay("scarcity")
verify.set_lens_resource("iron_ore")
verify.capture("scarcity_lens_full_iron")

-- A second good proves the selector re-skins the scarcity surface.
verify.set_lens_resource("steel")
verify.capture("scarcity_lens_full_steel")

-- Zoomed so the per-market block reads as one uniform region across many tiles.
verify.set_lens_resource("iron_ore")
frame_tile(42, 63, 8)
verify.capture("scarcity_lens_zoom_iron")
