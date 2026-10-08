-- The tile Selection element's PRODUCTION section (BL-1239, tile production
-- section). SELECTION.md § The tile element's layout, the Production bullet;
-- requirements.json group `tile-production-section`.
--
-- What each capture is for:
--   tile_production_status_page R5: the building card's Status page reading the
--                              running state with its reason, never "Operating.".
--                              Taken FIRST, before any economy tick: a finished
--                              player building only falls back to its Status
--                              page while the report has no row for it (once it
--                              has one, Profitability is its page).
--   tile_production_built      R1/R2/R3: a player-built tile — the nav opens on
--                              Production (1/6), one row per stack with glyph,
--                              count, good, output per tick and running state,
--                              then the catchment market and its good rows.
--   tile_production_stacked    R2: a tile carrying two or more buildings (the
--                              player's own by preference), so the stack grouping
--                              — a count above 1, or two stack rows — is on screen.
--   tile_production_unbuilt    R4: a land tile with a deposit and nothing built —
--                              "Nothing built here", its deposited goods still
--                              priced at their market.
--   tile_production_rival      R4: a rival's tile — type, count, owner, and
--                              "private" for output and state.
--   tile_production_door       R3: a press on a good row opens the Market ledger
--                              aimed at that market and good (asserted on the
--                              drawn Goods table, captured).
--   tile_production_idle       R2: the player tile with its workforce set to zero
--                              and ticks run — the row reads Idle with its reason.
--
-- Buildings and tiles are found through verify.buildings() and
-- verify.find_deposit_tile, never hard-coded, so a generation change re-aims the
-- check instead of blinding it.
--
-- Run with: ProjectIo --verify scripts/verify/tile_production.lua

verify.window(1280, 720)
verify.goto_surface("home")

local function section() return verify.pointer_target().selection_section end

-- Group every building on the home body by tile.
local by_tile, order = {}, {}
local all = verify.buildings()
for _, b in ipairs(all) do
    if not by_tile[b.tile] then
        by_tile[b.tile] = { x = b.x, y = b.y, n = 0, player = b.player, corp = b.corp }
        order[#order + 1] = b.tile
    end
    by_tile[b.tile].n = by_tile[b.tile].n + 1
end

-- The built capture wants a PRODUCING player building (extraction or
-- processing), so the row carries a good and an output — a port makes nothing.
local function producing(b)
    return b.type == "Extraction Site" or b.type == "Processing Facility"
end
local player_site = nil
for _, b in ipairs(all) do
    if b.player and b.complete and producing(b) and not player_site then player_site = b end
end
for _, b in ipairs(all) do
    if b.player and b.complete and not player_site then player_site = b end
end
assert(player_site, "tile_production.lua: no completed player building on the home body")

-- R5: the Status page, before any tick (see the header).
verify.select_building(player_site.x, player_site.y)
frame_tile(player_site.x, player_site.y, 18)
verify.frames(3)
local pages = verify.building_pages()
local has_status = false
for i = 1, pages.count do
    if pages.labels[i] == "Status" then verify.building_page(i - 1); has_status = true; break end
end
verify.expect(has_status, "an unreported player building offers its Status page")
verify.frames(2)
verify.capture("tile_production_status_page")

verify.econ_step(6)  -- a few ticks so every building carries a report row

local function show_tile(x, y, name)
    verify.select_tile(x, y)
    frame_tile(x, y, 18)
    verify.frames(3)
    verify.expect(section() == 0,
                  name .. ": the nav opens on Production (section 0), got " .. tostring(section()))
    verify.capture(name)
end

-- R1/R2/R3: a built player tile.
show_tile(player_site.x, player_site.y, "tile_production_built")

-- R1: the nav wraps at SIX. Real presses on the right chevron, each asserted to
-- advance by exactly one, and six of them returning to Production. Measured at
-- 1280x720 off tile_production_built: the right chevron sits at (718, 515).
-- (selection_accordion.lua asserts the same walk, but its fixture cannot raise
-- a unit on this world — raise_player_force is tech-locked — so the wrap is
-- asserted here too.)
for i = 1, 6 do
    local before = section()
    verify.click(718, 515)
    verify.frames(2)
    verify.expect(section() == (before + 1) % 6,
                  "right chevron: section " .. tostring(before) .. " -> " .. tostring(section()))
end
verify.expect(section() == 0, "six presses of the right chevron return to Production")
verify.click(860, 478) -- empty band header: park the pointer off the chevron's tooltip
verify.frames(2)

-- R2: a stacked tile — the player's own first, so the open half (output and
-- state) is on screen; any owner's otherwise. Re-read after the ticks (the
-- background corps build during them), and if the player has no stack, raise
-- one: a second site on the player's own tile, the way the Build door would.
by_tile, order = {}, {}
for _, b in ipairs(verify.buildings()) do
    if not by_tile[b.tile] then
        by_tile[b.tile] = { x = b.x, y = b.y, n = 0, player = b.player, corp = b.corp }
        order[#order + 1] = b.tile
    end
    by_tile[b.tile].n = by_tile[b.tile].n + 1
end
local player_stack = false
for _, t in ipairs(order) do
    if by_tile[t].n >= 2 and by_tile[t].player then player_stack = true; break end
end
if not player_stack then
    local opening = verify.player_balance()
    verify.set_balance(opening + 1000000)
    -- The four targets place_mode can arm.
    for _, g in ipairs({ "iron_ore", "agricultural_produce", "water", "petroleum" }) do
        verify.place_mode("extraction", g)
        if verify.build_at(player_site.x, player_site.y) == "placed" then
            local t = by_tile[player_site.tile]
            t.n = t.n + 1
            print("tile_production: stacked a " .. g .. " site on the player's tile")
            break
        end
    end
    verify.set_balance(opening)
    verify.econ_step(1)
end
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
    show_tile(stacked.x, stacked.y, "tile_production_stacked")
else
    print("tile_production: no tile on the home body carries two buildings; "
          .. "the stacked capture was skipped")
end

-- R4: an unbuilt land tile with a deposit.
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
assert(unbuilt, "tile_production.lua: no unbuilt deposit tile found")
show_tile(unbuilt.x, unbuilt.y, "tile_production_unbuilt")

-- R4: a rival's tile, other than the stacked one, so the two captures are two
-- different readings.
local rival_site = nil
for _, b in ipairs(all) do
    if (not b.player) and (not stacked or b.x ~= stacked.x or b.y ~= stacked.y) then
        rival_site = b; break
    end
end
assert(rival_site, "tile_production.lua: no rival building on the home body")
show_tile(rival_site.x, rival_site.y, "tile_production_rival")

-- R3: the door. Back to the player's tile; press a good row. The rows sit under
-- the stack lines, so their position is not fixed — sweep down the centre column
-- until the Market ledger draws its Goods table, then assert on it.
verify.select_tile(player_site.x, player_site.y)
frame_tile(player_site.x, player_site.y, 18)
verify.frames(3)
local opened = false
for y = 540, 700, 6 do
    verify.click(560, y)
    verify.frames(3)
    local rows = verify.goods_table()
    if rows and #rows > 0 then opened = true; break end
end
verify.expect(opened, "a good row press opens the Market ledger's Goods table")
verify.expect(verify.pointer_target().has_selection,
              "the door leaves the tile selected behind it")
verify.capture("tile_production_door")
verify.show_panel("market", false)
verify.frames(2)

-- R2: the same player tile, idled by labour.
-- The set_workforce verb (corp_command.hpp: 3) through the same seam the
-- Construction ledger's lever writes, which also clears workforce_auto so the
-- economy tick does not re-solve the target straight back; set_workforce_auto
-- (11) hands the dial back afterwards.
local VERB_SET_WORKFORCE, VERB_SET_WORKFORCE_AUTO = 3, 11
verify.expect(verify.corp_command{ verb = VERB_SET_WORKFORCE, subject = player_site.id,
                                   workforce = 0 } == "applied",
              "set_workforce 0 applies to the player's building")
verify.econ_step(2)
verify.click(860, 478) -- empty band header: moves the pointer off the rows so no tooltip covers the capture
show_tile(player_site.x, player_site.y, "tile_production_idle")
verify.corp_command{ verb = VERB_SET_WORKFORCE_AUTO, subject = player_site.id }
verify.econ_step(2)

verify.expect_no_clipping("tile_production")
