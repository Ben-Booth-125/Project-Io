-- Verify the unit designer (docs/ui/ledgers/military.md § The designer;
-- requirement group `unit-designer` R3).
--
-- 1. The Military ledger's Design tab: one press per land class.
-- 2. A class press opens the designer on the full canvas: infantry (two slot
--    boxes) and armour (three), each box joined to an anchor on the image.
-- 3. A box press opens the slot's item list: base first, each later item LOCKED
--    with its research node named (no kit node is in the age tree yet).
-- 4. A locked item is refused: its pick is disabled with the seam's reason, and
--    pressing it changes nothing.
-- 5. Toggles: the box press closes its list; the class press that opened the
--    designer closes it, leaving the ledger where it was; the return control too.
-- 6. The unit card's type name opens the designer for the unit's class, and
--    pressing it again closes it.
--
-- Every press aims at the point the surface published (verify.military_ledger,
-- verify.unit_designer). No world or tile data is read beyond verify.units().

verify.goto_surface("home")
verify.clear_selection()
verify.set_overlay("none")
verify.mouse(-1, -1)
verify.frames(3)

local fails = 0
-- Both go through verify.expect, so a failure fails the run, not just the log.
local function fail(msg) fails = fails + 1; verify.expect(false, "unit_designer: " .. msg) end
local function ok(msg)   verify.expect(true, "unit_designer: " .. msg) end
local function settle() verify.mouse(-1, -1); verify.frames(3) end

