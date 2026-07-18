-- Verify the corporate reach layer (BL-182, visual slice).
-- Under the Corporation lens, every RIVAL corporation on the active body should show
-- an HQ-projected border: a reach ring centred on its HQ (the holding nearest that
-- corp's holdings centroid) plus an HQ star, each in the corp's identity colour. The
-- player's own border is the always-on home ring/HQ star (BL-085), so the reach layer
-- excludes the player to avoid a double-draw. Render-only chrome; the full gameplay
-- mechanic stays deferred in BL-182.
verify.goto_surface("Kepler")
verify.set_overlay("corporation")

-- Wide view: multiple rival corporations' reach rings should be visible at once,
-- reading as the corporate-landscape counterpart to the Country lens's national
-- borders (plus the player's always-on home ring).
verify.set_zoom(3)
verify.capture("corporate_reach_wide")

-- Framed on the player's HQ: the player's always-on home ring + HQ star (BL-085)
-- should be centred, coexisting with rival reach rings nearby (corporations have
-- borders too — the player's is the home ring, rivals' are the reach rings).
local buildings = verify.buildings()
for _, b in ipairs(buildings) do
    if b.player then
        frame_tile(b.x, b.y, 12)
        verify.capture("corporate_reach_player_hq")
        break
    end
end
