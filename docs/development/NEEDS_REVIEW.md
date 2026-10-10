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

*23 entries — 23 open, 0 resolved.*

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

---

## Resolved

Kept, not pruned: the reasoning is the point. Prune only in a deliberate sweep, once the
answer has landed in an authority doc.

