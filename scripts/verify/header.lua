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

-- Textual confirmation of the underlying figures the header summarises.
verify.dump_economy()
