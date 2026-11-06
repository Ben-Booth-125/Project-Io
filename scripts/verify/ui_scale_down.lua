-- BL-1495 (UI scale down): the shipped UI draw scale, side by side with a scale of 1.
--
-- The rest of the harness runs at 1, the tightest layout (LAYOUT.md § UI scale). This
-- check draws the shipped constant: the province card BL-1490 clipped at 1280 x 720, the
-- shell at Ben's 1720 x 1080, a real press through the scaled hit-test, and the same walk
-- as text_overflow_floor at the floor, whose ledger must come back clean.
--
-- Run:     ProjectIo --verify scripts/verify/ui_scale_down.lua
-- Writes:  screenshots/uiscale_*.png, screenshots/text_overflow.txt

verify.window(1280, 720)
verify.goto_surface("home")
verify.econ_step(4)

-- The surface opens with its home tile selected, so the band holds the land's province
-- card: the clip, before, in the same frame at 1.
verify.capture("uiscale_1000_province")

verify.ui_draw_scale() -- the shipped constant
verify.capture("uiscale_warm") -- the atlas rebuilds on this frame
verify.capture("uiscale_shipped_province")

-- A press lands through the scale: the gear, read back as drawn, in UI pixels.
local g = verify.escape_menu().gear
verify.click(g.x, g.y)
verify.capture("uiscale_shipped_menu")
if verify.escape_menu().open then
    print("ui_scale_down: PASS a press at the gear's UI-pixel centre opens the menu")
else
    error("ui_scale_down: FAIL the gear did not answer under the scale")
end
verify.click(g.x, g.y)
verify.capture("uiscale_frame")

verify.show_panel("market", true)
verify.capture("uiscale_shipped_market")
verify.show_panel("market", false)

-- Ben's display.
verify.window(1720, 1080)
verify.capture("uiscale_shipped_1720")
verify.window(1280, 720)

-- The floor's own walk, at the shipped scale: no record may clip.
dofile("scripts/verify/text_overflow_floor.lua")
print("ui_scale_down: OK")
