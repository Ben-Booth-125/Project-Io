-- Verify the Military ledger (rail slot 7; docs/ui/ledgers/military.md).
--
-- 1. Slot 7 is Military: pressing it opens the ledger, its glyph lit.
-- 2. Recruit with no base: "No military base" and the press onto the
--    Construction ledger's site finder, pre-set to the military base.
-- 3. Recruit with a base chosen: the picker, the roster (dimmed rows say why),
--    each row's price and Raise press, and the muster queue's empty state.
-- 3b. BL-1463: each theatre is headed by its recruit pool strip (held / cap,
--     the quarter's output), checked against the world (verify.recruit_pools);
--     its tooltip captured.
-- 4. A Raise press through the real control: the hire seam's answer is shown.
--    The pool opens empty, so the Raise first quotes the strip's held figure;
--    one hire's recruits are staged (verify.grant_recruits) and the hire spends them.
-- 5. The Design tab: one row per land class.
-- 6. The toggle rule: re-pressing the active tab closes the ledger, and an
--    open unit designer with it.
-- 7. The rail icon's close also closes the designer.
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

-- 3b. BL-1463 (MILITARY.md § Recruits): each theatre is headed by its recruit pool -
--     held, the cap, the quarter's output - and the strip reads what the world holds:
--     the nation's pool, its running bases x the authored rate, fifteen quarters of it.
local function check_pool(led_now, theatre)
    local p = led_now.pools and led_now.pools[theatre]
    local wp = verify.recruit_pools()[theatre]
    if not p then fail(theatre .. " recruit pool strip was not drawn"); return nil end
    local want_out = wp.bases * wp.rate
    local want_cap = wp.cap_quarters * want_out
    print(string.format("military_ledger: %s pool strip %d / %d +%d a quarter; world held %d, %d base(s) x %d",
                        theatre, p.held, p.cap, p.output, wp.held, wp.bases, wp.rate))
    if p.held == wp.held and p.output == want_out and p.cap == want_cap then
        ok(theatre .. " pool strip matches the world (held, cap, output)") else
        fail(string.format("%s pool strip %d/%d +%d, world wants %d/%d +%d", theatre,
                           p.held, p.cap, p.output, wp.held, want_cap, want_out)) end
    return p
end
local land_pool = check_pool(led, "land")
check_pool(led, "air")
if land_pool and land_pool.output > 0 then ok("a running military base feeds the land pool") else
    fail("the land pool strip shows no output with a base standing") end
-- Its explanation is the strip's tooltip: hover it and capture.
if land_pool then
    verify.hover(land_pool.x, land_pool.y, 3) -- through the real input path, as stat_grid.lua does
    verify.capture("military_ledger_pool_tooltip")
    verify.mouse(-1, -1)
    verify.frames(2)
end

-- 4. A Raise press through the real control. Lend and stock as the fixtures do,
--    then press the first live row; repay after.
local LOAN = 5000.0
verify.set_balance(verify.player_balance() + LOAN)
verify.grant_stock("iron_ore", 50)
verify.grant_stock("agricultural_produce", 50)
verify.grant_stock("coal", 50)
verify.frames(2)
led = verify.military_ledger()
-- The pools open empty (bases make recruits once a quarter): a fieldable row's Raise
-- is then off with the seam's words, and they quote the strip's own held figure.
local held = led.pools.land and led.pools.land.held or -1
local quoted = nil
for _, r in ipairs(led.raises) do
    if r.fieldable and r.reason:find("recruits; the pool holds", 1, true) then quoted = r; break end
end
if held < 50 then
    if quoted and quoted.reason:find("the pool holds " .. held .. ".", 1, true) then
        ok("a Raise short of recruits quotes the strip's held figure (" .. quoted.reason .. ")") else
        fail("no Raise reason quotes the strip's held " .. held ..
             (quoted and (": '" .. quoted.reason .. "'") or "")) end
    -- Stage the recruits one hire spends (verify-only), and the strip follows.
    verify.grant_recruits("land", 50 - held)
    verify.frames(2)
    led = verify.military_ledger()
    check_pool(led, "land")
end
held = led.pools.land and led.pools.land.held or -1
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
    -- The hire spent its men: the strip drops by 50 and still reads the world.
    local after = check_pool(led, "land")
    if led.message ~= "Unit mustering." then
        fail("the Raise was refused ('" .. led.message .. "'), so no recruits were spent")
    elseif after and after.held == held - 50 then ok("the hire spent 50 recruits off the strip") else
        fail("the land strip did not drop by 50 after the hire (" .. held .. " -> " ..
             tostring(after and after.held) .. ")") end
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

-- 5b. A class row opens the designer, so step 6 can check the ledger's close
--     takes it too (military.md § The designer, Ben 2026-10-30).
local designer_opened = false
if led.classes and #led.classes > 0 then
    local c = led.classes[1]
    verify.click(c.x, c.y)
    verify.mouse(-1, -1)
    verify.frames(3)
    designer_opened = verify.unit_designer().open
    if designer_opened then ok("a Design class row opens the designer") else
        fail("the Design class row did not open the designer") end
    led = verify.military_ledger()
else
    fail("the Design tab drew no class rows")
end

-- 6. Re-pressing the active tab closes the ledger - and the designer with it.
verify.click(led.tab_design_x, led.tab_design_y)
verify.mouse(-1, -1)
verify.frames(3)
if not verify.military_ledger().open then ok("re-pressing the active tab closes the ledger") else
    fail("the active tab re-press left the ledger open") end
if designer_opened then
    if not verify.unit_designer().open then ok("closing the ledger by its tab closes the designer") else
        fail("the designer stayed open after the ledger closed") end
end

-- 7. The rail icon's close takes the designer too: reopen, open a class, press
--    slot 7 again.
verify.click(sx, sy)
verify.mouse(-1, -1)
verify.frames(3)
led = verify.military_ledger()
if led.open and led.view == 1 and led.classes and #led.classes > 0 then
    local c = led.classes[1]
    verify.click(c.x, c.y)
    verify.mouse(-1, -1)
    verify.frames(3)
    if verify.unit_designer().open then
        verify.click(sx, sy)
        verify.mouse(-1, -1)
        verify.frames(3)
        if not verify.military_ledger().open and not verify.unit_designer().open then
            ok("closing the ledger by its rail icon closes the designer") else
            fail("the rail close left the ledger or the designer open") end
    else
        fail("the class row did not reopen the designer")
    end
else
    fail("the rail press did not reopen the ledger on Design")
end
