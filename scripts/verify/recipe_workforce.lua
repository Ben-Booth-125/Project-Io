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

verify.goto_surface("home")

-- Find the player's first processing facility — the building type that carries a
-- recipe choice. If the generated world seeds none, the flow has nothing to steer;
-- fail loudly rather than silently pass (the acceptance test must exercise a real
-- building, not skip).
local b = verify.first_processing_building()
verify.expect(b.found == true,
    "player has a processing facility to manage (US-007 needs a real building)")

if b.found then
    -- Set workforce to 0 %: the economy must honour the target and produce nothing.
    local wf = verify.set_building_workforce(b.tile, 0)
    verify.expect(wf == 0, "workforce target committed to 0% (got " .. wf .. ")")

    verify.econ_step(1)
    local out_idle = verify.building_output(b.tile)
    verify.expect(out_idle <= 0.0,
        "at 0% workforce the building produces nothing (out=" .. out_idle .. ")")

    -- Raise workforce to full: output must now be positive — the target change
    -- reached the economy through the real field the slider writes.
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
        verify.econ_step(1)
        verify.expect(verify.building_output(b.tile) >= 0.0,
            "economy ticks cleanly on the new recipe")
    end
end

verify.capture("recipe_workforce")
