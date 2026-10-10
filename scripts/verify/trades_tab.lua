-- The Market ledger's TRADES tab (BL-1269; docs/economy/TRADE.md): my trade
-- points and reserve, my manual trades through this market, every trade here,
-- what I could be moving, and what actually cleared.
--
-- THIS ASSERTS RATHER THAN CAPTURING, for the reason goods_table.lua gives:
-- `expect_no_clipping` records ZERO on this class of surface even over visibly
-- clipped frames (NR-663), so a picture alone proves nothing about the content.
-- Captures are kept at the end, but the verdict comes from the assertions.
--
-- What this holds down:
--
--   1. THE PRESSES REACH THE SEAM. The reserve is set by CLICKING the tab's own
--      "+" step and "Set reserve" button through the real input path (a review
--      found the button could never fire: the field re-synced on the mouse-down
--      and disabled it before release). Add (set_trade) and x (clear_trade) are
--      enqueued exactly as the tab's buttons enqueue them. All are applied by
--      app::render and land in WORLD state; the seam's answer is shown.
--   2. THE TRADE-POINTS LINE IS THE WORLD'S. made / reserve / auto are the
--      corporation record's, with the reserve clamped to what is made.
--   3. THE READS ARE DISTINCT, AND THE GATE ON READ 2 IS REAL - including SHUT.
--   4. AN ABSENT COUNTERPARTY IS THE MARKET; NO ROW PRINTS A PROFIT.
--   5. READ 3's ARITHMETIC AND RANKING.
--
-- The retired order book (sell orders, the order-close notice) is gone; its two
-- scripts (sell_order.lua, order_close_notice.lua) retired with it.

verify.goto_surface("home")
-- Enough clearing ticks that the exchange record has rows and prices have moved
-- apart across the body's markets. Without this every read below is vacuous.
verify.econ_step(16)
verify.show_panel("market", true)
verify.panel_view("market", 1) -- Trades
verify.frames(2)

-- BL-1265: under the shelf economy a market files an exchange only for a
-- landing or a shelf draw there, so the body's lowest-id market (the ledger's
-- default) can clear nothing at all. Aim the tab at the home-body market that
-- clears the most, so the history read below is about a market with history.
do
    local first = verify.trades_market()
    local best, best_rows = nil, -1
    for _, m in ipairs(verify.markets_on_body(first.body)) do
        local s = verify.exchange_ring_span(m)
        if s.rows_here > best_rows then best, best_rows = m, s.rows_here end
    end
    if best ~= nil then
        verify.select_market(best)
        verify.frames(2)
    end
end

local where = verify.trades_market()
verify.expect(where.market ~= nil and where.market ~= 0,
    "the Trades tab drew for a market (market " .. tostring(where.market) .. ")")
local here, home_body = where.market, where.body

-- =========================================================================
-- 2. THE TRADE-POINTS LINE IS THE WORLD'S
-- =========================================================================
local function near(a, b) return math.abs(a - b) <= math.max(1e-3, math.abs(b) * 1e-4) end

local pts = verify.trade_points_panel()
verify.expect(pts.drawn, "the Trades tab drew its trade-points line")
verify.expect(near(pts.made, verify.player_trade_points()),
    "points made match the corporation record (" .. pts.made .. " vs " .. verify.player_trade_points() .. ")")
print(string.format("MEASURED trade points: made %.2f, reserve %.2f (used %.2f), auto %.2f",
    pts.made, pts.reserve, pts.reserve_used, pts.auto))

-- The tab's own reserve control, pressed with the mouse: three clicks on the
-- field's "+" step (1.0 each), then "Set reserve".
local r0 = pts.reserve
verify.expect(not pts.set_enabled, "'Set reserve' is idle while the field equals the reserve")
verify.expect(pts.plus_x > 0 and pts.set_x > 0, "the reserve control published its press points")
for _ = 1, 3 do
    verify.click(pts.plus_x, pts.plus_y)
    verify.frames(2)
