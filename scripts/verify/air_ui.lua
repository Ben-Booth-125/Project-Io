-- Verify the air UI (BL-1450; docs/ui/ledgers/military.md § Grouped by theatre,
-- docs/military/MILITARY.md § Air).
--
-- 1. Recruit groups its rows under Land and Air headers, each with a count; a
--    header press folds its rows and a second press unfolds them (the toggle rule).
-- 2. With no airbase, the Air theatre says so; with one, the Air picker lists it
--    and the six air rows each carry a Raise press (dimmed rows say why).
-- 3. The player's wings are listed under Air; a row press selects the wing and the
--    Selection band shows its card: the missions its class flies, and None.
-- 4. A mission with no province is held on the card; a right-click on a province
--    in range assigns it (the hover line says so first), one out of range is
--    refused with its distance and the wing's range, and the card's remove press
--    takes a province back off.
-- 5. The wing marker at the airbase tile: hovering names a wing, pressing selects one.
-- 6. Design lists the six air classes under Air; a row opens the designer on the
--    air slots (airframe, armament, avionics).
--
-- Staging: an airbase is STAGED on the player's own ground (stage_building), and
-- wings are STAGED at it (stage_wing) — the hire's price and shelf gate are the
-- seam's and covered by air_combat_harness; this script is about the surfaces.
-- Every press aims at the point the surface published.

verify.goto_surface("home")
verify.clear_selection()
verify.set_overlay("none")
verify.mouse(-1, -1)
verify.frames(3)

local fails = 0
local function fail(msg) fails = fails + 1; print("air_ui: FAIL " .. msg) end
local function ok(msg)   print("air_ui: PASS " .. msg) end
local function settle(n) verify.mouse(-1, -1); verify.frames(n or 3) end

local function header(led, tab, theatre)
    for _, h in ipairs(led.headers) do
        if h.tab == tab and h.theatre == theatre then return h end
    end
    return nil
end

-- 1. Recruit, grouped.
verify.show_panel("military", true)
verify.panel_view("military", 0)
settle()
local led = verify.military_ledger()
local land, air = header(led, 0, "land"), header(led, 0, "air")
if land and air then ok(string.format("Recruit draws Land (%d) and Air (%d) headers", land.count, air.count))
else fail("Recruit is missing a theatre header") end
if air and air.count == 6 then ok("Air counts the six air classes") else fail("Air header count is not 6") end
verify.capture("air_ui_recruit_grouped")

-- The header is a toggle.
if land then
    verify.click(land.x, land.y); settle()
    led = verify.military_ledger()
    local l2 = header(led, 0, "land")
    if l2 and not l2.open and #led.raises == 0 then ok("pressing Land folds its rows") else
        fail("pressing Land did not fold it") end
    verify.capture("air_ui_recruit_land_folded")
    verify.click(l2.x, l2.y); settle()
    led = verify.military_ledger()
    local l3 = header(led, 0, "land")
    if l3 and l3.open and #led.raises > 0 then ok("pressing Land again unfolds it") else
        fail("the second press left Land folded") end
end

-- 2. The airbase.
local home = nil
for _, b in ipairs(verify.buildings()) do
    if b.player then home = b; break end
