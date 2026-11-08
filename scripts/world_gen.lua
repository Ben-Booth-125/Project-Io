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

    -- BL-1200: an enclosed water body (any but a body's largest) of at least
    -- this many tiles is a SEA (coast + ocean: ports, wharves, no Wells);
    -- smaller ones are lakes (TILES.md § Water kinds). 75 is Ben's ruling
    -- (2026-10-22, BL-1397; it was 150, the measured gap): on the 16 curated
    -- seeds (tools/verify/lake_census.cpp) it keeps 293 lakes and makes 67
    -- seas. Integer in [1, 1000000], rejected (never clamped) otherwise.
    lake_size_cap = 75,

    kepler_market = {
        -- Starting per-resource base price on Kepler's seeded markets.
        base_price = {
            iron_ore             = 2.5,
            petroleum             = 3.5,
            water                 = 1.5,
            agricultural_produce  = 3.0,
            steel                 = 16.1,
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

            -- BL-340: the processing-chain roster. Margin widens up the tiers
            -- (Tier 3 promises "widest price divergence" per RESOURCES.md) -
            -- spacecraft_components sits ~123x iron ore, the value gradient the
            -- space-equipment premise rests on. Every value here is what the
            -- good's cheapest in-band route needs under the recipe margin anchor
            -- (PRODUCTION.md § The recipe margin anchor; tools/verify/recipe_margin
            -- is the check) - re-derive there, never nudge here.
            silicon                = 9.6,
            refined_copper         = 13.6,
            ree_alloy              = 25.6,
            machinery              = 61,
            alloys                 = 85,
            electronics            = 41.6,
            spacecraft_components  = 310,

            -- BL-368: the habitability tranche - welfare goods, not high-margin
            -- industrial products (RESOURCES.md § Habitability goods). Priced by
            -- the anchor like everything else: consumer_goods carries food_rations
            -- + steel + cloth-grade inputs, hence its size.
            clean_water            = 7.7,
            consumer_goods         = 61,
            medical_supplies       = 14.0,

            -- BL-457: the military terminal good. DERIVED, not picked: what the
            -- Fabricator's route (steel + machinery) needs under the anchor. The
            -- military value anchor (unit_upkeep, economy.lua; tools/verify/
            -- value_anchor) reads this number, so a move here moves the head wage.
            ordnance               = 155.8,

            -- Propellant has a base price (Ben, 2026-10-08; BL-1217;
            -- RESOURCES.md § What trades). Unpriced, every propellant plant
            -- forecast zero revenue and was idled the moment it stood. DERIVED
            -- like every Tier 3 price, on the route a body WITH AIR runs — the
            -- home-body markets this table prices (Ben, 2026-10-09: the routes
            -- follow the body's air, so the airless route is never the cheaper
            -- choice beside it): propellant_atmospheric, refined_fuel x2 = 31.20
            -- at base + 0.75 wage per batch = 31.95 marginal; k = 1 needs 63.90,
            -- and the table carries the same 0.1 over the exact need every
            -- anchored price does. propellant_electrolysis (water x3 +
            -- refined_fuel x1, 20.85 marginal) runs only on airless bodies and
            -- clears k_alt = 0 at it. tools/verify/recipe_margin is the check.
            propellant             = 64.0,

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

            -- BL-585/BL-586 (2026-08-24) - the wide ancient roster's first slice.
            -- Priced by the anchor off each good's only route; the old ~1.433x
            -- markup derivation is superseded (BL-744).
            ceramics               = 6.5,
            dressed_stone          = 6.1,
            planks                 = 7.6,
            tools                  = 90.6,

            -- BL-586 slice 2 (2026-08-24) — Tannery/Weaver/Shipwright. `hides`
            -- and `fibre` are raws, priced like the ambient/endemic bulk
            -- commons above (stone 1.0 / timber 1.5 / clay 1.2): `hides` a
            -- little dearer for being endemic (rarer, regional, like the
            -- unpriced tobacco/spices/coffee/furs would be); `fibre` at the
            -- plain ambient-crop tier, alongside clay/peat. `leather`/`cloth`/
            -- `rigging` priced by the anchor off their single routes.
            hides                  = 2.5,  -- endemic raw, dearer than the ambient bulk floor
            fibre                  = 1.3,  -- ambient crop, same tier as clay/peat
            leather                = 11.6,
            cloth                  = 6.8,
            rigging                = 38,

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

            -- RETIRED SLOT: construction capacity (BL-709 priced it; the system is
            -- retired, Ben 2026-10-27, BL-1438). Nothing produces or draws it;
            -- the price stays so an old save's leftover shelf stock still reads a
            -- finite price while it spoils away. Not a sizing anchor any more.
            construction_capacity  = 6.6,

            -- BL-1422 — the kit goods by kind (MILITARY.md § Land — equipment,
            -- refit and manpower). DERIVED, not picked: what each Arms Works
            -- route needs under the recipe margin anchor (recipes.lua, the five
            -- appended rows; tools/verify/recipe_margin is the check). Each is
            -- 2 x (inputs at base + 0.75 wage a batch) + the table's usual 0.1:
            -- small arms 2 x 47.35, armoured vehicles 2 x 178.95, artillery pieces
            -- 2 x 93.95, guided weapons 2 x 120.25, drones 2 x 84.85.
            small_arms             = 94.8,
            armoured_vehicles      = 358,
            artillery_pieces       = 188,
            guided_weapons         = 240.6,
            drones                 = 169.8,
            -- BL-1436, the sixth kit good, derived the same way: airframes
            -- 2 x (85 + 30.5 + 20.8 + 0.75) + 0.1.
            airframes              = 274.2,
            -- BL-1527, the seventh kit good, derived the same way: ship hulls
            -- 2 x (16.1 + 42.5 + 30.5 + 0.75) + 0.1.
            ship_hulls             = 179.8,
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

    -- BL-1268 — THE RETROFIT (docs/economy/TRADE.md § Trade in generation):
    -- after Industrialisation the history's trade record is spent on completed
    -- Planetary Marketplaces, on the markets whose trade the history carried.
    -- The record is the history's own (flow volume and treaty partner-years, per
    -- nation and per capital seat, 1200-1960); these rates turn it into trade
    -- points and the points into Marketplaces. PROPOSED BY MEASUREMENT, NOT
    -- FINAL: Ben approves them. Mirrored in world_gen_config.hpp's defaults.
    --
    -- MEASURED (tools/verify/trade_retrofit_probe.cpp, the 16 curated seeds,
    -- 2026-10-10). A market's record runs from nothing to ~5.6M flow-volume-
    -- years and ~160k partner-years. At 40/1000 the flow term leads (the top
    -- markets read ~120-220k from flow against ~50-160k from relations), so
    -- trade, not treaty count, decides where a Marketplace stands; relations
    -- still lift a well-bound seat. At 40,000 a Marketplace and a cap of 3 the
    -- 16 seeds place 164 (2-19 a seed, median ~11) on ~5 of ~28 markets each,
    -- held by 86 corporations (~5 a seed of ~125, ~1 a specialist). Rungs read
    -- (16-seed totals): price 30k -> 210, 50k -> 135, 80k -> 69; relations off
    -- (0/yr) at 50k -> 85. Markets whose record buys but where no corporation
    -- holds ground get none (the owner rule), and that is most of the gap
    -- between bought and placed.
    trade_retrofit = {
        flow_points_per_1000     = 40,    -- points per 1000 flow-volume-years, each end
        relation_points_per_year = 1,     -- points per partner-year of a mutual treaty
        points_per_marketplace   = 40000, -- points one Marketplace costs
        max_per_market           = 3,     -- the most one market's record may buy
    },

    -- BL-1473 — A MAJOR'S OPENING KIT (docs/generation/CORPORATION_GENERATION.md
    -- § A major's opening kit, Ben 2026-11-02): every major's ground is cleared of
    -- the walk's extraction, processing and civic buildings and laid with the
    -- active kit below, and its treasury set to the kit's (NATION_GENERATION.md
    -- § Pass 7). Minors keep the walk. "The kit is data": a configuration under
    -- test is an edit here. `active = "none"` (or no table) lays no kit.
    --
    -- Slots name raws and goods by their resource names, recipes by their
    -- scripts/recipes.lua `name`, civic buildings by type. An unknown name is an
    -- error. Extraction "flex" is the raw the kit's own processors are shortest
    -- of, decided after the fixed slots. A processing slot's `recipes` are its
    -- preference; with none feasible it bends to another recipe making the good.
    opening_kit = {
        active = "rich_core",
        configs = {
            standard = {
                treasury = 8000,
                -- The logistics slots beside the capital's source: a cap (Ben, 2026-11-03).
                logistics = { inland_logistics_hub = 2, railway_station = 1, port = 1 },
                -- The fixed army (Ben, 2026-11-02; MILITARY.md § Nation garrisons): eight units
                -- of 50 men, four at the capital post, the rest one each at the border posts in
                -- descending grudge order, wrapping; the land pool opens on 200 recruits.
                army = {
                    capital = { "infantry", "infantry", "artillery", "light_armour" },
                    border  = { "motorised", "infantry", "infantry", "infantry" },
                    men = 50, land_recruits = 200,
                },
                -- The fixed fleet (Ben, 2026-11-07; MILITARY.md § Sea): all four classes in
                -- fixed shares, raised on base kit, three to a port from the capital port out;
                -- the sea pool opens on 100 recruits.
                fleet = {
                    ships = { "transport", "destroyer", "destroyer", "capital", "submarine" },
                    per_port = 3, sea_recruits = 100,
                },
                extraction = {
                    "agricultural_produce", "agricultural_produce", "agricultural_produce",
                    "agricultural_produce", "agricultural_produce", "water", "coal",
                    "petroleum", "iron_ore", "copper_ore", "copper_ore", "copper_ore", "flex",
                },
                processing = {
                    { good = "food_rations",   recipes = { "food_rations" } },      -- Food Processor
                    { good = "steel",          recipes = { "steel" } },             -- Smelter
                    { good = "refined_copper", recipes = { "refined_copper" } },    -- Smelter
                    { good = "machinery",      recipes = { "machinery" } },         -- Fabricator
                    { good = "refined_fuel",   recipes = { "refined_fuel" } },      -- Refinery
                    { good = "power",          recipes = { "coal_power_plant", "oil_power_plant" } },
                },
                civic = { "planetary_marketplace", "research_institute", "schooling" },
            },
            -- The Rich comparison point (44 buildings with the logistics and
            -- military slots): Standard plus ordnance, silicon and a second food
            -- and steel line, with the extraction spread they need to be fed.
            -- Extraction counts are CALIBRATED, not sized at placement (Ben,
            -- 2026-11-03): the spread each processing slot needs to pass the
            -- spare-input rule on the quick tier (opening_parity seeds 0/28/46/12).
            rich_core = {
                treasury = 12000,
                logistics = { inland_logistics_hub = 2, railway_station = 1, port = 1 },
                -- The fixed army (Ben, 2026-11-02; MILITARY.md § Nation garrisons): eight units
                -- of 50 men, four at the capital post, the rest one each at the border posts in
                -- descending grudge order, wrapping; the land pool opens on 200 recruits.
                army = {
                    capital = { "infantry", "infantry", "artillery", "light_armour" },
                    border  = { "motorised", "infantry", "infantry", "infantry" },
                    men = 50, land_recruits = 200,
                },
                -- The fixed fleet (Ben, 2026-11-07; MILITARY.md § Sea): all four classes in
                -- fixed shares, raised on base kit, three to a port from the capital port out;
                -- the sea pool opens on 100 recruits.
                fleet = {
                    ships = { "transport", "destroyer", "destroyer", "capital", "submarine" },
                    per_port = 3, sea_recruits = 100,
                },
                extraction = {
                    "agricultural_produce", "agricultural_produce", "agricultural_produce",
                    "agricultural_produce", "agricultural_produce", "agricultural_produce",
                    "agricultural_produce", "agricultural_produce", "agricultural_produce",
                    "water", "coal", "petroleum", "iron_ore", "iron_ore", "copper_ore",
                    "copper_ore", "copper_ore", "silica", "silica", "silica", "silica",
                    "silica", "flex", "flex",
                },
                processing = {
                    { good = "food_rations",   recipes = { "food_rations" } },
                    { good = "steel",          recipes = { "steel" } },
                    { good = "refined_copper", recipes = { "refined_copper" } },
                    { good = "machinery",      recipes = { "machinery" } },
                    { good = "refined_fuel",   recipes = { "refined_fuel" } },
                    { good = "power",          recipes = { "coal_power_plant", "oil_power_plant" } },
                    { good = "ordnance",       recipes = { "ordnance" } },          -- Fabricator: steel + machinery
                    { good = "food_rations",   recipes = { "food_rations" } },
                    { good = "silicon",        recipes = { "silicon" } },           -- Refinery, from silica
                    { good = "steel",          recipes = { "steel" } },
                    -- BL-1551 (Ben, 2026-11-08): every class is raised and refilled with its arms.
                    { good = "small_arms",        recipes = { "small_arms" } },        -- Arms Works
                    { good = "armoured_vehicles", recipes = { "armoured_vehicles" } }, -- Arms Works
                    { good = "artillery_pieces",  recipes = { "artillery_pieces" } },  -- Arms Works
                },
                civic = {
                    "planetary_marketplace", "research_institute", "research_institute",
                    "schooling", "university",
                },
            },
        },
    },
}
