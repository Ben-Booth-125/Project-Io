-- BL-1242 (landforms and rivers baked): a river from source to mouth.
--
-- A river bakes into the ground as a carved course along its river_edges chain — a
-- smooth curve through the chain by the roads' quadratic B-spline rule, WIDENING
-- DOWNSTREAM with accumulated flow, with bank shelving and a wet margin blended into
-- the ground (RENDERING.md § Mountains, rivers and terrain variety). The canvas stroke
-- and its chevrons are retired: the width gradient says which way the water flows.
--
-- What to look for: one continuous curve with no kink at a tile edge; thin at the
-- source, wide at the mouth; the course meets the sea cleanly at the coast; tributaries
-- join without a seam; no straight centre-to-centre stroke and no chevron anywhere.
--
-- The river is the home body's highest-flow river that reaches the sea, read from
-- ground_bake_check's feature readout: main stem source [243,92] -> mouth [234,102],
-- 29 tiles. If generation moves it, that harness prints the new pair.

-- The reference window (RENDERING.md § Level of detail), selection cleared so the
-- Selection band does not cover the ground under study.
verify.window(1720, 1080)
verify.clear_selection()
verify.goto_surface("home")
verify.set_overlay("none")

-- The far page: the river must still read as a line at the whole-grid view.
verify.center_tile(238, 97, 1.26)
verify.frames(2)
verify.capture("river_course_far")

-- Rung 1: the whole course, source to mouth, in one frame.
verify.center_tile(238, 97, 2.5)
verify.frames(2)
verify.capture("river_course_whole")

-- Rung 2: the same course closer — the width gradient along it.
verify.center_tile(238, 97, 5)
verify.frames(2)
verify.capture("river_course_mid")

-- The mouth at the sea, on the 22.5-degree tilted rung, then the 45-degree one.
verify.center_tile(234, 102, 10)
verify.frames(2)
verify.capture("river_course_mouth_tilt")
verify.center_tile(234, 102, 20)
verify.frames(2)
verify.capture("river_course_mouth_close")

-- The source: the course tapers to its spring.
verify.center_tile(243, 92, 10)
verify.frames(2)
verify.capture("river_course_source")
