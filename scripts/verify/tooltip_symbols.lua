-- BL-1703 (terse tooltips): the hover card writes its quantities as symbols
-- (TOOLTIP.md § The symbols). A player building's card leads with its good's item
-- glyph (an extraction site) and its workforce behind the labour glyph, not the words
-- "Extracting:" / "Workforce:". Capture-only evidence; the verdict is the expects.
--
-- Run with: ProjectIo --verify scripts/verify/tooltip_symbols.lua

verify.econ_step(2)
verify.goto_surface("home")
verify.show_panel("construction", false)
verify.show_panel("build", false)

local pick
for _, b in ipairs(verify.buildings()) do
    if b.player and b.complete then
        if b.type == "Extraction Site" then pick = b; break end
        pick = pick or b
    end
end
verify.expect(pick ~= nil, "a completed player building stands to hover")
if not pick then return end
print(string.format("tooltip_symbols: hovering a %s at %d,%d", pick.type, pick.x, pick.y))

verify.center_tile(pick.x, pick.y)
verify.frames(3)
local p = verify.tile_screen(pick.x, pick.y)
verify.hover(p.x - 300.0, p.y + 200.0, 3)
verify.hover(p.x, p.y, 31)
local t = verify.pointer_target()
verify.expect(t.hover_card, "the hover card appears over the building")
verify.capture("tooltip_symbols_building_card")

-- Ben, 2026-11-20: on a tile standing several stacks, each STRUCTURE can be hovered
-- (TOOLTIP.md). Find a player tile with two stacks (different type or target), hover
-- each structure's footprint at the 2-stack slot layout (structure_stamps.cpp
-- k_slots), and capture both cards - they must name different buildings.
local by_tile = {}
for _, b in ipairs(verify.buildings()) do
    if b.player and b.complete then
        local k = b.x .. "," .. b.y
        by_tile[k] = by_tile[k] or {}
        by_tile[k][#by_tile[k] + 1] = b
    end
end
local stacked
for _, list in pairs(by_tile) do
    local kinds = {}
    for _, b in ipairs(list) do kinds[b.type .. "/" .. tostring(b.group)] = true end
    local n = 0
    for _ in pairs(kinds) do n = n + 1 end
    if n == 2 then stacked = list[1]; break end
end
if not stacked then
    print("tooltip_symbols: no player tile stands exactly two stacks; stacked hover not captured")
    return
end
print(string.format("tooltip_symbols: two stacks on %d,%d", stacked.x, stacked.y))
verify.set_zoom(6.0)
verify.frames(2)
verify.center_tile(stacked.x, stacked.y)
verify.frames(4)
-- tile_screen CENTRES the view on the tile it is asked about, so ask once; the hex
-- radius and the camera squash come from the ground HUD read.
local c  = verify.tile_screen(stacked.x, stacked.y)
local gs = verify.ground_stats()
local R  = gs.draw_r or 0
local sy = (gs.sy and gs.sy > 0) and gs.sy or 1
print(string.format("tooltip_symbols: hex R %.1f px, squash %.2f", R, sy))
local slots = { { 0.20, 0.26 }, { -0.42, -0.30 } }
for j, sl in ipairs(slots) do
    local hx, hy = c.x + sl[1] * R, c.y + sl[2] * R * sy
    verify.hover(hx - 300.0, hy + 200.0, 3)
    verify.hover(hx, hy, 31)
    local pt = verify.pointer_target()
    verify.expect(pt.hover_card, "stacked structure " .. j .. " shows a card")
    print("tooltip_symbols: structure " .. j .. " hovers kind " .. tostring(pt.hovered_kind))
    verify.capture("tooltip_symbols_stack_" .. j)
end
