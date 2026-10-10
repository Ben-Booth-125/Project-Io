-- BL-1259 (ground disk cache) — the LIVE path's reading (TECH_FOUNDATIONS.md
-- § Target hardware: "a return to a body visited before shows its ground
-- within a few seconds, from the disk cache"). A measurement, not a golden:
-- it prints times, sizes and frame hitches, and fails only where the design is
-- broken (home re-baked, a spilled body re-baked instead of loading).
--
--   A. home -> inner -> home: the home master is pinned — no re-bake.
--   B. a third body forces the inner planet out (the non-home budget lowered
--      so both do not fit): its spill runs on the writer thread while frames
--      render (the hitch reading); a revisit streams it back from the cache —
--      time until the view is final and until the whole master is back;
--      then the same return as a RE-BAKE (the cache dropped), for comparison.
--
-- Run on the POOL path, with no speculative bakes, from a shell that sets the
-- variables (PowerShell: $env:IO_GROUND_BENCH=1; $env:IO_GROUND_NO_BACKGROUND=1):
--   ProjectIo --verify scripts/verify/ground_disk_cache_bench.lua

local function stats() return verify.ground_stats() end
local function counters() return verify.ground_bake_counters() end
local function line(fmt, ...) print(string.format("[ground_disk_cache_bench] " .. fmt, ...)) end

if not stats().pool_path then
    error("ground_disk_cache_bench: needs the pool path (IO_GROUND_BENCH=1 in the environment)")
end

local kBound = 600 -- s

-- Frames until pred() holds; returns seconds, frames, and the frame times.
local function until_(label, pred)
    local t0 = stats().clock_ms
    local last = t0
    local frames, worst, sum, slow = 0, 0, 0, 0
    while true do
        verify.frames(1)
        frames = frames + 1
        local s = stats()
        local dt = s.clock_ms - last
        last = s.clock_ms
        sum = sum + dt
        if dt > worst then worst = dt end
        if dt > 50 then slow = slow + 1 end
        if pred(s) then
            return (s.clock_ms - t0) / 1000.0, frames, worst, sum / frames, slow
        end
        if (s.clock_ms - t0) / 1000.0 > kBound then
            error(string.format("ground_disk_cache_bench: %s did not happen within %d s (%d/%d)",
                                label, kBound, s.master_ready, s.master_total))
        end
    end
end
local function master_whole(s) return s.master_total > 0 and s.master_ready >= s.master_total and s.master_current end
local function view_final(s) return s.view_final end

local function cache_entry(name)
    local id = verify.ground_body_id(name)
    for _, b in ipairs(verify.ground_cache().bodies) do
        if b.body == id then return b end
    end
    return nil
end

verify.window(1720, 1080)
verify.goto_surface("home")
verify.set_overlay("none")
local secs = until_("home master", master_whole)
line("home master baked in %.1f s (RAM all bodies %.2f GB)", secs, stats().ram_total / 2^30)

-- A. home -> inner -> home.
verify.goto_surface("inner")
local sv = until_("inner view", view_final)
local sm = until_("inner master", master_whole)
line("inner first visit: view final %.2f s, master whole %.2f s later (RAM all bodies %.2f GB)",
     sv, sm, stats().ram_total / 2^30)
local c0 = counters()
verify.goto_surface("home")
local hv, hf = until_("home view", view_final)
local c1 = counters()
verify.expect(c1.chunk_bakes == c0.chunk_bakes, "A: home re-baked nothing on return")
local he = cache_entry("home")
verify.expect(he and he.resident and he.pinned, "A: home resident and pinned")
line("A home return: view final in %.3f s (%d frames), %d chunk bakes", hv, hf, c1.chunk_bakes - c0.chunk_bakes)

-- Idle frame times (the hitch baseline).
local idle_n = 0
local _, _, idle_worst, idle_avg, idle_slow = until_("idle", function() idle_n = idle_n + 1; return idle_n >= 300 end)
line("idle frames (300): avg %.1f ms, worst %.1f ms, %d over 50 ms", idle_avg, idle_worst, idle_slow)

-- B. The moon forces the inner planet out.
verify.goto_surface("moon")
local mv = until_("moon view", view_final)
until_("moon master", master_whole)
local s_moon = stats()
line("moon first visit: view final %.2f s; RAM all bodies %.2f GB", mv, s_moon.ram_total / 2^30)
-- Lower the non-home budget below inner + moon and step home: the budget takes
-- the least recently visited (inner) — spilled on the writer thread.
verify.goto_surface("home")
verify.frames(5) -- the body switch's whole-view upload is not the spill's hitch
local prev = verify.ground_set_ram_budget(1.0)
local sp_secs, sp_frames, sp_worst, sp_avg, sp_slow = until_("spill written", function()
    return verify.ground_cache().pending == 0 and verify.ground_bake_counters().spills > 0
end)
line("B spill while rendering: %.2f s, %d frames, frame avg %.1f ms, worst %.1f ms, %d over 50 ms "
     .. "(idle avg %.1f, worst %.1f)", sp_secs, sp_frames, sp_avg, sp_worst, sp_slow, idle_avg, idle_worst)
