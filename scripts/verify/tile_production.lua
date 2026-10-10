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
--   tile_production_rival      R4: a rival's tile (a processing facility by
--                              preference) — type, count, owner, and "private"
--                              for output and state. ASSERTED off
--                              verify.tile_production_rows(): no recipe group
--                              name, no good, nothing listed as made here.
--   tile_production_door       R3: a press on a good row opens the Market ledger
--                              aimed at that market and good (asserted on the
--                              drawn Goods table, captured); a second press on
--                              the aimed row closes it (the Toggle rule).
--   tile_production_idle       R2: the player tile with its workforce set to zero
--                              and ticks run — the row reads Idle with its reason.
--   tile_production_mixed      R2: two player processing facilities on one tile
--                              on different recipes — two rows, two goods, both
--                              listed as made (skipped, said so, if unstageable).
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
-- different readings. A rival PROCESSING tile by preference: its recipe is the
-- private fact (DISCOVERY.md § The operational fog) the row must withhold.
local now_all = verify.buildings()
local rival_site = nil
for _, b in ipairs(now_all) do
    if (not b.player) and b.type == "Processing Facility"
       and (not stacked or b.x ~= stacked.x or b.y ~= stacked.y) then
        rival_site = b; break
    end
end
if not rival_site then
    print("tile_production: no rival processing facility on the home body; "
          .. "the rival recipe-leak check falls back to any rival building")
    for _, b in ipairs(now_all) do
        if (not b.player) and (not stacked or b.x ~= stacked.x or b.y ~= stacked.y) then
            rival_site = b; break
        end
    end
end
assert(rival_site, "tile_production.lua: no rival building on the home body")
show_tile(rival_site.x, rival_site.y, "tile_production_rival")

-- The rival row WITHHOLDS (BL-1239 fix round): no recipe group name, no good,
-- nothing listed as "made here" — only the public type, count and owner, and
-- output/state "private". Read off the section as drawn, since an absence is
-- exactly what a capture cannot prove.
do
    local rows = verify.tile_production_rows()
    verify.expect(#rows > 0, "rival tile: the Production section drew rows")
    local here = {}
    for _, b in ipairs(now_all) do
        if b.x == rival_site.x and b.y == rival_site.y then here[#here + 1] = b end
    end
    local any_open, closed = false, 0
    for _, r in ipairs(rows) do
        if r.stack and r.open then any_open = true end
        if r.stack and not r.open then
            closed = closed + 1
            verify.expect(r.good == "",
                          "rival stack row names no good (got '" .. r.good .. "')")
            verify.expect(r.detail:find("private", 1, true) ~= nil,
                          "rival stack row reads output/state private: '" .. r.detail .. "'")
            local typed = false
            for _, b in ipairs(here) do
                if not b.player then
                    if r.label:find(b.type, 1, true) == 1 then typed = true end
                    if b.group ~= b.type then
                        verify.expect(r.label:find(b.group, 1, true) == nil,
                                      "rival stack row hides the recipe group '" .. b.group
                                      .. "' (label '" .. r.label .. "')")
                    end
                end
            end
            verify.expect(typed, "rival stack row is labelled by its public type: '" .. r.label .. "'")
        end
    end
    verify.expect(closed > 0, "rival tile: at least one closed (rival) stack row")
    if not any_open then
        for _, r in ipairs(rows) do
            verify.expect(r.stack or r.detail ~= "made",
                          "rival-only tile lists nothing as made here (got " .. r.label .. ")")
        end
    end
end

-- R3: the door. Back to the player's tile; press a good row. The rows sit under
-- the stack lines, so their position is not fixed — sweep down the centre column
-- until the Market ledger draws its Goods table, then assert on it.
verify.select_tile(player_site.x, player_site.y)
frame_tile(player_site.x, player_site.y, 18)
verify.frames(3)
local opened, door_y = false, nil
for y = 540, 700, 6 do
    verify.click(560, y)
    verify.frames(3)
    local rows = verify.goods_table()
    if rows and #rows > 0 and verify.pointer_target().open_panel == "market" then
        opened, door_y = true, y; break
    end
end
verify.expect(opened, "a good row press opens the Market ledger's Goods table")
verify.expect(verify.pointer_target().has_selection,
              "the door leaves the tile selected behind it")
verify.capture("tile_production_door")
-- Toggle rule: the aimed row reads selected, so a second press on it shuts the
-- ledger rather than re-aiming it.
if door_y then
    verify.click(560, door_y)
    verify.frames(3)
    verify.expect(verify.pointer_target().open_panel ~= "market",
                  "a second press on the aimed good row closes the Market ledger (got "
                  .. tostring(verify.pointer_target().open_panel) .. ")")
end
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

-- A MIXED-RECIPE tile (BL-1239 fix round): two player processing facilities on
-- one tile running different recipes are TWO rows, each naming its own good,
-- and Part 2 lists both as made. Staged through the Build door's own seam
-- (place_mode + build_at, which places the default recipe) and the
-- construction panel's recipe write (set_building_recipe). Best effort: a
-- world whose start refuses a processing facility skips it, said so.
do
    local opening = verify.player_balance()
    verify.set_balance(opening + 1000000)
    verify.place_mode("processing")
    -- The unbuilt tile only: set_building_recipe writes the FIRST building it
    -- meets on the tile, so a tile holding anything but the two processors
    -- would make the write land on the wrong building.
    local staged = nil
    if verify.build_at(unbuilt.x, unbuilt.y) == "placed"
       and verify.build_at(unbuilt.x, unbuilt.y) == "placed" then
        staged = unbuilt
    end
    verify.set_balance(opening)
    if not staged then
        print("tile_production: the unbuilt tile did not take two processing facilities; "
              .. "the mixed-recipe check was skipped")
    else
        local tile_id = nil
        for _, b in ipairs(verify.buildings()) do
            if b.x == staged.x and b.y == staged.y then tile_id = b.tile; break end
        end
        local function open_rows()
            verify.select_tile(staged.x, staged.y)
            verify.frames(3)
            local stacks, made = {}, {}
            for _, r in ipairs(verify.tile_production_rows()) do
                if r.stack and r.open then stacks[#stacks + 1] = r end
                if (not r.stack) and r.detail == "made" then made[r.label] = true end
            end
            return stacks, made
        end
        local base = #open_rows()
        local n = (verify.first_processing_building().recipes or 0)
        local split = false
        for i = 0, n - 1 do
            verify.set_building_recipe(tile_id, i)
            local stacks, made = open_rows()
            if #stacks == base + 1 then
                split = true
                local goods = {}
                for _, r in ipairs(stacks) do
                    if r.good ~= "" then
                        verify.expect(made[r.good] == true,
                                      "mixed tile: Part 2 lists " .. r.good .. " as made")
                        goods[r.good] = true
                    end
                end
                local distinct = 0
                for _ in pairs(goods) do distinct = distinct + 1 end
                verify.expect(distinct >= 2, "mixed tile: the rows name different goods")
                frame_tile(staged.x, staged.y, 18)
                verify.frames(2)
                verify.capture("tile_production_mixed")
                break
            end
        end
        verify.expect(split, "two processing facilities on different recipes read as two rows")
    end
end

verify.expect_no_clipping("tile_production")
