-- Quick Start on the main menu; loading bars in a top band (Ben, 2026-10-08).
--
--   ProjectIo --verify scripts/verify/quick_start.lua
--
-- STARTUP.md § Main menu. Two claims, both driven by a REAL PRESS:
--
--   Q1. Quick Start is a button on the menu, and a press on it leaves the menu
--       for the loading bar (app_screen::building) -- no wizard round drawn.
--   Q2. The cold build behind that bar lands on the SEAT canvas, the same
--       screen the wizard's Begin reaches. It is Begin's own cold case
--       (launch_cold_build: make_hard_coded_world, then finish_campaign_world),
--       not a second generation path.
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

local QX, QY = 958, 682 --the Quick Start button's centre (see the menu capture)
verify.click(QX, QY)
verify.frames(2)
local s = verify.screen()
verify.expect(s == "building",
              "Quick Start leaves the menu for the loading bar, no wizard round (screen: " .. s .. ")")
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
