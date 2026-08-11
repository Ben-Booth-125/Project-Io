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
    -- id 0 — Smelter: iron ore + coal (reagent) -> steel. Coal joined the
    -- priced set at BL-340 (world_gen.lua's base_price), closing the gap
    -- between this recipe and PRODUCTION.md's Smelter table, which always
    -- named coal as a reagent — and giving coal a consumer, per the
    -- admission rule (BL-340) rather than leaving it an orphan raw.
    {
        name    = "steel",
        inputs  = { iron_ore = 2.0, coal = 1.0 },
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

    -- id 4 — Smelter, second recipe (BL-323 S1, PRODUCTION.md): metallic-asteroid
    -- feedstock needs no carbon addition (already reduced), so this is single-input
    -- like id 0's steel recipe but with no coal reagent.
    {
        name    = "steel_from_iron_nickel",
        inputs  = { iron_nickel_ore = 2.0 },
        outputs = { steel = 1.0 },
    },

    -- id 5 — Chemical Plant, ATMOSPHERE route (BL-308, Era 0). PRODUCTION.md
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

    -- id 6 — Chemical Plant, AIRLESS route (BL-308, Era 1). No atmosphere to
    -- separate, so the oxidiser comes from water electrolysis; the fuel half is
    -- shipped in or synthesised locally. This is the in-situ propellant loop
    -- PRODUCTION.md names as the defining Era 1 logistical problem: it costs
    -- water the body has, plus refined fuel it usually does not.
    --
    -- NOTE ON IDS: these were authored as 4 and 5 on a branch that predated the
    -- Smelter's id-4 steel recipe, and renumbered to 5 and 6 at merge. Recipe ids
    -- are POSITIONAL — `building_component::active_recipe_index` and
    -- `recipe_registry::recipe_at` both index this array — so appending is the
    -- only safe way to add one, and inserting would silently repoint every saved
    -- selection.
    {
        name    = "propellant_electrolysis",
        inputs  = { water = 3.0, refined_fuel = 1.0 },
        outputs = { propellant = 1.0 },
    },

    -- BL-340 (2026-08-11) — the processing-chain roster. Every value here
    -- passes the admission rule (PRODUCTION.md / this item): consumed by an
    -- authored recipe below, or terminal (spacecraft_components — the
    -- militia's procurement contract object, BL-350; no background demand).
    -- Building types stay recipes on the generic processing_facility, per
    -- the shipped five — no new building_type enum values.

    -- id 7 — Refinery: silica -> silicon.
    {
        name    = "silicon",
        inputs  = { silica = 2.0 },
        outputs = { silicon = 1.0 },
    },

    -- id 8 — Smelter: copper ore -> refined copper.
    {
        name    = "refined_copper",
        inputs  = { copper_ore = 2.0 },
        outputs = { refined_copper = 1.0 },
    },

    -- id 9 — Refinery: rare earth ore -> REE alloy.
    {
        name    = "ree_alloy",
        inputs  = { rare_earth_ore = 2.0 },
        outputs = { ree_alloy = 1.0 },
    },

    -- id 10 — Fabricator: steel + refined copper -> machinery. The
    -- Fabricator's alternative path to alloys (id 11) — a real choice
    -- between the two rather than a forced chain.
    {
        name    = "machinery",
        inputs  = { steel = 1.0, refined_copper = 1.0 },
        outputs = { machinery = 1.0 },
    },

    -- id 11 — Fabricator: steel + REE alloy -> alloys.
    {
        name    = "alloys",
        inputs  = { steel = 1.0, ree_alloy = 1.0 },
        outputs = { alloys = 1.0 },
    },

    -- id 12 — Electronics Lab: silicon + refined copper + REE alloy -> electronics.
    {
        name    = "electronics",
        inputs  = { silicon = 1.0, refined_copper = 1.0, ree_alloy = 0.5 },
        outputs = { electronics = 1.0 },
    },

    -- id 13 — Assembly Plant: alloys + electronics -> spacecraft components.
    -- Terminal good; BL-365 gives it deliberately NO background demand so
    -- the militia's BL-350 contracts are its only buyer.
    {
        name    = "spacecraft_components",
        inputs  = { alloys = 2.0, electronics = 1.0 },
        outputs = { spacecraft_components = 1.0 },
    },
}

print(string.format("[Lua] recipes.lua loaded  recipe_count=%d", #recipes))
