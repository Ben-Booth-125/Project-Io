-- Verify the Military ledger (rail slot 7; docs/ui/ledgers/military.md).
--
-- 1. Slot 7 is Military: pressing it opens the ledger, its glyph lit.
-- 2. Recruit with no base: "No military base" and the press onto the
--    Construction ledger's site finder, pre-set to the military base.
-- 3. Recruit with a base chosen: the picker, the roster (dimmed rows say why),
--    each row's price and Raise press, and the muster queue's empty state.
-- 4. A Raise press through the real control: the hire seam's answer is shown.
-- 5. The Design tab: one row per land class.
-- 6. The toggle rule: re-pressing the active tab closes the ledger.
--
-- Staging: a base comes from lib.lua's raise_player_force (the real
-- construct_building and hire_unit paths). The Raise press is made with a loan
-- and stock granted, repaid after, so the treasury the captures show is the
-- real opening one. Every press aims at the point the ledger published.

verify.goto_surface("home")
verify.clear_selection()
verify.set_overlay("none")
verify.mouse(-1, -1)
verify.frames(3)

local function fail(msg) print("military_ledger: FAIL " .. msg) end
local function ok(msg)   print("military_ledger: PASS " .. msg) end

local function open_panel() return verify.pointer_target().open_panel end

-- 1. Press rail slot 7.
local sx, sy = verify.nav_slot(7)
assert(sx, "rail slot 7 was not drawn")
verify.click(sx, sy)
verify.mouse(-1, -1)
verify.frames(3)
if open_panel() == "military" then ok("slot 7 opens the Military ledger") else
    fail("slot 7 opened '" .. tostring(open_panel()) .. "', not military") end

local function player_base()
    for _, b in ipairs(verify.buildings()) do
        if b.player and b.type == "Military Base" and b.complete then return b end
    end
    return nil
end

-- 2. The empty state, when the opening world has no base.
local led = verify.military_ledger()
if player_base() == nil then
    verify.capture("military_ledger_no_base")
    if led.no_base_x >= 0 then ok("no-base state draws its site-finder press") else
        fail("no base, yet no 'Find a site' press was drawn") end
    -- The press opens Construction with the site finder armed on the base.
    verify.click(led.no_base_x, led.no_base_y)
    verify.mouse(-1, -1)
    verify.frames(3)
    print("military_ledger: no-base press opened '" .. tostring(open_panel()) .. "'")
    verify.capture("military_ledger_no_base_door")
    -- Stand a base up, then come back through the rail.
    local corp = nil
    for _, b in ipairs(verify.buildings()) do
        if b.player then corp = b.corp; break end
    end
    assert(corp, "no player-owned building found")
    raise_player_force(corp)
    verify.place_mode("none")
    verify.show_panel("military", false)
    verify.show_panel("construction", false)
    verify.frames(2)
    sx, sy = verify.nav_slot(7)
    verify.click(sx, sy)
    verify.mouse(-1, -1)
    verify.frames(3)
else
    print("military_ledger: the opening world already holds a completed base")
end

-- 3. Recruit with a base chosen.
local base = player_base()
assert(base, "no completed player military base")
verify.center_tile(base.x, base.y, 6.0)
verify.frames(3)
led = verify.military_ledger()
print(string.format("military_ledger: view %d, base %d, %d base row(s), %d roster rows, %d muster rows",
                    led.view, led.base, #led.bases, #led.raises, led.queue_rows))
if led.view == 0 and led.base ~= 0 then ok("Recruit is the default and a base is chosen") else
    fail("Recruit not default or no base chosen") end
local live, dimmed = 0, 0
for _, r in ipairs(led.raises) do
    if r.enabled then live = live + 1 end
    if not r.fieldable then dimmed = dimmed + 1 end
    print(string.format("  row %2d %-22s %s price %.0f %s", r.row, r.name,
                        r.enabled and "LIVE " or "off  ", r.price, r.reason))
end
print(string.format("military_ledger: %d live Raise press(es), %d dimmed row(s)", live, dimmed))
verify.capture("military_ledger_recruit")

-- 4. A Raise press through the real control. Lend and stock as the fixtures do,
--    then press the first live row; repay after.
local LOAN = 5000.0
verify.set_balance(verify.player_balance() + LOAN)
verify.grant_stock("iron_ore", 50)
verify.grant_stock("agricultural_produce", 50)
verify.grant_stock("coal", 50)
verify.frames(2)
led = verify.military_ledger()
local target = nil
for _, r in ipairs(led.raises) do
    if r.enabled then target = r; break end
end
if target then
    local units_before = 0
    for _, u in ipairs(verify.units()) do if u.player then units_before = units_before + 1 end end
    verify.click(target.x, target.y)
    verify.mouse(-1, -1)
    verify.frames(3)
    local units_after = 0
    for _, u in ipairs(verify.units()) do if u.player then units_after = units_after + 1 end end
    led = verify.military_ledger()
    print(string.format("military_ledger: Raise '%s' -> message '%s'; player units %d -> %d, muster rows %d",
                        target.name, led.message, units_before, units_after, led.queue_rows))
    if led.message ~= "" then ok("the Raise press reached the hire seam") else
        fail("the Raise press produced no answer") end
    verify.capture("military_ledger_raise")
else
    fail("no live Raise press even with the loan")
end
verify.set_balance(verify.player_balance() - LOAN)

-- 4b. The foot of Recruit: the muster queue (its honest empty state until a
--     muster lands), scrolled to.
verify.scroll_panel("military", 1.0)
verify.frames(3)
verify.capture("military_ledger_queue")
verify.scroll_panel("military", 0.0)
verify.frames(2)

-- 5. The Design tab, through its real press.
led = verify.military_ledger()
verify.click(led.tab_design_x, led.tab_design_y)
verify.mouse(-1, -1)
verify.frames(3)
led = verify.military_ledger()
if led.view == 1 then ok("the Design tab opens") else fail("Design tab press did not switch view") end
verify.capture("military_ledger_design")

-- 6. Re-pressing the active tab closes the ledger.
verify.click(led.tab_design_x, led.tab_design_y)
verify.mouse(-1, -1)
verify.frames(3)
if not verify.military_ledger().open then ok("re-pressing the active tab closes the ledger") else
    fail("the active tab re-press left the ledger open") end
