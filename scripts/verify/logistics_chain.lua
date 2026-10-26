-- Visual verification for three rulings on the Logistics lens and the capital star
-- (Ben, 2026-10-25). LENSES.md § Logistics lens and PLANETARY.md § The capital star
-- own what they show; RENDERING.md § Roads and sea lanes owns the unpainted ground.
--
--   C1  THE CAPITAL STAR. Every nation's capital carries a star in its nation's colour,
--       the player's larger and ringed - on the plain canvas and under most lenses;
--       NONE under Resource, Continent or Population. Asserted on the canvas's own
--       count of stars drawn (verify.capital_stars), captured wide and close.
--   R1  THE ROADS ARE NOT DRAWN TWICE. Under the Logistics lens the bake paints no route
--       surface (strip_painted_routes); the canvas's coloured stroke is the only road.
--       Captured close on the same road with the lens off and on; the toggle's re-bake
--       is counted and TIMED (the ROAD_HIDE lines - the cost Ben asked to see).
--   L1  EVERY LINK OF THE CHAIN. Rail legs along their track and sea legs down their
--       water carry the lens's colour and numbers; a road on the chain that no route
--       uses reads CONNECTED, IDLE (one number, steel), not 0 over 0; a road no
--       connected source reaches reads off the chain (umber, no number).
--
-- Run with: ProjectIo --verify scripts/verify/logistics_chain.lua

verify.goto_surface("home")
verify.set_overlay("none")

-- Find the player's capital: the canvas's own star hook names where its seat is (a
-- public position, the star marks it), and the population centres say which centres
-- stand on a capital tile.
local home_body = nil
for _, b in ipairs(verify.buildings()) do
  if b.player then home_body = b.body break end
end
verify.frames(2)
local cs = verify.capital_stars()
local cap_x, cap_y = nil, nil
if cs.has_capital then cap_x, cap_y = cs.x, cs.y end
local capitals = 0
for _, pc in ipairs(verify.population_centres()) do
  if pc.capital and (home_body == nil or pc.body == home_body) then capitals = capitals + 1 end
end
print("[logistics_chain] player nation " .. tostring(cs.player_nation) .. ": "
      .. (cap_x and ("capital at " .. cap_x .. "," .. cap_y .. " on body " .. tostring(cs.body)
                     .. " (home body " .. tostring(home_body) .. ")") or "NO capital tile")
      .. "; centres on a capital tile on the home body: " .. capitals)
verify.expect(cap_x ~= nil, "C1: the player's nation has a capital to mark")

-- C1 - the plain canvas, whole grid, then close on the player's capital.
shot("capital_star_default_full")
local s0 = verify.capital_stars()
print("[logistics_chain] plain canvas: " .. s0.drawn .. " star(s) drawn, player's " .. tostring(s0.player))
verify.expect(s0.drawn > 0, "C1: the plain canvas draws capital stars (" .. s0.drawn .. ")")
if cap_x then
  frame_tile(cap_x, cap_y, 4)
  shot("capital_star_default_mid")
  local sm = verify.capital_stars()
  verify.expect(sm.player, "C1: the player's capital star is drawn, framed on the capital")
  frame_tile(cap_x, cap_y, 12)
  shot("capital_star_default_close")
end

-- C1 - shown under Market and Nations, hidden under Resource, Continent and Population.
for _, lens in ipairs({ "market", "nations", "resource", "continent", "population" }) do
  verify.set_overlay(lens)
  verify.frames(3)
  local s = verify.capital_stars()
  local hidden = (lens == "resource" or lens == "continent" or lens == "population")
  print("[logistics_chain] " .. lens .. ": " .. s.drawn .. " star(s)")
  if hidden then
    verify.expect(s.drawn == 0, "C1: no capital star under the " .. lens .. " lens (" .. s.drawn .. ")")
  else
    verify.expect(s.drawn > 0, "C1: capital stars under the " .. lens .. " lens (" .. s.drawn .. ")")
  end
end
verify.set_overlay("market")
shot("capital_star_market_mid")

