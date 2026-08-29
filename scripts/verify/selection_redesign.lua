-- Visual verification for the Selection element redesign (BL-093).
-- Confirms the action-first surface + the lens strip relocated onto the minimap:
--   * a selected TILE leads with the Build front door (action), with the BL-071
--     Thrives/Valid affordance read as its facts column — no stat block, no lens
--     supplement;
--   * a selected player BUILDING leads with Manage, with the BL-074 profitability
--     as its facts;
--   * the minimap carries the seven-glyph lens mode bar along its bottom, and the
--     bottom-left no longer carries a separate lens strip;
--   * the selection element occupies the full bottom-left corner.
-- Run with: ProjectIo --verify scripts/verify/selection_redesign.lua

verify.econ_step(4)                    -- populate live building run states / profit
verify.goto_surface("home")            -- Kepler: fully surveyed, buildings visible

-- A tile: the Build front door is the hero (left), the Thrives/Valid affordance
-- read is the facts (right). Header shows [icon] Name . type + [>] [x].
verify.select_tile(66, 6)
verify.capture("selection_tile_build")

-- A player building: Manage is the hero, the profitability readout the facts.
local player
for _, b in ipairs(verify.buildings()) do
    if b.player then player = b; break end
end
assert(player, "selection_redesign.lua: no player building on the home body")
verify.select_building(player.x, player.y)
verify.capture("selection_building_manage")

-- A resource lens active: the minimap mode bar highlights the active glyph and
-- shows the resource-selector popup button; the bottom-left has no lens strip.
verify.set_overlay("resource")
verify.select_tile(66, 6)
verify.capture("selection_lens_bar")
