-- BL-1524 (pointed trees) - RENDERING.md § The bake: a forest tree is a pointed
-- crown; scrub keeps its round head. Frames a forest edge on the home body at the
-- three closest rungs of the ladder at the reference window, so the form is judged
-- at the resolution each rung actually reads. The aim is ground_bake_check `--look`'s
-- forest_edge, with its neighbour rows framed by the view.
--
-- Run: ProjectIo --verify scripts/verify/tree_form.lua

local kMinZoom = 1.06875 -- body_surface_canvas.cpp kMinZoom

verify.window(1720, 1080)
verify.goto_surface("home")
verify.set_overlay("none")
verify.set_border_band(false)

for k = 2, 4 do
    verify.center_tile(26, 25, kMinZoom * (2 ^ k))
    verify.frames(3)
    verify.capture(string.format("tree_form_rung%d", k))
end

verify.set_border_band(true)
