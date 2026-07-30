-- Visual verification for the FROZEN hover card (BL-228, retires BL-200's
-- dwell-to-open).
--
-- The behaviour under test, in Ben's words: "retire the hover opening selection
-- panel part. Hovering should just freeze a tooltip until the user's cursor exits
-- tooltip bounds." So:
--   1. Hovering an entity for kHoverDelay (20) frames summons the card.
--   2. The card FREEZES at the position it was summoned at - it does not follow
--      the pointer, so it can be read to the end of a line.
--   3. It persists while the pointer stays within its bounds (+ the exit pad that
--      spans the gap to the anchor), and is dismissed the moment the pointer
--      leaves them.
--   4. Hover NEVER opens the Selection band. Only a click does.
--
-- This check is only possible because of verify.frames(n): capture() composits
-- exactly one frame, so before it existed no script could reach a 20-frame hover
-- delay, and none of this was covered.
--
-- Run with: ProjectIo --verify scripts/verify/hover_freeze.lua

verify.goto_surface("home")
verify.set_zoom(11.0)
verify.frames(2)

local cx = 640.0  -- canvas centre-ish, clear of the shell column and right chrome
local cy = 240.0

-- 1) Hold the pointer still until the glance delay elapses: the card appears.
--    The Selection band must remain CLOSED behind it - hover no longer opens it.
verify.mouse(cx, cy)
verify.frames(25)
verify.capture("hover_freeze_01_summoned")

-- 2) Nudge the pointer a few pixels. Pre-BL-228 the card tracked the cursor; now
--    it stays pinned where it was summoned. Compare against capture 1: the card
--    must be in the SAME place, not shifted by the nudge.
verify.mouse(cx + 10.0, cy + 6.0)
verify.frames(3)
verify.capture("hover_freeze_02_pinned_after_nudge")

-- 3) Move well clear of the card's bounds: it is dismissed. (Up and left, since
--    the card is drawn above-right of its anchor.)
verify.mouse(cx - 260.0, cy + 260.0)
verify.frames(3)
verify.capture("hover_freeze_03_dismissed_on_exit")
