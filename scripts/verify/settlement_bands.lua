-- BL-1711 (settlement size rendering) - RENDERING.md § Installations, the band table
-- (Ben, 2026-11-19): each population-centre size band draws a different group on
-- its one tile - village houses, a town's pitched grid and spire, a city's flat
-- blocks and civic dome, a metropolis's tower cluster, a megacity's skyline.
--
-- One capture per band at the play rung and one close, framing the centre of that
-- scale on the home body nearest the middle latitude (ties by position, so the frame
-- does not wander as generation moves), off the wrap seam. A band the world does
-- not hold prints a line and is skipped rather than failing: scale 5 is rare.
--
--   ProjectIo --verify scripts/verify/settlement_bands.lua

verify.goto_surface("home")
verify.set_overlay("none")

local home = nil
local by_scale = {}
for _, c in ipairs(verify.population_centres()) do
    if c.capital then home = home or c.body end
end
local miny, maxy = nil, nil
for _, c in ipairs(verify.population_centres()) do
    if home == nil or c.body == home then
        miny = (miny == nil or c.y < miny) and c.y or miny
        maxy = (maxy == nil or c.y > maxy) and c.y or maxy
    end
end
local midy = ((miny or 0) + (maxy or 0)) / 2
-- Per band, the centre nearest the middle latitude (polar ground frames badly), ties
-- by (x, y); off the wrap seam (the first and last columns frame half on each edge).
for _, c in ipairs(verify.population_centres()) do
    if (home == nil or c.body == home) and c.x >= 8 then
        local cur = by_scale[c.scale]
        local d, dc = math.abs(c.y - midy), cur and math.abs(cur.y - midy)
        if cur == nil or d < dc or (d == dc and (c.x < cur.x or (c.x == cur.x and c.y < cur.y))) then
            by_scale[c.scale] = c
        end
    end
end

local names = { "village", "town", "city", "metropolis", "megacity" }
local found = 0
for s = 1, 5 do
    local c = by_scale[s]
    if c == nil then
        print(string.format("settlement_bands: no scale-%d centre (%s) on the home body", s, names[s]))
    else
        found = found + 1
        print(string.format("settlement_bands: %s (scale %d) at (%d, %d)", names[s], s, c.x, c.y))
        frame_tile(c.x, c.y, 8.55)   -- the 48 px/hex level
        shot(string.format("settlement_band_%d_%s", s, names[s]), 3)
        frame_tile(c.x, c.y, 17.1)   -- the 96 px/hex level
        shot(string.format("settlement_band_%d_%s_close", s, names[s]), 3)
    end
end
if found < 3 then
    error("settlement_bands: fewer than three size bands on the home body - the check cannot compare them")
end
