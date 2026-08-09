-- Visual verification for the Supply lens (overlay_mode::supply).
-- Confirms requirements.json § supply-layer R9:
--   R9 Supply lens shows inter-body route lines (Solar), per-body throughput
--      badges (Circumplanetary), and tile segment glyphs (Planetary).
--
-- Driver: seed one test convoy in flight (Kepler → Selene, progress=0.5) via
-- verify.seed_convoy, bypassing the auto-dispatch launchpad gate so the lens
-- has something to render. Captures each rung of the zoom ladder.
-- Run with: ProjectIo --verify scripts/verify/supply_lens.lua

-- Kepler → Pallas: two well-separated solar bodies; a minimal market is created
-- on Pallas by seed_convoy so the Solar route line spans the canvas visibly.
verify.seed_convoy("home", "asteroid", "iron_ore", 50.0)

-- Navigate to Solar (two rungs above Kepler surface).
verify.goto_surface("home")
verify.command("ascend")   -- Planetary → Circumplanetary
verify.command("ascend")   -- Circumplanetary → Solar

-- Solar: route line between Kepler and Selene.
verify.set_overlay("supply")
verify.capture("supply_lens_solar_route")

-- Circumplanetary: convoy-count badge alongside each involved body label.
verify.command("descend")
verify.capture("supply_lens_circum_badges")

-- Planetary (Kepler surface): convoy glyph on every tile — zoom in on the
-- player building area so the per-tile convoy marker is clearly legible.
verify.command("descend")
frame_tile(22, 82, 12)
verify.capture("supply_lens_planetary_glyphs")
