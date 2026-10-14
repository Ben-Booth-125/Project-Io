-- The Goods table (BL-686): one row per traded good, the 8-quarter graph
-- flattened into the row.
--
-- THIS ASSERTS RATHER THAN CAPTURING, deliberately. The view it replaces was
-- verified by a single `capture` and that proved almost nothing: every sparkline
-- was drawn to its own scale, nothing had ever seen past the fourth good of ~42
-- (NR-719), and `expect_no_clipping` records ZERO on this class of surface even
-- over visibly clipped frames (NR-663). A picture is kept at the end, but the
-- verdict comes from the assertions.

verify.goto_surface("home")

-- 0. BEFORE ANY QUARTER (BL-1320). The verify world is built inline by
--    setup_world, UNSETTLED (app.cpp, load_economy: "unsearched and unsettled"),
--    so unlike a campaign — whose twelve warm-up quarters run run_settle_tick ->
--    clear_markets and publish the trade value record before play opens — no
--    quarter has completed here. That is the one state the ledger must show "—"
--    in, not a false zero: the same state a loaded save opens in.
verify.show_panel("market", true)
verify.panel_view("market", 0)
verify.frames(2)
do
    local t0 = verify.market_top()
    print(string.format("MEASURED before any econ step: drawn %s, sold_known %s, traded_known %s",
                        tostring(t0.drawn), tostring(t0.sold_known), tostring(t0.traded_known)))
    verify.expect(t0.drawn and not t0.traded_known,
        "before any quarter completes, Traded in / out draw no figure (the unsettled verify world)")
end
verify.show_panel("market", false)
verify.frames(1)

-- Enough ticks that the price series have more than the 8-quarter window in them,
-- so "the graph plots at most 8" is a real constraint and not vacuously true.
verify.econ_step(16)
verify.show_panel("market", true)
verify.panel_view("market", 0) -- Goods
verify.frames(2)

local rows   = verify.goods_table()
local traded = verify.market_traded_goods()

-- 1. EVERY TRADED GOOD IS LISTED. The old view's scroll bug meant goods 4-42 had
--    never been looked at by any capture, golden or human; a count assertion is
--    what makes "the table lists them" checkable at all.
local n_rows, n_traded = #rows, #traded
verify.expect(n_rows > 0, "Goods table drew at least one row (drew " .. n_rows .. ")")
verify.expect(n_rows == n_traded,
    "every traded good has a row: " .. n_rows .. " rows vs " .. n_traded .. " traded goods")

-- 2. THE FIGURES MATCH `market_component`. Each row carries what the surface drew
--    AND an independent read of the same field, so this compares two reads rather
--    than restating one.
local bad_price, bad_vs, bad_samples, worst_vs = 0, 0, 0, 0.0
for _, r in ipairs(rows) do
    if r.world_price and math.abs(r.price - r.world_price) > 1e-4 then
        bad_price = bad_price + 1
    end
    if r.world_base and math.abs(r.base_price - r.world_base) > 1e-4 then
        bad_price = bad_price + 1
    end
    -- `price vs base` is price/base_price and nothing else.
    if r.base_price > 0.0 then
        local want = r.price / r.base_price
        local err  = math.abs(r.vs_base - want)
        if err > worst_vs then worst_vs = err end
        if err > 1e-4 then bad_vs = bad_vs + 1 end
    end
    -- 3. THE GRAPH WINDOW IS 8 QUARTERS. Ben's number once the arithmetic was put
    --    to him: record_histories samples once per econ tick and a tick is a
    --    quarter, so "six months" would have been two points.
    if r.samples > 8 then bad_samples = bad_samples + 1 end
end

verify.expect(bad_price == 0,
    "every drawn price/base matches market_component (" .. bad_price .. " mismatched)")
verify.expect(bad_vs == 0,
    "vs-base column is price/base_price (" .. bad_vs .. " wrong, worst err " ..
    string.format("%.6f", worst_vs) .. ")")
verify.expect(bad_samples == 0,
    "no row's graph plots more than 8 quarters (" .. bad_samples .. " over)")

-- The window must actually BE 8 after 16 ticks, not merely "at most 8" — an
-- always-empty graph would satisfy the bound above and show nothing.
local full = 0
for _, r in ipairs(rows) do if r.samples == 8 then full = full + 1 end end
verify.expect(full > 0,
    "after 16 ticks at least one row plots a full 8-quarter window (" .. full .. " do)")

-- 4. THE SCROLL REQUEST REACHES THE REAL SCROLLER (NR-719, the half that was
--    silent). The Goods list is in a child scroller; before this fix the request
--    aimed at the window, `SetScrollY` was a no-op, and the "foot" capture came
--    back byte-identical to the head. `expect_scrolled` fails when nothing claimed
--    the request, so a re-misaimed hook cannot go green again.
verify.capture("goods_table_head")
verify.scroll_panel("market", 1.0)
verify.frames(2)
verify.expect_scrolled("market goods list scrolled to the foot")
verify.capture("goods_table_foot")
verify.scroll_panel("market", 0.0)
verify.frames(2)

