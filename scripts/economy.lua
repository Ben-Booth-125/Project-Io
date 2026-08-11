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
    -- fraction of a full run's input need that the corp pool must cover. A body
    -- WITH a market bypasses these entirely: the building runs a full batch,
    -- pool-first, auto-buying any shortfall (economy_system.cpp; PRODUCTION.md).
    -- The thresholds are the no-market fallback:
    --   pool coverage >= t_full          -> run full from the pool (self-sufficient)
    --   t_idle <= coverage < t_full      -> partial run, scaled to coverage
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
    -- BL-166 / BL-168: Hydroponics Bay and Fishing Wharf are the same generic
    -- processing_facility / extraction_site types as every other named building
    -- (Farm, Smelter, ...) — distinguished by recipe/target, not a new
    -- building_type — so they share the economics entries below rather than
    -- carrying their own. Placement (not economics) is what sets them apart
    -- (placement_rules.cpp): Hydroponics Bay only where the Farm's deposit was
    -- NOT seeded; Fishing Wharf only on a coastal tile.
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
        -- BL-325 S1: Military Base — the unit muster building. Produces nothing
        -- (base_rate 0, staffs at zero like the port/hub); its value is where
        -- units are raised (hire moves onto it in BL-325 S2). Deliberately NOT a
        -- supply anchor: military reach IS the economic reach field (ruling 3),
        -- so a base extends nothing. Dearer than a hub (a garrison installation,
        -- not a waypoint), cheaper than a launchpad.
        military_base = {
            base_rate   = 0.0,
            maintenance = 15.0,
            base_wage   = 10.0,
            build_cost  = 300.0,
            build_duration_ticks = 4.0,
            resource_costs = { steel = 35.0 },
        },
        -- BL-332: Research Institute — the dedicated tech-progress building.
        -- Same passive shape as military_base (base_rate 0, staffs at zero);
        -- its output is a flat per-tick credit to the owning corp's
        -- military.science accumulator (economy.military below), not a
        -- resource_type good. Priced like military_base — a garrison-grade
        -- installation, not a waypoint.
        research_institute = {
            base_rate   = 0.0,
            maintenance = 15.0,
            base_wage   = 10.0,
            build_cost  = 300.0,
            build_duration_ticks = 4.0,
            resource_costs = { steel = 35.0 },
        },
    },

    -- BL-332: capability-point accumulation rates. Flat per-tick credit to the
    -- owning corp's corporation_component.military_points / .science for every
    -- COMPLETED military_base / research_institute it owns — passive, no
    -- workforce dependency (both buildings staff at zero). First-cut authored
    -- constants; retune by playtest. No spend mechanism reads either yet
    -- (BL-087's constellation is the intended future sink).
    military = {
        military_points_per_base_tick       = 1.0,
        science_per_research_institute_tick = 1.0,
    },

    -- BL-350: the procurement/contract seam — "a build order placed with
    -- someone else", BL-095's shape with materials drawn against the
    -- SUPPLIER's market and the output delivered to the BUYER's pool.
    -- First-cut authored constants; retune by playtest.
    procurement = {
        deposit_fraction       = 0.25, -- debited at accept_quote; remainder paced across lead_time_ticks
        base_lead_ticks        = 2.0,
        reputation_floor       = -5.0, -- request_quote declines below this; 0 for an unseen pair
        reputation_on_complete = 1.0,
        reputation_on_cancel   = -2.0,
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

    -- BL-368 (2026-08-11): the real per-centre population demand basket,
    -- generalising the BL-190 flat agricultural_produce-only stub. Same
    -- price-elastic shape as the substrate model above, but population is a
    -- pure CONSUMER — no supply term. DEMAND = pcc.scale × demand_scale ×
    -- basket[r] × elasticity(price). Read by inject_population_demand
    -- (market_clearing.cpp), called from clear_markets after its demand reset.
    population_demand = {
        -- Per-scale-point demand weight per resource. Unlisted resources get 0.
        demand_basket = {
            food_rations          = 0.60, -- population primary (replaces the old flat agri stub)
            agricultural_produce  = 0.20, -- direct consumption, lighter than the processed staple
            water                 = 0.30, -- life support
            clean_water           = 0.35, -- BL-368 habitability tranche
            consumer_goods        = 0.25, -- BL-368 habitability tranche
            medical_supplies      = 0.15, -- BL-368 habitability tranche
        },
        demand_elasticity = 0.80, -- exponent on (base_price / price); matches the substrate shape
        elasticity_min    = 0.30,
        elasticity_max    = 2.50,
        demand_scale      = 1.00,
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

        -- BL-323 S2 — LOGISTICAL MAX BUILDING RANGE.
        -- The furthest a new building may sit from its nearest supply anchor (a
        -- city, a port, or an inland logistics hub), in the same weighted units
        -- the convoy A* pays: per-tile landform cost (plains 1.0 → mountain 2.0),
        -- discounted by road tier, averaged across each edge.
        --
        -- So ~24 means roughly two dozen tiles of easy roaded plains, or barely a
        -- dozen of trackless mountain. Remote ground is not forbidden — it is
        -- forbidden UNTIL you pay for a hub or a road to reach it, which is the
        -- decision the rule exists to create.
        --
        -- Negative disables the rule entirely (the pre-BL-323 behaviour).
        max_logistics_reach = 24.0,

        -- BL-323 S3 — BUILD TIME DEPENDS ON THE SITE. Three multipliers on the
        -- base build_duration_ticks, applied once at placement: landform reuses
        -- logistics.hpp's own cost function (no separate table to author here).
        --   site_time_reach_scale     — extra time (as a fraction of base) at the
        --     furthest placeable reach; 1.0 means a site right at the
        --     max_logistics_reach budget takes 2x base, one at an anchor takes 1x.
        --   site_time_stack_discount  — per-existing-building discount on a tile
        --     that already carries the same building type; 0.15 means the second
        --     site there takes 0.85x, the third 0.70x, ...
        --   site_time_stack_min       — floor on the stack discount.
        site_time_reach_scale    = 1.0,
        site_time_stack_discount = 0.15,
        site_time_stack_min      = 0.5,
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
