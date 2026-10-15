-- Every wizard round fits without a scroll bar (Ben, 2026-10-15, at the live
-- click; STARTUP.md § Rounds: "Every round fits without a scroll bar").
--
--   ProjectIo --verify scripts/verify/wizard_fit.lua
--
-- Captures all seven rounds at the design-review resolution (1920x1080) and at
-- the 1280x720 floor. The lapse rounds are captured on their landed record (the
-- board, its ticker and the footer). THE PNGs ARE THE PRODUCT: look at each one
-- for a scroll bar in the left column and for Reroll / Back / Next on screen.

local function walk(w, h)
    verify.window(w, h)
    local tag = "wizard_fit_" .. w
    for r = 0, 2 do
        verify.generation_stage(r)
        verify.frames(6)
        verify.capture(tag .. "_round" .. (r + 1))
    end
    for k = 0, 3 do
        verify.history_wait(k)
        verify.history_wait_land()
        verify.frames(6)
        verify.capture(tag .. "_round" .. (k + 4))
        -- The fit settles rather than oscillating: one frame on, the same rows.
        verify.frames(1)
        verify.capture(tag .. "_round" .. (k + 4) .. "_next")
    end
end

walk(1920, 1080)
walk(1280, 720)
