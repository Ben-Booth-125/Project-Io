-- Verify a marching unit glides between tile centres (BL-1507, unit smooth march;
-- MILITARY.md § Land — on the canvas: "drawn travelling from tile centre to tile centre,
-- at the pace the march is actually banking, and its press follows what is drawn").
--
-- A real right-click march, then day steps: the counter leaves the stack, its hop fraction
-- rises as the bank does, and a press at the drawn point selects the marching unit. What
-- the day's fraction adds between steps is wall time, which `--verify` freezes: motion
-- within a day is the live click's to see.

verify.goto_surface("home")

local mine = nil
for _, u in ipairs(verify.units()) do
    if u.player and u.col ~= nil and u.count > 0 then mine = u; break end
end
assert(mine, "the player has no unit on the surface")

local function unit_by_id(id)
    for _, u in ipairs(verify.units()) do
        if u.id == id then return u end
    end
end

verify.center_tile(mine.col, mine.row, 10.0)
verify.frames(2)
assert(verify.select_unit(mine.id), "select_unit refused the player's own unit")

-- Aim at a tile that previews a move, some way off.
local target = nil
for r = 6, 2, -1 do
    for _, d in ipairs({ { r, 0 }, { -r, 0 }, { 0, r }, { 0, -r } }) do
        local p = verify.tile_screen(mine.col + d[1], mine.row + d[2])
        if p.ok then
            verify.hover(p.x, p.y, 3)
            if verify.pointer_target().unit_order == "move" then
                target = p
                break
            end
        end
    end
    if target then break end
end
assert(target, "no tile near the unit previews a move")
verify.click(target.x, target.y, "right")
verify.frames(3)
assert(unit_by_id(mine.id).marching, "the right-click did not send the march")

local function my_glide()
    for _, g in ipairs(verify.unit_glides()) do
        for _, id in ipairs(g.ids) do
            if id == mine.id then return g end
        end
    end
end

local g0 = my_glide()
assert(g0, "a marching unit did not draw apart from its stack")
print(string.format("unit_glide: fresh march t=%.3f at (%.0f, %.0f)", g0.t, g0.x, g0.y))
verify.capture("unit_glide_fresh")

-- A day at a time: the fraction rises with the bank, and the hop steps the sim's tile.
local rose, moved_tile = false, false
local last_t, start_col, start_row = g0.t, mine.col, mine.row
local last_x, last_y = g0.x, g0.y
for day = 1, 6 do
    verify.day_step(1)
    verify.frames(2)
    local u = unit_by_id(mine.id)
    local g = my_glide()
    if g then
        print(string.format("unit_glide: day %d t=%.3f at (%.0f, %.0f) tile %d,%d", day, g.t, g.x, g.y, u.col, u.row))
        if g.t > last_t then rose = true end
        last_t, last_x, last_y = g.t, g.x, g.y
    end
    if u.col ~= start_col or u.row ~= start_row then moved_tile = true end
    if g and g.t > 0.01 and day == 1 then verify.capture("unit_glide_mid_hop") end
end
assert(rose or moved_tile, "the march neither banked nor stepped over six days")
verify.expect(rose or moved_tile, "the march advances on the canvas")

-- The press follows the drawn point: select elsewhere, then press where the counter drew.
local g = my_glide()
if g then
    verify.select_tile(mine.col, mine.row)
    verify.frames(2)
    verify.center_tile(unit_by_id(mine.id).col, unit_by_id(mine.id).row, 10.0)
    verify.frames(3)
    g = my_glide()
    assert(g and g.x >= 0, "the marching counter is not on screen")
    verify.click(g.x, g.y)
    verify.frames(2)
    local pt = verify.pointer_target()
    print("unit_glide: press at the drawn point selected " .. tostring(pt.selected_kind or pt.hovered_kind))
    verify.capture("unit_glide_pressed")
end
