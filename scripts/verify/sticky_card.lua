-- Visual verification for the sticky detail card (BL-194 frame + BL-195 content move).
--
-- The card is the click-opened, canvas-confined home of the Selection content: a
-- single click opens it (binary open/closed) at the click anchor, it renders the
-- FULL per-kind Selection layout (tile graphs / building operate panel / action|
-- facts) — not the terse hover card — and it persists until dismissed. In --verify
-- the click anchor is the {-1,-1} sentinel, so the card centres on the free canvas
-- rect (clear of the shell column, header, and right chrome column).
--
-- Driver: direct state manipulation (select_tile / select_building /
-- clear_selection) — there is no click/key injection in the headless harness.
--
-- Run with: ProjectIo --verify scripts/verify/sticky_card.lua

verify.goto_surface("home")

-- 1) A single click opens the card (open).
verify.select_tile(0, 0)
verify.capture("sticky_card_00_open")

-- 2) Deselect (BL-266 — dismissal is retired): the band stays open and rests on
--    the player's own corporation. The capture shows the corp resting state.
verify.clear_selection()
verify.capture("sticky_card_01_resting_corp")

-- 3) A fresh selection re-points the band from the resting state.
verify.select_tile(0, 1)
verify.capture("sticky_card_02_reopened")

-- 4) The card must host the RICH per-kind layouts, not just a stat line. Build a
--    producing extraction site (guaranteeing its tile carries a deposit) so both
--    the building layout AND a deposit tile's production graphs can be shown in the
--    card. Same staging as building_element.lua: sweep the worked rival cluster
--    around (155,37) and take the first tile that accepts the placement (tile data
--    is not exposed to Lua, so the deposit tile cannot be asked for directly).
verify.econ_step(6)
verify.goto_surface("home")
verify.place_mode("extraction", "iron_ore")

local bx, by = nil, nil
for dx = -3, 3 do
    for dy = -3, 3 do
        if verify.build_at(155 + dx, 37 + dy) == "placed" then
            bx, by = 155 + dx, 37 + dy
            break
        end
    end
    if bx then break end
end
if not bx then
    error("sticky_card.lua: no valid iron tile near the rival cluster")
end

verify.econ_step(40) -- finish the build and produce a few ticks

-- Close every fold-out ledger econ_step / placement may have opened, so the capture
-- shows the card over a clean canvas rather than a column ledger beside it.
verify.show_panel("construction", false)
verify.show_panel("tile",         false)
verify.show_panel("market",       false)
verify.show_panel("balance",      false)
verify.show_panel("corporation",  false)
verify.show_panel("build",        false)

-- The built extraction site: a player building routes to the building operate layout
-- INSIDE the card (output lead, profitability, method combo, workforce dial).
local built = nil
for _, b in ipairs(verify.buildings()) do
    if b.player and b.type == "Extraction Site" then built = b end
end
if built ~= nil then
    verify.center_tile(built.x, built.y)
    verify.select_building(built.x, built.y)
    verify.capture("sticky_card_03_building")
end

-- A deposit-bearing tile: the iron deposit is a contiguous cluster, so an UNBUILT
-- neighbour of the extraction site typically carries iron too and shows the
-- per-resource production graphs (Tile vs Top-10%). Sweep the ring and take the
-- first neighbour that is not the built tile itself.
local tx, ty = nil, nil
for _, d in ipairs({ {1,0}, {0,1}, {-1,0}, {0,-1}, {1,1}, {-1,-1} }) do
    local cx, cy = bx + d[1], by + d[2]
    if not (cx == built.x and cy == built.y) then
        tx, ty = cx, cy
        break
    end
end
if tx then
    verify.center_tile(tx, ty)
    verify.select_tile(tx, ty)
    verify.capture("sticky_card_04_deposit_tile")

    -- 5) Drill into the tile's first deposited resource → the dual-axis time-series
    --    chart (BL-196/197/198): the body aggregate (columns, left axis) with this
    --    tile's own series (line, right axis) over a Year/Quarter X axis. The body
    --    aggregate already carries ~46 ticks of history; the tile series starts
    --    recording at the drill, so advance time to build its line, then capture.
    local drilled = verify.card_drill()
    if drilled >= 0 then
        verify.econ_step(24)
        verify.show_panel("construction", false)
        verify.show_panel("tile",         false)
        verify.show_panel("market",       false)
        verify.show_panel("balance",      false)
        verify.show_panel("corporation",  false)
        verify.show_panel("build",        false)
        verify.capture("sticky_card_05_resource_chart")
    end
end
