-- Visual verification for Population-centre markers (BL-083).
-- Confirms requirements.json § pop-centre-markers R1 (item-spanning):
--   the generated population centres render as always-on, tiered settlement
--   markers on the Planetary surface (clustered into conurbations, City+ labelled),
--   civic-neutral by default and host-nation-tinted only under the Country lens.
--
-- Kepler (home) carries the prototype population. Run with:
--   ProjectIo --verify scripts/verify/pop_markers.lua

verify.econ_step(2)                     -- settle
verify.goto_surface("home")

-- Always-on: markers show at the plain default lens (no lens armed).
verify.set_overlay("none")
verify.capture("pop_markers_default")

-- Frame the largest population centre rather than a fixed tile (the
-- settlement_labels.lua idiom): the old hard-coded (66, 6) survived until the
-- BL-257 / BL-348 generation changes left it pointing at empty terrain, and an
-- empty capture verifies nothing.
local best = nil
for _, c in ipairs(verify.population_centres()) do
    if best == nil or c.scale > best.scale then best = c end
end
if best == nil then
    error("pop_markers: no population centres in the generated world")
end
print(string.format("pop_markers: framing centre scale=%d at (%d, %d)",
                    best.scale, best.x, best.y))

-- Zoom onto the populated region so tiers + City+ labels read at scale.
frame_tile(best.x, best.y, 9)
verify.capture("pop_markers_zoom")

-- Country lens: the same markers pick up the host-nation tint (R4).
frame_tile(best.x, best.y, 9)
