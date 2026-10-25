# Project Io — Needs Review

**Ben's review queue.** Readable mirror of [`NEEDS_REVIEW.json`](NEEDS_REVIEW.json),
which is canonical — the JSON wins on any disagreement.

> **Generated file.** Produced by `node tools/session/render_needs_review.js`.
> Edit the JSON, then re-run; hand edits here are overwritten.

Things here are waiting on **your judgement**, not on work. Three kinds:

| Kind | Meaning |
|---|---|
| **question** | An open call nobody has made. Not blocking — a blocking item is a backlog entry with `blocked_on` set. |
| **decision-taken** | A call made **on your behalf** so work could continue. Recorded so it can be *overturned* rather than quietly becoming precedent. |
| **observation** | Something noticed in passing, too small or too cross-cutting to file, that a human should still see. |

**How this differs from the neighbours.** [`review.json`](review.json) is a *blocker* list —
items blocked on a visual artifact only you can produce; work there cannot proceed at all.
[`backlog.json`](backlog.json) is *work*. Entries here are neither: they are questions and
reversible calls. If an answer creates work, file a backlog item and resolve the entry with
that item's id.

This queue is **transient**: resolved entries are pruned promptly rather than kept for
posterity — the reasoning lands in code, an authority doc, or a backlog item at the moment
the work happens, and that is the durable record. What stays here is what is still open.

*55 entries — 34 open, 21 resolved.*

---

## Open

### NR-966 — NOVEL WORK: a both-trees shape probe for re-blesses (rebless_shape_probe)
*novel-work · raised 2026-10-03 · from the sprint 48 re-bless lane*

tools/verify/rebless_shape_probe.cpp measures the world's shape (centres, scales, provinces, markets, roads, wars, sea, firms, seats, haulage) on two source trees so a re-bless can be described in shape, as DELIVERY.md asks. It is new tooling with no saved skill. Also new this sprint with no skill: culture_preference_census, centre_abandonment_census, centre_decline_trace, garrison_border_probe, market_gravity_ladder.

**Why it matters.** Tool creation is skill creation (CLAUDE.md); a skill wrapper needs your permission.

