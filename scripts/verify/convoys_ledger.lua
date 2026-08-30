-- The Convoys ledger (BL-689): rail slot 7, one flat view, arms supply_routes.
--
-- IT PRESSES THE RAIL SLOT rather than calling show_panel, and that is the point
-- of the check. `shell_pass` opens ledgers BY NAME, so it cannot see a
-- slot->surface mapping that has silently shifted (NR-716) — and this change
-- renumbered seven slots. Pressing slot 7 and requiring the Convoys ledger to be
-- what opens is the only form of the assertion that exercises the mapping.

-- The open fold-out surface, by the name pointer_target reports.
local function open_panel() return verify.pointer_target()["open_panel"] end

verify.goto_surface("home")
-- Enough ticks for the auto-dispatcher to put cargo in flight; an empty ledger
-- would satisfy most of what follows vacuously.
verify.econ_step(16)
verify.frames(2)

-- A lens that is NOT supply_routes, so "opening it armed the lane overlay" is a
-- real transition and not the state we happened to start in.
verify.set_overlay("market")
verify.frames(2)
verify.expect(verify.overlay_name() == "market", "precondition: the market lens is armed")

-- 1. SLOT 7 OPENS CONVOYS. The rail geometry comes from the slot as DRAWN, so a
--    renumber moves the click with it and the assertion still means what it says.
local x, y = verify.nav_slot(7)
verify.expect(x ~= nil, "nav rail slot 7 exists and was drawn")
if x ~= nil then
    verify.click(x, y)
    verify.frames(2)
    verify.expect(open_panel() == "convoys",
        "pressing rail slot 7 opens the Convoys ledger (opened: " ..
        tostring(open_panel()) .. ")")

    -- 2. IT ARMS `supply_routes` ON OPEN (convoys.md § 3). This is the pairing a
    --    tab strip could never give it: hosting Convoys in the Market ledger
    --    armed the price wash instead, because a strip arms one lens for all tabs.
    verify.expect(verify.overlay_name() == "supply_routes",
        "opening Convoys arms the supply-routes lens (armed: " ..
        verify.overlay_name() .. ")")

    verify.capture("convoys_ledger")

    -- 3. THE TOGGLE RULE: pressing the active slot again closes it.
    verify.click(x, y)
    verify.frames(2)
    verify.expect(open_panel() ~= "convoys",
        "pressing slot 7 again closes the Convoys ledger")
end

-- 4. MARKET KEEPS SLOT 6, AND THE SHIFTED SLOTS LAND WHERE MENU.md SAYS. The
--    renumber is the risk this file exists to cover, so the neighbours are
--    checked by press too, not just the new slot.
local expected = { [3] = "construction", [5] = "acquisitions", [6] = "market",
                   [9] = "corporations", [10] = "tile", [11] = "generation_ledger",
                   [12] = "decisions", [13] = "strategy", [14] = "contracts" }
for slot, want in pairs(expected) do
    local sx, sy = verify.nav_slot(slot)
    verify.expect(sx ~= nil, "nav rail slot " .. slot .. " was drawn")
    if sx ~= nil then
        verify.click(sx, sy)
        verify.frames(2)
        local got = tostring(open_panel())
        verify.expect(got == want,
            "slot " .. slot .. " opens " .. want .. " (opened: " .. got .. ")")
        verify.click(sx, sy) -- leave the column empty for the next leg
        verify.frames(2)
    end
end

-- 5. THE LEDGER SCROLLS, and the request reaches a real scroller (NR-719). The
--    Convoys window is its own scroller — no nested child — so this exercises
--    foldout_begin's own hook rather than the child one the Goods table uses.
verify.show_panel("convoys", true)
verify.frames(2)
verify.scroll_panel("convoys", 1.0)
verify.frames(2)
verify.expect_scrolled("convoys list scrolled to the foot")
verify.capture("convoys_ledger_foot")
verify.scroll_panel("convoys", 0.0)
verify.frames(2)

-- 6. DESTINATIONS DO NOT CLIP. `convoys.md` records `Huhaidar -> Kua Sua…`
--    overrunning the column edge — NR-709's family, four surfaces so far. The
--    route now takes its own line and WRAPS (container 1's stated policy), so a
--    clipped/unfittable record here is a real regression. Note this call is
--    weak on its own (NR-663) and is a backstop to the capture, not the verdict.
verify.expect_no_clipping("convoys ledger")
verify.capture("convoys_ledger_1920")
