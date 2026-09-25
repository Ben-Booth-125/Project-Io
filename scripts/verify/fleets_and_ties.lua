-- FLEETS AND TIES (BL-1095; Ben, 2026-09-24, sprint 47 rulings R13;
-- EXPLORATION.md § Force persists now and § The colonial tie is a sea lane;
-- STARTUP.md § Round 5).
--
--   ProjectIo --verify scripts/verify/fleets_and_ties.lua
--
-- WHAT THIS CHECKS. Round 5 draws the fleets that are there: a dashed tie
-- overlord -> subject from 'falls under the overlordship of' (or 'buys the
-- province of') that vanishes -- or fades to a trade line -- at 'refuses
-- renewal and breaks from'; a treaty arc between party capitals that snaps at
-- 'breaks its treaty with'; a hull at each capital scaled by the navy; a
-- harbour that silts; a sail crossing on every 'sails against'; a landing on
-- a seat taken across water.
--
-- THE HARNESS'S OWN WORLD (seed 0, the library's reference world), which
-- carries eleven ties, over a hundred treaties, thirty-odd wet campaigns and
-- three over-water captures on its Exploration record -- everything here has
-- something to photograph. The item asked for seed 13 or 41 (the colonial
-- worlds); `verify.new_world(N)` cannot pin one from a script today, because
-- `app::setup_world` builds only when no world exists (BL-1085's contract),
-- so a call on the harness's already-built world is a no-op -- the rows below
-- read identically after new_world(13) and new_world(41) (2026-09-25). A
-- kind the world never recorded is skipped with a line saying so.
-- Every assertion reads `history_fleets` / `history_tie_state`, which count
-- off the SAME derivation the map draws from -- a capture proves the glyph
-- renders where the readout says it is. That a tie fades within a
-- screen-second under playback, and that a hull shrinks as the ticker says
-- the fleet lapses, is Ben's live click (R4). Playback is frozen under
-- --verify, so `history_year` alone moves it.
--
-- 1920x1080, the design-review resolution.

verify.window(1920, 1080)

-- Every capture logs what the frame holds, off the map's own derivation, so
-- the log reads as a reading and not only as a list of files.
local function shot(name)
    verify.frames(2)
    local f = verify.history_fleets()
    print(string.format("fleets_and_ties: %s -- ties %d, trade lines %d, arcs %d, hulls %d, harbours %d, sails %d, landings %d",
          name, f.ties or -1, f.trade_lines or -1, f.arcs or -1, f.hulls or -1,
          f.harbours or -1, f.sails or -1, f.landings or -1))
    verify.capture(name)
end

-- Event kinds, as era_timelapse.hpp numbers them (the wire form).
local treaty_formed    = 12
local sea_leg_campaign = 24

verify.history_run(2) -- round 5, Exploration, on the harness's own world
verify.frames(4)
verify.capture("fleets_and_ties_warmup")

local first, last = verify.history_span()
verify.expect(last > first, "round 5 carries a record with a span (" .. first .. " -> " .. last .. ")")
local window = math.max(1, (last - first) // 30) -- lapse_marker_window_years

local fl = verify.history_fleets()
verify.expect(fl.ties ~= -1, "the round's derived fields are built (history_fleets reads a frame)")
print(string.format("fleets_and_ties: recorded %d ties, %d arcs, %d sails, %d landings; navy peak %.0f",
      fl.ties_recorded or -1, fl.arcs_recorded or -1, fl.sails_recorded or -1,
      fl.landings_recorded or -1, fl.navy_peak or -1))
verify.expect((fl.ties_recorded or 0) > 0, "the record carries at least one colonial tie")

-- THE TIE: the last tie whose freeing leaves a whole marker window inside
-- the span (the wizard clamps the playhead to the record, so a freeing at
-- the close could never be photographed faded); else the last binding.
-- A freeing that falls in the BINDING's own year counts: on this record the
-- span's opening rounds bind a subject and see it refuse renewal the same
-- round, three rounds running (19 -> 228 at 1200, 1204, 1208; the sim's own
-- doing, reported for the main session) -- so the tie stands one window and
-- fades, which is exactly the fade to photograph. A freeing some years after
-- the binding is preferred when the record has one.
local function pick_tie()
    local ties = verify.history_ties()
    local later_pick, same_pick, bound_pick = nil, nil, nil
    for _, t in ipairs(ties) do
        print(string.format("fleets_and_ties:   tie %d -> %d bound %d freed %s%s trade %s bought %s",
              t.overlord, t.subject, t.bound, tostring(t.freed), t.refused and " (refused)" or "",
              tostring(t.trade), tostring(t.bought)))
        if t.freed and t.freed + window <= last then
            if t.freed > t.bound then later_pick = t else same_pick = t end
        end
        if t.bound + window <= last then bound_pick = t end
    end
    return later_pick or same_pick or bound_pick
end
local pick = pick_tie()
verify.expect(pick ~= nil, "a tie with a marker window inside the span exists")
if pick then
    print(string.format("fleets_and_ties: tie %d -> %d bound %d freed %s trade %s",
          pick.overlord, pick.subject, pick.bound, tostring(pick.freed), tostring(pick.trade)))
    verify.history_year(pick.bound)
    verify.expect(verify.history_tie_state(pick.subject) == "tie",
                  "at the binding year the subject's tie is drawn (" .. pick.bound .. ")")
    shot("fleets_and_ties_tie_a_bound")
    if pick.freed then
        verify.history_year(pick.freed)
        local at = verify.history_tie_state(pick.subject)
        verify.expect(at == "tie" or at == "fading",
                      "at the freeing year the tie is still on the frame, starting to fade (" .. at .. ")")
        shot("fleets_and_ties_tie_b_freed")
        verify.history_year(pick.freed + window)
        local after = verify.history_tie_state(pick.subject)
        verify.expect(after == "none" or after == "trade",
                      "a window after the freeing the tie is gone or carried on as trade (" .. after .. ")")
        shot("fleets_and_ties_tie_c_after")
    end
end

-- THE HULLS AND HARBOURS: at the close, a realm with a navy wears a hull.
verify.history_year(last)
local close = verify.history_fleets()
if (close.navy_peak or 0) > 0 then
    verify.expect(close.hulls > 0, "at the close a realm with a navy wears a hull at its capital")
else
    print("fleets_and_ties: no realm ever held a navy on this record; hulls skipped")
end
if close.harbours == 0 then print("fleets_and_ties: no built port at the close; harbours skipped") end
shot("fleets_and_ties_close")

-- THE SAIL: the last wet campaign with a window to spare, a third of the
-- way across the water.
do
    local y = verify.history_event_year(sea_leg_campaign, -1, last - window)
    if y < first then
        print("fleets_and_ties: no sea_leg_campaign on this record; sail skipped")
    else
        verify.history_year(y + window // 3)
        local mid = verify.history_fleets()
        verify.expect(mid.sails > 0, "a sail is crossing a third of a window after the launch")
        shot("fleets_and_ties_sail")
    end
end

-- THE LANDING: the last over-water seat capture with a window to spare, as
-- the beach-head fans out.
do
    local y = verify.history_landing_year(last - window)
    if y < first then
        print("fleets_and_ties: no over-water seat capture on this record; landing skipped")
    else
        verify.history_year(y + (window * 3) // 4)
        local at = verify.history_fleets()
        verify.expect(at.landings > 0, "a landing is drawn three quarters of a window after the capture")
        shot("fleets_and_ties_landing")
    end
end

-- THE TREATY ARC: the last treaty formed with a window to spare.
do
    local y = verify.history_event_year(treaty_formed, -1, last - window)
    if y < first then
        print("fleets_and_ties: no treaty_formed on this record; arc skipped")
    else
        verify.history_year(y)
        local at = verify.history_fleets()
        verify.expect(at.arcs + at.trade_lines > 0,
                      "at a treaty's forming an arc (or a freed tie's trade line) is on the frame")
        shot("fleets_and_ties_arc")
    end
end

verify.expect_no_clipping("fleets_and_ties")
