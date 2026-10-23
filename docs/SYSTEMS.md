# Project Io — Systems Overview

> **Settles:** which systems exist and how they relate · which are load-bearing and which are
> flavour · what test a system must pass to earn its place · which doc owns each system's
> detail.
> **Not here:** who the player is and how it should feel (CONCEPT) · the rules of any one system
> (its own doc) · what a term means (GLOSSARY).
> **Confused with:** CONCEPT.md, MANUAL.md, META_LAYER.md.

## Structure

Two pillars define the game's end goals: **Trade** and **Conflict**. Every other system creates the conditions, constraints, or capabilities that flow into one or both.

The player is a **major nation, and it builds its own buildings** (Ben, 2026-10-23; CONCEPT.md § Player identity). As the state it sets its budget, tariffs and law, and it raises its own units, buying their equipment off the market's shelf. As the builder it owns every building on its ground, paid from its treasury, to find the ground worth industrialising and build the machinery that gets to space. There are no corporations or companies, and nothing is bought out: every building is owned by the nation whose ground it stands on (`docs/politics/NATIONS.md` § What a nation owns). A nation builds only on its own ground, and the player's economy grows only by the player's builds.

Each system below answers one test: **does it change what the nation can field, or what it must answer to?** The nation is the player's major nation, answering to other nations and its population (CONCEPT.md § Player identity).

Systems are grouped into three supporting tiers below the pillars.

---

## Pillars

### Trade

A nation holds no stock: everything its buildings make **lands on their market's shelf and is sold there**, at that tick's clearing price, and everything it uses it buys off a shelf at the posted price (`docs/economy/MARKETS.md` § The shelf economy). Base prices are set by global rarity; local supply and demand shift them each Tick. Goods move between markets **only by trade** (`docs/economy/TRADE.md`): a nation's Planetary Marketplaces and Ports make **trade points**, and a trade spends them to buy on one shelf, haul, and land on another, keeping the margin net of the haul. The player exploits price differentials by choosing where to produce and which routes to trade — setting manual trades and reserving points from auto — while logistical cost decides which routes pay.

