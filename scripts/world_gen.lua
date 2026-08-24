-- World-generation balance values (BL-236). Authored data for the values
-- world_gen_config reads in src/world/world_gen_config.cpp. Structural
-- generation (body layout, orbits, grid sizes, tile pipeline passes) stays in
-- C++ — this file is scoped to the tunable numbers a modder would actually
-- want to retune, per docs/tech/TECH_FOUNDATIONS.md's Lua boundary (data
-- definitions and balance values, not hot-path simulation code).

world_gen = {
    -- Deposit-density multiplier per abundance tier. `standard` (earth-like) is
    -- the ceiling — no tier should exceed 1.0 (GENERATION_STRATEGY.md § The
    -- resource ceiling).
    deposit_scalar = {
        sparse   = 0.40,
        lean     = 0.65,
        standard = 1.00,
    },

    kepler_market = {
        -- Starting per-resource base price on Kepler's seeded markets.
        base_price = {
            iron_ore             = 2.5,
            petroleum             = 3.5,
            water                 = 1.5,
            agricultural_produce  = 3.0,
            steel                 = 8.0,
            refined_fuel          = 10.0,
            food_rations          = 6.0,

            -- BL-340: closing the minable-but-unsellable asymmetry. These
            -- raws had authored deposits (BL-040) but no base price, so a
            -- processing building drawing on them stalled forever.
            coal                  = 2.0,  -- also the steel reagent, see recipes.lua
            silica                = 2.0,
            copper_ore            = 3.0,
            rare_earth_ore        = 6.0,  -- low concentration, high base price
            iron_nickel_ore       = 3.0,
            platinum_group_metals = 40.0, -- terminal: the belt's high-value trade good

            -- BL-340: the processing-chain roster. Margin widens up the
            -- tiers (Tier 3 promises "widest price divergence" per
            -- RESOURCES.md) — spacecraft_components sits 56x iron ore,
            -- the value gradient the space-equipment premise rests on.
            -- First-cut authored constants; retune by playtest.
            silicon                = 5.0,
            refined_copper         = 7.5,
            ree_alloy              = 16.0,
            machinery              = 22.0,
            alloys                 = 34.0,
            electronics            = 29.0,
            spacecraft_components  = 140.0,

            -- BL-368: the habitability tranche. Priced modestly above their
            -- primary input (water 1.5 / agricultural_produce 3.0 / food_rations
            -- 6.0 / steel 8.0) — welfare goods, not high-margin industrial
            -- products (RESOURCES.md § Habitability goods). First-cut, retune
            -- by playtest.
            clean_water            = 3.0,
            consumer_goods         = 12.0,
            medical_supplies       = 14.0,

            -- BL-457: the military terminal good. DERIVED from the processing
            -- roster's own markup convention rather than picked — that roster
            -- prices an output at 1.415-1.443x its input basket (machinery
            -- 1.419, alloys 1.417, electronics 1.415, spacecraft_components
            -- 1.443). Ordnance draws steel 8.0 + machinery 22.0 = 30.0, so
            -- 43.0 is a ratio of 1.433, inside that band. Re-derive if either
            -- input's price or the recipe quantities change (recipes.lua id 27).
            ordnance               = 43.0,

            -- BL-429: the ANCIENT tier. BL-340 closed the minable-but-unsellable
            -- asymmetry for the space chain and left it wide open here — every one
            -- of these had authored deposits and extraction rules (tile_generation
            -- generate_deposits, placement_rules k_extractable) and no price at all,
            -- so an ancient corp could mine them and never sell a unit.
            --
            -- Priced as bulk commons: cheap, heavy, worth something only in volume
            -- or after work. Stone and timber sit below iron ore (2.5) because they
            -- are everywhere; peat below both, being the poor fuel.
            stone                  = 1.0,
            timber                 = 1.5,
            sand                   = 1.0,
            clay                   = 1.2,
            peat                   = 1.2,

            -- NR-257 (2026-08-16, Ben's call). Regolith had no price by design —
            -- RESOURCES.md had it "excluded from market tables", present only so
            -- building cost formulas could reference it, which none ever did.
            -- Giving it the In-Situ Smelter as a consumer put it into market
            -- demand at the tiles that run one, so the exclusion stopped being
            -- tenable.
            --
            -- 0.6 is the CHEAPEST good in this table, below the stone/sand bulk
            -- floor of 1.0, which is what "high mass, low unit value" should mean
            -- for something present on every tile of every airless body at
            -- deposits of 20-50.
            --
            -- The recipe ratio moved WITH it, and had to. At 8 regolith per steel
            -- the in-situ route would clear 8.0 - 4.8 = 3.2 against the Smelter's
            -- 1.0 and the iron-nickel route's 2.0 — making the deliberately poor
            -- route the most profitable steel in the game. At 12 it clears 0.8:
            -- thin, positive, and the worst of the three industrial routes, which
            -- is the shape the recipe was authored for. Pricing a good is not
            -- separable from the recipes that consume it.
            regolith               = 0.6,

            -- The ancient intermediates. Charcoal is dearer than the timber it
            -- comes from (a burn takes days and loses mass); blooms carry the
            -- charcoal plus the ore.
            charcoal               = 4.0,
            iron_blooms            = 9.0,

            -- BL-286's placeholder luxury, given a producer at last (the potter's
            -- and weaver's output). Priced as a modest trade good, not a treasure.
            trade_goods_misc       = 8.0,

            -- BL-585/BL-586 (2026-08-24) — the wide ancient roster's first slice.
            -- Every price DERIVED at the roster's observed ~1.433x markup over
            -- its input basket (id 27 ordnance's own ratio, recipes.lua), not
            -- picked. Re-derive if an input quantity in recipes.lua changes.
            ceramics               = 3.4,  -- clay 2.0 * 1.2 = 2.4; 2.4 * 1.433
            dressed_stone          = 2.9,  -- stone 2.0 * 1.0 = 2.0; 2.0 * 1.433
            planks                 = 4.3,  -- timber 2.0 * 1.5 = 3.0; 3.0 * 1.433
            tools                  = 25.5, -- iron_blooms 1.5*9.0 + planks 1.0*4.3 = 17.8; 17.8 * 1.433
        },

        -- Nation tradeable-resource concentration gates (BL-096): a nation
        -- richer than mean * rich_factor fractures into more markets; one
        -- poorer than mean * barren_factor folds into its neighbour.
        -- corp_presence_gain (BL-132): each distinct corporation holding an
        -- asset in a nation's territory multiplies its concentration by
        -- (1 + this), so a commercially-contested territory fractures
        -- further on top of raw geology. 0 disables the effect.
        carving = {
            rich_factor        = 1.30,
            barren_factor      = 0.70,
            corp_presence_gain = 0.15,
        },

        -- Endemic-good distance pricing (BL-191): cheap at the source tile,
        -- dearer by distance_gain across the globe.
        endemic = {
            source_price  = 1.5,
            distance_gain = 7.0,
        },
    },

    corporations = {
        -- Non-player corporations generated alongside the player's own.
        count = 8,
    },
}
