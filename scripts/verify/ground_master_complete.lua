-- BL-1246 (ground one master): the home body's master ALWAYS completes — from
-- any state, on either --verify ground path. Asserts; errors loudly.
--
-- 2026-10-09: canvas_vector_perf.lua and lens_modes.lua polled master_ready and
-- "stalled" at 1960 / 2975 at the 1720x1080 home view. Cause: those runs were on
-- the SYNCHRONOUS --verify path (IO_GROUND_BENCH not set — a POSIX `VAR=1 cmd`
-- prefix sets nothing in PowerShell or cmd), which by design bakes only the
-- chunks a frame draws; pump never feeds the pool there, so a poll waits
-- forever at the view's coverage. verify.ground_complete_master() is the way a
-- script reaches a whole master on either path; on the POOL path (IO_GROUND_BENCH=1)
-- this also proves the background fill alone completes within a wall-clock bound.
--
-- States, each followed by a completion check:
--   1. a first visit (nothing baked);
--   2. after a sweep through every rung (the drawn level and its window move);
--   3. after a body switch away and back (the moon's master starts; home's stays);
--   4. after a burst of dirty chunks (30 economy days + a placed building: the
--      source moves, the sweep marks chunks dirty, they re-bake — some as patches).
--
-- Run: ProjectIo --verify scripts/verify/ground_master_complete.lua
--      IO_GROUND_BENCH=1 ProjectIo --verify scripts/verify/ground_master_complete.lua

local kMinZoom   = 1.2531328   -- body_surface_canvas.cpp kMinZoom
local kPoolBound = 300         -- s: the pool path's background fill, from any state
local kSyncBound = 600         -- s: ground_complete_master's own timeout

local function stats() return verify.ground_stats() end

local pool = stats().pool_path
print(string.format("[ground_master_complete] path: %s", pool and "POOL" or "SYNCHRONOUS"))

-- The pool path: pumping frames alone completes the master (no help).
local function pool_fills(label)
    if not pool then return end
    local t0 = stats().clock_ms
    local frames = 0
    while true do
        verify.frames(1)
        frames = frames + 1
        local s = stats()
        local secs = (s.clock_ms - t0) / 1000.0
        if s.master_total > 0 and s.master_ready >= s.master_total and s.master_current then
            print(string.format("[ground_master_complete] %s: pool filled %d/%d in %d frames, %.1f s",
                                label, s.master_ready, s.master_total, frames, secs))
            return
        end
        if secs > kPoolBound then
            error(string.format("ground_master_complete: %s: the pool path did not complete the master "
                                .. "within %d s (%d/%d, current %s, %d frames)", label, kPoolBound,
                                s.master_ready, s.master_total, tostring(s.master_current), frames))
        end
    end
end

-- Either path: the blocking bake completes and is current.
local function completes(label)
    local r = verify.ground_complete_master(kSyncBound)
    if not r.ok or r.ready ~= r.total or r.total <= 0 or not r.current then
        error(string.format("ground_master_complete: %s: master not complete (ok %s, %d/%d, current %s, %.0f ms)",
                            label, tostring(r.ok), r.ready, r.total, tostring(r.current), r.ms))
    end
    local s = stats()
    if s.master_ready ~= s.master_total or not s.master_current then
        error(string.format("ground_master_complete: %s: ground_stats disagrees (%d/%d, current %s)",
                            label, s.master_ready, s.master_total, tostring(s.master_current)))
    end
    print(string.format("[ground_master_complete] %s: complete %d/%d, current, %.0f ms",
                        label, r.ready, r.total, r.ms))
end

verify.window(1720, 1080)
verify.goto_surface("home")
verify.set_overlay("none")
verify.frames(3)

-- 1. A first visit.
pool_fills("first visit")
completes("first visit")

-- 2. Every rung (the drawn level changes; the view window moves with it).
for k = 0, 4 do
    verify.set_zoom(kMinZoom * (2 ^ k))
    verify.set_pan(0, 0)
    verify.frames(3)
end
verify.add_pan(-400, 0)
verify.frames(3)
pool_fills("after every rung")
completes("after every rung")

-- 3. A body switch away and back.
verify.goto_surface("moon")
verify.frames(10)
verify.goto_surface("home")
verify.frames(3)
pool_fills("after a body switch")
completes("after a body switch")

-- 4. A burst of dirty chunks: the world moves, the source follows.
local c0 = verify.ground_bake_counters()
verify.econ_step(30)
local placed = verify.build_first_valid()
verify.frames(35) -- past the 30-frame source cadence (pool path) / re-snapshot (sync path)
pool_fills("after a dirty burst")
completes("after a dirty burst")
local c1 = verify.ground_bake_counters()
print(string.format("[ground_master_complete] dirty burst: build %s; chunk re-bakes %d (patched %d)",
                    placed, (c1.chunk_rebakes or 0) - (c0.chunk_rebakes or 0),
                    (c1.chunk_patches or 0) - (c0.chunk_patches or 0)))

print("[ground_master_complete] PASS")
