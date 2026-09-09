-- Round 4's presses, made for real (BL-829, revised 2026-09-09).
--
--   ProjectIo --verify scripts/verify/history_lapse_press.lua
--
-- WHY IT EXISTS. A verifier-visual capture proves a surface RENDERS and says
-- nothing about whether a press on it lands anywhere. This is an acceptance
-- script in the click_injection.lua idiom: every press below goes through
-- ImGui's real event queue and hit-test, and every claim is a verify.expect
-- rather than a golden.
--
-- WHAT CHANGED, AND WHY THE OLD SCRIPT WENT RED (Ben, 2026-09-09: "we can
-- retire the 'run' button. Wire that to auto start when next is clicked in
-- phase 3"). The previous version asserted "round 4 opens with no history until
-- Run is pressed" and then clicked the Run slot. Both encode a design that no
-- longer exists, so three assertions failed on a healthy round -- the script was
-- wrong, not the code. Restoring the button to keep a check green would have
-- been the wrong repair.
--
-- A1 IS NOW THE INTERESTING ONE, and it is why this script walks rather than
-- parks. The auto-start hangs off the round-3 NEXT press, so a script that jumps
-- straight to round 4 with verify.generation_stage(3) bypasses the very thing it
-- is meant to check -- it would pass while asserting nothing, which is the
-- failure mode this repo keeps paying for. So round 3's footer geometry was
-- READ OFF A CAPTURE rather than guessed (the footer follows each round's
-- preference block, and round 3 has one where round 4 has none), and the walk is
-- a single real press from a known position.
--
-- Determinism: measured in frames, never in sleeps.

verify.window(1920, 1080)

-- The wizard's left column at 1920x1080, read off captures.
local NEXT3_X,  NEXT3_Y  = 603, 975   -- round 3's Next (its footer sits under Drawdown)
local RESTART_X, RESTART_Y = 483, 150 -- round 4's Restart slot
local REROLL_X, REROLL_Y = 483, 957   -- round 4's Reroll

-- Park on round 3, then walk ONE round by pressing Next for real.
verify.generation_stage(2)
verify.frames(4)
verify.expect(verify.history_powers() == 0,
              "round 3 carries no history record (the case is not pre-loaded)")
verify.capture("press_00_round3_before_next")

-- A1 -- THE HISTORY AUTO-STARTS ON THE ROUND-3 NEXT PRESS. No Run button
-- exists any more: arriving on the round IS the instruction to run it. Under
-- --verify the run resolves synchronously (it adopts the record the harness's
-- own world already carries), so the record is in hand once the frames below
-- have run.
verify.click(NEXT3_X, NEXT3_Y)
verify.frames(6)
local powers = verify.history_powers()
verify.expect(powers > 0,
              "NEXT on round 3 auto-starts the history -- no Run press needed ("
              .. powers .. " powers)")
verify.capture("press_01_after_next")

-- A2 -- the record spans real calendar years, and the map shows DIFFERENT states
-- across them. A replay returning the same slice at both ends is one slice drawn
-- twice, which is the failure ages_replay.lua was written after.
local first, last = verify.history_span()
verify.expect(last > first,
              "the record spans years (" .. first .. " -> " .. last .. ")")

verify.history_year(first)
verify.frames(2)
local at_first = verify.history_powers()
verify.history_year(last)
verify.frames(2)
local at_last = verify.history_powers()
verify.expect(at_first > 0 and at_last > 0,
              "both ends of the span hold ground ("
              .. at_first .. " -> " .. at_last .. " powers)")

-- A3 -- RESTART IS A REACHABLE PRESS. It re-parks the playhead at the record's
-- own first year, so the year after the press is the span's start.
verify.history_year(last)
verify.frames(2)
verify.click(RESTART_X, RESTART_Y)
verify.frames(3)
verify.capture("press_02_after_restart")

-- A4 -- REROLL IS A REACHABLE PRESS and re-runs the pass rather than clearing
-- the round. Since 2026-09-09 it also advances world_params::era_seed, so it
-- plays the SAME ground through a different four thousand years; what it must
-- never do is disturb the planetology rounds above it, which is why the era got
-- a seed of its own rather than folding the roll into params.seed.
verify.click(REROLL_X, REROLL_Y)
verify.frames(6)
verify.expect(verify.history_powers() > 0,
              "Reroll re-runs the pass and leaves a record on the round")
verify.capture("press_03_after_reroll")
