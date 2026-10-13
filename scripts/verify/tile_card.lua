-- The tile element after the TILE CARD SPLIT (BL-1300). SELECTION.md § The tile
-- element's layout, § Multi-building tiles, § The building element's layout
-- (Ben, 2026-10-13). Replaces tile_production.lua and selection_accordion.lua:
-- the Production section and the six-section nav they drove are retired.
--
-- What each capture is for:
--   tile_card_unbuilt      an unbuilt land tile: no header row, four columns
--                          (preview | province | tile | actions), "Nothing built
--                          here", the province card's capacity bar, resource
--                          table and population.
--   tile_card_capacity_tip the capacity bar hovered: where the ceiling is granted
--                          (area, habitability, infrastructure, population).
--   tile_card_built        a player tile: one button per stack, three across,
--                          glyph + count, name, net profit per quarter.
--   tile_card_stacked      a tile carrying two or more buildings.
--   tile_card_expanded     a building button PRESSED (a real click at the
--                          button's published centre): the stack expands in the
--                          tile column, the action grid retargets to the building.
--   tile_card_back         the back control pressed: the grid returns.
--   tile_card_rival        a rival's tile — "private" on its buttons, asserted
--                          off verify.tile_card(): no recipe group, no good.
--   tile_card_rival_expanded  a rival building expanded: owner + private, no
--                          running state, no charts.
--
-- Buildings and tiles are found through verify.buildings() and
-- verify.find_deposit_tile, never hard-coded.
--
-- Run with: ProjectIo --verify scripts/verify/tile_card.lua

verify.window(1720, 1080)
verify.goto_surface("home")
verify.econ_step(6)  -- a few ticks so every building carries a report row

local function card() return verify.tile_card() end

-- Group every building on the home body by tile.
local function group()
    local by_tile, order = {}, {}
    for _, b in ipairs(verify.buildings()) do
        if not by_tile[b.tile] then
            by_tile[b.tile] = { x = b.x, y = b.y, n = 0, player = b.player }
            order[#order + 1] = b.tile
        end
        by_tile[b.tile].n = by_tile[b.tile].n + 1
    end
    return by_tile, order
end
local by_tile, order = group()

local function show_tile(x, y)
    verify.select_tile(x, y)
    frame_tile(x, y, 18)
    verify.frames(3)
    local c = card()
    verify.expect(c.drawn, "the tile element drew")
    verify.expect(not c.is_expanded, "a TILE selection shows the grid, not an expanded card")
    return c
end

-- Column widths, measured off the draw (reported, not asserted to a number).
do
    local all = verify.buildings()
    local any = all[1]
    local c = show_tile(any.x, any.y)
    print(string.format("tile_card: columns at 1720x1080 - preview %.0f, province %.0f, tile %.0f, actions %.0f, height %.0f px",
                        c.preview_w, c.province_w, c.tile_w, c.actions_w, c.height))
    verify.expect(math.abs(c.province_w - c.tile_w) <= 1.0,
                  "the two cards split the centre equally")
end

-- An unbuilt land tile with a deposit.
local unbuilt = nil
for _, g in ipairs({ "coal", "iron_ore", "copper_ore", "timber", "stone",
                     "agricultural_produce", "crude_oil" }) do
    local d = verify.find_deposit_tile(g)
    if d.ok then
        local taken = false
        for _, t in ipairs(order) do
            if by_tile[t].x == d.x and by_tile[t].y == d.y then taken = true; break end
        end
        if not taken then unbuilt = d; break end
    end
end
assert(unbuilt, "tile_card.lua: no unbuilt deposit tile found")
do
    local c = show_tile(unbuilt.x, unbuilt.y)
    verify.expect(#c.buttons == 0, "an unbuilt tile has no building buttons")
    verify.expect(#c.province_rows > 0, "the province card lists its workable resources")
    verify.capture("tile_card_unbuilt")
    if c.capacity_x >= 0 then
        verify.hover(c.capacity_x, c.capacity_y, 3)
        verify.capture("tile_card_capacity_tip")
    else
        print("tile_card: capacity unknown on this province - tooltip capture skipped")
    end
end

-- A built player tile.
local player_site = nil
for _, b in ipairs(verify.buildings()) do
    if b.player and b.complete and not player_site then player_site = b end
end
assert(player_site, "tile_card.lua: no completed player building on the home body")
do
    local c = show_tile(player_site.x, player_site.y)
    verify.expect(#c.buttons > 0, "a built tile shows at least one building button")
    for _, b in ipairs(c.buttons) do
        verify.expect(b.detail ~= "", "every button carries its third line: " .. b.label)
    end
    verify.hover(860, 300, 2) -- park the pointer on the canvas, off the band (no press)
    verify.frames(2)
    verify.capture("tile_card_built")
end

-- A stacked tile, the player's own by preference.
local stacked = nil
for _, t in ipairs(order) do
    if by_tile[t].n >= 2 and by_tile[t].player then stacked = by_tile[t]; break end
end
if not stacked then
    for _, t in ipairs(order) do
        if by_tile[t].n >= 2 then stacked = by_tile[t]; break end
    end
end
if stacked then
    show_tile(stacked.x, stacked.y)
    verify.capture("tile_card_stacked")
else
    print("tile_card: no tile carries two buildings; the stacked capture was skipped")
end

-- PRESS a player building button (a real click at its published centre): the
-- stack expands in place, and the action grid retargets.
do
    local c = show_tile(player_site.x, player_site.y)
    local mine = nil
    for _, b in ipairs(c.buttons) do if b.open then mine = b; break end end
    verify.expect(mine ~= nil, "the player's tile has an open (own) button")
    if mine then
        verify.click(mine.x, mine.y)
        verify.frames(3)
        local e = card()
        verify.expect(e.drawn and e.is_expanded, "a button press expands the building in place")
        verify.expect(e.tile == c.tile, "the expanded card is the SAME tile's card")
        verify.expect(e.player, "the expanded building is the player's")
        verify.expect(e.status ~= "", "the expanded card carries a status line")
        verify.expect(#e.buttons == 0, "the grid gives way to the expanded card")
        local cc = verify.construction_controls()
        verify.expect(cc.open_ledger_ok, "the action grid retargets: Open in ledger is on it")
        verify.hover(860, 300, 2)
        verify.frames(2)
        verify.capture("tile_card_expanded")

        -- The back control returns the grid.
        e = card()
        verify.click(e.back_x, e.back_y)
        verify.frames(3)
        local r = card()
        verify.expect(r.drawn and not r.is_expanded, "the back control returns to the tile's grid")
        verify.expect(r.tile == c.tile, "...on the same tile")
        verify.hover(860, 300, 2)
        verify.frames(2)
        verify.capture("tile_card_back")
    end
end

-- A rival's tile: buttons withhold.
local rival_site = nil
for _, b in ipairs(verify.buildings()) do
    if (not b.player) and b.type == "Processing Facility" then rival_site = b; break end
end
if not rival_site then
    for _, b in ipairs(verify.buildings()) do
        if not b.player then rival_site = b; break end
    end
end
assert(rival_site, "tile_card.lua: no rival building on the home body")
do
    local c = show_tile(rival_site.x, rival_site.y)
    local closed = 0
    for _, b in ipairs(c.buttons) do
        if not b.open then
            closed = closed + 1
            verify.expect(b.good == "", "rival button names no good (got '" .. b.good .. "')")
            verify.expect(b.detail == "private" or b.detail:find("ETA", 1, true) == 1
                          or b.detail == "Paused",
                          "rival button reads private (or its ETA): '" .. b.detail .. "'")
            for _, bb in ipairs(verify.buildings()) do
                if bb.x == rival_site.x and bb.y == rival_site.y and not bb.player
                   and bb.group ~= bb.type then
                    verify.expect(b.label:find(bb.group, 1, true) == nil,
                                  "rival button hides the recipe group '" .. bb.group .. "'")
                end
            end
        end
    end
    verify.expect(closed > 0, "rival tile: at least one closed button")
    verify.capture("tile_card_rival")

    -- Selecting the rival building by any route draws the same expanded card.
    verify.select_building(rival_site.x, rival_site.y)
    verify.frames(3)
    local e = card()
    verify.expect(e.drawn and e.is_expanded, "selecting a building draws its tile with it expanded")
    verify.expect(not e.player, "the expanded building is a rival's")
    verify.expect(not e.charts, "a rival's card draws no Profitability charts")
    if rival_site.complete then
        verify.expect(e.status == "", "a rival's running state is private: '" .. e.status .. "'")
    end
    local cr = verify.construction_controls()
    verify.expect(not cr.open_ledger_ok, "a rival's grid carries no Open in ledger")
    verify.capture("tile_card_rival_expanded")
end

verify.expect_no_clipping("tile_card")
