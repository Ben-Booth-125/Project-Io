-- Visual verification for the glance-then-stick hover card (BL-228/230,
-- retires BL-200's dwell-to-open).
--
-- The behaviour under test, in Ben's words: "make sure tooltips do not stick
-- when initially open, and take 1/2 a second before they appear, then 2
-- seconds before they stick. From cursor pov they are behind tiles until they
-- stick, but from a rendering pov they are always in front." So, two phases:
--   1. Hovering an entity for kHoverAppearDelay (30 frames @ 60 Hz = 0.5 s)
--      summons the card as a GLANCE - it tracks the live cursor like an
--      ordinary tooltip and does not yet own the pointer.
--   2. Past kHoverStickDelay (150 frames total = 2.5 s, i.e. 2 s after it
--      appeared) the card STICKS - freezes at its current position, stops
--      following the cursor, and is dismissed only once the pointer leaves
--      its bounds (+ the exit pad spanning the gap to the anchor).
--   3. In both phases the card renders above everything (ImGuiWindowFlags_
--      Tooltip) but never captures the pointer (NoInputs), so the Selection
--      band never opens from hover - only a click does.
--
-- This check is only possible because of verify.frames(n): capture() composits
-- exactly one frame, so before it existed no script could reach a 150-frame
-- hover delay, and none of this was covered.
--
-- Run with: ProjectIo --verify scripts/verify/hover_freeze.lua

verify.goto_surface("home")
verify.set_zoom(11.0)
verify.frames(2)

local cx = 640.0  -- canvas centre-ish, clear of the shell column and right chrome
local cy = 240.0

-- 1) Hold the pointer still until the appear delay elapses: the card shows up
--    as a glance. The Selection band must remain CLOSED behind it - hover
--    never opens it.
verify.mouse(cx, cy)
verify.frames(31)
verify.capture("hover_freeze_01_glance_appeared")

-- 2) Nudge the pointer a few pixels while still glancing (well under the
--    stick delay). Unlike the old always-frozen card, a glancing card TRACKS
--    the cursor - compare against capture 1: the card must have MOVED.
verify.mouse(cx + 10.0, cy + 6.0)
verify.frames(2)
verify.capture("hover_freeze_02_glance_tracks_cursor")

-- 3) Keep holding near the same spot until the stick delay elapses (total
--    hover ~153 frames on the same entity): the card freezes in place.
verify.frames(120)
verify.capture("hover_freeze_03_stuck")

-- 4) Nudge again now that it is stuck. Compare against capture 3: the card
--    must be in the SAME place, not shifted by the nudge.
verify.mouse(cx + 16.0, cy + 10.0)
verify.frames(3)
verify.capture("hover_freeze_04_pinned_after_nudge")

-- 5) Move well clear of the card's bounds: it is dismissed. (Up and left,
--    since the card is drawn above-right of its anchor.)
verify.mouse(cx - 260.0, cy + 260.0)
verify.frames(3)
verify.capture("hover_freeze_05_dismissed_on_exit")
