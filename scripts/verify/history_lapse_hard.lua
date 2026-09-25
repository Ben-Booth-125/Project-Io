-- THE HARD BORDER on the Empires round, and its carry into Exploration
-- (BL-1090; Ben, 2026-09-24, sprint 47 rulings R9).
--
--   ProjectIo --verify scripts/verify/history_lapse_hard.lua
--
-- WHAT THIS CHECKS. A realm whose share of the world's people stands above
-- `lapse_hard_on_q` draws a hard border -- 2 px of dark and a 1 px inner
-- stroke in its own colour -- and its board row is bold; a realm under it
-- keeps the 1 px line (STARTUP.md § Identity across the rounds). The flag has
-- hysteresis across recorded steps and carries by id into round 5, so the
-- realms hard at 1200 on round 4 open round 5 hard.
--
-- Captures round 4 at 1000 CE (the item's done-when year) and at its close,
-- then round 5 at its first year, where the carried flag is the only thing
-- that can bold a row before the first recorded step. The verify world is
-- seed 0, one of the sixteen curated seeds, so history_sweep's BL-1090 block
-- says how many realms each capture should bold: compare its `bold@1000` /
-- `bold@close` columns for seed 0 against the bold rows on the board.
--
-- A capture proves the border renders; it cannot prove the flag holds across
-- a single-step dip or that a press reaches the round -- those are the live
-- click the requirement group's R3 owes. Playback is frozen under --verify,
-- so `history_year` is the only thing that moves the playhead.
--
-- 1920x1080, the design-review resolution.

verify.window(1920, 1080)

local function shot(name)
    verify.frames(2)
    verify.capture(name)
end

-- Round 4. The first wizard frame after a screen switch is a warmup (the
-- surface build and the tile-to-region walk land on it), captured and
-- discarded as history_lapse_empires.lua does.
verify.history_run(1)
verify.frames(4)
verify.capture("history_lapse_hard_warmup4")

local first, last = verify.history_span()
verify.expect(last > first, "round 4 carries a record with a span (" .. first .. " -> " .. last .. ")")
verify.expect(first < 1000 and 1000 <= last, "1000 CE lies inside the Empires span")

verify.history_year(1000)
shot("history_lapse_hard_1_1000ce")
verify.expect(verify.history_powers() > 0,
              "polities hold ground at 1000 CE (" .. verify.history_powers() .. " powers)")

verify.history_year(last)
shot("history_lapse_hard_2_close")

-- Round 5: the carry. Adopted from the same report; the launch inherits the
-- closing flags of round 4 by polity id before the first frame draws.
verify.history_run(2)
verify.frames(4)
verify.capture("history_lapse_hard_warmup5")

local f5, l5 = verify.history_span()
verify.expect(l5 > f5, "round 5 carries a record with a span (" .. f5 .. " -> " .. l5 .. ")")
verify.expect(f5 == last, "round 5 opens where round 4 closed (" .. f5 .. " vs " .. last .. ")")

verify.history_year(f5)
shot("history_lapse_hard_3_r5_open")

-- The clipping ledger: the board's bold rows are the same fitted text drawn
-- twice, so nothing new can clip, and this proves it rather than assumes it.
verify.expect_no_clipping("history_lapse_hard")
