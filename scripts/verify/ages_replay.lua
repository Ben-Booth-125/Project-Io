-- The Ages view: the Era -1 political time-lapse, captured across its own span.
--
--   ProjectIo --verify scripts/verify/ages_replay.lua
--
-- WHY THIS IS A SCRIPT OF ITS OWN rather than four more lines in ledger_pass.
-- Ages is the one ledger view whose subject is TIME, so a single frame of it
-- says almost nothing — the review question is "does a frontier move", and that
-- is a comparison across years, not a picture. ledger_pass captures one frame
-- per sub-view by construction; this walks the transport.
--
-- WHAT IT REPLACES. Until 2026-08-30 this view was uncapturable, and
-- ledger_pass carried a footer explaining why: the first draw of Ages produced
-- no frame in NINETEEN MINUTES (NR-710). The cause was not slowness in the sim.
-- tile_inspector.cpp constructed its own `history_sim_params` — 0 -> 1960 CE
-- with the tick bands left at their struct default — and the default ladder's
-- last band ends at year 0, so every year past it fell back to a ONE-YEAR step.
-- That is 1960 decision rounds against the 100 generation runs, on a span lying
-- entirely AFTER the era the world actually has. The view now calls
-- `era_minus_one_sim_params` / `era_minus_one_sim_seed` (world/era_minus_one.hpp),
-- so it replays GENERATION'S era: 400 BCE -> 0 CE on one four-year band.
--
-- THE PARK ORDER IS LOAD-BEARING, and it is why every capture below runs frames
-- BEFORE setting the year. `tile_inspector.cpp` parks `s.ages_year` at the run's
-- first year on the frame it (re)builds the cache, so a year set before that
-- frame is overwritten and the capture shows the start whatever was asked for
-- (NR-710's second half). Opening the view, letting the cache build, and only
-- then scrubbing is the ordering that survives it.
--
-- 1920x1080, the design-review resolution (ledger_pass § the resolution line).

verify.window(1920, 1080)

local staged = stage_ui_fixture()
print("AGES state_hash=" .. verify.state_hash())

local function shot(name)
    verify.frames(2)
    verify.capture(name)
end

-- Open History on Ages and let the cache build. The first frame here is the one
-- that pays for the sim and the one that parks the year; everything after it is
-- a cheap redraw off the cached owner-change list.
verify.show_panel("tile", true)
verify.panel_view("history", 2)
verify.frames(4)

-- The resting frame: whatever the view opens on, with no scrub applied. This is
-- the frame a player meets, so it is the one that has to read correctly on its
-- own -- the era's FIRST year, not its last.
shot("ages_00_at_rest")

-- The transport, walked. Generation's era is 400 BCE -> 0 CE, so these are the
-- start, three interior years and the epoch. Two things are under review across
-- them: that the region count and the power count MOVE (a static map means the
-- replay is showing one slice five times), and that the year reads as a real
-- calendar year in both eras -- "400 BCE", never "-400 CE".
for _, year in ipairs({ -400, -300, -200, -100, 0 }) do
    verify.panel_view("ages_year", year)
    shot(string.format("ages_%s", (year < 0) and (tostring(-year) .. "bce")
                                              or (tostring(year) .. "ce")))
end

-- The clipping ledger over the frames above. It is not expected to find
-- anything on this surface -- the transport is three controls and a one-line
-- readout -- but the assertion is free and the multipolarity line is built with
-- snprintf into a fixed buffer, which is the shape that overruns.
verify.expect_no_clipping("ages_replay")
