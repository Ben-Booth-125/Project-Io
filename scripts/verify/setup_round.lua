-- The wizard's SETUP round (BL-1279; STARTUP.md § Rounds, Ben 2026-10-10/11/13).
--
--   ProjectIo --verify scripts/verify/setup_round.lua
--
-- The setup round sits between System and Life and holds the world-wide inputs:
-- the two nation targets (total, majors) and the abundance radio, which moved
-- here from the main menu. The SEED IS HIDDEN (Ben, 2026-10-15): no entry, Roll
-- or Copy anywhere in the wizard -- setup_02_setup_round shows none; the Life
-- round's Reroll is how a player gets another world, and --verify sets seeds
-- through the verify API. The wizard is
-- SEVEN rounds. Claims, each driven by a REAL PRESS where a press exists:
--
--   S1. The menu carries no seed and no abundance any more (capture).
--   S2. New Game opens round 1 (System); Next lands on round 2, Setup, and the
--       counter reads "Round 2 of 7" (capture).
--   S3. The wizard always sets both targets: on arrival they are the defaults
--       (20 nations, 4 majors), never unset.
--   S4. An illegal pair is refused whole (total > 40, majors > 6, majors >
--       total, zero) and moves nothing; a legal one lands.
--   S5. Next from Setup lands on Life (round 3); Back from Life lands on Setup;
--       Back from Setup lands on System.
--   S7. Reroll, Back and Next are anchored at the bottom of the window (Ben,
--       2026-10-16): Next sits at the same height on System, Setup and Life,
--       within 80 px of the panel floor (1080 - 48 at this window).
--   S6. Quick Start opens the setup round ALONE: same round, flagged quick, the
--       counter replaced by "Quick Start", Back returns to the menu (capture).
--
-- Quick Start's build straight through to the seat, and the census of the
-- world it lands on, are quick_start.lua's (slow: the cold build).
--
-- The menu coordinates are read off the 1920x1080 captures.

verify.window(1920, 1080)
verify.show_menu(true)
verify.frames(3)
verify.capture("setup_00_menu")

local NEWGAME_X, NEWGAME_Y = 958, 574   -- the menu's New Game (see setup_00_menu)
local QUICK_X,   QUICK_Y   = 958, 628   -- the menu's Quick Start
-- The footer is anchored at the window's floor (Ben, 2026-10-16); Back and
-- Next are read off verify.wizard_buttons at each press.
local function press(which)
    verify.frames(1)
    local _, _, bx, by, nx, ny = verify.wizard_buttons()
    if which == "back" then verify.click(bx, by) else verify.click(nx, ny) end
end

-- S7 -- the footer's height, read off the button the press will hit.
local footer_y = {}
local function note_footer(name)
    verify.frames(1)
    local _, _, _, _, _, ny = verify.wizard_buttons()
    footer_y[#footer_y + 1] = { name = name, y = ny }
end

-- S2 -- New Game, then Next onto the setup round.
verify.click(NEWGAME_X, NEWGAME_Y)
verify.frames(4)
verify.expect(verify.screen() == "generating", "New Game opens the wizard (screen: "
              .. verify.screen() .. ")")
local round, rounds, quick = verify.wizard_round()
verify.expect(rounds == 7, "the wizard walks SEVEN rounds (got " .. rounds .. ")")
verify.expect(round == 0 and not quick, "New Game opens on round 1, System (0-based "
              .. round .. ")")
verify.capture("setup_01_system")
note_footer("System")

press("next")
verify.frames(4)
round = select(1, verify.wizard_round())
verify.expect(round == 1, "Next on System lands on round 2, SETUP (0-based " .. round .. ")")
verify.capture("setup_02_setup_round")
note_footer("Setup")

-- S3 -- the defaults.
local total, majors = verify.wizard_targets()
verify.expect(total == 20 and majors == 4,
              "the wizard always sets both targets: defaults 20 / 4 (got "
              .. total .. " / " .. majors .. ")")

-- S4 -- the pair is constrained, never clamped.
for _, bad in ipairs({ { 41, 4 }, { 20, 7 }, { 3, 4 }, { 0, 4 }, { 20, 0 } }) do
    local ok = verify.wizard_set_targets(bad[1], bad[2])
    local t, m = verify.wizard_targets()
    verify.expect(not ok and t == 20 and m == 4,
                  "an illegal pair " .. bad[1] .. " / " .. bad[2]
                  .. " is refused whole and moves nothing (now " .. t .. " / " .. m .. ")")
end
verify.expect(verify.wizard_set_targets(6, 6), "a legal pair 6 / 6 (total == majors) lands")
verify.frames(3)
verify.capture("setup_03_targets_6_6")
verify.expect(verify.wizard_set_targets(40, 1), "a legal pair 40 / 1 lands")
verify.frames(3)
verify.capture("setup_04_targets_40_1")
verify.wizard_set_targets(20, 4)
verify.frames(3)

-- S5 -- causal order, walked both ways.
press("next")
verify.frames(4)
round = select(1, verify.wizard_round())
verify.expect(round == 2, "Next on Setup lands on round 3, LIFE (0-based " .. round .. ")")
verify.capture("setup_05_life")
note_footer("Life")
local FLOOR = 1080 - 48 -- the panel is disp.y - 96 tall, centred
for _, f in ipairs(footer_y) do
    verify.expect(f.y > FLOOR - 80 and f.y < FLOOR,
                  "S7: Next is anchored at the bottom on " .. f.name .. " (y " .. f.y
                  .. ", floor " .. FLOOR .. ")")
    verify.expect(math.abs(f.y - footer_y[1].y) < 1.0,
                  "S7: Next sits at the same height on " .. f.name .. " as on System ("
                  .. f.y .. " vs " .. footer_y[1].y .. ")")
end
press("back")
verify.frames(4)
round = select(1, verify.wizard_round())
verify.expect(round == 1, "Back on Life lands on Setup (0-based " .. round .. ")")
press("back")
verify.frames(4)
round = select(1, verify.wizard_round())
verify.expect(round == 0, "Back on Setup lands on System (0-based " .. round .. ")")

-- S6 -- Quick Start opens the setup round alone.
verify.show_menu(true)
verify.frames(3)
verify.click(QUICK_X, QUICK_Y)
verify.frames(4)
round, rounds, quick = verify.wizard_round()
verify.expect(verify.screen() == "generating" and round == 1 and quick,
              "Quick Start opens the setup round alone (screen " .. verify.screen()
              .. ", round " .. round .. ", quick " .. tostring(quick) .. ")")
verify.capture("setup_06_quick_start")
press("back")
verify.frames(3)
round, rounds, quick = verify.wizard_round()
verify.expect(verify.screen() == "menu" and not quick,
              "Back on Quick Start's setup round returns to the menu (screen "
              .. verify.screen() .. ")")
