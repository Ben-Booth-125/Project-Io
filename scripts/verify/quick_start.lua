-- Quick Start on the main menu; loading bars in a top band (Ben, 2026-10-08).
--
--   ProjectIo --verify scripts/verify/quick_start.lua
--
-- STARTUP.md § Main menu and § Rounds. Claims, each driven by a REAL PRESS:
--
--   Q1. Quick Start is a button on the menu, and a press on it opens the
--       wizard's SETUP round alone (BL-1279; Ben, 2026-10-13) -- the one place
--       the nation targets, the seed and the abundance are set.
--   Q1b. A press on the Total slider moves the nation total, and the pair
--       stays legal (the slider's range is cut by the majors).
--   Q1c. The setup round's Begin leaves for the loading bar
--       (app_screen::building) -- no other wizard round drawn.
--   Q2. The cold build behind that bar lands on the SEAT canvas, the same
--       screen the wizard's Begin reaches. It is Begin's own cold case
--       (launch_cold_build: make_hard_coded_world, then finish_campaign_world),
--       not a second generation path.
--   Q3. The params flow: the world built was built from the setup round's
--       targets, and lands on them (nations alive on the homeworld == the
--       total; major nations == the majors). nation_targets_harness owns the
--       sim's half across seeds; this is the wizard-to-world seam.
--
-- And one visual: the loading bar sits ~15% of the window's height from the
-- top, horizontally centred (ui::loading_bar_y). The building capture is for
-- inspection, never a golden: the bars stand wherever the worker has got to.
--
-- The press is a pointer click at the button's centre on the fixed 1920x1080
-- verify window. The menu is a centred auto-sized card, so the point is fixed
-- for this layout; if the menu grows a row, Q1 fails loudly (the screen stays
-- "menu") rather than the script testing nothing.
--
-- SLOW: the cold build runs the whole generation, the landscape search and the
-- settle (minutes in Debug). The loop polls by frames against a wall deadline.

verify.window(1920, 1080)
verify.as_interactive() -- a person at the keyboard (the seat canvas opens), init.lua loaded
verify.show_menu(true)
verify.frames(3)
verify.capture("quick_start_00_menu")

local QX, QY = 958, 628 --the Quick Start button's centre (see the menu capture)
verify.click(QX, QY)
verify.frames(4)
local s = verify.screen()
local round, rounds, quick = verify.wizard_round()
verify.expect(s == "generating" and round == 1 and quick,
              "Quick Start opens the setup round alone (screen " .. s .. ", round "
              .. round .. ", quick " .. tostring(quick) .. ")")
verify.capture("quick_start_00b_setup")

-- Q1b -- a press a third of the way along the Total slider (Quick Start's
-- setup round draws it at y~170, x 248..720 at 1920x1080).
local t0, m0 = verify.wizard_targets()
verify.click(400, 170)
verify.frames(3)
local total, majors = verify.wizard_targets()
verify.expect(total ~= t0 and total >= majors and total <= 40 and majors == m0,
              "a press on the Total slider moves the total and keeps the pair legal ("
              .. t0 .. " / " .. m0 .. " -> " .. total .. " / " .. majors .. ")")
verify.capture("quick_start_00c_setup_moved")

-- The setup round's Begin under Quick Start, read off the footer (it follows
-- the round's content, so no fixed coordinate holds; Ben, 2026-10-15).
local _, _, _, _, BEGIN_X, BEGIN_Y = verify.wizard_buttons()
verify.click(BEGIN_X, BEGIN_Y)
verify.frames(2)
s = verify.screen()
verify.expect(s == "building",
              "the setup round's Begin leaves for the loading bar, no other round (screen: " .. s .. ")")
verify.frames(30)
verify.capture("quick_start_01_building")

-- No wall clock in the verify Lua state (no `os`), so the cap is a frame
-- count; the loop ends the moment the build lands.
local polls = 0
while verify.screen() == "building" and polls < 200000 do
    verify.frames(10)
    polls = polls + 1
    if polls == 300 then
        verify.capture("quick_start_02_building_later")
    end
end
s = verify.screen()
verify.expect(s == "choosing_seat",
              "the cold build lands on the seat canvas, as the wizard's Begin does (screen: " .. s .. ")")
local open, candidates = verify.seat_state()
verify.expect(open and candidates > 0,
              "the seat canvas offers the specialists (" .. tostring(candidates) .. " candidates)")
verify.frames(3)
verify.capture("quick_start_03_seat")

-- Q3 -- the targets flowed from the setup round into the built world.
local n_total, n_majors, p_total, p_majors = verify.nation_census()
verify.expect(p_total == total and p_majors == majors,
              "the world was built from the setup round's targets (" .. p_total .. " / "
              .. p_majors .. " vs " .. total .. " / " .. majors .. ")")
verify.expect(n_total == total,
              "the homeworld holds the target total (" .. n_total .. " nations, target "
              .. total .. ")")
verify.expect(n_majors == majors,
              "the homeworld holds the target majors (" .. n_majors .. " majors, target "
              .. majors .. ")")
