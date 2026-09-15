-- The EMPIRES round's time-lapse map, its top-16 board and the arc readout
-- (BL-1000 on BL-829 / BL-830 / BL-891).
--
--   ProjectIo --verify scripts/verify/history_lapse_empires.lua
--
-- WHAT THIS CHECKS. history_lapse.lua photographs round 3, the Culture
-- migration, whose record carries no polity samples. The board's RANK column
-- is share of PEOPLE (Ben, 2026-09-15, NR-876): a polity's sampled population
-- over every living polity's at the recorded step at or before the playhead,
-- in history_sweep's own arithmetic. That column only has numbers on a record
-- with samples, so this script takes round 4 (`verify.history_run(1)`) and
-- captures it at its first year, mid-span and its close, where the arc
-- readout's "largest empire held N% of the world's people" is the figure
-- history_sweep prints as `peak_share_pop_q` for the same seed.
--
-- IT CANNOT PROVE THE ROUND IS REACHABLE BY PRESSES; round4_arc_reach.lua
-- walks there by real clicks, and the live watch is recorded in the delivery.
-- Playback is frozen under --verify, so `history_year` is the only thing that
-- moves the playhead and no capture races an animation.
--
-- 1920x1080, the design-review resolution.

verify.window(1920, 1080)

local function shot(name)
    verify.frames(2)
    verify.capture(name)
end

-- Enter round 4 and take the record. The first wizard frame after a screen
-- switch is a warmup — the surface build and the tile-to-region walk both land
-- on it — so it is captured and discarded, as history_lapse.lua does.
verify.history_run(1)
verify.frames(4)
verify.capture("history_lapse_empires_warmup")

-- The span is read off the record rather than asserted: the Empires round runs
-- 400 BCE -> 1200 CE (CIVILISATION.md § The span is 400 BCE to 1200 CE), and a
-- retuned era should move these marks rather than empty them.
local first, last = verify.history_span()
verify.expect(last > first, "round 4 carries a record with a span (" .. first .. " -> " .. last .. ")")

local marks = {
    { first,                        "history_lapse_empires_1_origin" },
    { first + (last - first) // 2,  "history_lapse_empires_2_middle" },
    { last,                         "history_lapse_empires_3_close" },
}

for _, m in ipairs(marks) do
    verify.history_year(m[1])
    shot(m[2])
end

verify.expect(verify.history_powers() > 0,
              "polities hold ground at the close, so the board has rows to rank ("
              .. verify.history_powers() .. " powers)")

-- THE ARC READOUT SITS BELOW THE CHART CHILD'S FOLD AT 1080p, so scroll it up
-- and capture it: this is the frame that shows "the largest empire held N% of
-- the world's people". The figure is also READ, not only photographed —
-- `history_arc` returns it in per-mille, and it is reported rather than pinned
-- to a constant because it is a fact about the seed, not about the surface.
-- Compare it by hand against history_sweep.json's `peak_share_pop_q` for the
-- same seed (0 under --verify); the two must agree, and a disagreement is a
-- defect in one of the two arithmetics, never something to re-bless.
verify.scroll_panel("wizard_charts", 1.0)
shot("history_lapse_empires_4_arc")
verify.scroll_panel("", 0)

local peak_pop_q, end_pop_q, peak_region_q = verify.history_arc()
verify.expect(peak_pop_q > 0,
              "the arc readout has a people-share peak to print (peak_share_pop_q="
              .. peak_pop_q .. " per-mille, end " .. end_pop_q
              .. "; region peak " .. peak_region_q .. ") - compare against "
              .. "history_sweep.json seed 0")

-- The clipping ledger over those frames: the board is a six-column table of
-- generated seat names in a third-width column, and the People/Land headers
-- are drawn by ImGui's own TableHeadersRow, which clips silently.
verify.expect_no_clipping("history_lapse_empires")
