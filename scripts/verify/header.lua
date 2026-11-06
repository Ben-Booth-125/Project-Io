-- Visual verification for the player balance header (header_panel.cpp).
-- Runs a few economy ticks so the player corporation's balance has moved, its
-- pools hold valued stock, and the balance history has enough points for a net
-- figure + sparkline, then captures the always-on header strip.
--
-- Run: ProjectIo --verify scripts/verify/header.lua
-- Inspect: screenshots/header*.png — the strip should read
--   TREASURY <Cr ...> (<±.../qtr>)  |  CARGO <Cr ...>  |  RUNWAY  12q
-- with the net coloured by direction; the graph is the money figures' tooltip.

-- Land on the home planet so the player's producing assets are in scope.
verify.goto_surface("home")

-- Resolve several economy ticks: each step_economy() moves the balance and pushes
-- a point into the header's balance history (so the net delta and sparkline draw).
verify.econ_step(4)

verify.capture("header_populated")

-- BL-1472 (HEADER.md § Contents, Recruits): the three recruit readouts at the top
-- right, land / air / sea, each held / cap with a bar.
local h = verify.header_recruits()
if h.drawn and h.land.x >= 0 and h.air.x > h.land.x and h.sea.x > h.air.x then
    print(string.format("header: PASS recruit readouts land %d/%d air %d/%d sea %d/%d (fit %d)",
                        h.land.held, h.land.cap, h.air.held, h.air.cap, h.sea.held, h.sea.cap, h.fit))
else
    print("header: FAIL the recruit readouts were not drawn land, air, sea")
end

-- The fit at the 1280 floor (Ben, 2026-11-02: squeeze the recruit floor to 52 px so the
-- runway's value keeps its place). The runway is resolved before the readouts' numbers;
-- at 1280x720 the money run plus "   |   12q" must fit in what the readouts' floor leaves.
-- The widths are measured whether or not the treasury is burning, so the check is never
-- vacuous; the label ("   |   RUNWAY  12q") is reported, not asserted.
verify.window(1280, 720)
verify.capture("header_1280")
local n = verify.header_recruits()
local spare = n.room - n.money_used - n.runway_value
print(string.format("header: 1280 room %.0f money %.0f runway value %.0f label %.0f spare %.0f (runway step %d, recruit fit %d)",
                    n.room, n.money_used, n.runway_value, n.runway_full, spare, n.runway, n.fit))
if spare >= 0 and n.runway ~= 0 then
    print("header: PASS the runway keeps its value at 1280")
else
    print(string.format("header: FAIL the runway is %.0f px short of its value at 1280 (step %d)", -spare, n.runway))
end

-- Textual confirmation of the underlying figures the header summarises.
verify.dump_economy()

-- BL-1512: hovering a money figure shows the shared tooltip with the graph. The figures
-- start at the header's left edge, past the shell column; the treasury is first.
verify.window(1280, 720)
verify.econ_step(2)
verify.capture("header_frame")
local tip_seen = false
for x = 300, 760, 12 do
    verify.hover(x, 46, 2)
    verify.capture("header_frame")
    if verify.header_recruits().money_tip then
        tip_seen = true
        verify.capture("header_money_tooltip")
        break
    end
end
print(tip_seen and "header: PASS a money figure's tooltip shows the graph" or "header: FAIL no money tooltip")
