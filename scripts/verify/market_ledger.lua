-- Verify the Market Ledger window (BL-027 / market-ledger R1).
-- Runs economy ticks so prices diverge from base, then opens the Market Ledger
-- and captures it showing the per-resource supply/demand/price table.
verify.econ_step(6)
verify.show_panel("economy", false)  -- hide economy panel opened by econ_step
verify.show_panel("market", true)
verify.capture("market_ledger")
