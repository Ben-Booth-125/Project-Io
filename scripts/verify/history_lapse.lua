-- Wizard round 4: the time-lapse map and its top-16 board (BL-829 / BL-830).
--
--   ProjectIo --verify scripts/verify/history_lapse.lua
--
-- WHAT THIS CHECKS, and what it deliberately cannot. Round 4 replaces the
-- wizard's globe with a 2D MAP and plays the recorded Era -1 ownership history
-- across it, with an ordered, capped board of sixteen polities beside it. A
-- globe shows a world; a map shows a FRONTIER (Ben, 2026-09-08, at the live
-- app), and the frontier is this round's whole subject.
--
-- The review question is the same one ages_replay.lua asks and for the same
-- reason: the subject is TIME, so a single frame says almost nothing. Three
-- captures across one span are the minimum that can show a frontier moving and
-- a board re-ranking. What has to MOVE between them is the map's colour
-- coverage, the year stamp, and the order of the rows.
--
-- IT CANNOT PROVE RUN IS REACHABLE. A capture proves a surface renders; only a
-- live click proves a press on it lands. BL-829's own "done when" says so, and
-- the live watch is recorded in the delivery, not here.
--
-- THE RUN IS SYNCHRONOUS HERE, which is the only reason this script is
-- affordable. `verify.history_run` adopts the record the harness's own world
-- already carries (`m_generation_report`) rather than running the project's most
-- expensive pass a second time to reproduce a record it is holding. Playback is
-- frozen under --verify exactly as the globe's rotation is, so `history_year` is
-- the only thing that moves the playhead and no capture can race an animation.
--
-- 1920x1080, the design-review resolution.

verify.window(1920, 1080)

local function shot(name)
    verify.frames(2)
    verify.capture(name)
end

-- Enter round 4 and take the record. The first wizard frame after a screen
-- switch is a warmup — the surface build and the tile-to-region walk both land
-- on it — so it is captured and discarded, as main_menu.lua does.
verify.history_run()
verify.frames(4)
verify.capture("history_lapse_warmup")

-- The span. Generation's ancient era runs 400 BCE -> 0 CE
-- (world_params::prehistory_years = 400, era_minus_one.hpp), so these are its
-- first year, its middle, and its last. The wizard clamps to the record's own
-- span, so a retuned era degrades to a repeated end frame rather than to an
-- empty one.
local marks = {
    { -400, "history_lapse_1_origin" },
    { -200, "history_lapse_2_spread" },
    {    0, "history_lapse_3_settled" },
}

for _, m in ipairs(marks) do
    verify.history_year(m[1])
    shot(m[2])
end

-- The clipping ledger over those frames. The board is a four-column table of
-- generated region names in a third-width column, which is exactly the shape
-- that overruns; the year stamp and the counters line are snprintf into fixed
-- buffers, which is the other one.
verify.expect_no_clipping("history_lapse")
