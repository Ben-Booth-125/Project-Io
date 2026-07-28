-- BL-211 (player-facing History ledger) + BL-212 (nation-voiced Public comms).
-- Opens the History nav-rail slot (the "Tile Ledger" window, now carrying the
-- Generation History biography above the tile table) on Kepler, then captures
-- the Comms panel to confirm the Public channel is nation-authored.
verify.goto_surface("Kepler")
verify.show_panel("tile", true)
verify.capture("history_ledger_kepler")

verify.show_panel("tile", false)
verify.capture("comms_public_nation_voiced")
