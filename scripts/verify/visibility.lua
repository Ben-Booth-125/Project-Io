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
-- player's and the rivals' markers are visible. Buildings are located dynamically
-- via verify.buildings() rather than hard-coded coordinates, so the script stays
-- valid when world generation moves the starting holdings (the 2026-07-04 audit
-- found the original fixed tiles no longer held buildings — the captures were
-- silently photographing nothing).
-- Run with: ProjectIo --verify scripts/verify/visibility.lua

verify.econ_step(4)            -- populate live building run states
verify.show_panel("economy", false)   -- clear the auto-opened panel off the capture
verify.goto_surface("home")    -- Kepler: fully surveyed, all markers visible

local rival, player
for _, b in ipairs(verify.buildings()) do
    if b.player then
        player = player or b
    else
        rival = rival or b
    end
end
assert(rival,  "visibility.lua: no rival building found on the home body")
assert(player, "visibility.lua: no player building found on the home body")

-- A RIVAL building (corp-owned, not the player's): the competitor layout. Public
-- type + owner + tile location, then the 'private' placeholder rows that render
-- the withheld channels rather than silently omitting them.
verify.select_building(rival.x, rival.y)
frame_tile(rival.x, rival.y, 18)
verify.capture("visibility_rival_panel")

-- A PLAYER building for contrast: full management detail (target, tile, workforce).
verify.select_building(player.x, player.y)
frame_tile(player.x, player.y, 18)
verify.capture("visibility_player_panel")
