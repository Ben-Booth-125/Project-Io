-- BL-1246 (ground one master) — docs/ui/RENDERING.md § Level of detail.
-- (Was BL-1244's never-magnify walk over the per-rung tiers; the tiers, the
-- 192 px tier and the stand-in it captured are retired.)
--
-- Walks the five stepped zoom rungs (kMinZoom * 2^k, k = 0..4) at the
-- reference 1720x1080 window and at a 4K-height window, and for each rung
-- prints the frame HUD's ground line as data (verify.ground_stats): the drawn
-- hex radius (hex_size * zoom — the ground quad's own scale; F34), the LEVEL
-- of the one master it drew from, texel/px (level px per hex / drawn px per
-- hex — >= 1.0 minified, < 1.0 magnified: only the top rung, reading the
-- 96 px master ~1.15x at the reference window, as accepted), the camera
-- squash (the one angle, every rung), the master's progress, and the RAM and
-- GPU bytes per level. One capture per rung, frame HUD on, border band off
-- so the ground is judged bare. Detail must not change between rungs — only
-- scale: every level is a downsample of the same master.
--
-- Then the first-visit frame: the "inner" planet visited with master bakes
-- frozen after a handful of chunks (verify.ground_fill_limit) — the far page
-- carries the frame where the master has not landed, and nothing else
-- stands in.
--
-- Run: ProjectIo --verify scripts/verify/ground_sharpness.lua
-- (Debug: the first frame bakes the home master — ~2 min on 14 workers.)

local kMinZoom = 1.2531328  -- body_surface_canvas.cpp kMinZoom (~1.253)

local function walk(tag, w, h)
    verify.window(w, h)
    verify.frames(2)
    for k = 0, 4 do
        verify.ground_stats_reset()     -- the bakes this rung needs, alone
        verify.set_zoom(kMinZoom * (2 ^ k))
        verify.frames(2)                -- request -> bake -> publish settles
        local s = verify.ground_stats()
        print(string.format(
            "GROUND %s %dx%d rung %d  draw_r %.2f  level %.0f  texel/px %.3f  sy %.4f  chunks %d  master %d/%d",
            tag, s.window_w, s.window_h, k, s.draw_r, s.level_ppr, s.texel_per_px, s.sy, s.chunks,
            s.master_ready, s.master_total))
        if s.draw_r <= 96.0 then
            verify.expect(s.texel_per_px >= 1.0 - 1e-3, string.format(
                "%s rung %d: at or under 96 px a level is never magnified (texel/px %.3f)",
                tag, k, s.texel_per_px))
        else
            -- Past the master's 96 px the master itself magnifies (accepted:
            -- ~1.15x at the reference window, ~2.3x at 4K height).
            verify.expect(s.level_ppr == 96.0, string.format(
                "%s rung %d: past 96 px the master is drawn (level %.0f)", tag, k, s.level_ppr))
        end
        verify.expect(s.texel_per_px <= 2.0 + 1e-3, string.format(
            "%s rung %d: the level is minified by at most 2:1 (texel/px %.3f)", tag, k, s.texel_per_px))
        for i = 0, 7 do
            local r = s.slots[i]
            if r and r.ppr > 0 and (r.bakes > 0 or r.ram_bytes > 0 or r.gpu_bytes > 0) then
                print(string.format(
                    "GROUND   slot %d ppr %.0f  bakes %d  bake_ms %.1f  ms/each %.2f  RAM %.1f MB  GPU %.1f MB",
                    i, r.ppr, r.bakes, r.bake_ms,
                    r.bakes > 0 and r.bake_ms / r.bakes or 0,
                    r.ram_bytes / (1024 * 1024), r.gpu_bytes / (1024 * 1024)))
            end
        end
        verify.capture(string.format("ground_sharpness_%s_rung%d", tag, k))
    end
end

verify.ground_stats_reset()
verify.goto_surface("home")
verify.set_overlay("none")
verify.set_border_band(false)
verify.show_panel("frame_hud", true)

walk("ref", 1720, 1080)
walk("4k", 3840, 2160)

-- The first visit, mid-bake: the inner planet, at most 6 master chunks.
verify.window(1720, 1080)
verify.ground_fill_limit(6)
verify.goto_surface("inner")
verify.frames(2)
do
    local s = verify.ground_stats()
    print(string.format("GROUND first-visit draw_r %.2f  level %.0f  chunks %d  master %d/%d",
                        s.draw_r, s.level_ppr, s.chunks, s.master_ready, s.master_total))
end
verify.capture("ground_sharpness_first_visit")
verify.ground_fill_limit(-1)
verify.frames(2)
verify.capture("ground_sharpness_first_visit_filled")
verify.goto_surface("home")

verify.set_border_band(true)
verify.show_panel("frame_hud", false)
