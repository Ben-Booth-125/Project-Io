-- Verify the Corporation lens player-tile border (BL-001 / corp-lens-border R1).
-- Navigates to Kepler's surface, activates the Corporation overlay, and captures
-- the planetary surface showing player tiles with a palette::selection border.
verify.goto_surface("Kepler")
verify.set_overlay("corporation")
verify.set_zoom(4)
verify.capture("corp_lens_border_full")

-- Zoom in on a player building to make the border visible
local buildings = verify.buildings()
for _, b in ipairs(buildings) do
    if b.player then
        frame_tile(b.x, b.y, 16)
        verify.capture("corp_lens_border_zoom_player")
        break
    end
end
