-- BL-1244 (ground never magnified) — docs/ui/RENDERING.md § Level of detail.
-- Requirement group `ground-never-magnified`.
--
-- Walks the five stepped zoom rungs (kMinZoom * 2^k, k = 0..4) at the
-- reference 1720x1080 window and at a 4K-height window, and for each rung
-- prints the frame HUD's ground line as data (verify.ground_stats): the drawn
-- hex radius (hex_size * zoom — the ground quad's own scale, not the polygon
-- fills' 1 px border-inset; F34), the tier it drew from, texel/px (tier px per hex / drawn px per
-- hex — >= 1.0 is minified, < 1.0 is MAGNIFIED, the thing the rule forbids),
-- the bake cost of the chunks that rung needed (verify bakes synchronously,
-- so the timing is the bake itself, not a worker race), and the resident
-- texture bytes. One capture per rung, frame HUD on, border band off so the
-- ground is judged bare.
--
-- Then the R4 mid-fill frame: with the --verify bake-everything path turned
-- off for ONE frame (verify.ground_fill_limit), a rung change shows what
-- stands in while the new tier's chunks fill — the nearest finer tier resident
-- in view, else the nearest coarser one, the far page only where nothing
-- closer is (F34). Then the ZOOM-OUT mid-fill: back down a rung with bakes
-- frozen, the finer tier just left must stand in (not the far page).
--
-- Run: ProjectIo --verify scripts/verify/ground_sharpness.lua

local kMinZoom = 1.2531328  -- body_surface_canvas.cpp kMinZoom (~1.253)

local function walk(tag, w, h)
    verify.window(w, h)
    verify.frames(2)
    for k = 0, 4 do
        verify.ground_stats_reset()     -- the bakes this rung needs, alone
        verify.set_zoom(kMinZoom * (2 ^ k))
        verify.frames(2)                -- request -> bake -> publish settles
        local s = verify.ground_stats()
        local slot = s.slots[s.active_slot]
        print(string.format(
            "GROUND %s %dx%d rung %d  draw_r %.2f  tier %.0f  texel/px %.3f  chunks %d",
            tag, s.window_w, s.window_h, k, s.draw_r, s.tier_ppr, s.texel_per_px, s.chunks))
        for i = 0, 7 do
            local r = s.slots[i]
            if r and r.ppr > 0 and (r.bakes > 0 or r.resident_bytes > 0) then
                print(string.format(
                    "GROUND   slot %d ppr %.0f  bakes %d  bake_ms %.1f  ms/chunk %.1f  resident %.1f MB",
                    i, r.ppr, r.bakes, r.bake_ms,
                    r.bakes > 0 and r.bake_ms / r.bakes or 0,
                    r.resident_bytes / (1024 * 1024)))
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

-- R4, the MID-FILL frame (run first, away from the home framing so the walk's
-- bakes stay near-clean): rung 2 bakes the 48 px tier, then with chunk bakes frozen
-- (ground_fill_limit 0) the canvas steps to rung 3, re-centred west. The new
-- 96 px tier has no chunk ready, so the frame must show the 48 px tier
-- standing in where it is resident, and the far page only beyond it.
-- The spot is south-west land on the home body (tile 215,70), clear of the
-- home framing; the rung-3 frame is centred 26 columns west, so its west
-- half lies beyond the rung-2 tier's resident set.
verify.window(1720, 1080)
verify.center_tile(215, 70, kMinZoom * 4)
verify.frames(2)
verify.ground_fill_limit(0)
verify.center_tile(189, 70, kMinZoom * 8)
verify.frames(2)
do
    local s = verify.ground_stats()
    print(string.format("GROUND midfill draw_r %.2f  tier %.0f  texel/px %.3f  chunks %d  stand-in %d",
                        s.draw_r, s.tier_ppr, s.texel_per_px, s.chunks, s.standin))
end
verify.capture("ground_sharpness_midfill")
verify.ground_fill_limit(-1)
verify.frames(2)
verify.capture("ground_sharpness_midfill_filled")
-- Zoom-out mid-fill: rung 3 is now resident around column 189; step back to
-- rung 2 framed at column 175 — overlapping that set, west of rung 2's own
-- resident set around 215 — with bakes frozen. The 96 px tier just left is finer and resident in view, so it
-- stands in — minified, crisp — rather than the far page.
verify.ground_fill_limit(0)
verify.center_tile(175, 70, kMinZoom * 4)
verify.frames(2)
do
    local s = verify.ground_stats()
    print(string.format("GROUND zoomout-midfill draw_r %.2f  tier %.0f  chunks %d  stand-in %d",
                        s.draw_r, s.tier_ppr, s.chunks, s.standin))
end
verify.capture("ground_sharpness_zoomout_midfill")
verify.ground_fill_limit(-1)
verify.frames(2)
verify.goto_surface("home")

walk("ref", 1720, 1080)
walk("4k", 3840, 2160)

verify.set_border_band(true)
verify.show_panel("frame_hud", false)