**Intra-body market structure (multiple tile-centred markets).** A market is **centred on a tile** (usually a body's principal/capital tile), and a body may carry **several** markets. A tile clears against the market whose centre is nearest by grid distance — the market's *catchment* (`market_for_tile`, `src/world/market_clearing.cpp`); a building sells into and buys from the market whose catchment holds its tile. A body with a single market routes there unconditionally. `hard_coded_world.cpp` anchors a market to each qualifying population centre once nations and population centres exist, with the split **resource-carved per nation** (BL-096, resource-carved markets) — a resource-rich nation fractures into more markets, a barren one folds into a neighbour's catchment.

**Inter-body markets.** Each body resolves its market **in isolation**; there is no abstract cross-body price-coupling term, and that isolation is the *point*. Two bodies each keep their **own** locally-resolved market, coupled **only through trade** (`docs/economy/TRADE.md`): a trade buys off the source shelf, and its convoy lands the cargo on the destination market, where it is sold at that tick's clear, so prices converge or diverge purely as a function of what logistics physically carry, net of logistical cost. Trade *is* the coupling, and a profitable route is simply *source price + per-unit logistical cost < destination price*. The shipment mechanics are § Supply below and `docs/economy/SUPPLY.md`; this records the market-side framework.

### Conflict
The player claims, defends, and invades territory through military force. Combat runs concurrently with the economy: supply routes are live targets, and active conflict on a body inhibits its trade connections. Territorial control is both a strategic objective and a source of ongoing economic pressure on opponents.

A **nation** commands force by raising it and buying its equipment, and the design test is what keeps that force from being flavour on an economy. BL-315 (conflict spine) owns the loop that makes this pillar load-bearing.

There are **two battle resolvers**, and the split is deliberate (Ben, 2026-08-13). `resolve_battle` (`src/world/combat.cpp`) is nation-scale — class-matchup matrix × formation doctrine × terrain × supply × season, integer per-mille arithmetic, deterministic tie-break — and answers "region beats region, this year" for the Era −1 history sim. `resolve_campaign_battle` (`src/world/campaign_battle.cpp`) is campaign-scale — seeded rounds and priced withdrawal — and is opened by `run_battles` in the economy tick whenever hostile units share a province. Authority: `docs/military/MILITARY.md`.

---

## Productive tier
*The systems the player actively builds and maintains to keep both pillars running.*

### Budget
The nation's treasury and its weighted budget. Revenue comes from what its buildings sell, its wage tax and its tariffs; outgoings cover construction, inputs, wages, maintenance, research and military operations. Budget is the direct converter between economic output and military capability. A nation never runs into debt: a building its treasury cannot pay for idles, network nodes paid first (`docs/politics/NATIONS.md` § What a nation owns). Financial collapse — a treasury that can pay for little, so its industry idles — stays one of the two primary loss vectors (CONCEPT.md).

### Resources
Raw materials are extracted from bodies and processed into higher-tier goods through industrial chains. Output feeds both trade networks and military supply. Starting resource conditions differ between factions, making early industrial priorities a meaningful strategic choice.

### Supply
The physical movement of goods, materiel, and forces through space. Supply is cheap on the homeworld but becomes costly and vulnerable with distance and conflict. A disrupted route halts production, construction, and operations at the destination. The player cannot project power or sustain a colony beyond what their logistics can support.

Layer 5 of the economy is the **convoy layer** — the mechanism that couples bodies through physical cargo movement (BL-039, the convoy layer; `src/world/supply_system.cpp`). It is documented in `docs/economy/SUPPLY.md` and summarised here; the network it runs over — reach, roads, traversal cost, travel time, throughput, interdiction — is `docs/economy/LOGISTICS.md` (*Logistics is the road; Supply is the traffic*).

**Convoy entity.** A convoy is a world component carrying `(source market, destination market, mode {land|sea|air|space}, cargo {resource, qty}, progress 0–1, speed)`. The coupling is market-to-market. A convoy is a trade's shipment, created when a trade buys its cargo off the source shelf; it advances `progress` per Tick at a rate set by physical scale and mode (linear; no orbital mechanics); on arrival the cargo lands on the destination market and is sold there, and the convoy is retired. Cargo leaves the source shelf when the shipment **leaves**, not on arrival — goods in transit are committed.

**Logistical cost.** Each convoy incurs a `base_logistics_cost × distance × qty` budget term, drawn from the Lua economy-constants registry. For space convoys, distance is Euclidean body-centre to body-centre. The cost is the term that makes distant arbitrage marginal and grounds the rule that *logistics affects margin*.

**What ships.** A trade decides — manual routes the owner sets, and auto, which spends the unreserved trade points on the best-margin routes each tick (`docs/economy/TRADE.md` § Auto and reserved trade). A trade between bodies needs the trader's Launchpad on the source body and propellant bought off the source shelf for each launch.

**Infrastructure gates per mode.**
- **Land** — ungated; always available across contiguous land on a body. Roads are a per-tile cost-reducer: the three-tier Track/Road/Highway ladder (BL-146–149, road network + hubs; generated lattice in `src/world/road_generation.cpp` plus player placement; `tile_component.road_level` discounts A* traversal).
- **Sea** — gated on a **Port** building at both endpoints.
- **Air** — gated on an **Airfield** building at both endpoints.
- **Space** — gated on a **Launchpad** at the origin and an **Orbital Port** at the destination; requires **Era 1** (per `docs/economy/ERAS.md`).

Per-mode `base_logistics_cost` multipliers live in `scripts/economy.lua` (land < sea < air < space). Per-node throughput — what the network permits *per tick*, as opposed to whether a place can be reached — is **Logistic Points** (BL-464, logistic points): a per-tick rate not a stock, a cap not a price, the node half only (`docs/economy/LOGISTICS.md` § Logistic Points).

> **Design-reference note — the target logistics feel (2026-06-15).** The long-term direction for the **land / sea / air** logistic-strength model is an **emanation / cross-section "fuel" model**: supply radiates from sources and **attenuates across distance and terrain** (a continuous supply *field*, contested along its path), and the same logistics carry **goods, unit supply, and population supply** — not just discrete point-to-point goods convoys. **Space is a separate, larger consideration** (the convoy/launch model stands for it). The reference for the desired feel — despite the genre, theme, and tonal difference — is **Shadow Empire**'s logistics. This is a durable design-direction note, not a backlog item; the per-mode-cost convoy model is the base it grows from.

### Infrastructure
The physical substrate that makes all other systems possible: surface installations, orbital facilities, ports, and relay stations. Infrastructure determines the efficiency and capacity of every system in a given location. Damage has cascading consequences distinct from battlefield losses, and construction cost is shaped by local environment.

The convoy-mode gates above were settled 2026-06-15 ([B4]): land ungated with road as a cost-reducing tile attribute; sea on a Port at both endpoints; air on an Airfield; space on a Launchpad at the origin and an Orbital Port at the destination, Era 1 required. Full detail in `docs/economy/SUPPLY.md`.

### Workforce
The labour layer that operates extraction, production, and military assets. The player allocates workforce across competing priorities; shortages create bottlenecks throughout the productive tier. Military units draw from the same pool, creating tension between economic output and force size.

---

## Discovery and modifier tier
*The systems that reveal what is available and set the local cost profile of action.*

### Exploration
Exploration reveals bodies, their resource profiles, and their suitability for settlement or extraction. It is a prerequisite for expansion and an early-game decision: where to look first, and how much to invest in surveying versus exploiting what is already known.

**Survey system.** A body starts *unsurveyed* — the player sees only its type, orbital position, and grid size, never its tiles or deposits. Dispatching a survey debits credits from the player nation's treasury **upfront**, then plays out over sim ticks (one tick = one day): a **transit** phase (probe en route, nothing revealed) followed by a **scan** phase that reveals the surface **one region at a time** in deterministic raster order until the whole body and its deposit *richness bands* (rich / moderate / sparse) are known. Exact deposit amounts wait for first extraction-site placement. Both **cost and duration scale with body size (tile count) and distance** (heliocentric orbital radius from home; a moon uses its parent's radius) — a near small asteroid is cheap and fast, a far large planet expensive and slow. There is **no concurrency cap**: many bodies may be surveyed at once, with credit cost the only throttle. The reveal is RNG-free (a pure function of grid dimensions), so it is deterministic. The home planet (and the star) start surveyed. Surfaced on the Solar canvas (a survey badge per body), the Planetary canvas (unrevealed regions masked), and the Selection panel (a Survey section with the cost/ETA preview and live progress). Logic lives in `src/world/survey_system.{hpp,cpp}`; the `survey_state` is carried inline on `body_component`. A nation can fund a survey too, through its budget's survey line (§ Policy).

