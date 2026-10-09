-- Ground bake worker pool — fill-time bench (docs/ui/RENDERING.md § Chunks,
-- cache and invalidation). NOT a capture check: it takes no captures.
--
-- Walks the five stepped zoom rungs (kMinZoom * 2^k, k = 0..4) at the
-- reference 1720x1080 window with the POOL path running under --verify, and
-- lets each rung fill. The timing is printed by ground_layer itself, one
-- "GROUND_FILL tier ..." line per rung change (wall ms from the rung change to
-- the active tier covering the viewport) and one "GROUND_FILL far page" line
-- per body switch.
--
-- Run (both env vars are required; without IO_GROUND_BENCH every bake is
-- synchronous and the times are serial bake cost, not fill time):
--   IO_GROUND_BENCH=1 [IO_GROUND_WORKERS=1] ProjectIo --verify scripts/verify/ground_fill_bench.lua
-- IO_GROUND_WORKERS overrides the pool size (1 = the single-worker baseline).

local kMinZoom = 1.2531328  -- body_surface_canvas.cpp kMinZoom (~1.253)

-- Lua here has no clock, so a rung is "filled" when the stand-in is gone and
-- the published active-chunk count has held still for `still` polls.
local function settle(still)
    local last, same = -1, 0
    for _ = 1, 4000 do
        verify.frames(10)
        local s = verify.ground_stats()
        if s.standin == 0 and s.chunks == last then
            same = same + 1
            if same >= still then return s end
        else
            same = 0
        end
        last = s.chunks
    end
    return verify.ground_stats()
end

verify.window(1720, 1080)
verify.goto_surface("home")
verify.set_overlay("none")
verify.set_border_band(false)
settle(30)
for k = 0, 4 do
    verify.set_zoom(kMinZoom * (2 ^ k))
    local s = settle(30)
    print(string.format("GROUND_BENCH rung %d  draw_r %.2f  tier %.0f  chunks %d",
                        k, s.draw_r, s.tier_ppr, s.chunks))
end
verify.set_border_band(true)
