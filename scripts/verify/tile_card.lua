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
--   tile_card_population_tip  the header's population figure hovered: the
--                          province's centres (round 2: the list left the card).
--   tile_card_single_press a REAL canvas press on a single-building hex: the
--                          tile's grid, never the building (round 2).
--   (round 2 also asserts the province card's own window never scrolls, and the
--    expanded card's header count, method, net / expenses figures and slider)
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
    verify.expect(c.province_scroll_max == 0,
                  "the province card's own window does not scroll (range "
                  .. tostring(c.province_scroll_max) .. ")")
    verify.capture("tile_card_unbuilt")
    if c.population_x >= 0 then
        verify.hover(c.population_x, c.population_y, 3)
        verify.capture("tile_card_population_tip")
    end
    if c.capacity_x >= 0 then
        verify.hover(c.capacity_x, c.capacity_y, 3)
        verify.capture("tile_card_capacity_tip")
    else
        print("tile_card: capacity unknown on this province - tooltip capture skipped")
    end
end

-- A canvas PRESS on a single-building hex selects the TILE, opening its grid
-- (BL-1300 round 2): never the building. A real press through click_tile.
do
    local single = nil
    for _, t in ipairs(order) do
        if by_tile[t].n == 1 then single = by_tile[t]; break end
    end
    if single then
        verify.clear_selection()
        frame_tile(single.x, single.y, 18)
        verify.frames(2)
        verify.expect(verify.click_tile(single.x, single.y), "the press on a single-building hex landed")
        verify.frames(3)
        local c = card()
        verify.expect(c.drawn and not c.is_expanded,
                      "a canvas press on a single-building hex shows the tile's GRID, not the building")
        verify.expect(#c.buttons == 1, "...with that building's one button")
        verify.hover(860, 300, 2)
        verify.capture("tile_card_single_press")
    else
        print("tile_card: no single-building tile found; the tile-first press check was skipped")
    end
end

-- A built player tile.
-- A PRODUCING building by preference (extraction or processing), so the
-- expanded card has a method, a profit row and a slider to show; any
-- completed player building otherwise (a port names no method).
local player_site = nil
for _, b in ipairs(verify.buildings()) do
    if b.player and b.complete and not player_site
       and (b.type == "Extraction Site" or b.type == "Processing Facility") then
        player_site = b
    end
end
if not player_site then
    -- The seeded player corp may own only a Port. Raise a producing site the
    -- way the Build door would, on a deposit the find helper names, and run
    -- quarters until it finishes (construction is material-gated, BL-095).
    local opening = verify.player_balance()
    verify.set_balance(opening + 1000000)
    for _, g in ipairs({ "agricultural_produce", "iron_ore" }) do
        local d = verify.find_deposit_tile(g)
        if d.ok then
            verify.place_mode("extraction", g)
            if verify.build_at(d.x, d.y) == "placed" then
                print("tile_card: raised a " .. g .. " site at [" .. d.x .. ", " .. d.y .. "]")
                break
            end
        end
    end
    verify.set_balance(opening)
    verify.econ_step(24)
    for _, b in ipairs(verify.buildings()) do
        if b.player and b.complete and not player_site
           and (b.type == "Extraction Site" or b.type == "Processing Facility") then
            player_site = b
        end
    end
    by_tile, order = group()
end
for _, b in ipairs(verify.buildings()) do
    if b.player and b.complete and not player_site then player_site = b end
end
assert(player_site, "tile_card.lua: no completed player building on the home body")
do
    local c = show_tile(player_site.x, player_site.y)
    verify.expect(#c.buttons > 0, "a built tile shows at least one building button")
    verify.expect(c.province_scroll_max == 0, "built tile: the province card's own window does not scroll")
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
    for _, b in ipairs(c.buttons) do if b.open and b.first == player_site.id then mine = b; break end end
    if not mine then
        for _, b in ipairs(c.buttons) do if b.open then mine = b; break end end
    end
    verify.expect(mine ~= nil, "the player's tile has an open (own) button")
    if mine then
        verify.click(mine.x, mine.y)
        verify.frames(3)
        local e = card()
        verify.expect(e.drawn and e.is_expanded, "a button press expands the building in place")
        verify.expect(e.tile == c.tile, "the expanded card is the SAME tile's card")
        verify.expect(e.player, "the expanded building is the player's")
        verify.expect(e.status ~= "", "the expanded card carries a status line")
        verify.expect(e.title:find("this tile", 1, true) == nil,
                      "the header carries no 'this tile's buildings' text: '" .. e.title .. "'")
        verify.expect(e.count == mine.count, "the header's count is the stack's (" .. tostring(e.count) .. ")")
        if e.count > 1 then
            verify.expect(e.title:find(utf8.char(215) .. tostring(e.count), 1, true) ~= nil,
                          "the header shows the stack count: '" .. e.title .. "'")
        end
        if player_site.type == "Extraction Site" or player_site.type == "Processing Facility" then
            verify.expect(e.method ~= "", "the expanded card names its production method: '" .. e.method .. "'")
        end
        if e.charts then
            verify.expect(e.net ~= "" and e.expenses ~= "",
                          "the profit row shows net and expenses: '" .. e.net .. "' / '" .. e.expenses .. "'")
            verify.expect(e.slider, "the player's completed building carries the workforce slider")
        else
            print("tile_card: the pressed building has no profit data yet - profit/slider asserts skipped")
        end
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

-- BL-1309 (SELECTION.md § The building element's layout: "Every card shows its
-- method and status line, always ... A building with no output still has
-- costs"). A player PORT, expanded: it names the trade points it makes, and its
-- profit row reads its (negative) net and expenses — a Port has no report row,
-- so before BL-1309 its card stopped after the status line.
do
    local port = nil
    for _, b in ipairs(verify.buildings()) do
        if b.player and b.complete and b.type == "Port" and not port then port = b end
    end
    if port then
        local c = show_tile(port.x, port.y)
        local btn = nil
        for _, b in ipairs(c.buttons) do
            for _, bb in ipairs(verify.buildings()) do
                if bb.id == b.first and bb.type == "Port" and b.open then btn = b end
            end
        end
        verify.expect(btn ~= nil, "the port's tile shows the port's own button")
        if btn then
            verify.click(btn.x, btn.y)
            verify.frames(3)
            local e = card()
            verify.expect(e.drawn and e.is_expanded, "the port's button expands its card")
            verify.expect(e.method ~= "", "a Port's card names what it does: '" .. e.method .. "'")
            verify.expect(e.method:find("trade points", 1, true) ~= nil,
                          "...its trade points: '" .. e.method .. "'")
            verify.expect(e.status ~= "", "a Port's card carries a status line: '" .. e.status .. "'")
            verify.expect(e.charts, "a Port's card draws its profit row")
            verify.expect(e.net ~= "" and e.expenses ~= "",
                          "a Port's profit row reads net and expenses: '" .. e.net .. "' / '" .. e.expenses .. "'")
            print(string.format("tile_card: port %d - method '%s', status '%s', net '%s', expenses '%s'",
                                port.id, e.method, e.status, e.net, e.expenses))
            verify.hover(860, 300, 2)
            verify.frames(2)
            verify.capture("tile_card_port_expanded")
        end
    else
        print("tile_card: the player owns no completed Port on the home body; the Port card check was skipped")
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
