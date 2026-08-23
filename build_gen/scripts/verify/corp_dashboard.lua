-- Verify the Corporation Overview Dashboard (BL-022 / corp-overview-dashboard R1).
-- Runs economy ticks so balances diverge, then opens the corporation panel and
-- captures the per-corp table with the player row tinted.
verify.econ_step(6)
verify.show_panel("economy", false)
verify.show_panel("corporation", true)
verify.capture("corp_dashboard")
