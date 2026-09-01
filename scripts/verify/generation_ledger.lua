-- The Generation ledger: the resting index, then every section opened.
--
--   ProjectIo --verify scripts/verify/generation_ledger.lua
--
-- WHY THIS EXISTS. The ledger is six collapsing sections over a body selector,
-- and four of the six are closed at rest - so a capture of the resting frame
-- shows headers and almost no content. Before `verify.section` there was no way
-- to open one from a script, and `panel_view` could not help: sections are
-- ui_state BOOLS, not a view index. Every capture this surface had ever had
-- showed whatever the defaults happened to be.
--
-- That is the same gap that let a script ask for the ledger's old Tile view, get
-- silently nothing, and still report success (NR-714). `verify.section` prints a
-- named list on an unknown section rather than ignoring it, so the next such
-- request fails loudly.
--
-- 1920x1080, the design-review resolution (ledger_pass.lua carries the reasoning).

verify.window(1920, 1080)

local staged = stage_ui_fixture()
print("GENLEDGER state_hash=" .. verify.state_hash())

local PANELS = {
    "corporation", "balance", "construction", "tech_tree", "acquisitions",
    "market", "corporations_table", "tile", "generation_ledger", "decisions",
    "strategy", "build",
}

local SECTIONS = {
    "gen_profile", "gen_thresholds", "gen_bands",
    "gen_substrate", "gen_cover", "gen_landform",
}

local function shot(name)
    verify.frames(2)
    verify.capture(name)
end

local function open_only(panel)
    for _, p in ipairs(PANELS) do verify.show_panel(p, false) end
    verify.frames(1)
    verify.show_panel(panel, true)
end

-- 1. AT REST. The frame a player meets: Profile and Thresholds open because they
-- are what the rest is read against, the other four collapsed to one line each.
-- The whole surface should fit the column with room to spare - that is the point
-- of the reformat, and this is the frame that proves or disproves it.
open_only("generation_ledger")
for _, sec in ipairs(SECTIONS) do
    verify.section(sec, sec == "gen_profile" or sec == "gen_thresholds")
end
shot("genledger_00_at_rest")

-- 2. EVERY SECTION OPEN. The review frame: all six tables at once, which is also
-- the worst case for the column's height and so the frame most likely to clip.
for _, sec in ipairs(SECTIONS) do verify.section(sec, true) end
shot("genledger_01_all_open")

-- 3. THE DISTRIBUTIONS ALONE, scrolled to the foot. Substrate, Cover and Landform
-- are the tuning read and they sit below the fold once opened; the three share a
-- denominator (the whole grid, ocean included) which each header names.
verify.section("gen_profile", false)
verify.section("gen_thresholds", false)
verify.section("gen_bands", false)
verify.frames(2)
verify.scroll_panel("generation_ledger", 1.0)
verify.frames(3)
verify.capture("genledger_02_distributions_foot")
verify.scroll_panel("generation_ledger", 0.0)
verify.frames(2)

-- 4. A REAL PRESS ON A HEADER. `verify.section` writes the ui_state bool
-- directly, so every capture above proves the sections RENDER at a state - not
-- that a player can reach that state. This is the standing "a UI requirement
-- needs a live check" rule paid in the harness: `verify.click` injects a real
-- pointer press that ImGui reads exactly as a mouse does.
--
-- WHAT IS ACTUALLY AT RISK. The headers are driven by
-- SetNextItemOpen(state, ImGuiCond_Always) every frame, and a forced-open state
-- is precisely the shape that can swallow a user's click. It does not, because
-- CollapsingHeader returns the state AFTER the press and the draw writes that
-- back - but that is an argument, and this is the observation.
--
-- The coordinate is the "Latitude bands" header in the frame captured above, at
-- 1920x1080 with Profile and Thresholds open. It moves if either of those grows
-- a row; if this capture ever shows the section still shut, check the y first.
for _, sec in ipairs(SECTIONS) do
    verify.section(sec, sec == "gen_profile" or sec == "gen_thresholds")
end
verify.frames(2)
verify.click(200, 381)
shot("genledger_03_after_real_press")

-- 5. AN UNKNOWN SECTION, deliberately. Proves the hook complains rather than
-- silently doing nothing - the property this script's header is about. It prints
-- a named list; it does not fail the run.
verify.section("gen_no_such_section", true)

verify.expect_no_clipping("generation_ledger")
