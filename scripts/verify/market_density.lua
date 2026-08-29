-- The Market ledger's price list at the resolutions people actually use.
--
-- WHY THIS EXISTS (Ben, 2026-08-29): "it seems your captures are not at
-- 1920x1080p, so what we see here is zoomed compared to my viewing. In other
-- words, the UI doesn't shrink at smaller resolutions."
--
-- He is right, and the arithmetic says why. `shell_column_width` is
-- 0.20 * disp_x clamped to [380, 460], so the column is 380 px at 1280 and 384 px
-- at 1920 — four pixels wider across the whole common range. But the chrome above
-- and below it is FIXED: the profile tile, and a bottom band whose height derives
-- from `minimap_width` = max(336, 0.28 * min(disp_x, disp_y)), which is 336 at
-- BOTH resolutions. So every pixel of the extra 360 rows at 1080p lands in the
-- ledger's own content height and nowhere else.
--
-- The consequence for a review: density judged at 720p is judged against roughly
-- HALF the content height the reviewer has. Every "only N rows fit" number this
-- sprint produced was measured on the wrong screen.
--
--   ProjectIo --verify scripts/verify/market_density.lua

local function market_at(w, h, tag)
    verify.window(w, h)
    verify.frames(2)
    verify.show_panel("market", true)
    verify.panel_view("market", 0) -- Prices
    shot(string.format("market_density_%s", tag))
    verify.show_panel("market", false)
    verify.frames(1)
end

verify.goto_surface("home")
verify.econ_step(12)

-- 1280x720 — BL-215's stated smallest supported display, and what every capture
-- in this sprint was taken at.
market_at(1280, 720, "1280x720")

-- 1920x1080 — Ben's display, and therefore the resolution a design review is
-- actually about.
market_at(1920, 1080, "1920x1080")
