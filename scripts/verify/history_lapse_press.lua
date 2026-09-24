-- The lapse rounds' presses, made for real (BL-829; split into two rounds by
-- BL-860, 2026-09-09; a third lapse round -- Exploration -- added by BL-946,
-- 2026-09-13).
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
-- BL-946 ADDS A THIRD LAPSE ROUND, Exploration, between Empires and
-- Industrialisation: the wizard walks SIX rounds -- System, Life, Culture,
-- Empires, Exploration, Industrialisation. BL-1068 (round six plays the span)
-- makes the last one a lapse round too, so all four pass rounds replay a real,
-- recorded span on the same shared engine (EXPLORATION.md sec The engine is
-- shared).
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
--
-- TWO FOOTER ROWS, NOT ONE. Round 3's footer sits under its Drawdown preference
-- block; the PASS rounds have no preference block at all, so theirs sits lower.
-- Both were read off captures rather than derived, and a pass round's footer is
-- the same on every one of them because the decision block is empty on all
-- four (Culture carries its own Drawdown row, the other three carry none).
local NEXT3_X,  NEXT3_Y  = 603, 975   -- the last planetology round's Next
-- BL-914 (2026-09-11): Restart is retired on every lapse round -- a scrubber
-- makes it redundant and its row went to the ranking board. The SAME slot is
-- now the Pause/Play toggle; a landed round is parked (paused) under --verify
-- (m_golden_dir set), so this button reads "Play", and pressing it both
-- starts playing AND -- because the playhead sits at the record's last year
-- here -- re-parks it at the first year, exactly the observable behaviour A3
-- below has always asserted. The scrubber beside it takes no --verify hook
-- yet and is not exercised by this script; a live click is owed for it.
local TRANSPORT_X, TRANSPORT_Y = 483, 150
local REROLL_X, REROLL_Y = 483, 957   -- a pass round's Reroll
local BACKP_X,  BACKP_Y  = 363, 995   -- a pass round's Back
local NEXTP_X,  NEXTP_Y  = 603, 995   -- a pass round's Next

-- Park on round 3, then walk ONE round by pressing Next for real.
verify.generation_stage(1)
verify.frames(4)
local round, rounds = verify.wizard_round()
verify.expect(rounds == 6,
              "the wizard walks SIX rounds -- System, Life, Culture, Empires, "
              .. "Exploration, Industrialisation (got " .. rounds .. ")")
verify.expect(round == 1, "parked on round 2, Life (0-based " .. round .. ")")
verify.expect(verify.history_powers() == 0,
              "round 2 carries no history record (the case is not pre-loaded)")
verify.capture("press_00_round3_before_next")

-- A1 -- THE HISTORY AUTO-STARTS ON THE ROUND-3 NEXT PRESS. No Run button
-- exists any more: arriving on the round IS the instruction to run it. Under
-- --verify the run resolves synchronously (it adopts the record the harness's
-- own world already carries), so the record is in hand once the frames below
-- have run.
verify.click(NEXT3_X, NEXT3_Y)
verify.frames(6)
round = select(1, verify.wizard_round())
verify.expect(round == 2, "NEXT on round 2 lands on round 3, CULTURE (0-based "
                          .. round .. ")")
local powers = verify.history_powers()
verify.expect(powers > 0,
              "NEXT on round 2 auto-starts round 3's pass -- no Run press needed ("
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

-- A3 -- THE TRANSPORT'S PLAY PRESS IS REACHABLE, and pressing it FROM THE END
-- re-parks the playhead at the record's own first year (BL-914's stand-in for
-- the retired Restart: resuming from the very end restarts, since a Play
-- button that visibly does nothing would be worse than the button it
-- replaced). Frozen under --verify regardless of the resulting `playing`
-- state, so the year holds at the start for the capture.
verify.history_year(last)
verify.frames(2)
verify.click(TRANSPORT_X, TRANSPORT_Y)
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
              "Reroll re-runs round 3's pass and leaves a record on the round")
verify.capture("press_03_after_reroll")

-- ── BL-860: THE HISTORY IS ITS OWN ROUND ──────────────────────────────────
--
-- A5 -- ROUND 4 (EMPIRES) RUNS ITS OWN PASS ON ARRIVAL. The auto-start is
-- generic now rather than round 3's special case, so the Next press that
-- moves onto round 4 starts round 4's pass exactly as round 3's started
-- round 3's. `history_powers` reads the round the wizard is ON, so a record
-- here is round 4's own record and not round 3's showing through.
verify.click(NEXTP_X, NEXTP_Y)
verify.frames(6)
round = select(1, verify.wizard_round())
verify.expect(round == 3, "NEXT on round 3 lands on round 4, EMPIRES (0-based "
                          .. round .. ")")
local r5 = verify.history_powers()
verify.expect(r5 > 0,
              "round 4 runs its OWN pass on arrival (" .. r5 .. " powers)")
verify.capture("press_04_round5_history")

