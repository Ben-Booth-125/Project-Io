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
--
-- BL-433: an optional `era` field ("any" | "ancient" | "industrial") says which
-- product a recipe belongs to. Absent means "any" — shared by both arcs. The
-- registry MASKS on it rather than removing, precisely so the ids above stay
-- stable across bands; see recipe_registry.hpp § BL-433 era band. An unknown
-- string is a load-time error, not a silent fallback.
--
-- The ancient roster is deliberately thin right now — steel, food_rations and
-- refined_copper are the only untagged processing chains — because tagging is
-- all this item does. Filling it out is BL-429 (ancient building roster), which
-- is the next item in Sprint 17.
-- Magnitudes are legible round defaults, iterated by playtest (PRODUCTION.md).

recipes = {
    -- id 0 — Smelter: iron ore + coal (reagent) -> steel. Coal joined the
    -- priced set at BL-340 (world_gen.lua's base_price), closing the gap
    -- between this recipe and PRODUCTION.md's Smelter table, which always
    -- named coal as a reagent — and giving coal a consumer, per the
    -- admission rule (BL-340) rather than leaving it an orphan raw.
    --
    -- BL-429: tagged `industrial`. Coal-fired direct smelting is the industrial
    -- route; the ancient arc reaches steel through the bloomery chain at the end
    -- of this file (timber -> charcoal -> blooms -> steel). This retag is what
    -- gives the ancient economy any DEPTH at all — measured before it, ancient max
    -- chain depth was 1, meaning one layer above raws and nothing beyond.
    {
        name    = "steel",
        era     = "industrial", -- BL-429
        inputs  = { iron_ore = 2.0, coal = 1.0 },
        outputs = { steel = 1.0 },
    },

    -- id 1 — Refinery: petroleum -> refined fuel.
    {
        name    = "refined_fuel",
        era     = "industrial", -- BL-433
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
        era     = "industrial", -- BL-433
        inputs  = { water = 1.5, steel = 0.5 },
        outputs = { agricultural_produce = 1.0 },
    },

    -- id 4 — Smelter, second recipe (BL-323 S1, PRODUCTION.md): metallic-asteroid
    -- feedstock needs no carbon addition (already reduced), so this is single-input
    -- like id 0's steel recipe but with no coal reagent.
    {
        name    = "steel_from_iron_nickel",
        era     = "industrial", -- BL-433
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
        era     = "industrial", -- BL-433
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
        era     = "industrial", -- BL-433
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
        era     = "industrial", -- BL-433
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
        era     = "industrial", -- BL-433
        inputs  = { rare_earth_ore = 2.0 },
        outputs = { ree_alloy = 1.0 },
    },

    -- id 10 — Fabricator: steel + refined copper -> machinery. The
    -- Fabricator's alternative path to alloys (id 11) — a real choice
    -- between the two rather than a forced chain.
    {
        name    = "machinery",
        era     = "industrial", -- BL-433
        inputs  = { steel = 1.0, refined_copper = 1.0 },
        outputs = { machinery = 1.0 },
    },

    -- id 11 — Fabricator: steel + REE alloy -> alloys.
    {
        name    = "alloys",
        era     = "industrial", -- BL-433
        inputs  = { steel = 1.0, ree_alloy = 1.0 },
        outputs = { alloys = 1.0 },
    },

    -- id 12 — Electronics Lab: silicon + refined copper + REE alloy -> electronics.
    {
        name    = "electronics",
        era     = "industrial", -- BL-433
        inputs  = { silicon = 1.0, refined_copper = 1.0, ree_alloy = 0.5 },
        outputs = { electronics = 1.0 },
    },

    -- id 13 — Assembly Plant: alloys + electronics -> spacecraft components.
    -- Terminal good; BL-365 gives it deliberately NO background demand so
    -- the militia's BL-350 contracts are its only buyer.
    {
        name    = "spacecraft_components",
        era     = "industrial", -- BL-433
        inputs  = { alloys = 2.0, electronics = 1.0 },
        outputs = { spacecraft_components = 1.0 },
    },

    -- BL-368 (2026-08-11) — the habitability tranche (RESOURCES.md § Habitability
    -- goods). Recipe quantities are first-cut, legible defaults per the item's
    -- own design note ("an implementation-time tuning value... matching how
    -- every other recipe in the prototype is handled"), not a design commitment.
    -- All three run on the generic processing_facility, per the shipped set —
    -- no new building_type enum values.

    -- id 14 — Water Treatment Plant: water -> clean water.
    {
        name    = "clean_water",
        era     = "industrial", -- BL-433
        inputs  = { water = 2.0 },
        outputs = { clean_water = 1.0 },
    },

    -- id 15 — Consumer Goods Factory: food rations + steel -> consumer goods.
    -- "Refined goods (various)" in RESOURCES.md; steel stands in as the one
    -- already-shipped refined industrial input.
    {
        name    = "consumer_goods",
        era     = "industrial", -- BL-433
        inputs  = { food_rations = 1.0, steel = 1.0 },
        outputs = { consumer_goods = 1.0 },
    },

    -- id 16 — Pharmaceutical Lab: water + agricultural produce -> medical
    -- supplies. RESOURCES.md names "chemical + agricultural" inputs; no
    -- standalone chemical resource_type exists in the prototype set, so water
    -- stands in as the process input, mirroring hydroponics_bay's own
    -- water-as-process-input precedent (id 3 above).
    {
        name    = "medical_supplies",
        era     = "industrial", -- BL-433
        inputs  = { water = 1.0, agricultural_produce = 1.0 },
        outputs = { medical_supplies = 1.0 },
    },

    -- =====================================================================
    -- BL-429 — the ANCIENT chain. Appended (never inserted): a recipe's id is
    -- its index here and building_component.recipe stores it, so inserting
    -- would repoint every existing building.
    --
    -- The admission rule (BL-340) applied to the ancient tier: every one of
    -- these consumes a raw that already had deposits, extraction rules and —
    -- until this item — no price and no consumer. Nothing new was added to
    -- resource_type; these are the orphans BL-286 left, given consumers.
    --
    -- The chain is deliberately LAYERED, because chain depth is the growth
    -- track (BL-428): timber -> charcoal -> blooms -> steel is four rungs, so
    -- an ancient corp's reach grows as it builds rather than as a counter ticks.
    -- =====================================================================

    -- id 17 — Charcoal Burner: timber -> charcoal. The ancient fuel step, and
    -- the reason a forest tile is worth holding. Lossy on purpose (3 -> 1): a
    -- burn drives off most of the mass, which is what makes charcoal dearer
    -- per unit than the timber it came from.
    {
        name    = "charcoal",
        era     = "ancient",
        inputs  = { timber = 3.0 },
        outputs = { charcoal = 1.0 },
    },

    -- id 18 — Bloomery: iron ore + charcoal -> iron blooms. The ancient world's
    -- only route to worked iron; no coal, no blast furnace.
    {
        name    = "iron_blooms",
        era     = "ancient",
        inputs  = { iron_ore = 2.0, charcoal = 1.0 },
        outputs = { iron_blooms = 1.0 },
    },

    -- id 19 — Smithy: blooms + charcoal -> steel. The ancient counterpart of
    -- id 0, which is now `industrial`. Same terminal good by a longer road:
    -- depth 3 here against depth 1 there, which is precisely the difference
    -- between the two arcs' industry.
    {
        name    = "steel_from_blooms",
        era     = "ancient",
        inputs  = { iron_blooms = 2.0, charcoal = 1.0 },
        outputs = { steel = 1.0 },
    },

    -- id 20 — Potter/Weaver: clay + timber -> trade goods. Gives BL-286's
    -- placeholder luxury a producer, and clay its first consumer. Timber is
    -- the kiln's fuel, not a component.
    {
        name    = "trade_goods",
        era     = "ancient",
        inputs  = { clay = 2.0, timber = 1.0 },
        outputs = { trade_goods_misc = 1.0 },
    },

    -- id 21 — Miller: agricultural produce + stone -> food rations. The ancient
    -- route to the same good id 2 makes, and stone's first consumer (millstones
    -- wear out). Shallower than the chain above on purpose: food should not be
    -- gated behind an industry, or an ancient start cannot feed itself.
    {
        name    = "food_rations_milled",
        era     = "ancient",
        inputs  = { agricultural_produce = 2.0, stone = 1.0 },
        outputs = { food_rations = 1.0 },
    },
}

print(string.format("[Lua] recipes.lua loaded  recipe_count=%d", #recipes))