for _, nm in ipairs({ "inner", "moon" }) do
    local e = cache_entry(nm)
    if e then
        line("B disk: %s %d files, %.2f GB (raw %.2f GB, ratio %.3f), resident %s", nm, e.files,
             e.bytes / 2^30, e.raw_bytes / 2^30, e.raw_bytes > 0 and e.bytes / e.raw_bytes or 0,
             tostring(e.resident))
    end
end
verify.ground_set_ram_budget(prev)

-- The return from the cache.
local cl0 = counters()
verify.goto_surface("inner")
local rv, _, rv_worst, rv_avg = until_("inner view (cache)", view_final)
local rm, _, rm_worst, rm_avg = until_("inner master (cache)", master_whole)
local cl1 = counters()
local nl = cl1.chunk_loads - cl0.chunk_loads
line("B return from cache: view final %.2f s, master whole %.2f s after that; %d loads, %d bakes, %d failures",
     rv, rm, nl, cl1.chunk_bakes - cl0.chunk_bakes, cl1.load_failures - cl0.load_failures)
line("B per load (worker): %.1f ms, of which read %.1f ms; frames avg %.1f / %.1f ms, worst %.1f / %.1f ms",
     nl > 0 and (cl1.load_ms - cl0.load_ms) / nl or 0, nl > 0 and (cl1.load_read_ms - cl0.load_read_ms) / nl or 0,
     rv_avg, rm_avg, rv_worst, rm_worst)
verify.expect(cl1.chunk_loads - cl0.chunk_loads > 0, "B: the return loaded from the cache")

-- A second spill and return: the files are already written (nothing to
-- write), so this reads the same files a second time.
do
    local p2 = verify.ground_set_ram_budget(1.0)
    verify.goto_surface("home")
    verify.frames(2)
    verify.ground_set_ram_budget(p2)
    local c2 = counters()
    verify.goto_surface("inner")
    local v2 = until_("inner view (cache, again)", view_final)
    local m2 = until_("inner master (cache, again)", master_whole)
    local c3 = counters()
    local n2 = c3.chunk_loads - c2.chunk_loads
    line("B second return from cache: view final %.2f s, master whole %.2f s after; %d loads, %d writes; "
         .. "per load %.1f ms (read %.1f ms)", v2, m2, n2, c3.chunk_writes - c2.chunk_writes,
         n2 > 0 and (c3.load_ms - c2.load_ms) / n2 or 0, n2 > 0 and (c3.load_read_ms - c2.load_read_ms) / n2 or 0)
end

-- The same return as a re-bake.
verify.goto_surface("home")
verify.frames(2)
verify.ground_drop("inner")
local cb0 = counters()
verify.goto_surface("inner")
local bv = until_("inner view (re-bake)", view_final)
local bm = until_("inner master (re-bake)", master_whole)
local cb1 = counters()
line("B return as a re-bake: view final %.2f s, master whole %.2f s after that; %d bakes",
     bv, bm, cb1.chunk_bakes - cb0.chunk_bakes)

-- C. An EXPENSIVE master through the cache: the home body (a verify lever —
-- the budget never spills home; this is the cost a home-sized body elsewhere
-- would pay). Spilled while the inner planet is on screen, then revisited.
verify.frames(5)
local sh = verify.ground_spill("home")
local hs_secs, hs_frames, hs_worst, hs_avg, hs_slow = until_("home spill written", function()
    return verify.ground_cache().pending == 0
end)
local he2 = cache_entry("home")
line("C home spill: %d chunks written in %.2f s while rendering (%d frames, avg %.1f ms, worst %.1f ms, "
     .. "%d over 50 ms); disk %.2f GB (raw %.2f GB, ratio %.3f)", sh.written, hs_secs, hs_frames, hs_avg,
     hs_worst, hs_slow,
     he2.bytes / 2^30, he2.raw_bytes / 2^30, he2.raw_bytes > 0 and he2.bytes / he2.raw_bytes or 0)
local ch0 = counters()
verify.goto_surface("home")
local hv2, _, hv2_worst, hv2_avg = until_("home view (cache)", view_final)
local hm2, _, hm2_worst, hm2_avg = until_("home master (cache)", master_whole)
local ch1 = counters()
local nh = ch1.chunk_loads - ch0.chunk_loads
line("C home return from cache: view final %.2f s, master whole %.2f s after that (vs %.1f s baked); "
     .. "%d loads, %d bakes, %d failures; per load %.1f ms (read %.1f ms); frames avg %.1f / %.1f ms, worst %.1f / %.1f ms",
     hv2, hm2, secs, nh, ch1.chunk_bakes - ch0.chunk_bakes, ch1.load_failures - ch0.load_failures,
     nh > 0 and (ch1.load_ms - ch0.load_ms) / nh or 0, nh > 0 and (ch1.load_read_ms - ch0.load_read_ms) / nh or 0,
     hv2_avg, hm2_avg, hv2_worst, hm2_worst)
verify.expect(nh > 0 and ch1.chunk_bakes == ch0.chunk_bakes, "C: home came back from the cache, nothing re-baked")
line("done")
