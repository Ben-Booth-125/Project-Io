-- The Planetary canvas's vector layer at every rung (sprint 51; PLANETARY.md
-- § Draw-loop cost model; TECH_FOUNDATIONS.md § Target hardware: 60 fps at
-- every zoom).
--
-- NOT a golden check: it asserts nothing. Per rung, plain and under one lens, it
-- waits for the ground to be final, takes one still capture (rungs 0 and 1 —
-- the two the vector budget bites at), then pans 300 frames and writes the
-- frame-timing CSV (frame_csv: total/build/submit/present ms + draw volume).
--
-- Run (Release, from the build directory), with the pass meter on so the per-pass
-- vertex/CPU table prints to stdout:
--   IO_CANVAS_PASS_LOG=1 ProjectIo --verify scripts/verify/canvas_vector_perf.lua
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
poll(function(s) return s.master_total > 0 and s.master_ready >= s.master_total end)

for _, lens in ipairs({ "none", "market" }) do
    verify.set_overlay(lens)
    for k = 0, 4 do
        verify.set_zoom(kMinZoom * (2 ^ k))
        verify.set_pan(0, 0)
        verify.frames(3)
        if lens == "none" then poll(final_, 2000) end
        verify.frames(5)
        if k <= 1 then
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
