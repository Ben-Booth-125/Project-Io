-- Verify building management controls (BL-049 / building-management R1, R7).
-- Runs economy ticks to show live building states, then opens the construction
-- panel showing a building's recipe selector and workforce slider.
verify.econ_step(6)
verify.show_panel("construction", true)

-- Navigate to a body with buildings so the building detail section populates
verify.goto_surface("home")
verify.capture("building_management_panel")

-- Show a close-up of a player building to confirm on-canvas marker convention
local buildings = verify.buildings()
for _, b in ipairs(buildings) do
    if b.player then
        frame_tile(b.x, b.y, 20)
        verify.capture("building_management_tile_player")
        break
    end
end
