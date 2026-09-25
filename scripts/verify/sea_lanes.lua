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
-- eye, because no binding counts drawn lanes. (Measured 2026-09-25 off the
-- map's own geometry: 13 of seed 32's 16 lanes join neighbouring regions and
-- draw 3-6 px long at 1920x1080, 11 of them on one archipelago; three run 36,
-- 81 and 136 px, and the longest shares its line with a colonial tie. The run
-- log's "history_lapse:" line prints the short count.)
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

-- BL-1124 R1 -- THE THREE CANDIDATE LANE FORMS at the SAME close frame, one
-- capture each: (a) a bright core line inside the soft band, (b) an anchor at
-- each end drawn over the ties, (c) the band bowed south off the tie's line.
-- Switched by a REAL PRESS on the temporary selector over the map's top edge
-- (history_lapse.cpp, `draw_lane_form_selector`), because no binding sets the
-- form and a press is what the live app does. The coordinates are the radio
-- buttons' at 1920x1080 on this world: the row sits one frame-height above
-- the map's top edge (map 848,349 824x382, per the run log's
-- "history_lapse pane:" line). Each capture shows which radio is lit and the
-- legend's "Sea lane" swatch in the same form, so a press that missed shows.
-- TEMPORARY, with the selector: U3 removes the two forms Ben does not pick.
local forms = {
    {name = "a_core_line",   x = 1141, y = 331},
    {name = "b_end_anchors", x = 1260, y = 331},
    {name = "c_bowed_arc",   x = 1403, y = 331},
}
for _, f in ipairs(forms) do
    verify.click(f.x, f.y)
    verify.frames(2)
    verify.capture("sea_lanes_form_" .. f.name)
end
verify.click(forms[1].x, forms[1].y) -- leave the default form selected
