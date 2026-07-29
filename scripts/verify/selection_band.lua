-- BL-213: the Selection band is now a fixed rect at the bottom of the screen,
-- between the shell column and the right chrome column, and no longer closes
-- an open fold-out ledger. Confirms both, plus the tile layout's fit in the
-- new wide-short band shape (previously designed for a narrow, tall card).
verify.goto_surface("home")

-- Open a nav-rail ledger AND select a building at the same time.
verify.show_panel("economy", true)
local blds = verify.buildings()
if #blds > 0 then
    frame_tile(blds[1].x, blds[1].y, 14)
    verify.select_building(blds[1].x, blds[1].y)
end
verify.capture("selection_band_with_ledger_open")

-- A bare tile selection (the vertical hex + accordion layout, BL-123).
verify.show_panel("economy", false)
verify.select_tile(70, 40)
verify.capture("selection_band_tile")
