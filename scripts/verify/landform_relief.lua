-- BL-231 (landform render), re-pointed by BL-1242 (landforms and rivers baked).
--
-- The landform axis renders on two channels (PLANETARY.md § Terrain channels): a subtle
-- relief tint for the common ground (plains, valley, highland) and, for the four DRAMATIC
-- landforms (mountain, canyon, crater, rift — each <=1.5% of land, each x1.3 movement or
-- worse), a relief FORM baked into the ground: mountain as massif and ridge, canyon as a cut
-- between paired rims, crater as a raised-rim bowl, rift as a dark fissure. No glyph is drawn
-- on the canvas. A contiguous run bakes as ONE form — a range, one cut, one fissure — each
-- tile baking its half toward a same-landform neighbour (RENDERING.md § Mountains, rivers
-- and terrain variety).
--
-- What to look for: the forms read at EVERY rung, the far page included (these are the
-- expensive tiles, so an invisible one is a surprise), the run reads as one feature rather
-- than a repeated stamp, and no glyph or span stroke appears anywhere.

-- The far page and the play rungs on the home body. The dramatic set is sparse here, so
-- the forms should read as scattered accents, not a rash. The reference window
-- (RENDERING.md § Level of detail), selection cleared so the Selection band does not
-- cover the ground under study.
verify.window(1720, 1080)
verify.clear_selection()
verify.goto_surface("home")
verify.set_overlay("none")
verify.set_zoom(1.26)
verify.frames(2)
verify.capture("landform_home_far")

verify.set_zoom(2.5)
verify.frames(2)
verify.capture("landform_home_wide")

-- The relief tint alone (plains vs highland), at a mid rung.
verify.set_zoom(5)
verify.frames(2)
verify.capture("landform_home_relief")

-- A DRY body: the sunken half of the relief scale (valley) actually appears here.
-- Not the home body, so it opens unsurveyed; reveal it or the captures show the lock fill.
verify.set_survey("inner", 99999)
verify.goto_surface("inner")
verify.set_zoom(1.26)
verify.frames(2)
verify.capture("landform_inner_far")

verify.set_zoom(5)
verify.frames(2)
verify.capture("landform_inner_zoom")

-- Under a lens. Continent is the most saturated lens (0.80).
verify.set_overlay("continent")
verify.capture("landform_inner_lens_continent")

-- The baked forms, framed tight. Coordinates come from ground_bake_check's feature
-- readout (the best-linked tile of each landform on the home body); if a generation change
-- moves them, that harness prints the new ones instead of these captures quietly becoming
-- pictures of empty ground.
verify.goto_surface("home")
verify.set_overlay("none")

-- A range: the mountain with the most same-landform neighbours (5) — the run should bake
-- as one massif with a ridge, not a cluster of cones. Then a lone peak for contrast.
verify.center_tile(232, 85, 5)
verify.frames(2)
verify.capture("landform_range_mountain_run")
verify.center_tile(232, 85, 10)
verify.frames(2)
verify.capture("landform_range_mountain_run_tilt")

verify.center_tile(7, 22, 5)
verify.frames(2)
verify.capture("landform_range_mountain_lone")

-- A rift — one continuous dark fissure along its run.
verify.center_tile(235, 57, 5)
verify.frames(2)
verify.capture("landform_fissure_rift_run")
verify.center_tile(235, 57, 20)
verify.frames(2)
verify.capture("landform_fissure_rift_close")

-- A canyon — a cut between paired rims.
verify.center_tile(230, 55, 10)
verify.frames(2)
verify.capture("landform_cut_canyon_run")

-- A crater — a raised-rim bowl; craters never span.
verify.center_tile(162, 18, 10)
verify.frames(2)
verify.capture("landform_bowl_crater")

-- THE TOOLTIP: the form carries no label, so the hover card names the landform and its
-- consequence. The card is gated on the glance-then-stick delay, so the cursor sits still.
verify.hover_tile(232, 85)
verify.frames(40)
verify.capture("landform_tooltip_mountain")

verify.hover_tile(235, 57)
verify.frames(40)
verify.capture("landform_tooltip_rift")
