-- Every nav-rail slot must be ON SCREEN at every supported resolution.
--
-- WHY THIS EXISTS. Adding Convoys at slot 7 (BL-689) took the rail to fourteen,
-- and at the documented 1280x720 floor the fourteenth slot's centre landed at
-- y=746 against a 720 px screen. Contracts — a player system — was DRAWN and
-- unreachable: ImGui's hit-test rejects a press outside the window, so the slot
-- did nothing at all. At 1920x1080 the identical layout was fine, which is why no
-- capture could have caught it and why this check takes both resolutions.
--
-- `nav_slot` reports where each slot was ACTUALLY drawn, so this measures the
-- rail rather than re-deriving it from the constants the rail itself uses.

verify.goto_surface("home")
verify.frames(2)

-- The rail's own slot count. Kept here rather than read from the code so that
-- ADDING a slot without widening the rail's budget fails this check loudly.
local RAIL_SLOTS = 14

for _, res in ipairs({ {1280, 720}, {1920, 1080} }) do
    local w, h = res[1], res[2]
    verify.window(w, h)
    verify.frames(2)

    local drawn, offscreen, last_y = 0, 0, 0
    for slot = 1, RAIL_SLOTS do
        local x, y = verify.nav_slot(slot)
        if x == nil then
            verify.expect(false, string.format("%dx%d: slot %d was not drawn", w, h, slot))
        else
            drawn = drawn + 1
            last_y = y
            -- The CENTRE must clear the screen edge; a slot whose centre is off
            -- the bottom cannot be pressed even if a sliver of it is visible.
            if y >= h then offscreen = offscreen + 1 end
        end
    end

    verify.expect(drawn == RAIL_SLOTS,
        string.format("%dx%d: all %d rail slots drawn (%d)", w, h, RAIL_SLOTS, drawn))
    verify.expect(offscreen == 0,
        string.format("%dx%d: every slot centre is on screen (%d off; last centre y=%.1f, h=%d)",
                      w, h, offscreen, last_y, h))
end

verify.window(1280, 720)
verify.frames(2)
verify.capture("nav_rail_fit_720")
verify.window(1920, 1080)
verify.frames(2)
verify.capture("nav_rail_fit_1080")
