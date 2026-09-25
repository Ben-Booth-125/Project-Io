-- BL-1091 -- FROM LIFE TO PEOPLE: the Life round hands a named world and its
-- cradles to Culture (Ben, 2026-09-24, sprint-47 rulings R12).
--
--   ProjectIo --verify scripts/verify/life_to_people.lua
--
-- WHAT THIS CHECKS. Four things the item exists for, each photographed and
-- where the record allows it, asserted:
--   1. The Life round closes on the LEGACY fold -- water..legacy is one round
--      now (generation_charts.cpp's chain table) -- and the Drawdown lean sits
--      under it, on Life, not on Culture.
--   2. Next on Life leaves no blank pane: the Culture round holds the Life
--      round's globe through its wait, and the moment the record lands the
--      migration map REPLACES it -- a sharp jump, no dissolve (Ben, 2026-09-25:
--      "so the globe doesn't block a player's view"). Under --verify the run
--      is synchronous, so the wait cannot be photographed; the record's first
--      year is captured to show the map whole, with no globe over it.
--   3. The ticker's first lines name each cradle and the package it raised:
--      a `cradle` moment per people (lapse_event_kind::cradle, 23), read off
--      the settlement's pure-output records.
--   4. The lapse header names the body (the year stamp reads "<body>  <year>"),
--      and the History ledger's Chain view still shows Spend as its third
--      group.
--
-- IT CANNOT PROVE A PRESS IS REACHABLE (the Next press that starts the round,
-- the lean row that re-runs the Life-gate world): a capture proves a surface
-- renders; the live click is Ben's (the group's R5).
--
-- 1920x1080, the design-review resolution.

verify.window(1920, 1080)

local function shot(name)
    verify.frames(2)
    verify.capture(name)
end

-- The chain's stage indices (world/planetology.hpp chain_stage).
local STAGE_LEGACY = 8

-- ── 1. The Life round, on its closing fold ────────────────────────────────
-- First wizard frame after a screen switch is a warmup (main_menu.lua's rule).
verify.generation_stage(1)
verify.frames(4)
verify.capture("life_to_people_warmup")
shot("life_to_people_1_life_round")
-- The Legacy fold expanded in place: the round's payoff, and the fold the
-- Drawdown lean sits under.
verify.fold("generation_stage", STAGE_LEGACY)
verify.scroll_panel("wizard_charts", 1.0)
shot("life_to_people_2_legacy_fold")
verify.fold()

-- ── 2. The Culture map replaces the globe at once ────────────────────────
-- `history_run(0)` adopts the harness's own finished record for the Culture
-- round (the same fold generation makes), so the captures never race a run.
verify.history_run(0)
verify.frames(4)
verify.capture("life_to_people_warmup_culture")

local first, last = verify.history_span()
verify.expect(last > first, "the Culture record has a span")
local span = last - first

verify.history_year(first)
shot("life_to_people_3_map_at_start")
verify.history_year(first + math.floor(span * 0.20))
shot("life_to_people_4_map")

-- ── 3. The cradles are announced ─────────────────────────────────────────
local KIND_CRADLE = 23
local cradles = verify.history_event_count(KIND_CRADLE)
verify.expect(cradles > 0, "the record carries a cradle moment per people ("
                           .. cradles .. ")")
verify.expect(verify.history_event_count(KIND_CRADLE, first) == cradles,
              "every cradle is dated the span's first year")

verify.history_year(first)
local rows  = verify.history_ticker()
local named = 0
for _, line in ipairs(rows) do
    if string.find(line, "begin at", 1, true) then named = named + 1 end
end
verify.expect(#rows > 0, "the ticker has lines at the first year")
-- A cradle takes the COMMON tier (history_lapse.cpp ticker_priority, BL-1091),
-- the same tier as a `culture_split`, and lapse_ticker_rows walks a tier
-- newest-first -- so a split dated the same first year can displace a cradle
-- line. The honest bound counts those splits (every split at `first`, folded
-- or not: the ticker drops the folded ones, so this over-counts and the bound
-- stays a floor). On the reference world no split lands at 2400 BCE, so the
-- floor is min(#rows, cradles) and every row is a cradle.
local KIND_SPLIT = 8
local same_year_splits = verify.history_event_count(KIND_SPLIT, first)
verify.expect(named >= math.min(#rows - same_year_splits, cradles),
              "the ticker's first lines are the cradle announcements (" .. named
              .. " of " .. #rows .. ", " .. cradles .. " cradles, "
              .. same_year_splits .. " same-year splits)")
-- The package follows the seat as a colon-led list ("...: floodplain, coast
-- and 3 more."); the colon is the tell that the line carries one.
local with_package = 0
for _, line in ipairs(rows) do
    if string.find(line, "begin at", 1, true) and string.find(line, ": ", 1, true) then
        with_package = with_package + 1
    end
end
verify.expect(with_package == named, "every cradle line names the package it raised")

-- ── 4. The History ledger still shows Spend ──────────────────────────────
verify.show_generation(false)
verify.show_panel("tile", true)
verify.panel_view("history", 1)
verify.panel_view("history_round", 2)
shot("life_to_people_6_ledger_spend")
verify.panel_view("history", 0)
verify.show_panel("tile", false)

-- The clipping ledger over every frame above: the lean rows and the stamp
-- are the shapes that overrun.
verify.expect_no_clipping("life_to_people")