### Environment
Each body, and each subdivision of land within it, has a procedurally generated profile of resources, terrain, hazard, and habitability. Environment sets the local cost of construction, the difficulty of military operations, and extraction yields.

**Climate** is the living half of that profile: a per-body commons, altered by the strain of what
nations extract and process on it, running on **planetology's own scalars — instellation,
surface temperature, arable share, biosphere stage — unfrozen**. It is the **next Era's
catastrophe** (`docs/economy/ERAS.md` § The point of an Era) and is designed ahead in
`docs/CLIMATE.md`; it is **outside prototype scope** and nothing in the prototype builds against it.
Note it does not run on the tile hazard figure, which is the local operating-difficulty number and
whose design job is the off-world settlement calculus.

### Generation (Planetology & the chain)
The world is generated, not authored — a deterministic, seeded chain: planetology (atmosphere, chemistry, evolution history — `src/world/planetology.cpp`, BL-167 planetology) → continents → tiles → population centres → history ladder → nations → roads → markets → charter buildings, each owned by its ground nation. Each stage is a consequence of the one upstream, entered through the New World wizard. Authority: `docs/generation/GENERATION_STRATEGY.md` and `docs/generation/PLANETOLOGY.md`.

### History ladder
The institutional history that makes the campaign premise causal rather than asserted. The pre-national ladder (BL-221, `src/world/history_ladder.cpp`) runs upstream of nation generation and *drives* it — counting agrarian cradles, pricing conquest against exit, and setting the nation seed budget (`nation_params_from_ladder`). The industrial ladder (BL-222) and the averted rupture (BL-223) are the later stages. Authority: `docs/lore/HISTORY.md`.

