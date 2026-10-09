-- BL-1243 (terrain variant families) — RENDERING.md § Mountains, rivers and
-- terrain variety, the "More tile sets — variant families" bullet.
--
-- Every terrain family carries several procedural variants, each tile picking
-- one by a hash of its grid coordinates, cross-faded across the tile boundary.
-- What to look for: a wide plain and a deep forest no longer read as wallpaper
-- (tonal patches, combed and cracked ground, glades and thickets vary from
-- tile to tile), neighbouring tiles of one terrain do not repeat, and NO
-- variant edge is drawn anywhere — no hex grid on the ground.
--
-- Aims come from ground_bake_check's `--aim` readout on the home body (the
-- tile at the heart of the widest run of one cover); if a generation change
-- moves them, that readout prints the new ones.
--
-- BL-1251 (tiles hold their own ground — RENDERING.md § Tiles hold their own
-- ground): each tile's ground is its own across its body, blending only in a
-- narrow band at the shared edge, and edges between different families carry
-- border sets. The shore, field-edge and forest-fringe aims are the edges
-- ground_bake_check `--border` (P27) prints. What to look for: tiles readable
-- as tiles, no blur over a tile's body, transitions that read as terrain (a
-- fringe of trees, a hedge or furrowed headland, a pale shelf and shallows),
-- and nothing that reads as a drawn hex outline. The mountain run is
-- landform_relief.lua's best-linked range. The two close rungs (zoom 10 =
-- the 48 px tier at 22.5 degrees, zoom 20 = the 96 px tier at 45 degrees)
-- carry the read. Slow under a Debug build (every chunk bakes synchronously
-- at 2x supersample): allow several minutes per capture.

verify.window(1720, 1080)
verify.clear_selection()
verify.goto_surface("home")
verify.set_overlay("none")
verify.set_border_band(false)

local aims = {
    { "plain",    250, 86 },
    { "bare",     125, 27 },
    { "forest",    38, 29 },
    { "mountain", 232, 85 },
    { "shore",     10, 25 },
    { "field_edge", 183, 25 },
    { "fringe",    18, 25 },
}
local rungs = { { 10, "close" }, { 20, "closest" } }

for _, a in ipairs(aims) do
    for _, r in ipairs(rungs) do
        verify.center_tile(a[2], a[3], r[1])
        verify.frames(3)
        verify.capture("variants_" .. a[1] .. "_" .. r[2])
    end
end

verify.set_border_band(true)
