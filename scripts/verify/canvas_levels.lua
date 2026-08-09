-- Demo capture: one frame at each rung of the zoom ladder.
-- Planetary (default) -> ascend -> Circumplanetary -> ascend -> Solar.
verify.goto_surface("home")
verify.capture("level_1_planetary")

verify.command("ascend")
verify.capture("level_2_circumplanetary")

verify.command("ascend")
verify.capture("level_3_solar")
