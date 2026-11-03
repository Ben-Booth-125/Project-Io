-- Visual verification for the building Selection element (Ben's 2026-07-22 review).
--
-- The element answers four questions: how profitable is this building, what can I
-- do to make it more profitable, how many more of these can I build here, and how
-- do I remove it. This check captures all of it in one frame: the output & rate
-- lead, the profitability block, the production-method combo, the workforce dial,
-- the stack-capacity readout, and the Idle / Demolish controls.
--
-- NOTE on staging: the Selection element only draws when NO nav ledger owns the
-- fold-out column (app gates it on ui::any_panel_open). building_profit.lua closed
-- only one ledger, which is why its golden shows an empty column and never
-- actually exercised this surface. This script closes every ledger that shares the
-- slot, then picks a player building from verify.buildings() rather than hard-coding
-- grid coordinates that a generation change can silently invalidate.
--
-- Run with: ProjectIo --verify scripts/verify/building_element.lua

verify.econ_step(6)          -- populate run states, prices, and the report rows
verify.goto_surface("home")  -- Kepler: fully surveyed, player buildings visible

-- Free the fold-out column: any open ledger suppresses the Selection element.
verify.show_panel("construction", false)
verify.show_panel("tile",         false)
verify.show_panel("market",       false)
verify.show_panel("balance",      false)
verify.show_panel("corporation",  false)
verify.show_panel("build",        false)

-- Two frames, because the element takes two shapes. A PRODUCING building (the
-- extraction site) is the main case: output lead, rate bar, method combo, stack
-- readout. Passive infrastructure (the port) produces nothing, and the check exists
-- to prove it says so rather than printing a hollow "0.0 Iron Ore / tick" off the
-- default target — which is exactly what the first cut of this panel did.
-- The player's nation may own no producing building to find, so the producing case
-- is built rather than found; the ticks after it let the building finish
-- construction and appear in the economy report, which is what the output lead and
-- the rate bar read.
--
-- THE PICK IS THE PLAYER'S OWN GROUND (BL-1457, building element stale aim). It used
-- to sweep a 7x7 box around (155,37), the rival cluster of an older generation; once
-- generation moved and BL-1399 kept a nation to its own ground, every tile there
-- answered invalid_tile or foreign_ground and the script errored. The verify API names
-- no tile's owner, by design, so the pick asks the SURFACE that already answers
-- "where may the player build this?": the site finder, whose rows are exactly the
-- tiles the build door accepts an Iron Mine on (own ground, tech, road, stack,
-- province ceiling, treasury). Its rows are read in drawn order (profit, ties by tile
-- id), deterministic for a seed, and they follow the player's ground wherever
-- generation moves it. The same idiom as construction_progress.lua.
verify.show_panel("build", true)
verify.frames(1)
local picked_ok = verify.site_pick("Iron Mine")
verify.frames(3)
local sites = verify.site_finder()
print(string.format("building_element: site finder armed=%s, %d Iron Mine site(s) of %d own tile(s)",
                    tostring(picked_ok), sites.n_rows or 0, sites.own_tiles or 0))
if not (picked_ok and (sites.n_rows or 0) > 0) then
    error("building_element.lua: the site finder lists no player ground for an Iron Mine")
end
verify.site_clear()
verify.show_panel("build", false)
verify.frames(2)

verify.place_mode("extraction", "iron_ore")

-- BL-1467: a site builds only while its local market's shelf can supply its build
-- materials; short of one, it pauses owing nothing (economy_system's site rate,
-- the BL-095 reason the old fixed tile was chosen). The top site-finder row's market
-- could not, so a fixed 40 steps captured a site still under construction - the
-- element's SITE shape, not its producing one. So: place on each accepting row in
-- drawn order, step up to 8 quarters, and keep the first site that completes. A
-- stalled site is left standing; the capture is refused rather than taken of the
-- wrong shape.
local placed, built = nil, nil
local refusals = "" -- in row order (the verify state opens no `table` library)
local stalled = ""
local k_tries, k_quarters = 6, 8
local tries = 0

local function find_at(row)
    for _, b in ipairs(verify.buildings()) do
        if b.player and b.type == "Extraction Site" and b.x == row.x and b.y == row.y then
            return b
        end
    end
    return nil
end

for i = 1, sites.n_rows do
    if tries >= k_tries then break end
    local row = sites.rows[i]
    local r = verify.build_at(row.x, row.y)
    if r ~= "placed" then
        refusals = refusals .. string.format("%s(%d,%d)=%s", refusals == "" and "" or ", ", row.x, row.y, r)
    else
        tries = tries + 1
        print(string.format("building_element: placed an Iron Mine at (%d,%d)", row.x, row.y))
        local b = nil
        for q = 1, k_quarters do
            verify.econ_step(1)
            b = find_at(row)
            if b == nil then
                error(string.format("building_element.lua: the extraction site placed at (%d,%d) did not register",
                                    row.x, row.y))
            end
            if b.complete then break end
        end
        if b.complete then
            placed, built = row, b
            break
        end
        stalled = stalled .. string.format("%s(%d,%d)", stalled == "" and "" or ", ", row.x, row.y)
    end
end
if not placed then
    error("building_element.lua: no Iron Mine site completed within " .. k_quarters ..
          " quarters; stalled: " .. stalled .. "; refused: " .. refusals)
end
print(string.format("building_element: the site at (%d,%d) is complete (stalled first: %s)",
                    placed.x, placed.y, stalled == "" and "none" or stalled))
verify.econ_step(5) -- produce for a few ticks, so the output & rate line reads a real rate
built = find_at(placed)

-- Producing building: the primary shape of the element.
verify.center_tile(built.x, built.y)
verify.select_building(built.x, built.y)
verify.capture("building_element_player")

-- Passive infrastructure: the no-output branch (the seeded Port).
local passive = nil
for _, b in ipairs(verify.buildings()) do
    if b.player and (b.type == "Port" or b.type == "Inland Logistics Hub") then
        passive = b
        break
    end
end
if passive ~= nil then
    verify.center_tile(passive.x, passive.y)
    verify.select_building(passive.x, passive.y)
    verify.capture("building_element_infrastructure")
end
