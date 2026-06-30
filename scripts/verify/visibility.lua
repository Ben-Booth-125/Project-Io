-- Visual verification for the Visibility model (BL-068).
-- Confirms requirements.json § visibility-model R1 (item-spanning):
--   on a surveyed body, selecting a RIVAL building opens the competitor Selection
--   panel — public facts (type, owner, location) plus explicit 'private'
--   placeholder rows for the withheld production / stockpile channels, and never
--   any throughput; selecting a PLAYER building still shows full management detail.
-- Rival-marker region gating (absent in an unrevealed region) is already proven by
-- survey.lua's masked capture — this script covers the read-time asymmetry.
--
-- Driver: verify.select_building(col,row) sets the selection on the active body,
-- exactly as a canvas click would. Kepler (home) opens fully surveyed, so both the
-- player's and the rivals' markers are visible.
-- Run with: ProjectIo --verify scripts/verify/visibility.lua

verify.econ_step(4)            -- populate live building run states
verify.show_panel("economy", false)   -- clear the auto-opened panel off the capture
verify.goto_surface("home")    -- Kepler: fully surveyed, all markers visible

-- A RIVAL building (corp-owned, not the player's): the competitor layout. Public
-- type + owner + tile location, then the 'private' placeholder rows that render
-- the withheld channels rather than silently omitting them.
verify.select_building(70, 46)
frame_tile(70, 46, 18)
verify.capture("visibility_rival_panel")

-- A PLAYER building for contrast: full management detail (target, tile, workforce).
verify.select_building(70, 38)
frame_tile(70, 38, 18)
verify.capture("visibility_player_panel")