-- 1. Military ledger, Design tab.
local sx, sy = verify.nav_slot(7)
assert(sx, "rail slot 7 was not drawn")
verify.click(sx, sy)
settle()
local led = verify.military_ledger()
assert(led.open, "slot 7 did not open the Military ledger")
verify.click(led.tab_design_x, led.tab_design_y)
settle()
led = verify.military_ledger()
if led.view == 1 then ok("Design tab is showing") else fail("Design tab did not open (view " .. led.view .. ")") end
print(string.format("unit_designer: %d class row(s) drawn", #led.classes))

local function class_row(cls)
    for _, c in ipairs(verify.military_ledger().classes) do
        if c.cls == cls then return c end
    end
    return nil
end

local INFANTRY, ARMOUR = 0, 3

local function show(tag)
    local d = verify.unit_designer()
    print(string.format("unit_designer[%s]: open=%s cls=%d slot_open=%d boxes=%d items=%d stale=%d msg='%s'",
        tag, tostring(d.open), d.cls, d.slot_open, #d.boxes, #d.items, d.stale_units, d.message))
    for _, b in ipairs(d.boxes) do
        print(string.format("  box slot %d %-12s fitted '%s' at (%.0f,%.0f) -> anchor (%.0f,%.0f)",
            b.slot, b.name, b.fitted, b.x, b.y, b.anchor_x, b.anchor_y))
    end
    for _, i in ipairs(d.items) do
        print(string.format("  item %3d %-48s %s%s%s %s", i.item, i.name,
            i.fitted and "FITTED " or "", i.locked and "LOCKED " or "", i.enabled and "LIVE" or "off", i.reason))
    end
    return d
end

-- 2a. Infantry.
local row = class_row(INFANTRY)
assert(row, "no infantry row in the Design tab")
verify.click(row.x, row.y)
settle()
local d = show("infantry")
if d.open and d.cls == INFANTRY and #d.boxes == 2 then ok("infantry opens with its two slot boxes") else
    fail("infantry designer: open=" .. tostring(d.open) .. " boxes=" .. #d.boxes) end
verify.capture("unit_designer_infantry")

-- 2b. Armour (a different class row switches the designer).
row = class_row(ARMOUR)
assert(row, "no armour row in the Design tab")
verify.click(row.x, row.y)
settle()
d = show("armour")
if d.open and d.cls == ARMOUR and #d.boxes == 3 then ok("armour opens with its three slot boxes") else
    fail("armour designer: open=" .. tostring(d.open) .. " boxes=" .. #d.boxes) end
for _, b in ipairs(d.boxes) do
    if b.anchor_x < 0 then fail("box " .. b.slot .. " has no anchor") end
end
verify.capture("unit_designer_armour")

-- 3. Hover then press the Armament box (slot 0): the item list.
local box0 = d.boxes[1]
for _, b in ipairs(d.boxes) do if b.slot == 0 then box0 = b end end
verify.hover(box0.x, box0.y, 3)
verify.capture("unit_designer_armour_hover")
verify.click(box0.x, box0.y)
settle()
d = show("armour slot 0")
if d.slot_open == 0 and #d.items >= 3 then ok("the Armament box opens its item list") else
    fail("item list: slot_open=" .. d.slot_open .. " items=" .. #d.items) end
if #d.items > 0 and d.items[1].fitted and not d.items[1].locked then ok("base item is first, and fitted") else
    fail("the first item is not the fitted base item") end

-- Hover a locked item: the summary previews its stats.
local locked = nil
for _, i in ipairs(d.items) do if i.locked then locked = i; break end end
if locked then
    verify.hover(locked.x, locked.y, 3)
    verify.capture("unit_designer_armour_items")
else
    verify.capture("unit_designer_armour_items")
    fail("no locked item in the Armament list")
end

-- 4. A locked item is refused.
if locked then
    if not locked.enabled and string.find(locked.reason, "Locked", 1, true) then
        ok("locked item '" .. locked.name .. "' is disabled: " .. locked.reason)
    else
        fail("locked item is not refused (enabled=" .. tostring(locked.enabled) .. ", reason '" .. locked.reason .. "')")
    end
    local before = box0.fitted
    verify.click(locked.x, locked.y)
    settle()
    d = verify.unit_designer()
    local after = ""
    for _, b in ipairs(d.boxes) do if b.slot == 0 then after = b.fitted end end
    if after == before and d.message == "" then ok("pressing the locked item changed nothing ('" .. after .. "')") else
        fail("pressing the locked item changed the design: '" .. before .. "' -> '" .. after .. "', msg '" .. d.message .. "'") end
end

-- 5a. The box press closes its own list.
d = verify.unit_designer()
for _, b in ipairs(d.boxes) do if b.slot == 0 then box0 = b end end
verify.click(box0.x, box0.y)
settle()
d = verify.unit_designer()
if d.slot_open == -1 then ok("re-pressing the box closes its list") else fail("box press did not close the list") end

-- 5b. The class press that opened it closes it; the ledger stays on Design.
row = class_row(ARMOUR)
verify.click(row.x, row.y)
settle()
d = verify.unit_designer()
led = verify.military_ledger()
if not d.open and led.open and led.view == 1 then ok("re-pressing the class row closes the designer; the ledger is where it was") else
    fail("toggle close: designer open=" .. tostring(d.open) .. ", ledger open=" .. tostring(led.open) .. " view " .. led.view) end
verify.capture("unit_designer_closed")

-- 5c. The return control closes it too.
row = class_row(INFANTRY)
verify.click(row.x, row.y)
settle()
d = verify.unit_designer()
if d.open and d.back_x >= 0 then
    verify.click(d.back_x, d.back_y)
    settle()
    if not verify.unit_designer().open then ok("the return control closes the designer") else fail("the return control did not close it") end
else
    fail("could not reopen the designer for the return-control check")
end

-- 6. The unit card's type name.
verify.show_panel("military", false)
settle()
local mine = nil
for _, u in ipairs(verify.units()) do
    if u.player and u.count > 0 then mine = u; break end
end
if mine == nil then
    print("unit_designer: the player has no unit; the card entry is not checked")
else
    verify.select_unit(mine.id)
    verify.unit_card_page(1) -- the roster page carries the type name
    settle()
    d = verify.unit_designer()
    if d.card_type_x < 0 then
        fail("the unit card drew no type-name press")
    else
        verify.click(d.card_type_x, d.card_type_y)
        settle()
        d = show("card")
        if d.open then ok("the unit card's type name opens the designer (class " .. d.cls .. ")") else
            fail("the type-name press did not open the designer") end
        verify.capture("unit_designer_from_card")
        d = verify.unit_designer()
        if d.card_type_x >= 0 then
            verify.click(d.card_type_x, d.card_type_y)
            settle()
            if not verify.unit_designer().open then ok("re-pressing the type name closes it") else
                fail("re-pressing the type name did not close the designer") end
        end
    end
end

print(string.format("unit_designer: done, %d failure(s)", fails))
