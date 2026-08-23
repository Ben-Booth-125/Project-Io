-- BL-211 (player-facing History ledger) + BL-212 (nation-voiced Public comms).
-- Opens the History nav-rail slot (the "Tile Ledger" window) on Kepler and walks
-- its three views: Story (the oral-history biography), Chain (the generation
-- charts, one collapsing accordion per stage, captured for each of the three
-- rounds), and Tiles (the tile/building/market tables). Then captures the Comms
-- panel to confirm the Public channel is nation-authored.
verify.goto_surface("Kepler")
verify.show_panel("tile", true)

verify.panel_view("history", 0)
verify.capture("history_story_kepler")

verify.panel_view("history", 1)
for round = 0, 2 do
    verify.panel_view("history_round", round)
    verify.capture(string.format("history_chain_round%d", round))
end

-- Spark, pinned open (BL-209). The round loop above can only ever show each
-- round's FIRST stage, so the abiogenesis charts -- how far the chemistry climbed,
-- and the two cofactor metals that gate the photosystem fork -- would otherwise
-- never appear in a capture at all.
verify.panel_view("history_round", 1) -- the Life round holds Spark
verify.panel_view("history_stage", 5) -- chain_stage::spark
verify.capture("history_chain_spark")
verify.panel_view("history_stage", -1)

verify.panel_view("history", 2)
verify.capture("history_tiles_kepler")

verify.panel_view("history", 0)
verify.show_panel("tile", false)
verify.capture("comms_public_nation_voiced")