-- 4b. DOES `body_average_price` SURVIVE? The design named it the first column to
--     drop IF the row will not fit, and required the fit be MEASURED first. So
--     measure it under both layouts and report the numbers rather than asserting
--     a preference. The binding figure is how many of the 45 good names fit the
--     name column without eliding — two goods that both elide to "Iron ..." are
--     indistinguishable on a board whose whole job is telling goods apart.
local function name_fit_report(label)
    local fit, total, worst_name, worst_short = 0, 0, "", 1e9
    local avail = 0
    for _, r in ipairs(verify.goods_table()) do
        total = total + 1
        avail = r.name_avail
        if r.name_fits then fit = fit + 1
        elseif (r.name_avail - r.name_needed) < worst_short then
            worst_short = r.name_avail - r.name_needed
            worst_name  = r.name
        end
    end
    print(string.format(
        "MEASURED %s: name column %.1f px; %d/%d names fit; worst short by %.1f px (%s)",
        label, avail, fit, total, (total > fit) and worst_short or 0.0, worst_name))
    return fit, total
end

verify.goods_body_column(true)
verify.frames(2)
local fit_with, total_all = name_fit_report("WITH body-average column")
verify.capture("goods_table_with_body_col")

verify.goods_body_column(false)
verify.frames(2)
local fit_without = name_fit_report("WITHOUT body-average column")
verify.capture("goods_table_without_body_col")

-- Back to the SHIPPING layout (the column defaults off on the measurement above)
-- so the row-height captures below show what a player would actually see. Which
-- one ships is Ben's call off these numbers and pictures, not this script's.
verify.goods_body_column(false)
verify.frames(2)
verify.expect(fit_without >= fit_with,
    "dropping the body column cannot make the name column narrower (" ..
    fit_with .. " -> " .. fit_without .. " of " .. total_all .. " names fit)")

-- 4c. THE MARKET'S TOP SECTION (BL-1317; ledgers/market.md § 2), superseding
--     BL-688's chip row. "No national presence" is a legitimate state, so this
--     reports rather than demanding rows — but every row drawn must carry the
--     nation's FULL name, the one the world holds, or the list is no better than
--     the initials it replaced.
-- The ledger opens on the body's first market, which may have no presence and no
-- sales; that makes both checks vacuous. Point it at the market with the most
-- nations (ties: most sold) THROUGH THE PATH A CLICK TAKES (select_market).
local best, best_n, best_sold = nil, -1, -1.0
for _, mid in ipairs(verify.markets_on_body(verify.market_bodies()[1].body)) do
    verify.select_market(mid)
    verify.frames(2)
    local n, sold = #verify.nation_rows(), verify.market_top().sold_here
    if n > best_n or (n == best_n and sold > best_sold) then
        best, best_n, best_sold = mid, n, sold
    end
end
verify.select_market(best)
verify.frames(2)
print(string.format("MEASURED busiest market: %s (%d), %d nation(s), Sold here %.2f",
                    verify.market_name(best), best, best_n, best_sold))

