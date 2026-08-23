-- Project Io — economy.lua
-- Per-building-type economic constants and the global production thresholds for
-- the Layer 3 economy. Loaded at startup into the C++ recipe registry
-- (see src/world/recipe_registry.{hpp,cpp}).
--
-- Magnitudes are legible round defaults, iterated by playtest, not derived
-- (PRODUCTION.md / RESOURCES.md). They are deliberately easy to read in the
-- economy panel: a staffed mine moves ore in tens per tick, refined goods sell
-- for several times their inputs.

economy = {
    -- Two-threshold partial-run model for processing buildings, expressed as a
    -- fraction of a full run's input need that the corp pool must cover:
    --   pool coverage >= t_full          -> run full from the pool (self-sufficient)
    --   t_idle <= coverage < t_full      -> run full, auto-buying the shortfall
    --   coverage < t_idle                -> idle
    thresholds = {
        t_full = 1.0,
        t_idle = 0.2,
    },

    -- Per building_type constants.
    --   base_rate   — extraction: output units per tick at richness 1, workforce 1.
    --                 processing: recipe batches per tick at workforce 1.
    --   maintenance — flat per-tick upkeep charged to the owning corp.
    --   base_wage   — wage per unit workforce per tick (wage = workforce * base_wage).
    --   build_cost  — one-off construction cost (used by the Layer 4 build UI).
    --   build_duration_ticks — economy ticks the building spends under
    --                 construction before it produces anything (0 = instant).
    -- MATERIAL COSTS: re-enabled 2026-07-06 (BL-095-lite). BL-044 originally gated
    -- these against the corp's own pool on the body (construction.cpp), which
    -- deadlocked a fresh start — a corp's steel surplus auto-sells each tick
    -- (market_clearing.cpp), so the pool never held enough to build. Construction
    -- now buys the material cost from the tile's local MARKET at its prevailing
    -- price, folded into the credit debit — the resource line is what the cash
    -- buys, not a pool gate. A depletable market stockpile (the fuller BL-095
    -- gate/charge model) is still open; this is the quick, playable interim.
    -- BUILD DURATION added 2026-07-06 (playtest patch): a headless spree check
    -- found the player could place a dozen-plus buildings in one instant burst
    -- with zero ticks elapsed — construction had no time cost at all, letting
    -- the small chartered-company layer outpace the automated population/
    -- substrate economy it's supposed to sit on top of (GENERATION_STRATEGY.md).
    buildings = {
        extraction_site = {
            base_rate   = 20.0,
            maintenance = 5.0,
            base_wage   = 8.0,
            build_cost  = 100.0,
            build_duration_ticks = 2.0,
            resource_costs = { steel = 20.0 },
        },
        processing_facility = {
            base_rate   = 8.0,
            maintenance = 10.0,
            base_wage   = 12.0,
            build_cost  = 200.0,
            build_duration_ticks = 3.0,
            resource_costs = { steel = 25.0 },
        },
        port = {
            base_rate   = 0.0,
            maintenance = 8.0,
            base_wage   = 6.0,
            build_cost  = 150.0,
            build_duration_ticks = 2.0,
            resource_costs = { steel = 20.0 },
        },
        launchpad = {
            base_rate   = 0.0,
            maintenance = 20.0,
            base_wage   = 15.0,
            build_cost  = 500.0,
            build_duration_ticks = 6.0,
            resource_costs = { steel = 50.0, refined_fuel = 20.0 },
        },
        -- BL-149: Inland Logistics Hub — a land-mode convoy consolidator. Produces
        -- nothing (base_rate 0, like the port); its value is the logistics-node discount
        -- its tile confers on intra-body hauls that pass through it (see logistics.node_discount).
        -- Cheaper than a launchpad but dearer than a port, reflecting its network reach.
        inland_logistics_hub = {
            base_rate   = 0.0,
            maintenance = 12.0,
            base_wage   = 8.0,
            build_cost  = 250.0,
            build_duration_ticks = 3.0,
            resource_costs = { steel = 30.0 },
        },
    },

    -- Player-placeable roads, now a three-tier ladder (BL-172; BL-147 shipped a single tier). A
    -- road is a per-tile mutation (raises tile.road_level, lowering its A* traversal cost), not a
    -- building — no maintenance/recipe, just an up-front placement cost. The tiers, by road_level:
    --   track (1)   — minor / low-throughput; traversal x0.67. The cheap reach investment.
    --   road (2)    — regular; traversal x0.50.
    --   highway (3) — high-throughput backbone; traversal x0.40 (diminishing returns).
    -- ("Throughput" is cost-discount, not a capacity cap — per-node capacity is out of prototype
    -- scope, SUPPLY.md.) Generation (BL-146/BL-172) lays Road/Highway between cities; the player
    -- places any tier and may upgrade in place. Cost mirrors a building's shape (credits +
    -- materials from the local market) but is far cheaper — a road tile is a small reach step.
    roads = {
        track   = { build_cost = 25.0, resource_costs = { steel = 3.0  } },
        road    = { build_cost = 45.0, resource_costs = { steel = 6.0  } },
        highway = { build_cost = 90.0, resource_costs = { steel = 14.0 } },
    },

    -- BL-078: the elastic nation-substrate model. The saturated background
    -- economy (GENERATION_STRATEGY.md) is given two precise faces at tick time.
    --   DEMAND  = population_weight × demand_basket[r] × elasticity(price)
    --     a tiered per-capita basket (food primary, industry inputs lighter) that
    --     is price-elastic — a lower price raises consumed quantity along a
    --     down-sloping curve, so price *discovers* instead of clamping.
    --   SUPPLY  = min(capacity[r] × capacity_scale, demand × clearing_fraction)
    --     an abstract nation production capacity (deposit-derived at generation)
    --     that tracks demand and clears it only to `clearing_fraction`, leaving a
    --     live margin — the saturation cushion where capacity is ample, a wide
    --     opportunity gap where the nation lacks the deposit (supply pegs short,
    --     price rises: the gap the player fills).
    -- All faces are deterministic (a curve, not RNG). Magnitudes are legible
    -- defaults, retuned by playtest once real price ranges are visible.
    substrate = {
        -- Per-capita aggregate demand weight per resource (population + background
        -- industry pull). Unlisted resources get 0 (no substrate demand).
        demand_basket = {
            food_rations         = 0.70,  -- population primary
            agricultural_produce = 0.55,  -- food processing + direct
            steel                = 0.45,  -- construction / industry
            water                = 0.40,  -- life support + industry
            refined_fuel         = 0.40,  -- energy
            iron_ore             = 0.35,  -- background smelting input
            petroleum            = 0.30,  -- background refining input
        },
        capacity_scale     = 2.0,  -- deposit-derived capacity → supply ceiling scale
        clearing_fraction  = 0.90, -- abstract supply clears this fraction of demand (leaves the cushion/margin)
        demand_elasticity  = 0.80, -- exponent on (base_price / price); higher = more price-responsive
        elasticity_min     = 0.30, -- clamp on the elasticity factor (dear goods still consumed a little)
        elasticity_max     = 2.50, -- clamp on the elasticity factor (cheap goods consumption saturates)
        demand_scale       = 1.00, -- global population → demand scale
        growth_met_threshold = 0.50, -- basket met-supply ratio a centre needs to grow (BL-078 growth)
    },

    -- BL-095: construction is gated on local market supply. Each tick a build
    -- draws 1/build_duration_ticks of its materials as real market DEMAND (bids
    -- the price up, competes with population and other builds) and progresses at a
    -- rate set by how much of that need the market can supply:
    --   market supplies the full need   -> full speed (base build_duration_ticks)
    --   market supplies part            -> stretched, proportionally slower
    --   supplies less than 1/max_stretch -> paused until supply recovers
    -- so a starved build takes up to max_stretch × its base duration, then stalls.
    construction = {
        max_stretch = 10.0, -- longest a starved build stretches to (×base duration); below 1/max_stretch it pauses.
    },
}

