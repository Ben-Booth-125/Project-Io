-- The Market ledger's TRADES tab: my positions, the market's positions, what I
-- could be doing, and what actually moved.
--
-- THIS ASSERTS RATHER THAN CAPTURING, for the reason goods_table.lua gives:
-- `expect_no_clipping` records ZERO on this class of surface even over visibly
-- clipped frames (NR-663), so a picture alone proves nothing about the content.
-- Captures are kept at the end, but the verdict comes from the assertions.
--
-- The four things this exists to hold down, all of them things the design says
-- explicitly and none of them visible in a screenshot:
--
--   1. THE THREE READS ARE DISTINCT. My book, the whole book, and a derivation
--      are not one table and must not become one.
--   2. THE GATE ON READ 2 IS REAL. "The player owns a building on that body"
--      (Ben, 2026-08-29, over three alternatives) must actually exclude a body
--      where the player owns nothing. A gate that is assumed is not a gate.
--   3. AN ABSENT COUNTERPARTY IS THE MARKET, not "unknown". Three of the four
--      clearing paths trade against the market and carry the volume.
--   4. NO ROW PRINTS A PROFIT. There is no cost basis anywhere in the model, so
--      a margin is not derivable from a sale; `quantity * unit_price` is the
--      only honest figure and the column is REVENUE.

verify.goto_surface("home")
-- Enough clearing ticks that the exchange record has rows and prices have moved
-- apart across the body's markets. Without this every read below is vacuous.
verify.econ_step(16)
verify.show_panel("market", true)
verify.panel_view("market", 1) -- Trades
verify.frames(2)

local where = verify.trades_market()
verify.expect(where.market ~= nil and where.market ~= 0,
    "the Trades tab drew for a market (market " .. tostring(where.market) .. ")")
local home_body = where.body

-- =========================================================================
-- 1. THE THREE READS ARE DISTINCT
-- =========================================================================
-- Not "three sections exist" - three DIFFERENT populations. Read 1 is a filter
-- on read 2; read 3 shares no row with either, because it is a derivation and
-- not a record at all.

local mine = verify.my_trades()
local all  = verify.market_trades()
local pot  = verify.potential_trades()
local hist = verify.trade_history()