- A: wrap rebless_shape_probe as a re-bless skill (the one each sprint's re-bless uses)
- B: leave them as ad hoc tools in tools/verify
- C: other

> **Recommendation:** A for rebless_shape_probe; B for the rest.

*Files: `tools/verify/rebless_shape_probe.cpp`*

### NR-970 — DECISION TAKEN: how the shelf spoils (BL-1179)
*decision · raised 2026-10-05 · from the sprint 49 main session (Ben: "I'll also trust your judgement"; NR-968 brought spoilage forward)*

Settled in MARKETS.md sec The shelf spoils: every good on a market shelf loses a fixed per-tick share, authored per good by class - perishable (food rations, agricultural produce, water, clean water, medical supplies) fastest, consumable (consumer goods, fuels, chemicals) slower, durable (ores, metals, alloys, materials, components) slowest but never zero; first-cut rates, then measured. Only the SHELF spoils (a corp pool is its owner's stock). Nobody is charged (the market already paid; goods leave, no credits move). Spoilage drains after the tick's draws and before the shelf's share of supply is read. The k cap stays; k > 0 is set by measurement.

**Why it matters.** Spoilage plus the household draw are the two shelf drains that make k > 0 safe, per your 2026-10-03 ruling.

- A: keep it
- B: spoilage also bites corp pools (a holding cost)
- C: one flat rate for every good
- D: other

> **Recommendation:** A.

*Files: `docs/economy/MARKETS.md`, `scripts/economy.lua`, `src/world/market_clearing.cpp`*

### NR-975 — DECISION TAKEN: the lake size cap is 150 tiles (BL-1200)
*decision · raised 2026-10-05 · from the sprint 49 main session, on Ben's delegation (NR-974 said: measured)*

lake_census (dff0abc1) measured 360 enclosed water bodies on the 16 curated seeds: no clean gap. The pooled dip is 89-143 tiles (3 bodies), with a dense cluster from 154 to 204 above it; 21 enclosed bodies exceed 1,000 tiles (seed 11: a 4,027-tile "lake" beside a 4,090-tile sea). Chosen: 150 - just under the cluster, keeping the near-twins 119 and 138 (seeds 25, 13) together as lakes. Pooled: 301 lakes (3,079 tiles, ~3,800 shore) and 59 seas; lakeshore falls from ~14,700 tiles to ~3,800.

**Why it matters.** Where the line falls decides which shores give Wells free fresh water and which get ports and wharves.

- A: keep 150
- B: 100 (the pooled dip; splits seed 25s and 13s near-twins)
- C: 300 (mid-size 150-300 bodies stay lakes)
- D: other

> **Recommendation:** A.

*Files: `src/world/tile_generation.cpp`, `docs/economy/TILES.md`, `tools/verify/lake_census.cpp`*

### NR-977 — DECISION TAKEN: a short shelf is shared within each phase of the tick, construction first (BL-1209)
*decision · raised 2026-10-07 · from the BL-1209 cold review*

Your pro-rata ruling says every draw admitted against a short shelf in a tick shares it. The tick draws in phases: construction sites draw before labour is solved, processors after. Pooling both phases into one ration would mean reordering the tick, so the share is computed within each phase: sites share among themselves, then processors share what is left; upkeep draws stay first-come after both. A short steel shelf can therefore go to sites before plants. MARKETS.md will say so exactly.

**Why it matters.** Construction-first can starve production of a shared input in a tight market; the alternative is a tick reorder.

- A: keep per-phase, construction first
- B: pool the phases (reorder the tick so sites and plants share one ration)
- C: per-phase, production first

> **Recommendation:** A for sprint 49; revisit in sprint 50 (logistics) if the trade-flow lens shows sites starving plants.

*Files: `src/world/economy_system.cpp`, `docs/economy/MARKETS.md`*

### NR-978 — DECISION TAKEN: the seat clean slate refunds everything a site was charged, not only materials (BL-1206)
*decision · raised 2026-10-07 · from the BL-1206 cold review*

Your ruling read "materials paid so far refunded". The build refunds the recorded sum of every charge the site took: materials at the posted price, the flat build-cost slice, and the construction capacity it drew. A site paused for want of materials but still drawing capacity is refunded those charges too. CORPORATION_GENERATION.md step 5 now says so.

**Why it matters.** A clean slate means the player pays for nothing they did not choose; refunding materials only would leave the flat cost of builds the AI started on the player's books.

- A: keep - refund everything the site was charged
- B: materials only (needs a field separating the flat slice and capacity)

> **Recommendation:** A.

*Files: `src/world/economy_system.cpp`, `src/world/corporation_generation.cpp`, `docs/generation/CORPORATION_GENERATION.md`*

### NR-979 — Six curated seeds no longer serve the purpose they were chosen for: re-curate them?
*question · raised 2026-10-07 · from the sprint 49 seed-library rewrite (Ben, the re-bless form: rewrite descriptions from the new readings)*

After the sprint 49 re-bless (the lake cap moved every seed's history), the library was rewritten to be true; six seeds now say plainly that they no longer answer their question: 41 (colonial ties without capital - now capital, almost no colonial ties), 43 (concentrated wealth in a thin map - now the poorest), 38 (wealth that stayed home - neither), 32 (roads without traffic - mid-library on both), 9 (the displaced(no-nb) reading - no seed reads it now), 40 (money and roads without an outward turn - only roads survive). Two more are weaker than a sibling at their own job: 10 (37 is the cleaner roadless case) and 13 (46 and 38 now hold more living subjects).

**Why it matters.** A check aimed at "the rich small world" or "the roadless traders" now runs on a seed that is neither; tags still route --for queries to them.

- A: re-curate - sweep a seed pool for replacements that serve the lost purposes (a sprint 50/51 item)
- B: keep the rewritten seeds; drop the lost purposes from the library
- C: re-curate only the purposes a harness or skill depends on

> **Recommendation:** C - check which tags a harness or skill queries (--for), replace those seeds, and let the rest go.

*Files: `docs/generation/seed_library.json`*

### NR-980 — Trade-flow lens: may "no room" reflect rivals' cargo in flight to a market?
*question · raised 2026-10-07 · from the BL-1222 (trade-flow lens) cold review, 2026-10-07*

The lens marks a short market "no room" when the destination cannot absorb more at the landed price. That room subtracts EVERY corporation's convoys in flight to it and stock pooled there, so "no room" tells the player, in aggregate, that someone is already filling that market. It names no rival and no quantity. DISCOVERY.md § Competitor visibility makes market supply/demand aggregates public but does not say whether goods in flight count. Options: (A) accept it as a public market signal (cargo on the road is observable) and say so in DISCOVERY.md; (B) compute the player's room without rivals' in-flight cargo for the lens only (a second room figure the dispatcher does not use, so the lens and the decision could disagree).

### NR-982 — Decisions taken on your behalf building "an order is a floor, not a hold" (BL-1229)
*decision · raised 2026-10-07 · from the BL-1229 (steel stays home) build lane, 2026-10-07*

Four readings of the ruling, taken as the lane built them (reversible): (1) the dispatch margin applies ON TOP of the floor - a haul from an ordered pool must beat the floor by the same margin it must beat home by (the floor acts as that pool's home price when it is higher); (2) several orders on one (corp, body, good): the HIGHEST floor binds; (3) a quantity cap limits only what is listed at home per tick, not what may be hauled; (4) REVERSED by the cold review (2026-10-07): a pool hauled empty must NOT close its order, or the haul then ships below the floor and the ruling breaks - a pool hauled from this tick counts as not empty. Floors almost never bind today (0.25-0.31x base, 3 of 782 surplus-ticks), so (1) and (2) rarely matter in play.

### NR-983 — Decision taken on your behalf: spare supply stays a conservative estimate where several markets share one producer (BL-1233)
*decision · raised 2026-10-08 · from the BL-1233 (processors to inputs) round 2 re-review, 2026-10-08*

The sized test's spare (input_reach.cpp reachable_supply) charges each consumer market min(its draw, the output of the markets that reach it), split among those markets by output. Where one producer is reached by several hungry consumers it can still be charged past its output (P2 making 10, reached by C, Q1, Q2 each drawing 10 -> spare 0, true 10). This only ever REFUSES a plant that could stand, never admits one that cannot; the exact answer needs a small max-flow per input. TAKEN: accepted as a conservative estimate, with a code comment naming it. Reversible; say if you want the exact (max-flow) reading.

### NR-984 — Decision taken on your behalf: a nation's want for a good nobody holds yet is recorded at its capital market
*decision · raised 2026-10-08 · from the BL-1227 (idle mines) round 4 build and re-review*

Your ruling: the space programme's / network upkeep's want counts as a bid "there", filled or not. When no pool or shelf anywhere holds the good, there is no "there" to read, so the build records the want at the nation's CAPITAL market. Effect: one such chain (propellant, spacecraft components) can start per nation, at its capital; once a plant exists the want follows the good. In play this took spacecraft-components plants from 0 to 22 on 5 seeds. TAKEN as built and written into AI_OPPONENT.md § 2B. Alternatives: every market of the nation (more starts, more idle risk), or the market nearest the nation's space infrastructure.

### NR-985 — Decision taken on your behalf: an unpowered grid that no window can feed does not hold up the others (BL-1232)
*decision · raised 2026-10-08 · from the BL-1232 (power plants per grid) re-review, 2026-10-08*

Your ruling: unpowered grids first - every short grid gets a plant before any gets a second. The re-review found that a short grid NO chartering centre's window can feed (no market centre on it, or its feeding markets outside every window) would hold the rule forever: every power firm narrowed to it, found no ground, and the core stopped getting plants. TAKEN: such a grid is dropped from the unpowered set for that centre, so an unreachable grid never holds up the others; if nothing unpowered is reachable, every short grid is served. Reversible: the strict reading (the rule holds even for an unreachable grid) starves the body's core.

### NR-987 — Decision taken on your behalf: 'never cleared' is read from the market's current state, not a stored first-clear tick (BL-1217 D3b)
*decision · raised 2026-10-09 · from cold re-review of the BL-1217 D3 round 3 (a37f2099), sprint 50*

Your ruling: the dial forecasts at base 'only on a market that has never cleared'; the build veto takes the same 'no clear yet: no signal' reading. Both now read market_has_cleared from fields only the clear writes (supply, demand, hauler_want) - fixing a real hole where a market spawned mid-step read as cleared. TAKEN: kept the state proxy. Gap: a market that HAS cleared but is wholly dead (no supply, demand or silenced want on any good) reads 'never cleared' again, so the veto gives no signal and the dial forecasts at base there - the old test had the same gap. Practically unreachable today (off-world markets get interbody demand; home markets carry people). Reversible: a set-once first-clear tick on the market would close it exactly, at the cost of a saved field (another save bump).

### NR-988 — Decision taken on your behalf: a non-player roster corporation left with no holdings stays on legacy and no-budget worlds (BL-1217 D6)
*decision · raised 2026-10-09 · from the BL-1217 D5/D6 harness audit, sprint 50*

Your rulings bound every processor by want and unplace a recipe-less processor whose default output is unwanted. On the LEGACY landscape apply (no budget, refused budget or no specialists) that leaves 1 non-player roster corporation with no holdings (2 in two cases); the budget path - every shipped campaign - removes the roster right after, and the player is never left holdless. TAKEN: leave the holdless corporation as is (it holds cash and a seat in the roster, acts on nothing). Reversible: remove a non-player roster corporation that ends generation with no holdings.

### NR-989 — Novel work: road generation SNAPS a new road onto an existing one beside it (BL-1252, no parallel roads)
*novel-work · raised 2026-10-09 · from the BL-1252 lane, sprint 50*

To remove the parallel roads the ruled stale-flood reuse leaves, the lane added a new mechanism rather than re-pricing: before stamp_edge lays a route, any stretch that would lay 2+ consecutive new land tiles within one cell of an existing road is re-walked over that road, kept only if it lays fewer new tiles. Pricing and flood reuse are unchanged. Long parallels 235 -> 47 at 1.04x the time; road tiles -4.4%. LOGISTICS.md section 4 owns it. Flagged because it reshapes what is laid after pricing - a new kind of rule in generation. An income cost on the 5 tuning seeds is being re-measured on the current tip before the merge call.

### NR-990 — Novel work: verify fixtures that fabricate world state (sea_route_fixture lays Ports; air_gate inserts an unreachable airless works)
*novel-work · raised 2026-10-09 · from the sprint 50 scripted visual checks lane*

To verify the air gate and the sea-route lane headlessly, the verify API gained accessors that build world state no game path would: sea_route_fixture lays two Ports to make a sea pair with real land legs, and air_gate inserts an electrolysis works on an airless body the player cannot reach. Verify-only (bound behind --verify), deterministic, nothing in play changes. No doc owns the rule for when a verify fixture may fabricate state rather than find it; flagged so the precedent is chosen, not accreted. Also new: verify corp_command reads recipe_name/target_name/quantity, validated.

### NR-992 — Novel work: retire corporation pools - all production lands on market shelves (Ben, 2026-10-10)
*novel-work · raised 2026-10-09 · from Ben, sprint 50 close, the market stock form*

Ben (2026-10-10): remove corporation stockpiles - all of them, production lands on the shelf - and push initial goods to markets (the same total, redistributed to the markets the corporations sit in), to simplify the model and fix construction at game start; build now in sprint 50. Flagged as large scope growth mid-close: pools carry production, own-input draws, sell orders, corporation convoys, the launch fuel reservation, procurement, upkeep and the space programme, and several of this sprint’s rulings read pools (opening stock held, the dial’s stock-fed draws, R2’s spare charge). Paused for Ben’s design calls (who owns shelf goods; what replaces corporation hauling; extend 50 or own sprint) before any build. Owning docs to rewrite: MARKETS, PRODUCTION, SUPPLY, FINANCE, CORPORATION_GENERATION, AI_OPPONENT.

### NR-1001 — Sprint 51 write-up: five calls taken on Ben's behalf beyond the design form
*decision taken on your behalf · raised 2026-10-08 · from the sprint 51 design write-up, 2026-10-08 (RENDERING.md, SELECTION.md, LENSES.md, PLANETARY.md; BL-1239..BL-1244)*

The form settled the shape; writing it into the docs needed five smaller calls, each now in its owning doc and reversible there. (a) COMPANY LENS DEFAULT - no firm picked, so every firm's ground opens owned-grey: the player owns no company, so "the player only" has no counterpart (LENSES.md § Company lens). Alternative: every firm picked (the old look). (b) RIVAL ROWS IN THE PRODUCTION SECTION show type, count and owner; output and running state read "private", matching the rival building card (SELECTION.md). Alternative: running state public, since a working plant is observable. (c) SCAFFOLDING - "the art is static" has one exception, a construction site bakes as scaffolding, because start and completion are events rather than ticks and so cost no per-tick re-bake (RENDERING.md § Installations). (d) A FOURTH STACK adds nothing to the ground (the cluster caps at three), and a small settlement may vanish at the far page where the density dot stood; a scale >= 3 centre must still read as a city at every rung. (e) NEVER-MAGNIFY shifts every rung up one tier (13 px rung -> 24 px tier, top rung -> 192), and supersampling bakes the 192 tier at 384 px per hex - four times the bake pixels on the worker. BL-1244 measures bake time and memory before and after; if the 192 x 2 bake is too slow, the lever is to supersample only the tiers below it.

### NR-1002 — After a refund, the header NET and the Balance card net disagree: should the header exclude the refund?
*question · raised 2026-10-08 · from lane L6, BL-1215 (refunds flow shown), sprint 51*

On a seat that received a refund the header reads NET +332/qtr (the quarterly return net, which includes the 190.3 refund) while the Balance card reads +142.1 (operating net) with a new line "Refunds: +190.3 (not earnings)". The line makes the gap readable but the two headline numbers still differ. Options: (A) leave both, the card explains the gap; (B) the header shows the operating net, excluding refunds, so the two agree; (C) the header keeps the return net and gains a refund marker on hover.

### NR-1003 — Structures, settlements, landforms and rivers vanish wherever the bake is not drawn: under every lens, and on unsurveyed ground in spectator god view
*question · raised 2026-10-08 · from lane L4, BL-1241 (structures baked), sprint 51*

RENDERING.md § Installations says a structure is ground and takes the lens wash. As built, every lens draws through the old vector fill path, which never shows the bake, so with the canvas glyphs retired a lens shows no buildings at all. That is the gap the cancelled BL-734 (ground/chrome layer contract) was to settle. Options: (A) new sprint 51 work - lenses composite their tint over the baked ground (one item, touches the lens fill path BL-1240 just changed); (B) accept for now - a lens is an analytic read and the Selection element names what stands there; file the bake-under-lens work for a later sprint; (C) under a lens, draw a minimal structure footprint mark in the vector path (a glyph by another name, against the design). WIDER THAN FIRST RECORDED (the integrated cold review, 2026-10-08): the same cause hides them (1) under the Corporation and Company lenses, where BL-1240 asks the player to pick owners by colour and none of their buildings show; (2) in spectator god view on unsurveyed ground, where the retired glyph paths used to lift rival works, rivers and landforms into view and the bake masks those tiles; (3) briefly on a body switch before the far page is ready. Rivers and landforms are lost the same way as structures, since their canvas strokes and glyphs are also retired. RULED (Ben, 2026-10-09): build lens over the bake now - LENSES.md § A lens washes the rendered ground. STILL OPEN from this entry: the spectator god-view case (unsurveyed ground hides rival works, rivers and landforms in god view, because the master masks those tiles).

### NR-1004 — The player-identity wash tints the player's own baked structures blue
*question · raised 2026-10-08 · from lane L4, BL-1241 (structures baked), sprint 51*

The always-on player-identity wash (about 30% on the player's tiles on the plain canvas) is applied over the bake, so the player's structures come out tinted, against RENDERING.md "a structure carries no owner colour". Ben kept the always-on player OUTLINE (the sprint 51 form) but did not rule on the wash. Options: (A) drop the wash, keep the outline only; (B) keep the wash but mask it off structure pixels; (C) keep as is and amend the doc.

### NR-1005 — Sprint 51 lane calls taken: the market-state band, non-producing buildings read Running, and verify staging that writes world state
*decision taken on your behalf · raised 2026-10-08 · from lanes L1 (BL-1239) and L4 (BL-1241), sprint 51*

(a) The Production section's short / balanced / surplus reads supply against demand with a +-10% band, borrowed from the Scarcity lens; no doc fixed a threshold. (b) Ports, hubs, launchpads, military bases and other non-producing types now read "Running" in the hover card and the Production section: they staff at zero by design, so "labour short" described a shortage that cannot exist. (c) "No producer in reach" is not derived as an input-shortage cause (it needs the input_reach logistics walk, too heavy per frame); the line falls back to "input short: <good>". (d) NOVEL: L4 added verify-only functions (stage_gallery, stage_building, stage_centre) that insert buildings and restep centres directly, bypassing placement rules, and stage_gallery returns the tile positions it staged to. C++ picks the ground, but positions reach Lua - compare the scoped NR-698 grant (verify.find_deposit_tile). Options for (d): accept as a verify-only staging grant recorded beside NR-698; or require scripts to stage through the real placement seam. (e) RIVER WIDTH IS MASK-BLIND (cold review): a revealed river's width reads accumulated flow including unsurveyed upstream tiles, so width hints at the size of an unsurveyed catchment; courses and forms never draw from masked ground. Taken as acceptable; confirm or require flow to stop at the mask.

### NR-1007 — BL-1246 ground one master: four calls from the build
*decision taken on your behalf · raised 2026-10-09 · from the BL-1246 (ground one master) lane, sprint 51*

(a) NOVEL, taken: unsurveyed ground is a FILL, not a bake - a master chunk whose every tile in reach is unsurveyed is filled with the lock colour directly, byte-identical to the full bake (ground_bake_check P23). It is what lets a first visit to an unsurveyed body be sharp in 0.77 s. (b) A Debug --verify run that frames a whole body now pays minutes of synchronous master bake (Debug bakes ~10x slower). Option: compile the bake TUs optimised in Debug (a CMake change), or accept. (c) --autostart-windowed smoke runs now wait 35-55 s on the painting step (until the Life-round pre-bake lands). Option: skip the wait under autostart (the ground fills in live), or accept. (d) The Selection band's neighbourhood page is still its own flat 48 px bake, not a crop of the master - so it does not match the 22.5 deg canvas. Work for a later item unless you want it now.

### NR-1008 — Lens wash and partial re-bake: four calls and two novel pieces
*decision taken on your behalf · raised 2026-10-09 · from the BL-1250 (lens washes ground) and BL-1246 partial re-bake lanes, sprint 51*

(a) Wash strengths taken: categorical lenses 0.46, sequential 0.54 (LENSES.md). (b) Resource lens: ground with no deposit keeps the WHITE wash Ben ruled 2026-10-04 (0.50) rather than "no wash" - LENSES.md says an unanswered tile takes no wash, so one of the two needs your word. (c) CALL: at the close rungs the Company lens draws the unpicked owners' dark rim on nearly every tile (firms hold most ground), which reads as a heavy hex grid. Options: thin/fade the rim at close zoom; drop the rim where the close-zoom seam already marks edges; accept. (d) NOVEL, fixed: a lit structure's hover wash drew at the inset radius and left a 1 px grid of unlit gaps over the bake; now drawn at full radius. (e) NOVEL, scope grew: the partial re-bake needed a bounds mode in the structure pass (rasterise a tile's parts writing nothing) and a shared build_tile, because a conservative reach only saved ~50%; plus a 1/256 px rounding fix in four structure forms caught by the new window-invariance row. (f) Rung 0 frame time read ~20 ms in the lens lane under heavy machine load (11.5 ms clean in the 60 fps lane); a clean re-measure is owed.

### NR-1035 — Sprint 50 onto main: G3 (firms alive at tick 400) moved 99.8% -> 95.8%; every other gate digit identical
*question · raised 2026-10-10 · from Main-session gates on the sprint 50 merge (a560c5d7 + d8d2a04c), 2026-10-10*

market_viability on merged main: G1 93.3, G1 t50 87.5, G1b 5.7, G2 113.4 - identical to sprint 50 tip c8752b8a - but G3 95.8% (1959/2045) against the tip's 99.8%. world_determinism PASS; player_seed_sweep --guard 12 ALL PASS; history_sim_harness at its 2-failure baseline (R3a2/R3a3). The likeliest cause is sprint 51's main-side world movers (BL-1218 wharf placed on shore, BL-1227 mine candidate priced at its stack rank, both in placement_rules) meeting the shelf economy for the first time; not attributed.

**Why it matters.** G3 stays far above its 70% target, so nothing fails; but the merged world is not the one sprint 50's re-bless described, and the version cut reads this tree.

- Accept: record the merged reading as the cut baseline
- Attribute first: run market_viability on sprint 51's tip and sprint 50's tip on this binary (about two hours) before the cut

> **Recommendation:** Accept, and let the next re-bless attribute it - every G1/G2 digit held.

*Files: `tools/verify/market_viability.cpp`, `src/world/placement_rules.cpp`*

### NR-1064 — Sprint 57 doc sweep: readings taken on your behalf re-subjecting ~60 docs to nations
*decision · raised 2026-10-23 · from sprint 57 doc sweep (four lanes, main session review)*

Taken in band while re-subjecting the corpus to 'nations own everything'. ECONOMY: FINANCE drops the '<5% of a median firm's income' anchor and the [in debt] flag; glut clearing rests on idling (no building-exit rule); the tariff triggers on the convoy's OWNING nation; MARKETS' listed value retires with no replacement; generated Marketplaces may now land on the player's ground; opening stock falls back to the capital's market where it used the HQ's; POPULATION's labour share stays the share of building count; CONTRACTS keeps no live form between nations. GENERATION: CORPORATION_GENERATION retitled Industry Generation; a charter is a priced set of buildings owned by the centre's nation; per-firm capital no longer sizes opening stock; focus is kept per charter, region-derived; INDUSTRIALISATION's listed value sums building book value. UI: slot 1 renamed 'Nation overview'; the stance ledger (slot 8) retires rather than re-subjecting; the Selection band rests on the player's nation; acquisitions.md keeps its body under a retirement banner. AI/MISC: PEOPLE's company head becomes the nation's Head; RELATIONS keeps only nation-nation stance and retires reputation and the equity_taken / levy_charged / lobbied_against factors; a garrison is drawn into battle by declared nation stance; MILITARY's base is the nation's own build. Main session: 'no capital starts dark' stands, paid from the nation's whole treasury (LOGISTICS).

**Why it matters.** Each is a faithful reading, but together they re-shape a lot of prose at once; any one you would rule differently is cheap to change before the code lanes build against it.

*Files: `docs/economy/FINANCE.md`, `docs/economy/MARKETS.md`, `docs/generation/CORPORATION_GENERATION.md`, `docs/generation/INDUSTRIALISATION.md`, `docs/ui/ledgers/corporation.md`, `docs/politics/RELATIONS.md`, `docs/PEOPLE.md`, `docs/military/MILITARY.md`*

### NR-1065 — Too many majors: the poorest surplus major is DEMOTED to a minor, not folded into another
*decision · raised 2026-10-23 · from sprint 57 doc sweep, main session correction*

The 2026-10-11 reconciliation folded the poorest major's SPECIALISTS into the richest major's champion; ground never moved. With no corporations there is nothing to move, so the faithful reading is a demotion: the poorest surplus major becomes a minor and keeps its ground and buildings, and the nation total is untouched. A lane had written it as a territorial fold, which would have cut the nation count by one outside the total's reconciliation; corrected in NATION_GENERATION.md and NATIONS.md § Own ground. BL-1404 (generation buys for nations) builds it.

**Why it matters.** It decides whether reconciling the major count can ever change the map.

*Files: `docs/generation/NATION_GENERATION.md`*

### NR-1066 — Six questions the sweep could not settle once corporations retired
*question · raised 2026-10-23 · from sprint 57 doc sweep*

(1) EMBLEMS: PROFILE.md and ICONS.md now key the player's emblem by nation id, which sits against ICONS' own 'no nation flags' rule - keep a corporate-style emblem for the nation, or no emblem. (2) EMBARGO: what an embargo between nations gates, now there is no procurement between firms (RELATIONS.md open question 4). (3) DISCLOSURE: may the player read a rival nation's quarterly return or treasury (FINANCE.md § Disclosure; RELATIONS open question 5). (4) WORKS LEAN: INDUSTRIALISATION's Private Works vs State Arsenal fork has no subject - derive it some other way, or retire it. (5) INDUSTRIAL FOCUS: whether a nation's build scorer keeps a focus weight (AI_OPPONENT ~789) or it retires with the specialist. (6) INDUSTRY LENS: re-subjected to 'plants of nations other than the player's' - keep, or retire.

**Why it matters.** Each is a design hole the removal opened; none blocks BL-1399..BL-1404, but (3) and (5) shape the AI and UI lanes.

> **Recommendation:** (1) no emblem until designed; (2) an embargo bars a nation's goods from entering your markets; (3) a rival's treasury is private, its GDP public; (4) retire; (5) keep focus as the nation's economic_focus; (6) keep.

*Files: `docs/ui/PROFILE.md`, `docs/politics/RELATIONS.md`, `docs/economy/FINANCE.md`, `docs/generation/INDUSTRIALISATION.md`, `docs/ai/AI_OPPONENT.md`, `docs/ui/LENSES.md`*

### NR-1067 — BL-1403 (nation money flows): the tax rates now bite on wages, and a nation's import duty is capped at its treasury
*decision · raised 2026-10-24 · from BL-1403 build, main session*

(1) WAGE TAX RATES: the tier rates I..V (0 / 5 / 10 / 15 / 20 %) were calibrated as shares of a firm's operating PROFIT; under your ruling they are now shares of the WAGES a nation pays, which is a much larger base, so tier V returns a fifth of the whole wage bill. Kept unchanged in band; the post-fold measurement will say whether the tiers still read as a trade-off against Discontent. (2) DUTY CAP: a nation never runs into debt, so when a nation (not a corporation) owes import duty on goods entering a foreign market, the duty is capped at what its treasury holds; the cargo still lands. The alternative is refusing the shipment at dispatch when the importer cannot pay, which is a larger change to the trade pass.

**Why it matters.** (1) decides whether a high tax tier is a strong lever or a free one; (2) decides whether a broke nation can keep importing.

*Files: `src/world/budget_system.cpp`, `src/world/supply_system.cpp`, `src/world/nation_tax.hpp`*

### NR-1068 — BL-1402 (fold to ground): market_viability's G2/G3 re-pointed, and the seat's clean slate now covers its whole nation
*decision · raised 2026-10-24 · from BL-1402 build + cold review, main session*

(1) G2 (field income) summed each corporation's filed income; after the fold every corporation earns nothing and G2 read 0.0. It now adds each nation's state-ledger income (its buildings' sales) - the same 'whole field's sales' quantity. (2) G3 (firms alive at the last tick / at the handoff) has no subject on a folded world. Re-pointed to BUILDINGS alive at the last tick / at the handoff, target unchanged at 70% - the closest question ('does the handoff economy survive the play'); the alternative of nations alive is vacuous (a nation only dies by conquest). (3) Firm exit is inert on a folded world: it would erase the empty champion records the major/minor reading still keys on until BL-1407. (4) BL-1206's seat clean slate (the seat's unfinished settle builds cancelled and refunded) now cancels the seat NATION's unfinished builds, its own AI's settle sites included - the ruling re-subjected to 'the seat is the nation'. (5) A nation's trade reserve is not carried by the fold and a nation cannot set one until BL-1407 saves the field (generation leaves every reserve at 0).

**Why it matters.** (1)-(2) change what two merge gates measure; (4) decides how much construction the player's nation opens without.

*Files: `tools/verify/market_viability.cpp`, `src/world/corporation_generation.cpp`, `src/world/corp_command.cpp`*

### NR-1069 — After the fold, nations' treasuries fall during play, and on seed 46 the majors stop building
*question · raised 2026-10-24 · from BL-1402 gate, main session*

On the folded world (BL-1402), seed 46's 4 majors place NO production building in 40 seated ticks (seeds 0 and 28 still pass: 3/5 and 1/1). Diagnosed 2026-10-24 with nation_ai_builds' new diagnostic lines: every extraction candidate is refused before selection - placement (60-210 per major), construction materials not obtainable (30-110), net <= 0 and glut vetoes - and the majors decide only dials; their treasuries are far above the floor (e.g. 8549 vs 312) but FALLING (27407 -> 8549 over the window for nation 47763). A reading of a thin, glutted economy, not a scorer defect; the row is left failing rather than weakened. Owned by BL-1409. market_viability FULL on the folded world still passes every gate: G1 92.9, G1t50 85.4 (target 85), G1b 4.6, G2 95.3, G3 100.0 (buildings alive, NR-1068). BL-1403 (tax from wages, no levy) is not applied yet; its wage tax returns 10% of wages at tier III, which slows the fall.

**Why it matters.** A nation that pays every wage and input from one treasury, and whose treasury drains, idles its buildings one by one under the never-debt rule - the folded economy's long-run health is the question BL-1409 measures.

> **Recommendation:** Let BL-1409 measure the 400-tick treasury trajectory across the 16 seeds before any change.

*Files: `tools/verify/nation_ai_builds.cpp`, `tools/verify/nation_floor_probe.cpp`*

### NR-1070 — NOVEL WORK (BL-1406): a verify-only staging call verify.fold_world, seven retired verify scripts deleted, and a golden blessed under a retired lens
*novel-work · raised 2026-10-24 · from BL-1406 lane (UI and protocol as nation)*

(1) A new verify-only STAGING call, verify.fold_world, runs fold_to_ground on the --verify world (whose builder does not go through finish_campaign_world), and verify.buildings() lists nation buildings once folded. The standing rules record dated scoped exceptions for what the verify API may reveal or stage (road_admits, find_water_crossing, stage_road); this is a fifth of that family, raised here rather than assumed. (2) Seven verify scripts for retired surfaces (Corporation / Company lenses, acquisitions, company ledger, stance table) were deleted and ~15 re-pointed to the Nations lens; .claude/skills/verifier-visual/SKILL.md still names the deleted ones - a skill edit, which is yours to approve. (3) The icon_silhouettes_overview golden was blessed with the retired Corporation lens on screen, so it now differs; the golden policy says no bulk re-bless and --bless never creates - a single re-bless needs your go (BL-1408).

**Why it matters.** (1) widens what verify scripts may do to the world; (2) and (3) are your gates on skills and goldens.

> **Recommendation:** (1) grant, verify-only and behind --verify like its siblings; (2) approve trimming the deleted names from SKILL.md; (3) re-bless the one golden at BL-1408.

*Files: `src/core/verify_api.cpp`, `.claude/skills/verifier-visual/SKILL.md`, `scripts/verify/`*

### NR-1071 — BL-1407 consequences: unit upkeep now runs only as the garrison budget claim, the nation scorer does not hire, and two smaller readings
*decision · raised 2026-10-24 · from BL-1407 lane + main session*

(1) UNIT UPKEEP: the credits+goods+supply-decay vector (run_unit_upkeep) only ever ran for CORPORATION units; with corporations gone every unit takes the garrison branch, whose upkeep is a military_research budget-line claim (MILITARY.md § Nation garrisons) - so the per-unit vector no longer runs anywhere. That is MILITARY.md's model for a nation's units, applied to all of them. (2) HIRE: the scorer's corporation hire/base branch is left OFF for nations - a nation raises force through the muster (MILITARY.md), and opening the corporation hire path too would give it two. The 2026-10-23 grant named hiring among the carried-over decisions, so this is a narrowing. (3) THE SCORER'S TRADE, MARKETPLACE AND SURVEY BRANCHES are dead after the delete (they were corporation-only), so no AI nation pins a trade, builds a Marketplace or surveys; AUTO trade still runs. Re-opening them under the grant (BL-1402 T8, parked as docs/development/drafts/bl-1402-t8-nation-ai-trade.patch) was measured: market_viability FULL G1t50 85.2 -> 84.0 (FAILS the 85 target), G2 94.1 -> 96.9, G1b 4.6 -> 4.9; with manual pins off but Marketplace and surveys on, G1t50 84.6 (still fails) and G1b 5.0. Left OFF so the gates pass; YOUR CALL: re-open (and accept or re-target the gate), or keep off until BL-1409 explains how a whole-economy nation trades against its own plants. (4) A nation's quarterly return counts the wage tax as income; the seat card's trailing net still reads 0 though nations now file returns.

**Why it matters.** (1) and (2) decide what a nation's army costs it and how it grows one; (3) restores AI trade, which otherwise stopped entirely.

> **Recommendation:** Keep it off and fold the question into BL-1409, whose measurement (treasury trajectory and seed 46) is the same question of how a nation that owns every plant spends.

*Files: `src/world/corp_ai.cpp`, `src/world/economy_system.cpp`, `src/world/spawn_seat.cpp`*

### NR-1075 — The seat clean slate still refunds a cancelled site's materials; the new rule refunds credits only
*question · raised 2026-10-25 · from Construction payment lane (92f99bf3, merged 3dc6f34a)*

A site cancelled before completion now refunds its credit cost in full; materials drawn stay spent (Ben, 2026-10-25). The seat clean slate (BL-1206: taking a seat clears the nation's unfinished sites) used to refund everything charged, materials included. The lane kept that total for the seat: the door refunds the credits and seat_clean_slate refunds the materials on top, so the seat's returned sum is unchanged.

**Why it matters.** The seat clearing is not the player's choice, so a full refund keeps them whole for something done to them. But it is an exception to the rule you just set.

- A) Keep the exception: the seat clean slate refunds credits and materials (as built).
- B) The seat follows the rule: credits only, drawn materials stay spent.

> **Recommendation:** A. The player did not cancel those sites; the seat did.

*Files: `src/world/construction.cpp`, `tools/verify/seat_clean_slate.cpp`, `docs/economy/PRODUCTION.md`*

### NR-1076 — The placement credit cost includes the site multiplier; forward claims share a short shelf pro-rata
*decision taken on your behalf · raised 2026-10-25 · from Construction payment lane (92f99bf3, merged 3dc6f34a)*

(1) The credit cost paid once at placement is build_cost x site days / (authored quarters x 90): a mountain site that takes longer also costs more credits, as PRODUCTION.md's site-time rule already stretches cash. (2) The quarter-ahead rate reads the shelf less every site's owed materials; on a contended shelf the sites share it pro-rata through the BL-1209 solver (MARKETS.md requires pro-rata on a short shelf). The claim reserves against other construction only: a processor can still buy claimed stock that tick, and the gap shows as a carried shortfall.

**Why it matters.** Both shape what a build costs and how fast it runs. They follow existing rules, so they were taken rather than raised.

> **Recommendation:** Keep both.

*Files: `src/world/economy_system.cpp`, `docs/economy/PRODUCTION.md`*

---

## Resolved

Kept, not pruned: the reasoning is the point. Prune only in a deliberate sweep, once the
answer has landed in an authority doc.

### NR-1047 — Space asset calibration (BL-1327 space asset model): costs, reach, upkeep, decay chosen in-band
*decision · raised 2026-10-15 · from BL-1327 lane + cold review, merged to main 2026-10-15*

Destinations are named slots (body + orbit|surface), tiered from the owner home body: home orbit, moon orbit, moon surface, far orbit, far surface. Launch cost: satellite 10000, rover 20000, crewed 60000, station module 40000; reach 0/8000/15000/30000/45000 + 20000 per AU (far tiers). Travel quarters 0/1/1/2/3 + floor(AU), clamped at 400 extra. Upkeep/quarter: 500/1000/3000/2000. Lost after 4 unpaid quarters (hard ceiling 64). Launch cost and upkeep are pure treasury sinks until BL-1328 (launch infrastructure) buys goods from corporations.

**Why it matters.** These set how many launches a nation can afford by 2040; BL-1325 runs will show whether moon landings and stations happen at all.

> **RESOLVED.** Ben, 2026-10-18: keep the calibration as chosen and let the 2040 run judge it. No dial named; a dial moves on the run's evidence, not ahead of it.

### NR-1048 — The player's nation has its budget scored by nation_ai, against NATIONS.md and AI_OPPONENT.md § 11
*question · raised 2026-10-15 · from BL-1275 (nation research AI) lane report, confirmed by reading src/world/nation_ai.cpp:543 and nation_step.cpp:54*

score_national_budgets walks every landed nation and run_nation_step writes the result into w.nation_budgets, the player's nation included. The docs say the player's nation is never scored by nation_ai (AI_OPPONENT.md § 11, the 2026-10-10 narrowing). BL-1275's research pass does skip the player's nation; the budget scorer does not. This predates sprint 55.

**Why it matters.** Either the AI is setting the player's national budget every cadence (an act on the player the grant forbids), or the player has no budget control yet and the scored weights stand in as a default. The two readings need different fixes.

> **RESOLVED.** Ben: B, keep scoring the player's nation as a dated stand-in until the budget verbs exist. Written into NATIONS.md and AI_OPPONENT.md § 11.

### NR-1049 — BL-1352 (one LP system): three readings taken so a march could draw the nation's pool
*decision · raised 2026-10-18 · from BL-1352 build, main session*

The march now draws Logistic Points and claims ahead of production. Four readings the doc does not settle, each cheap to change. (1) WHOSE POOL - THE MOVER'S. A corporation's march draws its HOME NATION's points, a nation's garrison its own; never the pool of the ground it stands on. The first cut read the ground's nation and a cold review caught both consequences: a unit whose points ran out on unowned wilderness could never be granted again (a refusal moves nothing, so it re-fired for ever and the unit was stranded for good), and a unit invading another nation drew ITS VICTIM's pool, starving the defender's industry to fund the attack. LOGISTICS.md's 'Movement a player or rival DIRECTS' is the reading taken. The alternative - the ground's nation, with a stranding escape and a foreign-ground rule - is a real design and is Ben's if he wants it. (2) NO REACH TEST ON A MARCH - a unit draws its own nation's pool at full strength wherever it stands, because the off-network military reach is still 'military detail later' (LOGISTICS.md). (3) THE CLAIM IS GATHERED BEFORE THE BATTLE PHASE - the resolve runs at tick phase 4 and the march at phase 11, with battles at 10, so a unit killed in battle has already consumed its nation's points for that tick; the alternative is resolving the pool twice a tick. (4) THE EXCHANGE RATE IS UNCHANGED - one point admits one march-point of movement (infantry 1.0, cavalry 1.5 a tick) against a production need of 10 per unit of run, so a 50-unit army asks for ~60 points of a pool measured in the hundreds. Nothing measured says a soldier-tick and a unit of grain burden a nation equally; the old per-anchor rate (20/tick at every city) carried the same assumption.

**Why it matters.** Reading 4 decides whether 'guns, butter and freight compete in one currency' is felt at all: at these numbers a march is a rounding error against production, which is the opposite of the campaign-starves-its-industry cost the doc describes. Reading 1 is the one that changes what a war looks like - who pays for an invasion's logistics. The 2040 spectator run is the measurement that should settle 4.

> **RESOLVED.** Ben, 2026-10-23 (the sprint 55 close form): (1) the mover's own nation pays, as built. (4) the 1:1 exchange rate is kept as settled, not as a reading awaiting the 2040 measurement. Readings (2) and (3) stand as built.

### NR-1050 — BL-1346 (market attraction): the off-road leg's price, and what it costs to cross water
*decision · raised 2026-10-19 · from BL-1346 build + cold review, main session*

MARKETS.md rules that catchment is logistics cost over pull, and LOGISTICS.md section 3b rules an off-road tile is priced by 'the off-road cost per tile times the number of 4-cardinal steps'. Two things the docs do not settle were decided in band. (1) THE PRICE: k_offroad_leg_per_tile = 3.0, taken from the Logistic Point model's own off-network step (lp_offnet_step) so the two off-road prices agree rather than drifting apart; it is dear against a plains road step of 1.0 before the tier discount, which is what keeps a market's edge following the road. (2) TERRAIN: the leg is multiplied by the tile's own traversal weight, so crossing a mountain or open water costs proportionally more than crossing a field. The doc says 'per tile' and is silent on terrain; a flat leg would price an unroaded island tile three steps across the sea exactly as a field three steps across a meadow, which a cold review flagged as the one place the walk ignores a domain every other cost walk in the tree respects.

**Why it matters.** The leg decides how far a market's edge bulges past the end of its roads, and the terrain term decides whether islands and far coasts belong to the market across the water from them. Measured on seed 0 after the change: market_viability pooled G1 92.0 -> 90.1, G1t50 88.1 -> 86.3, G2 182.1 -> 185.0, all five gates still passing, and the same-nation fifth moves 279 of 31581 tiles.

> **RESOLVED.** Ben, 2026-10-23 (the sprint 55 close forms): keep 3.0 x terrain for market attraction, as built. The leg prices catchment only and moves no Logistic Points; off the networks, points supply units alone. Written into LOGISTICS.md section 3b.

### NR-1051 — BL-1311 (ground paint perf) builds the ground bake at /O2 in Debug: a new object library and CMake policy CMP0184
*novel-work · raised 2026-10-17 · from BL-1311 lane report (commit 3af45cdb); CMakeLists.txt*

The four bake files (ground_bake, structure_stamps, route_paint, terrain_palette) now build as their own library at /O2 even in Debug, with runtime checks off; CMake policy CMP0184 is switched on so Debug runtime checks become a per-target property, and /RTC1 is stripped from the cached Debug flags for top-level targets (they keep the checks through the property). IO_OPTIMISE_GROUND_BAKE_IN_DEBUG=OFF restores single-stepping. Existing build trees rebuild once. This cut the Debug seat wait from 750 s to 12.7 s with byte-identical captures.

**Why it matters.** It changes what the Debug build means for one subsystem (no runtime checks, not steppable by default) and touches every build tree; a build-system change is project-wide scope.

> **RESOLVED.** Ben: keep the /O2 ground-bake library in Debug, on by default (IO_OPTIMISE_GROUND_BAKE_IN_DEBUG=ON).

### NR-1052 — BL-1382 band scaling: f(s) = 1 + (s-1)/2 by anchor scale
*decision · raised 2026-10-18 · from PROVINCES.md § The normalising pass*

Ben ruled the band (merge under 24, split over 100) and that it scales with the centre scale 1-5. The scaling form is mine: f(s) = 1 + (s-1)/2, so 1, 1.5, 2, 2.5, 3 for scales 1-5 (village 24-100, scale-5 city 72-300). Chosen because the measured covered-land province mean by anchor scale runs s1 75, s2 88, s3 145 (a scale-3 about twice a scale-1).

**Why it matters.** It sets how much larger a city province may be than a village one, and so how many splits a world takes.

> **RESOLVED.** Ben, 2026-10-23 (the sprint 55 close form): f(s) = 1 + (s-1)/2, x1 to x3, as built.

### NR-1053 — No doc owns who holds a Logistic Points hub placed during generation; a broke major idles its hubs and starves its consumers
*novel-work · raised 2026-10-18 · from BL-1382 economy trace (seed 28), src/world/logistic_points.cpp seed_logistics_sources / place_hub, src/world/state_industry.cpp plan_state_funding*

seed_logistics_sources gives every hub on a major's ground to the nation as state industry (NATIONS.md § State industry: transport is state industry; a losing building idles, never debt). A major leaving generation with treasury 0.0 idles all its hubs at tick 0, so its consumers get 0 Logistic Points and the loss reflex decommissions them. BL-1382's merges moved 181 consumers onto such a major on seed 28 (G1 96.8 -> 44.7). Present before BL-1382 at smaller scale (seeds 9, 32, 40). The seeding report also claims 100% coverage before the funding gate runs.

**Why it matters.** Hub ownership at generation is a design rule no economy doc states; LOGISTICS.md says anyone may build one, NATIONS.md makes transport state industry.

> **RESOLVED.** Ben: B — no major leaves generation on a zero treasury (Pass 7). Foundings keep a year (four ticks) of the state bill; the poorest realms earn more in the history until none closes empty. The LP coverage report reads the funding gate.

### NR-1054 — Seed 40: background firms herd-build oil power plants in one market; the build check does not see same-window input claims
*novel-work · raised 2026-10-19 · from BL-1382 economy trace, seed 40 (G1 t50 74.2 with the province pass, 83.7 without)*

Ticks 23-36: 25 background firms each build an oil power plant in market 47446 (grid 15253), oversubscribing petroleum; 24 of 32 are decommissioned by tick 50. The province pass only reshaped the grids (51 vs 31). The fix reaches the corp-AI build scorer (AI_OPPONENT.md § 11 grants).

**Why it matters.** A herd is a scorer behaviour; changing what the scorer sees is AI work under the grant register, outside the economy brief.

> **RESOLVED.** Ben: accept as world variance (seed 40 G1 t50 74.2). Same form: seed 9's hydroponics charter web accepted as world variance (G1 t50 ~83). No AI grant widened.

### NR-1055 — Tile Logistics section: Uses counts only the player's own buildings
*decision · raised 2026-10-20 · from BL-1354 (tile logistics section), built 2026-10-20*

Ben asked for the points consumed by a tile. A rival building's point need is workforce x target, i.e. its run rate, which DISCOVERY.md keeps private. Taken on Ben's behalf: Uses counts the player's own buildings (god view: all), and the hover names how many rival buildings were left out. SELECTION.md records it.

**Why it matters.** A figure that reads a rival's private state is a visibility call, which is Ben's, not an in-band calibration.

> **RESOLVED.** Ben, 2026-10-23 (the sprint 55 close form): Uses counts the player's own buildings and the hover counts the rivals left out, as built.

### NR-1056 — Rail's fade past 20: kept at 8 (zero at 28), not the road's double-distance rule
*decision · raised 2026-10-20 · from BL-1360 (road tier extends reach), cold review finding 5*

Your ruling gives rail 20 tiles and gives the zero-at-double fade to ROADS (Track 6/12, Road 8/16, Highway 10/20). Rail's fade was left at its existing 8, so a station 24 track steps down the line gets half strength and one at 28+ gets nothing. LOGISTICS.md says only '20 tiles' for rail.

**Why it matters.** It decides whether a station 21-27 steps away is served at all. Today that is a fading partial, and the census moved little on it (rail sources unchanged: 69 / 0 / 185 on seeds 0 / 28 / 46).

> **RESOLVED.** Ben, 2026-10-23 (the sprint 55 close form): rail takes the road's rule - full to 20, zero at double, 40. Written into LOGISTICS.md; the constant change is work (lp_fade_length rail 8 -> 20).

### NR-1057 — Routing: does an isolated hub still add to the nation pool?
*decision · raised 2026-10-21 · from BL-1356 (routed requests), second cold review finding 3*

As built, every running source adds its points to the one nation pool (your 'one nation pool' ruling), connected or not; routing only decides which requests may draw and in what order. So a hub with no chain to the capital delivers to nobody but still raises the pool every connected request draws from. LOGISTICS.md says such a hub 'supplies nothing', which reads as stricter.

**Why it matters.** Decides whether an outlying hub is worthless until connected, or a pool top-up anywhere. Generation step 2 (pool sufficiency) places hubs for pool alone, so it leans on the as-built reading.

> **RESOLVED.** Ben, 2026-10-21 (routing form): b) only connected sources add to the pool. Built in BL-1356: lp_source_row::connected; generation's pool hubs stand only where they join the chain.

### NR-1058 — Routing: does a road hub connect a station it covers, or only the reverse?
*decision · raised 2026-10-21 · from BL-1356 (routed requests), second cold review finding 2*

The chain admits a source when ITS OWN reach touches the capital or a connected source's tile (your wording, built literally). A rail station's field holds only station tiles, so a station joins only through another connected station, or by standing on the capital or a connected source's tile. A connected road hub whose reach covers the station does NOT connect it. So a line between two provincial cities, neither linked by rail to a station at the capital, delivers nothing.

**Why it matters.** Rail is the inter-market carrier; under the literal rule most generated lines may sit outside every chain. Not yet measured per line.

> **RESOLVED.** Ben, 2026-10-21 (routing form): b) a link counts in either direction. Built in BL-1356 (memo_chain); a connected road hub covering a station puts the line on the chain.

### NR-1059 — Routing: rival siting still buys one hub where a chain is needed (an AI grant question)
*question · raised 2026-10-21 · from BL-1356 (routed requests), second cold review finding 1; AI_OPPONENT.md § 11*

corp_ai prices an out-of-reach build at one hub and builds that hub on or beside the site. Under routing, a hub there counts only if it chains back to the capital, so on far ground the rival pays upkeep for a hub that never covers its site. The scorer's INPUT (lp_coverage_at) follows the new rule; its RESPONSE does not.

**Why it matters.** Rivals will over-build isolated hubs on far ground. Teaching the scorer to lay a chain is new AI behaviour, so it is a grant for you to give or refuse, not an in-place fix.

> **RESOLVED.** Ben, 2026-10-21 (routing form): b) granted - the scorer prices and builds the chain. Recorded in AI_OPPONENT.md section 11 'AI lays a hub chain to reach the capital'; the work is BL-1395 (AI_LAYS_HUB_CHAIN).

### NR-1060 — Rival hub chains on the player's nation's ground: builder or ground?
*decision · raised 2026-10-21 · from BL-1395 (rival lays a hub chain), cold review finding 4*

Your grant excludes 'any chain for the player's nation or champion'. The 2026-10-15 grant's like wording is implemented as a BUILDER test (the player's corp and champion never source). Built conservatively: a rival building on the PLAYER's nation's ground keeps the old rule exactly - one hub on or beside the site, priced as one - and never lays a chain there. Elsewhere it lays the full chain.

**Why it matters.** On the player's ground a rival's far site gets one hub that may not connect (so it covers nothing and adds nothing to the pool), while on every other nation's ground it gets a working chain. Reading the wording as builder-only would let rivals lay working chains on the player's ground too - hubs that also feed the player's pool.

> **RESOLVED.** Ben, 2026-10-23 (the sprint 55 close forms): stricter than either reading - a rival may build NOTHING on the player's nation's ground (AI_OPPONENT.md section 11, a narrowing). And a shipment needs the Logistic Points of its WHOLE route, from its market's centre to the market it trades with, international trades included (LOGISTICS.md, SUPPLY.md). Both are work, filed in the backlog.

### NR-1061 — Ports cross sea, not lakes
*decision · raised 2026-10-21 · from BL-1396 (port reach over water), cold review finding 7*

Your rule says a port's points cross 'water'. Built as SEA only (ocean and coast), matching how placement already reads a coast: a lakeshore is not a coast, so a port cannot stand on one, and a sea port's points do not cross a lake to land on its shore.

**Why it matters.** Decides whether a lake joined to the coast by a port reach counts as water the points travel over. Rare on generated worlds; no measured case.

> **RESOLVED.** Ben, 2026-10-21: 'Ports should be able to cover lakes too.' b) any water: the sea walk crosses lakes and lands on their shores; a port still stands only on a sea coast. Built in BL-1396.

### NR-1062 — What a shipment's anchor draws against, now that the pool is the nation's
*decision · raised 2026-10-22 · from BL-1361 (a shipment draws points by cargo), as the handoff asked: decide and log*

Built: the anchor is the SOURCE MARKET'S CENTRE TILE. A shipment draws one point a unit from what the march and production left in the pool of the nation whose ground that tile is; the centre's routed cover caps its share (a half-covered centre ships half of each cargo); a centre on nobody's ground, or one no connected source reaches, ships nothing; under a share rule the trader draws its own class's pot; space legs draw nothing. The rival Marketplace estimate reads the anchor's leftover again (its grant text never stopped saying so) and scales by the cover. Written into LOGISTICS.md section How a tile's points get to it.

**Why it matters.** Trade now waits on logistics at its source. Measured (16-seed market_viability, haulage --far-trade) before -> after: convoys window E 868 -> 791, Y 909 -> 803; units E 7493 -> 6308, Y 7178 -> 5561; G1t50 89.8 -> 88.2 and G2 176.3 -> 158.8, every gate passing; per seed G1t50 under 85 on seeds 12 (79.4), 46 (81.6), 10 (81.8) and 32 (84.4). A market centre generation leaves uncovered ships nothing until somebody builds a source there - which BL-1367's lesser sources at market centres would answer.

> **RESOLVED.** Ben, 2026-10-23 (the logistics funding form): keep the centre tile. a) as built.

### NR-1063 — 'The capital is the peak' read as: every capital covered at full strength
*decision · raised 2026-10-22 · from BL-1367 (where generation puts sources), re-read against routing as the handoff asked*

Your 2026-10-18 rule says no tile of a nation may hold more points than its capital at generation. It was written for a field of points incident at a tile. Since BL-1356 a tile's points arrive as a SHARE of its request, at most all of it, so no tile can outrun a capital covered at full strength. Built: the check is that every capital is covered at full strength, which the state hub generation now stands on every capital guarantees. The census's old points-incident figures (cap_pts / max_pts) are still printed as report, and by that measure a rail station's tile still outruns most capitals.

**Why it matters.** Decides whether the census gate is the routing reading (built; passes on all 16 seeds) or the field reading (fails wherever rail runs).

> **RESOLVED.** Ben, 2026-10-23 (the logistics funding form): the routing reading. a) as built.

