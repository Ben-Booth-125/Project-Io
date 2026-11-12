-- Verify the Space ledger (rail slot 9; docs/ui/ledgers/space.md).
--
-- 1. Slot 9 is Space: pressing it opens the ledger on Launch.
-- 2. Launch: the class picker, the destination list (free slots only for a
--    slot-holding class), the quote, and the Launch press - a station core to
--    the home low orbit, then a Comms satellite to the home high orbit, each
--    through the real press and the near-space seam.
-- 3. Assets: one row per launched asset, with coverage.
-- 4. Stations: the designer grid is 5x3 with the core at the centre and the edge
--    marked; after a quarter the core is in place and the budgets read its supply.
--    A slot press opens the module list; picking Power fills the slot (the grid
--    read changes), and a quarter later the power budget carries it.
-- 5. Templates: Save layout lists the template.
-- 6. Crew: the pool and the training centres.
-- 7. Toggle rule: re-pressing the active tab closes the ledger; the rail re-opens
--    it and re-pressing the rail closes it.
--
-- Staging: verify.space_ledger_fixture gives the player nation the technology, a
-- staffed Launchpad and training centre, astronauts, money and a stocked home
-- shelf - and nothing in space. Every launch and fit is a press on the ledger.

verify.goto_surface("home")
verify.clear_selection()
verify.set_overlay("none")
verify.mouse(-1, -1)
verify.frames(3)

local fails = 0
local function check(cond, msg)
    if cond then print("space_ledger: PASS " .. msg)
    else print("space_ledger: FAIL " .. msg); fails = fails + 1 end
    verify.expect(cond, msg)
end
local function press(x, y)
    verify.click(x, y)
    verify.mouse(-1, -1)
    verify.frames(3)
end
local function led() return verify.space_ledger() end

-- Scroll the column until the press @p pick finds lies inside its visible span,
-- as a player would scroll to it. Returns the press (re-read at that scroll), or nil.
local function reach(pick)
    for step = 0, 10 do
        verify.scroll_panel("space", step / 10)
        verify.frames(3)
        local l = led()
        local p = pick(l)
        if p and p.y > l.view_top + 30 and p.y < l.view_bottom - 15 then return p end
    end
    return nil
end
local function module_press(id)
    return function(l)
        for _, m in ipairs(l.module_presses) do if m.id == id then return m end end
        return nil
    end
end

local fx = verify.space_ledger_fixture()
check(fx.ok, "the fixture staged a pad and a stocked shelf (staged " .. tostring(fx.staged) .. ")")

-- 1. The rail.
local sx, sy = verify.nav_slot(9)
assert(sx, "rail slot 9 was not drawn")
press(sx, sy)
check(verify.pointer_target().open_panel == "space", "slot 9 opens the Space ledger")
check(led().view == 0, "it opens on Launch")

local function node_row(kind)
    for _, n in ipairs(led().node_rows) do
        if n.body == fx.home_body and n.kind == kind then return n end
    end
    return nil
end

