-- Project Io — recipes.lua
-- Processing recipes for the Layer 3 economy. Loaded at startup into the C++
-- recipe registry (see src/world/recipe_registry.{hpp,cpp}).
--
-- Each recipe is { name, inputs, outputs } where inputs/outputs map a resource
-- name (matching the resource_type enum in components.hpp) to a per-batch
-- quantity. Recipes support multiple inputs and outputs and reagents (an input
-- that yields no separate product — e.g. coal in the full steel recipe).
--
-- The recipe's **id** is its 0-based index in this list. A building stores that
-- id in building_component.recipe; keep the order stable so authored ids hold.
-- Magnitudes are legible round defaults, iterated by playtest (PRODUCTION.md).

recipes = {
    -- id 0 — Smelter: iron ore -> steel.
    -- (The full design adds coal as a reagent; coal is outside the seven-resource
    --  prototype subset, so the L3 recipe is the single-input form.)
    {
        name    = "steel",
        inputs  = { iron_ore = 2.0 },
        outputs = { steel = 1.0 },
    },

    -- id 1 — Refinery: petroleum -> refined fuel.
    {
        name    = "refined_fuel",
        inputs  = { petroleum = 2.0 },
        outputs = { refined_fuel = 1.0 },
    },

    -- id 2 — Food Processor: agricultural produce -> food rations.
    {
        name    = "food_rations",
        inputs  = { agricultural_produce = 2.0 },
        outputs = { food_rations = 1.0 },
    },

    -- id 3 — Hydroponics Bay (BL-166): a processing_facility that produces
    -- agricultural_produce from refined inputs instead of a terrain deposit,
    -- feeding the same Food Processor -> Food rations chain the Farm feeds.
    -- No "energy" resource_type exists in the prototype set, so the recipe uses
    -- water (life-support/irrigation analog) plus steel (the structural good the
    -- bay itself is built from) as its two inputs.
    {
        name    = "hydroponics_bay",
        inputs  = { water = 1.5, steel = 0.5 },
        outputs = { agricultural_produce = 1.0 },
    },

    -- id 4 — Chemical Plant, ATMOSPHERE route (BL-308, Era 0). PRODUCTION.md
    -- § Chemical Plant: on a body with an atmosphere the plant separates its own
    -- liquid oxygen cryogenically from the local air, so the oxidiser costs no
    -- stockpiled input (energy only, abstracted into the rate). Refined fuel is
    -- therefore the whole authored input. Liquid oxygen is folded into the
    -- recipe rather than given a resource_type — nothing else would ever hold it.
    {
        name    = "propellant_atmospheric",
        inputs  = { refined_fuel = 2.0 },
        outputs = { propellant = 1.0 },
    },

    -- id 5 — Chemical Plant, AIRLESS route (BL-308, Era 1). No atmosphere to
    -- separate, so the oxidiser comes from water electrolysis; the fuel half is
    -- shipped in or synthesised locally. This is the in-situ propellant loop
    -- PRODUCTION.md names as the defining Era 1 logistical problem: it costs
    -- water the body has, plus refined fuel it usually does not.
    {
        name    = "propellant_electrolysis",
        inputs  = { water = 3.0, refined_fuel = 1.0 },
        outputs = { propellant = 1.0 },
    },
}

print(string.format("[Lua] recipes.lua loaded  recipe_count=%d", #recipes))
