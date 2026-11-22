-- Visual verification for two lens rulings (Ben, 2026-11-22). LENSES.md owns both.
--
--   N1  THE RAIL / ROAD TOGGLE (BL-1732). The Logistics lens's key carries Both / Rail /
--       Road, opening on Both. A press on Rail keeps the rail network alone (its legs,
--       its track, its stations); Road keeps the roads alone; a press on the active one
--       returns to Both (the toggle rule); a re-arm opens on Both again. Real presses
--       through the input path, at the positions the key publishes.
--   M1  CONVOYS DRAW UNDER TRADE FLOW AND SUPPLY (BL-1729). No convoy marker on the plain
--       canvas or under another lens; markers under Trade Flow and Supply.
--
-- Captures: a bend and a junction near the capital, and a rail line, under each view.
-- Run with: ProjectIo --verify scripts/verify/logistics_network_view.lua

verify.window(1920, 1080)
verify.goto_surface("home")
verify.set_overlay("none")
verify.frames(2)

local cs = verify.capital_stars()
local cap_x, cap_y = nil, nil
if cs.has_capital then cap_x, cap_y = cs.x, cs.y end
verify.expect(cap_x ~= nil, "the player's nation has a capital to frame on")

local fx = verify.logistics_chain_fixture()
verify.expect(fx.ok and fx.rail, "the chain fixture stages a rail leg on the player's capital")
verify.econ_step(2)
verify.frames(2)

-- ---------------------------------------------------------------- N1
verify.set_overlay("logistics")
verify.frames(3)
local r0 = verify.logistics_roads()
print(string.format("[network_view] Both: view %d; roads %d, rail %d, legs rail %d, anchors %d",
      r0.network_view, r0.roads, r0.rail, r0.legs_rail, r0.anchors))
verify.expect(r0.network_view == 0, "N1: the lens opens on Both (" .. r0.network_view .. ")")
verify.expect(r0.view_rail_x and r0.view_rail_x > 0, "N1: the key publishes its toggle")
if cap_x then frame_tile(cap_x, cap_y, 10) end
shot("netview_both_capital")

local function press(which)
  local r = verify.logistics_roads()
  local x, y = r["view_" .. which .. "_x"], r["view_" .. which .. "_y"]
  verify.hover(x, y, 2)
  verify.click(x, y)
  verify.frames(3)
  return verify.logistics_roads()
end

-- Rail.
local rr = press("rail")
print(string.format("[network_view] Rail: view %d; roads %d, rail %d, legs rail %d, anchors %d",
      rr.network_view, rr.roads, rr.rail, rr.legs_rail, rr.anchors))
verify.expect(rr.network_view == 1, "N1: a press on Rail shows the rail network (" .. rr.network_view .. ")")
verify.expect(rr.legs_rail > 0, "N1: the rail leg still draws under Rail (" .. rr.legs_rail .. ")")
verify.expect(rr.roads < r0.roads, "N1: Rail drops the road cells (" .. rr.roads .. " < " .. r0.roads .. ")")
frame_tile((fx.station_a_x + fx.station_b_x) // 2, (fx.station_a_y + fx.station_b_y) // 2, 6)
shot("netview_rail_line")
frame_tile(fx.station_b_x, fx.station_b_y, 12)
shot("netview_rail_station_close")

-- Rail again: the toggle rule returns to Both.
local rb = press("rail")
verify.expect(rb.network_view == 0, "N1: a press on the active Rail returns to Both (" .. rb.network_view .. ")")

-- Road.
local rd = press("road")
print(string.format("[network_view] Road: view %d; roads %d, rail %d, legs rail %d, anchors %d",
      rd.network_view, rd.roads, rd.rail, rd.legs_rail, rd.anchors))
verify.expect(rd.network_view == 2, "N1: a press on Road shows the roads (" .. rd.network_view .. ")")
verify.expect(rd.legs_rail == 0, "N1: no rail leg draws under Road (" .. rd.legs_rail .. ")")
verify.expect(rd.roads > 0, "N1: the roads still draw under Road (" .. rd.roads .. ")")
if cap_x then
  frame_tile(cap_x, cap_y, 14)
  shot("netview_road_junction_close")
  frame_tile(cap_x + 3, cap_y + 2, 14)
  shot("netview_road_bend_close")
end
frame_tile((fx.station_a_x + fx.station_b_x) // 2, (fx.station_a_y + fx.station_b_y) // 2, 6)
shot("netview_road_on_rail_line")

-- Road to Rail directly is an ordinary view change.
local rx = press("rail")
verify.expect(rx.network_view == 1, "N1: Road -> Rail switches the view (" .. rx.network_view .. ")")

-- A re-arm opens on Both.
verify.set_overlay("none")
verify.frames(2)
verify.set_overlay("logistics")
verify.frames(3)
local ra = verify.logistics_roads()
verify.expect(ra.network_view == 0, "N1: a re-arm opens on Both (" .. ra.network_view .. ")")
if cap_x then frame_tile(cap_x, cap_y, 14) end
shot("netview_both_junction_close")

-- ---------------------------------------------------------------- M1
local own_id = verify.seed_lane_convoy("iron_ore", 20, 0.05)
verify.expect(own_id ~= 0, "M1: a convoy stages on the player's ground")
verify.frames(2)
local anchor = nil
for _, u in ipairs(verify.units()) do
  if u.player and u.col ~= nil and u.count > 0 then anchor = u; break end
end
if anchor then verify.center_tile(anchor.col, anchor.row, 3.0) end

local function markers(lens)
  verify.set_overlay(lens)
  verify.frames(3)
  local n = #verify.convoy_markers()
  print("[network_view] convoy markers under " .. lens .. ": " .. n)
  return n
end
local m_none = markers("none")
local m_log  = markers("logistics")
local m_tf   = markers("trade_flow")
shot("convoys_trade_flow")
local m_sup  = markers("supply")
shot("convoys_supply")
verify.expect(m_none == 0, "M1: no convoy marker on the plain canvas (" .. m_none .. ")")
verify.expect(m_log == 0, "M1: no convoy marker under the Logistics lens (" .. m_log .. ")")
verify.expect(m_tf > 0, "M1: convoy markers under Trade Flow (" .. m_tf .. ")")
verify.expect(m_sup > 0, "M1: convoy markers under Supply (" .. m_sup .. ")")
verify.set_overlay("none")