### Force
Units, doctrine, and the ceiling on what the player can raise and sustain. A unit is a `unit_component` with a tile-canonical position, raised through the `hire_unit` verb at a nation's own `military_base`; province manpower (BL-273: `manpower_ceiling`, `replenish_manpower`, `raise_manpower`) is the Era −1 sim's ceiling. No technology closes the Military Base: it is open to every nation from the start — NR-1022 (old gates open; § Research). The levers that reach force from above — conscription, requisition, war powers — are **laws**, and a nation authors them for its own ground; on another nation's ground they bind it. Authority: `docs/military/MILITARY.md`.

### Research
Technology is organised into one tree per age, keyed by the previous age's quests (`docs/economy/RESEARCH.md`; the first is `docs/economy/tech/DIGITISATION_TREE.md`). **The nation holds technology, and every nation starts the campaign at the same tech level, holding nothing (Ben, 2026-10-10)** — the pre-game history runs no tree (`docs/generation/trees/TREES.md`). Research points accrue to the nation from its research budget line and its Universities and are spent on nodes from a queue. Research raises capability ceilings across all systems and competes directly with other budget priorities.

**A deed is a predicate.** A quest's deed is a real `condition_set` (§ Conditions below), read from state the game already carries; the holder is the nation (`docs/economy/RESEARCH.md` § Quests key the next age), and the quarterly research pass reads it: monotonic (a node once held is never lost to a later dip below its threshold) and deterministic. The research surface (F9) reads the same deed the simulation enforces.

**A building or recipe no node unlocks is open to every nation.** The Military Base, the Toolmaker, the Bessemer Converter and `refined_copper` are open from the start — NR-1022 (old gates open, Ben, 2026-10-10). A node closes a type or a recipe only through its unlock effect, and a nation clears it when it holds the node (`src/world/nation_tech.hpp` § `structure_unlocked`, `recipe_unlocked`). The tree's JSON store compiles into a header (`src/world/digitisation_tree_data.hpp`) rather than loading through Lua, because an unlock that gates construction has to be linkable from the SDL/Lua-free world superset.

Research points are the nation's and are **spent** on nodes (`docs/economy/RESEARCH.md` § Research points); the `research` condition subject reads whether a nation holds a node (META_LAYER.md). Deeds are BL-087 (era-1 tech/quest system). Verified by `tools/verify/nation_tech_harness.cpp`.

### Policy
Two different things share this word, and BL-094 separates them.

**Automation policy** — standing rules governing automatic behaviour: trade thresholds, workforce allocation preferences, exchange policy. This is a convenience altitude; it changes a number in the player's own cost model.

**Law** — the load-bearing sense of law in Io. Conscription, requisition, embargo, tariff, war powers: rules that bind actors other than the one who passed them, and that reach force rather than only price. Laws are enacted by nations, and the player is one: it is the legislator for its own nation and bound by every other nation's law (`docs/politics/NATIONS.md` § What a nation is).

*Authority: [`docs/politics/NATIONS.md`](politics/NATIONS.md) owns the nation as an actor — the law object, the enforcement seam, the treasury, the national budget, and the 2026-08-18 behaviour grant. What follows is the summary; that doc is the detail.*

**The law object.** A `law` (`src/world/law.{hpp,cpp}`) is an id, a `condition_set`, an effect (`law_effect_kind` — a money-flow family: `import_tariff`, and the retired `extraction_levy`), an author and an `enacted` flag, held on `world::laws` in authored order. The **extraction levy** is retired (Ben, 2026-10-23): it charged a firm for what it extracted on its author's ground, and with no firms a nation would charge itself. The laws a world opens with are its **import tariffs** — a duty on a foreign nation's goods entering the enacting nation's market, paid by that foreign nation to the market nation (`docs/politics/NATIONS.md` § The import tariff). Enactment belongs to the nation actor; the Budget ledger's Laws section is browse-only. A law's charge surfaces as its own line in the Finance card's flow chart — because a law the player cannot see working is indistinguishable from an unimplemented one.

**The enforcement seam:** *a law is a modifier OVER the market, never an override OF it.* This is the principle established when price clamps were vetoed (2026-07-11) — a clamp fights price resolution rather than shifting a flow's cost. So a law's charge applies where the flow is **accounted**, not where the price is **resolved** (`clear_markets`): goods are priced by the market exactly as before, and the charge is a separate accounted cost. The market stays the only thing that sets prices, and the player sees the tax as its own number rather than as an unexplained worse price. Predicates are resolved once per law per tick, before the money loop reads them, so ordering is fixed and the determinism invariant holds.