-- Logistics cost constants (BL-045 / BL-039 supply layer).
-- base_cost_per_unit_distance is multiplied by convoy distance × cargo_qty to
-- give the per-dispatch budget debit. Ordered land < sea < air < space so that
-- space transport is the most expensive, reflecting the energy and infrastructure
-- cost of leaving a gravity well.
logistics = {
    base_cost_per_unit_distance = {
        land  = 0.02,
        sea   = 0.05,
        air   = 0.15,
        space = 1.00,
    },

    -- BL-148 / BL-149: logistics-node discount. An intra-body convoy's haul cost is discounted
    -- for each logistics node its A* path crosses — population centres (BL-148, cities are free
    -- hubs) and player-built Inland Logistics Hubs (BL-149). A city discounts by its scale (tier
    -- 1–5); a hub by a flat amount. The total is capped so a route can never be free. Together
    -- with the generated road lattice (BL-146) this makes the world's cities a cheap network the
    -- specialist player plugs its remote extraction into.
    node_discount = {
        city_per_scale = 0.04, -- discount fraction per population-centre scale point on the path.
        hub            = 0.12, -- flat discount fraction per Inland Logistics Hub tile on the path.
        cap            = 0.50, -- ceiling on the summed node discount (fraction of the haul cost).
    },
}

print("[Lua] economy.lua loaded")
