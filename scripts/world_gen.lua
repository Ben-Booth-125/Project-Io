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
        },

        -- Nation tradeable-resource concentration gates (BL-096): a nation
        -- richer than mean * rich_factor fractures into more markets; one
        -- poorer than mean * barren_factor folds into its neighbour.
        carving = {
            rich_factor   = 1.30,
            barren_factor = 0.70,
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