-- ── BL-916: THE EVENT LAYER IS ON THE ROUND ────────────────────────────────
--
-- A9 -- at the END of the span the ticker has lines to show and the arc's
-- "destroyed" figure came off recorded realm_ended events rather than off an
-- absence in the closing sample (which was structurally zero on every world).
-- Parked at the last year so the capture carries the fullest ticker; the
-- marker window is derived from the span, so whatever happened inside the
-- last ~1/30th of it is ringed on the map in the same frame.
local ev_first, ev_last = verify.history_span()
verify.history_year(ev_last)
verify.frames(2)
local ev_total, ev_ended, ev_broke = verify.history_events()
verify.expect(ev_total > 0,
              "round 4's record carries typed events (" .. ev_total .. " at "
              .. ev_last .. ")")
verify.expect(ev_ended > 0 or ev_broke > 0,
              "the record names at least one realm ending or breaking away ("
              .. ev_ended .. " ended, " .. ev_broke .. " broke away) -- a quiet "
              .. "world here means the emit sites are not wired, not a quiet world")
verify.capture("press_04b_round4_ticker_at_end")

-- A10 -- PARKED ON THE MOMENT A REALM BROKE AWAY (kind 3 = broke_away, kind 2
-- = realm_ended, era_timelapse.hpp). The ticker's newest line is that event,
-- bright, and its region is ringed on the map in the same frame -- the
-- "breaks away" capture BL-916's DONE WHEN names. A world with no secession
-- falls back to the last realm ending, which every world so far has.
local park = verify.history_event_year(3)
if park < ev_first then park = verify.history_event_year(2) end
verify.expect(park >= ev_first,
              "the record carries a break-away or a realm ending to park on ("
              .. park .. ")")
verify.history_year(park)
verify.frames(2)
verify.capture("press_04c_round4_break_away")

-- A6 -- ROUND 4's REROLL IS ITS OWN. Same slot, different round: it must leave a
-- record on round 4 rather than clearing it or acting on round 3's.
verify.click(REROLL_X, REROLL_Y)
verify.frames(6)
verify.expect(verify.history_powers() > 0,
              "Reroll re-runs round 4's pass and leaves a record on the round")
verify.capture("press_05_round5_after_reroll")

-- ── BL-946: EXPLORATION IS THE THIRD LAPSE ROUND ──────────────────────────
--
-- A7 -- NEXT FROM 4 (EMPIRES) LANDS ON 5, EXPLORATION -- the round runs its
-- own span (1200 -> 1660 CE) on arrival, on the SAME auto-start every other
-- lapse round uses, and its record is its OWN (`exploration_timelapse`), not
-- the Empires round's showing through.
verify.click(NEXTP_X, NEXTP_Y)
verify.frames(6)
round = select(1, verify.wizard_round())
verify.expect(round == 4, "NEXT on round 4 lands on round 5, EXPLORATION (0-based "
                          .. round .. ")")
local r6 = verify.history_powers()
verify.expect(r6 > 0,
              "round 5 (Exploration) runs its OWN pass on arrival (" .. r6 .. " powers)")
local exp_first, exp_last = verify.history_span()
verify.expect(exp_last > exp_first,
              "Exploration's own record spans years (" .. exp_first .. " -> "
              .. exp_last .. ")")
verify.capture("press_06_round6_exploration")

-- A11 -- EXPLORATION'S OWN REROLL, same slot, same contract as A4/A6: it must
-- leave a record on THIS round, not clear it or touch Empires' above it.
verify.click(REROLL_X, REROLL_Y)
verify.frames(6)
verify.expect(verify.history_powers() > 0,
              "Reroll re-runs round 5's (Exploration) pass and leaves a record "
              .. "on the round")
verify.capture("press_07_round6_after_reroll")

-- A12 -- NEXT FROM 5 (EXPLORATION) LANDS ON 6, INDUSTRIALISATION (BL-1068) --
-- the last round, and a lapse round like the three before it: it runs its
-- own span (1660 -> 1960 CE) on arrival and its record is its OWN
-- (`industrialisation_timelapse`), not Exploration's showing through. Its
-- own press is "Begin" and this script does not touch it -- pressing it
-- would generate a world and leave the wizard entirely.
verify.click(NEXTP_X, NEXTP_Y)
verify.frames(6)
round = select(1, verify.wizard_round())
verify.expect(round == 5, "NEXT on round 5 lands on round 6, INDUSTRIALISATION (0-based "
                          .. round .. ")")
local r7 = verify.history_powers()
verify.expect(r7 > 0,
              "round 6 (Industrialisation) runs its OWN pass on arrival (" .. r7 .. " powers)")
local ind_first, ind_last = verify.history_span()
verify.expect(ind_first == exp_last,
              "Industrialisation opens where Exploration closed (" .. ind_first
              .. " vs " .. exp_last .. ")")
verify.expect(ind_last > ind_first,
              "Industrialisation's own record spans years (" .. ind_first .. " -> "
              .. ind_last .. ")")