end
pts = verify.trade_points_panel()
verify.expect(near(pts.edit, r0 + 3.0), "the '+' step edited the field (" .. pts.edit .. ")")
verify.expect(pts.set_enabled, "'Set reserve' is pressable once the field differs")
verify.expect(near(pts.reserve, r0), "editing alone sends nothing (" .. pts.reserve .. ")")
verify.click(pts.set_x, pts.set_y)
verify.frames(3) -- press frame, drain frame, redraw frame
pts = verify.trade_points_panel()
verify.expect(near(pts.reserve, r0 + 3.0),
    "the 'Set reserve' CLICK set the reserve (" .. pts.reserve .. ", answer: " .. verify.trade_message() .. ")")
verify.expect(near(pts.edit, pts.reserve) and not pts.set_enabled,
    "the field re-syncs to the world once the press lands")
verify.expect(near(pts.reserve_used, math.min(pts.reserve, pts.made)),
    "the reserve is clamped to what is made (" .. pts.reserve_used .. ")")
verify.expect(near(pts.auto, math.max(0.0, pts.made - pts.reserve_used)),
    "auto is the rest (" .. pts.auto .. ")")
verify.expect(not verify.set_trade_reserve(-1.0), "a negative reserve is refused")

-- =========================================================================
-- 1. THE PRESSES REACH THE SEAM
-- =========================================================================
-- A route to try: the best potential trade from here if any (it is a good trade
-- carries, traded at both ends), else any other market on the body.
local pot = verify.potential_trades()
local candidates = {}
for _, r in ipairs(pot) do candidates[#candidates + 1] = { key = r.good_key, to = r.to_market } end
local others = {}
for _, m in ipairs(verify.markets_on_body(home_body)) do if m ~= here then others[#others + 1] = m end end
if #others > 0 then
    for _, key in ipairs({ "iron_ore", "copper_ore", "agricultural_produce", "water", "steel", "refined_fuel" }) do
        candidates[#candidates + 1] = { key = key, to = others[1] }
    end
end
verify.expect(#candidates > 0, "a route to trade on exists from this market")

local before = #verify.world_player_trades()
local placed = nil
for _, c in ipairs(candidates) do
    verify.trades_tab_add(c.key, here, c.to, 2.0)
    verify.frames(2)
    if verify.trade_message() == "Trade set." then placed = c; break end
end
verify.expect(placed ~= nil, "the Add press set a trade (last answer: " .. verify.trade_message() .. ")")
print("MEASURED: placed " .. tostring(placed and placed.key) .. " -> market " .. tostring(placed and placed.to))

local wt = verify.world_player_trades()
verify.expect(#wt == before + 1, "the world holds one more trade of mine (" .. #wt .. " vs " .. before .. ")")
local placed_id = wt[#wt] and wt[#wt].trade_id
local mine = verify.my_trades()
local drawn = nil
for _, r in ipairs(mine) do if r.trade_id == placed_id then drawn = r end end
verify.expect(drawn ~= nil, "'My trades' draws the trade the world holds (id " .. tostring(placed_id) .. ")")
if drawn then
    verify.expect(drawn.mine and drawn.from_market == here and drawn.to_market == placed.to,
        "the row is mine and runs from this market to the one named")
    verify.expect(drawn.good_key == placed.key, "the row's good is the one named (" .. drawn.good_key .. ")")
    verify.expect(near(drawn.points, 2.0), "the row carries the points set (" .. drawn.points .. ")")
    verify.expect(drawn.units > 0.0, "the row ships a positive amount a tick (" .. drawn.units .. ")")
    verify.expect(drawn.from ~= "" and drawn.to ~= "", "both ends are named by city")
    print(string.format("MEASURED row: %s %s > %s, %.1f pts = %.1f units/tick",
        drawn.good, drawn.from, drawn.to, drawn.points, drawn.units))
end
pts = verify.trade_points_panel()
verify.expect(pts.manual_count == #wt, "the line counts my trades (" .. pts.manual_count .. ")")

-- A refused press says why and mutates nothing: a market to itself.
verify.trades_tab_add(placed and placed.key or "iron_ore", here, here, 1.0)
verify.frames(2)
verify.expect(verify.trade_message() ~= "Trade set.", "a trade to its own market is refused: " .. verify.trade_message())
verify.expect(#verify.world_player_trades() == #wt, "the refusal mutated nothing")

-- Remove it through the row's x.
if placed_id then
    verify.trades_tab_remove(placed_id)
    verify.frames(2)
    verify.expect(verify.trade_message() == "Trade removed.", "the x press removed it: " .. verify.trade_message())
    local still = false
    for _, r in ipairs(verify.my_trades()) do if r.trade_id == placed_id then still = true end end
    verify.expect(not still, "'My trades' no longer draws it")
    verify.expect(#verify.world_player_trades() == before, "the world holds it no longer")
end

-- Put one back for the gate checks below: the gate must hide something real.
if placed then
    verify.trades_tab_add(placed.key, here, placed.to, 1.0)
    verify.frames(2)
end

-- =========================================================================
-- 3. THE READS ARE DISTINCT, AND THE GATE IS REAL
-- =========================================================================
mine = verify.my_trades()
local all = verify.market_trades()
local hist = verify.trade_history()
pot = verify.potential_trades()
print(string.format("MEASURED Trades tab: %d mine, %d here (gate %s), %d potential, %d history",
    #mine, #all.rows, tostring(all.open), #pot, #hist))

local not_mine = 0
for _, r in ipairs(mine) do if not r.mine then not_mine = not_mine + 1 end end
verify.expect(not_mine == 0, "every row of 'my trades' belongs to the player (" .. not_mine .. " do not)")

local touches = 0
for _, r in ipairs(mine) do if r.from_market ~= here and r.to_market ~= here then touches = touches + 1 end end
verify.expect(touches == 0, "every row of 'my trades' leaves or lands on this market")

verify.expect(all.open == verify.player_operates_on(home_body),
    "the gate agrees with the world on the home body (open=" .. tostring(all.open) .. ")")
if all.open then
    local in_all = {}
    for _, r in ipairs(all.rows) do in_all[r.trade_id] = true end
    local missing = 0
    for _, r in ipairs(mine) do if not in_all[r.trade_id] then missing = missing + 1 end end
    verify.expect(missing == 0, "every trade in 'my trades' also stands in 'all trades here' (" .. missing .. " missing)")
end

-- Read 3 shares no identity with the lists: a potential trade has no id.
local pot_shape = 0
for _, r in ipairs(pot) do
    if r.trade_id ~= nil then pot_shape = pot_shape + 1 end
    if r.buy_price == nil or r.sell_price == nil or r.haulage == nil then pot_shape = pot_shape + 1 end
end
verify.expect(pot_shape == 0, "a potential trade is a derivation: no id, and all three terms present")

-- =========================================================================
-- 4. AN ABSENT COUNTERPARTY IS THE MARKET; NO ROW PRINTS A PROFIT
-- =========================================================================
verify.expect(verify.world_exchange_count() > 0,
    "the clearing tick filed exchanges at all (" .. verify.world_exchange_count() .. " in the ring)")
local span = verify.exchange_ring_span(here)
print(string.format("MEASURED exchange ring: %d rows, ticks %d..%d, %d name this market",
    span.rows, span.oldest_tick, span.newest_tick, span.rows_here))
verify.expect(#hist > 0, "the history section drew rows for this market (" .. #hist .. ")")
verify.expect(#hist == math.min(span.rows_here, 120),
    "the history section drew every ring row for this market, to its cap (" .. #hist .. " of " .. span.rows_here .. ")")
local blank_side, market_sides, profit_field, bad_revenue = 0, 0, 0, 0
for _, r in ipairs(hist) do
    if r.seller_is_market then market_sides = market_sides + 1; if r.seller ~= "Market" then blank_side = blank_side + 1 end end
    if r.buyer_is_market then market_sides = market_sides + 1; if r.buyer ~= "Market" then blank_side = blank_side + 1 end end
    if r.seller == nil or r.seller == "" then blank_side = blank_side + 1 end
    if r.buyer == nil or r.buyer == "" then blank_side = blank_side + 1 end
    if r.profit ~= nil or r.margin ~= nil then profit_field = profit_field + 1 end
    local want = r.quantity * r.unit_price
    if math.abs(r.revenue - want) > math.max(1e-3, math.abs(want) * 1e-5) then bad_revenue = bad_revenue + 1 end
end
verify.expect(blank_side == 0, "every counterparty renders as a name or as the market (" .. blank_side .. " bad)")
verify.expect(market_sides > 0, "the market-as-counterparty rows are KEPT (" .. market_sides .. " sides)")
verify.expect(profit_field == 0, "no history row carries a profit or margin field")
verify.expect(bad_revenue == 0, "revenue is quantity * unit_price and nothing else (" .. bad_revenue .. " wrong)")

-- =========================================================================
-- 5. READ 3's ARITHMETIC AND ITS RANKING
-- =========================================================================
print("MEASURED potential trades: " .. #pot .. " rows")
if #pot > 0 then
    local bad_margin, out_of_order, non_positive = 0, 0, 0
    local prev = nil
    for _, r in ipairs(pot) do
        local want = r.sell_price - r.buy_price - r.haulage
        if math.abs(r.margin - want) > math.max(1e-3, math.abs(want) * 1e-4) then bad_margin = bad_margin + 1 end
        if r.margin <= 0.0 then non_positive = non_positive + 1 end
        if prev ~= nil and r.margin_per_point > prev + 1e-4 then out_of_order = out_of_order + 1 end
        prev = r.margin_per_point
    end
    verify.expect(bad_margin == 0, "margin is sell there - buy here - haulage (" .. bad_margin .. " wrong)")
    verify.expect(out_of_order == 0, "potential trades are in the trade pass's order: margin per point, best first")
    verify.expect(non_positive == 0, "a listed potential trade clears its haulage")
else
    print("MEASURED: no potential trade clears its haulage from this market")
end

-- =========================================================================
-- CAPTURES, at 1920x1080 (the screen being reviewed).
-- =========================================================================
verify.window(1920, 1080)
verify.frames(2)
verify.capture("trades_tab_head")
verify.scroll_panel("market_trades", 1.0)
verify.frames(2)
verify.expect_scrolled("the Trades tab's scroll request reached a real scroller")
verify.capture("trades_tab_foot")
verify.scroll_panel("market_trades", 0.0)
verify.frames(2)
verify.panel_view("market", 0)
verify.frames(2)
verify.capture("trades_tab_strip_goods")
verify.panel_view("market", 1)
verify.frames(2)
verify.capture("trades_tab_strip_trades")

-- =========================================================================
-- 6. THE SHUT HALF OF THE GATE - last, because it demolishes the estate
-- =========================================================================
local player_here = {}
for _, b in ipairs(verify.buildings()) do
    if b.player and b.body == home_body then player_here[#player_here + 1] = b.id end
end
verify.expect(#player_here > 0, "the player holds buildings on this body before the demolition (" .. #player_here .. ")")
for _, bid in ipairs(player_here) do
    verify.corp_command{ verb = 1, subject = bid } -- 1 == corp_verb::demolish
end
verify.frames(2)
verify.expect(verify.player_operates_on(home_body) == false,
    "the world agrees the player now owns nothing on this body")
local shut = verify.market_trades()
verify.expect(shut.open == false, "THE GATE SHUTS once the player owns no building on the body")
verify.expect(#shut.rows == 0, "a shut gate lists no rows (" .. #shut.rows .. " listed)")
-- And it is a GATE, not an empty list: the world still holds the trade.
local still = verify.world_orders_on_body(home_body)
print(string.format("MEASURED shut gate: world still holds %d trades out / %d in on the body; surface shows %d",
    still.sells, still.buys, #shut.rows))
verify.expect(still.sells + still.buys > 0, "the trades the gate hides are still there")
verify.capture("trades_gate_shut")
