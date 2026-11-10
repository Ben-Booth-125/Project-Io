-- Verify a marching counter moves continuously when its stack changes under it (Ben,
-- 2026-11-09: "Convoys and units moving on the map seem to jitter around rather than
-- moving smoothly. Often this happens when stacking / unstacking"; MILITARY.md § Land -
-- on the canvas).
--
-- A counter that leaves its own side's stack opens ahead of it, so the two never sit one
-- on the other. That opening used to be a STEP: a third of a hop added the frame the
-- stack behind appeared and taken away the frame it went, so the counter leapt whenever
-- the stack it left changed. Here one unit of a stack of two marches, then the second
-- follows on the next day: the first counter's stack behind empties under it. Its drawn
-- point is sampled EVERY FRAME across that change, the camera fixed, and the largest
-- frame-to-frame step is reported against the largest step a whole day's march makes.
--
-- --verify freezes wall time, so a day step moves the counter by a day's walk at once:
-- that is the honest ceiling a frame may move here. A step on the stack change itself
-- larger than it is the jitter.
--
-- Run with: ProjectIo --verify scripts/verify/glide_continuity.lua

verify.goto_surface("home")

local function unit_by_id(id)
    for _, u in ipairs(verify.units()) do
        if u.id == id then return u end
    end
end

-- Two of the player's units on one tile: a stack to march out of. Where generation stood
-- none together, the nearest pair is brought together by a real march first.
local by_tile, first, second = {}, nil, nil
local mine = {}
for _, u in ipairs(verify.units()) do
    if u.player and u.col ~= nil and u.count > 0 and not u.marching then
        mine[#mine + 1] = u
        local key = u.col .. "," .. u.row
        if by_tile[key] ~= nil and first == nil then
            first, second = by_tile[key], u
        else
            by_tile[key] = by_tile[key] or u
        end
    end
end
if first == nil then
    local best = nil
    for i = 1, #mine do
        for j = i + 1, #mine do
            local d = math.abs(mine[i].col - mine[j].col) + math.abs(mine[i].row - mine[j].row)
            if best == nil or d < best then best, first, second = d, mine[i], mine[j] end
        end
    end
    assert(first and second, "the player has fewer than two units")
    print(string.format("glide_continuity: bringing unit %d (%d,%d) to unit %d (%d,%d), %d tiles",
                        second.id, second.col, second.row, first.id, first.col, first.row, best))
    verify.center_tile(first.col, first.row, 6.0)
    verify.frames(2)
    assert(verify.select_unit(second.id), "select_unit refused the player's own unit")
    verify.frames(2)
    local p = verify.tile_screen(first.col, first.row)
    verify.hover(p.x, p.y, 3)
    assert(verify.pointer_target().unit_order == "move", "the first unit's tile previews no move")
    verify.click(p.x, p.y, "right")
    verify.frames(2)
    for _ = 1, 120 do
        local u = unit_by_id(second.id)
        if u.col == first.col and u.row == first.row and not u.marching then break end
        verify.day_step(1)
    end
    local u = unit_by_id(second.id)
    assert(u.col == first.col and u.row == first.row, "the second unit never reached the first")
    second = u
    verify.clear_selection()
end
print(string.format("glide_continuity: stack of two at %d,%d (units %d, %d)",
                    first.col, first.row, first.id, second.id))

verify.center_tile(first.col, first.row, 10.0)
verify.frames(3)

local function glide_of(id)
    for _, g in ipairs(verify.unit_glides()) do
        for _, gid in ipairs(g.ids) do
            if gid == id then return g end
        end
    end
end

-- A tile that previews a move for the selected unit, some way off.
local function march(id)
    assert(verify.select_unit(id), "select_unit refused the player's own unit")
    verify.frames(2)
    for r = 6, 2, -1 do
        for _, d in ipairs({ { r, 0 }, { -r, 0 }, { 0, r }, { 0, -r } }) do
            local p = verify.tile_screen(first.col + d[1], first.row + d[2])
            if p.ok then
                verify.hover(p.x, p.y, 3)
                if verify.pointer_target().unit_order == "move" then
                    verify.click(p.x, p.y, "right")
                    verify.frames(1)
                    return true
                end
            end
        end
    end
    return false
end

local samples, max_frame, max_day, max_change = {}, 0.0, 0.0, 0.0
local last = nil
local function sample(tag)
    local g = glide_of(first.id)
    if g == nil or g.x < 0 then return end
    if last ~= nil then
        local step = math.sqrt((g.x - last.x) ^ 2 + (g.y - last.y) ^ 2)
        if tag == "day" then max_day = math.max(max_day, step)
        elseif tag == "change" then max_change = math.max(max_change, step)
        else max_frame = math.max(max_frame, step) end
        samples[#samples + 1] = string.format("%s %.1f", tag, step)
    end
    last = { x = g.x, y = g.y }
end

assert(march(first.id), "no tile near the stack previews a move")
assert(unit_by_id(first.id).marching, "the right-click did not send the march")
for _ = 1, 12 do verify.frames(1); sample("frame") end
verify.capture("glide_continuity_left_stack")

-- A day's walk: the ceiling one step may move here.
verify.day_step(1)
verify.frames(1); sample("day")
for _ = 1, 12 do verify.frames(1); sample("frame") end

-- The stack behind empties: the second unit marches too, while the first is mid-hop.
local still_on = unit_by_id(first.id)
local same_tile = still_on.col == first.col and still_on.row == first.row
print("glide_continuity: first still on the stack's tile: " .. tostring(same_tile))
if same_tile and march(second.id) then
    sample("change")
    for _ = 1, 16 do verify.frames(1); sample("change") end
    verify.capture("glide_continuity_stack_left")
end

local joined = ""
for i, v in ipairs(samples) do joined = joined .. (i > 1 and ", " or "") .. v end
print("glide_continuity: steps " .. joined)
print(string.format("glide_continuity: max day step %.1f px, max frame step %.1f px, "
                    .. "max step on the stack change %.1f px", max_day, max_frame, max_change))
verify.expect(same_tile, "the first unit was still on the stack's tile when the second left")
verify.expect(max_change <= math.max(max_day, 1.0),
              "the stack emptying under a marching counter moves it no more than a day's walk")