local nations = verify.nation_rows()
print("MEASURED nation list: " .. #nations .. " row(s)")
local bad_name, elided = 0, 0
for _, n in ipairs(nations) do
    if n.name == nil or n.name == "" or n.name ~= n.world_name then bad_name = bad_name + 1 end
    if n.elided then elided = elided + 1 end
end
verify.expect(bad_name == 0,
    "every nation row carries the nation's full name (" .. bad_name .. " wrong)")
print("MEASURED nation names elided: " .. elided .. " of " .. #nations)

-- Sold here is read from the exchange record (the newest quarter's sale rows on
-- this market). The world-side sum is an independent walk of the same ring.
local top = verify.market_top()
verify.expect(top.drawn, "the market top section drew")
if top.sold_known then
    verify.expect(math.abs(top.sold_here - top.world_sold_here) <= 1e-3 * math.max(1.0, top.world_sold_here),
        string.format("Sold here matches the exchange record (%.2f drawn vs %.2f in the record, quarter %d)",
                      top.sold_here, top.world_sold_here, top.quarter))
end
verify.expect(#nations > 0, "the busiest market lists at least one nation")

-- 4d. TRADED IN / TRADED OUT (BL-1320; TRADE.md § The trade value record). After
--     16 econ steps a quarter has completed, so both carry a figure, not a dash.
local function finite(x) return x == x and x ~= math.huge and x ~= -math.huge end
print(string.format("MEASURED busiest market Traded in %.2f, Traded out %.2f (known %s)",
                    top.traded_in, top.traded_out, tostring(top.traded_known)))
verify.expect(top.traded_known, "Traded in / out carry a figure once a quarter has completed")
verify.expect(finite(top.traded_in) and top.traded_in >= 0.0,
    string.format("Traded in is finite and non-negative (%.2f)", top.traded_in))
verify.expect(finite(top.traded_out) and top.traded_out >= 0.0,
    string.format("Traded out is finite and non-negative (%.2f)", top.traded_out))

-- ONE LINE EACH (market.md § 2, Ben 2026-10-13): every metric's figure sits on
-- its label's line, and the three lines stack downward.
local ly, fy = top.metric_label_y, top.metric_figure_y
local off_line = 0
for k = 1, 3 do if math.abs(ly[k] - fy[k]) > 0.5 then off_line = off_line + 1 end end
verify.expect(off_line == 0,
    string.format("each metric's label and figure share one line (%d off; y %.1f/%.1f, %.1f/%.1f, %.1f/%.1f)",
                  off_line, ly[1], fy[1], ly[2], fy[2], ly[3], fy[3]))
verify.expect(ly[1] < ly[2] and ly[2] < ly[3], "the three metrics stack top to bottom")

-- Traded out against an INDEPENDENT WALK: the convoys that left this market this
-- quarter (still at progress 0) x the posted price the exchange record's purchase
-- row carries. Aimed at the market on this body with the most Traded out, so the
-- comparison is not vacuous; a body with no trade at all reports and skips.
local tbest, tbest_out = nil, -1.0
local ibest, ibest_in = nil, -1.0
for _, mid in ipairs(verify.markets_on_body(verify.market_bodies()[1].body)) do
    verify.select_market(mid)
    verify.frames(2)
    local t = verify.market_top()
    if t.traded_out > tbest_out then tbest, tbest_out = mid, t.traded_out end
    if t.traded_in > ibest_in then ibest, ibest_in = mid, t.traded_in end
end
print(string.format("MEASURED most Traded in on this body: %s (%d), %.2f",
                    verify.market_name(ibest), ibest, ibest_in))
verify.select_market(tbest)
verify.frames(2)
local tt = verify.market_top()
print(string.format(
    "MEASURED busiest trade market: %s (%d), Traded in %.2f, Traded out %.2f, walk %.2f over %d fresh convoy(s), %d unpriced",
    verify.market_name(tbest), tbest, tt.traded_in, tt.traded_out, tt.world_traded_out,
    tt.fresh_convoys, tt.fresh_unpriced))
if tt.traded_out > 0.0 then
    verify.expect(tt.fresh_unpriced == 0, "every fresh trade convoy has its purchase row in the exchange record")
    verify.expect(math.abs(tt.traded_out - tt.world_traded_out) <= 1e-3 * math.max(1.0, tt.world_traded_out),
        string.format("Traded out matches an independent walk of this quarter's convoys (%.2f drawn vs %.2f walked)",
                      tt.traded_out, tt.world_traded_out))
else
    print("MEASURED no market on this body traded out this quarter: the walk comparison is vacuous")
end
verify.select_market(best)
verify.frames(2)

-- 5. ROW HEIGHT (Ben, 2026-10-13, closing the 8/10/12/15-row comparison): 0.4 x
--    the ten-rows-to-the-view row of the layout before the top section existed,
--    floored at one frame height. Measured at 1920x1080 because that is the
--    screen being reviewed, and at 1280x720 for the smallest supported display.
local function rows_at(w, h)
    verify.window(w, h)
    verify.frames(3)
    local t = verify.market_top()
    print(string.format(
        "MEASURED %dx%d: column %.1f px, top section %.1f px (%.3f of column), legacy row %.1f px, goods row %.1f px (frame %.1f)",
        w, h, t.column_h, t.top_h, t.top_h / math.max(1.0, t.column_h), t.legacy_row_h, t.goods_row_h, t.frame_h))
    local names = {"Sold", "In", "Out"}
    local overrun = 0
    for k = 1, 3 do
        local a, fw = t.metric_figure_avail[k], t.metric_figure_w[k]
        if fw > a + 0.5 then overrun = overrun + 1 end
        print(string.format("MEASURED %dx%d metric %s: figure %.1f px in %.1f px%s",
                            w, h, names[k], fw, a, (fw > a + 0.5) and " (OVERRUNS)" or ""))
    end
    t.metric_overrun = overrun
    -- Short labels so every figure fits beside its label (Ben, 2026-10-13, option A).
    verify.expect(overrun == 0, string.format("%dx%d every metric figure fits beside its label (%d overrun)", w, h, overrun))
    local want = math.max(t.frame_h, 0.4 * t.legacy_row_h)
    verify.expect(math.abs(t.goods_row_h - want) < 0.01,
        string.format("%dx%d goods row is max(frame, 0.4 x legacy) (%.2f vs %.2f)", w, h, t.goods_row_h, want))
    return t
end
rows_at(1280, 720)
rows_at(1920, 1080)
verify.capture("goods_table_1920")

-- The top section sits below the selectors and above the tabs.
verify.capture("goods_table_top_section")
