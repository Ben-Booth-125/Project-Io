-- Visual check for the building management view (Ben's 2026-07-15 mockup, moved onto
-- the Selection element by his 2026-07-16 steer): placeholder image, a glyph-tagged
-- production-method dropdown, trend-coloured profit + amber workforce graphs, and the
-- Auto / manual workforce-target control.
--
-- Reached by SELECTING a building — a tile carrying one reads as its building, so the
-- tile element stays the unbuilt-tile prospecting view. No panel/tab is involved (the
-- Building ledger is Construction-only now), which is why this needs no panel plumbing.
-- Run: ProjectIo --verify scripts/verify/building_management_shell.lua
verify.econ_step(6)                       -- populate run states + market prices
verify.show_panel("economy", false)
verify.goto_surface("home")               -- Kepler: player buildings visible

local player
for _, b in ipairs(verify.buildings()) do
    if b.player then player = b; break end
end
assert(player, "building_management_shell.lua: no player building on the home body")

-- Select the building: a new selection takes the fold-out column, so the Selection
-- element draws its management view unaided.
verify.select_building(player.x, player.y)
verify.capture("building_management_shell")
