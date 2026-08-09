-- BL-211 (player-facing History ledger) + BL-212 (nation-voiced Public comms).
-- Opens the History nav-rail slot (the "Tile Ledger" window) on Kepler and walks
-- its two views: Story (the oral-history biography) and Chain (the generation
-- charts, one collapsing accordion per stage, captured for each of the three
-- rounds). The third view, Tiles, was retired by BL-281 — its capture came down
-- with it. Then captures the Comms panel to confirm the Public channel is
-- nation-authored.
verify.goto_surface("home")
verify.show_panel("tile", true)

verify.panel_view("history", 0)
verify.capture("history_story_kepler")

verify.panel_view("history", 1)
for round = 0, 2 do
    verify.panel_view("history_round", round)
    verify.capture(string.format("history_chain_round%d", round))
end

verify.panel_view("history", 0)
verify.show_panel("tile", false)
verify.capture("comms_public_nation_voiced")
