-- BL-1202 (order close notice): the player SEES when one of their standing sell
-- orders closes itself.
--
-- MARKETS.md step 4: an order whose pool stands empty for
-- `sell_order_empty_close_ticks` (4) quarters is removed by the clearing pass and
-- the good returns to auto-surplus. The pass logs the close as an agency-topic
-- history line; the Market Ledger's Trades tab reads it back under My trades as
-- a "Closed" table (Closed . Good . Floor, the log line on hover).
--
-- The check: place an order on a good the player holds NONE of on the home body
-- (so its pool is empty from the first tick), run five quarters, and assert
--   1. the order left the book (My trades no longer lists it), and
--   2. the Trades tab drew a closed row naming THAT order, its good and floor.
-- Asserts rather than relies on the capture: a picture of an empty section and a
-- picture of a missing one look the same.
--
-- Run: ProjectIo --verify scripts/verify/order_close_notice.lua

verify.goto_surface("home")
verify.econ_step(2)

-- A good the home market prices but the player's pool holds none of. Late-chain
-- goods first: a fresh start does not make them, so the pool stays empty.
local candidates = {
    "spacecraft_components", "ordnance", "medical_supplies", "ree_alloy",
    "electronics", "platinum_group_metals", "propellant", "machinery",
    "alloys", "consumer_goods", "furs", "spices", "coffee", "tobacco",
}
local res, price = nil, 0.0
for _, c in ipairs(candidates) do
    local p = verify.home_market_price(c)
    if p > 0.0 and verify.home_pool(c) <= 0.0 then
        res, price = c, p
        break
    end
end
verify.expect(res ~= nil, "found a priced good with an empty home pool (" .. tostring(res) .. ")")
if res == nil then return end

-- Quantity 0 = no cap (BL-1201). A floor at twice the price, so the Floor
-- column has a distinctive figure to match.
local floor = price * 2.0
local n = verify.place_sell_order(res, 0.0, floor)
verify.expect(n >= 1, "standing sell order registered (n=" .. n .. ")")

verify.show_panel("market", true)
verify.panel_view("market", 1) -- Trades
verify.frames(2)

-- The order's id, read off the surface (the newest of my orders on this good).
local where = verify.trades_market()
local order_id = 0
for _, r in ipairs(verify.my_trades()) do
    if not r.is_buy and r.order_id > order_id and math.abs(r.limit - floor) < 0.01 then
        order_id = r.order_id
    end
end
verify.expect(order_id ~= 0, "the new order is in My trades (id " .. order_id ..
    ", market " .. tostring(where.market) .. ")")

-- Five quarters: the 4th consecutive empty tick closes it.
verify.econ_step(5)
verify.frames(2)

local still = false
for _, r in ipairs(verify.my_trades()) do
    if r.order_id == order_id then still = true end
end
verify.expect(not still, "order #" .. order_id .. " closed itself after 4 empty quarters")

local closed = verify.closed_trades()
print(string.format("MEASURED closed orders drawn: %d", #closed))
local hit = nil
for _, r in ipairs(closed) do
    print(string.format("  #%d good=%s floor=%.2f known=%s day=%d :: %s",
        r.order_id, r.good, r.floor, tostring(r.good_known), r.day, r.why))
    if r.order_id == order_id then hit = r end
end
verify.expect(hit ~= nil, "the Trades tab draws a closed row for order #" .. order_id)
if hit then
    verify.expect(hit.good_known, "the closed row names its good (tag parsed)")
    verify.expect(math.abs(hit.floor - floor) < 0.01,
        string.format("the closed row carries the floor (%.2f drawn vs %.2f placed)", hit.floor, floor))
    verify.expect(string.find(hit.why, "nothing to sell", 1, true) ~= nil,
        "the closed row carries the why on hover")
end

verify.capture("order_close_notice")