A tariff reaches economic outcomes and not military ones. What matters is that **nothing in the record or the effect dispatch assumes an economic subject** — `law_effect_kind` is an open taxonomy a military effect joins without reshaping anything, and the predicate already carries military subjects. BL-155 (law/policy surface) owns the remaining laws and the three other effect families; BL-186 (laws ledger) the browse surface. Verified by `tools/verify/law_harness.cpp`.

**The nation spends.** A nation holds a treasury (`nation_component::treasury`), credited by what its buildings sell, its wage tax and its tariffs; the **national budget** (BL-537, national budget) allocates it each tick over weighted priority lines — so a nation can fund a survey. The weights are authored by the **nation scorer** (BL-542, nation scorer; `src/world/nation_ai.cpp`): pure, seeded, a scored-utility layer and never a planner, under the 2026-08-18 grant. **Lobbying** — a corporation's paid reach on law — is retired with the corporations (Ben, 2026-10-23).

### Conditions

*Authority: [`META_LAYER.md`](META_LAYER.md) owns the predicate and effect substrate in full — both vocabularies, their properties, and the wiring asymmetry between them. What follows is the overview.*

The shared predicate laws, techs and quests all read (BL-342, condition evaluator) — `src/world/condition_set.{hpp,cpp}`. BL-155 (laws) and BL-156 (techs) had independently settled on the same object, *"a flat AND-list of atomic conditions — no nested OR-mesh"*; one small pure evaluator serves both.

An atomic condition is `<subject> <comparator> <operand>` plus the qualifier its subject reads. Three properties are load-bearing:

1. **Pure and deterministic.** `evaluate` reads a `const world&` and nothing else — no RNG, no clock, no cached mutable state. It runs every tick for every enacted law, so it sits directly on the determinism invariant; where a measure sums over an unordered container it sums in ascending entity-id order.
2. **An empty set is true.** Most laws are unconditional once enacted, so the degenerate case is the *common* case and is the cheap path rather than a bolted-on special case.
3. **A subject may be MILITARY.** BL-094's design test applied at the foundation. The first six subjects are the original tech tree's six labels promoted into resolvable quantities — `research`, `structure`, `stockpile`, `market`, `surplus`, `era` — joined by `military_units` and `military_strength`.
 A subject enum that enumerated only economic quantities is exactly the failure the identity pivot exists to avoid, and it is far cheaper to avoid at the foundation than to unpick.

Counts compare as integers (so `exactly 3` means three, not "within an epsilon of three"); quantities compare as floats. Verified by `tools/verify/condition_set_harness.cpp`.

---

## Relational tier
*The systems governing interactions with other actors.*

### Diplomacy
Each faction holds a sentiment value toward every other, shaped by trade history, territorial conflict, and ideological alignment. Diplomacy modulates the likelihood and cost of conflict and affects what trade arrangements are available. Military takeover by another faction is one of the two primary loss vectors.

Sentiment is the **substrate** (Ben, 2026-08-22): one directed, continuous, derived value from one nation to another (`src/world/sentiment.{hpp,cpp}`, BL-545). **Stance** — hostility directed, friendship symmetric — is the declared, discrete layer on top, and sentiment informs a declaration but never makes one. Authority: `docs/politics/RELATIONS.md`.

### AI opponent
Rival nations, majors and minors, act (Ben, 2026-10-23): narrow per-building reflexes, and a deterministic scored-utility **build layer** over each nation's own industry on its own ground (the code still lives in `src/world/corp_ai.cpp`), with BL-203 (predictive spending) on top of it. The budget is the same nation's second faculty (BL-542, nation scorer). The player's nation is never auto-acted on strategically, save the one opt-out dial, and nothing builds on its ground for it.

Above that core sits the **language layer** (direction settled 2026-08-03): a small local model playing through the word interface below, socketed by the Io MCP server (BL-278). Cloud models generate the fine-tuning corpus (BL-279); they never play, and the engine ships no network client. Authority: `docs/ai/AI_OPPONENT.md` § 10; the scoped limits live in `.claude/rules/io-standing-rules.md`.

