-- Ledger opens, lens arms: every rail slot that pairs with a map lens.
--
--   ProjectIo --verify scripts/verify/lens_ledger_pairs.lua
--
-- THE PAIRING, and where it comes from. LENSES.md's routing table has always
-- sent a CLICK ON A LENS to a ledger - Market/Scarcity/Resource land on the
-- Market ledger, Continent lands on History. The reverse direction (open the
-- ledger, arm the lens) existed only as a per-doc proposal until slot 7 built
-- one, and Ben closed the Market half on 2026-08-30: "when we open the market
-- ledger, we should also activate the market lens".
--
-- THE RULE THESE SLOTS FOLLOW, which is slot 7's and is still formally unowned
-- (NR-722): arm on OPEN, never disarm on close. A player who opens a ledger gets
-- its lens; a player who closes it keeps whatever they are looking at rather than
-- having the canvas yanked back. Both halves are asserted below, because "does
-- not disarm" is a real behaviour and not merely the absence of one.
--
-- WHY IT CLICKS THE RAIL INSTEAD OF CALLING show_panel. The arm lives in the
-- slot's Selectable press in nav_pane.cpp, so `show_panel` - which writes the
-- ui_state flag directly - opens the ledger and arms NOTHING. So does
-- `verify.nav_slot`, which despite the name only returns the slot's centre
-- coordinates. A check built on either would have passed while proving nothing,
-- which is very likely why slot 7's pairing went unverified from the day it
-- landed. The press has to be a real one: nav_slot for the point, click for the
-- press.

verify.window(1920, 1080)

-- No stage_ui_fixture: this check presses rail slots and reads the overlay, and
-- needs neither the unit nor the twelve ticks that fixture stages.
verify.goto_surface("home")
verify.frames(3)
print("LENSPAIRS state_hash=" .. verify.state_hash())

local fails = 0

--- Press rail slot `slot`, and assert the overlay it should arm.
local function pair(slot, label, want, shot, disarms)
    verify.set_overlay("none")
    verify.frames(2)

    local x, y = verify.nav_slot(slot)
    if x == nil then
        print(string.format("FAIL %s: rail slot %d has no centre", label, slot))
        fails = fails + 1
        return
    end

    -- Open.
    verify.click(x, y)
    verify.frames(3)
    local armed = verify.overlay_name()
    if armed ~= want then
        print(string.format("FAIL %s: opened, expected lens '%s', got '%s'", label, want, armed))
        fails = fails + 1
    else
        print(string.format("PASS %s: opened -> lens '%s'", label, armed))
    end
    if shot then verify.capture(shot) end

    -- Close. The lens must SURVIVE - except on the Market slot, which disarms the
    -- lens it armed (Ben, 2026-10-13).
    verify.click(x, y)
    verify.frames(3)
    local after = verify.overlay_name()
    if disarms then
        if after ~= "none" then
            print(string.format("FAIL %s: closing kept the lens ('%s'), expected 'none'", label, after))
            fails = fails + 1
        else
            print(string.format("PASS %s: closed -> lens disarmed", label))
        end
    elseif after ~= want then
        print(string.format("FAIL %s: closing disarmed the lens ('%s' -> '%s')", label, want, after))
        fails = fails + 1
    else
        print(string.format("PASS %s: closed -> lens '%s' survives", label, after))
    end
end

-- Slot 5, Market ledger -> the price/catchment wash. Ben, 2026-08-30.
pair(5, "market ledger", "market", "lenspair_market", true)

-- Slot 6, Convoys ledger -> the lane overlay. Landed with BL-689 and, for the
-- reason in this file's header, has never been asserted until now.
pair(6, "convoys ledger", "supply_routes", "lenspair_convoys", true)

-- Slot 9, History -> the Continent lens, but ON ENTERING THE TECTONICS VIEW
-- rather than on opening the ledger (Ben's ruling on NR-742). A different rule
-- from the two above, so it gets its own assertions rather than pair()'s.
--
-- History carries four views and only Tectonics has a map twin, which is why a
-- fixed arm was refused: it would hand the Continent lens to a player who opened
-- on Story. Three properties are checked, and the third is the one that makes
-- "on entering" mean something.
local function tectonics_pair()
    local x, y = verify.nav_slot(9)
    if x == nil then
        print("FAIL history: rail slot 9 has no centre"); fails = fails + 1; return
    end

    -- 1. Opening History on a NON-Tectonics view must arm nothing.
    verify.set_overlay("none")
    verify.panel_view("history", 0)   -- Story
    verify.frames(2)
    verify.click(x, y)                -- open
    verify.frames(3)
    local on_story = verify.overlay_name()
    if on_story ~= "none" then
        print(string.format("FAIL history: opening on Story armed '%s', expected none", on_story))
        fails = fails + 1
    else
        print("PASS history: opening on Story arms nothing")
    end

    -- 2. Entering Tectonics arms the Continent lens.
    verify.panel_view("history", 3)   -- Tectonics
    verify.frames(3)
    local armed = verify.overlay_name()
    if armed ~= "continent" then
        print(string.format("FAIL history: entering Tectonics gave '%s', expected continent", armed))
        fails = fails + 1
    else
        print("PASS history: entering Tectonics -> lens 'continent'")
    end
    verify.capture("lenspair_tectonics")

    -- 3. A DELIBERATE lens change while sitting on Tectonics must SURVIVE. This is
    -- the whole difference between arming on the edge and arming every frame, and
    -- it is the property that would make the lens strip unusable if it regressed.
    verify.set_overlay("population")
    verify.frames(3)
    local kept = verify.overlay_name()
    if kept ~= "population" then
        print(string.format("FAIL history: a deliberate lens was overridden ('%s')", kept))
        fails = fails + 1
    else
        print("PASS history: a deliberate lens survives while on Tectonics")
    end

    -- 4. Closing keeps a lens the player chose (Ben, 2026-10-13: the disarm takes
    -- only the lens the ledger armed).
    verify.click(x, y)                -- close
    verify.frames(3)
    local after_choice = verify.overlay_name()
    if after_choice ~= "population" then
        print(string.format("FAIL history: closing replaced a chosen lens ('%s')", after_choice))
        fails = fails + 1
    else
        print("PASS history: closing keeps a chosen lens")
    end

    -- 5. Reopening onto the parked Tectonics arms continent; closing disarms it.
    verify.click(x, y)                -- reopen
    verify.frames(3)
    verify.click(x, y)                -- close
    verify.frames(3)
    local after_close = verify.overlay_name()
    if after_close ~= "none" then
        print(string.format("FAIL history: closing kept the lens ('%s'), expected 'none'", after_close))
        fails = fails + 1
    else
        print("PASS history: closed -> lens disarmed")
    end
end

tectonics_pair()

print(string.format("lens_ledger_pairs: %d failure(s)", fails))
verify.expect(fails == 0, "every paired ledger arms its lens on open and disarms it on close")
