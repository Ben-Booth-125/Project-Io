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

-- 1. Press rail slot 8.
local sx, sy = verify.nav_slot(8)
assert(sx, "rail slot 8 was not drawn")
click(sx, sy)
assert(open_panel() == "diplomacy", "slot 8 opened '" .. tostring(open_panel()) .. "', not diplomacy")
ok("slot 8 opens the Diplomacy ledger")
local d = led()
assert(d.view == 0, "the ledger did not open on Nations")
assert(#d.rows > 0, "Nations drew no rows")
print("diplomacy_ledger: " .. #d.rows .. " nation rows; first " .. d.rows[1].nation
      .. " access " .. string.format("%.2f", d.rows[1].access)
      .. " trust " .. string.format("%.2f", d.rows[1].trust)
      .. " alarm " .. string.format("%.2f", d.rows[1].alarm))
verify.capture("diplomacy_nations_folded")

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

-- 2. Expand the target's row in place.
local r = row_of(target)
click(r.x, r.y)
assert(led().expanded == target, "the expander did not expand the row")
ok("the row expands in place")
local emb = press_of("embargo", target)
assert(emb ~= nil and emb.enabled, "the Embargo press is not drawn live")
local friend = press_of("offer_friendship", target) or press_of("withdraw", target)
               or press_of("accept_friendship", target)
assert(friend ~= nil, "no friendship press drawn")
local decl = press_of("declare_open", target)
assert(decl ~= nil, "no Declare war press drawn")
print("diplomacy_ledger: friendship press '" .. friend.kind .. "', declare enabled=" .. tostring(decl.enabled)
      .. (decl.enabled and "" or (" (" .. decl.reason .. ")")))
verify.capture("diplomacy_nations_expanded")

-- Re-pressing the expander folds it (a toggle), then open it again. The row may
-- have scrolled to the column top when it opened, so read its point afresh.
r = row_of(target)
click(r.x, r.y)
assert(led().expanded == 0, "re-pressing the expander did not fold the row")
click(row_of(target).x, row_of(target).y)
ok("the expander is a toggle")

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
