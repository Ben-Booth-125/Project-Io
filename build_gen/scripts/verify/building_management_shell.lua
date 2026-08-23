-- Visual check for the redesigned building-management screen (Ben's 2026-07-15
-- mockup, UI shell): title + placeholder image, a glyph-tagged production-method
-- dropdown, profit + workforce graphs (placeholder series), and a workforce-target
-- button grid. Hosted as the Buildings tab's selected-building detail in the
-- construction panel (not selection-navigable on the canvas).
-- Run: ProjectIo --verify scripts/verify/building_management_shell.lua
verify.econ_step(6)                       -- populate run states + market prices
verify.show_panel("economy", false)
verify.goto_surface("home")               -- Kepler: player buildings visible

-- Select a player building FIRST — a new selection closes open panels, so the
-- panel must be opened after the selection is set, not before.
local player
for _, b in ipairs(verify.buildings()) do
    if b.player then player = b; break end
end
assert(player, "building_management_shell.lua: no player building on the home body")
verify.select_building(player.x, player.y)

-- construction_view acknowledges the current selection, so opening the panel here is
-- not stomped by the new-selection panel-close.
verify.show_panel("construction", true)
verify.construction_view(1)               -- the Buildings tab (management detail)
verify.capture("building_management_shell")
