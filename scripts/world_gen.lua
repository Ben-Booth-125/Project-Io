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
            steel                 = 13.6,
            refined_fuel          = 15.6,
            food_rations          = 13.6,

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
            silicon                = 9.6,
            refined_copper         = 13.6,
            ree_alloy              = 25.6,
            machinery              = 56,
            alloys                 = 80,
            electronics            = 41.6,
            spacecraft_components  = 280,

            -- BL-368: the habitability tranche. Priced modestly above their
            -- primary input (water 1.5 / agricultural_produce 3.0 / food_rations
            -- 6.0 / steel 8.0) — welfare goods, not high-margin industrial
            -- products (RESOURCES.md § Habitability goods). First-cut, retune
            -- by playtest.
            clean_water            = 7.7,
            consumer_goods         = 56,
            medical_supplies       = 14.0,

            -- BL-457: the military terminal good. DERIVED from the processing
            -- roster's own markup convention rather than picked — that roster
            -- prices an output at 1.415-1.443x its input basket (machinery
            -- 1.419, alloys 1.417, electronics 1.415, spacecraft_components
            -- 1.443). Ordnance draws steel 8.0 + machinery 22.0 = 30.0, so
            -- 43.0 is a ratio of 1.433, inside that band. Re-derive if either
            -- input's price or the recipe quantities change (recipes.lua id 27).
            ordnance               = 140.8,

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
            -- It sits AT the stone/sand bulk floor of 1.0, not below it. It was
            -- authored at 0.6 - "high mass, low unit value" for something present
            -- on every tile of every airless body at deposits of 20-50 - and
            -- BL-744 (NR-779) lifted it: at 0.6 no fixed cost that still leaves a
            -- mine a building could cover the floor half of the recipe margin
            -- anchor (an off-world site earned 3.0/tick at the price floor against
            -- 4.8 of costs). It is the one raw price the anchor moved.
            --
            -- The recipe ratio moved WITH it, and had to. At 12 regolith per steel
            -- the in-situ route cleared inputs by 1.6 against the Smelter's 6.6
            -- and the iron-nickel route's 7.6 and failed the floor half; at 8.5 it
            -- clears 5.1 - still the worst of the three industrial routes, which
            -- is the shape the recipe was authored for. Pricing a good is not
            -- separable from the recipes that consume it.
            regolith               = 1,

            -- The ancient intermediates. Charcoal is dearer than the timber it
            -- comes from (a burn takes days and loses mass); blooms carry the
            -- charcoal plus the ore.
            charcoal               = 6.5,
            iron_blooms            = 24.6,

            -- BL-286's placeholder luxury, given a producer at last (the potter's
            -- and weaver's output). Priced as a modest trade good, not a treasure.
            trade_goods_misc       = 8.0,

            -- BL-585/BL-586 (2026-08-24) — the wide ancient roster's first slice.
            -- Every price DERIVED at the roster's observed ~1.433x markup over
            -- its input basket (id 27 ordnance's own ratio, recipes.lua), not
            -- picked. Re-derive if an input quantity in recipes.lua changes.
            ceramics               = 6.5,  -- clay 2.0 * 1.2 = 2.4; 2.4 * 1.433
            dressed_stone          = 6.1,  -- stone 2.0 * 1.0 = 2.0; 2.0 * 1.433
            planks                 = 7.6,  -- timber 2.0 * 1.5 = 3.0; 3.0 * 1.433
            tools                  = 90.6, -- iron_blooms 1.5*9.0 + planks 1.0*4.3 = 17.8; 17.8 * 1.433

            -- BL-586 slice 2 (2026-08-24) — Tannery/Weaver/Shipwright. `hides`
            -- and `fibre` are raws, priced like the ambient/endemic bulk
            -- commons above (stone 1.0 / timber 1.5 / clay 1.2): `hides` a
            -- little dearer for being endemic (rarer, regional, like the
            -- unpriced tobacco/spices/coffee/furs would be); `fibre` at the
            -- plain ambient-crop tier, alongside clay/peat. `leather`/`cloth`/
            -- `rigging` all DERIVED at the same ~1.433x markup as above.
            hides                  = 2.5,  -- endemic raw, dearer than the ambient bulk floor
            fibre                  = 1.3,  -- ambient crop, same tier as clay/peat
            leather                = 11.6,  -- hides 2.0*2.5 = 5.0; 5.0 * 1.433
            cloth                  = 6.8,  -- fibre 2.0*1.3 = 2.6; 2.6 * 1.433
            rigging                = 38, -- planks 1.5*4.3 + cloth 1.0*3.7 = 10.15; 10.15 * 1.433

            -- BL-708 (2026-08-31) — POWER, the grid good. It HAS a price, and
            -- that is the design's whole first claim: power clears on the market
            -- like any other good, and only its MOVEMENT is special (it rides
            -- the road network, never a convoy). A base price is therefore not
            -- optional decoration here — a good with base_price 0 is unbuyable
            -- by the arithmetic of the reservation ceiling, so an unpriced power
            -- would leave every upkeep draw permanently unmet.
            --
            -- DERIVED, not picked, at the roster's ~1.433x markup, jointly with
            -- the two recipes in recipes.lua (see their comment for the full
            -- working): coal 2.0*2.0 = 4.0 in -> 4.0 power out, and petroleum
            -- 1.0*3.5 = 3.5 in -> 3.5 power out, both clearing 1.45x at this
            -- price. Cheaper PER UNIT than either fuel because one unit of fuel
            -- makes about two of power: power is the divided, distributed form
            -- of the same energy, and the price says so.
            power                  = 2.6,

            -- BL-709 (2026-08-31) — CONSTRUCTION CAPACITY, the sector's product.
            -- docs/economy/PRODUCTION.md § Construction as a rate.
            --
            -- It needs a price for exactly the reason power did: a good with
            -- base_price 0 is unbuyable by the arithmetic of the reservation
            -- ceiling, so an unpriced capacity would leave every build and every
            -- maintenance draw permanently unmet — the shape of the BL-641
            -- collapse, arrived at from the pricing side instead of the supply
            -- side.
            --
            -- 3.0 is the SHARED ANCHOR the five methods are sized against, and
            -- it is chosen so all five clear the roster's own observed
            -- 1.415-1.443x markup at once (recipes.lua carries the full working
            -- per method). Sizing the METHODS to the price, rather than picking
            -- five prices, is what makes the choice of method a question about
            -- what the ground carries instead of a question about which method
            -- is quietly the better business:
            --
            --   timber frame        : in 7.30 -> out 3.50 * 3.0 = 10.50  (1.438x)
            --   stone and brick     : in 6.10 -> out 2.90 * 3.0 =  8.70  (1.426x)
            --   iron frame          : in 7.25 -> out 3.45 * 3.0 = 10.35  (1.428x)
            --   steel frame         : in 8.00 -> out 3.80 * 3.0 = 11.40  (1.425x)
            --   reinforced concrete : in 8.00 -> out 3.80 * 3.0 = 11.40  (1.425x)
            --
            -- Re-derive all five if this moves, or if any input price does.
            construction_capacity  = 4.8,
        },

        -- Nation tradeable-resource concentration gates (BL-096): a nation
        -- richer than mean * rich_factor fractures into more markets; one
        -- poorer than mean * barren_factor folds into its neighbour.
        -- corp_presence_gain (BL-132): each distinct corporation holding an
        -- asset in a nation's territory multiplies its concentration by
        -- (1 + this), so a commercially-contested territory fractures
        -- further on top of raw geology. 0 disables the effect.

        -- BL-744 (2026-09-02): the ANCIENT band's overrides. Base prices are
        -- era-banded for the reason recipes are (BL-433): the ancient band
        -- reaches steel by timber -> charcoal -> blooms -> steel, three doubling
        -- stages under the anchor, where the industrial band reaches it in one
        -- (ore + coal). One shared number cannot clear both - priced off the
        -- bloom chain, steel dragged the industrial ladder to 600x iron ore.
        -- Absent = inherit the table above. Read by world_gen_config::
        -- base_price_for_epoch at market seeding; recipe_margin checks each
        -- band against its own table. NR-778 records the call.
        base_price_ancient = {
            steel                 = 113,  -- 2 blooms (24.4) + charcoal, doubled
            ordnance              = 113,  -- the Smithy's route, same inputs
            construction_capacity = 6.6,    -- timber-frame method at planks 7.6
        },
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
