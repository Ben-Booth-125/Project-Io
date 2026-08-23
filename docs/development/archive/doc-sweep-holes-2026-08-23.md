# Doc state-independence sweep — holes audit (2026-08-23)

Every 'what is absent / build status / limitation' line deleted from an authority doc in the 2026-08-23 sweep, with its verdict at the time: an owning open item, NO ITEM, or STALE (the code already did it). Scan output, not work: the backlog was rebuilt the same day around Sprint 16 only, so NO ITEM lines here are **not** filed. Format per line: doc | hole | verdict.

## A

# Group A — holes audit

Format: `<doc> | <hole> | BL-nnn (short_name) | NO ITEM | STALE (code already does it)`

CONCEPT.md | player identity pivot to the militia (space arc) not in the player's seat | BL-094 (PLAYER_MILITIA_PIVOT, designed, v0.3.0)
CONCEPT.md | sentiment layer "designed, not built" | STALE — `src/world/sentiment.{hpp,cpp}` exists (BL-545 SENTIMENT_SUBSTRATE still open in backlog; surface is BL-556 SENTIMENT_HAS_NO_SURFACE)
CONCEPT.md | "nothing in the campaign layer commands the battle resolver" | STALE — `run_battles` (battle_system.cpp) opens campaign battles; BL-467 complete
CONCEPT.md | Era 1 name/theme "identity owed"; Era 2 theme unresolved | NO ITEM (BL-375 TIME_TO_SPACE_PACING is the nearest, not the same)
CONCEPT.md | whether the player holds any research capability (Sprint 8 open question) | NO ITEM
CONCEPT.md | syndicate tier not built | BL-524 (SYNDICATE_TIER, designed, v0.1.23)
SYSTEMS.md | Conflict "nothing in Era 0 commands a unit yet" | STALE — hire_unit verb, unit_component, run_battles all in code
SYSTEMS.md | Force "designed, not built at campaign scale" | STALE (units/muster/battles live); remaining spine is BL-315 (ARMED_HOUSE_CONFLICT_SPINE)
SYSTEMS.md | Airfield building not in building set (air mode never dispatched) | NO ITEM
SYSTEMS.md | per-node throughput capacity "deferred" | BL-464 (LOGISTIC_POINTS, design-owed, v0.1.20)
SYSTEMS.md | research points economy "still open" | STALE for accumulation (BL-332 complete, research_institute exists); spend side is BL-478 (ANCIENT_RESEARCH_SPEND)
SYSTEMS.md | constellation grain, deeds, quest trees | BL-087 (ERA1_TECH_QUEST_SYSTEM, designed, v0.3.0)
SYSTEMS.md | "the other nine laws, the other three effect families" | BL-155 (LAW_POLICY_SURFACE_DESIGN, designed, v0.1.11)
SYSTEMS.md | laws ledger (enactment politics surface) | BL-186 (LAWS_LEDGER_UI, designed, v0.1.11)
SYSTEMS.md | relationship axis BL-345 listed as open | STALE — BL-345 is complete
SYSTEMS.md | negotiated tax rate | BL-280 (NEGOTIATED_TAX_RATE, design-owed, v0.1.11)
SYSTEMS.md | history ladder later stages "open" | BL-222 (HISTORY_LADDER_INDUSTRIAL), BL-223 (AVERTED_RUPTURE_DIPLOMACY_ORIGIN), both designed, v0.4.0
SYSTEMS.md | "reach is binary — never how much can move" (progression-chain ceiling) | BL-464 (LOGISTIC_POINTS)
SYSTEMS.md | nation treasury "spent by nothing" (inherited from NATIONS.md summary) | STALE — run_national_budget + nation scorer live (commit f5d68ff1); BL-537/BL-542 still open in backlog
GLOSSARY.md | Militia "designed, not built" | BL-094 (PLAYER_MILITIA_PIVOT)
GLOSSARY.md | Syndicate "designed, not built" | BL-524 (SYNDICATE_TIER); BL-525..BL-530 children
GLOSSARY.md | HQ influence range / corporate border "deferred" | BL-182 (CORPORATE_BORDERS, designed, v0.3.0)
GLOSSARY.md | Sentiment "no value stored anywhere" | STALE — sentiment.cpp exists; BL-545 open for the remaining wiring
GLOSSARY.md | Faction: per-faction sentiment not built | STALE (same)
GLOSSARY.md | Province locality DISCOUNT "still owed" | NO ITEM (NR-342 records the duplication question; nation_ai.hpp has a locality row but no price discount in code)
GLOSSARY.md | ISRU "designed, not built" | NO ITEM (space arc; ERAS.md owns the design)
GLOSSARY.md | Drill-through idiom "owed, not built" | STALE — BL-214 is complete
GLOSSARY.md | Railroad mode "deferred" | BL-173 (RAILROAD_TRANSPORT_MODE, designed, v0.1.12)
GLOSSARY.md | history ladder industrial stages / averted rupture "open" | BL-222, BL-223
META_LAYER.md | five of six modifier subjects wired to nothing | processing_yield → BL-513 (PROVINCE_BUILDING_LIMIT, complete — but code still reads no processing_yield modifier: the header's promise is unmet, NO open item names it); unit_upkeep → BL-543 (UNIT_VALUE_ANCHOR); logistics_cost → BL-464 (LOGISTIC_POINTS); wage_floor → BL-538 (TREASURY_PRIORITY_LINES); collapse_strain → BL-477 (ERA_COLLAPSE_DEFINES_META) / BL-548 (EVENT_SYSTEM)
META_LAYER.md | quests do not exist | BL-087 (ERA1_TECH_QUEST_SYSTEM)
META_LAYER.md | science is reached, not spent | BL-478 (ANCIENT_RESEARCH_SPEND, design-owed, v0.1.11)
META_LAYER.md | no serialiser (already struck through in the doc) | STALE — `src/world/world_save.{hpp,cpp}` serialises both enums range-gated (backlog still lists BL-536 WORLD_SNAPSHOT_SAVE as designed — backlog lags code)
META_LAYER.md | two notions of era drifted apart (NR-333) | NO ITEM (kept in the doc as a design caveat on the `era` subject, not a hole)
META_LAYER.md | BL-155 four-family taxonomy named by nothing in code | BL-155 (LAW_POLICY_SURFACE_DESIGN)
PEOPLE.md | the person system does not exist | BL-547 (NAMED_PEOPLE_AND_ROLES, design-owed, v0.2.0)
PEOPLE.md | company head figure | BL-370 (CORP_LEADER_FIGURE, design-owed, v0.2.0)
PEOPLE.md | persona counsel "parked" | BL-207 (PERSONA_COUNSEL_PACKS, designed, v0.2.0)
PEOPLE.md | doctrine is an all-zero stub a commander would fill | BL-472 (UNIT_FORMATIONS, designed, v0.1.20) is the nearest owner; no item names the commander's doctrine row
EVENTS.md | the event system / trigger does not exist | BL-548 (EVENT_SYSTEM, design-owed, v0.2.0)
EVENTS.md | collapse_strain wired to nothing | BL-477 (ERA_COLLAPSE_DEFINES_META, design-owed, v0.1.11)
EVENTS.md | sky events deferred | BL-289 (sky events as extinction drivers, design-owed, v0.3.0)
EVENTS.md | interdiction ships silent (no notification) | BL-458 (SUPPLY_LINES_CANNOT_BE_CUT, designed, v0.1.15) — backlog still open though LOGISTICS.md says it shipped; no item owns the message itself
EVENTS.md | political systems have no notification layer | BL-548 (EVENT_SYSTEM)
MANUAL.md | campaign conflict layer "nothing in play commands it" | STALE for engagement (run_battles, battle card, Field dispatches); the contract loop is BL-377 (MERCENARY_CONTRACT_SEAM, designed, v0.1.15) and BL-315
MANUAL.md | a product name ("Io is a moon of Jupiter") | NO ITEM
MANUAL.md | ancient building roster at tile grain | BL-429 (ANCIENT_BUILDING_ROSTER, designed, v0.1.17)
MANUAL.md | save and load "no save format ships" | STALE — F5/F6 quick save/load in ACTIONS.json, world_save.cpp in code
MANUAL.md | sentiment-based diplomacy not in code | STALE — sentiment.cpp
MANUAL.md | inset still shows the circumplanetary view (BL-378) | AMBIGUOUS — BL-378 MINIMAP_BASE_RENDER is marked complete, but no trace of it in src/ or docs/ui/MINIMAP.md; manual now follows MINIMAP.md (zoom-ladder rung)
MANUAL.md | contracts "[DESIGNED]" | BL-377 (MERCENARY_CONTRACT_SEAM)
MANUAL.md | law/tech "surfaces" owed (authority pointed at BL-155/BL-156) | BL-155, BL-186 (LAWS_LEDGER_UI); tech viewer F9 exists (STALE for tech)

Summary: 33 with item / 7 NO ITEM / 13 STALE / 1 ambiguous.

## B

# Group B holes audit — PRODUCTION / MARKETS / FINANCE / RESOURCES

FINANCE.md | unit upkeep credit + goods rates authored 0.0 (kept in doc as shape-without-number) | BL-496 (ORDNANCE_RATE_GOES_LIVE)
FINANCE.md | storage caps / per-node throughput charges unowned | BL-464 (LOGISTIC_POINTS)
FINANCE.md | policy levers on the budget (taxes, budget laws) | BL-155 (LAW_POLICY_SURFACE_DESIGN) + BL-537 (NATIONAL_BUDGET)
FINANCE.md | "enactment by generation-seeded nation today; nation scorer later" | BL-542 (NATION_SCORER)
RESOURCES.md | habitability goods' undersupply effects (habitability / workforce efficiency / growth) not wired | NO ITEM (POPULATION.md design only)
RESOURCES.md | propellant carries no base_price (unpriced, pool-only) | NO ITEM (doc states it as design: made and burned, never sold)
RESOURCES.md | trade_goods_misc is a placeholder; a named luxury is "a separate design step" | NO ITEM
RESOURCES.md | Building materials / Utilities absent from resource_type | NO ITEM (by design — not resources)
RESOURCES.md | five removed logistics goods (grain, fodder, salt, transport capacity, bullion) "intended, not existing" | STALE (removed; record only — dropped)
RESOURCES.md | "distance is physical for now; geopolitical term later" | NO ITEM (ruling Ben 2026-07-22 kept as attribution)
MARKETS.md | buy-order book engine-only; no verb/press writes world::buy_orders | BL-160 (AUTO_EXCHANGE_POLICY) / BL-383 (REMOVE_DORMANT_BUY_SIDE)
MARKETS.md | pull_fraction 0.50 not re-tuned after counterpart change | BL-440 (MINES_ONLY_TARGET_THE_RICHEST — repricing pass, NR-277)
MARKETS.md | procurement pace fixed rather than market-gated (stretch/pause on supplier throughput) | NO ITEM
MARKETS.md | procurement has no UI / no AI user | BL-445 (PROCUREMENT_HAS_NO_UI) / BL-446 (SCORER_CANNOT_PROCURE)
MARKETS.md | ~80 background firms listed individually; no aggregation on Corporation lens/dashboard | NO ITEM (BL-523 CORP_KIND_AXIS covers corp_ai reading is_background, not the UI)
MARKETS.md | population-demand undersupply effects unwired | NO ITEM (same as RESOURCES hole)
MARKETS.md | "Markets are static; no runtime creation" | STALE (maybe_spawn_market, BL-263 landed)
MARKETS.md | "Anchored prices [0.25x, 4x]" | STALE (economy.lua ceil_mult = 10.0)
MARKETS.md | "Most of the enum cannot trade / BL-040 raws unsellable" | STALE (every value but propellant priced)
MARKETS.md | Owed follow-ons BL-130 / BL-131 / BL-132 listed open | STALE (all three status complete)
MARKETS.md | treasury "spent by nothing" | STALE (src/world/nation_budget.cpp, BL-537 landed)
MARKETS.md | inter-body trade zero (space lane refused by launchpad/propellant gates) | NO ITEM as a defect — design gate; BL-375 (TIME_TO_SPACE_PACING) is the nearest owner
MARKETS.md | trade_routes blind to intra-body trade (NR-289) | NO ITEM
PRODUCTION.md | recipe_switch no-dominance guard R1 fails on four sibling pairs (NR-243) | BL-430 (ALTERNATE_PRODUCTION_METHODS, still open) — kept in doc as Ben's open call
PRODUCTION.md | corp_ai dial_recipe not switch-cost/cooldown aware (NR-242) | NO ITEM (recorded as decision NR-242)
PRODUCTION.md | named ancient buildings (quarry, woodcutter, kiln, smithy) with placement rules + glyphs | BL-429 (ANCIENT_BUILDING_ROSTER, open)
PRODUCTION.md | sand and peat "still have no consumer" | STALE (glass ← sand, peat_charcoal ← peat)
PRODUCTION.md | "ancient roster deliberately thin; BL-429 next in Sprint 17" | STALE
PRODUCTION.md | "recipes.lua holds three recipes" | STALE (29 recipes)
PRODUCTION.md | building_type has six values | STALE (eight: + military_base, research_institute)
PRODUCTION.md | construction pacing reads derived supply, "real inventory revisited in BL-130" | STALE (reads/drains market_component.inventory)
PRODUCTION.md | stockpile flow text: [0.25x,4x] band, nation substrate supply | STALE (10x; substrate deleted by BL-365)
PRODUCTION.md | logistics/transport-capacity open note (storage cap, throughput, Warehouse, Storage Depot) | BL-464 (LOGISTIC_POINTS)
PRODUCTION.md | Orbital Port design target, no enum value | NO ITEM
PRODUCTION.md | Construction Yard / Power Plant unbuilt | NO ITEM (by design — no resource)
PRODUCTION.md | province ceiling moves during play — intended? | NR-406 (kept as open question); NO ITEM
PRODUCTION.md | workforce auto-solver fidelity (finer tiers, input-price response, hysteresis) | NO ITEM
PRODUCTION.md | non-extraction stacking "answered by BL-366" | STALE wording (landed; kept as design)
PRODUCTION.md | BL-107 save header "has not landed" | STALE (save streams carry headers)

Totals: 12 with item / 15 NO ITEM / 14 STALE (some lines double-count the same hole across docs).

## C

# Group C holes audit (TILES, LOGISTICS, SUPPLY, CONTRACTS, POPULATION, ERAS, SPACE_ASSETS)

TILES.md | Landform "build cost modifier" never wired to placement; only traversal cost (`landform_logistics_cost`) exists | NO ITEM (doc rewritten to say traversal; build-cost-by-landform is an open design call)
TILES.md | Kepler/wet-body 0% valley — Pass 5 uses absolute height < 0.35 instead of the Pass 4b percentile mask | NO ITEM (BL-051 tile-generation refinements is complete)
TILES.md | Biome-balance S2 failure (forest+wetland 2.41%) | STALE (re-measured 10.96% after 3x grid + BL-338)
TILES.md | Marine goods on water tiles "(deferred)" | NO ITEM
TILES.md | Edge features (cliffs/escarpments/coastline) as separate item | NO ITEM
TILES.md | Point features (volcano cone, oasis, spring, cave, waterfall) as separate item | NO ITEM
TILES.md | Salt cover carries no deposit "if one is restored" (salt removed NR-257) | NO ITEM (RESOURCES.md lists follow-on as unfiled)
TILES.md | Crater rim/floor distinction "possible later" | NO ITEM
TILES.md | Mountain relief +0.45 / glyph retired (stated as design) | BL-565 (MOUNTAINS_READ_AS_ELEVATION)
LOGISTICS.md | Throughput — network answers reach, never how much can move | BL-464 (LOGISTIC_POINTS)
LOGISTICS.md | Interdiction ships silent (no comms message, no ledger cause, no tile mark) | BL-458 (SUPPLY_LINES_CANNOT_BE_CUT) / BL-453 (CONVOYS_HAVE_NO_LEDGER); NR-407
LOGISTICS.md | No sea routes / port model | BL-188 (COASTAL_PORTS_SEA_TRADE)
LOGISTICS.md | Nothing generates a hub; rival scorer has no port/hub build candidate | BL-447 (SCORER_NEVER_DEMOLISHES_OR_ROADS) covers missing scorer verbs; hub candidate specifically NO ITEM beyond BL-464 finding 5
LOGISTICS.md | No convoy escort (unit assigned to convoy) | NO ITEM (excluded by BL-458 design)
LOGISTICS.md | Road maintenance / nation bankruptcy | BL-550 (NATIONAL_INSOLVENCY)
LOGISTICS.md | Friend immunity from interdiction | BL-549 (FRIENDSHIP_PERMITS_TWO_THINGS)
LOGISTICS.md | LP rates for both halves, which consumer lands first | BL-464 (LOGISTIC_POINTS)
SUPPLY.md | Per-node throughput capacity | BL-464 (LOGISTIC_POINTS)
SUPPLY.md | Port gating for sea mode (no port check; path picks mode) | BL-188 (COASTAL_PORTS_SEA_TRADE)
SUPPLY.md | Air mode / airfield building | NO ITEM
SUPPLY.md | Orbital Port at destination + Era 1 gate for space mode | BL-087 (ERA1_TECH_QUEST_SYSTEM) — space arc
SUPPLY.md | In-app dispatch form on market Selection card | NO ITEM (BL-452 complete; BL-453 ledger owns Hold press only)
SUPPLY.md | Space launches auto-dispatch; "should leaving the gravity well be explicit" | NO ITEM (space arc)
SUPPLY.md | Whole-route crosses_ocean bit misprices hauls | BL-522 (CROSSES_OCEAN_IS_A_WHOLE_ROUTE_BIT)
SUPPLY.md | supply_system.hpp header comment says air not dispatched | STALE as a hole (comment is true: air never dispatched)
SUPPLY.md | Railroad mode | BL-173 (RAILROAD_TRANSPORT_MODE)
SUPPLY.md | Convoys outside state_hash | NO ITEM (stated as deliberate design; R5 check)
CONTRACTS.md | Mercenary contract sell side has no code | BL-377 (MERCENARY_CONTRACT_SEAM)
CONTRACTS.md | Procurement has no UI | BL-445 (PROCUREMENT_HAS_NO_UI)
CONTRACTS.md | Procurement has no AI user | BL-446 (SCORER_CANNOT_PROCURE)
CONTRACTS.md | No contract-template Lua table | BL-377 (MERCENARY_CONTRACT_SEAM)
CONTRACTS.md | Reputation invisible on blackboard | BL-390 (THE_SEAM_HAS_NO_READ_BACK)
CONTRACTS.md | Nothing verifies an offer fires | BL-377 (MERCENARY_CONTRACT_SEAM) requirement
CONTRACTS.md | Reputation floor deadlock | BL-391 (REPUTATION_FLOOR_IS_A_DEADLOCK) / BL-546
CONTRACTS.md | Rivals bid for contracts | BL-551 (CONTRACT_BIDDING)
POPULATION.md | Land use field not attached (land_use_component declared, unused); population and extraction do not compete for tiles | NO ITEM
POPULATION.md | Agglomeration / scale production bonus has no code | NO ITEM
POPULATION.md | Real per-centre demand basket (clean water, consumer goods, habitability goods not in resource_type) | NO ITEM
POPULATION.md | Nation-seeded, level-derived centre generation (design) vs shipped before-nations weighted draw | STALE as written — code order is the settled design (ladder reads centres); doc overridden to code
POPULATION.md | Wage level derived from habitability/population pressure | BL-544 (UNIT_WAGE_REFERENCE) partially; corp wage level NO ITEM
POPULATION.md | Allocation priority policy under contention | NO ITEM (SYSTEMS.md § Policy)
POPULATION.md | Settlement names derived from host nation | NO ITEM
POPULATION.md | Region demography does not seed population-centre scale on graduation | NO ITEM (BL-273 complete; BL-271 sim owns graduation)
POPULATION.md | Recruitment cheaper from high-habitability body; research unlocks; conflict lowers habitability | NO ITEM
ERAS.md | No era state in code (no enum, gate, transition); launchpad operate-gate unimplemented | BL-087 (ERA1_TECH_QUEST_SYSTEM)
ERAS.md | Propellant / machinery have no resource_type; propellant threshold absent from Lua | NO ITEM (space arc)
ERAS.md | Era 0 / Era 1 named buildings are not building_type values | NO ITEM (space arc; BL-429 covers the ancient roster only)
ERAS.md | Era 2 | out of scope by design
SPACE_ASSETS.md | Whole design (asset types, launch path, orbital model, upkeep) | NO ITEM (space arc)

## D

# Group D — holes audit

NATIONS.md | no spend side; treasury only rises | STALE (run_national_budget / run_nation_step debit it)
NATIONS.md | nation scorer emits weights but nothing in the tick calls it; no nation verb | STALE (nation_step.cpp called from app.cpp/main.cpp)
NATIONS.md | treasury not folded into state_hash | STALE (world.cpp folds treasuries when non-zero, plus nation_budgets)
NATIONS.md | grudge is a substitute for Era −1 pair outcomes | BL-541 (DIRECTIONAL_TARIFFS)
NATIONS.md | scorer does not read ideology; pair sentiment owed to substrate | BL-545 (SENTIMENT_SUBSTRATE)
NATIONS.md | no authoring path for any law after generation | BL-541 (DIRECTIONAL_TARIFFS), BL-539 (LOBBYING)
NATIONS.md | import tariff unreachable in play — nothing enacts one (NR-400) | BL-541 (DIRECTIONAL_TARIFFS)
NATIONS.md | one effect family of BL-155's four | BL-155 (LAW_POLICY_SURFACE_DESIGN)
NATIONS.md | no military effect of any law | BL-399 (COMPANY_ANSWERABILITY)
NATIONS.md | no answerability (which law classes bind the company) | BL-399 (COMPANY_ANSWERABILITY)
NATIONS.md | no client for the mercenary loop | BL-538 (TREASURY_PRIORITY_LINES), BL-377 (MERCENARY_CONTRACT_SEAM)
NATIONS.md | what each budget line buys (only public_exploration has a consumer) | BL-538 (TREASURY_PRIORITY_LINES)
NATIONS.md | lobby verb does not exist | BL-539 (LOBBYING)
NATIONS.md | nation→corp Access/Trust has no emitter | BL-540 (NATION_TO_CORP_STANCE)
NATIONS.md | unit wage unanchored | BL-544 (UNIT_WAGE_REFERENCE)
NATIONS.md | BL-480 shipped but reads designed (NR-514) | NO ITEM (bookkeeping; BL-480 still open as `designed`)
NATIONS.md | Laws ledger browse-only surface | BL-186 (LAWS_LEDGER_UI)
RELATIONS.md | no rival ever declares anything | BL-450 (RIVALS_CANNOT_REASON_ABOUT_STANCE)
RELATIONS.md | friendship permits nothing | BL-549 (FRIENDSHIP_PERMITS_TWO_THINGS)
RELATIONS.md | no sentiment layer | STALE (sentiment.{hpp,cpp} exists)
RELATIONS.md | no nation in the relational layer | BL-540 (NATION_TO_CORP_STANCE)
RELATIONS.md | reputation / floor invisible; no read-back | BL-390 (THE_SEAM_HAS_NO_READ_BACK)
RELATIONS.md | stance has no surface | BL-449 (STANCE_NEEDS_A_SURFACE)
RELATIONS.md | nothing survives a save | STALE (world_save.cpp carries stance, sentiment, embargo)
RELATIONS.md | decay authored nowhere | STALE (economy.lua economy.sentiment)
RELATIONS.md | every factor weight but the two procurement ones is zero | BL-539 (lobbied_against), BL-524 (equity_taken), BL-540 (levy/embargo); trade_conducted / hostility_declared / friendship_accepted / force_used — NO ITEM
RELATIONS.md | embargo has no author | BL-399 (COMPANY_ANSWERABILITY), BL-161 (COUNTERPARTY_ALLOW_DENY)
RELATIONS.md | standing lacks production axis | BL-262 (SCORING_SYSTEM)
RELATIONS.md | rival-vs-rival declarations visibility | NO ITEM (kept as open question; BL-068 complete)
RELATIONS.md | reputation deadlock in play | STALE (decay authored; BL-391 item still open as designed)
HISTORY.md | non-hegemony asserted, not enforced | BL-224 (NON_HEGEMONY_INVARIANT_ASSERTED)
HISTORY.md | Stages 5–6 superseded; replacement owed | BL-223 (AVERTED_RUPTURE_DIPLOMACY_ORIGIN)
HISTORY.md | rupture placement disagrees across three docs | BL-223 (AVERTED_RUPTURE_DIPLOMACY_ORIGIN)
HISTORY.md | asset-seizure sentiment/credit cost | BL-225 (SEIZURE_SENTIMENT_CREDIT_COST), BL-223
HISTORY.md | Stage 2 hegemon branch never reached in 12-seed spread | NO ITEM (BL-219 sweep complete; BL-224 is the nearest open owner)
HISTORY.md | river connectivity / domesticable clades substituted in agrarian_score | STALE (BL-170, BL-217 complete)
HISTORY.md | dates/thresholds uncalibrated; works magnitudes authored by judgement | NO ITEM (BL-275 sweep complete; magnitudes kept as flat fact)
HISTORY.md | Era −1 sim cost comments disagree (~110 ms vs ~23 s) | BL-320 (ERA1_SIM_PERF), BL-462 (HARNESSES_MEASURE_A_DIFFERENT_SIM)
HISTORY.md | run is 400 years one band, target is 4000-year ladder | BL-494 (FOUR_THOUSAND_YEAR_LADDER)
HISTORY.md | BL-222 industrial ladder substantially overtaken | BL-222 (HISTORY_LADDER_INDUSTRIAL) still open
HISTORY.md | Charter Age line → player-corp linkage | NO ITEM (kept as open question)
HISTORY.md | rupture event text (Era 0 → Era 1) | BL-223
CREEDS.md | no live religion mechanic, no player surface | BL-487 (POLITY_CREED_AXES), BL-300 (ERA_MINUS1_MYTH_THEOLOGY)
CREEDS.md | 1951 globalisation vs averted-rupture bloc structure | BL-223 (AVERTED_RUPTURE_DIPLOMACY_ORIGIN)
CREEDS.md | tribal marches are scalar comparisons (abstract war) | NO ITEM (BL-272 complete; cradle-grain welding kept scalar, stated as fact)
CREEDS.md | weld frequency tuning window | NO ITEM (BL-219 sweep complete)
COLLAPSE.md | strain accumulator / fragmentation / hegemony measure / Release / readable strain / transfer / reach-fed strain | BL-504..BL-510 (all open)
COLLAPSE.md | E4/E5/E6/E7 culminations | BL-483, BL-484, BL-485, BL-486 (all open)
COLLAPSE.md | creed axes, strategy weightings, narration bank, attractor sweep | BL-487, BL-488, BL-489, BL-490 (all open)
COLLAPSE.md | perf ladder rungs | BL-320, BL-491, BL-492, BL-493, BL-494 (all open)
COLLAPSE.md | "the optimisation is not deferred to BL-320 alone" | BL-494 (FOUR_THOUSAND_YEAR_LADDER)

## E1

# E1 holes audit — PLANETOLOGY / TILE_GENERATION / CONTINENTS / GENERATION_LEDGER

CONTINENTS.md | Rift clusters are not seeded along divergent plate boundaries | NO ITEM (BL-210 oral-history pivot names the fuller S1–S4 sim, not this specifically)
CONTINENTS.md | Volcanic activity is not concentrated along plate boundaries | NO ITEM
CONTINENTS.md | continents.hpp names GENERATION_STRATEGY.md as its authority | STALE (header already cites CONTINENTS.md)
CONTINENTS.md | Only the homeworld receives the convergent mask; other bodies use the height path | NO ITEM (kept as flat fact)
GENERATION_LEDGER.md | Field lenses (height / moisture / band) not built | BL-304 (GENERATION_FIELD_OVERLAY_LENSES)
GENERATION_LEDGER.md | Cluster seed positions not captured in generation_record | NO ITEM (kept as open design question)
GENERATION_LEDGER.md | History slot views not exploration-gated | BL-211 (PLAYER_FACING_HISTORY_LEDGER)
GENERATION_LEDGER.md | Hover card / Selection element do not yet wrap draw_tile_derivation (only the ledger calls it) | BL-211 (PLAYER_FACING_HISTORY_LEDGER) / BL-503 (ENTITY_BUILDERS_CONVERGE)
GENERATION_LEDGER.md | generation_record does not attribute continent bias or Pass 6 post-multiplies | NO ITEM (kept as flat fact)
TILE_GENERATION.md | Smooth band transitions (noise-weighted blending at band edges) | NO ITEM
TILE_GENERATION.md | Archipelagos and large lakes not produced by any pass | NO ITEM
TILE_GENERATION.md | Additional body types (world_params::body_count beyond the prototype set) | NO ITEM
TILE_GENERATION.md | Which province domain a lake belongs to (open question for Ben) | NO ITEM (kept as open question)
TILE_GENERATION.md | tile_borders_river has no farming-predicate reader outside river_generation | NO ITEM
TILE_GENERATION.md | Road on-canvas rendering "BL-147 follow-on" | STALE (body_surface_canvas.cpp renders road_level)
PLANETOLOGY.md | GOE gate reads present-day theta, not its own epoch | BL-301 (GOE_EPOCH_RELATIVE_CALIBRATION)
PLANETOLOGY.md | Resource-list expansion (limestone, bauxite, salt/potash, phosphate, uranium) | NO ITEM (kept as open call)
PLANETOLOGY.md | Biography survey-gated or always visible | BL-211 (PLAYER_FACING_HISTORY_LEDGER) (kept as open call)
PLANETOLOGY.md | Report surface: Tile/Generation Ledger section vs dedicated Planetology ledger | STALE (History slot Story view + Generation Ledger profile echo exist)
PLANETOLOGY.md | Fire threshold: hard gate or soft penalty | NO ITEM (kept as open call)
PLANETOLOGY.md | Corporation generation reads nothing from the planetology chain | BL-210 (GENERATION_ORAL_HISTORY_PIVOT)
PLANETOLOGY.md | Cradle iron-poorer than Mat World — not balance-tested against the economy | NO ITEM
PLANETOLOGY.md | chain_stage has no stable wire mapping for a serialiser | BL-536 (WORLD_SNAPSHOT_SAVE) owns the save seam; no item names the enum
PLANETOLOGY.md | Clay generated on airless Selene ("live bug") | STALE (sedimentary endowment zeroes clay without water history; harness R3 asserts it)
PLANETOLOGY.md | Per-lean cost figures stale, re-run earthlike_lean_trace | STALE (instrument exists; numbers removed, instrument named)
PLANETOLOGY.md | Six archetypes / S5–S8 ladder unreachable until body_count grows | NO ITEM (accepted by ruling, Ben 2026-07-21)
PLANETOLOGY.md | Homeworld corridor width unmeasured | STALE (earthlike_corridor measures it)
PLANETOLOGY.md | Open call: ore provinces vs global scalars | STALE (ore-province field in Pass 6)

## E2

# E2 holes audit — NATION_GENERATION / CORPORATION_GENERATION / GENERATION_STRATEGY / PROVINCES

PROVINCES.md | No naval model; sea provinces are addressable empty space | BL-188 (COASTAL_PORTS_SEA_TRADE) | (Ben deferred 2026-08-22; kept in doc as design fact)
PROVINCES.md | Nothing bounds "rare" over-12 share; harness reports, never asserts | NO ITEM | deliberate — Ben's judgement against a number; kept as design
PROVINCES.md | 12% of provinces below the 3-tile floor | NO ITEM | ruled accepted 2026-08-22 (NR-433 closed); kept as design fact
PROVINCES.md | "No serialisation" of the partition | STALE (code already does it) | world.hpp: partition is the trailing section of the history-log stream
PROVINCES.md | Per-province firm cap inert (103 capacity vs 24 slots); ceiling should drop to where it bites + tech relieves it | BL-513 (PROVINCE_BUILDING_LIMIT) is COMPLETE; no open item carries Ben's 2026-08-22 "drop it to where it bites / tech deepens" ruling | NO ITEM for the ruling
PROVINCES.md | Straddling provinces / which nation owns a province | BL-563 (PROVINCE_RESPECTS_NATION) + BL-567 (PROVINCE_IS_THE_CONQUEST_UNIT) | ruled co-generate; doc now states it as design
PROVINCES.md | Modifier subject for the ceiling "does not exist yet" | NO ITEM | META_LAYER's stopping-condition rule; BL-513 closed without naming one
GENERATION_STRATEGY.md | body_count knob reserved (bodies are hand-authored profiles) | NO ITEM | BL-114 complete; no follow-on filed
GENERATION_STRATEGY.md | Full S1–S4 continents simulation replacing noise machinery | BL-210 (GENERATION_ORAL_HISTORY_PIVOT) |
GENERATION_STRATEGY.md | Planetology/ladder → corporation generation half of the connection | BL-210 (GENERATION_ORAL_HISTORY_PIVOT) |
GENERATION_STRATEGY.md | Later ladder stages / industrialisation | BL-222 (HISTORY_LADDER_INDUSTRIAL), BL-223 (AVERTED_RUPTURE_DIPLOMACY_ORIGIN) |
GENERATION_STRATEGY.md | Owed surfaces: tile-derivation ledger, region surface, market-carve explanation, background-firm live surface | NO ITEM | kept in doc's visibility table by design ("promote from this table")
NATION_GENERATION.md | No nation_ai / nation takes no autonomous action | STALE (code already does it) | src/world/nation_ai.{hpp,cpp} exists; BL-542 (NATION_SCORER) open for the scorer
NATION_GENERATION.md | min_nation_tiles doc-comment quotes pre-ladder 17–21 | STALE (code already does it) | nation_generation.hpp comment now says 43
NATION_GENERATION.md | substrate_density written by Pass 6, read by nothing (Industry lens reads background firms directly) | NO ITEM | dead field; doc now states it flatly
NATION_GENERATION.md | Full historical fragmentation — disputed zones, contested tiles | BL-518 (WAR_REDRAWS_BORDERS) / BL-567 | BL-054 is complete; nearest owners
NATION_GENERATION.md | Generation-time sentiment between nations | BL-545 (SENTIMENT_SUBSTRATE) |
NATION_GENERATION.md | Era-based reform unscoped | NO ITEM | open item retained in doc
CORPORATION_GENERATION.md | Corporate-border operate gate, origin gate, branch offices, cost-field range, tall/wide curve | BL-182 (CORPORATE_BORDERS), BL-364 (CORP_BORDER_HEXES) |
CORPORATION_GENERATION.md | Corp-selection analytical profile + re-roll verbs | NO ITEM | BL-435 complete; no follow-on found (grep "re-roll", "wizard")
CORPORATION_GENERATION.md | Processing earns less than extraction | BL-436 (PROCESSING_UNDEREARNS_EXTRACTION) |
CORPORATION_GENERATION.md | Nation-seeded privatisation (runtime + generation-time) | NO ITEM | grep "privatisation" empty; nearest is BL-537 budget lines
CORPORATION_GENERATION.md | Tax spend unresolved | BL-537 (NATIONAL_BUDGET) |
CORPORATION_GENERATION.md | Franchise generation | NO ITEM | grep "franchise" empty; open item retained in doc
CORPORATION_GENERATION.md | Diplomatic posture / corp sentiment | BL-545 (SENTIMENT_SUBSTRATE) |
CORPORATION_GENERATION.md | Pre-game ticks "currently 12" | STALE (code already does it) | app.hpp pre_game_ticks = 80; doc corrected

## F

# Group F holes audit

Format: `<doc> | <hole> | BL-nnn (short_name) | NO ITEM | STALE (code already does it)`

## MULTIPLAYER_PRINCIPLES.md
MULTIPLAYER | save/load "planned, not built", no save path in world/* | STALE (world_save.cpp IOSV + core/save_game.cpp IOSG exist; save_roundtrip harness)
MULTIPLAYER | only history_log serialises, schema otherwise unversioned | STALE (IOSV/IOSG/IOHL/IOOB/IOPC all carry magic+version)
MULTIPLAYER | corp_command "eight verbs" | STALE (append-only enum, 25 verbs; rewritten as "append-only corp_verb enum")
MULTIPLAYER | netcode layer absent | NO ITEM (out of scope by the doc's own non-binding status; kept as "later work")
MULTIPLAYER | state hash vs snapshot "want reconciling when the save model lands" | NO ITEM (rewritten as a design rule: weigh any new field for both)

## TECH_FOUNDATIONS.md
TECH_FOUNDATIONS | combat resolution out of scope / campaign combat unreachable | STALE (run_battles calls resolve_campaign_battle in the economy tick)
TECH_FOUNDATIONS | terrain_combat has no consumer; history ladder prices conquest via is_barrier | STALE (BL-233 complete; combat.cpp, battle_system.cpp, history_sim.cpp and history_ladder.cpp all read terrain_defence/attrition)
TECH_FOUNDATIONS | unit upkeep rates authored 0.0 (kept in-doc as shape-without-number) | BL-496 (ORDNANCE_RATE_GOES_LIVE)
TECH_FOUNDATIONS | doctrine all-zero stub / season hardcoded / reinforcement / holding the field no consequence (pointer to MILITARY.md's absent list removed) | BL-472 (UNIT_FORMATIONS) for doctrine; BL-567 (PROVINCE_IS_THE_CONQUEST_UNIT) for field consequence; NO ITEM for season and reinforcement (MILITARY.md owns them as shape-without-number)
TECH_FOUNDATIONS | no equipment system (kept as an explicit scope exclusion) | NO ITEM (design boundary, by instruction)
TECH_FOUNDATIONS | no unit transport infrastructure (kept as scope exclusion) | NO ITEM (design boundary, by instruction)
TECH_FOUNDATIONS | sentiment tracking and diplomacy unbuilt, data-model comments only | BL-545 (SENTIMENT_SUBSTRATE)
TECH_FOUNDATIONS | per-econ-tick autosnapshot "never built" | STALE by decision (saves are discrete; stated as the design)
TECH_FOUNDATIONS | CI removed; headless tier the only guard (kept as "with no CI") | NO ITEM
TECH_FOUNDATIONS | save migration / forward compatibility deferred (kept as a deliberate decision) | NO ITEM (rejection is the migration story, by decision)
TECH_FOUNDATIONS | SQLite deferred (kept as deliberate decision) | NO ITEM

## AI_OPPONENT.md
AI_OPPONENT | processors realise −6..−12/tick vs predicted ~0; three of five seeds insolvent; ai_skill_harness left red | BL-436 (PROCESSING_UNDEREARNS_EXTRACTION)
AI_OPPONENT | build score is net²/capex; explicit linear metric + re-tune "step 2 still open" | BL-417 (AI_BUILD_SCORE_IS_QUADRATIC)
AI_OPPONENT | scorer scores no demolish / place_road candidates (table implied it did) | BL-447 (SCORER_NEVER_DEMOLISHES_OR_ROADS)
AI_OPPONENT | scorer scores no stance verbs | BL-450 (RIVALS_CANNOT_REASON_ABOUT_STANCE)
AI_OPPONENT | scorer scores no convoy / procurement / unit / withdrawal candidates | NO ITEM (seam-reachable by an agent; stated as such)
AI_OPPONENT | base_price is a rarity floor not a production cost — AI can sell at a loss | BL-385 (BLACKBOARD_EXPORTS_NO_REFERENCE_PRICE)
AI_OPPONENT | order book one-sided: buy_order has state + save format but no verb | BL-383 (REMOVE_DORMANT_BUY_SIDE)
AI_OPPONENT | real trading strategy (price trend, timed release, targeting a rival's shortage) "later work" | NO ITEM
AI_OPPONENT | set_exchange_policy / set_counterparty "once landed" | BL-160 (AUTO_EXCHANGE_POLICY), BL-161 (COUNTERPARTY_ALLOW_DENY)
AI_OPPONENT | player chat message box has no mechanical effect | BL-334 (STAGE_C_DIALOGUE_LAYER) — the hook it consumes; no dedicated item
AI_OPPONENT | Stage C in-character model chat | BL-334 (STAGE_C_DIALOGUE_LAYER)
AI_OPPONENT | goal layer above the scorer | BL-336 (GOAL_LAYER_MYOPIA_MITIGATION)
AI_OPPONENT | Stage C ship order vs v0.2.0 and model size/quantisation unsettled | BL-334 (STAGE_C_DIALOGUE_LAYER)
AI_OPPONENT | ~300-token decision estimate unmeasured | BL-335 (MEASURE_DECISION_TOKEN_COST) complete; follow-on BL-481 (BLACKBOARD_COMPACT_ENCODING)
AI_OPPONENT | MCP prompts/* not exposed (reason_to_select served via tools only) | NO ITEM (stated as the design)
AI_OPPONENT | spectate: player presses not disabled (BL-409 R4) | BL-413 (SPECTATE_DISABLE_PLAYER_PRESSES)
AI_OPPONENT | spectate: viewpoint inherits the seed-0 insolvent player corp | BL-418 (SPECTATE_VIEWPOINT_DEFAULT)
AI_OPPONENT | history log: save-game menu/UI not built | STALE (F5/F6 quick save, --load, save_game.cpp)
AI_OPPONENT | history log: BL-218/BL-219 "expected to write into this log" | STALE (both complete)
AI_OPPONENT | CivBench table row conflates two projects, "should be split when next touched" | STALE (split in this rewrite)
AI_OPPONENT | Paradox per-tick AI CPU budget unpublished | NO ITEM (moot — model is out of process; stated so)
AI_OPPONENT | five --serve protocol defects (unparsed verb args, four collapsed decline codes, Linux spawn, survey never advanced, unnameable body) | STALE (all fixed; BODIES opcode, list_bodies, agent_protocol.cpp shared parser)
AI_OPPONENT | idle/resume oscillation, one-way workforce dial | STALE (fixed; stated as the design rules: both tiers set cooldown, one estimator, solver-reported gain)
AI_OPPONENT | runner_up "next candidate in sort order" | STALE (now best rejected candidate, NR-232; doc updated)
AI_OPPONENT | trade_floor_multiple "at 1.0 refuses to sell below base" | STALE (authored 0.25 = price-band floor; doc updated from corp_ai.hpp)

## STRATEGIES.md
STRATEGIES | military card family unenumerated "until BL-157 units is mapped" | NO ITEM (units exist; no military card seeded — stated as reserved until the blackboard carries military tells)
STRATEGIES | strategies.json / STRATEGY_INDEX.json / strategy_query.js / lint data store | NO ITEM (doc says an item is minted when the loop mints a deck)
STRATEGIES | weights line does not compile into corp_ai | NO ITEM (Ben's ruling: uncoupled until the loop validates the deck)
STRATEGIES | deed facts on the blackboard | BL-309 (DEED_HISTORY_LOG_LINES) / BL-087 (ERA1_TECH_QUEST_SYSTEM)
STRATEGIES | launch cadence / count signal | NO ITEM
STRATEGIES | alarm_own / alarm_total / ceiling (danger model observables) | BL-223 (AVERTED_RUPTURE_DIPLOMACY_ORIGIN) — nearest owner; no blackboard item
STRATEGIES | rupture countdown observable | NO ITEM
STRATEGIES | tech_unlocked / doctrine_taken on the blackboard | BL-156 (TECH_SYSTEM_EARLY_DESIGN) — nearest owner
STRATEGIES | space-market supply/demand predicate (ST-09 unwritable) | NO ITEM
STRATEGIES | import-dependence / interdependence reads | NO ITEM (BL-088 persistent trade routes is complete; no blackboard read item)
STRATEGIES | unrest signal | NO ITEM (POPULATION.md arc)
STRATEGIES | Era 1 tree draft "not yet reviewed by Ben" | NO ITEM (research doc status, dropped)

## LANGUAGE_POLICY_FEASIBILITY.md
LANGUAGE_POLICY | "awaiting review" banner | STALE (ruled NR-094, adopted — AI_OPPONENT § 10g)
LANGUAGE_POLICY | eight verbs / seven result codes / 115 controls / six tools | STALE (updated to the append-only enum, full result set, seven tools)
LANGUAGE_POLICY | N rivals and target-machine spec unknown (kept as threats to validity) | NO ITEM (research-note caveat)
LANGUAGE_POLICY | Cicero transfer without a human dialogue corpus unproven (kept) | BL-334 (STAGE_C_DIALOGUE_LAYER)

## G

# Group G holes audit — SELECTION.md, LENSES.md, LAYOUT.md

Format: `<doc> | <hole> | BL-nnn (short_name) | NO ITEM | STALE`

SELECTION.md | Tile card History button has no surface behind it (drawn disabled) | NO ITEM
SELECTION.md | Tile card Supply button not wired (drawn disabled; supply routing is the lens's) | NO ITEM
SELECTION.md | Habitability/Hazard charts have no per-tile time-series history (not drillable) | NO ITEM
SELECTION.md | Per-tile pollution and population not modelled, so no tile view for them | NO ITEM (POPULATION.md owns the design)
SELECTION.md | Building card: no per-building profit history — "Net, 6 mo." is a placeholder series (NR-249) | NO ITEM
SELECTION.md | Building card: no revenue sub-breakdown to chart (NR-248) | NO ITEM
SELECTION.md | Building card: three reserved action slots | NO ITEM (by design)
SELECTION.md | Unit card: no unit-type-keyed glyph vocabulary (humanoid placeholder) | BL-471 (UNIT_MARKER_AND_COMMAND_SURFACE) owns the marker; catalogue move is BL-502 (GLYPHS_JOIN_THE_CATALOGUE)
SELECTION.md | Unit card: five reserved action slots (March/Halt/Disband presses) | BL-471 (UNIT_MARKER_AND_COMMAND_SURFACE)
SELECTION.md | `unit_component::strength` documented as fixed-point but written raw everywhere | NO ITEM (NR logged at the time)
SELECTION.md | Market / Nation / Corporation facts column empty ("stubbed") | NO ITEM
SELECTION.md | Tile 'go to' is a no-op (pan-to-tile out of scope) | NO ITEM
SELECTION.md | Lens-driven hover/selection resolution not wired — a click resolves identically under every lens | BL-372 (LENS_KEYED_SELECTION)
SELECTION.md | Canvas hit-testing for buildings/units/markets "not yet independently click-selectable" | STALE (resolve_marker_hit hit-tests building + market_centre; units reached via the repeat-click Soldier rung)
SELECTION.md | Battle card: whether rival aggregate strength should be redacted | NR-469 open, NO ITEM
SELECTION.md | Selection kind icon is a first pass ahead of a richer per-entity icon | NO ITEM
SELECTION.md | Multi-select | NO ITEM (out of scope by design)
LENSES.md | Corporation lens has no per-corp colour key (now stated as its legend) | NO ITEM
LENSES.md | Reach lens Solar connected-body glow | NO ITEM
LENSES.md | Supply-routes lens Solar aggregated graph edges | NO ITEM
LENSES.md | Production lens Circumplanetary per-body output badge | NO ITEM
LENSES.md | Scarcity lens Circumplanetary per-body shortfall badge | NO ITEM
LENSES.md | Supply lens throughput scale-key (line weight -> goods/tick) | NO ITEM (BL-175 SUPPLY_LENS_FLOW_LEGIBILITY covers the Planetary arrow field, not the key)
LENSES.md | Supply lens Planetary flow read (arrows on traversed tiles, not uniform glyph) | BL-175 (SUPPLY_LENS_FLOW_LEGIBILITY)
LENSES.md | Reach lens reuses icons::convoy — dedicated glyph TODO | NO ITEM
LENSES.md | Corporate reach gameplay mechanic (range gates operations, multi-HQ, tall/wide) | BL-182 (CORPORATE_BORDERS)
LENSES.md | Industry lens goldens need re-blessing after the field re-point | STALE per golden policy (no bulk re-bless; captures elsewhere) — dropped
LENSES.md | Opportunity "margin formula is a first-cut estimate; refine when build-cost amortisation lands" | STALE (lens reads demand-gap rank, not a margin estimate)
LENSES.md | Per-tile market variation "once multiple centres are seeded" | STALE (markets are tile-centred with catchments; BL-036 landed)
LENSES.md | Stale code comments "w.convoys is empty until dispatch lands" | NO ITEM (comment hygiene)
LENSES.md | Continent key sits in the corner the Selection band occupies (foreground-list workaround) | BL-401 (CONTINENT_KEY_CORNER_MOVE)
LENSES.md | Legends inside the minimap box / minimap to screen edge (successor to current placement) | BL-566 (LEGEND_INSIDE_MINIMAP)
LENSES.md | Country lens retired in favour of a national hue on the base map | BL-535 (NATIONAL_HUE_REPLACES_COUNTRY_LENS)
LAYOUT.md | Pinned-items panel: rect exists (`pinned_panel_rect`), panel/pin toggle/pin glyph not drawn; `PINS.md` spec does not exist | BL-216 (CHAT_BOTTOM_LEFT_PINNED_RIGHT) is marked complete — its pinned-items slice has NO open ITEM
LAYOUT.md | Canvases not inset clear of the chrome; zoom-ladder canvases not migrated onto `canvas_rect()` | NO ITEM
LAYOUT.md | Context menus and richer confirmation dialogs (beyond system menu / Dismantle / Withdraw confirms) | NO ITEM
LAYOUT.md | Takeover entry/exit transition unsettled (instant) | NO ITEM (feel question, by design)
LAYOUT.md | Comms dock has no per-channel mute (NR-471) | NO ITEM
LAYOUT.md | `ledger_chrome.hpp` floating-window constants exist with zero callers | NO ITEM (dead code; dropped from doc)
LAYOUT.md | Text-wrap render audit follow-through | BL-215 (TEXT_WRAP_RENDER_AUDIT) complete — STALE as a hole
LAYOUT.md | Selection band "close x button hides the band" | STALE (no close button; band always open — LAYOUT corrected to match SELECTION/code)
LAYOUT.md | Selection band "player building offers Manage building" | STALE (Manage removed; building card has Mothball/Dismantle/Auto)

## I

# Group I — holes audit

Format: `<doc> | <hole> | owner`

balance.md | Trend view has no renderer (player_timeseries time series) | STALE (draw_profit_chart renders a 12-tick profit series; BL-171 complete)
balance.md | Runway not on the Budget surface | STALE (header carries it; BL-177 complete — doc now says so)
balance.md | Corporation combo / per-corp budget absent | STALE by ruling (BL-142 player-only, complete)
balance.md | Policy levers (tax / wage tier) are UI state with no mechanics — panel prints "Policy levers - not yet wired (BL-155)" | BL-155 (LAW_POLICY_SURFACE_DESIGN), BL-280 (NEGOTIATED_TAX_RATE)
balance.md | Assets is a bare count (no by-type / by-body roll-up) | NO ITEM (kept as an open question for Ben in the doc)
balance.md | starting_capital = 0 placeholder in mock corporations.csv | NO ITEM (mockdata exporter gap; mockdata README out of scope)
construction.md | Queue table never lists in-progress builds — draw_queue_section hardcodes any_items = false although building_component carries ticks_remaining / construction_progress; panel always reads "No active construction." | NO ITEM (BL-029 is not in backlog.json; BL-143 / BL-162 complete but neither wired the table)
construction.md | Lens not armed on open (opportunity / production proposal) | NO ITEM (proposal, kept as open question)
construction.md | buildings.csv lacks workforce_target / recipe / decommissioned / maintenance / progress columns | NO ITEM (mockdata exporter gap)
construction.md | No per-tile opportunity / production margin export for a lens mock | NO ITEM (mockdata exporter gap)
construction.md | `exhausted` exported but not surfaced in the panel | STALE as stated (the panel no longer carries a Manage roster; deposit exhaustion is a Selection/canvas concern — BL-438 CO_EXTRACTION_IS_INVISIBLE is the nearest open item)
corporation.md | Slot-8 table shows exact rival balances (visibility violation) | STALE (table is banded Reach / Capital / Share; BL-262 first slice landed)
corporation.md | Net/tick direction column not read by the panel | NO ITEM (proposal, open question)
corporation.md | No per-rival balance time series for a sparkline | NO ITEM (and visibility rule would band it anyway)
corporation.md | Long-run fate of the table under the scoring system | BL-262 (SCORING_SYSTEM), BL-475 (CORP_LEDGER_STANCE_DETAIL)
economy.md | Panel has no rail slot | STALE (slot 3 Workforce hosts it; BL-292 complete)
economy.md | Sector composition accessor over w.buildings does not exist | NO ITEM (proposal; the view itself is an open question)
economy.md | Holdings view leaks every corp's pools | BL-482 (ECONOMY_PANEL_POOLS_LEAK)
economy.md | Corps view prints exact rival balances (same visibility violation Corporation fixed) | NO ITEM (BL-482 covers pools only; the balance list is not named by any open item)
economy.md | Net worth / asset valuation does not exist | BL-527 (CORP_VALUATION)
market.md | Markets (per-body turnover) and Trends (resource combo) views not built | NO ITEM (kept as proposals; Prices already carries per-good sparklines)
market.md | markets.csv body_id join inflation | STALE (exporter emits market_id; doc said closed)
selection.md | BL-123 re-layout of the action|facts split for the column | STALE (BL-123 complete; tile/building/unit/province/battle have the 3-column card)
selection.md | Market / nation / corporation facts are empty | NO ITEM for market and nation; BL-475 (CORP_LEDGER_STANCE_DETAIL) for corporation
selection.md | Building / unit / market canvas hit-testing | BL-372 (LENS_KEYED_SELECTION) — kept as an open question
selection.md | Lens armed on selection | NO ITEM (open question A/B, lean A = never)
tile_ledger.md | Market section duplicated inside the Tiles view | STALE (Tiles view retired; BL-281 complete)
tile_ledger.md | No tiles.csv / history export for a Power BI mock | NO ITEM (mockdata exporter gap)
tile_ledger.md | Lens not armed on open | NO ITEM as a History item; BL-304 (GENERATION_FIELD_OVERLAY_LENSES) is the candidate pairing
tile_ledger.md | Floating-window / no-nav_button banner | STALE (docked, three nav_button views; BL-211 complete)
TECH_EFFECTS.md | Units "none" / unit effects unbuilt | STALE (unit_component + unit_roster_table exist; roster turnover owned by BL-274 ERA_KEYED_UNIT_ROSTERS)
TECH_EFFECTS.md | Laws unbuilt | STALE (world::laws, law_effect_kind exist; surface owned by BL-155 / BL-186)
TECH_EFFECTS.md | Negotiated tax rate unbuilt | BL-280 (NEGOTIATED_TAX_RATE)
TECH_EFFECTS.md | Propellant has no resource_type value | STALE (propellant recipes exist in scripts/recipes.lua)
TECH_EFFECTS.md | Closed modifier-target vocabulary does not exist | BL-156 (TECH_SYSTEM_EARLY_DESIGN) — kept as open question 4, pointing at modifier_set
TECH_EFFECTS.md | Effect vocabulary shared between ladder store and Lua tree | BL-087 (ERA1_TECH_QUEST_SYSTEM) / BL-156 — open question 1 kept
ANCIENT_TECH_LADDER.md | Works Doctrine charter gate not worked out | BL-311 (WORKS_DOCTRINE_CHARTER_GATE)
ANCIENT_TECH_LADDER.md | Industrial region surface in the F9 viewer (NR-064) | NO ITEM (NR-064 open; kept as open question 6)
ANCIENT_TECH_LADDER.md | T5/T6 Energy crossing two vertices (Machine-Age pass) | NO ITEM (kept as open question 8)
ERA1_TECH_LANDSCAPE.md | Ceiling vs pairwise Alarm reduction unspecified | NO ITEM (kept as an open note inside the doc)
ERA1_TECH_LANDSCAPE.md | Full quest-based tree / ERAS ↔ ROADMAP reconciliation | BL-087 (ERA1_TECH_QUEST_SYSTEM)

Summary: 14 with item / 17 NO ITEM / 13 STALE (44 lines; some holes map to both an item and a kept open question).

## military

# MILITARY.md holes audit (2026-08-23)

Every named hole from the removed sections "What is absent, and known to be" and
"Build status § Outstanding", checked against `backlog_query.js --grep`.

| Hole | Owning item | Folded into |
|---|---|---|
| Doctrine is an all-zero stub (no per-corp doctrine field) | BL-472 UNIT_FORMATIONS (a formation carries a doctrine row — "finally gives doctrine_row its campaign consumer") | § Battles |
| Season hardcoded to summer | NO ITEM (`--grep season` only hits BL-377, unrelated) | § Battles, stated as fact |
| Battle membership snapshots at open / no reinforcement | NO ITEM (`--grep reinforce` nothing) | § Battles, stated as fact |
| Holding the field has no consequence / no territorial control | BL-567 PROVINCE_IS_THE_CONQUEST_UNIT (design-owed; forward constraint on unbuilt conquest) | § Battles |
| No upkeep *rate* (all 0.0) / authoring the numbers | BL-496 ORDNANCE_RATE_GOES_LIVE (NR-321 ruling: rides battle consumption); anchor BL-543 value anchor | § Upkeep |
| Out-of-supply decay not in effect (`out_of_supply_reach` = 0) | Same as above — BL-496 / BL-458 SUPPLY_LINES_CANNOT_BE_CUT is what waits on it | § Upkeep, § One reach field |
| No battle state serialised / stance has no serialiser | BL-536 WORLD_SNAPSHOT_SAVE ("battles included") | § Battles |
| No fortification / `siege_stance` unread | NO ITEM (`--grep fortif`, `siege` nothing) | § resolve_battle, stated as fact |
| No tactical naval | NO ITEM (only BL-277 Era −1 military strategy, tangential) | § resolve_battle |
| No unit marker on the canvas | BL-471 UNIT_MARKER_AND_COMMAND_SURFACE | § The unit model |
| Merge/split, garrison/scout verbs | BL-472 UNIT_FORMATIONS; BL-314 UNIT_VERB_FAMILY | § Marching |
| ACTIONS.json transcription of march/halt/disband | BL-314 UNIT_VERB_FAMILY | Not folded (a dictionary-maintenance task, not a military design fact) |
| Hire price on screen | BL-405 HIRE_HAS_NO_PRICE_ON_SCREEN | § The muster interface |
| Era −1 sim conquers nothing | BL-384 ERA_MINUS1_SIM_CONQUERS_NOTHING | Not folded (an Era −1 sim defect; history_sim is not this doc's subject) |
| Rival units absent from blackboard export | NO ITEM (`--grep blackboard` hits nothing unit-specific) | § Marching, stated as fact |
| Friendship permits nothing | BL-549 FRIENDSHIP_PERMITS_TWO_THINGS | § Battles |
| Ordnance unproducible at ancient epoch | NO ITEM (BL-496/BL-498 touch ordnance but not the era mask) | § Upkeep callout, stated as fact |
| Campaign roster band hard-coded to industrial | Closed — `campaign_roster_band_for(era_band)` exists in code; not a hole | § The roster |
| In-battle march rejection unreachable | Closed — `unit_in_battle` exists and `march_unit` calls it | § Marching |

Summary: 19 holes audited. 12 have an owning item (one of them, BL-567, design-owed).
5 are NO ITEM: season, reinforcement, fortification/siege, tactical naval, rival units on
the blackboard, plus the ordnance era-mask mismatch (6 if counted). 2 were stale holes the
code has since closed (roster band, in-battle march). No backlog items filed.

# H2 holes — docs/ui/PLANETARY.md

PLANETARY.md | Amber "pinned" highlight on tiles: canvas passes pinned=false everywhere (highlight.hpp defines the state; no Explorer working set pins tiles) | NO ITEM
PLANETARY.md | Stockpile / output readout on tiles ("Layer 3 deferral") — per-building output lives in hover card / Selection / Production lens, no on-tile readout | NO ITEM (Production lens + hover card already answer it; arguably STALE)
PLANETARY.md | Resource deposit overlay ("shipped" row) | STALE (Resource lens exists, LENSES.md § Resource lens)
PLANETARY.md | Tile inspector ledger redesign (exploration system) | NO ITEM (nearest: BL-503 ENTITY_BUILDERS_CONVERGE, which is builder convergence not a redesign)
PLANETARY.md | Seam visualisation marker at the wrap | STALE-by-design (wrap is seamless; doc now states no marker is wanted — not a hole)
PLANETARY.md | "Stockpile readouts, market state, workforce indicators added in later layers" (Layer-2 framing) | STALE (Market/Population lenses + hover card carry them)
PLANETARY.md | Corp-HQ reach ring (retired note) | STALE (retired deliberately; LENSES.md)
