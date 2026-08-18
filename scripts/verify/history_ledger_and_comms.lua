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
verify.scroll_panel("history", 0.0)
verify.capture("history_story_kepler")

-- The column clips the biography after ~8 lines, so the head capture above can
-- only ever prove the OLDEST events render. Park the scroll at the foot and take
-- a second Story capture: the late history — the recent-epoch events, and
-- whatever date formatting they carry — now has a golden of its own.
-- frames(2) because the scroll resolves against the previous frame's extent.
verify.scroll_panel("history", 1.0)
verify.frames(2)
verify.capture("history_story_kepler_foot")
-- Back to the top, and left parked there: clearing the request would merely stop
-- setting the scroll, leaving the window still at the foot for later captures.
-- frames(1) because the request lands on the following frame, and the Chain
-- captures below would otherwise inherit the foot scroll and come out blank.
verify.scroll_panel("history", 0.0)
verify.frames(1)

verify.panel_view("history", 1)
for round = 0, 2 do
    verify.panel_view("history_round", round)
    verify.capture(string.format("history_chain_round%d", round))
end

verify.panel_view("history", 0)
verify.show_panel("tile", false)
verify.capture("comms_public_nation_voiced")
