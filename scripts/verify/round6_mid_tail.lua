-- BL-1084 (world built once and moved), the K4 cold review -- ROUND 6 WHILE ITS
-- TAIL BUILDS.
--
--   ProjectIo --verify scripts/verify/round6_mid_tail.lua
--
-- Round 6's record lands at its span's close and plays while the same worker
-- builds the tail and the finish behind it (STARTUP.md § Round 6). Two things
-- the cold review found broken in that window, checked on a REAL run
-- (verify.history_wait stages one from a held round-5 world, as the wizard's
-- own launch does):
--
--   (1) THE LAPSE KEEPS ITS PACE. STARTUP.md: one constant rate, set by the
--       pace control, for the whole span. The tail writes its own inner bars
--       (the nations' rows, the road walk, the search, the settle) into the
--       round's sink, and the playhead used to read them as the span's length
--       -- the lapse slowed, then raced to 1960 during "Laying roads". The
--       rate must be the record's own span over the pace setting while a tail
--       step reports a DIFFERENT inner total.
--   (2) REROLL WORKS MID-TAIL. The press used to bump the roll and do nothing
--       else, so Begin adopted the un-rerolled world. It must re-seed span 6,
--       clear the playing record at once, stop the superseded run, and land a
--       fresh record once that run has landed.
--
-- Heavy: two round-6 runs (the second abandoned after its record lands).
-- Nothing here is a golden; the captures are for inspection.

verify.window(1920, 1080)

local started = verify.history_wait(3)
verify.expect(started, "round 6's own stage is running on a worker, from a held copy of round 5's world")
verify.expect(verify.history_wait_record(),
              "round 6's record lands at its span's close, while its worker still builds the tail")

-- (1) THE PACE.
local tail_sub = verify.history_wait_tail_sub()
verify.expect(tail_sub > 0, "a tail step is reporting its own inner bar (" .. tail_sub .. ")")
local rate, years, secs = verify.history_rate()
local want = years / secs
verify.expect(years > 0 and math.abs(rate - want) < 1e-3,
              string.format("round 6 plays at the pace setting while the tail builds: %.3f years/s "
                            .. "= %d years over %.0f s (the tail's inner total %d would have made it %.3f)",
                            rate, years, secs, tail_sub, tail_sub / secs))
verify.expect(tail_sub ~= years, "the check is not vacuous: the tail's inner total differs from the span")
verify.frames(2)
verify.capture("round6_mid_tail_playing")

-- (2) THE REROLL, MID-TAIL.
local digest_before = verify.history_digest()
local seed_before   = verify.wizard_span_seed(3)
verify.wizard_reroll()
verify.expect(verify.wizard_span_seed(3) == seed_before + 1,
              "the press re-seeds span 6 alone (span_seed[3] " .. seed_before .. " -> "
              .. verify.wizard_span_seed(3) .. ", [2] " .. verify.wizard_span_seed(2) .. ")")
verify.expect(verify.history_powers() == 0, "the playing record is cleared at once")
verify.expect(verify.history_wait_record(),
              "the rerun's record lands once the superseded run has stopped and landed")
local digest_after = verify.history_digest()
verify.expect(digest_after ~= digest_before,
              string.format("the rerun is a different Industrialisation span (record %08X -> %08X)",
                            digest_before, digest_after))
verify.expect(verify.history_powers() > 0,
              "the rerun's record holds ground (" .. verify.history_powers() .. " powers)")
verify.frames(2)
verify.capture("round6_mid_tail_rerolled")

-- Done: the rerun's tail is not needed.
verify.expect(verify.history_wait_abandon(), "the staged run is stopped, landed and released")
