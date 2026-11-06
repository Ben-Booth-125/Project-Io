-- Potential trades offer only routes our markets can supply (BL-1516;
-- ledgers/market.md § 2b, Ben 2026-11-05).
--
-- Walks every market on the home body through the path a click takes and asks one
-- thing of each: offers exist only where the acting nation makes trade points, and
-- every offer leaves a market the trade verb accepts as a source. Captures a market
-- with the gate open and one with it shut, because the shut line is the new surface.

verify.goto_surface("home")
verify.econ_step(8)
verify.show_panel("market", true)
verify.panel_view("market", 1) -- Trades
verify.frames(2)

local home_body = verify.market_bodies()[1].body
local open_m, shut_m, offers_open, offers_shut, bad_gate, bad_src, mismatched = nil, nil, 0, 0, 0, 0, 0
for _, mid in ipairs(verify.markets_on_body(home_body)) do
    verify.select_market(mid)
    verify.frames(3)
    local g, pot = verify.potential_gate(), verify.potential_trades()
    print(string.format("MEASURED %s (%d): %.2f points (world %.2f), verb source %s, %d offers",
                        verify.market_name(mid), mid, g.points, g.world_points, tostring(g.verb_source), #pot))
    if math.abs(g.points - g.world_points) > 1e-4 then mismatched = mismatched + 1 end
    if g.points > 0.0 then
        offers_open = offers_open + #pot
        if open_m == nil or #pot > 0 then open_m = open_m or mid end
        if #pot > 0 and not g.verb_source then bad_src = bad_src + #pot end
    else
        offers_shut = offers_shut + #pot
        shut_m = shut_m or mid
        if #pot > 0 then bad_gate = bad_gate + #pot end
    end
end

verify.expect(mismatched == 0, "the gate reads the acting nation's points at this market (" .. mismatched .. " differ)")
verify.expect(bad_gate == 0, "a market with no trade points offers nothing (" .. bad_gate .. " offers)")
verify.expect(bad_src == 0, "every offer leaves a market the trade verb accepts (" .. bad_src .. " do not)")
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
    print("MEASURED every market on the home body makes the acting nation trade points")
end
