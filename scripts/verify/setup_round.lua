-- The wizard's SETUP round (BL-1279; STARTUP.md § Rounds, Ben 2026-10-10/11/13).
--
--   ProjectIo --verify scripts/verify/setup_round.lua
--
-- The setup round holds the world-wide inputs: the two nation targets (total,
-- majors) and the abundance radio, which moved here from the main menu. The SEED
-- IS HIDDEN (Ben, 2026-10-15): no entry, Roll or Copy. QUICK START IS THE ONLY WAY
-- IN, labelled New Game (Ben, 2026-11-18): no press opens the wizard, so the round-by-round
-- walk (System, Life and back) is no longer reachable from the menu and its claims
-- are withdrawn. Claims, each driven by a REAL PRESS where a press exists:
--
--   S1. The menu carries no seed, no abundance and no wizard press (capture).
--   S6. Quick Start opens the setup round ALONE: same round, flagged quick, the
--       counter replaced by "New Game" (capture).
--   S3. The wizard always sets both targets: on arrival they are the defaults
--       (30 nations, 4 majors; Ben, 2026-11-02), never unset.
--   S4. An illegal pair is refused whole (total > 40, majors > 6, majors >
--       total, zero) and moves nothing; a legal one lands.
--   S8. Back on Quick Start's setup round returns to the menu.
--
-- Quick Start's build straight through to the seat, and the census of the
-- world it lands on, are quick_start.lua's (slow: the cold build).

verify.window(1920, 1080)
verify.show_menu(true)
verify.frames(3)
verify.capture("setup_00_menu")

local function press(which)
    verify.frames(1)
    local _, _, bx, by, nx, ny = verify.wizard_buttons()
    if which == "back" then verify.click(bx, by) else verify.click(nx, ny) end
end

-- S6 -- Quick Start opens the setup round alone.
local qp = verify.escape_menu().main_quick_start
verify.expect(qp.x >= 0, "S1: the menu draws New Game (the Quick Start press)")
verify.click(qp.x, qp.y)
verify.frames(4)
local round, rounds, quick = verify.wizard_round()
verify.expect(verify.screen() == "generating" and round == 1 and quick,
              "Quick Start opens the setup round alone (screen " .. verify.screen()
              .. ", round " .. round .. ", quick " .. tostring(quick) .. ")")
verify.capture("setup_06_quick_start")

-- S3 -- the defaults.
local total, majors = verify.wizard_targets()
verify.expect(total == 30 and majors == 4,
              "the wizard always sets both targets: defaults 30 / 4 (got "
              .. total .. " / " .. majors .. ")")

-- S4 -- the pair is constrained, never clamped.
for _, bad in ipairs({ { 41, 4 }, { 20, 7 }, { 3, 4 }, { 0, 4 }, { 20, 0 } }) do
    local ok = verify.wizard_set_targets(bad[1], bad[2])
    local t, m = verify.wizard_targets()
    verify.expect(not ok and t == 30 and m == 4,
                  "an illegal pair " .. bad[1] .. " / " .. bad[2]
                  .. " is refused whole and moves nothing (now " .. t .. " / " .. m .. ")")
end
verify.expect(verify.wizard_set_targets(6, 6), "a legal pair 6 / 6 (total == majors) lands")
verify.frames(3)
verify.capture("setup_03_targets_6_6")
verify.expect(verify.wizard_set_targets(40, 1), "a legal pair 40 / 1 lands")
verify.frames(3)
verify.capture("setup_04_targets_40_1")
verify.wizard_set_targets(30, 4)
verify.frames(3)

-- S8 -- Back returns to the menu.
press("back")
verify.frames(3)
round, rounds, quick = verify.wizard_round()
verify.expect(verify.screen() == "menu" and not quick,
              "Back on Quick Start's setup round returns to the menu (screen "
              .. verify.screen() .. ")")
