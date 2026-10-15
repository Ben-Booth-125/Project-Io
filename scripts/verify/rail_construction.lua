-- Rail from the Construction interface (BL-1255, rail construction UI;
-- LOGISTICS.md § 4c, NATIONS.md § State industry).
--
--   ProjectIo --verify scripts/verify/rail_construction.lua
--
-- Rail is built like anything else: select a tile, and the Construction view's
-- list of buildings for that tile carries TRACK and RAILWAY STATION rows, each
-- with its cost and either a Build press or its refusal. No armed tool. The rows
-- sit with their kin (Ben, 2026-10-15): Track at the foot of the road ladder,
-- Railway Station inside the Infrastructure fold. The list scrolls, and a press
-- is published only while on screen, so each read sweeps the list's scroll.
--
-- WHAT IT PROVES, every claim read back from the world or the surface's own
-- published press positions (a picture of a ledger proves only that it drew):
--   R1  selecting a tile that does not touch the line (the run's far tile)
--       shows the Track row REFUSED (no Build press published);
--   R2  selecting the city shows Track buildable; its Build press lays one
--       tile through the seam: track +1, the treasury pays at least
--       track_build_cost, the market's shelf gives up track_steel_per_tile;
--   R3  the next two tiles, each selected then built from the list, extend
--       the line (the far tile is now legal because it touches the line);
--   R4  the city (now on track) offers Railway Station; its Build press starts
--       a station for the nation; after it the row is refused (one to a tile).
--
-- CAPTURES: 01 far tile refused, 02 city track row, 03 run laid (rail surface),
-- 04 station row on the city, 05 station started.

verify.window(1600, 900)

verify.new_world(2)
if verify.rail_state().nation == 0 then
    local r = verify.take_seat(verify.seat_candidate(0))
    verify.expect(r == "applied", "a major is seated (" .. tostring(r) .. ")")
end
verify.goto_surface("home")
verify.frames(3)

local function close_column()
    for _, p in ipairs({ "corporation", "balance", "construction", "tech_tree",
                         "acquisitions", "market", "corporations_table", "tile",
                         "generation_ledger", "decisions", "strategy" }) do
        verify.show_panel(p, false)
    end
    verify.frames(1)
end
close_column()
verify.clear_selection()
verify.show_construction()
verify.construction_view(0)
verify.frames(3)

local s = verify.rail_state()
print(string.format("RAIL nation=%d treasury=%.2f track=%d stations=%d", s.nation, s.treasury, s.track, s.stations))
verify.expect(s.nation ~= 0, "the seat's nation is a major")
verify.expect(s.track_x < 0 and s.station_x < 0, "no tile selected: no rail rows")

local f = verify.rail_fixture(true)
verify.expect(f.ok, "the seat nation has a city with a two-tile run of its own land off it")
if not f.ok then return end
print(string.format("RAIL fixture city=(%d,%d) run=(%d,%d)->(%d,%d) steel %.1f->%.1f",
    f.cx, f.cy, f.x1, f.y1, f.x2, f.y2, f.steel_before, f.steel))

verify.center_tile(f.cx, f.cy, 6.0)
verify.frames(3)

-- Sweep the building list's scroll until the named row's Build press is on
-- screen; a row refused (or absent) publishes no press at any scroll.
local function find(kind)
    local st
    for i = 0, 20 do
        verify.scroll_panel("build_list", i / 20); verify.frames(2)
        st = verify.rail_state()
        if st[kind .. "_x"] >= 0 then return st end
    end
    return st
end

-- Select a tile with a real canvas press, and let the ledger redraw for it.
local function pick(x, y, kind)
    verify.scroll_panel("build_list", 0)
    verify.click_tile(x, y); verify.frames(3)
    verify.show_construction(); verify.construction_view(0); verify.frames(3)
    return find(kind or "track")
end

-- R1: the far tile does not touch a line and is no city: refused ----------
s = pick(f.x2, f.y2)
verify.expect(s.track_x < 0, "Track is refused on a tile off the line (no Build press)")
verify.capture("rail_construction_01_refused")

-- R2: Track on the city ----------------------------------------------------
s = pick(f.cx, f.cy)
verify.expect(s.track_x >= 0, "the city's list offers Track with a Build press")
verify.capture("rail_construction_02_city_track")
verify.expect(find("station").station_x < 0, "the city offers no station before it touches track")
s = find("track")
local steel0 = verify.rail_market_steel(f.cx, f.cy)
verify.click(s.track_x, s.track_y); verify.frames(3)
local a = verify.rail_state()
local steel1 = verify.rail_market_steel(f.cx, f.cy)
print(string.format("RAIL city: track %d->%d treasury %.2f->%.2f steel %.2f->%.2f msg=%s",
    s.track, a.track, s.treasury, a.treasury, steel0, steel1, a.message))
verify.expect(a.track == s.track + 1, "the Build press lays one tile of track")
verify.expect(s.treasury - a.treasury >= s.track_cost - 1e-3,
    string.format("the treasury pays at least %.0f cr (paid %.2f)", s.track_cost, s.treasury - a.treasury))
verify.expect(math.abs((steel0 - steel1) - s.steel_per_tile) < 1e-3,
    string.format("the shelf gives up %.1f steel (drew %.2f)", s.steel_per_tile, steel0 - steel1))

-- R3: extend the line tile by tile from the list ---------------------------
for _, t in ipairs({ { f.x1, f.y1 }, { f.x2, f.y2 } }) do
    local b = pick(t[1], t[2])
    verify.expect(b.track_x >= 0, string.format("(%d,%d) touching the line offers Track", t[1], t[2]))
    verify.click(b.track_x, b.track_y); verify.frames(3)
    local c = verify.rail_state()
    verify.expect(c.track == b.track + 1, string.format("(%d,%d) laid (%s)", t[1], t[2], c.message))
    verify.expect(c.treasury < b.treasury, "the treasury paid for it")
end
verify.ground_complete_master(30)
verify.center_tile(f.x1, f.y1, 14.0)
verify.frames(10)
verify.capture("rail_construction_03_laid")
verify.center_tile(f.cx, f.cy, 6.0)
verify.frames(3)

-- R4: a station on the city ------------------------------------------------
verify.expect(pick(f.cx, f.cy, "track").track_x < 0, "the city already carries track: Track refused")
local t0 = find("station")
verify.expect(t0.station_x >= 0, "the city on the line offers Railway Station")
verify.capture("rail_construction_04_station_row")
verify.click(t0.station_x, t0.station_y); verify.frames(3)
local t1 = verify.rail_state()
print(string.format("RAIL station: stations %d->%d treasury %.2f->%.2f msg=%s",
    t0.stations, t1.stations, t0.treasury, t1.treasury, t1.message))
verify.expect(t1.stations == t0.stations + 1, "the Build press starts a station for the nation")
-- (a station is paid as it builds, construction.cpp; no up-front charge to assert)
verify.frames(3)
verify.expect(find("station").station_x < 0, "a second station on the tile is refused")
verify.frames(30)
verify.capture("rail_construction_05_station")
