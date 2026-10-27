-- Verify the canvas half of fronts (BL-1427 R5; MILITARY.md § Fronts,
-- SELECTION.md § The front element).
--
-- The player's fronts draw as a line along each span's shared hex edges, in the
-- colour of the player's stance toward that neighbour, with the assigned count on
-- a small plate. A press on a line selects the front, and the Selection band shows
-- the front card: neighbour, span length, the assigned units, Hold and Push.
--
-- Aiming: no verify call names where a front is (a new Lua exposure would be
-- needed for that, and none is added). The script frames the player's capital and
-- walks the pointer outward along eight rays until the hover pass reports a front
-- line under it (pointer_target().hovered_front — UI state, a boolean). On a
-- world whose nation has no land border, or before fronts are derived, nothing is
-- found and the script says so rather than asserting.

verify.goto_surface("home")
verify.set_overlay("none")

local cap = verify.capital_stars()
assert(cap.has_capital, "the player nation names no capital tile")

-- 1. The border at play zoom, and close up.
verify.clear_selection()
verify.mouse(-1, -1)
verify.center_tile(cap.x, cap.y, 3.0)
verify.frames(2)
local pt = verify.pointer_target()
print("fronts: fronts drawn at zoom 3 = " .. tostring(pt.fronts_drawn))
verify.capture("fronts_play_zoom")

verify.center_tile(cap.x, cap.y, 5.0)
verify.frames(2)
verify.capture("fronts_border")

-- 2. Find a front line: rays out from the capital's screen point.
local function find_front()
    local t = verify.tile_screen(cap.x, cap.y)
    if not t.ok then return nil end
    local dirs = { {1,0}, {-1,0}, {0,1}, {0,-1}, {0.7,0.7}, {-0.7,0.7}, {0.7,-0.7}, {-0.7,-0.7} }
    for _, d in ipairs(dirs) do
        for step = 0, 420, 2 do
            local x, y = t.x + d[1] * step, t.y + d[2] * step
            verify.hover(x, y, 1)
            if verify.pointer_target().hovered_front then
                return { x = x, y = y }
            end
        end
    end
    return nil
end

if (verify.pointer_target().fronts_drawn or 0) == 0 then
    print("fronts: NONE drawn - derive_fronts returned no front for the player nation "
          .. "(the world lane's derivation, or a nation with no land border)")
    return
end

local at = find_front()
if at == nil then
    print("fronts: FAIL fronts are drawn but no ray from the capital reached a front line")
    return
end

-- 3. The hovered line: brighter, wider, and the hover read naming it.
verify.hover(at.x, at.y, 3)
verify.capture("fronts_hover")

-- 4. A press on it selects the front and opens the card.
verify.click(at.x, at.y)
verify.mouse(-1, -1)
verify.frames(2)
pt = verify.pointer_target()
print("fronts: after the press selection_kind=" .. pt.selection_kind)
assert(pt.selection_kind == "front", "a press on a front line did not select the front")
shot("fronts_card")

-- 5. Assign a unit from its card: the unit card's sixth cell opens the menu.
--    (A live click: the popup is ImGui; the capture shows the cell.)
local mine = nil
for _, u in ipairs(verify.units()) do
    if u.col ~= nil and u.player then mine = u; break end
end
if mine ~= nil then
    assert(verify.select_unit(mine.id), "select_unit refused the player's own unit")
    verify.mouse(-1, -1)
    shot("fronts_unit_card")
else
    print("fronts: the player has no unit on the surface; the unit card capture not taken")
end