-- A13 -- THE LAPSE PLAYS: mid-span and at the close are DIFFERENT instants of
-- one record, both holding ground. Captured at both so the round is seen
-- mid-play and where it ends, not only as the frame it lands on.
local ind_mid = ind_first + (ind_last - ind_first) // 2
verify.history_year(ind_mid)
verify.frames(2)
local at_mid = verify.history_powers()
local lit_mid, pts_mid = verify.history_industry()
verify.capture("press_08_round7_industrialisation_mid")
verify.history_year(ind_last)
verify.frames(2)
local at_end = verify.history_powers()
local lit_end, pts_end, crossings = verify.history_industry()
verify.expect(at_mid > 0 and at_end > 0,
              "the Industrialisation span holds ground mid-span and at its close ("
              .. at_mid .. " -> " .. at_end .. " powers)")
verify.capture("press_08b_round7_industrialisation_end")

-- A14 -- ROUND 6 SHOWS INDUSTRY (BL-1080). The span credits industry points
-- and lights furnaces region by region; the record carries both, and the map
-- (ember marks), the board (the Ind column) and the ticker (crossings) draw
-- them. By 1960 the verify seed has crossed furnaces on the map and holds
-- points on the board, and industry GROWS across the span rather than
-- standing still -- the "the map barely changes 1810-1960" finding this item
-- was filed on.
print("[history_lapse_press] industry mid " .. ind_mid .. ": " .. lit_mid
      .. " regions lit, " .. string.format("%.0f", pts_mid) .. " points; end " .. ind_last
      .. ": " .. lit_end .. " regions lit, " .. string.format("%.0f", pts_end)
      .. " points; " .. crossings .. " crossings in the record")
-- CROSSINGS ARE REPORTED, NOT REQUIRED, and that is a measured fact rather
-- than a softened check (BL-1080, 2026-09-24). A region crosses the furnace
-- only if Stage 4 gave its ground a lag (settlement.cpp), and Stage 4 does not
-- run on an antiquity-epoch world -- which is the world the wizard builds, even
-- though its spans run on to 1960. So the default verify seed records NO
-- crossing, and the map has none to mark. What IS required: a record with
-- crossings marks them by 1960, and a record without marks nothing.
if crossings > 0 then
  verify.expect(lit_end > 0,
                "by 1960 regions have crossed the furnace and are marked on the map ("
                .. lit_end .. " lit of " .. crossings .. " crossings)")
else
  print("[history_lapse_press] REPORT: the Industrialisation span records NO furnace "
        .. "crossing on this seed (Stage 4 lags are not drawn on an antiquity-epoch "
        .. "world); the map's ember layer is empty and the board's industry column is "
        .. "the round's industry")
  verify.expect(lit_end == 0, "no crossing recorded, none marked (" .. lit_end .. ")")
end
verify.expect(pts_end > 0,
              "by 1960 the board's industry column holds points (" .. string.format("%.0f", pts_end) .. ")")
verify.expect(lit_end >= lit_mid and pts_end > pts_mid,
              "industry grows across the span (" .. lit_mid .. " -> " .. lit_end .. " lit, "
              .. string.format("%.0f", pts_mid) .. " -> " .. string.format("%.0f", pts_end) .. " points)")
-- Park on the LAST crossing the span recorded, so the ticker's newest line is
-- a furnace lighting (kind 17 = furnace_lit, era_timelapse.hpp).
local lit_year = verify.history_event_year(17)
if lit_year >= ind_first then
  verify.history_year(lit_year)
  verify.frames(2)
  verify.capture("press_08c_round7_furnace_lit")
end

-- A8 -- BACK WALKS THE LADDER DOWN ONE RUNG AT A TIME, and a finished record is
-- NOT discarded and re-run on the way past. That guard is the reason the
-- auto-start is conditioned on the arriving round being empty.
verify.click(BACKP_X, BACKP_Y)
verify.frames(4)
round = select(1, verify.wizard_round())
verify.expect(round == 4, "Back from round 6 lands on round 5, EXPLORATION (0-based "
                          .. round .. ")")
verify.expect(verify.history_powers() > 0,
              "round 5's (Exploration) record survived the trip to round 6 and back")
local back_first = select(1, verify.history_span())
verify.expect(back_first == exp_first,
              "round 5 shows its OWN record again, not round 6's (" .. back_first
              .. " vs " .. exp_first .. ")")

verify.click(BACKP_X, BACKP_Y)
verify.frames(4)
round = select(1, verify.wizard_round())
verify.expect(round == 3, "Back from round 5 lands on round 4, EMPIRES (0-based "
                          .. round .. ")")
verify.expect(verify.history_powers() > 0,
              "round 4's record survived the walk up to round 6 and back")

verify.click(BACKP_X, BACKP_Y)
verify.frames(4)
round = select(1, verify.wizard_round())
verify.expect(round == 2, "Back from round 4 lands on round 3, CULTURE (0-based "
                          .. round .. ")")
verify.expect(verify.history_powers() > 0,
              "round 3's record survived the walk up to round 6 and back")
verify.capture("press_09_back_on_round4")
