-- Recipe / workforce change acceptance (BL-113, US-007).
-- Proves a player can steer an existing building's PRODUCTION through the real
-- commit path: set its recipe and workforce target via the SAME writes the
-- construction panel's recipe combo + workforce slider perform, tick the sim, and
-- assert the output responds. This is the coverage the build/manage flow lacked:
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

    -- Set workforce to 0 %: the economy must honour the target and produce nothing,
    -- inputs or no inputs.
    local wf = verify.set_building_workforce(b.tile, 0)
    verify.expect(wf == 0, "workforce target committed to 0% (got " .. wf .. ")")

    verify.econ_step(1)
    local out_idle = verify.building_output(b.tile)
    verify.expect(out_idle <= 0.0,
        "at 0% workforce the building produces nothing (out=" .. out_idle .. ")")

    -- Raise workforce to full: output must now be positive — the target change
    -- reached the economy through the real field the slider writes. The inputs
    -- are topped up again: the 0 % tick may not consume, but nothing here should
    -- depend on that.
    verify.stock_building_inputs(b.tile, 1000)
    local wf2 = verify.set_building_workforce(b.tile, 100)
    verify.expect(wf2 == 100, "workforce target committed to 100% (got " .. wf2 .. ")")

    verify.econ_step(1)
    local out_full = verify.building_output(b.tile)
    verify.expect(out_full > 0.0,
        "at 100% workforce the building produces output (out=" .. out_full .. ")")

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
