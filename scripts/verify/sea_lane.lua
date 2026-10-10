-- A sea-route convoy is drawn along its legs (sprint 50; SUPPLY.md § Logistical
-- cost, "The lane is the legs"; BL-1195). Scripted in place of a live click on
-- Ben's request, 2026-10-10.
--
-- A haul that crosses water is priced land -> port -> sea -> port -> land, so its
-- lane on the Planetary canvas is those three legs end to end — not the direct
-- centre-to-centre path — and its head moves on the lane's clock: each leg at its
-- own speed, quick on the sea leg and slow over land.
--
--   S1  Fixture: two markets on Kepler whose cheapest haul for the player runs by
--       sea with at least MIN_LAND land hops at each end (`verify.sea_route_fixture`,
--       priced by the trade pass's own price_market_leg; it lays two Ports only
--       where the world's own Ports give no such pair).
--   S2  A real manual TRADE (set_trade, BL-1266; the retired dispatch_convoy
--       verb's successor) ships the fixture's goods off the source market's
--       shelf: one econ tick's trade pass puts a convoy on the route, in sea
--       mode, through the fixture's two Ports. The fixture's Ports make the
--       trade points the trade spends; the reserve holds them for it.
--   S3  The lane THE CANVAS DREW (`verify.convoy_lane`, the vision pass's own
--       beam) passes through both Ports, carries water only on its sea leg, and
--       is not the direct centre-to-centre path.
--   S4  The lane's clock spends less time per tile on the sea leg than on either
--       land leg; and the head THE CANVAS PLACES, sampled at even steps of travel
--       time across the whole lane, covers more tiles per step at sea than on
--       land. Sampled by parking the convoy's progress (`set_convoy_progress`):
--       the shipment's own travel time is a couple of econ ticks, too coarse to
--       step through. One real econ tick then shows the sim moving the head.
--   S5  Captures: the canvas with the head mid-land-leg and mid-sea-leg.
--
-- Nothing here changes the game; the fixture is verify-only.

local VERB_CLEAR_TRADE = 31 -- corp_verb::clear_trade
local CARGO, QTY = "iron_ore", 20

verify.goto_surface("home") -- Kepler; the verify world names it generatively
verify.set_balance(1000000.0)

-- --- S1: the fixture -------------------------------------------------------
local MIN_LAND = 3 -- land hops each end, so the head can be framed on land
local fx = verify.sea_route_fixture(CARGO, QTY, MIN_LAND)
verify.expect(fx.ok, "S1 a market pair on Kepler routes land -> port -> sea -> port -> land")
if not fx.ok then return end
print(string.format("sea_lane: markets %d -> %d, ports (%d,%d) / (%d,%d), %d seeded",
    fx.src, fx.dst, fx.port_a_x, fx.port_a_y, fx.port_b_x, fx.port_b_y, fx.seeded_ports))

-- --- S2: the trade's shipment --------------------------------------------
-- Reserve every point the player makes for manual trades, and set one trade on
-- the fixture's route with enough points to carry the whole seeded cargo.
-- A trade building makes points only when its upkeep (fuel and building
-- materials, bought off its own market's shelf) is met, and the verify world's
-- shelves may be bare of them: stock every market on the body with both bands'
-- upkeep goods, so the fixture's Ports run (TRADE.md § The Planetary Marketplace).
for _, m in ipairs(verify.markets_on_body(fx.body)) do
    for _, g in ipairs({ "charcoal", "coal", "timber", "stone" }) do
        verify.stock_market(m, g, 200.0)
    end
end
-- The player's trade points come from Ports it owns: the fixture's own seeded
-- pair when it had to lay one (seeded_ports > 0); a pair the world already had
-- may belong to anyone.
print("sea_lane: fixture seeded " .. fx.seeded_ports .. " Port(s) for the player")
verify.expect(verify.set_trade_reserve(1000.0), "S2 the trade reserve is set")
local r = verify.set_trade(CARGO, fx.src, fx.dst, 1000.0)
verify.expect(r == "applied", "S2 set_trade onto the sea route: " .. r)
if r ~= "applied" then return end
verify.econ_step(1) -- the trade pass ships it
print(string.format("sea_lane: the player made %.2f trade points this tick", verify.player_trade_points()))
verify.expect(verify.player_trade_points() > 0.0,
    "S2 the player makes trade points to ship with (the fixture's Ports)")

local cv
for _, c in ipairs(verify.convoys()) do
    if c.src == fx.src and c.dst == fx.dst and not c.arrived and cv == nil then cv = c end
end
verify.expect(cv ~= nil, "S2 the trade's shipment is in flight")
if not cv then return end
-- One shipment is the subject; stop the trade so later econ ticks add no more.
for _, t in ipairs(verify.world_player_trades()) do
    if t.from_market == fx.src and t.to_market == fx.dst then
        verify.corp_command{ verb = VERB_CLEAR_TRADE, order = t.trade_id }
    end
end
verify.expect(cv.mode == "sea", "S2 the convoy travels in sea mode: " .. cv.mode)
verify.expect(cv.port_a == fx.port_a and cv.port_b == fx.port_b,
    "S2 the convoy carries the fixture's two Ports")

-- --- S3: the drawn lane ----------------------------------------------------
verify.frames(2)
local L = verify.convoy_lane(cv.id)
verify.expect(L.found and L.drawn, "S3 the canvas drew a beam on the convoy's own route")
if not (L.found and L.drawn) then return end
print(string.format("sea_lane: lane %d tiles, port A at %d, port B at %d, water sea=%d land=%d, "
    .. "direct path %d tiles", L.n, L.port_a_idx, L.port_b_idx, L.water_sea, L.water_land, L.direct_n))
verify.expect(L.port_a_idx > 0 and L.port_b_idx > L.port_a_idx,
    "S3 the drawn lane passes through port A, then port B")
verify.expect(L.water_sea > 0, "S3 the sea leg crosses water: " .. L.water_sea .. " tiles")
verify.expect(L.water_land == 0, "S3 the land legs carry no water: " .. L.water_land .. " tiles")
verify.expect(not L.direct_same, "S3 the drawn lane is not the direct centre-to-centre path")

-- --- S4a: the clock --------------------------------------------------------
local function per_tile(a, b) -- clock time per tile hop between lane indices a < b
    if b <= a then return nil end
    return (L.at[b] - L.at[a]) / (b - a)
end
local land_out = per_tile(1, L.port_a_idx)
local sea      = per_tile(L.port_a_idx, L.port_b_idx)
local land_in  = per_tile(L.port_b_idx, L.n)
print(string.format("sea_lane: clock per tile - land out %s, sea %s, land in %s",
    tostring(land_out), tostring(sea), tostring(land_in)))
verify.expect(sea ~= nil and (land_out ~= nil or land_in ~= nil),
    "S4 the lane has a sea leg and at least one land leg")
if land_out then verify.expect(sea < land_out, "S4 the clock runs a sea tile faster than an outbound land tile") end
if land_in  then verify.expect(sea < land_in,  "S4 the clock runs a sea tile faster than an inbound land tile") end

-- --- S4b / S5: the head across the lane ----------------------------------
verify.clear_selection() -- the captures frame the canvas, not a stale tile card
local STEPS = 40
local moved = { land = 0, sea = 0 }
local steps = { land = 0, sea = 0 }
local captured = { land = false, sea = false }
local prev
for k = 0, STEPS - 1 do
    verify.expect(verify.set_convoy_progress(cv.id, k / STEPS), "S4 parked the convoy at " .. k / STEPS)
    verify.frames(1)
    local now = verify.convoy_lane(cv.id)
    if not (now.found and now.drawn) then
        verify.expect(false, "S4 the canvas kept drawing the lane at progress " .. k / STEPS)
        break
    end
    verify.expect(now.n == L.n, "S4 the drawn lane is stable while the head moves")
    if prev then
        -- Attribute the step to the leg the head STARTED it on.
        local leg = (prev.head_leg == "sea") and "sea" or "land"
        moved[leg] = moved[leg] + (now.head - prev.head)
        steps[leg] = steps[leg] + 1
        verify.expect(now.head >= prev.head, "S4 the head never runs backwards")
    end
    local now_leg = (now.head_leg == "sea") and "sea" or "land"
    -- Mid-leg: strictly inside the leg, not on a Port or a lane end.
    local inside = (now_leg == "sea" and now.head > now.port_a_idx and now.head < now.port_b_idx)
                or (now_leg == "land" and now.head > 1 and now.head < now.port_a_idx)
    if inside and not captured[now_leg] then
        print(string.format("sea_lane: capture %s at progress %.3f head %d/%d (%d,%d)",
            now_leg, now.progress, now.head, now.n, now.head_x, now.head_y))
        verify.center_tile(now.head_x, now.head_y, 4.0)
        verify.frames(1)
        verify.capture("sea_lane_head_" .. now_leg)
        captured[now_leg] = true
    end
    prev = now
end
verify.expect(captured.land, "S5 captured the head mid-land-leg")
verify.expect(captured.sea,  "S5 captured the head mid-sea-leg")
print(string.format("sea_lane: head moved %d tiles over %d land steps, %d tiles over %d sea steps",
    moved.land, steps.land, moved.sea, steps.sea))
verify.expect(steps.sea > 0 and steps.land > 0, "S4 the head spent steps on both kinds of leg")
if steps.sea > 0 and steps.land > 0 then
    verify.expect(moved.sea / steps.sea > moved.land / steps.land,
        string.format("S4 the head is faster at sea (%.2f tiles/step) than on land (%.2f)",
            moved.sea / steps.sea, moved.land / steps.land))
end

-- One REAL econ tick from the start: the sim moves the convoy, and the canvas's
-- head follows it forward along the same lane.
verify.set_convoy_progress(cv.id, 0.0)
verify.frames(1)
verify.econ_step(1)
verify.frames(1)
local after = verify.convoy_lane(cv.id)
if after.found and after.drawn then
    print(string.format("sea_lane: one econ tick -> progress %.3f head %d/%d on %s",
        after.progress, after.head, after.n, after.head_leg))
    verify.expect(after.progress > 0 and after.head > 1, "S4 one econ tick moves the drawn head forward")
else
    print("sea_lane: the convoy arrived inside one econ tick")
    verify.expect(after.found == false, "S4 one econ tick moves the convoy (it arrived)")
end
