-- BL-1256 (ground look C-F) — RENDERING.md § Art direction and palette: the
-- it3 C-F reference (docs/ui/design/renders/map/it3, bottom-right panel) is the
-- target for DETAIL and value, it1's top-down panel for COLOUR. A top-down bake
-- cannot have the reference's low camera (silhouettes, foreshortening, haze);
-- what it owes is the rest:
--
--   palette and value  it1's khaki, olive and green, white peaks; lit slopes near a warm off-white,
--                      shadows near black; no broad dark mottle of its own
--   relief everywhere  plains roll in folds a third to a half of a hex apart,
--                      ranges above them, continuous across tiles
--   cast shadows       the low NW sun throws shadow behind ridges and hills
--   water              blue-green, glinting; white where a river falls; rocky banks
--   forests            as before (their form is unchanged)
--
-- Subjects are ground_bake_check `--look`'s aims on the home body (that reading
-- prints them, with the luminance percentiles the palette is tuned against): a
-- plain, rolling hills, the best-linked mountain run, the river reach with the
-- steepest fall, a forest edge, the busiest town and a coast. Rungs 0 to 4 of
-- the stepped ladder: rungs 0-2 are judged for COLOUR against it1's top-down
-- panel (docs/ui/design/renders/map/it1), rungs 3-4 for detail against it3 C-F.
--
-- Run: ProjectIo --verify scripts/verify/ground_look.lua (Release).

local kMinZoom = 1.2531328  -- body_surface_canvas.cpp kMinZoom (~1.253)

verify.window(1720, 1080)
verify.clear_selection()
verify.goto_surface("home")
verify.set_overlay("none")
verify.set_border_band(false)

local aims = {
    { "plain",       250, 86 },
    { "hills",       127, 43 },
    { "mountain",    232, 85 },
    { "river_fall",  116, 61 },
    { "forest_edge",  26, 25 },
    { "town",         43, 24 },
    { "coast",         9, 25 },
}

-- The player's own ground, inside its vision: everywhere else the vision fog
-- (BL-151) dims the bake by half, so this is where the ground reads as baked.
for _, b in ipairs(verify.buildings()) do
    if b.player then
        aims[#aims + 1] = { "player_works", b.x, b.y } -- (the sandbox has no table library)
        break
    end
end

for _, a in ipairs(aims) do
    for k = 0, 4 do
        verify.center_tile(a[2], a[3], kMinZoom * (2 ^ k))
        verify.frames(3)
        verify.capture(string.format("ground_look_%s_rung%d", a[1], k))
    end
end

verify.set_border_band(true)
