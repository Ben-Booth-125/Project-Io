-- Rail from the Construction interface (BL-1255, rail construction UI;
-- LOGISTICS.md § 4c, NATIONS.md § State industry).
--
--   ProjectIo --verify scripts/verify/rail_construction.lua
--
-- WHAT IT PROVES, every claim read back from the world or the surface's own
-- published press positions (a picture of a ledger proves only that it drew):
--   R1  the Construction view carries a Rail group whose two tools are REAL
--       presses, and each is a TOGGLE (pressing the armed tool disarms it);
--   R2  with Lay track armed, map clicks PLAN a run without laying anything:
--       the treasury and the track count do not move; a click on the run's
--       last tile un-plans it;
--   R3  the ledger's Lay track press lays the planned run through the seam:
--       the nation's track grows by the run, the treasury pays
--       run x (track_build_cost + steel at the market's price), and the
--       market's shelf gives up run x track_steel_per_tile steel;
--   R4  with Railway station armed, a click on the city (now touching track)
--       starts a station for the nation; a second click there is refused
--       (one to a tile) and changes nothing.
--
-- CAPTURES: 01 tools at rest, 02 run planned (preview), 03 run laid (rail
-- surface), 04 station tool armed (legal tiles ringed), 05 station started.

verify.window(1600, 900)

-- Seat a major, on a world where the railway exists.
verify.new_world(2)
-- run_verify's roster already seats a major's champion; pick one only if not.
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
verify.expect(s.track_x >= 0, "the Rail group draws its Lay track tool")
verify.expect(s.station_x >= 0, "the Rail group draws its Railway station tool")
verify.capture("rail_construction_01_tools")

-- R1: the toggles -----------------------------------------------------------
verify.click(s.track_x, s.track_y); verify.frames(2)
verify.expect(verify.rail_state().tool == 1, "Lay track arms the track tool")
s = verify.rail_state()
verify.click(s.track_x, s.track_y); verify.frames(2)
verify.expect(verify.rail_state().tool == 0, "pressing the armed Lay track disarms it (toggle rule)")
s = verify.rail_state()
verify.click(s.station_x, s.station_y); verify.frames(2)
verify.expect(verify.rail_state().tool == 2, "Railway station arms the station tool")
s = verify.rail_state()
verify.click(s.track_x, s.track_y); verify.frames(2)
verify.expect(verify.rail_state().tool == 1, "the other tool switches, not stacks")

-- R2: plan a run ------------------------------------------------------------
local f = verify.rail_fixture(true)
verify.expect(f.ok, "the seat nation has a city with a two-tile run of its own land off it")
if not f.ok then return end
print(string.format("RAIL fixture city=(%d,%d) run=(%d,%d)->(%d,%d) steel %.1f->%.1f",
    f.cx, f.cy, f.x1, f.y1, f.x2, f.y2, f.steel_before, f.steel))

verify.center_tile(f.cx, f.cy, 6.0)
verify.frames(3)
local before = verify.rail_state()
verify.click_tile(f.cx, f.cy); verify.frames(2)
verify.click_tile(f.x1, f.y1); verify.frames(2)
verify.click_tile(f.x2, f.y2); verify.frames(2)
verify.expect(verify.rail_state().run == 3, "three clicks plan three tiles (" .. verify.rail_state().run .. ")")
verify.click_tile(f.x2, f.y2); verify.frames(2)
verify.expect(verify.rail_state().run == 2, "a click on the run's last tile un-plans it")
verify.click_tile(f.x2, f.y2); verify.frames(2)
local planned = verify.rail_state()
verify.expect(planned.run == 3, "and a click re-plans it")
verify.expect(planned.track == before.track and math.abs(planned.treasury - before.treasury) < 1e-3,
    "planning lays nothing and charges nothing")
verify.hover_tile(f.x2, f.y2); verify.frames(2)
verify.capture("rail_construction_02_planned")

-- R3: lay it ----------------------------------------------------------------
local steel0 = verify.rail_market_steel(f.cx, f.cy)
s = verify.rail_state()
verify.expect(s.commit_x >= 0, "the planned run offers its Lay track press")
verify.click(s.commit_x, s.commit_y); verify.frames(3)
local laid = verify.rail_state()
local steel1 = verify.rail_market_steel(f.cx, f.cy)
print(string.format("RAIL laid: track %d->%d treasury %.2f->%.2f steel %.2f->%.2f msg=%s",
    s.track, laid.track, s.treasury, laid.treasury, steel0, steel1, laid.message))
verify.expect(laid.track == s.track + 3, "the nation's track grows by the three planned tiles")
verify.expect(laid.run == 0, "the laid tiles leave the plan")
local paid = s.treasury - laid.treasury
verify.expect(paid >= 3 * s.track_cost - 1e-3,
    string.format("the treasury pays at least 3 x %.0f cr (paid %.2f)", s.track_cost, paid))
verify.expect(math.abs((steel0 - steel1) - 3 * s.steel_per_tile) < 1e-3,
    string.format("the shelf gives up 3 x %.1f steel (drew %.2f)", s.steel_per_tile, steel0 - steel1))
-- Re-take the ground's snapshot and bake the master, so the capture shows the
-- rail surface the world now carries (the synchronous verify path bakes lazily).
verify.ground_complete_master(30)
verify.center_tile(f.x1, f.y1, 14.0)
verify.frames(10)
verify.capture("rail_construction_03_laid")
verify.center_tile(f.cx, f.cy, 6.0)
verify.frames(3)

-- R4: a station on the city -------------------------------------------------
verify.click(laid.station_x, laid.station_y); verify.frames(2)
verify.expect(verify.rail_state().tool == 2, "Railway station arms")
verify.capture("rail_construction_04_station_tool")
local t0 = verify.rail_state()
verify.click_tile(f.cx, f.cy); verify.frames(3)
local t1 = verify.rail_state()
print(string.format("RAIL station: stations %d->%d treasury %.2f->%.2f msg=%s",
    t0.stations, t1.stations, t0.treasury, t1.treasury, t1.message))
verify.expect(t1.stations == t0.stations + 1, "a click on the city starts a station for the nation")
verify.click_tile(f.cx, f.cy); verify.frames(3)
verify.expect(verify.rail_state().stations == t1.stations, "a second station on the tile is refused")
verify.frames(30)
verify.capture("rail_construction_05_station")
