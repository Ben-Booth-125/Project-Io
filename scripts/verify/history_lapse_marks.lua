-- THE MARKS THAT EARN THEIR PLACE (BL-1094; Ben, 2026-09-24, sprint 47
-- rulings R10; STARTUP.md § Identity across the rounds).
--
--   ProjectIo --verify scripts/verify/history_lapse_marks.lua
--
-- WHAT THIS CHECKS. There is no blanket event ring (Ben, 2026-09-16). Four
-- kinds earn a map mark, each its own glyph: a seat captured draws a ring in
-- the winner's colour at the fallen seat; a break-away or schism draws a
-- crack from the parent's seat to the successor's; a civilisation formed
-- draws a two-tone diamond that STAYS at the coining region; a capital moved
-- slides the seat dot old -> new over the marker window. A corridor promoted
-- to Post Road pulses once along its length. A realm ending and a creed
-- preached draw nothing.
--
-- Each capture parks the playhead ON the last event of one kind on round 4
-- (the ring, the crack, the slide read at t = 0, the diamond at its coining)
-- and one more a half-window later (the ring and crack half faded, the dot
-- half way, the diamond unchanged). Round 6 parks on the last Post Road
-- promotion for the pulse. A kind the verify world never recorded is skipped
-- with a line saying so rather than failing: the record is the seed's, not
-- the surface's.
--
-- A capture proves each glyph renders where its event says; that a ring
-- fades within a screen-second under playback is the live click (R3).
-- Playback is frozen under --verify, so `history_year` alone moves it.
--
-- 1920x1080, the design-review resolution.

verify.window(1920, 1080)

local function shot(name)
    verify.frames(2)
    verify.capture(name)
end

-- Event kinds, as era_timelapse.hpp numbers them (the wire form).
local seat_captured       = 1
local broke_away          = 3
local capital_moved       = 4
local road_promoted       = 5
local civilisation_formed = 6
local schism              = 16

-- Round 4.
verify.history_run(1)
verify.frames(4)
verify.capture("history_lapse_marks_warmup4")

local first, last = verify.history_span()
verify.expect(last > first, "round 4 carries a record with a span (" .. first .. " -> " .. last .. ")")
local window = math.max(1, (last - first) // 30) -- lapse_marker_window_years

-- The LAST event of the kind with a whole marker window still inside the
-- span: the wizard clamps the playhead to the record, so an event at the
-- close could never be photographed half faded. `tier` is -1 for any.
local function mark(kind, label, tier)
    local y = verify.history_event_year(kind, tier or -1, last - window)
    if y < first then
        print("history_lapse_marks: no " .. label .. " event on this record; skipped")
        return
    end
    verify.history_year(y)
    shot("history_lapse_marks_" .. label .. "_a_at")
    verify.history_year(y + window // 2)
    shot("history_lapse_marks_" .. label .. "_b_half")
end

mark(seat_captured,       "seat_captured")
mark(broke_away,          "broke_away")
mark(schism,              "schism")
mark(civilisation_formed, "civilisation_formed")
mark(capital_moved,       "capital_moved")

-- The diamond STAYS: a whole window after the last coining it is still there.
do
    local y = verify.history_event_year(civilisation_formed)
    if y >= first then
        verify.history_year(math.min(last, y + window * 2))
        shot("history_lapse_marks_civilisation_formed_c_stays")
    end
end

-- Round 6: the Post Road pulse, on the last tier-3 promotion.
verify.history_run(3)
verify.frames(4)
verify.capture("history_lapse_marks_warmup6")
local f6, l6 = verify.history_span()
verify.expect(l6 > f6, "round 6 carries a record with a span (" .. f6 .. " -> " .. l6 .. ")")
do
    first, last = f6, l6
    window = math.max(1, (last - first) // 30)
    mark(road_promoted, "post_road", 3)
end

verify.expect_no_clipping("history_lapse_marks")
