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
    -- MATERIAL COSTS DISABLED (2026-07-06) — construction is credits-only until BL-095.
    -- Why: BL-044 gave every building a steel material cost, checked against the
    -- corp's own pool on the body (construction.cpp). But steel produced by a corp's
    -- smelter is auto-sold as surplus each tick (market_clearing.cpp — only processor
    -- inputs are retained, and construction is not a processor input), so the pool
    -- steel never rises above ~0 and NO building is ever placeable from a fresh start
    -- — a hard bootstrapping deadlock (you can't place your first building). The
    -- proper fix is BL-095 (construction draws steel from the local MARKET stock,
    -- paid in credits — the player has cash and the market has steel). Until BL-095
    -- lands, `resource_costs = {}` restores the playable credits-only loop.
    buildings = {
        extraction_site = {
            base_rate   = 20.0,
            maintenance = 5.0,
            base_wage   = 8.0,
            build_cost  = 100.0,
            -- Resource material cost (BL-044) — re-enable via BL-095 (market-sourced).
            resource_costs = { },
        },
        processing_facility = {
            base_rate   = 8.0,
            maintenance = 10.0,
            base_wage   = 12.0,
            build_cost  = 200.0,
            resource_costs = { },
        },
        port = {
            base_rate   = 0.0,
            maintenance = 8.0,
            base_wage   = 6.0,
            build_cost  = 150.0,
            resource_costs = { },
        },
        launchpad = {
            base_rate   = 0.0,
            maintenance = 20.0,
            base_wage   = 15.0,
            build_cost  = 500.0,
            resource_costs = { },
        },
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
}

print("[Lua] economy.lua loaded")
