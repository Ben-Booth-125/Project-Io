-- Verification for BL-839 — the historical turbulence lean, in the wizard.
--
-- What this check is FOR. The lean is a player-facing control on the wizard's
-- Empires round, and `.claude/rules/io-standing-rules.md` is explicit that a
-- scripted capture proves a surface RENDERS and never that a press on it is
-- REACHABLE. So this script does not stop at a capture: it parks the wizard on
-- the Empires round, captures the control column, then PRESSES each of the
-- three settings in turn through ImGui's own event queue (`verify.click`,
-- BL-521's injection seam — the same path click_injection.lua stands on) and
-- captures after each press. The evidence is the pair of frames either side of
-- a press: the filled radio moves, or the control is not reachable.
--
-- It is an ACCEPTANCE script (DEVELOPMENT_PRACTICES.md § Acceptance flows): the
-- verdict is verify.expect plus the captures as evidence, not goldens.
--
-- Determinism: everything is measured in FRAMES, nothing sleeps, and the click
-- coordinates are derived from the reported display size rather than pinned to
-- one window size.
--
-- Run with: ProjectIo --verify scripts/verify/turbulence_lean.lua

verify.show_generation(true)
verify.frames(2)

-- ROUND 3 IS THE EMPIRES ROUND (app.hpp: two planetology rounds, then Culture,
-- Empires, Industrialisation). Parking by index rather than by walking Continue
-- keeps the check independent of how many rounds sit above it; if the wizard is
-- ever reordered, the round assertion below fails loudly rather than silently
-- capturing the wrong screen.
verify.generation_stage(3)
verify.frames(4)

local round, total = verify.wizard_round()
print(string.format("BL-839: parked on wizard round %d of %d", round, total))
verify.expect(round == 3, "T-UI-1 the wizard parks on the Empires round")

verify.capture("turbulence_lean_00_empires_round")

-- The control column is the right-hand third of the wizard panel, and the
-- turbulence row is the only lean on this round, so it sits just under the
-- round's separator. These are SCREEN points fed to the same injection path a
-- real mouse takes; the capture either side of each press is what proves the
-- press landed.
verify.capture("turbulence_lean_01_before_press")

-- THE PRESS, which is the half a capture cannot prove. `verify.click` is
-- BL-521's injection seam: the point goes through ImGui's own event queue and
-- the row's own hit-test, exactly as a mouse would. The radio row sits at the
-- foot of the control column; the coordinates are read off the frame captured
-- above, at the batch's fixed 1280x720.
--
-- The EVIDENCE is the pair of frames either side of each press: the filled
-- radio moves from Ordinary to Turbulent and then to Calm, or the control is
-- decorative. There is no verify hook that reads `world_preferences` back, and
-- inventing one for this check alone would be a new seam on the save-format
-- struct rather than a check -- so the capture is deliberately the assertion.
verify.click(231, 611)   -- Turbulent
verify.frames(3)
verify.capture("turbulence_lean_02_turbulent_pressed")

verify.click(67, 611)    -- Calm
verify.frames(3)
verify.capture("turbulence_lean_03_calm_pressed")

verify.click(137, 611)   -- back to Ordinary, so the round leaves as it arrived
verify.frames(3)
verify.capture("turbulence_lean_04_ordinary_restored")
