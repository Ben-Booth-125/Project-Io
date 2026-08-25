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
-- BL-434: an optional `group` field ("Metal Foundry", "Food Processing", ...) says
-- which sub-facility KIND a recipe belongs to, so the generic processing_facility
-- building type reads (Build door) and switches (recipe-switch cost) as several
-- distinct facility kinds. Absent means "General" (see recipe_registry.hpp); every
-- recipe below is tagged since a natural sibling group exists for all of them
-- (see docs/economy/PRODUCTION.md § Sub-facility groups for the taxonomy table).
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
        group   = "Metal Foundry", -- BL-434
        inputs  = { iron_ore = 2.0, coal = 1.0 },
        outputs = { steel = 1.0 },
        -- BL-615: the heavy processor class — a steel mill must sit NEAR a
        -- population centre ("you can't build a steel mill far from a
        -- population centre", Ben, 2026-08-25). 6 grid steps: a quarter of
        -- the 24-cost logistics reach budget over plains, so the mill rule
        -- binds visibly tighter than reach without demanding co-location —
        -- POPULATION.md § Strata gate buildings.
        centre_proximity_radius = 6,
    },

    -- id 1 — Refinery: petroleum -> refined fuel.
    {
        name    = "refined_fuel",
        era     = "industrial", -- BL-433
        group   = "Refinery", -- BL-434
        inputs  = { petroleum = 2.0 },
        outputs = { refined_fuel = 1.0 },
    },

    -- id 2 — Food Processor: agricultural produce -> food rations.
    {
        name    = "food_rations",
        group   = "Food Processing", -- BL-434
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
        group   = "Food Processing", -- BL-434: agriculture-adjacent, same feed-the-population kind
        inputs  = { water = 1.5, steel = 0.5 },
        outputs = { agricultural_produce = 1.0 },
    },

    -- id 4 — Smelter, second recipe (BL-323 S1, PRODUCTION.md): metallic-asteroid
    -- feedstock needs no carbon addition (already reduced), so this is single-input
    -- like id 0's steel recipe but with no coal reagent.
    {
        name    = "steel_from_iron_nickel",
        era     = "industrial", -- BL-433
        group   = "Metal Foundry", -- BL-434
        inputs  = { iron_nickel_ore = 2.0 },
        outputs = { steel = 1.0 },
        centre_proximity_radius = 6, -- BL-615: heavy processor class — see the steel recipe's note.
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
        group   = "Chemical Works", -- BL-434
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
        group   = "Chemical Works", -- BL-434
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
        group   = "Electronics", -- BL-434
        inputs  = { silica = 2.0 },
        outputs = { silicon = 1.0 },
    },

    -- id 8 — Smelter: copper ore -> refined copper.
    {
        name    = "refined_copper",
        group   = "Metal Foundry", -- BL-434
        inputs  = { copper_ore = 2.0 },
        outputs = { refined_copper = 1.0 },
    },

    -- id 9 — Refinery: rare earth ore -> REE alloy.
    {
        name    = "ree_alloy",
        era     = "industrial", -- BL-433
        group   = "Electronics", -- BL-434
        inputs  = { rare_earth_ore = 2.0 },
        outputs = { ree_alloy = 1.0 },
    },

    -- id 10 — Fabricator: steel + refined copper -> machinery. The
    -- Fabricator's alternative path to alloys (id 11) — a real choice
    -- between the two rather than a forced chain.
    {
        name    = "machinery",
        era     = "industrial", -- BL-433
        group   = "Advanced Fabrication", -- BL-434
        inputs  = { steel = 1.0, refined_copper = 1.0 },
        outputs = { machinery = 1.0 },
    },

    -- id 11 — Fabricator: steel + REE alloy -> alloys.
    {
        name    = "alloys",
        era     = "industrial", -- BL-433
        group   = "Advanced Fabrication", -- BL-434
        inputs  = { steel = 1.0, ree_alloy = 1.0 },
        outputs = { alloys = 1.0 },
    },

    -- id 12 — Electronics Lab: silicon + refined copper + REE alloy -> electronics.
    {
        name    = "electronics",
        era     = "industrial", -- BL-433
        group   = "Electronics", -- BL-434
        inputs  = { silicon = 1.0, refined_copper = 1.0, ree_alloy = 0.5 },
        outputs = { electronics = 1.0 },
    },

    -- id 13 — Assembly Plant: alloys + electronics -> spacecraft components.
    -- Terminal good; BL-365 gives it deliberately NO background demand so
    -- the militia's BL-350 contracts are its only buyer.
    {
        name    = "spacecraft_components",
        era     = "industrial", -- BL-433
        group   = "Advanced Fabrication", -- BL-434
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
        group   = "Welfare Goods", -- BL-434
        inputs  = { water = 2.0 },
        outputs = { clean_water = 1.0 },
    },

    -- id 15 — Consumer Goods Factory: food rations + steel -> consumer goods.
    -- "Refined goods (various)" in RESOURCES.md; steel stands in as the one
    -- already-shipped refined industrial input.
    {
        name    = "consumer_goods",
        era     = "industrial", -- BL-433
        group   = "Welfare Goods", -- BL-434
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
        group   = "Welfare Goods", -- BL-434
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
        name         = "charcoal",
        display_name = "Charcoal Burner", -- BL-429 slice 2
        era          = "ancient",
        group        = "Fuel Production", -- BL-434: fuel/energy, not metal itself; the Bloomery/Smithy's supplier
        inputs       = { timber = 3.0 },
        outputs      = { charcoal = 1.0 },
    },

    -- id 18 — Bloomery: iron ore + charcoal -> iron blooms. The ancient world's
    -- only route to worked iron; no coal, no blast furnace.
    {
        name         = "iron_blooms",
        display_name = "Bloomery", -- BL-429 slice 2
        era          = "ancient",
        group        = "Metal Foundry", -- BL-434
        inputs       = { iron_ore = 2.0, charcoal = 1.0 },
        outputs      = { iron_blooms = 1.0 },
    },

    -- id 19 — Smithy: blooms + charcoal -> steel. The ancient counterpart of
    -- id 0, which is now `industrial`. Same terminal good by a longer road:
    -- depth 3 here against depth 1 there, which is precisely the difference
    -- between the two arcs' industry.
    {
        name         = "steel_from_blooms",
        display_name = "Smithy", -- BL-429 slice 2
        era          = "ancient",
        group        = "Metal Foundry", -- BL-434
        inputs       = { iron_blooms = 2.0, charcoal = 1.0 },
        outputs      = { steel = 1.0 },
    },

    -- id 20 — Potter/Weaver: clay + timber -> trade goods. Gives BL-286's
    -- placeholder luxury a producer, and clay its first consumer. Timber is
    -- the kiln's fuel, not a component.
    {
        name         = "trade_goods",
        display_name = "Potter & Weaver", -- BL-429 slice 2
        era          = "ancient",
        group        = "Artisan Goods", -- BL-434: shares its terminal good with the Glassworks below
        inputs       = { clay = 2.0, timber = 1.0 },
        outputs      = { trade_goods_misc = 1.0 },
    },

    -- id 21 — Miller: agricultural produce + stone -> food rations. The ancient
    -- route to the same good id 2 makes, and stone's first consumer (millstones
    -- wear out). Shallower than the chain above on purpose: food should not be
    -- gated behind an industry, or an ancient start cannot feed itself.
    {
        name         = "food_rations_milled",
        display_name = "Miller", -- BL-429 slice 2
        era          = "ancient",
        group        = "Food Processing", -- BL-434
        inputs       = { agricultural_produce = 2.0, stone = 1.0 },
        outputs      = { food_rations = 1.0 },
    },

    -- BL-429 slice 2 (2026-08-15) — closing R6, the two raws slice 1 recorded as
    -- "still orphaned": sand and peat were priced but consumed by nothing. Both
    -- were extractable in the tile-generation sense (sand already in
    -- placement_rules::k_extractable, peat's deposits authored in
    -- tile_generation.cpp but never added to k_extractable — a real gap, closed
    -- alongside this recipe) but had no building to place on them.

    -- id 22 — Glassworks: sand -> trade goods. A second, independent producer of
    -- trade_goods_misc alongside the Potter/Weaver — precedented by industrial
    -- `steel` already having three (coal-smelter, iron-nickel, bloomery route):
    -- distinct raws feeding a shared terminal good is an ordinary multi-producer
    -- economy fact, not BL-430's alternate-METHOD feature (one building offering
    -- interchangeable recipes for the same output).
    {
        name         = "glass",
        display_name = "Glassworks", -- BL-429 slice 2
        era          = "ancient",
        group        = "Artisan Goods", -- BL-434: second producer of trade_goods_misc, same group as Potter & Weaver
        inputs       = { sand = 2.0 },
        outputs      = { trade_goods_misc = 1.0 },
    },

    -- id 23 — Peat Kiln: peat -> charcoal. Peat as the poorer, cheaper
    -- pre-industrial fuel (world_gen.lua prices it below timber) beside the
    -- Charcoal Burner's wood route — the same "more than one raw reaches this
    -- good" shape as id 22 above, not a tuned alternate method.
    {
        name         = "peat_charcoal",
        display_name = "Peat Kiln", -- BL-429 slice 2
        era          = "ancient",
        group        = "Fuel Production", -- BL-434: second producer of charcoal, same group as Charcoal Burner
        inputs       = { peat = 2.0 },
        outputs      = { charcoal = 1.0 },
    },

    -- =====================================================================
    -- NR-257 (2026-08-16) — consumers for the three one-directional orphans
    -- chain_depth's R1 row found: machinery, platinum_group_metals and
    -- regolith were each obtainable and consumed by nothing.
    --
    -- APPENDED, not inserted, per id 6's note: recipe ids are POSITIONAL and
    -- inserting would silently repoint every saved building's selection.
    --
    -- Each is authored with inputs DISJOINT from its output's existing
    -- recipes, so chain_depth's R2 classifies it as a supply route (a second
    -- raw reaching a good) rather than an interchangeable method to be
    -- price-compared. That is the honest classification, not a way around the
    -- guard: all three are gated on reaching a PLACE — the belt, an airless
    -- body — rather than on picking the cheaper of two options at one building.
    -- =====================================================================

    -- id 24 — Assembly Plant, HEAVY route: machinery + steel -> spacecraft
    -- components. The crude counterpart to id 13's alloys + electronics: more
    -- structural mass and worked machinery, none of the refined-electronics
    -- chain. This is machinery's first consumer — before it, the Fabricator
    -- produced machinery and nothing in the roster wanted any.
    {
        name         = "spacecraft_components_heavy",
        display_name = "Heavy Assembly Plant",
        era          = "industrial",
        group        = "Advanced Fabrication", -- same group as id 13
        inputs       = { machinery = 2.0, steel = 2.0 },
        outputs      = { spacecraft_components = 1.0 },
    },

    -- id 25 — Electronics Lab, CONTACT-GRADE route: platinum group metals ->
    -- electronics. PGM are catalytic and contact metals (RESOURCES.md), and at
    -- base_price 40 this is a deliberately EXPENSIVE premium route rather than
    -- a cheap bypass of id 12's silicon + copper + REE chain.
    --
    -- A REAL TENSION, recorded rather than papered over: world_gen.lua tags PGM
    -- "terminal: the belt's high-value trade good", so its documented role was
    -- to be SOLD, not consumed. This gives it a consumer per Ben's instruction
    -- (NR-257 option D) without displacing that role — selling to the belt
    -- market stays the obvious use, and this is the route for a corp that would
    -- rather refine its own than ship it out.
    {
        name         = "electronics_contact_grade",
        display_name = "Contact-Grade Electronics Lab",
        era          = "industrial",
        group        = "Electronics", -- same group as id 12
        inputs       = { platinum_group_metals = 0.5 },
        outputs      = { electronics = 1.0 },
    },

    -- id 26 — Smelter, IN-SITU route: regolith -> steel. Regolith reduction on
    -- an airless body, which is exactly the "in-situ build mass" role
    -- RESOURCES.md already gives it. Deliberately poor grade — 12 regolith per
    -- steel against id 0's 2 iron ore — because regolith is abundant (present
    -- on every tile of every airless body, deposits 20-50) and the point is
    -- that you can build FROM WHERE YOU ARE, not that it is efficient.
    --
    -- THE RATIO IS TIED TO regolith's base_price (0.6, world_gen.lua, NR-257)
    -- and cannot be read without it. At 12 the basket costs 7.2 against steel's
    -- 8.0 — clearing 0.8, thinner than the Smelter's 1.0 and the iron-nickel
    -- route's 2.0, so this is the worst of the three industrial steel routes as
    -- intended. It was authored at 8 while regolith was unpriced; pricing the
    -- good at anything low enough to mean "high mass, low unit value" would
    -- have made 8:1 the MOST profitable steel in the game. Change either number
    -- and check the other.
    {
        name         = "steel_from_regolith",
        display_name = "In-Situ Smelter",
        era          = "industrial",
        group        = "Metal Foundry", -- same group as ids 0, 4 and 19
        inputs       = { regolith = 12.0 },
        outputs      = { steel = 1.0 },
    },

    -- id 27 — Fabricator: steel + machinery -> ordnance (BL-457). The MILITARY
    -- terminal good, and the roster's first one: until this recipe the chain
    -- terminated only in spacecraft_components, so the "trade coloured by
    -- military use" half of the 2026-08-10 refocus had no object to trade.
    --
    -- Sits at the same depth as alloys (id 11) and electronics (id 12) — one
    -- step below the Assembly Plant — deliberately, so BL-428's chain-depth gate
    -- treats it as a real growth-track object rather than something a starting
    -- corp can make on tick one. It draws machinery, which gives that good a
    -- second consumer beside id 24's heavy spacecraft route.
    --
    -- PRICE IS DERIVED, NOT PICKED. The processing roster prices an output at a
    -- strikingly tight markup over its input basket: machinery 1.419, alloys
    -- 1.417, electronics 1.415, spacecraft_components 1.443. Inputs here cost
    -- steel 8.0 + machinery 22.0 = 30.0, so base_price 43.0 (world_gen.lua) is
    -- a ratio of 1.433 — inside the observed band rather than beside it. Change
    -- either input quantity and re-derive rather than leaving the price to drift.
    --
    -- NO ANCIENT ROUTE HERE. An iron_blooms + charcoal path is a real idea and
    -- belongs with BL-429's ancient roster, not smuggled in beside an
    -- industrial-era recipe.
    {
        name    = "ordnance",
        era     = "industrial", -- BL-433
        group   = "Advanced Fabrication", -- BL-434, with machinery and alloys
        inputs  = { steel = 1.0, machinery = 1.0 },
        outputs = { ordnance = 1.0 },
    },

    -- id 28 — Smithy, ANCIENT ordnance route (BL-460): iron_blooms + charcoal
    -- -> ordnance. The route id 27's own comment named and deliberately left
    -- out ("belongs with BL-429's ancient roster, not smuggled in beside an
    -- industrial-era recipe") — that deferral shipped without a tracking item,
    -- so the ancient campaign (epoch_year = 0, the shipped default) drew a good
    -- it could never make: BL-454's unit upkeep is band-independent (units
    -- exist in both arcs), but ordnance's only producer was industrial-only.
    --
    -- SAME BUILDING AS id 19 (Smithy, "Metal Foundry" group), not a new one —
    -- the task explicitly asks to reuse the roster rather than invent a
    -- fourth ancient metalworking building. The Smithy already turns
    -- iron_blooms + charcoal into steel; this is BL-430's alternate-recipe
    -- mechanic doing exactly what it exists for — one built facility,
    -- switchable between its steel and ordnance recipes.
    --
    -- SAME QUANTITIES AS id 19's steel recipe (2 iron_blooms + 1 charcoal),
    -- deliberately: this is the identical basket the Smithy already draws,
    -- not a retuned one, so the switch between "make steel" and "make
    -- ordnance" is a straight recipe swap rather than a new resourcing
    -- decision. Ordnance's base_price (43.0, world_gen.lua, BL-457) is
    -- already authored and unchanged by this recipe.
    {
        name    = "ordnance_from_blooms",
        display_name = "Smithy", -- same building as id 19
        era     = "ancient",
        group   = "Metal Foundry", -- BL-434, with the Bloomery/Smithy steel chain
        inputs  = { iron_blooms = 2.0, charcoal = 1.0 },
        outputs = { ordnance = 1.0 },
    },

    -- =====================================================================
    -- BL-587 (2026-08-23) — the roster's first GENUINE interchangeable
    -- methods. chain_depth's R2 buckets every prior same-output sibling pair
    -- as a supply route (disjoint raws) or a named precondition; the
    -- "methods" bucket that BL-430's alternate-production-methods ruling
    -- describes was EMPTY. These two share an input with an existing sibling
    -- (so R2 actually compares them) and trade BL-430's chain-depth axis: a
    -- deeper method that needs a good not yet reached, in exchange for a
    -- cheaper input basket once it is. Neither dominates its sibling — R2
    -- checks this mechanically, not just by inspection.
    -- =====================================================================

    -- id 29 — Coking Kiln: a second route to charcoal, sharing `timber` with
    -- id 17's Charcoal Burner (so R2 actually compares them, unlike the
    -- Peat Kiln's disjoint raw). Reagent `iron_blooms` (a small, non-fuel
    -- quantity — tongs and tools, not stock) needs the Bloomery already
    -- built, so this route is DEEPER (required depth 2, against the
    -- Charcoal Burner's 0) but its reference cost is lower per batch. Early
    -- game runs the Charcoal Burner because nothing else exists yet; once a
    -- corp has smelted its first blooms, the Kiln becomes the cheaper way to
    -- keep making charcoal — the trade-off is about WHEN it opens, not
    -- whether it is stronger.
    {
        name         = "charcoal_from_kiln",
        display_name = "Coking Kiln",
        era          = "ancient",
        group        = "Fuel Production", -- BL-434, with the Charcoal Burner and Peat Kiln
        inputs       = { timber = 1.5, iron_blooms = 0.1 },
        outputs      = { charcoal = 1.0 },
    },

    -- id 30 — Bessemer Converter: a second route to industrial steel,
    -- sharing both `iron_ore` and `coal` with id 0's Smelter. Reagent
    -- `machinery` (required depth 2) puts this route below the Smelter's
    -- shallow one (required depth 0) on the same growth-track logic as id
    -- 29 above — a corp needs an existing machine-tool chain before it can
    -- build the converter that then makes its own steel cheaper. The
    -- industrial arc's counterpart to the ancient Kiln, on the same axis.
    {
        name         = "steel_bessemer",
        display_name = "Bessemer Converter",
        era          = "industrial",
        group        = "Metal Foundry", -- BL-434, with the Smelter and its siblings
        inputs       = { iron_ore = 1.5, coal = 0.5, machinery = 0.15 },
        outputs      = { steel = 1.0 },
        centre_proximity_radius = 6, -- BL-615: heavy processor class — see the steel recipe's note.
    },

    -- =====================================================================
    -- BL-585/BL-586 (2026-08-24) — the wide ancient roster's first slice:
    -- four named buildings on EXISTING raws (clay, stone, timber,
    -- iron_blooms), no new deposit or extraction target. `hides` and its
    -- leather/cloth consumers are a later slice — they need a new
    -- extractable raw with real tile-generation deposits, deliberately not
    -- attempted here (components.hpp's enum comment names the deferral).
    --
    -- PRICE IS DERIVED, NOT PICKED, same method id 27's comment states: the
    -- processing roster prices an output at ~1.42x its input basket
    -- (ordnance's own ratio, 1.433). Every base_price below in world_gen.lua
    -- follows that ratio; re-derive rather than hand-tuning if an input
    -- quantity changes.
    -- =====================================================================

    -- id 31 — Potter's Kiln: clay -> ceramics. Clay's SECOND consumer
    -- (alongside id 20's Potter & Weaver, which uses clay for
    -- trade_goods_misc) — a distinct terminal good, not an alternate method:
    -- the two recipes share clay but produce different outputs, so
    -- chain_depth's R2 never even pairs them (its grouping key is primary
    -- output resource, not input).
    {
        name         = "ceramics_kiln",
        display_name = "Potter's Kiln",
        era          = "ancient",
        group        = "Construction Materials", -- BL-434, new group — see PRODUCTION.md
        inputs       = { clay = 2.0 },
        outputs      = { ceramics = 1.0 },
    },

    -- id 32 — Stonemason: stone -> dressed_stone. Stone's second consumer
    -- (alongside id 21's Miller, which uses stone as a millstone reagent) —
    -- a terminal good sold to the market, same shape as ceramics above.
    {
        name         = "stonemason",
        display_name = "Stonemason",
        era          = "ancient",
        group        = "Construction Materials", -- BL-434, with the Kiln above
        inputs       = { stone = 2.0 },
        outputs      = { dressed_stone = 1.0 },
    },

    -- id 33 — Sawmill: timber -> planks. Timber's third consumer (alongside
    -- id 17's Charcoal Burner and id 20's Potter & Weaver). Unlike those
    -- two, planks is NOT terminal — it feeds the Toolmaker below, so the
    -- Sawmill is the roster's first ancient building whose whole purpose is
    -- an intermediate rather than a sale.
    {
        name         = "sawmill",
        display_name = "Sawmill",
        era          = "ancient",
        group        = "Construction Materials", -- BL-434, with the Kiln and Stonemason
        inputs       = { timber = 2.0 },
        outputs      = { planks = 1.0 },
    },

    -- id 34 — Toolmaker: iron_blooms + planks -> tools. Required depth is
    -- max(depth(blooms)=2, depth(planks)=1) = 2, so depth(tools) = 1+2 = 3 —
    -- TIED with the ancient ceiling (steel_from_blooms, ordnance_from_blooms),
    -- not past it (chain_depth's D6 confirms: ancient max stays 3). Still the
    -- roster's first chain that needs BOTH a smelted good and a milled one at
    -- once — proof depth composes ACROSS chains, not just within one — which
    -- is worth keeping even though it does not raise the ceiling. Terminal
    -- for now (sold to the market); BL-590 gives it a construction-material
    -- consumer later.
    {
        name         = "toolmaker",
        display_name = "Toolmaker",
        era          = "ancient",
        group        = "Metal Foundry", -- BL-434, with the Bloomery/Smithy chain — its deepest member
        inputs       = { iron_blooms = 1.5, planks = 1.0 },
        outputs      = { tools = 1.0 },
    },

    -- =====================================================================
    -- BL-586 slice 2 (2026-08-24) — the three chains slice 1 deliberately
    -- deferred (components.hpp's enum comment names the deferral): all three
    -- need a new extractable raw with real tile-generation deposits.
    -- `hides` is ENDEMIC (Ben's ruling: lat/sector-restricted, richness-scored,
    -- like furs — NOT the plain cover-based ambient mechanic timber/clay use);
    -- `fibre` is the ordinary case, grown by the SAME cover-based ambient/
    -- biotic mechanic agricultural_produce uses (tile_generation.cpp).
    --
    -- PRICE IS DERIVED, NOT PICKED, same method as the block above: ~1.433x
    -- markup over the input basket (id 27 ordnance's own ratio). Re-derive
    -- rather than hand-tuning if an input quantity changes.
    -- =====================================================================

    -- id 35 — Tannery: hides -> leather. Hides' only consumer; leather is
    -- TERMINAL — sold to the market, reprocessed by nothing, same shape as
    -- ceramics/dressed_stone above.
    {
        name         = "tannery",
        display_name = "Tannery",
        era          = "ancient",
        group        = "Artisan Goods", -- BL-434, with the Potter & Weaver and Glassworks
        inputs       = { hides = 2.0 },
        outputs      = { leather = 1.0 },
    },

    -- id 36 — Weaver: fibre -> cloth. Fibre's only consumer. Unlike leather,
    -- cloth is NOT terminal — it feeds the Shipwright below, the roster's
    -- second intermediate-only ancient output (alongside planks).
    {
        name         = "weaver",
        display_name = "Weaver",
        era          = "ancient",
        group        = "Artisan Goods", -- BL-434, with the Tannery above
        inputs       = { fibre = 2.0 },
        outputs      = { cloth = 1.0 },
    },

    -- id 37 — Shipwright: planks + cloth -> rigging. Required depth is
    -- max(depth(planks)=1, depth(cloth)=1) = 1, so depth(rigging) = 1+1 = 2 —
    -- past the flat depth-1 ceiling every other slice-2 chain sits at, and
    -- the design's own "every chain should reach depth 2 or better" bar.
    -- `rigging` is the chosen name for the design table's "terminal trade
    -- good": ropework/cordage/tackle, a genuine ancient-craft good a
    -- shipyard plausibly makes, in the same generic-material-noun register
    -- as ceramics/dressed_stone/tools (never a proper noun). Terminal — sold
    -- to the market, reprocessed by nothing.
    {
        name         = "shipwright",
        display_name = "Shipwright",
        era          = "ancient",
        group        = "Advanced Fabrication", -- BL-434 — new to the ancient roster (already used by the industrial machinery/alloys/spacecraft_components chain)
        inputs       = { planks = 1.5, cloth = 1.0 },
        outputs      = { rigging = 1.0 },
    },
}

print(string.format("[Lua] recipes.lua loaded  recipe_count=%d", #recipes))