### NR-1072 — Network upkeep bills 2-4x the world's stone and timber, so trade buildings starve and auto trade stops by tick 4
*question · raised 2026-10-24 · from Ben's live click (trade from the start) + diagnosis lane, 2026-10-24*

Ben asked for Marketplaces and automatic trade from the start. Generation places them (17 Marketplaces, 11 Ports on seed 0). Two causes stopped the trade: (1) FIXED - the fold discarded the opening stock (c404a607), so shelves opened empty; now 81 then 36 auto shipments go out on ticks 2-3. (2) OPEN - network upkeep (BL-643, the logistics_maintenance line; scripts/economy.lua network_upkeep) bills ~2383 stone and ~1191 timber a tick on seed 0 (3402 highway + 2480 road edges, 130 hubs; seed 1 3065/1532, seed 28 1356/678, seed 46 2346/1173) against ~640 stone / ~580 timber dug. Each nation buys from the largest shelf anywhere, after the clear, so the trade buildings' own upkeep (coal 0.25 / timber 0.15 / stone 0.15 a Marketplace) finds empty shelves and they make no trade points. The fold made it bite: the firms' cash now sits in treasuries, so the maintenance line can afford the whole bill (before, timber drew 5-46 a tick; now 100-1043). It was already partly true before the sprint: pre-fold seed 0 also starved stone by tick 4 and haulage read 0 convoys there - the old 868 came from seeds 1 and 2. Not the cause: no producer idled, Logistic Points fully granted, network code unchanged.

