-- The Planetary canvas's vector layer at every rung (sprint 51; PLANETARY.md
-- § Draw-loop cost model; TECH_FOUNDATIONS.md § Target hardware: 60 fps at
-- every zoom).
--
-- NOT a golden check: it asserts nothing. Per rung, plain and under two lenses
-- (Market, categorical; Population, sequential — both wash the ground, BL-1250;
-- Throughput, which also draws the whole road and lane network at full weight
-- at every rung, BL-1257),
-- it waits for the ground to be final, takes one still capture (rungs 0 and 1 —
-- the two the vector budget bites at — and rungs 3 and 4, where the close-zoom
-- seam draws, BL-1251), then pans 300 frames and writes the
-- frame-timing CSV (frame_csv: total/build/submit/present ms + draw volume).
--
-- Run (Release, from the build directory), with the pass meter on so the per-pass
-- vertex/CPU table prints to stdout:
--   IO_CANVAS_PASS_LOG=1 ProjectIo --verify scripts/verify/canvas_vector_perf.lua
-- FRAME TOTALS NEED THE PLAY PATH (BL-1260): plain --verify re-snapshots the world
-- into the ground's bake source every frame (~15 ms of build_ms that play never
-- pays), so read the CSVs from a run with IO_GROUND_BENCH=1 as well. The pass
-- meter's canvas split is the same either way (PLANETARY.md § Draw-loop cost model).
-- CSVs: canvas_still_ and canvas_pan_<lens>_rung<k>.csv; captures: screenshots/canvas_<lens>_rung<k>.png

local kMinZoom = 1.2531328  -- body_surface_canvas.cpp kMinZoom (~1.253)

local function poll(pred, limit)
    for i = 1, limit or 20000 do
        verify.frames(1)
        local s = verify.ground_stats()
        if pred(s) then return s, i end
    end
    return verify.ground_stats(), -1
end
local function final_(s) return s.pending_uploads == 0 and s.chunks > 0 end

verify.window(1720, 1080)
verify.goto_surface("home")
-- The WHOLE master, baked now, on either --verify ground path (BL-1246;
-- ground_master_complete.lua): the synchronous path bakes only what a frame
-- draws, so a poll of master_ready never completes there. Fails loudly.
do
    local r = verify.ground_complete_master(600)
    if not r.ok or r.ready ~= r.total or not r.current then
        error(string.format("canvas_vector_perf: the home master did not complete (%d/%d, current %s, %.0f ms)",
                            r.ready, r.total, tostring(r.current), r.ms))
    end
    print(string.format("CANVAS_MASTER %d/%d complete in %.0f ms", r.ready, r.total, r.ms))
end

for _, lens in ipairs({ "none", "market", "population", "throughput" }) do
    verify.set_overlay(lens)
    for k = 0, 4 do
        verify.set_zoom(kMinZoom * (2 ^ k))
        verify.set_pan(0, 0)
        verify.frames(3)
        poll(final_, 2000)  -- every lens draws the ground now (BL-1250)
        verify.frames(5)
        if k <= 1 or k >= 3 then  -- the wide rungs, and the close-zoom seam (BL-1251)
            verify.capture(string.format("canvas_%s_rung%d", lens, k))
        end
        -- Still: the vector layer alone (no ground chunk is streaming in).
        verify.frame_reset()
        for i = 1, 120 do verify.frames(1) end
        verify.frame_csv(string.format("canvas_still_%s_rung%d.csv", lens, k))
        verify.frame_reset()
        for i = 1, 300 do
            verify.add_pan(-24, 0)
            verify.mouse(400 + (i % 200), 400 + (i % 150))
            verify.frames(1)
        end
        verify.frame_csv(string.format("canvas_pan_%s_rung%d.csv", lens, k))
        print(string.format("CANVAS_RUNG lens %s rung %d done", lens, k))
    end
end
verify.set_overlay("none")
