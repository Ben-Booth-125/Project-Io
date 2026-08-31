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
            -- BL-436 calibration (2026-08-16). base_rate carries the SCALE of the
            -- economy; richness_reference below carries the RATIO to processing.
            -- Normalising richness fixed a 133:1 extraction:processing ratio but
            -- also cut absolute income ~53x, and the cost side (maintenance 5,
            -- wage 8, build_cost 100, starting capital) was tuned against the old
            -- inflated income — so every AI corp went bankrupt. Both producer
            -- base_rates scale x10 TOGETHER, restoring a workable scale without
            -- reopening the ratio: 200:80 is the same 2.5:1 as 20:8.
            base_rate   = 20.0,
            maintenance = 5.0,
            base_wage   = 8.0,
            build_cost  = 100.0,
            build_duration_ticks = 2.0,
            resource_costs = { steel = 20.0 },

            -- BL-436: how a deposit's richness becomes an extraction RATE.
            --
            -- base_rate has always documented itself as "units/tick AT RICHNESS 1",
            -- but tile deposits are generated as QUANTITIES — measured mean 53.3,
            -- max 72,321 — and were multiplied in raw. A mine therefore ran at
            -- ~1067 units/tick against a processing facility's flat 8: a 133:1
            -- structural gap that no price or wage could offset, and the reason
            -- refining could not pay (tier_margin R6).
            --
            -- `reference` is the MEASURED mean richness, so a typical deposit lands
            -- at ~1.0 and a mine runs at roughly its authored base_rate. The band
            -- keeps a spectacular tile worth ~2x a poor one rather than ~200x.
            --
            -- The design statement: richness decides how LONG a deposit lasts, not
            -- how fast you can pull it out. Reserve still scales with raw richness
            -- (resource_remaining, seeded at generation), so a rich tile is still
            -- worth far more over its life — it simply cannot also run a thousand
            -- times faster. Set richness_reference = 0 to restore raw behaviour.
            -- ENABLED 2026-08-17 (BL-436, Ben's call), after sitting at 0 since
            -- 2026-08-16.
            --
            -- 24.9 is the measured MEDIAN deposit richness, not the mean. The
            -- promise above — "a typical tile lands at ~1.0" — is what this value
            -- has to deliver, and the mean does not deliver it: the distribution
            -- runs to 72,321, so its mean (53.34) sits at the 78th percentile.
            -- Referenced against the mean, the median tile ran at 0.47 and 18% of
            -- deposits clamped flat at the richness_min floor, halving raw supply
            -- across the whole map. Measured: tier_margin R6b prints the
            -- percentiles (p10 10.58 / p25 15.76 / MEDIAN 24.92 / p75 45.09 /
            -- p90 98.34 / p99 360.91) so this number can be re-derived rather
            -- than trusted.
            --
            -- The reason it was off is recorded rather than deleted, because the
            -- reason turned out to be wrong. The old note read: "every AI corp goes
            -- bankrupt with it on ... a scale sweep showed raising extraction income
            -- makes the collapse WORSE, because corps spend it on processors that
            -- lose more per tick at higher scale". That mechanism could not happen.
            -- The AI had no processing_facility build candidate at all until BL-439
            -- (2026-08-17), so no corp could spend income on a processor at any
            -- scale, and the sweep's causal chain had no link in the middle
            -- (NR-265/NR-266). Whatever it measured, it was not that.
            --
            -- Set to 0 to restore raw pre-BL-436 behaviour.
            richness_reference = 24.9,
            richness_min       = 0.25,
            richness_max       = 2.0,

            -- BL-590 (2026-08-24): per-named-building material overrides, keyed
            -- by TARGET resource — an ancient extraction site is a wooden dig
            -- shed or a quarry frame, not a steel-reinforced structure, so the
            -- five ancient-only targets override the type's steel=20 default.
            -- Absent target = the default above; every space-sourced/industrial
            -- target (iron_ore, coal, ...) is UNCHANGED, deliberately — the
            -- anachronism BL-590 exists to fix is the ANCIENT arc's, and an
            -- industrial mine genuinely does need steel reinforcement.
            material_overrides = {
                stone  = { timber = 12.0 },              -- a timber-framed quarry
                timber = { stone  = 6.0  },               -- an axe-and-saw camp needs a whetstone, not a shed
                sand   = { timber = 10.0 },
                clay   = { timber = 10.0 },
                peat   = { timber = 8.0  },
                -- BL-586 slice 2 (2026-08-24): hides and fibre join the same
                -- ancient-arc override, a fenced pasture/hunting camp and a
                -- marsh/grass field camp respectively, both timber-framed
                -- like the rest of this table rather than the type's steel
                -- default.
                hides  = { timber = 10.0 },
                fibre  = { timber = 8.0  },
            },
        },
        processing_facility = {
            base_rate   = 8.0,
            maintenance = 10.0,
            base_wage   = 12.0,
            build_cost  = 200.0,
            build_duration_ticks = 3.0,
            resource_costs = { steel = 25.0 },

            -- BL-590 (2026-08-24): keyed by RECIPE — every industrial recipe is
            -- unchanged (still steel=25.0, the default above); the ancient
            -- roster's thirteen recipes each get a basket from goods the
            -- ancient arc actually makes. Grouped by what the building IS:
            material_overrides = {
                -- Fuel Production — a simple kiln, mostly timber.
                charcoal            = { timber = 10.0 },              -- Charcoal Burner
                peat_charcoal       = { timber = 6.0, stone = 3.0 },   -- Peat Kiln
                charcoal_from_kiln  = { timber = 8.0, stone = 4.0 },   -- Coking Kiln (BL-587)

                -- Metal Foundry — a masonry hearth, so stone-heavy. The Smithy's
                -- two recipes (steel/ordnance) share the SAME physical building
                -- (recipes.lua's own comment), so they carry the same basket.
                iron_blooms          = { stone = 15.0, timber = 5.0 }, -- Bloomery
                steel_from_blooms    = { stone = 15.0, timber = 5.0 }, -- Smithy (steel)
                ordnance_from_blooms = { stone = 15.0, timber = 5.0 }, -- Smithy (ordnance)
                toolmaker            = { stone = 12.0, timber = 10.0 }, -- Toolmaker (BL-586, deepest chain)

                -- Artisan Goods / Construction Materials — a clay-fired or
                -- timber workshop, cheap and light.
                trade_goods    = { clay = 8.0, timber = 4.0 },  -- Potter & Weaver
                glass          = { sand = 8.0, timber = 4.0 },  -- Glassworks
                ceramics_kiln  = { clay = 8.0, timber = 4.0 },  -- Potter's Kiln (BL-586)
                stonemason     = { timber = 8.0 },              -- Stonemason (BL-586)
                sawmill        = { stone = 6.0, timber = 4.0 }, -- Sawmill (BL-586) — cannot cost only its own output
                tannery        = { timber = 8.0, stone = 4.0 }, -- Tannery (BL-586 slice 2) — a timber-framed shed with stone-lined vats
                weaver         = { timber = 10.0 },             -- Weaver (BL-586 slice 2) — a simple loom shed

                -- Advanced Fabrication — the Shipwright is the roster's
                -- heaviest ancient facility (a slipway and yard), so it
                -- carries the biggest basket in this table even before
                -- BL-590's steel default would have.
                shipwright     = { timber = 14.0, stone = 6.0 }, -- Shipwright (BL-586 slice 2)

                -- Food Processing — a millstone needs stone.
                food_rations_milled = { stone = 10.0, timber = 4.0 }, -- Miller
            },
        },
        port = {
            base_rate   = 0.0,
            maintenance = 8.0,
            base_wage   = 6.0,
            build_cost  = 150.0,
            build_duration_ticks = 2.0,
            resource_costs = { steel = 20.0 },
        },
        -- BL-433: the one building type that is unambiguously not ancient. Tagged
        -- `industrial` so a campaign opened at 0 CE never offers it — the roster is
        -- what the Build door reads, and an anachronism there reads as a bug in the
        -- product rather than as a parked feature. Every other type below is shared
        -- by both arcs and stays untagged (= "any").
        launchpad = {
            era         = "industrial",
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
        -- BL-615: Schooling — the education building any settlement can host.
        -- Same passive shape as research_institute (base_rate 0, staffs at
        -- zero); its qualification-raising effect is a separate seam
        -- (POPULATION.md § Qualification), not authored here. Priced at the
        -- port's grade — a civic building, not a garrison installation — and
        -- costed in timber/stone so the SAME basket is obtainable in both era
        -- bands (a steel line here would be BL-590's ancient-arc anachronism
        -- over again). PLACEMENT is what defines it: requires_centre, any
        -- stratum (POPULATION.md § Strata gate buildings).
        schooling = {
            base_rate   = 0.0,
            maintenance = 8.0,
            base_wage   = 6.0,
            build_cost  = 150.0,
            build_duration_ticks = 3.0,
            resource_costs = { timber = 12.0, stone = 8.0 },
            requires_centre = true,
        },
        -- BL-615: University — the schooling building's City-tier sibling:
        -- "you can't build a university in a town" (Ben, 2026-08-25).
        -- min_centre_scale 4 = City on the Outpost(1)..Metropolis(5) ladder.
        -- Dearer than a research_institute (a campus, not one institute),
        -- cheaper than a launchpad; stone-heavy for the same both-bands
        -- reason as the schooling basket.
        university = {
            base_rate   = 0.0,
            maintenance = 18.0,
            base_wage   = 12.0,
            build_cost  = 400.0,
            build_duration_ticks = 5.0,
            resource_costs = { stone = 30.0, timber = 12.0 },
            requires_centre  = true,
            min_centre_scale = 4,
        },
    },

    -- BL-332: research accumulation rate. Flat per-tick credit to the owning
    -- corp's corporation_component.science for every COMPLETED
    -- research_institute it owns — passive, no workforce dependency (the
    -- building staffs at zero). First-cut authored constant; retune by playtest.
    --
    -- BL-641: BUILDING UPKEEP IN GOODS — the Industry demand channel.
    --
    -- A unit pays upkeep in credits AND a goods vector (economy.military.
    -- unit_upkeep below); a building paid credits only. That asymmetry was an
    -- omission, not a design, and it is the largest single reason the goods
    -- roster has more producers than consumers: nothing in the world consumed
    -- on a scale that GROWS with the world. Closing it makes every firm a
    -- consumer, so the sink scales with how much industry exists rather than
    -- with an authored weight somebody has to maintain (MARKETS.md § Demand
    -- channels property 1; the design is FINANCE.md § Upkeep is credits AND
    -- goods — for buildings too).
    --
    -- THE SHAPE IS unit_upkeep's, on the other kind of asset. A per-type basket
    -- is drawn from the owner's pool on the building's own body in ascending
    -- building id — the order is load-bearing, because two buildings of one
    -- corp on one body draw the same stock and the order decides which goes
    -- short — and an unmet draw WEAKENS the building by the same subtraction an
    -- out-of-supply unit takes. It never destroys, idles or decommissions it: a
    -- factory short of its tools runs badly, it does not vanish.
    --
    -- ERA-BANDED, because MARKETS.md property 2 says demand is. An ancient
    -- workshop runs on tools and planks; an industrial one on machinery and
    -- electronics. A basket under `any` applies in every campaign, one under a
    -- band only in that band, and a building's draw is the SUM of the baskets
    -- that apply — so a line common to both arcs is authored once.
    --
    -- A ZERO ENTRY IS SKIPPED EXACTLY AS AN ABSENT ONE (the BL-454 precedent).
    -- The shape landed with every rate at 0.0 and was proved bit-identical
    -- against econ_harness / econ_bankruptcy / econ_stability before a single
    -- number was authored; turning the sink on is therefore a DATA change and
    -- never a code change, and setting this whole table back to zero restores
    -- the pre-BL-641 economy exactly.
    --
    -- WHO DRAWS. Only a COMPLETED, non-decommissioned building. Under
    -- construction it is already bidding for materials through the Construction
    -- channel and charging operating goods too would count one building twice;
    -- decommissioned it is not operating at all.
    --
    -- MAGNITUDE, and it is derived rather than picked. The anchor is the
    -- building's OWN credit maintenance, which is the one figure in this file
    -- already denominated in "what it costs to keep this building standing for
    -- a tick". Target: the goods half is worth ROUGHLY HALF the credit half at
    -- authored base_price (world_gen.lua), so upkeep-in-goods is a real second
    -- cost without doubling the cost of owning anything. That is deliberately
    -- gentler than the unit vector's own 2x equipment:wage ratio — a regiment
    -- is mostly its equipment, a factory is mostly its wage bill — and it keeps
    -- BL-635's lesson in view: an upkeep line derived without checking it
    -- against what the asset earns is how upkeep came to be 90% of a spawning
    -- corp's outgoings.
    --
    -- Authored base_price, from scripts/world_gen.lua: tools 25.5, planks 4.3,
    -- machinery 22.0, electronics 29.0.
    --
    --   extraction_site      maintenance 5.0  -> target ~2.5 of goods
    --     ancient : tools  0.07 x 25.5 = 1.785
    --               planks 0.15 x  4.3 = 0.645   total 2.43
    --       A dig shed wears out picks and props. Both are ancient-roster
    --       terminal goods with a producer (Toolmaker, Sawmill) and, before
    --       this, no consumer at all — tools and planks are literally two of
    --       the seven goods the 0 CE census reports as produced with no sink.
    --     industrial : machinery 0.11 x 22.0 = 2.42
    --       A mine is machinery, and one line reads better than three.
    --
    --   processing_facility  maintenance 10.0 -> target ~5.0 of goods
    --     ancient : tools  0.14 x 25.5 = 3.57
    --               planks 0.30 x  4.3 = 1.29    total 4.86
    --       EXACTLY twice the extraction site's basket, as its maintenance is
    --       exactly twice — one ratio, not two independently-picked numbers.
    --     industrial : machinery   0.15 x 22.0 = 3.30
    --                  electronics 0.06 x 29.0 = 1.74   total 5.04
    --
    --   port / inland_logistics_hub / military_base / research_institute /
    --   schooling / university / launchpad: NOT AUTHORED, deliberately, and it
    --   is a scoping call rather than an oversight. Roads, ports and hubs kept
    --   standing are the INFRASTRUCTURE channel (BL-643), a different row of
    --   MARKETS.md's table with its own owner; a garrison's consumption is the
    --   Conflict channel's (BL-646); a research institute's is Research's
    --   (BL-645). Authoring them here would quietly claim three channels this
    --   item does not own. Extraction and processing ARE Industry — they are
    --   what "every firm" means — and they are 100% of the buildings any corp
    --   in the shipped world actually operates.
    --
    -- SUPPLY CURVE. The same numbers unit_upkeep uses, deliberately: one rule,
    -- one curve, two kinds of asset. decay 50 (5%/tick) means a fully-supplied
    -- building left unsupplied reaches zero output in 20 ticks (5 years at one
    -- tick/quarter) — long enough that one bad quarter is survivable, short
    -- enough that sustained neglect genuinely hollows a firm out. recovery 100
    -- is 2x the decay, so fixing a supply failure repairs faster than the
    -- failure did damage.
    --
    -- ===================================================================
    -- THE RATES SHIP AT ZERO. This is NOT the derivation above failing to
    -- be finished — it is what turning it on MEASURED, and the measurement
    -- says the sink cannot be switched on until its supply side exists.
    -- The derived figures are kept in the trailing comments so switching it
    -- on is four numbers, exactly as BL-635 kept unit_upkeep's pre-rescale
    -- values rather than rewriting them away.
    --
    -- WHAT WAS MEASURED. tools/verify/demand_census.exe --fast --band ancient,
    -- against docs/development/baseline_census_2026-08-26.txt:
    --
    --                        RATES OFF (baseline)   RATES ON (derived above)
    --   buildings                  584                    576
    --   ... complete & operating   227                     19
    --   ... under construction       0                     47
    --   units / heads         326 / 7690             309 / 6840
    --
    -- Turning the sink on collapsed the operating firm count by 92% over an
    -- 80-tick warm start. That is not a magnitude to be halved: HALVING IT
    -- ONLY DELAYS IT, because the cause is structural.
    --
    -- THE CAUSE, and it is the finding worth keeping. NOTHING IN THE SHIPPED
    -- ANCIENT WORLD PRODUCES TOOLS OR PLANKS — the census reports `produced`
    -- 0.0 for both, before and after. So every draw goes unmet, every tick;
    -- the supply factor decays 50/tick to zero inside 20 ticks; output goes
    -- to zero with it; the firm is then loss-making and BL-079's reflex tier
    -- idles it. The shortfall rule worked exactly as designed — it was fed a
    -- good nobody makes.
    --
    -- AND WHY NOBODY MAKES THEM WAS THE SAME LOOP, from the other end. The
    -- draw was a POOL draw (FINANCE.md's own shape, copied from
    -- run_unit_upkeep) and never reached `market_component::demand`. So
    -- wanting tools did not RAISE THE PRICE of tools, no rival ever scored
    -- building a Toolmaker, and the supply that would meet the draw was never
    -- induced. Tools and planks sat on the census's "produced in-band, NO
    -- market sink" list for exactly that reason.
    --
    -- BL-654 CLOSED THAT HALF (Ben's call, 2026-08-26, as the note above said
    -- it had to be). The draw is no longer pool-only: the pool is drawn first
    -- and the SHORTFALL IS BID on the local market and paid for, up to
    -- price_band.reservation_mult x base_price, above which the building
    -- declines to buy and the shortfall rule weakens it instead. One rule for
    -- every goods draw — run_unit_upkeep takes the identical path, not a
    -- second one. Wanting tools now prices tools.
    --
    -- THE RATES BELOW STILL SHIP AT ZERO, deliberately, and turning them on is
    -- a separate data change with its own measurement. BL-654 closed the
    -- PRICING half of the loop; the SUPPLY half — an ancient band that
    -- actually makes tools — has to follow the price signal before the draw
    -- can be met, and that takes ticks the harness has to be allowed to run.
    -- The shape is inert until then, with its ordering, its shortfall rule
    -- and its era bands all built and verified (tools/verify/building_upkeep).
    -- ===================================================================
    building_upkeep = {
        supply_decay_permille    = 50,   -- the ONE subtraction, per tick, on an unmet draw
        supply_recovery_permille = 100,  -- regained per tick while the draw is met

        goods = {
            extraction_site = {
                ancient    = { tools = 0.0, planks = 0.0 },      -- derived 0.07 / 0.15
                industrial = { machinery = 0.0 },                -- derived 0.11
            },
            processing_facility = {
                ancient    = { tools = 0.0, planks = 0.0 },      -- derived 0.14 / 0.30
                industrial = { machinery = 0.0, electronics = 0.0 }, -- derived 0.15 / 0.06
            },
        },
    },

    -- BL-455 (2026-08-17) removed `military_points_per_base_tick` from here with
    -- the field it fed: it credited an accumulator that nothing in src/ ever
    -- read. `science` kept its rate because it now HAS a reader —
    -- condition_subject::science, so a tech gate or a law can require a research
    -- level (reached, not spent; see condition_set.hpp).
    military = {
        science_per_research_institute_tick = 1.0,

        -- BL-394: hire_unit's credit cost, debited from the corp's balance by
        -- apply_corp_command: cost = hire_base_cost + hire_cost_per_power *
        -- the roster row's power_mod. The base is a FLOOR — the cheapest
        -- roster rows carry an all-zero resource gate, so without it a seam
        -- player could raise an unlimited free army (found in play,
        -- 2026-08-13). Levy Spear (power 0) costs 40; Rifle Regiment
        -- (power 380) costs 230. First-cut authored constants; retune by
        -- playtest. Standing-force UPKEEP is deliberately absent — flagged to
        -- BL-377's contract loop, not decided here.
        hire_base_cost      = 40.0,
        hire_cost_per_power = 0.5,

        -- BL-454: standing-force UPKEEP — what it costs to KEEP a unit, as
        -- against hire_* above, which is what it costs to raise one. Before
        -- this, `w.units` appeared in the economy, budget and construction
        -- systems exactly zero times: a regiment was debited once and was then
        -- free forever, while every building beside it paid maintenance and
        -- wages every tick.
        --
        -- Upkeep is credits AND military goods (Ben, 2026-08-17), so the cost
        -- is a VECTOR: a credit wage plus a set of {resource, qty} draws
        -- against the corp's pool on the unit's own body. When the goods do not
        -- arrive the unit WEAKENS rather than vanishing — one decay rule with
        -- two triggers (out of reach, or the draw went unmet), which is the
        -- same subtraction for the same reason.
        --
        -- BL-603 (UPKEEP_ZEROS, 2026-08-24): turned ON, first cut. NR-600 flags
        -- these numbers for Ben's calibration by playtest — this is a reasoned
        -- first cut against the BL-543 value anchor, not a final ruling.
        --
        -- BL-635 (spawn solvency, 2026-08-26): THE WHOLE VECTOR RESCALED BY
        -- 0.025. The derivation below is kept verbatim because it is still the
        -- shape of the thing and it is still internally consistent — what it
        -- got wrong is a UNIT, and reading it is how the unit error is seen.
        --
        -- WHAT WAS MEASURED. tools/verify/spawn_solvency.cpp, 8 seeds of the
        -- shipped 0 CE spawn, 80-tick warm start, per-flow attribution read off
        -- BL-626's filed quarterly returns: the seated corporation's operating
        -- outgoings were 667.5 cr/qtr against 21.2 cr/qtr of gross income, and
        -- UPKEEP WAS 600.0 OF THE 667.5 — 89.9% of everything the corp paid,
        -- and 28x everything it earned. Every swept seed was underwater by
        -- ~120,000 credits after the warm start.
        --
        -- THE UNIT ERROR, which is what makes this a defect rather than a
        -- taste. The derivation's closing claim is that "keeping a standing
        -- unit for a year costs meaningfully less than raising a fresh one",
        -- and it demonstrates that by comparing 24 (annualised
        -- credits_per_head) against 40 (hire_base_cost). Those are not the same
        -- quantity. credits_per_head is PER HEAD; hire_base_cost is PER UNIT,
        -- and a unit is `hire_batch_manpower` = 50 heads. In consistent units
        -- the old rate said a Levy Spear costs 40 credits to raise and
        -- 50 x 6.0 x 4 = 1,200 credits a year to keep — thirty times its own
        -- hire price, and sixty times an extraction site's entire annual
        -- maintenance (5.0/tick). The same 50x reaches the goods half: one
        -- ration per head per tick is 50 rations a quarter from ONE regiment,
        -- against a processing facility's whole output of 8 batches a tick.
        --
        -- THE FACTOR. Restore the derivation's OWN stated property in
        -- consistent units:
        --     4 ticks x 50 heads x credits_per_head  <  hire_base_cost (40)
        --     => credits_per_head < 0.2
        -- 0.15 is chosen with headroom below that edge, exactly as
        -- credits_per_head_per_power was chosen with headroom below its own —
        -- so a Levy Spear costs 40 to raise and 30 a year to keep. That is
        -- 0.15 / 6.0 = 0.025, and EVERY entry below is multiplied by it,
        -- credits and goods alike.
        --
        -- WHY UNIFORMLY. The anchor value_anchor.cpp asserts is a RATIO —
        -- SUM(goods_per_head[r] x base_price[r]) ~= 2 x wage_per_head — so
        -- scaling both halves by one factor leaves every roster row exactly
        -- where it was in the band. The relation is preserved, not weakened;
        -- what changed is the scale it is denominated in. The power bound
        -- rescales with it: (0.3005 x 0.667 - 0.15) / 420 = 0.00012, and
        -- 0.0001 keeps the same proportional headroom 0.004 kept below
        -- 0.004793.
        --
        -- WHAT IS LOST, said out loud: "one ration per head per tick" was the
        -- legible anchor for the food line and 0.025 rations per head is not
        -- legible at all. The legibility belonged to a per-head reading of a
        -- quantity the economy consumes per-BUILDING, which is the same
        -- mismatch this rescale corrects; recovering it means restating the
        -- goods draw per unit rather than per head, which is a design change
        -- and not this item's.
        --
        -- THE ANCHOR (NATIONS.md § 3, Ben 2026-08-22): "the equipment needed to
        -- sustain a unit costs about 2x their salary for that year", clarified
        -- to price the GOODS draw against the WAGE's flat term using authored
        -- base_price (world_gen.lua), never the resolved market price:
        --
        --     Σ ( goods_per_head[r] × base_price[r] )  ≈  2 × credits_per_head
        --
        -- An economy tick is one quarter, so the annual factor of 4 appears on
        -- both sides and cancels — the identity holds per-tick as written.
        --
        -- CREDITS. credits_per_head = 6.0 matches the lowest authored
        -- base_wage in this file (line 189, 6.0) — a token levy wage, the same
        -- floor a civilian worker draws, deliberately conservative for a first
        -- cut. credits_per_head_per_power adds a power-scaled top-up (the same
        -- "floor plus power-scaled component" shape BL-394 gave
        -- hire_cost_per_power) — BOUNDED, not picked freely: goods_per_head is
        -- ONE flat table for the whole roster (see GOODS below) while the wage
        -- is per-row, so tools/verify/value_anchor.cpp's R1/R5 rows require
        --     wage(row) = credits_per_head + credits_per_head_per_power × power_mod
        --     in  [ equip × 0.4, equip × 0.667 ]   (the ±25% band solved for wage)
        -- for EVERY row, equip = 12.02 (below). The roster's heaviest row
        -- (Mechanised Column, power_mod 420) is the binding case: solving
        -- wage(420) <= 12.02 × 0.667 = 8.013 gives
        --     credits_per_head_per_power <= (8.013 - 6.0) / 420 = 0.004793
        -- 0.004 is chosen with headroom below that edge (R5's own "measured
        -- off the edge, not on it") rather than at the limit — a Levy Spear
        -- (power_mod 0) pays 6.0/head/tick flat; a Rifle Regiment (power_mod
        -- 380) pays 6.0 + 380×0.004 = 7.52/head/tick; the heaviest row,
        -- Mechanised Column, pays 6.0 + 420×0.004 = 7.68/head/tick. Annualised
        -- (×4) that is 24 / 30.08 / 30.72 respectively — all well under their
        -- one-off hire_base_cost/hire_cost_per_power prices (40 / 230 / n/a),
        -- so keeping a standing unit for a year costs meaningfully less than
        -- raising a fresh one, as it should. VERIFIED: value_anchor's R1a
        -- passes at this rate (0/19 roster rows out of band; see harness
        -- output — before this bound was derived, an unbounded first guess of
        -- 0.02 put 10 of 19 rows outside the band, caught by that same row).
        --
        -- GOODS. The anchor target is 2 × credits_per_head's FLAT term = 12.0
        -- credits of goods value per head per tick (the power-scaled wage
        -- component has no goods counterpart — goods_per_head is one table,
        -- not per-class, per NATIONS.md's "a band, not an identity"). Split:
        --   food_rations_per_head = 1.0  -> 1.0 × 6.0  (base_price) =  6.00
        --   ordnance_per_head     = 0.14 -> 0.14 × 43.0 (base_price) =  6.02
        --                                                     total  = 12.02
        -- "one ration per head per tick" is the legible anchor for the food
        -- line; ordnance is sized to bring the pair to the 2x target (12.02 ≈
        -- 2 × 6.0). Both draw from resource_type::ordnance / food_rations
        -- (BL-457, BL-543).
        --
        -- SUPPLY CURVE. supply_decay_permille = 50 (5%/tick of the 0..1000
        -- factor): a fully-supplied unit left unsupplied reaches 0 supply in
        -- 20 ticks (5 years at one tick/quarter) — a long enough runway that
        -- one missed convoy isn't catastrophic, short enough that sustained
        -- neglect genuinely hollows a force out. supply_recovery_permille =
        -- 100, 2x the decay rate, so correcting a supply failure repairs
        -- faster than the failure itself did damage.
        --
        -- REACH. out_of_supply_reach = 24.0 reuses construction.max_logistics_
        -- reach (line 578) EXACTLY — not a new scale. MILITARY.md's own
        -- ruling is that economic reach IS military reach (one reach field,
        -- BL-325 S3), so the "how far is still reachable" number a building
        -- already answers is the same number a unit's supply line answers.
        unit_upkeep = {
            -- BL-635: every figure in this table is its pre-2026-08-26 value
            -- x 0.025. The old values are kept in the trailing comments so the
            -- rescale is one readable multiplication rather than a rewrite.
            credits_per_head           = 0.15,   -- flat wage per head per tick (was 6.0)
            credits_per_head_per_power = 0.0001, -- wage scaled by the roster row's power_mod (was 0.004; bounded <= 0.00012, see derivation above)

            -- The goods half of the vector, per head per tick. ORDNANCE is the
            -- good (BL-457 added it as the roster's first terminal MILITARY
            -- good precisely so this draw could consume it); food_rations is
            -- the sanctioned second line. Both rescaled by the same 0.025, so
            -- value_anchor's equipment:wage ratio is untouched.
            goods_per_head = {
                ordnance     = 0.0035, -- was 0.14
                food_rations = 0.025,  -- was 1.0
            },

            supply_decay_permille    = 50,   -- the ONE decay subtraction, per tick
            supply_recovery_permille = 100,  -- regained per tick while supplied
            out_of_supply_reach      = 24.0, -- reach cost past which a unit is out of supply; <= 0 disables
        },

        -- BL-470: march points spent per tick against per-tile traversal cost
        -- (the SAME cost body_reach_field/the road network already compute —
        -- see logistics.hpp's tile_traversal_cost, plains=1.0, mountain=2.0,
        -- discounted by road_level). PER-CLASS from the start (Ben's own
        -- overturn of the item's one-speed first cut, same elicitation
        -- session, 2026-08-19): a wing outpaces a line. First-cut authored
        -- constants — a Levy Spear on Plains (cost 1.0/tile) covers 1 tile/
        -- tick unescorted; a Rifle Regiment covers 1.5. Naval is 0 —
        -- strategic-only presence (combat.hpp), no naval movement model yet.
        march_points_per_class = {
            infantry = 1.0,
            cavalry  = 1.5,
            ranged   = 1.0,
            siege    = 0.5,
            naval    = 0.0,
        },

        -- BL-596 (LP_ACTIVE_MARCH, 2026-08-25): active Logistic Points — the
        -- first real consumer of LOGISTICS.md's Logistic Points design.
        -- NR-600 flags both numbers below for Ben's calibration by playtest;
        -- this is a reasoned first cut, not a final ruling (LOGISTICS.md
        -- "Active lands first", 2026-08-24).
        --
        -- CAPACITY, and it is ONE rate for BOTH halves of the bifold split
        -- (LOGISTICS.md rule 6, "no reserved military share... guns and
        -- butter stop competing" is the thing to avoid). The anchor passes
        -- so much THROUGH ITSELF per tick, and what flows is either goods or
        -- soldiers:
        --   * an active draw is march points (march_points_per_class above)
        --     — an infantry unit's whole tick costs 1.0, a cavalry unit 1.5;
        --   * a passive draw is CARGO UNITS — a 30-unit convoy costs 30.
        -- Treating one march-point as one cargo unit is the implicit exchange
        -- rate, and it is a first cut: nothing measured says a soldier-tick
        -- and a unit of grain burden a city equally. Flagged with the rest
        -- for calibration (NR-600).
        --
        -- The passive draw is QUANTITY and NOT DISTANCE (Ben, 2026-08-25,
        -- ruling on NR-620). Distance is already priced, in credits, by
        -- base_cost_per_unit_distance below; charging it again here is what
        -- LOGISTICS.md constraint 3 means by "LP *is* haulage cost again".
        --
        -- 20.0 MEASURED, not guessed. Against the real generated world
        -- (tools/verify/haulage_measure.cpp): the no-gate baseline dispatches
        -- 1055 convoys (802 intra-body); at this rate the quantity draw
        -- dispatches 1041 (789) — 98.7% of baseline, so ordinary peacetime
        -- trade flows, while ~14 legs a run still hit the cap, so it is felt
        -- rather than decorative (rule 3: a point generated and never spent
        -- is the military_points defect renamed). Re-run that harness against
        -- 1055/802 after ANY change here. For scale, the same rate under the
        -- rejected distance draw dispatched 284.
        active_lp_per_anchor_tick = 20.0,

        -- PRICE. active_lp_credit_per_unit_distance is argued against
        -- logistics.base_cost_per_unit_distance.land (0.02, below) rather
        -- than guessed — same "credits per unit-distance" shape the convoy
        -- layer already prices with. Doubled to 0.04: moving armed units
        -- costs more than routine cargo the way the convoy table itself
        -- already prices mode by burden (land 0.02 < sea 0.05 < air 0.15 <
        -- space 1.00) — a marching column's escort/coordination overhead
        -- sits above a cargo haul's, so a flat 2x on the cheapest (land)
        -- rate is the conservative first step up that ladder rather than a
        -- reach for the next rung. Charged per (active-LP-unit drawn x head
        -- in the unit), so a company of 200 drawing 1.0 LP this tick pays
        -- 200 x 1.0 x 0.04 = 8.0 credits — well under hire_base_cost (40,
        -- above), so marching an already-raised unit stays far cheaper than
        -- raising a fresh one, the same relationship BL-603's upkeep
        -- derivation keeps for standing-force wages.
        active_lp_credit_per_unit_distance = 0.04,
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
        -- BL-392: what a commitment buys, and what the distance costs. The
        -- discount is asymptotic in the order size; the freight is paid to the
        -- supplier (a transfer), and only when the goods cross bodies.
        volume_discount_max           = 0.15,
        volume_discount_half_quantity = 100.0,
        offbody_freight_fraction      = 0.05,
    },

    -- BL-545/BL-546: the relational substrate's DECAY (sentiment.hpp). Each
    -- rate is the fraction of the remaining distance to neutral a dimension
    -- sheds per tick; the substrate was inert until this table existed.
    --
    -- Ben's ruling (NR-568, 2026-08-23): a cancellation is forgotten in NINE
    -- QUARTERS. One tick is one quarter, so that is a HALF-LIFE of 9 ticks, and
    -- the rate is authored from it rather than guessed:
    --
    --     rate = 1 - 2^(-1 / half_life_ticks) = 1 - 2^(-1/9) = 0.0741253...
    --
    -- Authored as the RATE, not the half-life, because deriving it in-engine
    -- would put a std::pow on the tick (sentiment.hpp says why that is refused).
    -- Re-derive it from the line above if the ruling ever changes. Six
    -- significant figures: a -2 Trust event is -1.000003 after nine ticks in
    -- float32, which the sentiment harness asserts.
    --
    -- Access decays at the SAME rate until something emits into it (BL-540 is
    -- the first nation->corp emitter); there is no separate ruling for it yet.
    -- The loader (recipe_registry.cpp) REJECTS either rate at load unless it is
    -- a finite number in [0, 1] — an authored rate is never clamped silently.
    --
    -- The factor weights are NOT authored here: contract_completed /
    -- contract_cancelled Trust weights are seeded from procurement's
    -- reputation_on_complete / reputation_on_cancel above (BL-546), and every
    -- other factor stays at zero until its emitter lands.
    --
    -- BL-573: contract_failed is the mercenary contract's own third terminal
    -- state (CONTRACTS.md § Q2 has no procurement analog to seed from, unlike
    -- the two above). Authored directly here at DOUBLE reputation_on_cancel's
    -- magnitude (-2.0) — a round, legible number that satisfies CONTRACTS.md's
    -- "failed... down hardest" ordering against contract_cancelled's "down,
    -- but less". Iterate by playtest; nothing else derives this figure.
    -- BL-628: the whole-firm buyout's price level (docs/economy/FINANCE.md
    -- § Whole-firm acquisition). The price is
    --
    --     max(0, book_value + multiple x trailing_net + balance)
    --
    -- where trailing_net is the mean filed `net` over the last EIGHT quarters
    -- (k_acquisition_trailing_quarters, components.hpp — a constant, because the
    -- window says how much history a price may read, which is a design statement
    -- rather than a price level).
    --
    -- One economy tick is one quarter, so `multiple = 8` is TWO YEARS of
    -- purchase against two years of observation: the buyer pays two years of
    -- what the last two years actually earned. Round and legible, and nothing
    -- else derives it — move it by playtest. It is authored here precisely so
    -- that tuning the acquisition market is a data change.
    --
    -- SIGNED and unclamped by design. A chronically loss-making firm prices
    -- BELOW its book value and should; the only floor is zero, and that floor is
    -- there because the price is a SINK (there is no modelled seller to pay a
    -- negative price to), not because book value is a redemption anyone could
    -- take — the prototype refunds nothing on demolition.
    --
    -- Rejected at load, never clamped, if it is not a finite number >= 0.
    acquisition = {
        multiple = 8.0,
    },

    sentiment = {
        trust_decay_per_tick  = 0.074125,
        access_decay_per_tick = 0.074125,
        factors = {
            contract_failed = { trust = -4.0 },
        },
    },

    -- BL-430: switching a processing_facility's recipe through the player-grade
    -- seam (the management UI's method dropdown, or an AI's dial_recipe margin
    -- chase — both go through corp_command's set_recipe verb) is a COMMITMENT,
    -- not a free per-tick optimisation. The BL-079 reflex switch (an idled/
    -- floored building's automatic recipe change) does NOT read this table and
    -- stays free/instant — it is sanctioned auto-agency reacting to a loss, not
    -- a strategic choice. Cost is a round, legible number (half a Track road);
    -- cooldown is short enough that a genuine trade-off is still worth chasing,
    -- long enough that a corp cannot flip-flop every tick for a marginal margin.
    -- BL-434: a group-tagged recipe roster splits the generic processing_facility
    -- into recognizable sub-facility kinds (Metal Foundry, Food Processing, ...).
    -- Switching WITHIN a group (Bloomery -> Smithy, both Metal Foundry) is the
    -- cost/cooldown below; switching ACROSS groups is refused outright by
    -- try_switch_recipe (economy_system.cpp) — retracted from an earlier
    -- cross_group_multiplier price tier in this same session. The only way to
    -- change a building's group is dismantle + rebuild via the tile selector.
    recipe_switch = {
        switch_cost    = 12.0,
        -- Was 6 (Ben's playtest, 2026-08-16): 6 ticks x 90 days/tick is ~1.5
        -- in-game years to change what a building makes, which read as "not
        -- possible" rather than as a deliberate commitment. Dropped to 1 tick
        -- (one quarter) — still a real cooldown, not an instant flip, but not
        -- multi-year friction on a routine dial. Flagged for further playtest
        -- tuning, see NR-253.
        cooldown_ticks = 1,
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

    -- BL-365: the old BL-078 elastic nation-substrate model is GONE — the
    -- saturated background economy (GENERATION_STRATEGY.md) is now real
    -- background corporations (corporation_generation.cpp's
    -- generate_background_firms), producing and consuming through the normal
    -- recipe/workforce/market pipeline like any corp. What survives here is
    -- only the basket + threshold the population-growth gate in
    -- run_economy_step still needs, to test whether a centre's consumption is
    -- broadly met before it levels up.
    population_growth = {
        -- Per-capita basket weight per resource, used only to weight the
        -- growth gate's met-supply ratio. Unlisted resources get 0.
        demand_basket = {
            food_rations         = 0.70,  -- population primary
            agricultural_produce = 0.55,  -- food processing + direct
            steel                = 0.45,  -- construction / industry
            water                = 0.40,  -- life support + industry
            refined_fuel         = 0.40,  -- energy
            iron_ore             = 0.35,  -- background smelting input
            petroleum            = 0.30,  -- background refining input
        },
        growth_met_threshold = 0.50, -- basket met-supply ratio a centre needs to grow
    },

    -- BL-617 (population migration; docs/economy/POPULATION.md § Migration).
    -- A deterministic per-tick flow moves heads from low- toward high-
    -- attractiveness centres on each body, stance-gated between nations,
    -- carrying qualification (brain drain). First-cut-then-tune (NR-600):
    -- the shape is the design, every number here is a dial. The loader
    -- REJECTS out-of-range values (permille outside [0,1000], wage_weight
    -- non-finite/negative, selectivity below 1) rather than clamping.
    migration = {
        rate_permille         = 10,   -- share of a below-mean centre's heads seeking to move per tick
        neutral_gate_permille = 250,  -- throttle between NEUTRAL nations (friendly 1000, hostile 0)
        wage_weight           = 0.02, -- credits -> attractiveness on the clearing-wage signal
        qualified_selectivity = 1.5,  -- movers skew qualified: migrants carry min(1, origin_q x this)
    },

    -- BL-340/BL-365: background-industrial demand for the mid-chain processing
    -- goods real background firms alone would under-consume during the
    -- pre-game warm start / early game, before enough of them exist. A
    -- world-scale pull, not per-centre (unlike population_demand below).
    -- Deliberately excludes spacecraft_components — the militia's procurement
    -- contracts are that good's only intended buyer (BL-340).
    --
    -- BL-640 (2026-08-26): BANDED, NOT DELETED. All six goods are industrial —
    -- an ancient campaign could make none of them, and before this banding they
    -- were 79.8% of every modelled want at 0 CE. The whole basket therefore moves
    -- into an `industrial` row, so the pass injects nothing in the ancient band.
    -- It stays because it is still the stand-in for the Industry channel until
    -- BL-641 gives buildings a real upkeep goods vector; MARKETS.md property 1
    -- says a channel whose size is a constant is a stopgap, and this one is
    -- labelled one.
    background_demand = {
        -- No SHARED tranche: nothing this pass pulls is band-independent.
        demand_basket = {},
        baskets = {
            { era = "industrial", demand_basket = {
                silicon         = 0.20,
                refined_copper  = 0.20,
                ree_alloy       = 0.15,
                machinery       = 0.15,
                alloys          = 0.15,
                electronics     = 0.15,
                -- spacecraft_components intentionally absent — militia-only demand.
            } },
        },
        demand_elasticity = 0.80,
        elasticity_min     = 0.30,
        elasticity_max     = 2.50,
        demand_scale       = 1.00,
    },

    -- BL-368 (2026-08-11): the real per-centre population demand basket,
    -- generalising the BL-190 flat agricultural_produce-only stub. Same
    -- price-elastic shape as the substrate model above, but population is a
    -- pure CONSUMER — no supply term. DEMAND = pcc.scale × demand_scale ×
    -- basket[r] × elasticity(price). Read by inject_population_demand
    -- (market_clearing.cpp), called from clear_markets after its demand reset.
    --
    -- BL-640 (2026-08-26): THE BASKET IS ERA-BANDED, through the same `era`
    -- field and the same era_permits mask recipes have carried since BL-433.
    -- Recipes were banded; the baskets were not, so a basket authored in
    -- industrial goods left an ancient campaign wanting three things it could
    -- never make (clean_water, consumer_goods, medical_supplies — census:
    -- MARKETS.md § Demand channels, property 2 states the rule).
    --
    -- The split is by PRODUCIBILITY, not by taste. `demand_basket` below is the
    -- SHARED tranche: subsistence, which every band both wants and can supply
    -- (food_rations has an ancient recipe; agricultural_produce and water are
    -- deposits, band-independent by construction). Each `baskets` row is the
    -- band's own value chain on top of it.
    --
    -- THE TWO TRANCHE TOTALS WERE EQUAL IN UNITS (0.75 each), and BL-655 found
    -- that unit equality was measuring the wrong thing. A unit of ancient cloth
    -- (base 3.7) and a unit of industrial consumer goods (base 12.0) are not
    -- comparable quantities, so "0.75 = 0.75" made two very different costs of
    -- living look identical. Priced out at base_price — the only cross-band
    -- comparable measure the world has — the ancient household cost 3.12 credits
    -- per scale point against the industrial household's 6.15. HALF. The band was
    -- changing how much it costs to live, which is exactly what BL-640 said the
    -- band must NOT do.
    --
    -- SO THE INVARIANT IS RESTATED IN CREDITS, and it is the answer to
    -- POPULATION.md § Open items' "the cost of living — how much it costs a head
    -- to live". The two band tranches now cost the SAME at base price:
    --
    --     ancient    0.25x3.7 + 0.20x3.4 + 0.15x7.2 + 0.15x2.9      (cloth, ceramics,
    --              + 0.30x4.0 + 0.25x4.3 + 0.50x1.5  = 6.145 cr      leather, dressed stone,
    --                                                                charcoal, planks, timber)
    --     industrial 0.35x3.0 + 0.25x12.0 + 0.15x14.0 = 6.150 cr
    --
    -- 0.08% apart, and DERIVED from the industrial tranche rather than picked —
    -- re-derive if world_gen.lua's base_price table moves. Add the shared tranche
    -- (4.65 cr) and a head costs 10.80 credits per scale point to keep alive in
    -- either band. The UNIT totals are now 1.80 ancient against 0.75 industrial:
    -- an ancient household buys 2.4x as many things, each of them far cheaper,
    -- which is what a pre-industrial basket of bulk fuel and timber should look
    -- like next to a basket of medical supplies.
    --
    -- WHY THIS WAS RETUNED AT ALL (BL-655). Banding the baskets correctly took
    -- the generated world from 584 buildings / 104 corps to 261 / 64, because
    -- Pass 6 (CORPORATION_GENERATION.md § Pass 6) had been sizing background
    -- firms against a demand that included six industrial goods the ancient band
    -- structurally cannot produce. Ben's ruling: keep the fix, retune density up.
    -- The lever taken is THIS one — the world consumes more per head, so more
    -- firms are supported at the same ~90% coverage and prices hold. Pass 6's
    -- target fraction was deliberately NOT raised: that reaches the same count by
    -- making the world overproduce, and buys density with gluts.
    --
    -- ERA IS ONLY THE FIRST HALF of the ladder POPULATION.md § Population demand
    -- describes — era decides which value chain, STRATUM decides how far up it.
    -- The stratum half is not authored here: this pass still scales one basket
    -- by pcc.scale linearly. A future stratum gate is a second field on these
    -- same rows, not a second table.
    population_demand = {
        -- The SHARED (`any`) tranche: per-scale-point weight per resource, wanted
        -- in every band. Unlisted resources get 0.
        demand_basket = {
            food_rations          = 0.60, -- population primary (replaces the old flat agri stub)
            agricultural_produce  = 0.20, -- direct consumption, lighter than the processed staple
            water                 = 0.30, -- life support
        },
        baskets = {
            -- The ancient household. The terminal artisan goods the 0 CE roster
            -- ALREADY PRODUCES and, before this row, nobody bought: ceramics,
            -- dressed_stone and leather each sat in the census's
            -- "produced in-band, NO market sink" list. Cloth leads because
            -- clothing is the largest recurring artisan want; dressed stone is
            -- the household's share of building, distinct from BL-642's
            -- construction draw.
            --
            -- BL-655 added the FUEL AND TIMBER lines. They are not padding to
            -- reach a firm count: a pre-industrial household's largest recurring
            -- volume after food is what it burns, and this basket had no fuel
            -- line at all. charcoal is the worked fuel (a kiln's output, dearer
            -- than the wood it comes from); timber is the raw one — firewood,
            -- the poorest household's whole fuel bill — and is why timber is the
            -- second-heaviest line here by weight and one of the lightest by
            -- cost; planks are furniture, roofing and repair, the worked-wood
            -- counterpart to dressed_stone's worked stone.
            --
            -- WHAT WAS DELIBERATELY LEFT OUT, and it bounds this retune: `tools`
            -- and `trade_goods_misc` are both real ancient household goods, both
            -- produced in-band, and both currently RED in chain_depth's
            -- unsubstantiated list — naming either here would close a red and add
            -- another tranche of background firms. They are not named because
            -- another item already owns their buyer: BL-590 (a construction
            -- material draw) for tools, BL-647 (endemic luxury demand) for
            -- trade_goods_misc. Taking a good whose demand another item owns is
            -- how a tuning change quietly becomes someone else's design.
            { era = "ancient", demand_basket = {
                cloth         = 0.25,
                ceramics      = 0.20,
                leather       = 0.15,
                dressed_stone = 0.15,
                charcoal      = 0.30, -- the worked fuel: cooking and heat, every day
                planks        = 0.25, -- furniture, roofing, repair
                timber        = 0.50, -- firewood and unworked building wood; cheapest line, largest volume
            } },
            -- The industrial household: the BL-368 habitability tranche, moved
            -- here UNCHANGED in both membership and weight. Nothing about the
            -- industrial arc's demand is retuned by BL-640 — only the band it
            -- is confined to.
            { era = "industrial", demand_basket = {
                clean_water      = 0.35,
                consumer_goods   = 0.25,
                medical_supplies = 0.15,
            } },
        },
        demand_elasticity = 0.80, -- exponent on (base_price / price); matches the substrate shape
        elasticity_min    = 0.30,
        elasticity_max    = 2.50,
        demand_scale      = 1.00,
    },

    -- BL-442 (2026-08-17): THE price band. A market price is anchored to its
    -- rarity-derived base_price and pushed by the tick's supply/demand ratio
    -- (damped sqrt elasticity), then clamped to this band and eased toward it by
    -- an EMA. Read by resolve_price (market_clearing.cpp) AND by the BL-181
    -- workforce auto-solver's forward price estimate (wf_target_price,
    -- economy_system.cpp) — until BL-442 those were two hand-synchronised
    -- constexpr copies of the same two numbers.
    --
    -- STEP 2 (2026-08-17): the CEILING is now DERIVED, not authored. Ben's
    -- requirement is that scarcity must price high enough to cross the margin for
    -- a nearby market, so inter-market trading exists from day 1. That makes the
    -- ceiling a function of the haulage the supply layer already charges:
    --
    --     ceil_mult * base_price  >  base_price + haulage_per_unit
    --     ceil_mult               >  1 + haulage_per_unit / base_price
    --
    -- MEASURED, not assumed (tools/verify/haulage_measure.cpp, 5 seeds of the real
    -- generated world): per-unit haulage from a market to its NEAREST market
    -- neighbour is median 0.70, p90 1.67, max 4.83 credits. The cheapest good
    -- carrying a base price is 0.60. The BINDING case — worst haul, cheapest
    -- good — is therefore
    --
    --     ceil > 1 + 4.83 / 0.60 = 9.06   ->  10.0, the next round number above it
    --
    -- 4.0 satisfied only the MEDIAN neighbour pair (needs 2.16) and just barely
    -- the p90 (needs 3.79); it left the tail — the worst-connected market pair
    -- carrying the cheapest good — permanently unservable. 10.0 covers every
    -- nearest-neighbour pair measured, for every priced good.
    --
    -- The derivation is stated in full in docs/economy/MARKETS.md § The price band
    -- so it can be RE-DERIVED rather than trusted; re-run haulage_measure if the
    -- logistics costs, the map scale or the base-price table change.
    --
    -- THE FLOOR IS DELIBERATELY UNCHANGED. Ben's requirement derives a ceiling and
    -- says nothing about a floor. Lowering it would widen the arbitrage margin too,
    -- but only by cutting the revenue of producers already selling into a glut —
    -- the direction Sprint 19's falling numbers are already complaining about — and
    -- it would be an authored guess sitting next to a derived number, which is the
    -- exact confusion step 1 existed to remove.
    -- ===================================================================
    -- BL-654: THE BUYER'S RESERVATION CEILING lives in THIS family.
    --
    -- Ben's ruling, 2026-08-26: "Buy on the market, but at a threshold,
    -- buying is not allowed. This goes hand in hand with maximum and minimum
    -- prices for goods." It sits beside floor_mult / ceil_mult and not in
    -- upkeep because it is a statement about what a good is worth PAYING, not
    -- about who is buying — one number per world, read by every goods draw.
    --
    -- It is the exact mirror of the seller's floor_price (BL-386): both sides
    -- may decline a trade, neither may dictate one.
    --
    -- MEASURED, NOT GUESSED. tools/verify/demand_census.cpp R3 reports the
    -- mean resolved price as a multiple of base, per resource, per band, over
    -- every market that prices it. Seed 0, 80-tick warm start, shipped spawn:
    --
    --   LOWER BOUND — it must ADMIT what the live draws actually buy. The only
    --   goods draw live at shipped rates is unit upkeep, and its dearest good
    --   is food_rations: 7.788x base in the ancient band (14 markets, ceiled in
    --   8) and 8.629x in the industrial (9 markets, ceiled in 5). A ceiling
    --   below 8.63 declines the food draw in the band where it is dearest, so
    --   every unit's draw goes unmet — which is exactly the mechanism that took
    --   operating firms 227 -> 19 when BL-641 turned building upkeep on. The
    --   bound is the INDUSTRIAL figure, not the ancient one.
    --
    --   UPPER BOUND — it must DECLINE the cap. A resource pegged at ceil_mult
    --   is a generation-calibration signal (MARKETS.md § Price resolution),
    --   not a legitimate purchase, and a reservation equal to the cap declines
    --   nothing at all: the rule would be inert the day it landed.
    --
    --     8.63  <  reservation_mult  <  10.0   ->  9.0
    --
    -- WHAT 9.0 ACTUALLY REFUSES, measured on the same run. Ancient: ceramics
    -- (9.240x, ceiled in 12 of 14) and leather (9.968x, 13 of 14). Industrial:
    -- consumer_goods (9.082x), silicon (9.377x), alloys (9.666x), ree_alloy
    -- (9.867x) and electronics (9.986x). Every one of those is among its
    -- band's most-ceiled rows — the goods nothing in the world makes — and all
    -- 21 other priced goods are admitted. That is the rule doing its job: a
    -- starving building does not bid a good to its cap chasing a shortfall no
    -- amount of credits can fix.
    --
    -- A CONSEQUENCE WORTH EXPECTING, not a defect: a good that nobody supplies
    -- OSCILLATES around this ceiling. Demand with zero supply resolves to
    -- base x ceil_mult, which is above the reservation, so the next tick's draw
    -- declines and the EMA pulls the price back down until it bids again. The
    -- price settles near reservation_mult — high enough that a rival scoring
    -- the building which supplies it can see the gap, which is the whole point.
    --
    -- RE-DERIVE RATHER THAN TRUST. Re-run demand_census and read the R3 table
    -- whenever base_price, ceil_mult, or either upkeep basket changes — all
    -- three move the two bounds above.
    -- ===================================================================
    price_band = {
        floor_mult       = 0.25, -- lowest a price may fall, x base_price
        ceil_mult        = 10.0, -- highest a price may rise, x base_price (derived; see above)
        reservation_mult = 9.0,  -- BL-654: a goods draw declines to buy above this (derived; see above)
    },

    -- BL-263 (2026-08-11): spontaneous market emergence — a market appears the
    -- tick the first building completes on a body that has none, seeded from the
    -- home body's own prices/demand rather than from nothing. See
    -- maybe_spawn_market / inject_interbody_demand (market_clearing.cpp) and
    -- docs/economy/MARKETS.md § Spontaneous market emergence.
    market_emergence = {
        price_distance_gain = 0.08, -- opening-price markup per AU from the home body
        pull_fraction       = 0.50, -- fraction of the home body's unmet demand pulled onto an outpost
        distance_falloff    = 0.15, -- per-AU falloff denominator on the pulled demand
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
