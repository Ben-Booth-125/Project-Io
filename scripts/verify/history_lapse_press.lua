-- Round 4's RUN press, made for real (BL-829).
--
--   ProjectIo --verify scripts/verify/history_lapse_press.lua
--
-- WHY THIS IS A SECOND SCRIPT. history_lapse.lua captures the surface; a capture
-- proves a surface RENDERS and says nothing about whether a press on it lands
-- (DELIVERY.md § A UI requirement needs a live check). This one is an ACCEPTANCE
-- script in the click_injection.lua idiom: every press below goes through
-- ImGui's own event queue and the button's own hit-test, and the verdict is
-- verify.expect rather than a golden.
--
-- It is NOT a substitute for a human watching the span play. It cannot be: the
-- playhead is frozen under --verify, so nothing here observes motion on wall
-- time. What it does close is the half a capture leaves open — that Run, Restart
-- and Reroll are reachable presses on the round rather than bindings that happen
-- to work.
--
-- Determinism: measured in frames, never in sleeps. The wizard's left column is
-- authored to a fixed geometry, so the button centres below are constants; if
-- the layout moves, A1 fails loudly rather than the script pressing empty space.

verify.window(1920, 1080)

-- The wizard's left column, at 1920x1080. Read off the round-4 capture.
local RUN_X,    RUN_Y    = 483, 150   -- Run / Restart share the slot
local REROLL_X, REROLL_Y = 483, 957

-- Park on round 4 rather than walking to it by pressing Next four times. The
-- footer is NOT pinned to the panel's bottom — it follows the round's preference
-- block, which is a different height per round — so a walk driven by one
-- constant Y presses empty space on three of the four rounds and silently stays
-- on round 1. The wizard's own walk is BL-816's subject and was verified by a
-- live click there; the subject HERE is round 4's controls, whose positions are
-- stable because the round takes no preference rows.
verify.generation_stage(3)
verify.frames(4)
verify.capture("press_00_round4_before_run")

-- A1 — the walk landed on round 4 with no history yet. If the Next presses
-- missed, this is 0 for the wrong reason, which A2 then catches.
verify.expect(verify.history_powers() == 0,
              "round 4 opens with no history until Run is pressed")

-- A2 — RUN IS A REACHABLE PRESS. Under --verify the run resolves synchronously
-- (it adopts the record the harness's own world already carries), so the record
-- is in hand by the time the frames below have run.
verify.click(RUN_X, RUN_Y)
verify.frames(4)
local powers = verify.history_powers()
verify.expect(powers > 0,
              "pressing Run takes the history record (" .. powers .. " powers)")
verify.capture("press_01_after_run")

-- A3 — the record spans real calendar years, and the map shows DIFFERENT states
-- across them. A replay that returns the same slice at both ends is one slice
-- drawn twice, which is the failure ages_replay.lua was written after.
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

-- A4 — RESTART IS A REACHABLE PRESS. It re-parks the playhead at the record's
-- own first year, so the year after the press is the span's start.
verify.history_year(last)
verify.frames(2)
verify.click(RUN_X, RUN_Y)
verify.frames(3)
verify.capture("press_02_after_restart")

-- A5 — REROLL IS A REACHABLE PRESS on round 4 and re-runs the pass rather than
-- clearing the round. What it cannot yet do is produce a DIFFERENT history: the
-- era has no seed of its own in world_params, which the round says on its face.
verify.click(REROLL_X, REROLL_Y)
verify.frames(4)
verify.expect(verify.history_powers() > 0,
              "Reroll re-runs the pass and leaves a record on the round")
verify.capture("press_03_after_reroll")