**Why it matters.** Trade - one of the two pillars - stops in the first year on every measured seed, and the logistics network looks broken while the money flows elsewhere.

- A) Re-rate network upkeep down to what the world produces (a constant retune: the 'tens per quarter' figure holds per nation but sums to 2-4x supply).
- B) A nation buys its network upkeep only from its OWN markets, not the largest shelf in the world (a draw rule).
- C) Serve trade-building upkeep before network upkeep (an order of service).
- D) Accept it: trade stops until supply catches up.

> **Recommendation:** A and B together: the rate is an authored first cut ('retune by playtest'), and buying repairs from the far side of the world is the part that drains every trade shelf.

> **RESOLVED.** Ben, 2026-10-24: A and B - network upkeep re-rated to what the world produces (every rate a tenth of the first cut: seed 0 bills ~37% of stone and ~20% of timber dug, in-band) and a nation buys it only from its own markets. Measured: trade runs every tick on seeds 0/28/46 (134-186 points a tick on seed 0); haulage_measure --far-trade 898 / 851 convoys (pre-sprint 868 / 1042), PASS; nation_ai_builds ALL PASS (seed 46 majors build again, BL-1409's headline); market_viability FULL G1 95.6 G1t50 84.6 (0.4 under its 85 target - recorded, not tuned for; in-band calibration rule, Ben 2026-10-13) G1b 2.4 G2 163.0 G3 100.0.

*Files: `scripts/economy.lua`, `src/world/network_upkeep.cpp`, `tools/verify/trade_pass_probe.cpp`*

### NR-1073 — Construction from placement: the first clearing pays two quarters at once, and more sites stall
*question · raised 2026-10-25 · from Construction-in-days lane (c7e2b818, merged 6322792b), construction_stall vs main 2967c7cf*

Per your ruling, a site builds on credit from its placement day, and its first clearing pays the credit days plus the quarter ahead. Every gate passes (market_viability FULL: G1 92.0, G1t50 85.0 - at target for the first time, G1b 2.6, G2 147.8 against 165.8 before, G3 100). But construction_stall reads fewer completions per seed: 721/410/227 against 753/458/324 (seeds 0/43/10). Sites stalled 10+ ticks: 51/81/78 against 32/23/72, and the stall verdicts move from short to contended. Two mechanisms in the code, neither yet isolated by measurement: (1) the credit rate reads the shelf as it stands and reserves nothing, so every site placed in one tick reads the same stock and all get full rate; (2) at the next clearing each owes 90 days in arrears plus 90 ahead, and one rate throttles both, so a short shelf slows the work ahead as well as the debt.

**Why it matters.** A build that should finish a quarter sooner can finish later, because the lumped first draw meets a contended shelf. The ruling is honoured as written; the open question is how the debt competes with the work ahead.

- A) Ahead first: the clearing sets the rate on the quarter ahead alone, and pays arrears only from what is left. The debt carries forward until paid; a building may complete still owing it.
- B) Reserve on credit: a credit site claims its share of the shelf, so later placements in the same tick read what is left. This keeps the lumped draw but stops the over-promise.
- C) Keep as built, and record the stall figures as the cost of the ruling.

