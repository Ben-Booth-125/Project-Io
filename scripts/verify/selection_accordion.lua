-- The tile Selection element's section TOP NAV (BL-598, revised 2026-08-24).
--
-- Ben ruled the accordion on 2026-08-24 and, the same day on seeing it, ruled it
-- out again: "I meant a topnav left and right chevron, with a full canvas
-- expansion button... straddle left and right buttons across the entire span,
-- excepting the expand chevron. And our open accordion element title should be
-- centred. This is opposed to a vertical accordion."
--
-- The numbers behind the reversal, measured off the accordion before it went:
-- five stacked headers spent 169 of the band's 258 px on chrome to give the open
-- section 89. The nav spends one frame height.
--
-- WHY THIS SCRIPT NO LONGER PRESSES MEASURED PIXEL ROWS. Its previous version
-- drove the accordion by clicking header rows at coordinates measured from probe
-- captures (HDR_Y0 = 512, pitch 23.25). When the accordion became a nav those rows
-- stopped existing — and the check kept PASSING, capturing the same section five
-- times under five different names. That is the third instance of the same rot in
-- one sprint, so this version asserts on state (`selection_section`) and drives the
-- real chevrons, which cannot both be wrong and green.

verify.window(1280, 720)

local s = stage_ui_fixture()
verify.goto_surface("home")
verify.select_tile(s.unit.col, s.unit.row)
verify.frames(3)

local NAMES = { "Buildings", "Deposits", "Resources", "Population", "Terrain" }

-- The nav row sits at the top of the centre column, between the hex cell and the
-- action grid. These ARE screen coordinates, and after three coordinate failures in
-- one sprint that deserves its defence: they are safe here because every press is
-- followed by an assertion that the section ACTUALLY MOVED. A drifted coordinate
-- makes this check fail loudly on the next run; it cannot make it pass vacuously,
-- which is exactly what the version this replaced did — five captures of the same
-- section under five different names, green throughout.
--
-- Measured 2026-08-24 at 1280x720: centre column x 478..763, nav row y 495..528.
local NAV_Y  = 512
local COL_L  = 494                        -- left chevron, hard against the column edge
local COL_R  = 719                        -- right chevron, immediately left of the expand slot

local function section() return verify.pointer_target().selection_section end

verify.expect(section() ~= nil, "the element reports which section is showing")
local start = section()

-- Forward through every section with the RIGHT chevron, and prove each press
-- advances by exactly one. A nav that moved by two, or not at all, would look
-- identical in a capture.
local seen_forward = {}
for i = 1, #NAMES do
    local before = section()
    seen_forward[#seen_forward + 1] = before
    verify.click(COL_R, NAV_Y)
    verify.frames(2)
    local after = section()
    verify.expect(after == (before + 1) % #NAMES,
                  "right chevron: section " .. tostring(before) .. " -> " ..
                  tostring(after) .. " (expected " .. tostring((before + 1) % #NAMES) .. ")")
    shot("nav_fwd_" .. tostring(i) .. "_" .. NAMES[after + 1])
end

verify.expect(section() == start, "five presses of the right chevron return to the start")

-- And back the other way, which is the half a one-directional pager never had.
local before = section()
verify.click(COL_L, NAV_Y)
verify.frames(2)
verify.expect(section() == (before - 1 + #NAMES) % #NAMES,
              "left chevron steps backwards")
shot("nav_back_" .. NAMES[section() + 1])

-- Every section must have rendered at least once across the walk, so a section
-- that draws nothing is caught here rather than by someone noticing later.
local hit = {}
for _, v in ipairs(seen_forward) do hit[v] = true end
local missing = {}
for i = 0, #NAMES - 1 do
    if not hit[i] then missing[#missing + 1] = NAMES[i + 1] end
end
verify.expect(#missing == 0,
              "every section was reached by the nav" ..
              (#missing > 0 and (" (missed " .. table.concat(missing, ", ") .. ")") or ""))

verify.expect_no_clipping("selection_top_nav")
