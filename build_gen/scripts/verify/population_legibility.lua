-- Visual verification for Population legibility (BL-069).
-- Confirms requirements.json § population-legibility R1 (item-spanning):
--   the Population lens, re-keyed to WORKFORCE EFFICIENCY, shows the 0.6
--   habitability cliff (a flat-bright plateau at/above 0.6 over a sharp falloff);
--   the Selection panel on a population centre shows scale / population / local
--   habitability rows and the workforce cap as an absolute number.
--
-- Driver: verify.set_overlay + verify.select_tile on the active body. Population
-- centre coordinates come from verify.population_centres() (see discovery), so the
-- selected tile is a real centre. Kepler (home) carries the prototype population.
-- Run with: ProjectIo --verify scripts/verify/population_legibility.lua

verify.econ_step(4)            -- compute body_habitability so the cap reads live
verify.show_panel("economy", false)   -- clear the auto-opened panel off the capture
verify.goto_surface("home")

-- Re-keyed Population lens: tiles tint by workforce efficiency, so the cliff at
-- habitability 0.6 reads as a flat plateau over a sharp falloff (not a smooth ramp).
verify.set_overlay("population")
verify.capture("population_legibility_lens_full")

-- Zoom onto a varied region so the high/low-efficiency boundary reads at scale.
frame_tile(70, 47, 8)
verify.capture("population_legibility_lens_zoom")

-- Selection panel on a population centre (the scale-5 metropolis): scale +
-- population + local habitability rows, and the workforce cap as an absolute number.
verify.select_tile(66, 6)
frame_tile(66, 6, 10)
verify.capture("population_legibility_centre_panel")
