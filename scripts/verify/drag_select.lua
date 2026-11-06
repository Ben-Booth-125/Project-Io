-- Verify drag-select of units (BL-1508, drag select units; SELECTION.md § The dragged group).
--
-- Real drags through the input path: a band over own counters across tiles gathers them,
-- Shift adds, a right-click sends every unit of the group, a band over nothing of the
-- player's leaves the press's selection, and a rival's units never join. Aiming uses the
-- counters' own drawn points (verify.unit_counters): no tile data.

verify.window(1920, 1080)
verify.goto_surface("home")

local mine = {}
for _, u in ipairs(verify.units()) do
    if u.col ~= nil and u.count > 0 and u.player then mine[#mine + 1] = u end
end
assert(#mine >= 2, "the player needs two units on the surface")

-- Frame the nearest pair: a wider view bakes far more ground, and the run has a budget.
local pa, pb, best = nil, nil, nil
for _, p in ipairs(mine) do
    for _, u in ipairs(mine) do
        if u.col ~= p.col or u.row ~= p.row then
            local d = math.abs(u.col - p.col) + math.abs(u.row - p.row)
            if best == nil or d < best then best, pa, pb = d, p, u end
        end
    end
end
assert(pb, "every own unit stands on one tile; a band across tiles cannot be tested")
print("drag_select: nearest pair is " .. best .. " tiles apart")
verify.center_tile((pa.col + pb.col) // 2, (pa.row + pb.row) // 2, 5.0)
verify.frames(3)

local function has(list, id)
    for _, v in ipairs(list) do if v == id then return true end end
    return false
end

-- Own counters in view, by their drawn points, inside the canvas's clear middle.
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
-- Widen only as far as the pair needs.
local cs = own_counters()
local zoom = 5.0
for _, z in ipairs({ 4.0, 3.2 }) do
    if #cs >= 2 then break end
    zoom = z
    verify.center_tile((pa.col + pb.col) // 2, (pa.row + pb.row) // 2, z)
    verify.frames(3)
    cs = own_counters()
end
for _, c in ipairs(verify.unit_counters()) do
    print("drag_select: counter " .. c.id .. " at " .. math.floor(c.x) .. "," .. math.floor(c.y) .. (c.player and " own" or " rival"))
end
print("drag_select: " .. #cs .. " own counter(s) in view")
assert(#cs >= 2, "two own counters are not in view at this framing")
local a, b = cs[1], cs[2]
-- The units a counter stands for include its target; the target's id is what we assert.

-- 1. A band around both gathers both.
local pad = 18
local x0, x1 = math.min(a.x, b.x) - pad, math.max(a.x, b.x) + pad
local y0, y1 = math.min(a.y, b.y) - pad, math.max(a.y, b.y) + pad
verify.capture("drag_select_before")
verify.drag(x0, y0, x1, y1)
verify.frames(2)
local g = verify.selected_units()
print("drag_select: the band gathered " .. #g .. " unit(s)")
verify.expect(has(g, a.id) and has(g, b.id), "the band gathered both own units, across tiles")
for _, id in ipairs(g) do
    for _, u in ipairs(verify.units()) do
        if u.id == id then verify.expect(u.player, "only the player's units are gathered") end
    end
end
verify.capture("drag_select_group")
verify.expect_no_clipping("group band")

-- 2. A plain press on open canvas retires it.
verify.click(x0 - 60, y0 - 60)
verify.frames(2)
verify.expect(#verify.selected_units() == 0, "a plain press retired the group")

-- 3. A narrow band around one, then Shift adds the other.
verify.drag(a.x - 14, a.y - 12, a.x + 14, a.y + 12)
verify.frames(2)
print("drag_select: narrow band left " .. #verify.selected_units() .. " in the group list")
verify.drag(b.x - 14, b.y - 12, b.x + 14, b.y + 12, true)
verify.frames(2)
g = verify.selected_units()
print("drag_select: after the shift band " .. #g)
verify.expect(has(g, a.id) and has(g, b.id), "Shift added the second counter to the group")

-- 4. A right-click sends every unit of the group. (verify.tile_screen centres the view.)
local target = nil
for r = 4, 2, -1 do
    for _, d in ipairs({ { r, 0 }, { -r, 0 }, { 0, r }, { 0, -r } }) do
        local p = verify.tile_screen(pa.col + d[1], pa.row + d[2])
        if p.ok then
            verify.hover(p.x, p.y, 3)
            if verify.pointer_target().unit_order == "move" then target = p; break end
        end
    end
    if target then break end
end
verify.expect(target ~= nil, "a tile near the group previews a move")
if target then
    verify.click(target.x, target.y, "right")
    verify.frames(3)
    local marching = 0
    for _, id in ipairs(g) do
        for _, u in ipairs(verify.units()) do
            if u.id == id and u.marching then marching = marching + 1 end
        end
    end
    print("drag_select: " .. marching .. " of " .. #g .. " marching; status '" .. verify.pointer_target().status_message .. "'")
    verify.expect(marching == #g, "the right-click ordered every unit of the group")
    verify.capture("drag_select_ordered")
end

-- 5. A band over a rival's counter gathers nothing.
verify.click(40, 40)
verify.frames(2)
local rc = nil
for _, c in ipairs(verify.unit_counters()) do
    if not c.player and c.x > 100 and c.x < 1500 and c.y > 110 and c.y < 780 then rc = c; break end
end
if rc then
    verify.drag(rc.x - 14, rc.y - 12, rc.x + 14, rc.y + 12)
    verify.frames(2)
    verify.expect(#verify.selected_units() == 0, "a rival's units are not gathered")
else
    print("drag_select: no rival counter in view; the rival band was not taken")
end

-- 6. Under a lens a drag gathers nothing. The band is aimed before the lens opens, at the
-- pair the first band gathered, so a band that gathers nothing is the lens's doing.
verify.center_tile((pa.col + pb.col) // 2, (pa.row + pb.row) // 2, zoom)
verify.frames(3)
cs = own_counters()
verify.expect(#cs >= 2, "the pair is back in view for the lens band")
if #cs >= 2 then
    local p, q = cs[1], cs[2]
    verify.set_overlay("nations")
    verify.frames(2)
    verify.drag(math.min(p.x, q.x) - 18, math.min(p.y, q.y) - 18, math.max(p.x, q.x) + 18, math.max(p.y, q.y) + 18)
    verify.frames(2)
    verify.expect(#verify.selected_units() == 0, "under a lens a drag gathers nothing")
end
verify.set_overlay("none")
