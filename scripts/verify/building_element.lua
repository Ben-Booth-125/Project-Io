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
-- The seeded player corporation owns a single Port and no producing building, so
-- the producing case has to be built rather than found. build_first_valid places an
-- extraction site on the first valid tile and selects it; the ticks after it let the
-- building finish construction and appear in the economy report, which is what the
-- output lead and the rate bar read.
-- Construction is material-gated (BL-095): a site draws build materials from its
-- LOCAL market each tick and stays paused forever while that market is dry. So the
-- tile is chosen, not taken from build_first_valid's global first hit — (154,37)
-- sits in the worked cluster around the rival extraction + processing sites at
-- (155,36)/(156,37), whose output is what makes that market able to supply a build.
-- Tile data is deliberately not exposed to Lua (standing rule), so the script cannot
-- ask which tiles carry iron. It sweeps the neighbourhood of the rival cluster
-- instead and takes the first tile that accepts the placement.
verify.place_mode("extraction", "iron_ore")

local placed = nil
for dx = -3, 3 do
    for dy = -3, 3 do
        local r = verify.build_at(155 + dx, 37 + dy)
        if r == "placed" then
            placed = true
            break
        end
    end
    if placed then break end
end

if not placed then
    error("building_element.lua: no valid iron tile near the rival cluster")
end

verify.econ_step(40) -- finish the build, then produce for a few ticks

local built = nil
for _, b in ipairs(verify.buildings()) do
    if b.player and b.type == "Extraction Site" then built = b end
end
if built == nil then
    error("building_element.lua: the placed extraction site did not register")
end

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