> **Recommendation:** A, then measure. It removes the throttle on forward work without changing what is owed. B fixes only the over-promise. Each is a small change in run_construction.

> **RESOLVED.** Ben, 2026-10-25: the entire build cost in credits is taken when the building is placed, once; no payment each quarter. Only the material cost to the market happens over time: each economy tick draws the materials for the days built since the last tick (in arrears; a shortfall stays owed to the next tick) and sets the quarter-ahead rate from the shelf left. A tick never draws two quarters of materials. A site cancelled before completion refunds its credit cost in full (Ben, 2026-10-25); materials already drawn stay spent.

*Files: `src/world/economy_system.cpp`, `tools/verify/construction_stall.cpp`, `docs/economy/PRODUCTION.md`*

### NR-1074 — A site road may bridge at most 2 water tiles; seed 28 keeps 97 overseas sites unconnectable
*decision taken on your behalf · raised 2026-10-25 · from Site-join lane (483fc8ab), lp_coverage_census*

The generation site-join (every building joined to its nation's capital road network) may cross a strait as a bridge of at most 2 water tiles (kMaxCrossingTiles = 2; 3 was tried). With it, seeds 0 and 46 have no unconnectable consumer; seed 28 keeps 97, on ground 8-9 water tiles from the capital. Those need a sea link (the port seeding), not a longer bridge.

**Why it matters.** A longer bridge cap would join more sites but lays roads across open sea, which reads wrong. The 97 sites on seed 28 get no Logistic Points until a sea bridge carries them.

> **Recommendation:** Keep 2. Carry seed 28 as follow-up work: the port seeding bridges an overseas site group to its capital by sea.

> **RESOLVED.** Ben, 2026-10-25: generate port links for nations akin to the seed 28 example. The bridge cap stays at 2; an overseas building group joins its capital chain by a port link.

*Files: `src/world/road_generation.cpp`, `src/world/logistic_points.cpp`*

### NR-1077 — A major nation's capital on a 3-tile landlocked enclave leaves its whole economy off the chain (seed 1)
*question · raised 2026-10-25 · from Port-link lane, seed 1 census on main 3361ccf9*

Seed 1's 12 unconnectable consumers all belong to one major nation. Its capital stands on a 3-tile enclave of its own ground, wholly surrounded by a neighbour's land and touching no coast. The nation's other 1837 dry tiles form one coastal landmass about 46 tiles away, where all 12 consumers stand. The chain never crosses a neighbour's land and a port needs a coast joined to the capital, so the port link cannot serve them. Its Logistic Point pool is the capital hub alone (120); 1837 of 1840 dry tiles get nothing. TRACED (enclave_trace, 2026-10-25): the far landmass is not the realm's provinces but a seedless landmass Pass 2b hung on the carve cell of one province near the seat; the 1959 closing-round cession ceded that province's ground to the neighbour, and the cession moves ground only, so the islet stayed with the cedent and then passed as a remote island. The same pattern makes a 1-tile-capital major on seed 0 once history shifts. Ben's conquest cut-off rule (experiment be2b1287, not merged) removes seed 1's case only by changing the whole history, and roughly triples generation time.

**Why it matters.** A major nation the player may pick opens with its economy dark. LOGISTICS.md already counts ground a neighbour cuts off rather than forcing it, so this is lawful, but it reads as a generation defect.

- A) Nation generation seats the capital on the nation's largest landmass when the seat ground is a tiny enclave.
- D) A cession takes the Pass 2b islets hung on the ceded province with it, so the islet goes to the receiver.
- C) Accept it as geography and keep counting it.
- E) Keep the conquest cut-off rule (experiment be2b1287): it changes every settled world and is about 3x slower to generate.

> **Recommendation:** D. It fixes the producer the trace found, inside the existing cession step, and moves only the ground that rode on the ceded province.

> **RESOLVED.** Ben, 2026-10-25: D. A cession takes the Pass 2b islets hung on the ceded province with it. The conquest cut-off experiment (be2b1287) is dropped.

*Files: `src/world/nation_generation.cpp`, `docs/generation/NATION_GENERATION.md`, `docs/economy/LOGISTICS.md`*

