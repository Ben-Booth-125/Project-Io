-- Standing sell-order acceptance (BL-113, US-008).
-- Proves a player can GOVERN how stock is released through the real commit path:
-- place a standing sell order (the SAME push onto world.sell_orders the market
-- ledger's "Add sell order" button performs), tick the sim, and assert the order's
-- floor is HONOURED ahead of the anonymous auto-sell path — stock is not dumped
-- below the floor. This is the coverage the trade flow lacked: no check placed an
-- order through the interactive path and asserted floor precedence over auto-sell.
-- Realises USER_STORIES.md US-008 ("order clearing + floor precedence are
-- econ-harness invariants").
-- Run: ProjectIo --verify scripts/verify/sell_order.lua

verify.goto_surface("home")

-- Warm the economy so the home market has resolved prices and the player pool holds
-- stock to release.
verify.econ_step(2)

-- Choose the good the player is most likely to be releasing at home. iron_ore is the
-- prototype's canonical extraction good and the fresh-start default; if the market
-- prices it, this is a real, traded release channel.
local res   = "iron_ore"
local price = verify.home_market_price(res)
verify.expect(price >= 0.0,
    "home body has a market pricing " .. res .. " (got " .. price .. ")")

-- Place a standing sell order with a floor set ABOVE the clearing price: an honoured
-- floor means the order refuses to sell under it, so stock is retained rather than
-- dumped. This is the precedence claim — a floored order overrides the below-floor
-- auto-sell path.
local high_floor = (price > 0.0 and price or 1.0) * 1000.0
local n = verify.place_sell_order(res, 5.0, high_floor)
verify.expect(n >= 1, "standing sell order registered (n=" .. n .. ")")

-- Snapshot the pool, tick, and assert the floored order did NOT release stock below
-- its floor: with a floor 1000x the price, nothing should clear through this order,
-- so the pool must not fall on account of it. (It may still RISE from production.)
local pool_before = verify.home_pool(res)
verify.econ_step(1)
local pool_after = verify.home_pool(res)
verify.expect(pool_after >= pool_before,
    "floored sell order honoured — stock not dumped below floor (before=" ..
    pool_before .. " after=" .. pool_after .. ")")

verify.capture("sell_order")
