-- BL-1721 (coast texture follows land) - RENDERING.md § Level of detail, the close-zoom
-- detail layer: the fine-grain and coarse detail textures end where the PAINTED coast
-- ends, not at the hex side.
--
-- Ben, playtest 2026-11-22: "The fine grain textures are drawn to fit the hexagon, so
-- when land meets coast we can sometimes see fine grain textures drawn over the ocean,
-- or land with no texture where the hexagon spills onto the ocean."
--
-- Subjects: ground_look's coast aim on the home body, and the coast nearest the
-- player's capital (inside the player's vision, so the ground reads as baked). Each at
-- the two closest rungs, where the detail layer draws. Judged by eye: textured land
-- right up to the painted shore, and smooth water on the sea side of it.
--
-- Run: ProjectIo --verify scripts/verify/coast_detail.lua

local kMinZoom = 1.06875 -- body_surface_canvas.cpp kMinZoom

verify.window(1720, 1080)
verify.clear_selection()
verify.goto_surface("home")
verify.set_overlay("none")
verify.set_border_band(false)

local aims = { { "look", 9, 25 } }
-- A road crossing with a water end (verify.find_water_crossing): a shore the
-- generation put a road on, so it moves with generation rather than rotting.
local wc = verify.find_water_crossing()
if wc and wc.shore and wc.shore.ok then
    aims[#aims + 1] = { "shore", wc.shore.x, wc.shore.y }
end

for _, a in ipairs(aims) do
    for k = 3, 4 do
        verify.center_tile(a[2], a[3], kMinZoom * (2 ^ k))
        verify.mouse(-1, -1)
        verify.frames(4)
        verify.capture(string.format("coast_detail_%s_rung%d", a[1], k))
    end
end
