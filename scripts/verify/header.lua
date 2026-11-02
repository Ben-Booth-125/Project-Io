-- Visual verification for the player balance header (header_panel.cpp).
-- Runs a few economy ticks so the player corporation's balance has moved, its
-- pools hold valued stock, and the balance history has enough points for a net
-- figure + sparkline, then captures the always-on header strip.
--
-- Run: ProjectIo --verify scripts/verify/header.lua
-- Inspect: screenshots/header*.png — the strip should read
--   BALANCE <Cr ...>  |  STOCKPILE <Cr ...>  |  NET <±.../qtr> <sparkline>
-- with the balance coloured (red if negative) and the net coloured by direction.

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

-- Textual confirmation of the underlying figures the header summarises.
verify.dump_economy()