end
assert(home, "no player building to stand an airbase beside")
if #led.air_bases == 0 then ok("no airbase: the Air picker is empty") end
local airbase = verify.stage_building(home.x, home.y, "airbase")
assert(airbase ~= 0, "could not stage an airbase")
settle()
led = verify.military_ledger()
if #led.air_bases == 1 and led.air_base == airbase then ok("the Air picker lists the airbase and chooses it") else
    fail("the Air picker did not list the staged airbase (" .. #led.air_bases .. " rows)") end
for _, r in ipairs(led.air_raises) do
    print(string.format("  air row %d %-28s %s price %.0f %s", r.row, r.name,
                        r.enabled and "LIVE " or "off  ", r.price, r.reason))
end
if #led.air_raises == 6 then ok("six air rows, each with a Raise press") else fail("air rows drawn: " .. #led.air_raises) end

-- 3. Wings at the base, listed and selectable.
local fighter = verify.stage_wing(airbase, 0) -- Interceptor Wing (fighter)
local bomber  = verify.stage_wing(airbase, 2) -- Strategic Bomber Wing
assert(fighter ~= 0 and bomber ~= 0, "could not stage wings")
verify.scroll_panel("military", 1.0)
settle()
led = verify.military_ledger()
verify.capture("air_ui_recruit_air")
if #led.wing_rows == 2 then ok("Your wings lists both wings") else fail("wing rows: " .. #led.wing_rows) end
local row = nil
for _, r in ipairs(led.wing_rows) do if r.id == fighter then row = r end end
if row then
    verify.click(row.x, row.y); settle()
    local pt = verify.pointer_target()
    if pt.selection_kind == "wing" then ok("a wing row press selects the wing") else
        fail("the wing row press selected '" .. tostring(pt.selection_kind) .. "'") end
end
verify.scroll_panel("military", 0.0)
verify.show_panel("military", false)
settle()

local card = verify.wing_card()
if card.wing == fighter then ok("the band draws the wing card") else fail("no wing card for the fighter") end
local names = ""
for _, m in ipairs(card.missions) do names = names .. " " .. tostring(m.mission) .. (m.active and "*" or "") end
print("air_ui: fighter missions drawn:" .. names)
if #card.missions == 2 then ok("the fighter's card offers None and Air superiority only") else
    fail("the fighter's card drew " .. #card.missions .. " missions") end
verify.capture("air_ui_wing_card")

-- 4. Hold a mission, then assign by right-click.
local sup = nil
for _, m in ipairs(card.missions) do if m.mission == 1 then sup = m end end
if sup then
    verify.click(sup.x, sup.y); settle()
    card = verify.wing_card()
    for _, m in ipairs(card.missions) do if m.mission == 1 then sup = m end end
    if sup.active then ok("Air superiority is held on the card with no province yet") else
        fail("pressing Air superiority did not light it") end
end

verify.center_tile(home.x, home.y, 6.0)
settle(4)
local t = verify.tile_screen(home.x, home.y)
local function wing_of(id)
    for _, wg in ipairs(verify.wings()) do if wg.id == id then return wg end end
end
if t.ok then
    verify.hover(t.x, t.y, 3)
    card = verify.wing_card()
    print("air_ui: hover on the airbase's province: " .. card.hover_kind .. " - " .. card.hover_text)
    if card.hover_kind == "assign" then ok("the hover line offers the assignment") else
        fail("hover over an in-range province said " .. card.hover_kind) end
    verify.capture("air_ui_assign_hover")
    verify.click(t.x, t.y, "right"); settle()
    local wg = wing_of(fighter)
    if wg and wg.mission == 1 and wg.provinces == 1 then ok("the right-click assigned the province on Air superiority") else
        fail("after the right-click the wing flies mission " .. tostring(wg and wg.mission)
             .. " over " .. tostring(wg and wg.provinces)) end
    verify.capture("air_ui_assigned")
end

-- Out of range: walk east until the hover refuses with the distance.
local refused = false
for dx = 12, 60, 6 do
    verify.center_tile(home.x + dx, home.y, 6.0)
    settle(2)
    local p = verify.tile_screen(home.x + dx, home.y)
    if p.ok then
        verify.hover(p.x, p.y, 3)
        card = verify.wing_card()
        if card.hover_kind == "refused" and string.find(card.hover_text, "Out of range", 1, true) then
            print("air_ui: " .. card.hover_text)
            verify.capture("air_ui_out_of_range")
            verify.click(p.x, p.y, "right"); settle()
            local wg = wing_of(fighter)
            if wg and wg.provinces == 1 then ok("an out-of-range right-click changes nothing") else
                fail("an out-of-range right-click changed the assignment") end
            refused = true
            break
        end
    end
end
if refused then ok("a province out of range is refused with its distance and the range") else
    fail("no out-of-range refusal found walking east") end

-- Remove from the card.
verify.mouse(-1, -1); settle()
card = verify.wing_card()
if #card.removes == 1 then
    verify.click(card.removes[1].x, card.removes[1].y); settle()
    local wg = wing_of(fighter)
    card = verify.wing_card()
    local held = false
    for _, m in ipairs(card.missions) do if m.mission == 1 and m.active then held = true end end
    if wg and wg.provinces == 0 and held then ok("remove takes the province off; the mission stays held on the card") else
        fail("remove left " .. tostring(wg and wg.provinces) .. " provinces") end
else
    fail("the card drew " .. #card.removes .. " remove presses")
end

-- 5. The wing marker.
verify.clear_selection()
verify.center_tile(home.x, home.y, 10.0)
settle(4)
t = verify.tile_screen(home.x, home.y)
local found = nil
if t.ok then
    for d = 0, 60, 2 do
        verify.hover(t.x + d, t.y - d, 2)
        if verify.pointer_target().hovered_kind == "wing" then found = { x = t.x + d, y = t.y - d }; break end
    end
end
card = verify.wing_card()
print("air_ui: wing markers drawn " .. card.markers_drawn)
if found then
    ok("hovering the airbase's marker names a wing")
    verify.capture("air_ui_marker_hover")
    verify.click(found.x, found.y); settle()
    local first = verify.wing_card().wing
    verify.click(found.x, found.y); settle()
    local second = verify.wing_card().wing
    if first ~= 0 and second ~= 0 and first ~= second then ok("repeat presses on the marker walk the wings based there") else
        fail("marker presses selected " .. tostring(first) .. " then " .. tostring(second)) end
    verify.capture("air_ui_marker_selected")
else
    fail("the pointer never resolved to the wing marker")
end

-- 6. Design: the air classes, and the designer on air slots.
verify.clear_selection()
verify.show_panel("military", true)
verify.panel_view("military", 1)
settle()
led = verify.military_ledger()
local dair = header(led, 1, "air")
if dair and #led.air_classes == 6 then ok("Design lists six air classes under Air") else
    fail("Design air rows: " .. #led.air_classes) end
verify.scroll_panel("military", 1.0)
settle()
led = verify.military_ledger()
verify.capture("air_ui_design")
local ac = led.air_classes[#led.air_classes] -- the foot of the scrolled list: on screen
if ac then
    verify.click(ac.x, ac.y); settle()
    local d = verify.air_designer()
    if d.open and #d.boxes == 3 then ok("an air class opens the designer with three slots") else
        fail("the air designer drew " .. #d.boxes .. " boxes") end
    for _, b in ipairs(d.boxes) do print("  slot " .. b.slot .. " " .. b.name .. ": " .. b.fitted) end
    verify.capture("air_ui_air_designer")
    if d.boxes[1] then
        verify.click(d.boxes[1].x, d.boxes[1].y); settle()
        d = verify.air_designer()
        print("air_ui: airframe list " .. #d.items .. " items")
        verify.capture("air_ui_air_designer_slot")
    end
    -- The toggle rule: the row that opened it closes it.
    verify.click(ac.x, ac.y); settle()
    if not verify.air_designer().open then ok("pressing the lit air class closes the designer") else
        fail("the air designer stayed open") end
end

print(string.format("air_ui: done, %d failure(s)", fails))
