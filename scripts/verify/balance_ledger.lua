-- Verify the Balance Ledger window (BL-028 / balance-ledger R1).
-- Runs economy ticks so the balance moves from starting capital, then opens
-- the Balance Ledger and captures the treasury / net / assets view.
verify.econ_step(6)
verify.show_panel("economy", false)
verify.show_panel("balance", true)
verify.capture("balance_ledger")
