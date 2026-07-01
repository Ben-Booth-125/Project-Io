-- BL-071 build legibility: the always-on selected-tile affordance readout
-- ("Suited for" thrives/valid/invalid) and the reason-coded build front door.
-- Acceptance check: a selected land tile shows the affordance grouping; a
-- selected ocean tile shows a specific rejection reason, not a bare string.
verify.econ_step(2)                    -- give the world some deposits/markets to read
verify.show_panel("economy", false)    -- econ_step force-opens it; close for a clean read
verify.goto_surface("Kepler")

-- A land tile with deposits: affordance readout should list Extraction (thrives/
-- valid) plus Processing/Port validity, with territory owner.
verify.select_tile(70, 38)
verify.center_tile(70, 38, 8)
verify.capture("build_legibility_affordances")

-- An ocean tile: the build front door should read the placement reason code.
verify.select_tile(1, 1)
verify.center_tile(1, 1, 8)
verify.capture("build_legibility_reason")
