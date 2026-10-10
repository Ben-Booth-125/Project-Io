-- Ground one master — the fill-time bench (BL-1246; docs/ui/RENDERING.md
-- § Level of detail; docs/tech/TECH_FOUNDATIONS.md § Target hardware).
-- NOT a capture check: it takes no captures and asserts nothing.
--
-- With the POOL path running under --verify (IO_GROUND_BENCH=1), measures
-- what the five budgets ask, at the reference 1720x1080 window:
--   1. the whole home master from a cold visit (ground_layer prints
--      "GROUND_FILL master ..." with its wall time and RAM);
--   2. per zoom rung: the time from the rung change to the final ground
--      ("GROUND_FILL level ..."), then 300 frames of sustained pan written to
--      ground_pan_rung<k>.csv (frame_csv: total/build/submit/present ms);
--   3. a re-bake: a building placed with the whole body in view
--      ("GROUND_FILL re-bake ... after the source moved");
--   4. a first visit to another body ("GROUND_FILL visit body ..." — the view
--      final — and "GROUND_FILL master ..." for its whole master).
-- Lua here has no clock: ground_layer prints the wall times itself.
--
-- Run (Release, from the build directory):
--   IO_GROUND_BENCH=1 IO_GROUND_NO_BACKGROUND=1 ProjectIo --verify scripts/verify/ground_fill_bench.lua
-- IO_GROUND_NO_BACKGROUND keeps the other bodies unbaked, so step 4 is a true
-- first visit; IO_GROUND_WORKERS overrides the pool size.

local kMinZoom = 1.2531328  -- body_surface_canvas.cpp kMinZoom (~1.253)

-- Poll a frame at a time until pred(stats) holds; returns stats, frames.
local function poll(pred, limit)
    for i = 1, limit or 200000 do
        verify.frames(1)
        local s = verify.ground_stats()
        if pred(s) then return s, i end
    end
    return verify.ground_stats(), -1
end
local function whole(s) return s.master_total > 0 and s.master_ready >= s.master_total end
local function final_(s) return s.pending_uploads == 0 and s.chunks > 0 end
-- A rung change is read one frame late (the canvas writes its request as it
-- draws): wait for the request's radius to move before judging "final".
local function rung_final(prev_r)
    return function(s) return math.abs(s.draw_r - prev_r) > 0.5 and final_(s) end
end

verify.window(1720, 1080)
verify.set_overlay("none")
verify.set_border_band(false)

-- 1. The home master, cold.
verify.goto_surface("home")
local s, n = poll(whole)
print(string.format("GROUND_BENCH home master %d/%d chunks after %d frames; RAM all bodies %.2f GB",
                    s.master_ready, s.master_total, n, s.ram_total / 2^30))

-- 2. The rungs: rung change -> final, then a sustained pan.
local last_r = verify.ground_stats().draw_r
for k = 0, 4 do
    verify.set_zoom(kMinZoom * (2 ^ k))
    local r, f = poll(k == 0 and final_ or rung_final(last_r), 600)
    last_r = r.draw_r
    print(string.format("GROUND_BENCH rung %d  draw_r %.2f  level %.0f px/r  texel/px %.2f  chunks %d  final after %d frame(s)  uploads so far %d",
                        k, r.draw_r, r.level_ppr, r.texel_per_px, r.chunks, f, r.uploads))
    verify.frames(10)
    verify.frame_reset()
    for i = 1, 300 do
        verify.add_pan(-24, 0)
        verify.frames(1)
    end
    verify.frame_csv(string.format("ground_pan_rung%d.csv", k))
    local p = verify.ground_stats()
    print(string.format("GROUND_BENCH rung %d pan done: pending uploads %d, GPU level bytes %.1f MB",
                        k, p.pending_uploads, p.slots[p.active_slot].gpu_bytes / 2^20))
end
-- And back down the ladder, one rung a frame-settle at a time.
for k = 3, 0, -1 do
    verify.set_zoom(kMinZoom * (2 ^ k))
    local r, f = poll(rung_final(last_r), 600)
    last_r = r.draw_r
    print(string.format("GROUND_BENCH down to rung %d  level %.0f  final after %d frame(s)", k, r.level_ppr, f))
end

-- 3. A re-bake: the whole body in view, one building placed.
verify.set_zoom(kMinZoom)
poll(final_, 600)
verify.set_balance(1000000.0)
verify.place_mode("extraction", "iron_ore")
print("GROUND_BENCH build: " .. tostring(verify.build_first_valid()))
verify.frames(240)

-- 4. A first visit to another body.
verify.goto_surface("inner")
local v, vf = poll(final_, 20000)
print(string.format("GROUND_BENCH inner body: view final after %d frames  master %d/%d", vf, v.master_ready, v.master_total))
local m, mf = poll(whole)
print(string.format("GROUND_BENCH inner body master whole after %d more frames; RAM all bodies %.2f GB",
                    mf, m.ram_total / 2^30))
verify.goto_surface("home")
local h, hf = poll(final_, 2000)
print(string.format("GROUND_BENCH back home: final after %d frame(s), master %d/%d", hf, h.master_ready, h.master_total))
verify.set_border_band(true)
