-- BL-1254 (ground crisper) — RENDERING.md § Tiles hold their own ground, the
-- "Crisper texture inside a tile" bullet.
--
-- The ground inside a tile should hold the same crispness as what stands on
-- it: sharper relief shading, a stronger fine grain, and a light contact ink on
-- relief creases and on the family patterns (furrow lines, scree stones, ripple
-- crests, tussock bases). What to look for, rung by rung (2, 3, 4 of the
-- stepped ladder): the ground read side by side with structures and trees in
-- frame — no soft blotches over a tile's body, pattern marks that read as
-- marks, crests that read as folds — while it stays painterly, not noisy, and
-- the honeycomb (each tile's own tone) is unchanged.
--
-- Aims are ground_bake_check `--crisp`'s subjects on the home body (that
-- readout prints them, and the crispness numbers behind this look): the wide
-- plain, a furrowed field, a deep forest, the best-linked mountain run, a
-- shore, and the town with the most installations around it.
--
-- Run: ProjectIo --verify scripts/verify/ground_crisp.lua (Release: the home
-- master pre-bakes on the pool before the first capture).

local kMinZoom = 1.2531328  -- body_surface_canvas.cpp kMinZoom (~1.253)

verify.window(1720, 1080)
verify.clear_selection()
verify.goto_surface("home")
verify.set_overlay("none")
verify.set_border_band(false)

local aims = {
    { "plain",    250, 86 },
    { "field",     43, 28 },
    { "forest",    38, 29 },
    { "mountain", 232, 85 },
    { "shore",     10, 25 },
    { "town",      43, 24 },
}

for _, a in ipairs(aims) do
    for k = 2, 4 do
        verify.center_tile(a[2], a[3], kMinZoom * (2 ^ k))
        verify.frames(3)
        verify.capture(string.format("ground_crisp_%s_rung%d", a[1], k))
    end
end

verify.set_border_band(true)
