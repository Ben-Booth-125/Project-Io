-- Verify army groups (BL-1737, army groups; docs/military/MILITARY.md § Army groups,
-- docs/ui/SELECTION.md § The army group element, docs/ui/ledgers/military.md § Groups).
--
-- Every step presses the real control at the point the surface published:
-- 1. A drag gathers two of the player's units across tiles; Form group makes the 1st Army
--    and the band shows its card.
-- 2. A row's tick box, then Split off: the ticked unit leaves as the 2nd Army, the band
--    shows it.
-- 3. The Military ledger's Groups tab lists both, in ordinal order; a row press shows
--    that group's card.
-- 4. Merge on the 1st Army's card, picking the 2nd: one group again, the 2nd ended.
-- 5. The row's pin pins the group; the band's pin toggle reads it pinned; the pinned
--    panel carries its card; the row's pin again unpins it.
-- Captures: the formed card, the Groups tab, the merge list, the pinned group.

verify.window(1920, 1080)
verify.goto_surface("home")
verify.clear_selection()
verify.set_overlay("none")
verify.frames(3)

local function say(msg) print("army_groups: " .. msg) end
local function has(list, id)
    for _, v in ipairs(list) do if v == id then return true end end
    return false
end

-- Frame the nearest pair of the player's units on different tiles (drag_select.lua's aim).
local mine = {}
for _, u in ipairs(verify.units()) do
    if u.col ~= nil and u.count > 0 and u.player then mine[#mine + 1] = u end
end
assert(#mine >= 2, "the player needs two units on the surface")
local pa, pb, best = nil, nil, nil
for _, p in ipairs(mine) do
    for _, u in ipairs(mine) do
        if u.col ~= p.col or u.row ~= p.row then
            local d = math.abs(u.col - p.col) + math.abs(u.row - p.row)
            if best == nil or d < best then best, pa, pb = d, p, u end
        end
    end
end
assert(pb, "every own unit stands on one tile")
local function own_counters()
    local out, seen = {}, {}
    for _, c in ipairs(verify.unit_counters()) do
        if c.player and not seen[c.id] and c.x > 100 and c.x < 1500 and c.y > 110 and c.y < 780 then
            seen[c.id] = true
            out[#out + 1] = c
        end
    end
    return out
end
local cs = {}
for _, z in ipairs({ 5.0, 4.0, 3.2 }) do
    verify.center_tile((pa.col + pb.col) // 2, (pa.row + pb.row) // 2, z)
    verify.frames(3)
    cs = own_counters()
    if #cs >= 2 then break end
end
assert(#cs >= 2, "two own counters are not in view")
local a, b = cs[1], cs[2]
local pad = 18
verify.drag(math.min(a.x, b.x) - pad, math.min(a.y, b.y) - pad,
            math.max(a.x, b.x) + pad, math.max(a.y, b.y) + pad)
verify.frames(3)
local gathered = verify.selected_units()
say("the band gathered " .. #gathered .. " unit(s)")
verify.expect(#gathered >= 2, "the drag gathered two or more of the player's units")

-- 1. Form group.
local ag = verify.army_groups()
verify.expect(#ag.groups == 0, "no army group before Form group")
verify.expect(ag.form_x >= 0, "the dragged group's card draws Form group")
verify.click(ag.form_x, ag.form_y)
verify.frames(3)
ag = verify.army_groups()
say("after Form group: " .. #ag.groups .. " group(s); line '" .. ag.message .. "'")
verify.expect(#ag.groups == 1 and ag.groups[1].name == "1st Army", "Form group made the 1st Army")
verify.expect(#ag.groups == 1 and #ag.groups[1].members == #gathered, "every gathered unit is in it")
verify.expect(#ag.groups == 1 and ag.shown == ag.groups[1].id, "the band shows the new group's card")
verify.expect(ag.form_x < 0 and ag.merge_x >= 0 and ag.split_x >= 0,
              "the army group card carries Merge and Split off, not Form group")
verify.expect(not ag.split_enabled, "Split off is disabled with nothing ticked")
verify.mouse(-1, -1)
verify.frames(2)
verify.capture("army_groups_01_card")
verify.expect_no_clipping("army group card")
local first = ag.groups[1].id

-- 2. Tick the last row, Split off.
verify.expect(#ag.ticks == #gathered, "one tick box a row")
local split_unit = ag.ticks[#ag.ticks].unit
verify.click(ag.ticks[#ag.ticks].x, ag.ticks[#ag.ticks].y)
verify.frames(2)
ag = verify.army_groups()
verify.expect(ag.split_enabled, "one row ticked enables Split off")
verify.click(ag.split_x, ag.split_y)
verify.frames(3)
ag = verify.army_groups()
say("after Split off: " .. #ag.groups .. " group(s); line '" .. ag.message .. "'")
verify.expect(#ag.groups == 2 and ag.groups[2].name == "2nd Army", "Split off made the 2nd Army")
verify.expect(#ag.groups == 2 and has(ag.groups[2].members, split_unit) and not has(ag.groups[1].members, split_unit),
              "the ticked unit moved; the others stayed")
local second = #ag.groups == 2 and ag.groups[2].id or nil
verify.expect(second ~= nil and ag.shown == second, "the band shows the split-off group")

-- 3. The Groups tab.
verify.show_panel("military", true)
verify.frames(3)
ag = verify.army_groups()
verify.expect(ag.tab_x >= 0, "the Military ledger draws the Groups tab")
verify.click(ag.tab_x, ag.tab_y)
verify.frames(3)
ag = verify.army_groups()
verify.expect(ag.military_view == 2, "the Groups tab is open")
verify.expect(#ag.ledger_rows == 2 and ag.ledger_rows[1].group == first and ag.ledger_rows[2].group == second,
              "the tab lists both groups, in ordinal order")
verify.click(ag.ledger_rows[1].x, ag.ledger_rows[1].y)
verify.frames(3)
ag = verify.army_groups()
verify.expect(ag.shown == first, "a row press shows that group's card")
verify.mouse(-1, -1)
verify.frames(2)
verify.capture("army_groups_02_ledger")

-- 4. Merge the 2nd into the 1st.
verify.click(ag.merge_x, ag.merge_y)
verify.frames(3)
ag = verify.army_groups()
verify.expect(#ag.merge_rows == 1 and ag.merge_rows[1].group == second, "Merge lists the other group")
verify.capture("army_groups_03_merge_list")
if #ag.merge_rows == 1 then
    verify.click(ag.merge_rows[1].x, ag.merge_rows[1].y)
    verify.frames(3)
end
ag = verify.army_groups()
say("after Merge: " .. #ag.groups .. " group(s); line '" .. ag.message .. "'")
verify.expect(#ag.groups == 1 and ag.groups[1].id == first and #ag.groups[1].members == #gathered,
              "Merge took the 2nd Army's units into the 1st; the 2nd ended")

-- 5. Pin from the ledger row.
ag = verify.army_groups()
verify.expect(#ag.ledger_rows == 1, "the tab lists one group after the merge")
if #ag.ledger_rows == 1 then
    verify.click(ag.ledger_rows[1].pin_x, ag.ledger_rows[1].pin_y)
    verify.frames(3)
end
ag = verify.army_groups()
local p = verify.pinned_panel()
verify.expect(ag.groups[1].pinned, "the row's pin pinned the group")
verify.expect(p.toggle_subject == first and p.toggle_on, "the band's pin toggle names the group, pinned")
local carded = false
for _, c in ipairs(p.cards) do if c.id == first then carded = true end end
verify.expect(carded, "the pinned panel carries the group's card")
verify.mouse(-1, -1)
verify.frames(2)
verify.capture("army_groups_04_pinned")
verify.click(ag.ledger_rows[1].pin_x, ag.ledger_rows[1].pin_y)
verify.frames(3)
ag = verify.army_groups()
verify.expect(not ag.groups[1].pinned, "the row's pin again unpinned it (a toggle)")
