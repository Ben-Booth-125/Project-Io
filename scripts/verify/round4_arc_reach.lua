-- BL-891 -- CAN A PLAYER ACTUALLY REACH ROUND 4 AND READ THE ARC OFF IT?
--
--   ProjectIo --verify scripts/verify/round4_arc_reach.lua
--
-- WHY IT EXISTS, AND WHY IT IS NOT A GOLDEN. BL-891's whole subject is that a
-- rolled world cannot be JUDGED unless the player can see whether the arc
-- happened in it. A capture proves the readout renders; it says nothing about
-- whether the round carrying it is reachable. history_lapse_press.lua walks the
-- ladder with real presses and went red here on 2026-09-11 -- every Next after
-- round 3 missed -- so this script isolates WHERE the footer actually is rather
-- than asserting a coordinate somebody read off a capture two days earlier.
--
-- THE SUSPECT IS BL-891 ITSELF. The arc readout adds three lines to the left
-- column, and the footer follows the column's content. A surface that grows can
-- push its own Next off the bottom at 1920x1080, which would make the readout
-- unreadable precisely because it exists.
--
-- Determinism: measured in frames, never in sleeps.

verify.window(1920, 1080)

verify.generation_stage(1)
verify.frames(4)
verify.click(603, 975)            -- round 2's Next, the one press known to land
verify.frames(6)

local round = select(1, verify.wizard_round())
verify.expect(round == 2, "parked on round 3, CULTURE (0-based " .. round .. ")")
verify.capture("arc_00_round3")

-- WALK THE FOOTER DOWN THE COLUMN. Each press is real; the first one that
-- changes the round is where the button is. Reported rather than asserted --
-- the point is to LOCATE the control, and a hard-coded expectation here would
-- re-create the brittleness that sent the other script red.
local found = -1
for _, y in ipairs({995, 1005, 1017, 1027, 1040, 1055, 1070}) do
    if found < 0 then
        verify.click(603, y)
        verify.frames(4)
        local r = select(1, verify.wizard_round())
        if r ~= 2 then
            found = y
            verify.capture("arc_01_advanced_at_y" .. y)
        end
    end
end

verify.expect(found > 0,
              "round 3's Next is reachable somewhere in the column (found at y="
              .. found .. "; -1 means NO press in 995..1070 advanced the round)")

local round2 = select(1, verify.wizard_round())
verify.expect(round2 == 3,
              "the press landed on round 4, EMPIRES (0-based " .. round2 .. ")")
verify.expect(verify.history_powers() > 0,
              "round 4 carries a record, so the arc readout has numbers to show ("
              .. verify.history_powers() .. " powers)")
verify.capture("arc_02_round4_arc")
