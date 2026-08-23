-- Visual verification for the Layer 3 economy panel (economy_panel.cpp).
-- Runs a few economy ticks so pools, markets, balances, and building states are
-- populated, then captures the open panel. econ_step() opens the panel itself.
--
-- Run: ProjectIo --verify scripts/verify/economy_panel.lua
-- Inspect: screenshots/economy_panel*.png

-- Land on the home planet so the corporate assets that produce are in scope.
verify.goto_surface("Kepler")

-- Resolve three economy ticks: extraction fills pools, processors consume and
-- auto-buy, the market clears, the budget moves balances.
verify.econ_step(3)

verify.capture("economy_panel_overview")

-- Textual confirmation of the same data the panel renders (balances incl. any
-- negative flag, pools, market supply/demand, building run states).
verify.dump_economy()
