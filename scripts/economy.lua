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
            credits_per_head           = 6.0,   -- flat wage per head per tick
            credits_per_head_per_power  = 0.004, -- wage scaled by the roster row's power_mod (bounded <= 0.004793, see derivation above)

            -- The goods half of the vector, per head per tick. ORDNANCE is the
            -- good (BL-457 added it as the roster's first terminal MILITARY
            -- good precisely so this draw could consume it); food_rations is
            -- the sanctioned second line.
            goods_per_head = {
                ordnance     = 0.14,
                food_rations = 1.0,
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

    -- BL-340/BL-365: background-industrial demand for the mid-chain processing
    -- goods real background firms alone would under-consume during the
    -- pre-game warm start / early game, before enough of them exist. A
    -- world-scale pull, not per-centre (unlike population_demand below).
    -- Deliberately excludes spacecraft_components — the militia's procurement
    -- contracts are that good's only intended buyer (BL-340).
    background_demand = {
        demand_basket = {
            silicon         = 0.20,
            refined_copper  = 0.20,
            ree_alloy       = 0.15,
            machinery       = 0.15,
            alloys          = 0.15,
            electronics     = 0.15,
            -- spacecraft_components intentionally absent — militia-only demand.
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
    price_band = {
        floor_mult = 0.25, -- lowest a price may fall, x base_price
        ceil_mult  = 10.0, -- highest a price may rise, x base_price (derived; see above)
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
