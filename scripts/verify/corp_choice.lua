-- Starting-corp selection (BL-435) — the screen the player picks their
-- corporation on, captured for the first time.
--
-- WHY THIS SCRIPT NEEDS A HOOK AT ALL. The stage lives in exactly one frame of
-- the real start-up sequence: after generation (so the corps exist) and before
-- the pre-game warm start (so the corp the player picks is the corp corp_ai
-- freezes for all 80 ticks). Every automated path — --autostart, and the verify
-- harness's own start — takes the generator's seeded pick the instant that frame
-- appears, on purpose, because that is what keeps all existing captures and
-- goldens bit-identical across BL-435. So the screen was simply unreachable to
-- --verify, and a check written against an unreachable surface is green-but-blind.
-- verify.show_corp_choice re-enters the stage from the started world instead.
--
-- WHAT IT ASSERTS. Only what is true of every world: that a choice exists (more
-- than one selectable opening) and that the pool is the SPECIALIST set rather
-- than BL-365's background firms — the two properties requirement R1/R2 name.
-- It does NOT assert who is richer: BL-436 measured that a processing facility
-- currently earns LESS than the extraction site it replaces, so the corp with
-- more processors is the DEEPER opening, not the wealthier one, and no copy or
-- check here may claim otherwise.
--
-- Run: ProjectIo --verify scripts/verify/corp_choice.lua

verify.show_corp_choice(true)

local choices = verify.corp_choices()
local n = #choices
verify.expect(n > 1, "the stage offers a CHOICE, not a list of one; got " .. n .. " openings")

-- The pool is the specialist set. Background firms number 17-29 per seed against
-- 8 specialists, so a pool that had swept them in would be unmistakable by count
-- alone, and every row must carry the fields the screen prints.
verify.expect(n <= 12,
    "pool is the specialist set, not BL-365's background firms; got " .. n .. " openings")
for i = 1, n do
    local c = choices[i]
    verify.expect(c.name ~= nil and c.name ~= "", "opening " .. i .. " has a name")
    verify.expect(c.focus ~= nil and c.focus ~= "?", "opening " .. i .. " has a stated focus")
end

-- A warm-up frame first, exactly as main_menu.lua does: the stage's window is
-- AlwaysAutoResize, and ImGui sizes a window from the PREVIOUS frame's content,
-- so a cold frame-1 capture comes back empty (measured — the first cut of this
-- script blessed nothing but the clear colour).
verify.capture("corp_choice_warmup")
verify.capture("corp_choice")

verify.show_corp_choice(false)
