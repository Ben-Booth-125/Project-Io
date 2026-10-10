-- TOWNS DENSER (BL-1258; Ben, 2026-10-10: towns should read like the it3 C-F
-- reference — compact blocks of multi-storey buildings with lit roofs, shaded
-- sides, stacks and a street grid — reading as a town from the mid rungs;
-- RENDERING.md § Art direction and palette, the towns bullet, and
-- § Installations).
--
--   ProjectIo --verify scripts/verify/towns_denser.lua      (Release)
--
-- WHAT THIS PHOTOGRAPHS. On seed 0 at the reference 1720x1080 window, the
-- whole home master baked first:
--
--   towns_ladder_s<N>_rung<k>  five real population centres stepped to the
--               settlement ladder (Outpost 1 -> Metropolis 5) with
--               verify.stage_centre — each re-bakes by the partial re-bake
--               window, so this also exercises the bounds mode live — framed
--               at rungs 2 (~27 px, the mid rung a town must read as a town
--               at), 3 and 4;
--   towns_ladder_far           rung 0 over the ladder: a scale >= 3 centre
--               must still read as a city;
--   towns_roads_rung<k>        roads_painted.lua's city: roads through a town
--               are its streets, and roads meeting its works.
--
-- No assertion pins a pixel and no golden is compared: captures for the eye.
-- (tools/verify/ground_bake_check --towns carries the staged ladder, a town on
-- a highway, a crossed city and a town beside works at every level.)

verify.window(1720, 1080)
verify.goto_surface("home")
verify.frames(4)
verify.clear_selection()

local function complete()
    local r = verify.ground_complete_master(900)
    verify.expect(r.ok and r.ready == r.total and r.current,
                  string.format("towns_denser: the home master completes (%d/%d, %.0f ms)",
                                r.ready, r.total, r.ms))
end
complete()

local function settle()
    for i = 1, 4000 do
        verify.frames(1)
        local s = verify.ground_stats()
        if s.pending_uploads == 0 and s.chunks > 0 then break end
    end
    verify.frames(4)
end

local kMinZoom = 1.2531328 -- body_surface_canvas.cpp kMinZoom

-- The roads city (roads_painted.lua's framing): its body is the home body.
local city = { 103, 72 }
local home_body = nil
for _, pc in ipairs(verify.population_centres()) do
    if math.abs(pc.x - city[1]) <= 1 and math.abs(pc.y - city[2]) <= 1 then home_body = pc.body end
end
verify.expect(home_body ~= nil, "towns_denser: the roads city still stands at 103,72")

for _, k in ipairs({ 2, 3, 4 }) do
    verify.center_tile(city[1], city[2], kMinZoom * (2 ^ k))
    settle()
    verify.capture(string.format("towns_roads_rung%d", k))
end

-- The ladder: five home-body centres in one band of rows, west to east, not
-- the roads city, stepped to scales 1-5.
local picks = {}
for _, pc in ipairs(verify.population_centres()) do
    if pc.body == home_body and not (math.abs(pc.x - city[1]) <= 1 and math.abs(pc.y - city[2]) <= 1)
       and math.abs(pc.y - city[2]) <= 12 then
        picks[#picks + 1] = pc
    end
end
-- Nearest the roads city first (the verify sandbox carries no `table` library:
-- a selection sort).
local function before(a, b)
    local da = math.abs(a.x - city[1]) + math.abs(a.y - city[2])
    local db = math.abs(b.x - city[1]) + math.abs(b.y - city[2])
    if da ~= db then return da < db end
    return a.x < b.x
end
for i = 1, #picks do
    local m = i
    for j = i + 1, #picks do
        if before(picks[j], picks[m]) then m = j end
    end
    picks[i], picks[m] = picks[m], picks[i]
end
verify.expect(#picks >= 5, "towns_denser: five centres near the roads city to step (" .. #picks .. ")")
local ladder = {}
for i = 1, math.min(5, #picks) do
    local pc = picks[i]
    verify.expect(verify.stage_centre(pc.x, pc.y, i, false),
                  string.format("towns_denser: stage_centre %d,%d to scale %d", pc.x, pc.y, i))
    ladder[i] = pc
    print(string.format("TOWNS_LADDER scale %d at %d,%d", i, pc.x, pc.y))
end
verify.frames(2)
complete()

for i, pc in ipairs(ladder) do
    for _, k in ipairs({ 2, 3, 4 }) do
        verify.center_tile(pc.x, pc.y, kMinZoom * (2 ^ k))
        settle()
        verify.capture(string.format("towns_ladder_s%d_rung%d", i, k))
    end
end
if ladder[3] then
    verify.center_tile(ladder[3].x, ladder[3].y, kMinZoom)
    settle()
    verify.capture("towns_ladder_far")
end
