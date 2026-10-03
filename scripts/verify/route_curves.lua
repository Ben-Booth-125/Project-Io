-- ROADS AS THINNER CURVES, SEA LANES ON THEIR SEA PATH (Ben, 2026-10-03, playing
-- the build: "render [roads] as curves rather than lines, and make them thinner";
-- "sea lanes should always go over ocean, never over ground"; RENDERING.md
-- § Roads and sea lanes; STARTUP.md § Round 5, "The lane").
--
--   ProjectIo --verify scripts/verify/route_curves.lua
--
-- WHAT THIS PHOTOGRAPHS. On seed 0 (the harness's own world), the Planetary
-- canvas at two zooms (5 and 10, the ~27 px and ~55 px rungs) over three
-- framings -- a coastal stretch a sea lane runs along, a lane meeting its port
-- beside a city, and a city's road network carrying all three tiers -- each at
-- TWO route widths, so the width is a pick rather than a guess:
--
--   *_w050  the shipped width: half the old straight-segment roads per tier;
--   *_w075  0.75x the old width (`set_route_width_scale(1.5)`).
--
-- Then the wizard's round 5 lapse at its close, where lanes are drawn along the
-- stamp's water-only walk rather than bowed between the seats.
--
-- No assertion pins a pixel and no golden is compared: these are captures for
-- the width pick. WHY THE TILES ARE CONSTANTS. Lua may not read tile data, so a
-- script cannot search for a lane; these three framings were read off seed 0's
-- stamped field once (2026-10-03: 1,796 lane tiles, 214 of them on a coast,
-- none on land; roads 499 Track / 1,933 Road / 2,300 Highway). The city is
-- re-found from `population_centres` so a moved city fails loudly rather than
-- framing open ground; the lane framings are seed 0's and move if its history
-- does.

verify.window(1280, 720)

-- The campaign map.
verify.goto_surface("home")
verify.frames(4)
verify.clear_selection()

local function near_centre(x, y, radius)
    for _, pc in ipairs(verify.population_centres()) do
        if math.abs(pc.x - x) <= radius and math.abs(pc.y - y) <= radius then return pc end
    end
    return nil
end

local framings = {
    -- A coastal stretch: the densest run of laned sea tiles that touch land.
    { name = "coast_lane", col = 228, row = 36 },
    -- A lane's end tile (112, 91) beside the city at (110, 92): the lane meets its port.
    { name = "lane_port",  col = 111, row = 91, city = { 110, 92 } },
    -- A city carrying all three tiers within six tiles (33 Highway, 29 Road, 18 Track).
    { name = "city_roads", col = 103, row = 72, city = { 103, 72 } },
}

for _, f in ipairs(framings) do
    if f.city then
        verify.expect(near_centre(f.city[1], f.city[2], 1) ~= nil,
                      "route_curves: the " .. f.name .. " framing still has its city at "
                      .. f.city[1] .. "," .. f.city[2])
    end
    for _, z in ipairs({ 5.0, 10.0 }) do
        for _, w in ipairs({ { "w050", 1.0 }, { "w075", 1.5 } }) do
            verify.expect(verify.set_route_width_scale(w[2]), "route width scale accepted")
            verify.center_tile(f.col, f.row, z)
            verify.frames(4)
            verify.capture(string.format("route_curves_%s_z%02d_%s", f.name, math.floor(z), w[1]))
        end
    end
end

verify.set_route_width_scale(1.0)

-- ROUND 5 LAST (the wizard, once entered, holds the frame): the rounds run in
-- order, 4 then 5, as fleets_and_ties does.
verify.history_run(1)
verify.frames(4)
verify.history_run(2)
verify.frames(4)
local first, last = verify.history_span()
verify.expect(last > first, "round 5 carries a record with a span (" .. first .. " -> " .. last .. ")")
verify.history_year(last)
verify.frames(4)
verify.capture("route_curves_lapse_round5_close")
verify.expect(not verify.set_route_width_scale(0.0), "a zero route width scale is rejected")
verify.expect(not verify.set_route_width_scale(5.0), "a route width scale over 4 is rejected")
