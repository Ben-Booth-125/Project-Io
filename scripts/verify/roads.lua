-- Visual verification for the road network (BL-146 generation + BL-147 rendering &
-- placement) and the Inland Logistics Hub (BL-149) build affordance.
--
--   BL-147 R1: the generated road lattice renders always-on on the Planetary canvas as
--           edges between adjacent road tiles — trunk (road_level 2, the backbone) thicker
--           and brighter than local (road_level 1). Visible on the home body (Kepler), which
--           generates with a per-nation road network (BL-146).
--   BL-147 R2 / BL-149: the tile build front door lists a "Road" affordance and an
--           "Inland Logistics Hub" alongside the buildings, each with its cost + validity.
-- Run with: ProjectIo --verify scripts/verify/roads.lua

verify.econ_step(4)                     -- populate market state (build-cost context)
verify.show_panel("economy", false)
verify.goto_surface("home")             -- Kepler surface: the generated road lattice
verify.capture("roads_surface")

verify.set_zoom(0.5)                    -- wider view: the lattice spanning the body
verify.capture("roads_wide")

-- The tile build front door: a land tile now offers a Road (BL-147) and an Inland
-- Logistics Hub (BL-149) alongside the extraction / processing / port / launchpad types.
verify.set_zoom(1.0)
verify.select_tile(57, 34)
verify.capture("roads_build_select")    -- settle the selection (a new select closes the column)
verify.show_panel("build", true)
verify.capture("roads_build_front_door")
