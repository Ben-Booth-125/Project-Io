-- Potential trades list regardless of trade points, highest profit first, and points
-- gate only taking one (ledgers/market.md § 2b, Ben 2026-11-09, overturning the
-- 2026-11-05 list-only-where-it-has-points rule of BL-1516).
--
-- Walks every market on the home body through the path a click takes and asks three
-- things of each: a market with no trade points still lists routes, every list is in
-- descending profit per point, and any market WITH points is one the trade verb
-- accepts as a source, since that is the rule behind a live "+". Whether the "+" is
-- really disabled is a live click: no read reports a row's button state. Captures a
-- market with points and one without, because the no-points line is the new surface.

verify.goto_surface("home")
verify.econ_step(8)
verify.show_panel("market", true)
verify.panel_view("market", 1) -- Trades
verify.frames(2)

local home_body = verify.market_bodies()[1].body
local open_m, shut_m, offers_open, offers_shut, bad_src, mismatched, out_of_order, bad_profit = nil, nil, 0, 0, 0, 0, 0, 0
local listing_shut = 0 -- markets with no points that still list a route
-- This world moves no cargo on the home body in the first quarters, so the
-- Automatic Trades list is exercised on a staged convoy of ours, on a route with no
-- manual trade, leaving a home-body market for another body's.
local my_qty = 7.0
verify.seed_convoy("home", "inner", "iron_ore", my_qty, 0.4)
local my_rows, auto_m = 0, nil
for _, mid in ipairs(verify.markets_on_body(home_body)) do
    verify.select_market(mid)
    verify.frames(3)
    local g, pot = verify.potential_gate(), verify.potential_trades()
    local auto = verify.market_trades()
    for _, r in ipairs(auto.rows) do
        auto_m = auto_m or mid
        if r.mine and math.abs(r.units - my_qty) < 1e-3 then my_rows = my_rows + 1 end
    end
    print(string.format("MEASURED %s (%d): %.2f points (world %.2f), verb source %s, %d offers, %d automatic",
                        verify.market_name(mid), mid, g.points, g.world_points, tostring(g.verb_source), #pot, #auto.rows))
    if math.abs(g.points - g.world_points) > 1e-4 then mismatched = mismatched + 1 end
    local prev = nil
    for _, r in ipairs(pot) do
        if r.margin_per_point <= 0.0 then bad_profit = bad_profit + 1 end
        if prev ~= nil and r.margin_per_point > prev + 1e-4 then out_of_order = out_of_order + 1 end
        prev = r.margin_per_point
    end
    if g.points > 0.0 then
        offers_open = offers_open + #pot
        open_m = open_m or mid
        if not g.verb_source then bad_src = bad_src + 1 end
    else
        offers_shut = offers_shut + #pot
        if #pot > 0 then
            listing_shut = listing_shut + 1
            shut_m = shut_m or mid
        end
    end
end

verify.expect(mismatched == 0, "the gate reads the acting nation's points at this market (" .. mismatched .. " differ)")
verify.expect(listing_shut > 0, "a market with no trade points still lists routes (" .. listing_shut .. " do, " .. offers_shut .. " rows)")
verify.expect(my_rows == 1, "my staged cargo lists at its home-body market (" .. my_rows .. " rows)")
if auto_m then
    verify.select_market(auto_m)
    verify.frames(3)
    verify.scroll_panel("market_trades", 0.35)
    verify.frames(2)
    verify.capture("trade_offers_automatic")
end
verify.expect(out_of_order == 0, "every list is highest profit per point first (" .. out_of_order .. " out of order)")
verify.expect(bad_profit == 0, "every listed route earns a profit (" .. bad_profit .. " do not)")
verify.expect(bad_src == 0, "a market with trade points is one the trade verb accepts as a source (" .. bad_src .. " is not)")
print(string.format("MEASURED offers: %d from markets with points, %d from markets without", offers_open, offers_shut))

if open_m then
    verify.select_market(open_m)
    verify.frames(3)
    verify.scroll_panel("market_trades", 0.6)
    verify.frames(2)
    verify.capture("trade_offers_sourced_open")
else
    print("MEASURED no market on the home body makes the acting nation trade points")
end
if shut_m then
    verify.select_market(shut_m)
    verify.frames(3)
    verify.scroll_panel("market_trades", 0.6)
    verify.frames(2)
    verify.capture("trade_offers_sourced_shut")
else
    print("MEASURED no market without trade points lists a route on the home body")
end
