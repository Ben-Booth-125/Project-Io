-- BL-1092 R4 -- THE 400 BCE CLAMP on an OVERRUN seed (Ben, 2026-09-24,
-- sprint-47 rulings R11; STARTUP.md § Round 3: "a presentation clamp, not a
-- record clamp").
--
--   ProjectIo --verify scripts/verify/culture_overrun.lua
--
-- THE SEED IS MEASURED, NOT GUESSED. tools/verify/culture_round_coast_measure
-- (BL-947's sweep, re-run 2026-09-25 on this tree) reports 7 of 60 hashed
-- seeds whose migration is still filling when the Empires span opens (the
-- worst, seed 3665124156, ends at -2: 398 years over). Of the CURATED library
-- (docs/generation/seed_library.json) seed 9, "the frontier with no
-- neighbours", overruns too -- `migration_end_year` -257, 143 years past
-- 400 BCE, read off `history_census` the same day -- and a library world is
-- the one every other check is written about, so it is the seed here. That
-- world is built here (`verify.new_world`, a full Release build of ~1-2 min)
-- and its Culture record adopted from the finished report.
--
-- WHAT THIS CHECKS. The record's own span runs to its true end (the coast
-- rule keeps the later year: BL-947); the PLAYHEAD stops at the Empires
-- round's opening year -- parking past it clamps back to it -- and the
-- overrun caption is on the board. Round 4 opens at 400 BCE regardless
-- (its span is stated), so the calendar never steps back. The record is
-- untouched: `history_span` still reads the true end.

verify.window(1920, 1080)

local function shot(name)
    verify.frames(2)
    verify.capture(name)
end

local OVERRUN_SEED = 9
verify.new_world(OVERRUN_SEED)
verify.history_run(0)
verify.frames(4)
verify.capture("culture_overrun_warmup")

local first, last = verify.history_span()
local cradles, coined, folded, overrun = verify.history_census()
verify.expect(overrun > 0, "seed " .. OVERRUN_SEED .. "'s migration overruns 400 BCE by "
                           .. overrun .. " years (the record keeps its true end, " .. last .. ")")
verify.expect(last == -400 + overrun, "history_span reads the record's own end, unclamped")

-- Park PAST the clamp: the wizard holds the playhead at 400 BCE. The year
-- stamp and the scrubber in the capture read 400 BCE, and the caption under
-- the counters says how far the record ran on.
verify.history_year(last)
shot("culture_overrun_1_clamped_at_400bce")
verify.history_year(-400)
shot("culture_overrun_2_at_400bce")
-- And a frame well inside the span, so the round still plays.
verify.history_year(first + math.floor((-400 - first) / 2))
shot("culture_overrun_3_mid_span")

verify.expect_no_clipping("culture_overrun")
