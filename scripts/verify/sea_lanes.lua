-- Sea lanes on the Exploration round (BL-1097 R3, "a persistent lane line
-- from the events, drawn as its own water layer distinct from a colonial tie").
--
--   ProjectIo --verify scripts/verify/sea_lanes.lua
--
-- WHY IT EXISTS. Ben's live click (2026-09-25) did not find the lane line on a
-- wizard seed. The record carries lanes on 14 of the 16 library seeds (the
-- library sweep: 0 on seeds 12 and 37, 1-16 elsewhere), so the question is
-- whether the round DRAWS them where a player can see them. Seed 32 opens the
-- most (16), so it is the world to look at; a capture is the evidence, read by
-- eye, because no binding counts drawn lanes.
--
-- Pinned seed: this checks one world's drawing, not a property of every world.

verify.window(1920, 1080)
verify.new_world(32)
verify.frames(4)

-- Round 5 (Exploration), adopted from this world's own report.
verify.history_run(2)
verify.frames(4)
local first, last = verify.history_span()
verify.expect(last > first, "round 5 carries a record with a span (" .. first .. " -> " .. last .. ")")
verify.expect(first == 1200, "round 5 opens at 1200 CE (" .. first .. ")")

-- Mid-span and the close: a lane is drawn from the year its leg earns the tier
-- to the round's end, so the close shows every lane the span opened.
verify.history_year(1450)
verify.frames(2)
verify.capture("sea_lanes_1_1450")
verify.history_year(last)
verify.frames(2)
verify.capture("sea_lanes_2_close")
verify.expect(verify.history_powers() > 0,
              "realms hold ground at the close (" .. verify.history_powers() .. " powers)")
