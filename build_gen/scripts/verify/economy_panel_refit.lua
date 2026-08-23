-- Verify the economy panel refit (BL-026 / economy-panel-refit R1).
-- Runs economy ticks to populate data, then captures the economy panel showing
-- the corp balance table (player row highlighted) and building states.
verify.econ_step(6)
verify.capture("economy_panel_refit")
