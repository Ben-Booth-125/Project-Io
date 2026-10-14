-- Recipe / workforce change acceptance (BL-113, US-007).
-- Proves an existing building's PRODUCTION follows its recipe and workforce
-- target: set its recipe via the SAME write the construction panel's recipe combo
-- performs, tick the sim, and assert the output responds. BL-1322 (Ben,
-- 2026-10-13): the player no longer sets workforce - the game moves the target
-- ONE 25 % step a quarter toward the most profitable level (PRODUCTION.md
-- § Workforce target and the auto-solver). The workforce half stages a target
-- with the verify fixture and asserts the game's steps: at most 25 a quarter,
-- always in 0-100, and production runs at the target the game set. This is the coverage the build/manage flow lacked:
-- no check exercised a recipe/workforce change end-to-end (arm -> commit -> tick ->
-- effect), so a regression in how workforce_target or the active recipe reaches the
-- economy could ship invisibly. Realises USER_STORIES.md US-007
-- ("'Recipe honoured next tick' is golden-verified; workforce cap is headless").
-- Run: ProjectIo --verify scripts/verify/recipe_workforce.lua

-- Pin a seed whose corporation generation gives the PLAYER a processing facility.
-- Only some worlds do (measured at 3 of 12 seeds by planetology_harness R11, which
-- guards that such seeds keep existing), and this check needs a real building to
-- steer rather than a skip. The default world stopped supplying one when BL-167
-- began rolling the homeworld's parameters from the seed.
--
-- 2026-09-25: verify.new_world REALLY rebuilds again (BL-1091's lane), and the
-- run showed what the old pass was: a processing facility's output is gated on
-- its INPUTS (PRODUCTION.md), the player's corp procures nothing on its own, and
-- the reference world's facility produced exactly once off the settle's leftover
-- stock before starving for eleven ticks; seed 0x3C6EF362's never had any. The
-- claim here is that the WORKFORCE lever reaches production, so the script now
-- stocks the active recipe's inputs itself (verify.stock_building_inputs, into
-- the pool the building draws from) before each tick it reads. Measured
-- 2026-09-25 on both worlds: 12 ticks at 100 % with no inputs read 0.0 every tick.
verify.new_world(0x3C6EF362)
verify.goto_surface("home")

-- Find the player's first processing facility — the building type that carries a
-- recipe choice. If the generated world seeds none, the flow has nothing to steer;
-- fail loudly rather than silently pass (the acceptance test must exercise a real
-- building, not skip).
local b = verify.first_processing_building()
verify.expect(b.found == true,
    "player has a processing facility to manage (US-007 needs a real building)")

if b.found then
    -- Stock the active recipe's inputs so the only thing gating output is the
    -- workforce target under test (see the header).
    local stocked = verify.stock_building_inputs(b.tile, 1000)
    verify.expect(stocked > 0,
        "the active recipe's inputs are stocked in the building's pool (" .. tostring(stocked) .. " inputs)")

    -- BL-1322: stage the target at 0 % with the verify FIXTURE (no player
    -- control writes it any more), then let the game step it. Each quarter the
    -- economy tick moves it ONE 25 % step toward the solver's best, before
    -- production, so the target read after a step is the one production ran at.
    local wf = verify.set_building_workforce(b.tile, 0)
    verify.expect(wf == 0, "fixture staged the workforce target at 0% (got " .. wf .. ")")

    local prev, series = 0, "0"
    for q = 1, 5 do
        verify.stock_building_inputs(b.tile, 1000)
        verify.econ_step(1)
        local now = verify.first_processing_building().workforce
        local out = verify.building_output(b.tile)
        series = series .. " -> " .. tostring(now)
        verify.expect(now >= 0 and now <= 200,
            "quarter " .. q .. ": the target stays in 0-200 internal (got " .. now .. ")")
        verify.expect(math.abs(now - prev) <= 50,
            "quarter " .. q .. ": the game moves the target at most one 50-internal step ("
            .. prev .. " -> " .. now .. ")")
        if now == 0 then
            verify.expect(out <= 0.0,
                "quarter " .. q .. ": at 0% workforce the building produces nothing (out=" .. out .. ")")
        else
            verify.expect(out > 0.0,
                "quarter " .. q .. ": at " .. now .. "% workforce the stocked building produces (out=" .. out .. ")")
        end
        prev = now
    end
    print("recipe_workforce: the game's targets by quarter from a staged 0%: " .. series)

    -- If the building type offers more than one recipe, prove the recipe selector
    -- commits: switching the active recipe writes a non-empty recipe name.
    if b.recipes and b.recipes > 1 then
        local name = verify.set_building_recipe(b.tile, 1)
        verify.expect(name ~= "",
            "recipe selector committed a recipe (name='" .. tostring(name) .. "')")
        verify.stock_building_inputs(b.tile, 1000) -- the NEW recipe's inputs
        verify.econ_step(1)
        local out_new = verify.building_output(b.tile)
        verify.expect(out_new >= 0.0,
            "economy ticks cleanly on the new recipe (out=" .. out_new .. ")")
    end
end

verify.capture("recipe_workforce")
