-- Verify a disabled Construct button says why on hover (Ben, 2026-11-09: "When clicking on a
-- tile that doesn't have a road, hovering the construction button should explain why it's
-- greyed out. The other buttons too actually, with tooltips to say what they do.";
-- docs/ui/SELECTION.md § The tile card's action grid).
--
-- The button's press point is published only while it is enabled (construction_controls'
-- construct_x/y), and the tile card's action grid does not move between tiles. So the aim is
-- read on a tile the site finder lists - a tile the door accepts - and then a tile beside the
-- player's own buildings that the door refuses is selected, and the SAME point hovered. The
-- tooltip's text is a capture read: no verify read reports it.
--
-- Run with: ProjectIo --verify scripts/verify/construct_tooltip.lua

verify.window(1720, 1080)
verify.goto_surface("home")
verify.set_balance(5000000.0)

-- The aim: the Construct button on a tile the site finder lists.
local aim = nil
do
    verify.show_panel("construction", true)
    verify.frames(2)
    for _, o in ipairs(verify.site_options()) do
        verify.construction_view(2)
        verify.site_pick(o.name)
        verify.frames(2)
        local s = verify.site_finder()
        if s.n_rows > 0 then
            verify.site_clear()
            verify.show_panel("construction", false)
            verify.frames(2)
            verify.center_tile(s.rows[1].x, s.rows[1].y, 6.0)
            verify.select_tile(s.rows[1].x, s.rows[1].y)
            verify.frames(3)
            local c = verify.construction_controls()
            if c.construct_ok then aim = { x = c.construct_x, y = c.construct_y } end
            break
        end
    end
    verify.site_clear()
end
assert(aim, "no tile the site finder lists showed an enabled Construct button")
print(string.format("construct_tooltip: Construct button at (%.0f, %.0f)", aim.x, aim.y))
verify.hover(aim.x, aim.y, 3)
verify.capture("construct_tooltip_enabled")

-- A refused tile: beside the player's own buildings, on ground the door turns down.
local refused = nil
for _, b in ipairs(verify.buildings()) do
    if b.player and refused == nil then
        for _, d in ipairs({ { 0, 0 }, { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 }, { 2, 0 }, { -2, 0 } }) do
            local x, y = b.x + d[1], b.y + d[2]
            verify.select_tile(x, y)
            verify.frames(2)
            if not verify.construction_controls().construct_ok then
                refused = { x = x, y = y }
                break
            end
        end
    end
end
verify.expect(refused ~= nil, "a tile beside the player's buildings refuses Construct")
if refused then
    print(string.format("construct_tooltip: Construct refused on %d,%d", refused.x, refused.y))
    verify.center_tile(refused.x, refused.y, 6.0)
    verify.select_tile(refused.x, refused.y)
    verify.frames(3)
    verify.hover(aim.x, aim.y, 3)
    verify.capture("construct_tooltip_disabled")
end
