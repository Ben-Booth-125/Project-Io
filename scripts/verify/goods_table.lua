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

-- 4c. THE NATION PRESENCE ROW (BL-688). "No national presence" is a legitimate
--     state, so this reports rather than demanding chips — but a chip that draws
--     must carry initials, or the row is a strip of anonymous colour.
local chips = verify.nation_chips()
print("MEASURED nation presence row: " .. #chips .. " chip(s)")
local bad_ini = 0
for _, c in ipairs(chips) do
    if c.initials == nil or c.initials == "" or c.initials == "??" then
        bad_ini = bad_ini + 1
    end
end
verify.expect(bad_ini == 0,
    "every nation chip carries readable initials (" .. bad_ini .. " blank)")

-- 5. ROW HEIGHT IS THE OPEN MEASUREMENT (Ben: "compare this at 8 rows, 10 rows,
--    and 12 rows"). Captured at 1920x1080 because that is the screen being
--    reviewed — a density judgement at 720p is taken against half the content
--    height, which is how such numbers were got wrong before 2026-08-29.
verify.window(1920, 1080)
verify.frames(2)
for _, n in ipairs({8, 10, 12}) do
    verify.goods_rows(n)
    verify.frames(2)
    verify.capture("goods_table_1920_" .. n .. "rows")
end
verify.goods_rows(10)

-- The nation presence row (BL-688) sits below the selectors and above the tabs.
verify.capture("goods_table_nation_row")
