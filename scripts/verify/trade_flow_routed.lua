-- The Trade-flow lens draws a flow ALONG THE ROUTE IT RIDES (Ben, 2026-10-26: "render the
-- paths from one market to another over the actual network ... not as arrows straight
-- from market centre to market centre"). LENSES.md § Trade-flow lens owns what it shows.
--
--   F1  Fixture: two markets on the home body whose cheapest haul for the player runs
--       land -> port -> sea -> port -> land (`verify.sea_route_fixture`, priced by the
--       trade pass's own leg - the sea_lane.lua fixture), and one manual trade on it.
--   F2  The flow under the lens. A REAL shipment if the trade pass ships one; on the
--       verify world it does not (the player nation's Logistic Point pool is empty, so
--       the shipment's LP draw refuses it, BL-1361), and then ONE record is staged with
--       the dispatcher's own pricing call (`verify.stage_trade_flow`: price_trade_leg,
--       the waypoints run_trades records - no good, credit or point moves). The lens
--       holds a ROUTE for the flow (convoy_route_tiles over those waypoints) and draws
--       the flow along it, not as the straight fallback arrow; the route crosses water.
--   F3  Captures: the whole body, the flow at its source market and at its loading and
--       unloading ports, mid and close, so the path is seen on the roads and the lane.
--
-- Runs on the default (unseated) verify world, as sea_lane.lua does.
-- Run with: ProjectIo --verify scripts/verify/trade_flow_routed.lua

local CARGO, QTY = "iron_ore", 20

verify.goto_surface("home")
verify.set_balance(1000000.0)

-- F1 ----------------------------------------------------------------------
local fx = verify.sea_route_fixture(CARGO, QTY, 3)
verify.expect(fx.ok, "F1: a market pair on the home body routes land -> port -> sea -> port -> land")
if not fx.ok then return end
print(string.format("[trade_flow_routed] markets %d -> %d, ports (%d,%d) / (%d,%d), %d seeded",
    fx.src, fx.dst, fx.port_a_x, fx.port_a_y, fx.port_b_x, fx.port_b_y, fx.seeded_ports))
-- A trade building makes points only when its upkeep is met (sea_lane.lua S2).
for _, m in ipairs(verify.markets_on_body(fx.body)) do
    for _, g in ipairs({ "charcoal", "coal", "timber", "stone" }) do
        verify.stock_market(m, g, 200.0)
    end
end
verify.set_trade_reserve(1000.0)
local r = verify.set_trade(CARGO, fx.src, fx.dst, 1000.0)
print("[trade_flow_routed] set_trade on the sea route: " .. tostring(r))
verify.econ_step(1) -- the trade pass, which may or may not ship it

-- F2 ----------------------------------------------------------------------
verify.set_overlay("trade_flow")
verify.frames(3)
local t0 = verify.trade_flow_routes()
print(string.format("[trade_flow_routed] after the trade pass: %d shipment(s) in the player's record, "
    .. "%d player convoy(s), trade points %.2f", t0.shipments, t0.convoys, verify.player_trade_points()))
if t0.shipments == 0 then
    local st = verify.stage_trade_flow(fx.src, fx.dst, CARGO, QTY)
    print(string.format("[trade_flow_routed] staged one record (dispatcher's price_trade_leg): ok %s, "
        .. "sea %s, ports %s, stations %s", tostring(st.ok), tostring(st.sea), tostring(st.ports),
        tostring(st.stations)))
    verify.expect(st.ok and st.ports, "F2: the staged record carries the priced leg's two ports")
    verify.frames(3)
end
local a, m, p = verify.trade_flow_counts()
local tr = verify.trade_flow_routes()
print(string.format("[trade_flow_routed] lens: arrows %d markers %d passes %d; routes %d held "
    .. "(%d tiles, %d over water, %d on rail); drawn routed %d, unrouted %d",
    a, m, p, tr.routes, tr.tiles, tr.over_water, tr.over_rail, tr.routed, tr.unrouted))
verify.expect(a > 0, "F2: the flow is on the lens (" .. a .. ")")
verify.expect(tr.routes > 0 and tr.routed > 0 and tr.unrouted == 0,
              "F2: the flow is drawn along its route, not the straight arrow (" .. tr.routed .. " routed, "
              .. tr.unrouted .. " unrouted)")
verify.expect(tr.over_water > 0, "F2: the route rides its sea leg (" .. tr.over_water .. " over water)")

-- F3 ----------------------------------------------------------------------
verify.clear_selection()
verify.mouse(-100, -100)
shot("trade_flow_routed_body")
if tr.has_first then
    print("[trade_flow_routed] first route " .. tr.first_len .. " tiles, from " .. tr.first_x .. "," .. tr.first_y)
    frame_tile(tr.first_x, tr.first_y, 4)
    shot("trade_flow_routed_source_mid")
    frame_tile(tr.first_x, tr.first_y, 10)
    shot("trade_flow_routed_source_close")
end
frame_tile(fx.port_a_x, fx.port_a_y, 4)
shot("trade_flow_routed_port_mid")
frame_tile(fx.port_a_x, fx.port_a_y, 10)
shot("trade_flow_routed_port_close")
frame_tile(fx.port_b_x, fx.port_b_y, 10)
shot("trade_flow_routed_far_port_close")
verify.set_overlay("none")
