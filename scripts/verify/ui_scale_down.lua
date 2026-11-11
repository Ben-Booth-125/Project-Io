-- BL-1495 (UI scale down): the default 12 px UI, side by side with the 16 px step.
--
-- The harness runs at the default step (LAYOUT.md § UI scale): a 12 px font, every chrome
-- element at 0.75x of its authored size, the canvas ground unscaled. This check captures
-- the province card BL-1490 clipped at 1280 x 720 at both steps, presses the gear at the
-- default to prove the hit-test moves with the chrome, captures the shell at Ben's
-- 1720 x 1080, and walks text_overflow_floor at the default, whose ledger must come back
-- clean.
--
-- Run:     ProjectIo --verify scripts/verify/ui_scale_down.lua
-- Writes:  screenshots/uiscale_*.png, screenshots/text_overflow.txt

verify.window(1280, 720)
verify.goto_surface("home")
verify.econ_step(4)

-- The surface opens with its home tile selected, so the band holds the land's province
-- card. First at the 16 px step, the size the chrome was authored at.
verify.ui_scale_step(1)
verify.capture("uiscale_warm16") -- the atlas rebuilds on this frame
verify.capture("uiscale_16px_province")

verify.ui_scale_step() -- the default
verify.capture("uiscale_warm") -- the atlas rebuilds on this frame
verify.capture("uiscale_default_province")

-- A press lands at the scaled chrome: the gear, read back as drawn.
local g = verify.escape_menu().gear
verify.click(g.x, g.y)
verify.capture("uiscale_default_menu")
if verify.escape_menu().open then
    print("ui_scale_down: PASS a press at the gear's drawn centre opens the menu")
else
    error("ui_scale_down: FAIL the gear did not answer at the default scale")
end
verify.click(g.x, g.y)
verify.capture("uiscale_frame")

verify.show_panel("market", true)
verify.capture("uiscale_default_market")
verify.show_panel("market", false)

-- Ben's display.
verify.window(1720, 1080)
verify.capture("uiscale_default_1720")
verify.window(1280, 720)

-- The floor's own walk, at the default: no record may clip.
dofile("scripts/verify/text_overflow_floor.lua")
print("ui_scale_down: OK")