-- R1 - the same frame on the roads round the capital, plain and under the lens, at
-- three rungs: close, mid, and the whole grid. Each toggle is timed on this synchronous
-- --verify path, where every view chunk the flip moves is baked INSIDE the frame (in
-- play the pool bakes them behind the frame; the frame itself pays the snapshot).
local function toggle(lens, label)
  local c0 = verify.ground_bake_counters()
  verify.set_overlay(lens)
  verify.frames(1)
  local t0 = verify.ground_stats().clock_ms
  verify.frames(1)
  local g  = verify.ground_stats()
  local t1 = g.clock_ms
  verify.frames(3)
  local c1 = verify.ground_bake_counters()
  print(string.format("ROAD_HIDE %s -> %s: draw_r %.1f px (level ppr %.0f); frame %.1f ms; re-bakes %d "
        .. "(%d route-only patches, %d windows, %.0f px patched, %.0f px whole); worker ms %.1f",
        label, lens, g.draw_r, g.level_ppr, t1 - t0, c1.chunk_rebakes - c0.chunk_rebakes,
        c1.chunk_patches - c0.chunk_patches, c1.patch_windows - c0.patch_windows,
        c1.patch_px - c0.patch_px, c1.rebake_px - c0.rebake_px, c1.rebake_ms - c0.rebake_ms))
  return c1.chunk_rebakes - c0.chunk_rebakes
end
verify.set_overlay("none")
if cap_x then frame_tile(cap_x, cap_y, 14) end
shot("roads_plain_close")
local n_on = toggle("logistics", "close")
verify.expect(n_on > 0, "R1: arming the Logistics lens close re-bakes the chunks a route reaches")
shot("roads_lens_close")
toggle("none", "close")
shot("roads_plain_close_again")
if cap_x then frame_tile(cap_x, cap_y, 5) end
shot("roads_plain_mid")
toggle("logistics", "mid")
shot("roads_lens_mid")
toggle("none", "mid")
verify.set_zoom(1.2531328)
verify.set_pan(0, 0)
verify.frames(3)
local n_wide = toggle("logistics", "whole grid")
verify.expect(n_wide == 0, "R1: arming the lens on the whole grid re-bakes nothing (a road is under a pixel there)")
toggle("none", "whole grid")
if cap_x then frame_tile(cap_x, cap_y, 14) end
-- Steady state under the lens: no further re-bake once the flip has landed.
verify.set_overlay("logistics")
verify.frames(6)
local c3 = verify.ground_bake_counters()
verify.frames(30)
local c4 = verify.ground_bake_counters()
print(string.format("ROAD_HIDE steady: %d re-bake(s) over 30 frames with the lens up",
      c4.chunk_rebakes - c3.chunk_rebakes))
verify.expect(c4.chunk_rebakes == c3.chunk_rebakes, "R1: a held lens re-bakes nothing")

-- L1 - the chain, whole grid and close, with a real resolve behind it. This seat's
-- nation opens with no Logistic Point source on the home body, so the chain is STAGED:
-- a hub on the capital, a rail leg out to a centre, a sea leg out to a far coast
-- (verify.logistics_chain_fixture - positions back, no tile data).
local fx = verify.logistics_chain_fixture()
print(string.format("[logistics_chain] fixture: ok %s, rail %s, sea %s", tostring(fx.ok),
      tostring(fx.rail), tostring(fx.sea)))
verify.expect(fx.ok, "L1: the chain fixture stages on the player's capital")
verify.econ_step(2)
verify.frames(3)
shot("logistics_chain_full")
local r = verify.logistics_roads()
print(string.format("[logistics_chain] links: %d cells (%d rail, %d sea); carrying %d, idle %d, off the chain %d; "
      .. "legs: %d rail, %d sea", r.roads, r.rail, r.sea, r.carrying, r.idle, r.off_chain,
      r.legs_rail, r.legs_sea))
verify.expect(r.roads > 0, "L1: the player nation has links on the home body")
verify.expect(r.carrying + r.idle + r.off_chain == r.roads, "L1: every link cell has one chain state")
verify.expect(r.carrying > 0, "L1: some link carries a routed request")
if cap_x then
  frame_tile(cap_x, cap_y, 6)
  shot("logistics_chain_capital_mid")
  frame_tile(cap_x, cap_y, 14)
  shot("logistics_chain_capital_close")
end

