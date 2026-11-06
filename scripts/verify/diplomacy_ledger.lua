-- Verify the Diplomacy ledger (rail slot 8; docs/ui/ledgers/diplomacy.md).
--
-- 1. Slot 8 is Diplomacy: pressing it opens the ledger on Nations, every nation
--    a folded row with stance chips and Access / Trust / Alarm toward the player.
-- 2. Expanding a row in place: the reverse sentiment, power and front strength,
--    trade exposure, friendship, the Embargo / Lift toggle (enact, then lift)
--    and Declare war.
-- 3. The declaration: Declare war turns the row into it, the canvas marks the
--    target's provinces, a click on one chooses it as a goal, and the
--    justifications and price follow. Escape leaves no trace.
-- 4. Declare through the real press: the war opens, the row shows its score.
-- 5. Wars: every war in the world (a second, between two rivals, is staged
--    through the corp-command seam so the list is more than the player's own),
--    with score, exhaustion and goals; the peace composer opens on the player's.
-- 6. The toggle rule: re-pressing the active tab closes the ledger.
--
-- Every press aims at the point the ledger published (verify.diplomacy_ledger).
-- The goal click aims at a rival unit's tile, read off verify.units() — the
-- stack the player sees on the canvas — and the rival-war staging names a
-- province by the id the hover already reports (pointer_target().hovered_province).

-- 1920x1080, as ledger_pass: a design review of an expanded row and its
-- declaration, which the 1280x720 floor's short column scrolls.
verify.window(1920, 1080)
verify.goto_surface("home")
verify.clear_selection()
verify.set_overlay("none")
verify.mouse(-1, -1)
verify.frames(3)

local function fail(msg) print("diplomacy_ledger: FAIL " .. msg) end
local function ok(msg)   print("diplomacy_ledger: PASS " .. msg) end
local function open_panel() return verify.pointer_target().open_panel end
local function led() return verify.diplomacy_ledger() end
local function press_of(kind, nation)
    for _, p in ipairs(led().presses) do
        if p.kind == kind and (nation == nil or p.nation == nation) then return p end
    end
    return nil
end
local function row_of(nation)
    for _, r in ipairs(led().rows) do if r.nation == nation then return r end end
    return nil
end
local function click(x, y)
    verify.hover(x, y, 2)
    verify.click(x, y)
    verify.mouse(-1, -1)
    verify.frames(3)
end
-- BL-1511: the selector, by real presses: open the dropdown, press the nation's item.
local function choose(nation)
    local dl = led()
    click(dl.combo_x, dl.combo_y)
    verify.frames(2)
    for _, it in ipairs(led().combo_items) do
        if it.nation == nation then
            click(it.x, it.y)
            return
        end
    end
    error("the selector does not list nation " .. tostring(nation))
end

-- 1. Press rail slot 8.
local sx, sy = verify.nav_slot(8)
assert(sx, "rail slot 8 was not drawn")
click(sx, sy)
assert(open_panel() == "diplomacy", "slot 8 opened '" .. tostring(open_panel()) .. "', not diplomacy")
ok("slot 8 opens the Diplomacy ledger")
local d = led()
-- BL-1511 (diplomacy.md § 3): opening the Nations view arms the Nations lens.
assert(verify.overlay_name() == "nations", "the Nations view did not arm the Nations lens: " .. tostring(verify.overlay_name()))
ok("slot 8 arms the Nations lens")
assert(d.combo_x >= 0, "the Nations view drew no selector")
assert(d.view == 0, "the ledger did not open on Nations")
assert(#d.rows > 0, "Nations drew no rows")
print("diplomacy_ledger: " .. #d.rows .. " nation rows; first " .. d.rows[1].nation
      .. " access " .. string.format("%.2f", d.rows[1].access)
      .. " trust " .. string.format("%.2f", d.rows[1].trust)
      .. " alarm " .. string.format("%.2f", d.rows[1].alarm))
verify.capture("diplomacy_nations_selector")

-- BL-1470: hovering a sentiment bar names its largest cause. Aim at the first row
-- whose Access toward the player is not zero and hover its Access bar.
local caused = nil
for _, row in ipairs(d.rows) do
    if row.access ~= 0 and row.access_bar_x and row.access_bar_x >= 0 then caused = row; break end
end
if caused then
    print("diplomacy_ledger: nation " .. caused.nation .. " access "
          .. string.format("%.2f", caused.access) .. " cause '" .. tostring(caused.access_cause) .. "'")
    if caused.access_cause ~= nil and caused.access_cause ~= "" then
        ok("a non-zero Access bar names its largest cause")
    else
        fail("a non-zero Access bar names no cause")
    end
    verify.hover(caused.access_bar_x, caused.access_bar_y, 3)
    verify.frames(3)
    verify.capture("diplomacy_bar_hover_cause")
    verify.mouse(-1, -1)
    verify.frames(2)
else
    print("diplomacy_ledger: no row with a non-zero Access toward the player; the cause hover is not reached")
end

-- The target: a rival whose stack stands on the surface, so a goal can be clicked.
local theirs = nil
for _, u in ipairs(verify.units()) do
    if u.col ~= nil and not u.player and u.count > 0 and row_of(u.owner) ~= nil then
        theirs = u
        break
    end
end
local target = theirs and theirs.owner or d.rows[1].nation
print("diplomacy_ledger: target nation " .. target .. (theirs and (" (stack at " .. theirs.col .. "," .. theirs.row .. ")") or ""))

-- 2. Choose the target in the selector.
choose(target)
assert(led().expanded == target, "the selector did not choose the nation")
ok("the selector chooses the nation")
local emb = press_of("embargo", target)
assert(emb ~= nil and emb.enabled, "the Embargo press is not drawn live")
local friend = press_of("offer_friendship", target) or press_of("withdraw", target)
               or press_of("accept_friendship", target)
assert(friend ~= nil, "no friendship press drawn")
local decl = press_of("declare_open", target)
assert(decl ~= nil, "no Declare war press drawn")
print("diplomacy_ledger: friendship press '" .. friend.kind .. "', declare enabled=" .. tostring(decl.enabled)
      .. (decl.enabled and "" or (" (" .. decl.reason .. ")")))
verify.capture("diplomacy_nations_chosen")

-- A press on a nation under the Nations lens navigates here (LENSES.md § The Nations
-- lens), and from a closed ledger opens it. Aim at the rival stack's own ground.
if theirs then
    verify.center_tile(theirs.col, theirs.row, 6.0)
    verify.frames(3)
    local other = nil
    for _, row in ipairs(led().rows) do
        if row.nation ~= theirs.owner then other = row.nation; break end
    end
    if other then choose(other) end
    local gp = verify.tile_screen(theirs.col, theirs.row)
    click(gp.x, gp.y)
    assert(led().expanded == theirs.owner, "a press under the lens did not choose the nation")
    ok("a press under the Nations lens chooses the nation")
    local rx, ry = verify.nav_slot(8)
    click(rx, ry)
    assert(open_panel() ~= "diplomacy", "re-pressing slot 8 did not close the ledger")
    assert(verify.overlay_name() ~= "nations", "closing the ledger did not disarm the Nations lens")
    ok("closing the ledger disarms the lens")
    verify.set_overlay("nations")
    verify.frames(2)
    gp = verify.tile_screen(theirs.col, theirs.row)
    click(gp.x, gp.y)
    assert(open_panel() == "diplomacy", "a press under the lens did not open the closed ledger")
    ok("a press under the Nations lens opens the closed ledger")
    verify.capture("diplomacy_lens_press_opened")
    if led().expanded ~= target then choose(target) end
end

-- Embargo / Lift is a toggle: Embargo enacts and the press reads Lift; Lift
-- repeals and it reads Embargo again. Both through set_embargo.
emb = press_of("embargo", target)
click(emb.x, emb.y)
verify.frames(2)
local lift = press_of("lift", target)
print("diplomacy_ledger: after Embargo message='" .. led().message .. "' lift drawn=" .. tostring(lift ~= nil))
assert(lift ~= nil, "after Embargo the press does not read Lift")
verify.capture("diplomacy_embargo_in_force")
click(lift.x, lift.y)
verify.frames(2)
print("diplomacy_ledger: after Lift message='" .. led().message .. "'")
assert(press_of("embargo", target) ~= nil and press_of("lift", target) == nil,
       "Lift did not restore the Embargo press")
ok("Embargo / Lift toggles through set_embargo")

-- 3. The declaration.
decl = press_of("declare_open", target)
assert(decl and decl.enabled, "Declare war is not live: " .. tostring(decl and decl.reason))
click(decl.x, decl.y)
d = led()
assert(d.declaring == target, "Declare war did not open the declaration")
assert(d.candidates > 0, "the canvas marks none of the target's provinces")
print("diplomacy_ledger: declaration open, " .. d.candidates .. " provinces marked")
local dp = press_of("declare", target)
assert(dp and not dp.enabled, "Declare is live with no goal chosen")

-- Escape leaves no trace.
verify.press_escape()
verify.frames(2)
assert(led().declaring == 0 and led().goals == 0, "Escape did not close the declaration")
local wars0 = led().wars
ok("Escape closes the declaration, nothing sent (wars " .. wars0 .. ")")

-- Leaving the declaration's tab closes it: an unseen declaration must not keep
-- the canvas (it suppresses right-click orders and eats left-clicks as goals).
-- Open it, press Wars, then the canvas's right-click order works again.
click(press_of("declare_open", target).x, press_of("declare_open", target).y)
assert(led().declaring == target, "Declare war did not reopen the declaration")
click(led().tab_wars_x, led().tab_wars_y)
assert(led().view == 1, "the Wars tab did not open")
assert(led().declaring == 0, "switching to Wars left the declaration open, unseen")
ok("a tab change closes the declaration")
local mine = nil
for _, u in ipairs(verify.units()) do
    if u.col ~= nil and u.player and u.count > 0 then mine = u break end
end
if mine ~= nil and theirs ~= nil then
    assert(verify.select_unit(mine.id), "select_unit refused")
    verify.center_tile(theirs.col, theirs.row, 8.0)
    verify.frames(2)
    local p = verify.tile_screen(theirs.col, theirs.row)
    verify.hover(p.x, p.y, 2)
    local order = verify.pointer_target().unit_order
    print("diplomacy_ledger: after the tab change the stack previews '" .. order .. "'")
    assert(order == "declare", "the canvas's unit-order preview did not come back")
    verify.click(p.x, p.y, "right")
    verify.frames(3)
    assert(verify.pointer_target().order_confirm, "the right-click order did not work after the tab change")
    ok("the right-click order works again (it opens the declaration)")
    verify.press_escape()
    verify.frames(2)
    assert(led().declaring == 0, "Escape did not close the right-click's declaration")
    verify.clear_selection()
    verify.mouse(-1, -1)
    verify.frames(2)
end
local d0 = led()
if d0.view ~= 0 then click(d0.tab_nations_x, d0.tab_nations_y) end
if led().expanded ~= target then choose(target) end

-- Reopen and choose a goal on the canvas: the rival stack's province.
click(press_of("declare_open", target).x, press_of("declare_open", target).y)
if theirs ~= nil then
    verify.center_tile(theirs.col, theirs.row, 8.0)
    verify.frames(2)
    local p = verify.tile_screen(theirs.col, theirs.row)
    verify.hover(p.x, p.y, 2)
    verify.click(p.x, p.y)
    verify.frames(3)
    if led().goals == 0 then
        print("diplomacy_ledger: the stack's tile is not on the target's ground; no goal from it")
    end
end
d = led()
print("diplomacy_ledger: goals " .. d.goals .. ", price valid=" .. tostring(d.price_valid)
      .. " trust " .. string.format("%.2f", d.price_trust) .. " alarm " .. string.format("%.2f", d.price_alarm)
      .. " over " .. d.observers .. " observers, justified=" .. tostring(d.justified))
if d.goals >= 1 then ok("a canvas click chose a goal") else fail("no goal chosen on the canvas") end
if theirs ~= nil then
    local p = verify.tile_screen(theirs.col, theirs.row)
    verify.hover(p.x + 1, p.y + 1, 2) -- the pick's hover line in the capture
end
verify.capture("diplomacy_declaration")
verify.mouse(-1, -1)

-- 4. Declare through the real press.
dp = press_of("declare", target)
assert(dp ~= nil, "no Declare press")
if dp.enabled then
    click(dp.x, dp.y)
    verify.frames(2)
    d = led()
    print("diplomacy_ledger: after Declare wars=" .. d.wars .. " message='" .. d.message .. "'")
    assert(d.wars == wars0 + 1, "Declare did not open a war")
    assert(row_of(target).at_war, "the row does not read At war")
    ok("Declare opens the war through declare_war")
else
    fail("Declare not live: " .. dp.reason)
end
verify.capture("diplomacy_nations_at_war")

-- 5. A second war, between two rivals, staged through the seam: so Wars lists
--    more than the player's own. The defender's province is the id the hover
--    reports over one of its stacks.
local a, b = nil, nil
for _, u in ipairs(verify.units()) do
    if u.col ~= nil and not u.player and u.owner ~= target then
        if a == nil then a = u elseif u.owner ~= a.owner and b == nil then b = u end
    end
end
if a ~= nil and b ~= nil then
    verify.center_tile(b.col, b.row, 8.0)
    verify.frames(2)
    local p = verify.tile_screen(b.col, b.row)
    verify.hover(p.x, p.y, 2)
    local pid = verify.pointer_target().hovered_province
    verify.mouse(-1, -1)
    -- corp_verb::declare_war is 47 (append-only enum; corp_command.hpp).
    local res = verify.corp_command({verb = 47, corp = a.owner, counterparty = b.owner, provinces = {pid}})
    print("diplomacy_ledger: staged rival war " .. a.owner .. " on " .. b.owner .. " goal " .. pid .. ": " .. res)
end
verify.goto_surface("home")
verify.frames(2)

local d1 = led()
click(d1.tab_wars_x, d1.tab_wars_y)
d = led()
assert(d.view == 1, "the Wars tab did not open")
print("diplomacy_ledger: Wars rows " .. d.war_rows .. " of " .. d.wars .. " wars, truces " .. d.truce_rows)
assert(d.war_rows == d.wars, "Wars does not list every war")
ok("Wars lists every war")
verify.capture("diplomacy_wars")

local po = press_of("peace_open", target)
if po ~= nil then
    click(po.x, po.y)
    assert(led().peace_with == target, "Offer peace did not open the composer")
    local w = press_of("offer_white", target) or press_of("impose_white", target)
    assert(w ~= nil, "the composer drew no white-peace press")
    ok("the peace composer opens")
    verify.capture("diplomacy_wars_peace")
else
    fail("no Offer peace press on the player's war")
end

-- 6. The toggle rule: re-pressing the active tab closes the ledger.
d = led()
click(d.tab_wars_x, d.tab_wars_y)
assert(open_panel() ~= "diplomacy", "re-pressing the active tab did not close the ledger")
ok("re-pressing the active tab closes the ledger")
print("diplomacy_ledger: PASS")
