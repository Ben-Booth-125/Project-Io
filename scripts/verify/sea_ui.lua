-- Verify the sea UI (BL-1534; docs/ui/ledgers/military.md § Grouped by theatre and § Ships on
-- the map, docs/military/MILITARY.md § Sea, docs/economy/LOGISTICS.md § 7).
--
-- 1. Recruit's Sea tab: the port picker, the four ship rows each with a Raise press (a dimmed
--    one says why), "Your ships".
-- 2. A real Raise through hire_ship, with the money, goods and crew staged.
-- 3. A staged warship: its marker on the water selects it, the card lists only the missions its
--    class works and the lit one stands down (the toggle rule).
-- 4. A right-click on the water under the ship's marker assigns that sea province; the card's x
--    takes it off. With no mission chosen the same water refuses, and the press agrees.
-- 4b. A rival's ship: no marker until the day's pass found it, then the stack counts it and a
--    press reaches its card, which offers no mission and states no orders.
-- 5. A transport: the card has no mission, a unit at the port boards it through its press, and a
--    right-click on the port's own coast orders its landing; Recall calls it off.
-- 6. Design's Sea tab opens the designer on a ship class; the transport has no armament box.
-- 7. The closed-port mark and the interception mark and the Convoys ledger's Lost row.
--
-- Every press aims at a point the surface published: a marker, a row, or the port the ledger lists
-- (`verify.tile_screen` on its grid position). No tile is walked.

verify.goto_surface("home")
verify.clear_selection()
verify.set_overlay("none")
verify.mouse(-1, -1)
verify.frames(3)

local fails = 0
local function fail(msg) fails = fails + 1; print("sea_ui: FAIL " .. msg) end
local function ok(msg)   print("sea_ui: PASS " .. msg) end
local function check(c, msg) if c then ok(msg) else fail(msg) end end
local function settle(n) verify.mouse(-1, -1); verify.frames(n or 3) end

local function ttab(led, tab, theatre)
    for _, h in ipairs(led.theatre_tabs) do
        if h.tab == tab and h.theatre == theatre then return h end
    end
end
local function press_theatre(tab, theatre)
    local t = ttab(verify.military_ledger(), tab, theatre)
    if not t then fail("no " .. theatre .. " tab"); return verify.military_ledger() end
    verify.click(t.x, t.y); settle()
    return verify.military_ledger()
end