-- 2. A station core to home low orbit (kind 1).
local l = led()
check(#l.class_presses == 4, "four launch classes drawn")
press(l.class_presses[2].x, l.class_presses[2].y)
check(led().launch_class == 1, "Station core chosen")
local low = node_row(1)
check(low ~= nil, "the home low orbit is a destination row")
if low then
    check(low.free > 0, "the row shows a free slot (" .. low.free .. "/" .. low.capacity .. ")")
    press(low.x, low.y)
end
l = led()
check(l.launch.enabled, "the core's quote is clear: " .. tostring(l.launch.reason))
check(l.launch_cost > 0, "the quote prices it: " .. tostring(l.launch_cost))
verify.capture("space_ledger_launch")
press(l.launch.x, l.launch.y)
check(led().message == "Found station: done.", "Found station: " .. led().message)

-- A Comms satellite to the home high orbit (kind 2).
l = led()
press(l.class_presses[1].x, l.class_presses[1].y)
l = led()
check(#l.role_presses == 4, "four roles drawn for a satellite")
press(l.role_presses[1].x, l.role_presses[1].y) -- Comms
local high = node_row(2)
check(high ~= nil, "the home high orbit is a destination row")
if high then press(high.x, high.y) end
l = led()
check(l.launch.enabled, "the satellite's quote is clear: " .. tostring(l.launch.reason))
press(l.launch.x, l.launch.y)
check(led().message == "Launch: done.", "Launch satellite: " .. led().message)

-- 3. Assets.
press(led().tabs[2].x, led().tabs[2].y)
l = led()
check(l.view == 1, "Assets view")
check(#l.asset_rows == 2, "two asset rows: " .. #l.asset_rows)
local covered = true
for _, a in ipairs(l.asset_rows) do covered = covered and a.covered end
check(covered, "both home-orbit assets read in coverage")
verify.capture("space_ledger_assets")

-- 4. Stations: the designer.
press(l.tabs[3].x, l.tabs[3].y)
l = led()
check(l.view == 2, "Stations view")
check(l.grid_w == 5 and l.grid_h == 3, "the grid is 5x3 (" .. l.grid_w .. "x" .. l.grid_h .. ")")
check(#l.slots == 15 and l.slots[8].core, "fifteen slots, the core at the centre (slot 7)")
local edges = 0
for _, s in ipairs(l.slots) do if s.edge then edges = edges + 1 end end
check(edges == 12, "twelve edge slots marked on a 5x3 grid: " .. edges)

verify.econ_step(1) -- the core arrives
verify.frames(3)
l = led()
local b0 = l.budget
check(b0.drawn and b0.power_supply > 0, "the core supplies power: " .. b0.power_supply)
print(string.format("space_ledger: budget before fit power %.1f/%.1f room %d LS %d crew %d/%d",
    b0.power_supply, b0.power_draw, b0.crew_room, b0.life_support, b0.crew_aboard, b0.crew_needed))
verify.capture("space_ledger_designer")

-- Press slot 1 (top row), pick Power.
local slot = l.slots[2]
check(slot ~= nil, "slot 1 drawn")
if slot == nil then
    print("space_ledger: " .. fails .. " FAIL")
    return
end
press(slot.x, slot.y)
check(led().slot == 1, "slot 1 picked")
-- The module list sits under the grid: scroll the column until Power is on screen.
local power = reach(module_press(2))
l = led()
check(power ~= nil and power.enabled, "the module list offers Power, on screen: " .. tostring(power and power.reason))
check(#l.module_presses > 1, "the list holds the researched modules: " .. #l.module_presses)
verify.capture("space_ledger_module_list")
if power then press(power.x, power.y) end
verify.scroll_panel("space", 0.0)
verify.frames(3)
l = led()
check(l.message == "Fit module: done.", "Fit module: " .. l.message)
check(l.slots[2].kind == 2, "the grid read changed: slot 1 holds Power (kind " .. l.slots[2].kind .. ")")
verify.econ_step(1) -- the module arrives
verify.frames(3)
l = led()
print(string.format("space_ledger: budget after fit power %.1f/%.1f, slot 1 '%s'",
    l.budget.power_supply, l.budget.power_draw, l.slots[2].state))
check(l.budget.power_supply > b0.power_supply, string.format("the power budget carries the module: %.1f -> %.1f",
    b0.power_supply, l.budget.power_supply))
check(l.slots[2].state == "working", "the Power module is working: " .. l.slots[2].state)

-- 5. Save the layout as a template.
verify.capture("space_ledger_designer_fitted")
verify.space_template_name("Starter")
local save = reach(function(x) return x.save.x >= 0 and x.save or nil end)
check(save ~= nil and save.enabled, "Save layout is live with a name, on screen")
if save then press(save.x, save.y) end
l = led()
check(#l.template_presses == 1 and l.template_presses[1].reason == "Starter", "the template is listed")
verify.capture("space_ledger_designer_foot")
verify.scroll_panel("space", 0.0)
verify.frames(3)

-- Un-pick: pressing the picked slot again.
l = led()
press(l.slots[2].x, l.slots[2].y)
check(led().slot == -1, "re-pressing the picked slot un-picks it")

-- 6. Crew.
press(l.tabs[4].x, l.tabs[4].y)
l = led()
check(l.view == 3 and l.crew.drawn, "Crew view")
check(l.crew.centres >= 1, "a training centre counted: " .. l.crew.centres)
print(string.format("space_ledger: crew pool %d cap %d centres %d running %d rate %d aboard %d needed %d",
    l.crew.pool, l.crew.cap, l.crew.centres, l.crew.running, l.crew.rate, l.crew.aboard, l.crew.needed))
check(l.crew.aboard >= 1, "the core's crew is aboard")
verify.capture("space_ledger_crew")

-- 7. Toggle rule.
press(l.tabs[4].x, l.tabs[4].y)
check(not led().open, "re-pressing the active tab closes the ledger")
sx, sy = verify.nav_slot(9)
press(sx, sy)
check(led().open, "the rail re-opens it")
press(sx, sy)
check(not led().open, "re-pressing the rail closes it")

print("space_ledger: " .. (fails == 0 and "ALL PASS" or (fails .. " FAIL")))