print(string.format("MEASURED Trades tab: %d mine, %d in the book (gate %s), %d potential, %d history",
    #mine, #all.rows, tostring(all.open), #pot, #hist))

-- Every row of read 1 says so; no row of read 2 is silently mine-only.
local mine_not_mine = 0
for _, r in ipairs(mine) do if not r.mine then mine_not_mine = mine_not_mine + 1 end end
verify.expect(mine_not_mine == 0,
    "every row of 'my trades' belongs to the player (" .. mine_not_mine .. " do not)")

-- Read 1 is a SUBSET of read 2 by order id, and read 2 is strictly the wider
-- read. If they were the same population the design's distinction would be a
-- pair of headings over one list.
if all.open then
    local in_all = {}
    for _, r in ipairs(all.rows) do in_all[r.order_id] = true end
    local missing = 0
    for _, r in ipairs(mine) do if not in_all[r.order_id] then missing = missing + 1 end end
    verify.expect(missing == 0,
        "every order in 'my trades' also stands in the market's book (" .. missing .. " missing)")
    verify.expect(#all.rows >= #mine,
        "the market's book is not narrower than my own (" .. #all.rows .. " vs " .. #mine .. ")")

    -- AND IT MATCHES THE WORLD, not just itself. This is the read that would
    -- pass against a table that drew nothing if it only compared the surface to
    -- the surface.
    local w = verify.world_orders_on_body(home_body)
    verify.expect(#all.rows == w.sells + w.buys,
        "the book lists every standing order on the body: " .. #all.rows ..
        " drawn vs " .. (w.sells + w.buys) .. " in world state")
    verify.expect(#mine == w.mine,
        "'my trades' lists exactly my standing orders: " .. #mine .. " drawn vs " ..
        w.mine .. " in world state")
end

-- Read 3 shares no identity with the books: a potential trade has no order id,
-- because nobody has placed it. Assert it carries the derivation's own fields
-- instead, which is what makes it a different KIND of row.
local pot_shape = 0
for _, r in ipairs(pot) do
    if r.order_id ~= nil then pot_shape = pot_shape + 1 end
    if r.buy_price == nil or r.sell_price == nil or r.haulage == nil then
        pot_shape = pot_shape + 1
    end
end
verify.expect(pot_shape == 0,
    "a potential trade is a derivation, not an order: no id, and all three terms present")

-- =========================================================================
-- 2. THE GATE ON READ 2 IS REAL
-- =========================================================================
-- The surface's own answer must equal the world's, and it must actually SHUT
-- somewhere. A gate asserted only where it is open has not been tested.

verify.expect(all.open == verify.player_operates_on(home_body),
    "the book's gate agrees with the world on the home body (open=" ..
    tostring(all.open) .. ")")

local bodies = verify.market_bodies()
local shut_body, shut_market, shut_name = nil, nil, nil
for _, b in ipairs(bodies) do
    if not b.operates and b.market ~= nil and b.market ~= 0 then
        shut_body, shut_market, shut_name = b.body, b.market, b.name
        break
    end
end
print("MEASURED market-bearing bodies: " .. #bodies)

if shut_body ~= nil then
    -- The easy route, when the fixture offers it: point the selectors at a body
    -- the player has no building on and read the gate there.
    verify.select_market(shut_market)
    verify.frames(2)
    local away = verify.market_trades()
    local away_where = verify.trades_market()
    verify.expect(away_where.body == shut_body,
        "the selectors moved to " .. tostring(shut_name) .. " (body " .. shut_body .. ")")
    verify.expect(away.open == false,
        "the gate SHUTS on a body where the player owns no building (" .. tostring(shut_name) .. ")")
    verify.expect(#away.rows == 0,
        "a shut gate lists no rows (" .. #away.rows .. " listed)")
    verify.capture("trades_gate_shut")
    verify.select_market(where.market)
    verify.frames(2)
else
    -- THE FIXTURE HAS ONLY ONE MARKET-BEARING BODY, AND THE PLAYER OPERATES ON
    -- IT. Measured, not assumed: markets are seeded on the home body only and an
    -- off-world one emerges when a building COMPLETES there, and none does -
    -- still one market body after 400 econ ticks (a hundred years). So the shut
    -- half is unreachable by re-pointing the selectors, in this fixture, ever.
    --
    -- It is tested at the FOOT of this script instead, by taking the player's
    -- estate off the body through the real `demolish` verb - see § 6. Weakening
    -- the assertion to "the gate agrees with the world (true == true)" would be
    -- a check that cannot fail, which is what this file exists not to be.
    print("MEASURED: no second market-bearing body - the SHUT half of the gate " ..
          "is tested by demolition at the foot of this script")
end

-- =========================================================================
-- 3. AN ABSENT COUNTERPARTY IS THE MARKET
-- =========================================================================

hist = verify.trade_history()
verify.expect(verify.world_exchange_count() > 0,
    "the clearing tick filed exchanges at all (" .. verify.world_exchange_count() .. " in the ring)")
verify.expect(#hist > 0,
    "the history section drew rows for this market (" .. #hist .. ")")

local blank_side, market_sides = 0, 0
for _, r in ipairs(hist) do
    if r.seller_is_market then
        market_sides = market_sides + 1
        if r.seller ~= "Market" then blank_side = blank_side + 1 end
    end
    if r.buyer_is_market then
        market_sides = market_sides + 1
        if r.buyer ~= "Market" then blank_side = blank_side + 1 end
    end
    -- Nothing may render as unknown, ever: a null side is the market and a
    -- non-null side is a corp that exists.
    if r.seller == nil or r.seller == "" then blank_side = blank_side + 1 end
    if r.buyer  == nil or r.buyer  == "" then blank_side = blank_side + 1 end
end
print("MEASURED: " .. market_sides .. " of " .. (#hist * 2) ..
      " counterparty sides are the market itself")
verify.expect(blank_side == 0,
    "every counterparty renders as a name or as the market, never blank (" .. blank_side .. " bad)")
verify.expect(market_sides > 0,
    "the market-as-counterparty rows are KEPT, not skipped (" .. market_sides .. " sides)")

-- =========================================================================
-- 4. NO ROW PRINTS A PROFIT
-- =========================================================================
-- The strong form: the record has no profit FIELD, so nothing downstream can
-- print one by accident, and the revenue it does carry is exactly the term the
-- clearing statement accrued.

local profit_field, bad_revenue, worst = 0, 0, 0.0
for _, r in ipairs(hist) do
    if r.profit ~= nil or r.margin ~= nil then profit_field = profit_field + 1 end
    local want = r.quantity * r.unit_price
    local err  = math.abs(r.revenue - want)
    if err > worst then worst = err end
    if err > math.max(1e-3, math.abs(want) * 1e-5) then bad_revenue = bad_revenue + 1 end
end
verify.expect(profit_field == 0,
    "no history row carries a profit or margin field (" .. profit_field .. " do)")
verify.expect(bad_revenue == 0,
    "revenue is quantity * unit_price and nothing else (" .. bad_revenue ..
    " wrong, worst err " .. string.format("%.6f", worst) .. ")")

-- =========================================================================
-- 5. READ 3's ARITHMETIC AND ITS RANKING
-- =========================================================================
-- Ranking is permitted HERE and only here (CONCEPT.md § Player identity, and
-- Ben's 2026-08-29 qualification: the top row is one input among several, not
-- the move itself). So the ordering is a contract, and it is checked.

pot = verify.potential_trades()
print("MEASURED potential trades: " .. #pot .. " rows")
if #pot > 0 then
    local bad_margin, out_of_order, non_positive = 0, 0, 0
    local prev = nil
    for _, r in ipairs(pot) do
        local want = r.sell_price - r.buy_price - r.haulage
        if math.abs(r.margin - want) > math.max(1e-3, math.abs(want) * 1e-4) then
            bad_margin = bad_margin + 1
        end
        if r.margin <= 0.0 then non_positive = non_positive + 1 end
        if prev ~= nil and r.margin > prev + 1e-4 then out_of_order = out_of_order + 1 end
        prev = r.margin
    end
    verify.expect(bad_margin == 0,
        "margin is sell there - buy here - haulage (" .. bad_margin .. " wrong)")
    verify.expect(out_of_order == 0,
        "potential trades are ranked by margin, best first (" .. out_of_order .. " out of order)")
    verify.expect(non_positive == 0,
        "a listed potential trade clears its haulage (" .. non_positive .. " do not)")
    print(string.format("MEASURED best potential trade: %s to %s, buy %.2f sell %.2f haul %.2f margin %.2f in %d qtr",
        pot[1].good, pot[1].to, pot[1].buy_price, pot[1].sell_price,
        pot[1].haulage, pot[1].margin, pot[1].travel_ticks))
else
    -- Not a failure: a market where no route clears its haulage is a real state
    -- and the section says so. But it is REPORTED, because a silent zero here
    -- would let a broken derivation read as an honest empty one.
    print("MEASURED: no potential trade clears its haulage from this market")
end

-- =========================================================================
-- CAPTURES. At 1920x1080, the screen being reviewed - a density judgement taken
-- at 720p is taken against half the content height (NR-719's sibling finding).
-- =========================================================================
verify.window(1920, 1080)
verify.frames(2)
verify.capture("trades_tab_head")
-- "market_trades", not "market": the tab strip's two views are two different
-- child scrollers and only one is on screen. Aiming the Goods child's name at a
-- frame showing Trades would have been NR-719 by a third route, so the Trades
-- child has a name of its own and `expect_scrolled` makes a miss loud.
verify.scroll_panel("market_trades", 1.0)
verify.frames(2)
verify.expect_scrolled("the Trades tab's scroll request reached a real scroller")
verify.capture("trades_tab_foot")
verify.scroll_panel("market_trades", 0.0)
verify.frames(2)

-- The Goods tab is the tab strip's other half; capture it beside Trades so the
-- strip's two labels are both on record.
verify.panel_view("market", 0)
verify.frames(2)
verify.capture("trades_tab_strip_goods")
verify.panel_view("market", 1)
verify.frames(2)
verify.capture("trades_tab_strip_trades")

-- =========================================================================
-- 6. THE SHUT HALF OF THE GATE
-- =========================================================================
-- LAST, because it destroys the state every read above needs: it takes the
-- player's whole estate off the body through the `demolish` verb, which is what
-- `corporation_component::assets` shrinks on, and then asks the surface whether
-- it still shows the book.
--
-- This is the only route to the shut state in this fixture (§ 2 measured why),
-- and it is a REAL one - a corp that holds nothing on a body is a state the
-- game can reach, and demolish is the verb that reaches it. The gate is the
-- question: does `open` follow ownership, or is it hard-wired true?

local player_here = {}
for _, b in ipairs(verify.buildings()) do
    if b.player and b.body == home_body then player_here[#player_here + 1] = b.id end
end
print("MEASURED player estate on " .. tostring(home_body) .. ": " .. #player_here .. " buildings")
verify.expect(#player_here > 0,
    "the player holds buildings on this body before the demolition (" .. #player_here .. ")")

local refused = 0
for _, bid in ipairs(player_here) do
    local r = verify.corp_command{ verb = 1, subject = bid } -- 1 == corp_verb::demolish
    if r ~= "applied" then refused = refused + 1 end
end
print("MEASURED demolitions refused: " .. refused .. " of " .. #player_here)

verify.frames(2)
verify.expect(verify.player_operates_on(home_body) == false,
    "the world agrees the player now owns nothing on this body")

local shut = verify.market_trades()
verify.expect(shut.open == false,
    "THE GATE SHUTS once the player owns no building on the body (open=" ..
    tostring(shut.open) .. ")")
verify.expect(#shut.rows == 0,
    "a shut gate lists no rows (" .. #shut.rows .. " listed)")

-- And it is a GATE, not an empty book: the world still holds every one of those
-- orders. Without this line "no rows" would prove nothing at all.
local still = verify.world_orders_on_body(home_body)
print(string.format("MEASURED shut gate: world still holds %d sells + %d buys on the body; surface shows %d",
    still.sells, still.buys, #shut.rows))
verify.expect(still.sells + still.buys > 0,
    "the book the gate is hiding is still there (" .. (still.sells + still.buys) .. " orders)")

verify.capture("trades_gate_shut")
