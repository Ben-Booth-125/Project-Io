-- Verify right-click unit orders (BL-1443, tile contact orders; R5 of the
-- `tile-contact-orders` requirement; SELECTION.md § Right-click orders).
--
-- With the player's own unit selected: hovering a plain land tile previews a
-- MOVE (green outline, "Right-click: move"); hovering another nation's stack
-- previews an ATTACK (red, the battle glyph) or, toward a nation the player is
-- not hostile to, DECLARE AND ATTACK (orange) — and a right-click there opens the
-- declare confirm instead of sending anything. A right-click on a plain tile
-- sends the march, and the unit then holds a movement order.
--
-- Aiming uses only UI state: the preview's own word (pointer_target().unit_order)
-- and the hovered province id, both already exposed. No tile data is read: the
-- plain tile is found by stepping the pointer over the unit's neighbours until
-- the preview says "move".

verify.goto_surface("home")

local mine, theirs = nil, nil
for _, u in ipairs(verify.units()) do
    if u.col ~= nil and u.count > 0 then
        if u.player then
            if mine == nil then mine = u end
        elseif theirs == nil then
            theirs = u
        end
    end
end
assert(mine, "the player has no unit on the surface (every nation seeds a garrison)")

local function unit_by_id(id)
    for _, u in ipairs(verify.units()) do
        if u.id == id then return u end
    end
    return nil
end

local function hover_tile_at(col, row)
    local p = verify.tile_screen(col, row)
    if not p.ok then return nil end
    verify.hover(p.x, p.y, 3)
    return p
end

-- 1. Select the player's unit; nothing hovered yet.
verify.center_tile(mine.col, mine.row, 10.0)
assert(verify.select_unit(mine.id), "select_unit refused the player's own unit")
local home = hover_tile_at(mine.col, mine.row)
local home_prov = verify.pointer_target().hovered_province
print("unit_orders: own tile preview = " .. verify.pointer_target().unit_order
      .. " (expect none: already there)")

-- 2. A plain land tile: the move preview. Prefer one in ANOTHER province, since
--    on a base whose march_unit still reads `province`, a same-province march is
--    refused as "already there".
local near, far = nil, nil
for r = 1, 8 do
    for _, d in ipairs({ { r, 0 }, { -r, 0 }, { 0, r }, { 0, -r }, { r, r }, { -r, -r } }) do
        local c, w = mine.col + d[1], mine.row + d[2]
        if hover_tile_at(c, w) then
            local pt = verify.pointer_target()
            if pt.unit_order == "move" then
                if pt.hovered_province ~= home_prov then
                    if far == nil then far = { col = c, row = w } end
                elseif near == nil then
                    near = { col = c, row = w }
                end
            end
        end
        if far and near then break end
    end
    if far and near then break end
end
assert(far or near, "no tile near the player's unit previews a move")
local target = far or near
verify.center_tile(target.col, target.row, 10.0)
local tp = hover_tile_at(target.col, target.row)
local pt = verify.pointer_target()
print("unit_orders: move target " .. target.col .. "," .. target.row
      .. " preview=" .. pt.unit_order .. " other_province=" .. tostring(far ~= nil))
assert(pt.unit_order == "move", "the plain tile did not preview a move")
verify.capture("unit_orders_move_hover")

-- 3. A foreign stack: attack or declare.
if theirs ~= nil then
    verify.center_tile(theirs.col, theirs.row, 10.0)
    hover_tile_at(theirs.col, theirs.row)
    local fp = verify.pointer_target()
    print("unit_orders: foreign stack " .. theirs.col .. "," .. theirs.row
          .. " preview=" .. fp.unit_order)
    assert(fp.unit_order == "attack" or fp.unit_order == "declare",
           "another nation's stack did not preview an attack")
    verify.capture("unit_orders_foreign_hover")

    if fp.unit_order == "declare" then
        local p = verify.tile_screen(theirs.col, theirs.row)
        verify.click(p.x, p.y, "right")
        verify.frames(3)
        local cp = verify.pointer_target()
        print("unit_orders: after right-click on a non-hostile stack confirm=" .. tostring(cp.order_confirm))
        assert(cp.order_confirm, "the declare confirm did not open")
        verify.capture("unit_orders_declare_confirm")
        -- A left press elsewhere on the canvas closes it and sends nothing.
        verify.click(p.x - 220, p.y - 160) -- up-left of the stack: the confirm sits down-right
        verify.frames(2)
        assert(not verify.pointer_target().order_confirm, "a canvas press did not close the confirm")
        print("unit_orders: confirm closed by a canvas press; nothing sent")
    end
else
    print("unit_orders: no foreign unit on the surface; the attack preview was not taken")
end

-- 4. The right-click march.
assert(verify.select_unit(mine.id), "select_unit refused")
if near ~= nil and far ~= nil then
    -- The same-province case first, reported honestly.
    verify.center_tile(near.col, near.row, 10.0)
    local np = verify.tile_screen(near.col, near.row)
    verify.hover(np.x, np.y, 2)
    verify.click(np.x, np.y, "right")
    verify.frames(2)
    print("unit_orders: same-province right-click status='" .. verify.pointer_target().status_message
          .. "' marching=" .. tostring(unit_by_id(mine.id).marching))
    assert(verify.select_unit(mine.id), "select_unit refused")
end
verify.center_tile(target.col, target.row, 10.0)
tp = verify.tile_screen(target.col, target.row)
verify.hover(tp.x, tp.y, 2)
verify.click(tp.x, tp.y, "right")
verify.frames(2)
local after = unit_by_id(mine.id)
local st = verify.pointer_target().status_message
print("unit_orders: right-click march status='" .. st .. "' marching=" .. tostring(after.marching)
      .. " on_front=" .. tostring(after.on_front))
assert(after.marching, "the right-click did not give the unit a movement order")
verify.center_tile(mine.col, mine.row, 6.0)
verify.mouse(-1, -1)
verify.frames(2)
verify.capture("unit_orders_marching")

-- 5. Declare and attack, through the confirm's own button. The button stands at
--    a fixed offset from the right-click point (the confirm opens 12 px down-right
--    of the cursor; its first button is one text line below the window's top) —
--    measured from unit_orders_declare_confirm.png at this font.
if theirs ~= nil then
    assert(verify.select_unit(mine.id), "select_unit refused")
    verify.center_tile(theirs.col, theirs.row, 10.0)
    local p = verify.tile_screen(theirs.col, theirs.row)
    verify.hover(p.x, p.y, 2)
    local kind = verify.pointer_target().unit_order
    verify.click(p.x, p.y, "right")
    verify.frames(3)
    if kind == "declare" and verify.pointer_target().order_confirm then
        verify.hover(p.x + 89, p.y + 51, 2)
        verify.click(p.x + 89, p.y + 51)
        verify.frames(3)
    end
    local hov = (function() verify.hover(p.x, p.y, 2); return verify.pointer_target().unit_order end)()
    local u = unit_by_id(mine.id)
    print("unit_orders: declare-and-attack status='" .. verify.pointer_target().status_message
          .. "' marching=" .. tostring(u.marching) .. " preview now=" .. hov)
    assert(hov == "attack", "after declaring, the stack did not preview an attack")
    verify.capture("unit_orders_after_declare")
end
print("unit_orders: PASS")
