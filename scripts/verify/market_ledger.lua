-- Verify the redesigned Market Ledger (BL-120 mock-up): body selector -> market/city
-- selector (cascade, real city names) -> scrollable per-good price-over-time charts.
-- Runs 16 ticks so the price series have curves to plot.
verify.goto_surface("home")
verify.econ_step(16)
verify.show_panel("market", true)
verify.capture("market_ledger")
