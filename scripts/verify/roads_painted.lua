-- ROADS AND SEA LANES PAINTED INTO THE GROUND (BL-1253; Ben, 2026-10-09: "roads go
-- over buildings, rather than being painted as an optional part of the building
-- tile sets. Roads are also so simplified, we want to paint a texture on roads";
-- RENDERING.md § Roads and sea lanes).
--
--   ProjectIo --verify scripts/verify/roads_painted.lua      (Release)
--
-- WHAT THIS PHOTOGRAPHS. On seed 0 (the harness's own world) at the reference
-- 1720x1080 window, the whole home master baked first, three framings at the
-- rungs the painted roads carry -- rung 2 (~27 px), rung 3 (~55 px) and rung 4
-- (~110 px, the master near 1:1) -- plus rung 1 (~14 px), where a painted road
-- is near a pixel (the plain canvas draws no network at any rung, BL-1262):
--
--   city_roads  a city carrying all three tiers (Highway, Road, Track) and
--               roads meeting its works (the roaded variant, the forecourt);
--   lane_port   a sea lane's end beside a port city (the wake);
--   coast_lane  a coastal run of a lane (the wake along the shore).
--
-- Each framing is taken twice: on the plain canvas, where a road is a thin pale
-- thread painted into the ground (0.015-0.03 of a hex) and no network is drawn
-- (Ben, 2026-10-10); and under the THROUGHPUT lens (BL-1257, LENSES.md
-- § Throughput lens), which draws the whole road and lane network at full
-- weight (1 : 1.5 : 2, lanes in their blue) over its reach-cost field at
-- every rung -- roads_painted_<framing>_throughput_rung<k>.
--
-- No assertion pins a pixel and no golden is compared: captures for the eye.
-- The framings are route_curves.lua's (seed 0's stamped fields, read once);
-- the city is re-found from population_centres so a moved city fails loudly.

verify.window(1720, 1080)
verify.goto_surface("home")
verify.frames(4)
verify.clear_selection()

do
    local r = verify.ground_complete_master(900)
    verify.expect(r.ok and r.ready == r.total and r.current,
                  string.format("roads_painted: the home master completes (%d/%d, %.0f ms)",
                                r.ready, r.total, r.ms))
    print(string.format("ROADS_PAINTED_MASTER %d/%d complete in %.0f ms", r.ready, r.total, r.ms))
end

local function near_centre(x, y, radius)
    for _, pc in ipairs(verify.population_centres()) do
        if math.abs(pc.x - x) <= radius and math.abs(pc.y - y) <= radius then return pc end
    end
    return nil
end

local kMinZoom = 1.2531328 -- body_surface_canvas.cpp kMinZoom
local framings = {
    { name = "city_roads", col = 103, row = 72, city = { 103, 72 } },
    { name = "lane_port",  col = 111, row = 91, city = { 110, 92 } },
    { name = "coast_lane", col = 228, row = 36 },
}

local function settle()
    for i = 1, 4000 do
        verify.frames(1)
        local s = verify.ground_stats()
        if s.pending_uploads == 0 and s.chunks > 0 then break end
    end
    verify.frames(4)
end

for _, f in ipairs(framings) do
    if f.city then
        verify.expect(near_centre(f.city[1], f.city[2], 1) ~= nil,
                      "roads_painted: the " .. f.name .. " framing still has its city at "
                      .. f.city[1] .. "," .. f.city[2])
    end
    for _, lens in ipairs({ "none", "throughput" }) do
        verify.set_overlay(lens)
        for _, k in ipairs({ 1, 2, 3, 4 }) do
            verify.center_tile(f.col, f.row, kMinZoom * (2 ^ k))
            settle()
            if lens == "none" then
                verify.capture(string.format("roads_painted_%s_rung%d", f.name, k))
            else
                verify.capture(string.format("roads_painted_%s_%s_rung%d", f.name, lens, k))
            end
        end
    end
end
verify.set_overlay("none")
