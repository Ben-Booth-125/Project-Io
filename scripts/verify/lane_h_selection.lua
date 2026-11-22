-- Playtest 2 lane H (Ben, 2026-11-22): the tile card's figure row with its
-- workforce cell (BL-1725), the bare "Tile" title (BL-1726), and the Selection
-- band flush against the minimap and the screen's foot with square corners (BL-1723).
verify.goto_surface("home")
local blds = verify.buildings()
local aim = nil
for _, b in ipairs(blds) do
    if b.player then aim = b break end
end
assert(aim, "no player building to frame")
frame_tile(aim.x, aim.y, 14)
verify.select_tile(aim.x, aim.y)
verify.frames(3)
local c = verify.tile_card()
print(string.format("[lane_h] tile (%d, %d): tile_w %.0f, capacity %d/%d", aim.x, aim.y,
    c.tile_w or -1, c.capacity_used or -1, c.capacity_cap or -1))
verify.capture("lane_h_tile_with_buildings")
