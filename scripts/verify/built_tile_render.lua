-- Visual verification for the built-tile render: what a hex carrying a building
-- actually looks like, close enough that each mark occupies real pixels.
--
-- WHAT IT SHOULD SHOW (BL-596, buildings over the hex; Ben, 2026-08-24: "Remove
-- building background. Buildings should be drawn over the hex, not completely on
-- top."). A built tile carries a large silhouette drawn straight onto LIVE GROUND —
-- terrain hue, substrate grain, cover pattern, landform relief and the active lens
-- wash all keep rendering under and around the glyph. There is no plate. The frames
-- to read it on are the terrain and lens pair below: if the hex under a silhouette is
-- a flat block of colour rather than textured ground, the plate has come back.
--
-- (This script was written against the OPPOSITE call — Ben, 2026-07-22, "ensure that
-- tiles do not get rendered behind buildings, they are fully swapped out" — which
-- BL-596 overturns. The framing is kept because it is the right framing; only the
-- thing being looked for changed.)
--
-- WHY THIS SCRIPT EXISTS. Every pre-existing canvas golden PASSED the 2026-07-22
-- change at 0.02-0.09% differing, because they all frame the whole body at ~4px per
-- tile — far too small for the fill of a handful of built tiles to move the diff past
-- the 0.5% threshold. The suite was blind to how a built tile actually renders. This
-- check zooms in far enough that the ground, the silhouette and the owner emblem each
-- occupy real pixels, so a regression in any of them fails the diff.
--
-- FRAMING IS RESOLVED FROM THE WORLD, NOT HARD-CODED (2026-08-24). It used to name a
-- rival cluster at (156,37)/(156,36)/(155,37)/(155,36) by grid coordinate, and those
-- coordinates had gone stale: generation has moved since, and the check was framing
-- OPEN OCEAN — passing, capturing, and looking at no building at all. A visual check
-- pointed at the wrong tile is worse than no check, because it reports green. The tile
-- now comes from verify.buildings(), the same accessor stacked_tile_ring.lua uses, so a
-- generation change re-aims the check instead of silently blinding it.
--
-- Run with: ProjectIo --verify scripts/verify/built_tile_render.lua

verify.econ_step(4)
verify.goto_surface("home")
verify.show_panel("economy", false)

-- A RIVAL's building by preference: the owner tint on a rival differs from the player's,
-- so this frame also carries the identity read. Falls back to any building rather than
-- failing, since what is being looked at is the RENDER, not whose it is.
local site = nil
for _, b in ipairs(verify.buildings()) do
    if not b.player then site = b; break end
end
if site == nil then
    for _, b in ipairs(verify.buildings()) do site = b; break end
end
verify.expect(site ~= nil, "found a building on the home body to frame")
assert(site, "no building anywhere on the home body")
print(string.format("BUILT_TILE framing (%d,%d) type=%s", site.x, site.y, site.type))

-- Plain canvas: the silhouette over live terrain, with no lens competing for the fill.
verify.center_tile(site.x, site.y, 44)
verify.capture("built_tile_plain")

-- A blend lens still tints a built tile — a lens the player chose must not go blind
-- exactly where their assets are — and since BL-596 the tile takes the province blend
-- like any other, so a built hex no longer stands out as the one crisp cell in its cell.
verify.set_overlay("resource")
verify.capture("built_tile_resource")

-- Population and Opportunity deliberately REPLACE the silhouette with a per-tile value
-- mark (BL-135). These two frames are the regression guard on that suppression: the
-- value mark stands alone and the silhouette must not come back.
verify.set_overlay("population")
verify.capture("built_tile_population")

verify.set_overlay("opportunity")
verify.capture("built_tile_opportunity")