if fx.rail then
  print("[logistics_chain] rail leg " .. fx.station_a_x .. "," .. fx.station_a_y .. " -> "
        .. fx.station_b_x .. "," .. fx.station_b_y)
  verify.expect(r.legs_rail > 0 and r.rail > 0, "L1: the staged rail leg is drawn along its track ("
                .. r.legs_rail .. " leg(s), " .. r.rail .. " cell(s))")
  frame_tile((fx.station_a_x + fx.station_b_x) // 2, (fx.station_a_y + fx.station_b_y) // 2, 5)
  shot("logistics_chain_rail_mid")
  frame_tile(fx.station_b_x, fx.station_b_y, 12)
  shot("logistics_chain_rail_close")
end

-- L2 - ONE COLOUR PER TILE (Ben, 2026-10-26: "go with one colour per tile, showing both
-- networks added together"). A cell carrying a road AND a rail or sea leg is one tile of
-- the lens: its pair is the networks' SUM (the road's routed flow plus each leg's), it
-- takes one colour from that sum, and the road, the track and the leg stroke all draw in
-- it. The census splits the combined cells' totals into the road's part and the legs'.
do
  if fx.rail then
    frame_tile(fx.station_a_x, fx.station_a_y, 12)
    shot("logistics_one_colour_station_close")
  end
  local rb = verify.logistics_roads()
  print(string.format("[logistics_chain] road + leg cells: %d (%d with a leg share added); supplied %.2f = "
        .. "road %.2f + legs %.2f; requested %.2f = road %.2f + legs %.2f; leg cells stroked %d",
        rb.both, rb.both_added, rb.both_supplied, rb.both_road_supplied, rb.both_leg_supplied,
        rb.both_requested, rb.both_road_requested, rb.both_leg_requested, rb.leg_cells_drawn))
  verify.expect(rb.both > 0, "L2: some tile carries both a road and a leg (" .. rb.both .. ")")
  local tol = 1e-3 * math.max(1.0, rb.both_supplied)
  verify.expect(math.abs(rb.both_supplied - (rb.both_road_supplied + rb.both_leg_supplied)) <= tol,
                "L2: a road + leg tile's supplied is the two networks' sum")
  verify.expect(math.abs(rb.both_requested - (rb.both_road_requested + rb.both_leg_requested))
                <= 1e-3 * math.max(1.0, rb.both_requested),
                "L2: a road + leg tile's requested is the two networks' sum")
  verify.expect(rb.leg_cells_drawn > 0, "L2: the leg is stroked tile by tile in its tiles' colours ("
                .. rb.leg_cells_drawn .. ")")
end
if fx.sea then
  print("[logistics_chain] sea leg " .. fx.port_a_x .. "," .. fx.port_a_y .. " -> "
        .. fx.port_b_x .. "," .. fx.port_b_y)
  verify.expect(r.legs_sea > 0 and r.sea > 0, "L1: the staged sea leg is drawn down its water ("
                .. r.legs_sea .. " leg(s), " .. r.sea .. " cell(s))")
  -- Framed on the near port, not the midpoint: a sea leg can cross the cylinder seam,
  -- where the two ends' columns average to the far side of the world.
  frame_tile(fx.port_a_x, fx.port_a_y, 3)
  shot("logistics_chain_sea_mid")
  frame_tile(fx.port_b_x, fx.port_b_y, 10)
  shot("logistics_chain_sea_close")
end
verify.expect(r.idle > 0, "L1: some road on the chain is connected but idle (" .. r.idle .. ")")

-- K1 - THE RED END (Ben, 2026-10-26: "use red for the extreme"). The ramp is red at 0.5x
-- or less, yellow at 1x, green at 2x or more, log2 between. The census reads the canvas's
-- own ramp positions; the key is captured with the three stops on it.
-- The chain fixture's links all run at 2x or more, so a SHORTAGE is staged: one works on
-- a road of the nation no connected source reaches (C++ picks it, position back), whose
-- request lays on its own road with nothing supplied - 0 over its need, the red end.
local function ramp_line(rr, label)
  print(string.format("[logistics_chain] ramp %s: green %d (routed %d), between %d, red %d (routed %d), "
        .. "near 1x %d, of %d link cells", label, rr.green, rr.routed_green, rr.between, rr.red,
        rr.routed_red, rr.near_one, rr.roads))
end
ramp_line(r, "on the chain fixture")
verify.expect(r.green + r.between + r.red == r.roads, "K1: every link cell lands in one ramp class")
local sf = verify.logistics_shortage_fixture()
print("[logistics_chain] shortage fixture: " .. tostring(sf.ok)
      .. (sf.ok and (" at " .. sf.x .. "," .. sf.y) or ""))
verify.expect(sf.ok, "K1: a works stages on a road no connected source reaches")
verify.econ_step(1)
verify.frames(3)
local rk = verify.logistics_roads()
ramp_line(rk, "with the shortage")
verify.expect(rk.green + rk.between + rk.red == rk.roads, "K1: every link cell lands in one ramp class")
verify.expect(rk.routed_red > 0, "K1: a carrying link supplied at half its request or less reads RED ("
              .. rk.routed_red .. ")")
if sf.ok then
  frame_tile(sf.x, sf.y, 14)
  shot("logistics_red_road_close")
  frame_tile(sf.x, sf.y, 6)
  shot("logistics_red_road_mid")
end
-- The key, red 0.5x -> yellow 1x -> green 2x+ (the key draws at every framing).
if cap_x then frame_tile(cap_x, cap_y, 6) end
shot("logistics_key_red_end")

-- S1 - A PRESS UNDER THE LENS SELECTS THE TILE (Ben, 2026-10-26). It used to answer
-- nothing and clear the band to resting, which is the player's own nation - so a click
-- "selected the nation". The capital tile, then a road tile of the chain beside it.
if cap_x then
  frame_tile(cap_x, cap_y, 8)
  -- From a cleared band, so the press is what selects (not a selection left standing).
  verify.clear_selection()
  verify.frames(2)
  local pt0 = verify.pointer_target()
  verify.expect(not pt0.has_selection, "S1: the band starts cleared (" .. tostring(pt0.selection_kind) .. ")")
  verify.click_tile(cap_x, cap_y)
  verify.frames(3)
  local pt = verify.pointer_target()
  print("[logistics_chain] press on the capital under the lens: " .. tostring(pt.selection_kind))
  verify.expect(pt.has_selection and pt.selection_kind == "tile",
                "S1: a press under the Logistics lens selects the tile (" .. tostring(pt.selection_kind) .. ")")
  shot("logistics_press_selects_tile")
  verify.click_tile(cap_x, cap_y)
  verify.frames(3)
  local pt2 = verify.pointer_target()
  verify.expect(pt2.selection_kind == "tile", "S1: a repeat press re-selects the tile (one tier, no cycle)")
  verify.clear_selection()
end

-- P1 - A SEA LANE'S END GOES INTO ITS PORT (Ben, 2026-10-26). The census says how many
-- lane-to-port joins the canvas draws and, for the diagnosis, how lane ends and ports
-- sit on this body (counts; the positions go to the log from C++).
local lc = verify.lane_port_census()
print(string.format("[logistics_chain] lanes: %d tiles, %d ends - %d beside a standing port, %d beside an "
      .. "unfinished one, %d beside none (%d on a coast, %d with a port within 3); ports: %d standing, %d "
      .. "laned, %d dry, %d unlaned with a lane within 3; joins drawn %d",
      lc.lane_tiles, lc.ends, lc.ends_at_port, lc.ends_at_unstanding, lc.orphan_ends, lc.orphan_coastal,
      lc.orphan_port_near, lc.ports, lc.ports_laned, lc.ports_dry, lc.ports_unlaned_lane_near, lc.joins))
verify.expect(lc.joins > 0, "P1: the canvas joins some lane to its port (" .. lc.joins .. ")")
verify.expect(lc.joins >= lc.ports_laned, "P1: every laned port is joined (" .. lc.joins .. " joins, "
              .. lc.ports_laned .. " laned ports)")
if fx.sea then
  frame_tile(fx.port_a_x, fx.port_a_y, 14)
  shot("logistics_port_lane_end_close")
  frame_tile(fx.port_b_x, fx.port_b_y, 14)
  shot("logistics_port_lane_end_far_close")
  frame_tile(fx.port_a_x, fx.port_a_y, 6)
  shot("logistics_port_lane_end_mid")
end
verify.set_overlay("none")
