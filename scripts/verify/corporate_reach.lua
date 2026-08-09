-- Verify the corporate HQ-marker layer (BL-182 foundation; the reach ring
-- retired BL-329, 2026-08-08).
-- Under the Corporation lens, every RIVAL corporation on the active body shows
-- an HQ star (the holding nearest that corp's holdings centroid), in the
-- corp's identity colour. It previously also drew a fixed-radius reach ring
-- around that seat — retired per Ben's live critique ("it doesn't show
-- anything informative"; the BL-323 reach fog shows supply reach properly).
-- The player's own marker is the always-on home star (BL-085), so this layer
-- excludes the player to avoid a double-draw. Render-only chrome; the full
-- gameplay mechanic stays deferred in BL-182.
verify.goto_surface("home")
verify.set_overlay("corporation")

-- Wide view: multiple rival corporations' HQ stars should be visible at once,
-- reading as the corporate-landscape counterpart to the Country lens's
-- national borders (plus the player's always-on home star).
verify.set_zoom(3)
verify.capture("corporate_reach_wide")

-- Framed on the player's HQ: the player's always-on home star (BL-085) should
-- be centred, coexisting with rival HQ stars nearby.
local buildings = verify.buildings()
for _, b in ipairs(buildings) do
    if b.player then
        frame_tile(b.x, b.y, 12)
        verify.capture("corporate_reach_player_hq")
        break
    end
end