### Comms
The channel-based chat log — the surface of the diplomacy-as-communication principle: since every rival is AI, coordination between nations happens in a visible medium, and the mechanical actions of rival nations surface as messages first. `src/ui/chat_panel.{hpp,cpp}`; authority: `docs/ui/CHAT.md`.

---

## The progression chain

> **Each system's ceiling is the next system's door.**
> — the shape Ben named on 2026-08-22: *"we really want interconnectivity, so a player only
> progresses so far using one system before the next becomes a natural consequence."*

This is a **design test**. It applies to every system in this document and to every new one: *what
forces a player into it, what does it open, and what does it cap them at?* A system that answers only
the middle question is a feature. A system that answers all three is a rung.

The chain, and each rung's ceiling:

| Rung | You enter because | It opens | Its ceiling |
|---|---|---|---|
| **Extraction** | you start with a tile | output, and a balance | one tile's deposits run thin |
| **Logistics** | the good tile is far from the market | distant markets and deposits | **throughput** — the network says *can this be reached* and, through Logistic Points, *how much can move* |
| **Markets** | output is worth more elsewhere | price, arbitrage, scale | anonymous, instant, price-only — no lead time, no memory |
| **Contracts** | you need a counterparty who can refuse | equipment you cannot make; **income that is not extraction** | reputation |
| **Force** | a contract asks for an outcome, not a good | territory, interdiction, a third name | supply — and supply is the logistics rung again |
| **Territory / politics** | force without law is banditry | jurisdiction, treasury, law | attention |

**Two things the table is meant to make visible.**

**The chain closes rather than ending.** Force's ceiling is supply, which is the logistics rung
again at a higher grain — which is exactly BL-325 ruling 3 (*economic reach IS military reach*)
read as progression rather than as architecture. That is why Logistic Points (BL-464) is a bigger
item than its size suggests: it is the rung the whole chain turns on.

**A staircase is a solved sequence — but the variance is bounded.** A chain each rung of which is
unlocked by the last is learned once and then followed, so [`EVENTS.md`](EVENTS.md) is the argument
for something cutting across it. **Settled 2026-08-22 (Ben): variance in texture, never in the
sequence itself.** A learnable order is the point; what varies is how a given rung *feels* on a
given campaign, not which rung comes next.

His tone ruling for events belongs here too, because it is really a statement about the chain:
**"Events should usually be boring. Occasional high stress chains."** The default register is low,
and pressure arrives as a linked sequence rather than as one large roll.

*Both claims in this section were checked with Ben on 2026-08-22 (NR-530): the chain closing was
confirmed, and the staircase argument was confirmed with the bound above.*

---

## Cross-cutting notes

**Supply spans both pillars.** It is the connective tissue between economic production and military projection, and a target in both. It deserves particular attention during prototype scoping.

**Environment is load-bearing but silent.** Procedurally generated per tile, it sets the difficulty and character of every other system's operation locally, and directly drives replay variance.

**Budget is the primary pressure point.** All systems compete for it. Allocation decisions — research versus military versus infrastructure — define strategic identity and vulnerability to both loss conditions.

**The word interface is a system in its own right.** The game is drivable by text through three
legs plus a socket: **read** — the blackboard export (BL-206, blackboard export; `--export-blackboard`);
**meaning** — the action dictionary (BL-270, `docs/ai/ACTIONS.json`, every control as
`{press, args, preconditions, expected_output, reason_to_select}`); **write** — the command
seam (`src/world/corp_command.hpp`, the `corp_verb` family, rejections returned as data); and the
**socket**, the MCP server (BL-278, `ProjectIo --serve` plus `tools/mcp/`). Any change to a control,
lens, ledger or panel must update its dictionary entry — a stale entry misleads an AI player the
way a stale golden misleads a visual check.

**Every nation owns the economy on its ground.** The campaign opens on a saturated economy, and its
whole industrial base is owned by the nations whose ground it stands on (Ben, 2026-10-23: *"We can
remove the corp/company layer entirely. Just have nations building their own buildings"*). The
former premise — a bulk economy run by background firms, with the player and the rivals as specialist
corporations on a slice of the chain — is retired with the firms. Every building the player opens
with keeps running, its workforce stepped by the game, so the player's levers are what it builds next
(`docs/politics/NATIONS.md` § What a nation owns). The generation consequences live in
`docs/generation/GENERATION_STRATEGY.md`.
