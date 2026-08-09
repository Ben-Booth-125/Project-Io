-- Pan-cost measurement (the "stutter while panning" report, 2026-08-02).
--
-- NOT a golden check: it takes no captures and diffs nothing. It writes
-- frame-timing CSVs (one row per frame: total/build/submit/present/other/work
-- ms + draw volume) for offline analysis, via the verify.frame_* tap added to
-- the BL-249 frame-stats instrument.
--
-- Runs on the REAL renderer with the build's real vsync setting — nothing in
-- the app forces an offscreen driver under --verify — so present_ms is true
-- pacing. The sim stays paused (run_verify default), so `other_ms` here is the
-- event pump only; sim cost is measured elsewhere (econ_stability: 0.0018
-- ms/tick) and is not what panning exercises.
--
-- Run from the repo root:
--   build\ProjectIo.exe --verify scripts/verify/pan_perf.lua
-- CSVs land in the working directory as perf_*.csv.

verify.window(1720, 1080)          -- the live interactive size, not the 1280x720 golden size
verify.goto_surface("home")      -- the 180x84 full-tile body

-- One measured phase: place the view, warm up out of the measurement window,
-- then run 300 frames (the instrument's full ring) and dump it.
local function phase(zoom, pan_dx, out)
    verify.set_zoom(zoom)
    verify.set_pan(0, 0)
    verify.frames(30)              -- warm-up: first-layout costs age out of the ring
    verify.frame_reset()
    for i = 1, 300 do
        if pan_dx ~= 0 then
            verify.add_pan(pan_dx, 0)
            -- The OS cursor moves during a real middle-drag; feed the scripted
            -- cursor so hover lookups run each frame like they would live.
            verify.mouse(400 + (i % 200), 400 + (i % 150))
        end
        verify.frames(1)
    end
    verify.frame_csv(out)
end

-- Baseline (no pan) against sustained fast pan (~24 px/frame), across the zoom
-- range: whole-grid, mid, and the start-framing play zoom.
phase(0.9,  0,   "perf_static_z09.csv")
phase(0.9,  -24, "perf_pan_z09.csv")
phase(3.0,  0,   "perf_static_z3.csv")
phase(3.0,  -24, "perf_pan_z3.csv")
phase(11.0, 0,   "perf_static_z11.csv")
phase(11.0, -24, "perf_pan_z11.csv")
