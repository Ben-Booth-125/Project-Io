-- BL-1722 (units over construction wheel) - RENDERING.md § Draw order on the ground:
-- a unit counter draws OVER a building site's progress circle, and the pointer and
-- the press agree with what is on top.
--
-- Ben, playtest 2026-11-22: "Let's draw units over the construction wheel for newly
-- constructing buildings."
--
-- Staging: a site (verify.stage_building with site = true) on the tile of one of the
-- player's own units, read from verify.units(). Nothing hard-codes a coordinate.
--
--   U1  the site carries its circle (construction_controls.site_marks)
--   U2  a point inside BOTH the circle and the counter hovers the UNIT, and the
--       site card does not open over it
--   U3  a press at that point selects the unit (the press order matches the draw)
--   U4  a point on the circle and off the counter still opens the site card
--
-- Captures: the site with its unit at play zoom and close zoom, unhovered.
--
-- Run: ProjectIo --verify scripts/verify/units_over_site.lua

verify.window(1720, 1080)
verify.goto_surface("home")
verify.set_balance(5000000.0)

local mine = nil
for _, u in ipairs(verify.units()) do
    if u.col ~= nil and u.player then mine = u; break end
end
assert(mine, "the player has no unit on the surface (every nation seeds a garrison)")

local site = verify.stage_building(mine.col, mine.row, "logistics_hub", nil, { site = true })
verify.expect(site ~= 0, "staging: a site stands on the unit's tile")
verify.econ_step(1) -- a day or two built, so the circle carries an arc
verify.set_balance(5000000.0)
verify.clear_selection()

local function mark_here()
    local cc = verify.construction_controls()
    for _, m in ipairs(cc.site_marks or {}) do
        if m.building == site then return m end
    end
    return nil
end

for _, z in ipairs({ 3.0, 6.0, 10.0 }) do
    verify.center_tile(mine.col, mine.row, z)
    verify.mouse(-1, -1)
    verify.frames(3)
    verify.capture(string.format("units_over_site_z%d", math.floor(z)))
end

-- The checks run at play zoom 3, where the circle (radius clamped to 5..13 px) and
-- the counter (9 px floor, half a radius below the centre) overlap most.
verify.center_tile(mine.col, mine.row, 3.0)
verify.mouse(-1, -1)
verify.frames(3)
local m = mark_here()
verify.expect(m ~= nil, "U1: the site on the unit's tile carries the progress circle")
if m then
    -- Walk down from the circle's centre inside the circle; the first point that
    -- hovers a unit is inside both.
    local both = nil
    for dy = 0, math.floor(m.r), 1 do
        verify.hover(m.x, m.y + dy, 2)
        if verify.pointer_target().hovered_kind == "unit" then both = { x = m.x, y = m.y + dy }; break end
    end
    verify.expect(both ~= nil, "U2: some point inside the circle hovers the unit counter")
    if both then
        verify.hover(both.x, both.y, 3)
        local cc = verify.construction_controls()
        verify.expect((cc.site_card_for or 0) ~= site,
            "U2: the site card does not open while the pointer is on the counter")
        verify.capture("units_over_site_hover_overlap")
        verify.click(both.x, both.y)
        verify.frames(2)
        local pt = verify.pointer_target()
        verify.expect(pt.selection_kind == "unit",
            "U3: a press where counter and circle overlap selects the unit (got " .. tostring(pt.selection_kind) .. ")")
        verify.clear_selection()
    end
    -- Above the centre: on the circle, clear of the counter.
    verify.hover(m.x, m.y - m.r * 0.6, 3)
    local top = verify.construction_controls()
    verify.expect((top.site_card_for or 0) == site and verify.pointer_target().hovered_kind ~= "unit",
        "U4: the circle clear of the counter still opens its site card")
end
verify.mouse(-1, -1)
