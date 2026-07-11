-- Visual verification for the road network (BL-146 generation + BL-147/BL-172 rendering &
-- placement) and the Inland Logistics Hub (BL-149) build affordance.
--
--   BL-172 R1: the generated road lattice renders always-on on the Planetary canvas as
--           continuous, symmetric spans (each roaded tile draws its own half of the shared edge,
--           meeting at the edge midpoint — no "from vs to" asymmetry), over a three-tier ladder:
--           Track (road_level 1) thin/dim, Road (2) medium, Highway (3) thick/bright. Visible on
--           the home body (Kepler), which generates with a per-nation road network (BL-146/172).
--   BL-172 R1 / BL-149: the tile build front door lists THREE road affordances — Track, Road,
--           Highway (each with its own cost + validity) — and an "Inland Logistics Hub".
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
