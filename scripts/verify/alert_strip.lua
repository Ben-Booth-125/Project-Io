-- BL-1515 (alert strip): what wants attention, centred below the header, each alert a
-- press through to the surface that answers it (LAYOUT.md § Alert strip).
--
-- Reads the rows as drawn, presses each through the real input path, and asserts where
-- it led. Which alerts are up is the world's; the Research alert is staged by leaving
-- the queue empty, which is how a campaign opens.
--
-- Run:     ProjectIo --verify scripts/verify/alert_strip.lua
-- Writes:  screenshots/alert_*.png

local failed = 0
local function check(c, m)
    if c then print("alert_strip: PASS " .. m) else failed = failed + 1; print("alert_strip: FAIL " .. m) end
end

verify.window(1280, 720)
verify.goto_surface("home")
verify.econ_step(4)
verify.capture("alert_00_strip")

local rows = verify.alerts()
check(#rows > 0, "the strip has something to say (" .. #rows .. " alerts)")
for i = 1, #rows do
    print(string.format("alert_strip: row %d kind %d '%s' at %.0f,%.0f", i, rows[i].kind, rows[i].text, rows[i].x, rows[i].y))
    -- Below the header strip (92 px), and centred near the canvas's middle rather than a corner.
    check(rows[i].y >= 92, "'" .. rows[i].text .. "' sits below the header")
end

-- kind -> what the press must open
local expect = {
    [0] = function(l) return l.research end,
    [1] = function(l) return l.construction and l.view == 1 end,
    [2] = function(l) return l.diplomacy end,
    [3] = function(l) return l.budget end,
    [4] = function(l) return l.construction and l.view == 0 end,
    [5] = function(l) return l.construction and l.view == 1 end,
    [6] = function(l) return l.market end,
}
local seen = #rows
for i = 1, seen do
    local r = verify.alerts()[i]
    if r then
        verify.click(r.x, r.y)
        verify.capture("alert_press_" .. r.kind)
        local l = verify.open_ledgers()
        check(expect[r.kind](l), "'" .. r.text .. "' opens the surface that answers it")
        verify.press_escape() -- the ledger rung
        verify.capture("alert_frame")
    end
end

if failed > 0 then error("alert_strip: " .. failed .. " failed") end
print("alert_strip: OK")
