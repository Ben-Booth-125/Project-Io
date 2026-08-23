-- Verify the Construction Ledger window (BL-029 / construction-ledger R1).
-- Opens the construction panel and captures: the empty-queue placeholder
-- ('No active construction.') and the armed-placement Build section.
verify.show_panel("construction", true)
verify.capture("construction_ledger_empty")

-- Also arm placement mode to confirm the Build section is live
verify.place_mode("extraction", "iron_ore")
verify.capture("construction_ledger_armed")
