-- THE WORKS AND THE RUNG ON ROUND 6 (BL-1099, BL-1100; Ben, 2026-09-24,
-- sprint 47 rulings R15, R16, R22; STARTUP.md § Round 6).
--
--   ProjectIo --verify scripts/verify/history_lapse_works.lua
--
-- WHAT THIS CHECKS. Three things the Industrialisation round now carries, off
-- the record it plays and never off the world-gen roster:
--
--   1. A `works_chartered` note (kind 21) flashes a works glyph at its region's
--      anchor at its own year, fading over the marker window like the other
--      kind pings (a capture at t = 0 and one a half-window later).
--   2. A realm's `rung_crossed` note (kind 22) marks its capital with the
--      ember square from its crossing year, and the ticker names it ("The
--      realm of Y lights its furnaces at X") -- read through
--      `verify.history_ticker()` as prose, not hoped for in a frame.
--   3. At the record's LAST year the real charters flash at their anchor
--      tiles (`works_close`, filled from the finish's charter report), all at
--      full strength under --verify, where the stagger is frozen.
--
-- A kind the verify world never recorded is skipped with a line saying so
-- rather than failing: the record is the seed's, not the surface's. On a seed
-- where no realm crosses, the assertion is that the layer is honestly empty
-- (`verify.history_industry()` reads 0 lit) and no furnace line fires.
--
-- 1920x1080, the design-review resolution.

verify.window(1920, 1080)

local function shot(name)
    verify.frames(2)
    verify.capture(name)
end

-- Event kinds, as era_timelapse.hpp numbers them (the wire form).
local works_chartered = 21
local rung_crossed    = 22

-- Round 6.
verify.history_run(3)
verify.frames(4)
verify.capture("history_lapse_works_warmup6")

local first, last = verify.history_span()
verify.expect(last > first, "round 6 carries a record with a span (" .. first .. " -> " .. last .. ")")
local window = math.max(1, (last - first) // 30) -- lapse_marker_window_years

-- 1. The works note: the LAST one with a whole marker window inside the span.
do
    local y = verify.history_event_year(works_chartered, -1, last - window)
    if y < first then
        print("history_lapse_works: no works_chartered event on this record; skipped")
    else
        verify.history_year(y)
        shot("history_lapse_works_note_a_at")
        verify.history_year(y + window // 2)
        shot("history_lapse_works_note_b_half")
    end
end

-- 2. The realm's crossing: the mark at the capital and the ticker's line.
do
    local y = verify.history_event_year(rung_crossed)
    if y < first then
        print("history_lapse_works: no realm crossed the rung on this record; the layer must be empty")
        verify.history_year(last)
        verify.frames(2)
        local lit = verify.history_industry()
        verify.expect(lit == 0, "no crossing: the ember layer is honestly empty (lit = " .. lit .. ")")
        local rows = verify.history_ticker()
        for _, line in ipairs(rows) do
            verify.expect(not line:find("lights its furnaces", 1, true),
                          "no crossing: no furnace line on the ticker (" .. line .. ")")
        end
    else
        verify.history_year(y)
        shot("history_lapse_works_rung_a_at")
        local lit = verify.history_industry()
        verify.expect(lit >= 1, "a realm crossed: its capital carries the ember mark (lit = " .. lit .. ")")
        local rows  = verify.history_ticker()
        local named = false
        for _, line in ipairs(rows) do
            if line:find("lights its furnaces at", 1, true) then named = true end
        end
        verify.expect(named, "the ticker names the realm that crossed and its capital")
        verify.history_year(math.min(last, y + window * 2))
        shot("history_lapse_works_rung_b_stays")
        verify.expect(verify.history_industry() >= 1, "the ember mark STAYS two windows on")
    end
end

-- 3. The close: the real charters at their anchor tiles. Under --verify the
-- round adopts the harness world's record and the seed-candidate spend's own
-- (dated) charter report, so the marks are exactly that spend's charters on
-- the home body -- read back, not hoped for.
verify.history_year(last)
shot("history_lapse_works_close")
do
    local marks, notes, charters = verify.history_works()
    print("history_lapse_works: " .. marks .. " close marks for " .. charters
          .. " charters in the dated report, " .. notes .. " works notes on the record")
    -- EXACTLY the spend's charters, not merely some: a mark the home-body
    -- filter or the grid mapping dropped would pass a `> 0` and fail this.
    verify.expect(charters > 0, "the dated report holds charters to flash (" .. charters .. ")")
    verify.expect(marks == charters, "the close flashes exactly the real charters ("
                  .. marks .. " marks for " .. charters .. " charters)")
end

verify.expect_no_clipping("history_lapse_works")
