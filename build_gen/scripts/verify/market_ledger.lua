-- Verify the redesigned Market Ledger (BL-120 mock-up): body selector -> market/city
-- selector (cascade, real city names) -> scrollable per-good price-over-time charts.
-- Runs 16 ticks so the price series have curves to plot.
verify.goto_surface("Kepler")
verify.econ_step(16)
verify.show_panel("economy", false)  -- hide economy panel opened by econ_step
verify.show_panel("market", true)
verify.capture("market_ledger")
