-- BL-1106 -- THE TICKER AND BOARD SPEAK THE HISTORY'S WORDS.
--
--   ProjectIo --verify scripts/verify/names_and_voice.lua
--
-- WHAT THIS CHECKS. Four things the round's voice owes the player, each read
-- off the round's own record under --verify (the harness world, seed 0):
--
--   1. The CULTURE board (round 3) reads "peoples" and "Homeland" and prints
--      no battle cells -- a migration is a diffusion with nothing fighting in
--      it (STARTUP.md § Round 3). Photographed; the header is ImGui's own text.
--   2. On EMPIRES (round 4) a civilisation, a creed and a schism are each
--      NAMED in the ticker in the year they happen: the civilisation and the
--      creed under the names the sim coined (the record's name table, BL-1106),
--      the schism naming both parties and the creed the seceding people left.
--      `verify.history_ticker` returns the lines the ticker shows, so the
--      wording is asserted, not only photographed. A seed that carries no
--      event of a kind skips that kind and says so.
--   3. On EXPLORATION (round 5) at its first year, NO inherited realm carries
--      the board's entry mark: the lagged slice is clamped to the record's own
--      first year, so a resumed span does not mark everybody a newcomer.
--      `verify.history_board_marks` counts the marks by the draw's own rule.
--   4. The clipping ledger over every frame -- the board is a six-column
--      table of generated names in a third-width column.
--
-- The footers' "no docs/ string" half of the item is a code fact (grep the
-- wizard's string literals), not a frame fact, and is checked there.
--
-- Playback is frozen under --verify, so `history_year` is the only thing that
-- moves the playhead and no capture races an animation. 1920x1080, the
-- design-review resolution.

verify.window(1920, 1080)

local function shot(name)
    verify.frames(2)
    verify.capture(name)
end

local function any_line(lines, needle)
    for _, l in ipairs(lines) do
        if string.find(l, needle, 1, true) then return l end
    end
    return nil
end

-- 1. ROUND 3 -- CULTURE: peoples / Homeland, no battle cells.
verify.history_run(0)
verify.frames(4)
verify.capture("names_and_voice_warmup")
local first3, last3 = verify.history_span()
verify.expect(last3 > first3, "round 3 carries a migration record (" .. first3 .. " -> " .. last3 .. ")")
verify.history_year(last3)
shot("names_and_voice_1_culture_board")

-- 2. ROUND 4 -- EMPIRES: the named moments, each at its own year.
verify.history_run(1)
verify.frames(4)
local first4, last4 = verify.history_span()
verify.expect(last4 > first4, "round 4 carries a record with a span (" .. first4 .. " -> " .. last4 .. ")")

local kinds = {
    -- { label, lapse_event_kind wire value, the phrase the line must carry }
    { "civilisation", 6,  "is settled as a shared way of life at" },
    { "creed",        7,  "is preached at" },
    { "schism",       16, "breaks with" },
}
for _, k in ipairs(kinds) do
    local label, kind, phrase = k[1], k[2], k[3]
    local y = verify.history_event_year(kind)
    if y >= first4 then
        verify.history_year(y)
        -- THE TICKER SITS AT THE CHART CHILD'S FOLD AT 1080p on round 4 (the
        -- History lean's text below it is what pushes it there), so the
        -- newest line -- the one under test -- is half-clipped on the resting
        -- frame. Scrolled to the foot for the shot, as history_lapse_empires
        -- does for the arc readout, and reset after.
        verify.scroll_panel("wizard_charts", 1.0)
        shot("names_and_voice_2_" .. label)
        verify.scroll_panel("", 0)
        local lines = verify.history_ticker()
        local line  = any_line(lines, phrase)
        verify.expect(line ~= nil,
                      "a " .. label .. " line is on the ticker in " .. y
                      .. " (phrase '" .. phrase .. "'; " .. #lines .. " lines shown)")
        if line == nil then
            -- The rows that DID show, so a miss reads as what crowded it off
            -- rather than as a blank.
            for i, l in ipairs(lines) do
                verify.expect(true, "  ticker row " .. i .. ": " .. l)
            end
        end
        if line ~= nil then
            verify.expect(string.find(line, "unnamed", 1, true) == nil,
                          "the " .. label .. " line names its parties off the record: " .. line)
            if label == "schism" then
                -- The phrase, not the word: "an unnamed creed" carries "creed" too, so the
                -- fallback branch would pass the check it exists to fail (the cold review).
                verify.expect(string.find(line, "and its creed, ", 1, true) ~= nil
                              and string.find(line, "unnamed creed", 1, true) == nil,
                              "the schism line names the creed as well as both parties: " .. line)
            end
        end
    else
        verify.expect(true, "seed carries no " .. label .. " event on round 4; that line is not checked here")
    end
end

-- 3. ROUND 5 -- EXPLORATION at its first year: inherited realms are not newcomers.
verify.history_run(2)
verify.frames(4)
local first5, last5 = verify.history_span()
verify.expect(last5 > first5, "round 5 carries a record with a span (" .. first5 .. " -> " .. last5 .. ")")
verify.history_year(first5)
shot("names_and_voice_3_1200")
verify.expect(verify.history_powers() > 0,
              "realms hold ground at " .. first5 .. ", so the board has rows to mark ("
              .. verify.history_powers() .. " realms)")
local marks = verify.history_board_marks()
verify.expect(marks == 0,
              "at " .. first5 .. " no inherited realm carries the entry mark ("
              .. marks .. " marked; -1 means the round never drew)")
-- WHY THE FIRST-YEAR READ IS THE CHECK (the cold review asked): with the lagged slice clamped
-- to the record's first year the two slices coincide there, so zero is what the clamp
-- guarantees -- and it is exactly what the unclamped slice failed (every inherited realm marked
-- for the first twelfth). A read further in cannot serve: a realm that CLIMBS onto the board
-- a twelfth in is meant to carry the mark (a rank entrant), so a later count is not a count
-- of inherited-realm errors. Remove the clamp and this read goes non-zero again.

-- 4. The clipping ledger over those frames.
verify.expect_no_clipping("names_and_voice")