-- 1. Recruit's Sea tab.
verify.show_panel("military", true)
verify.panel_view("military", 0)
settle()
local led = press_theatre(0, "sea")
check(led.recruit_theatre == "sea", "the Sea tab switches the theatre")
check(ttab(led, 0, "sea").count == 4, "Sea counts the four ship rows")
check(#led.sea_raises == 4, "the roster draws four rows, " .. #led.sea_raises .. " drawn")
for _, r in ipairs(led.sea_raises) do
    if not r.enabled and r.reason == "" then fail("sea row " .. r.row .. " is off with no reason") end
end
verify.capture("sea_recruit")

-- The seat's own port may stand on a lake, with no mouth: its rows say so. Stage one on the sea.
for _, r in ipairs(led.sea_raises) do print("sea_ui: opening port, row " .. r.row .. ": " .. r.reason) end
local port = verify.stage_sea_port()
if port == 0 then
    fail("the verify world's seat has no coast for a sea port")
    verify.show_panel("military", false)
    return
end
settle(3)
local port_x, port_y
for _, b in ipairs(verify.buildings()) do
    if b.id == port then port_x, port_y = b.x, b.y end
end
led = verify.military_ledger()
for _, p in ipairs(led.ports) do
    if p.id == port then verify.click(p.x, p.y); settle() end
end
led = verify.military_ledger()
check(led.sea_base == port, "the picker takes the staged port")
print(string.format("sea_ui: port at grid %s,%s", tostring(port_x), tostring(port_y)))
verify.center_tile(port_x, port_y, 6.0)
print(string.format("sea_ui: %d port(s), sea_base %d", #led.ports, led.sea_base))

-- 1b. The goods leg dims a row, naming what is short (the seam's own plan, read_ship_hire_goods).
verify.grant_recruits("sea", 1000)
settle()
led = verify.military_ledger()
for _, r in ipairs(led.sea_raises) do
    print("sea_ui: before stock, row " .. r.row .. ": " .. (r.enabled and "lit" or r.reason))
    if not r.enabled and r.reason:find("^Short of") then
        check(r.reason:find("%(%d+ of %d+%)") ~= nil, "a short row names held and needed: " .. r.reason)
    end
end

-- 1c. A row's name is its card: a glance, then frozen after 2 s, its cells taking hover.
local nr = led.sea_raises[2]
check(nr.name_x > 0, "the destroyer row publishes its name")
verify.hover(nr.name_x, nr.name_y, 30)
check(verify.stat_grids().raise_card ~= nil, "hovering the name shows the row's card")
check(not verify.military_ledger().card_frozen, "half a second on, the card is still a glance")
verify.hover(nr.name_x, nr.name_y, 100)
check(verify.military_ledger().card_frozen, "two seconds on, the card freezes")
local grid = verify.stat_grids().raise_card
check(grid ~= nil and #grid > 0, "the frozen card draws its stat grid")
if grid ~= nil and #grid > 0 then
    verify.hover(grid[1].x, grid[1].y, 5)
    check(verify.military_ledger().card_frozen, "the frozen card holds while the pointer is on its cells")
    verify.capture("sea_row_card_frozen")
end
verify.hover(4, 4, 3)
check(not verify.military_ledger().card_frozen, "leaving the frozen card closes it")

-- 2. A real Raise through hire_ship.
for _, g in ipairs({"steel", "machinery", "electronics", "alloys"}) do verify.grant_stock(g, 30) end
settle()
led = verify.military_ledger()
local before = #verify.ships()
local raised = false
for _, r in ipairs(led.sea_raises) do
    if r.enabled then
        verify.click(r.x, r.y); settle(4); raised = true
        print("sea_ui: Raise row " .. r.row .. " -> " .. verify.military_ledger().sea_message)
        break
    else
        print("sea_ui: row " .. r.row .. " off: " .. r.reason)
    end
end
if raised then
    local msg = verify.military_ledger().sea_message
    if msg == "Ship mustering." then
        check(#verify.ships() == before + 1, "a pressed Raise raises a ship")
        check(#verify.military_ledger().ship_rows == before + 1, "Your ships lists it")
    else
        fail("the seam refused the Raise: " .. msg)
    end
else
    fail("no Raise press was lit with the money, goods and crew staged")
end
verify.show_panel("military", false)
settle()

-- 3. A warship: marker, card, the toggle.
local dd = verify.stage_ship(port, 1)
check(dd ~= 0, "a destroyer is staged at the port")
check(verify.select_ship(dd), "select_ship takes it")
verify.frames(3)
local card = verify.ship_card()
check(card.ship == dd, "the card draws the selected ship")
check(#card.missions == 5, "a destroyer's card offers None and its four missions, " .. #card.missions .. " drawn")
check(card.status ~= "", "the card states what it is doing: " .. card.status)
verify.capture("sea_card_destroyer")

-- The water under the ship's own marker: its port's mouth.
local function marker_of(id)
    verify.frames(2)
    for _, mk in ipairs(verify.ship_card().markers) do
        if mk.ship == id or mk.count > 1 then return mk end
    end
end
local mk = marker_of(dd)
check(mk ~= nil, "the staged ship has a marker on the water")
if not mk then
    print("sea_ui: " .. fails .. " failure(s)")
    return
end
local function mission_of(id)
    for _, s in ipairs(verify.ships()) do if s.id == id then return s end end
end

-- No mission chosen: the water refuses and says why, and the press agrees.
verify.hover(mk.x, mk.y, 2)
local c0 = verify.ship_card()
check(c0.hover_kind == "refused", "the water refuses with no mission: " .. c0.hover_text)
verify.click(mk.x, mk.y, "right"); settle(4)
check(mission_of(dd).mission == 0, "and the press sends nothing")
verify.select_ship(dd); verify.frames(3)

-- Hold Patrol on the card, then a right-click on water assigns.
card = verify.ship_card()
local patrol
for _, m in ipairs(card.missions) do if m.mission == 1 then patrol = m end end
verify.click(patrol.x, patrol.y); settle()
card = verify.ship_card()
local lit
for _, m in ipairs(card.missions) do if m.mission == 1 then lit = m end end
check(lit.active, "Patrol is lit and held on the card")
check(verify.ships()[#verify.ships()].mission == 0, "a held mission sends nothing")
verify.hover(mk.x, mk.y, 2)
local c1 = verify.ship_card()
local ax, ay, atext = mk.x, mk.y, c1.hover_text
if c1.hover_kind == "order" then
    verify.click(ax, ay, "right"); settle(4)
    local ships = verify.ships()
    local me
    for _, s in ipairs(ships) do if s.id == dd then me = s end end
    check(me.mission == 1 and me.provinces == 1, "a right-click assigns: " .. atext)
    verify.capture("sea_zone_assigned")
    card = verify.ship_card()
    check(#card.removes == 1, "the card lists the province, its x beside it")
    verify.click(card.removes[1].x, card.removes[1].y); settle(4)
    for _, s in ipairs(verify.ships()) do if s.id == dd then me = s end end
    check(me.mission == 0, "its last x recalls the ship")
    card = verify.ship_card()
    for _, m in ipairs(card.missions) do if m.mission == 1 then lit = m end end
    check(lit.active, "and the mission stays held on the card")
    verify.click(lit.x, lit.y); settle()
    card = verify.ship_card()
    for _, m in ipairs(card.missions) do if m.mission == 1 then lit = m end end
    check(not lit.active, "pressing the lit mission drops it (the toggle rule)")
else
    fail("the water under the marker takes no order: " .. c1.hover_text)
end

-- 3b. The marker selects.
verify.clear_selection()
verify.frames(3)
card = verify.ship_card()
print("sea_ui: markers drawn " .. #card.markers)
if #card.markers > 0 then
    local mk = card.markers[1]
    verify.click(mk.x, mk.y); settle()
    check(verify.ship_card().ship ~= 0, "a press on a marker selects a ship")
end

-- 4b. A rival's ship in the same water: hidden until found.
verify.clear_selection()
verify.frames(3)
local function stack_count()
    verify.frames(2)
    local best = 0
    for _, m in ipairs(verify.ship_card().markers) do
        if m.count > best then best = m.count end
    end
    return best
end
local unseen_count = stack_count()
local riv = verify.stage_ship(port, 1, true)
check(riv ~= 0, "a rival's ship is staged")
check(stack_count() == unseen_count, "the rival's ship draws nothing until it is found")
verify.stage_ship_found(riv, true)
check(stack_count() == unseen_count + 1, "the day's find puts it on the stack")
-- Walk the stack as a player does: each press selects the next.
local reached = false
for _ = 1, unseen_count + 2 do
    local m = marker_of(dd)
    verify.click(m.x, m.y); settle()
    if verify.ship_card().ship == riv then reached = true; break end
end
check(reached, "a press on the stack reaches the found ship")
if reached then
    local rc = verify.ship_card()
    check(#rc.missions == 0, "a rival's card offers no mission")
    check(rc.status == "", "and states no orders")
    verify.capture("sea_card_rival")
    verify.hover(mk.x, mk.y, 2)
    check(verify.ship_card().hover_kind == "none", "a rival selected: the water offers no order")
end
verify.stage_ship_found(riv, false)
check(stack_count() == unseen_count, "a day without the find takes it off the map")

-- 5. A transport.
local tr = verify.stage_ship(port, 3)
verify.select_ship(tr)
verify.frames(3)
card = verify.ship_card()
check(#card.missions == 0, "a transport's card offers no mission")
local staged = verify.stage_unit_at_port(port)
verify.frames(3)
card = verify.ship_card()
if staged ~= 0 and #card.boards > 0 then
    verify.capture("sea_card_transport")
    local before_units = card.aboard_rows
    verify.click(card.boards[1].x, card.boards[1].y); settle(4)
    card = verify.ship_card()
    check(card.aboard_rows == before_units + 1, "Board puts the unit aboard: " .. card.message)
    local ls = verify.tile_screen(port_x, port_y)
    verify.hover(ls.x, ls.y, 2)
    local c2 = verify.ship_card()
    local lx, ly, ltext = ls.x, ls.y, c2.hover_text
    if ls.ok and c2.hover_kind == "order" then
        verify.click(lx, ly, "right"); settle(4)
        card = verify.ship_card()
        local me
        for _, s in ipairs(verify.ships()) do if s.id == tr then me = s end end
        check(me.landing, "a right-click on a coast orders the landing: " .. ltext)
        verify.capture("sea_landing_ordered")
        check(card.recall_x > 0, "the card offers Recall")
        verify.click(card.recall_x, card.recall_y); settle(4)
        for _, s in ipairs(verify.ships()) do if s.id == tr then me = s end end
        check(not me.landing, "Recall calls it off: " .. verify.ship_card().message)
    else
        fail("the port's coast takes no landing: " .. c2.hover_kind .. " " .. c2.hover_text)
    end
else
    fail("no unit ready to board (staged " .. staged .. ", rows " .. #card.boards .. ")")
end
verify.clear_selection()

-- 6. Design's Sea tab.
verify.show_panel("military", true)
verify.panel_view("military", 1)
settle()
led = press_theatre(1, "sea")
check(#led.sea_classes == 4, "Design lists the four ship classes")
for _, c in ipairs(led.sea_classes) do
    if c.cls == 3 then
        verify.click(c.x, c.y); settle(4)
        local d = verify.ship_designer()
        check(d.open and d.cls == 3, "a class row opens the designer on it")
        check(d.boxes == 2, "a transport has no armament: two boxes, " .. d.boxes .. " drawn")
        verify.capture("sea_designer_transport")
        break
    end
end
verify.show_panel("military", false)
settle(4)
check(not verify.ship_designer().open, "closing the ledger closes the designer")

-- 7. The marks and the Lost row.
verify.stage_closed_port(port)
verify.stage_interception(port, true)
verify.show_panel("convoys", true)
settle(4)
card = verify.ship_card()
check(card.closed_marks >= 1, "the closed port is marked")
check(card.loss_marks >= 1, "the taken cargo's tile is marked")
check(card.loss_rows == 1, "the Convoys ledger states the cause")
verify.capture("sea_marks_and_lost")
verify.show_panel("convoys", false)

print(string.format("sea_ui: %d failure(s)", fails))
