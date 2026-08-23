# Project Io — Systems Overview

## Structure

Two pillars define the game's end goals: **Trade** and **Conflict**. Every other system creates the conditions, constraints, or capabilities that flow into one or both.

The player is a **mercenary company** (Ben, 2026-08-12, NR-177; CONCEPT.md § Player identity) — an owner of assets and operations, whose motive is narrower than a governing body's (it does not legislate) and whose agency reaches force directly: what it can field, procured from independent suppliers. The operating **corporations** the player deals with are arm's-length counterparties, not a linked treasury or a shared economic arm; the ones the player's syndicate controls by majority equity are the player's own (BL-524, syndicate tier). Profit is the motive for every corporation, and that shapes the economic layer throughout.

Each system below therefore answers one test: **does it change what the company can field, or what it must answer to?** A system that can only ever move a cost or a price, touching neither, is built for the corporate player the identity moved away from.

Systems are grouped into three supporting tiers below the pillars.

---

## Pillars

### Trade

A corporation sells goods from any location to any market. Markets are pooled exchanges — buyers and sellers anywhere can transact — but logistical cost determines whether a sale is profitable. Base prices are set by global rarity; local supply and demand shift them each Tick. The player exploits price differentials by controlling production locations and minimising the cost of getting goods to market. The player can also place **standing sell orders** — a per-(body, resource) quantity with a **floor price** — which are honoured at market clearing and take precedence over the anonymous auto-sell path, so the player governs how their controlled stock is released rather than dumping it at the resolved price.

**Counterparties are chosen preferentially** (BL-037, preferential purchasing): a buyer's `preferred_seller` hint wins ties at clearing and is honoured when up to 10% more expensive than the cheapest alternative (`src/world/market_clearing.cpp`).

**Intra-body market structure (multiple tile-centred markets).** A market is **centred on a tile** (usually a body's principal/capital tile), and a body may carry **several** markets. A tile clears against the market whose centre is nearest by grid distance — the market's *catchment* (`market_for_tile`, `src/world/market_clearing.cpp`); clearing routes each corp's body-aggregate supply/demand to the market nearest its representative holding. A body with a single market routes there unconditionally. `hard_coded_world.cpp` anchors a market to each qualifying population centre once nations and population centres exist, with the split **resource-carved per nation** (BL-096, resource-carved markets) — a resource-rich nation fractures into more markets, a barren one folds into a neighbour's catchment.

**Inter-body markets.** Each body resolves its market **in isolation**; there is no abstract cross-body price-coupling term, and that isolation is the *point*. Two bodies each keep their **own** locally-resolved market, coupled **only through Supply convoys** (`src/world/supply_system.cpp`): a convoy debits the source pool at dispatch and credits the destination market's supply on arrival, so prices converge or diverge purely as a function of what logistics physically carry, net of logistical cost. The convoy *is* the coupling, and a profitable arbitrage is simply *source price + per-unit logistical cost < destination price*. The convoy mechanics are § Supply below and `docs/economy/SUPPLY.md`; this records the market-side framework.

### Conflict
The player claims, defends, and invades territory through military force. Combat runs concurrently with the economy: supply routes are live targets, and active conflict on a body inhibits its trade connections. Territorial control is both a strategic objective and a source of ongoing economic pressure on opponents.

The **mercenary company** is the actor that commands force — a corporation's levers stop at the economic, which is why force is a different kind of actor's business. BL-315 (conflict spine) owns the loop that makes this pillar load-bearing.

There are **two battle resolvers**, and the split is deliberate (Ben, 2026-08-13). `resolve_battle` (`src/world/combat.cpp`) is nation-scale — class-matchup matrix × formation doctrine × terrain × supply × season, integer per-mille arithmetic, deterministic tie-break — and answers "region beats region, this year" for the Era −1 history sim. `resolve_campaign_battle` (`src/world/campaign_battle.cpp`) is campaign-scale — seeded rounds and priced withdrawal — and is opened by `run_battles` in the economy tick whenever hostile units share a province. Authority: `docs/military/MILITARY.md`.

---

## Productive tier
*The systems the player actively builds and maintains to keep both pillars running.*

### Budget
The corporation's operating balance. Revenue comes from trade margins and asset returns; outgoings cover construction, maintenance, research, workforce contracts, and military operations. Budget is the direct converter between economic output and military capability, and financial collapse is one of the two primary loss vectors.

### Resources
Raw materials are extracted from bodies and processed into higher-tier goods through industrial chains. Output feeds both trade networks and military supply. Starting resource conditions differ between factions, making early industrial priorities a meaningful strategic choice.

### Supply
The physical movement of goods, materiel, and forces through space. Supply is cheap on the homeworld but becomes costly and vulnerable with distance and conflict. A disrupted route halts production, construction, and operations at the destination. The player cannot project power or sustain a colony beyond what their logistics can support.

Layer 5 of the economy is the **convoy layer** — the mechanism that couples bodies through physical cargo movement (BL-039, the convoy layer; `src/world/supply_system.cpp`). It is documented in `docs/economy/SUPPLY.md` and summarised here; the network it runs over — reach, roads, traversal cost, travel time, throughput, interdiction — is `docs/economy/LOGISTICS.md` (*Logistics is the road; Supply is the traffic*).

**Convoy entity.** A convoy is a world component carrying `(source market, destination market, mode {land|sea|air|space}, cargo {resource, qty}, progress 0–1, speed)`. The coupling is market-to-market. A convoy is created when goods are dispatched toward a destination shortfall; it advances `progress` per Tick at a rate set by physical scale and mode (linear; no orbital mechanics); on arrival it credits the destination pool and is retired. Cargo leaves the source pool at **dispatch**, not arrival — goods in transit are committed.

**Logistical cost.** Each convoy incurs a `base_logistics_cost × distance × qty` budget term, drawn from the Lua economy-constants registry. For space convoys, distance is Euclidean body-centre to body-centre. The cost is the term that makes distant arbitrage marginal and grounds the rule that *logistics affects margin*.

**Dispatch trigger.** The **auto path is the default**: the system fills a destination shortfall from the cheapest reachable source without player intervention. Player-direction of individual convoys is reserved for sell-order / buy-match counterparties on another body. **Exception:** space launches — leaving the gravity well — are **always player-directed** and never auto-dispatched, in Era 0 and Era 1 alike. Terrestrial (land / sea / air) convoys auto-dispatch.

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

**Survey system.** A body starts *unsurveyed* — the player sees only its type, orbital position, and grid size, never its tiles or deposits. Dispatching a survey debits credits from the player corporation **upfront**, then plays out over sim ticks (one tick = one day): a **transit** phase (probe en route, nothing revealed) followed by a **scan** phase that reveals the surface **one region at a time** in deterministic raster order until the whole body and its deposit *richness bands* (rich / moderate / sparse) are known. Exact deposit amounts wait for first extraction-site placement. Both **cost and duration scale with body size (tile count) and distance** (heliocentric orbital radius from home; a moon uses its parent's radius) — a near small asteroid is cheap and fast, a far large planet expensive and slow. There is **no concurrency cap**: many bodies may be surveyed at once, with credit cost the only throttle. The reveal is RNG-free (a pure function of grid dimensions), so it is deterministic. The home planet (and the star) start surveyed. Surfaced on the Solar canvas (a survey badge per body), the Planetary canvas (unrevealed regions masked), and the Selection panel (a Survey section with the cost/ETA preview and live progress). Logic lives in `src/world/survey_system.{hpp,cpp}`; the `survey_state` is carried inline on `body_component`. A nation can fund a survey too, through its budget's survey line (§ Policy).

### Environment
Each body, and each subdivision of land within it, has a procedurally generated profile of resources, terrain, hazard, and habitability. Environment sets the local cost of construction, the difficulty of military operations, and extraction yields.

### Generation (Planetology & the chain)
The world is generated, not authored — a deterministic, seeded chain: planetology (atmosphere, chemistry, evolution history — `src/world/planetology.cpp`, BL-167 planetology) → continents → tiles → population centres → history ladder → nations → roads → markets → corporations. Each stage is a consequence of the one upstream, entered through the New World wizard. Authority: `docs/generation/GENERATION_STRATEGY.md` and `docs/generation/PLANETOLOGY.md`.

### History ladder
The institutional history that makes the campaign premise causal rather than asserted. The pre-national ladder (BL-221, `src/world/history_ladder.cpp`) runs upstream of nation generation and *drives* it — counting agrarian cradles, pricing conquest against exit, and setting the nation seed budget (`nation_params_from_ladder`). The industrial ladder (BL-222) and the averted rupture (BL-223) are the later stages. Authority: `docs/lore/HISTORY.md`.

### Force
Units, doctrine, and the ceiling on what the player can raise and sustain. A unit is a `unit_component` with a tile-canonical position, raised through the `hire_unit` corp verb at a `military_base`; province manpower (BL-273: `manpower_ceiling`, `replenish_manpower`, `raise_manpower`) is the Era −1 sim's ceiling. The first campaign lever to reach the system is a technology, not a building: `E0-ML-01` gates the Military Base (§ Research). The levers that reach force from above — conscription, requisition, war powers — are **laws**, and the company does not pass them; it is subject to them. Conscription and war powers are conditions the company's recruitment and operations must satisfy or route around, not instruments it wields. Authority: `docs/military/MILITARY.md`.

### Research
Technology is organised into discrete, modular trees, each unlocked by a visible precondition. Research raises capability ceilings across all systems and competes directly with other budget priorities. The company does not run laboratories: it reaches weapons, logistics and intelligence by buying their output through BL-350's procurement seam, the same as any other equipment. Whether the company itself holds any research capability is an open design question (Sprint 8; CONCEPT.md § Modular tech trees).

**The gate is a predicate.** `tech_node::condition` is a real `condition_set` (§ Conditions below), earned state is **per-corporation** (`world::earned_techs`), and `advance_tech_gates` runs once per economy tick: monotonic (a tech is never un-earned by a later dip below its threshold) and deterministic. The F9 constellation viewer (BL-310) reads the same predicate the simulation enforces.

The founding gate is a **military** one on purpose. `E0-ML-01` "Standing Garrison Doctrine" unlocks `building_type::military_base` (BL-325), gated on two extraction sites plus a Cr 2,000 balance — quantities reachable through ordinary play. That choice is BL-094's design test applied: *a technology that can only unlock a building is being designed for the corporate player the identity moved away from*. The predicate lives in `src/world/tech_gate.cpp` rather than in `scripts/tech_tree.lua`, because a gate that gates construction has to be linkable from the SDL/Lua-free world superset; the Lua file authors identity, topology and prose, and the viewer reads the same predicate the simulation enforces. A node with **no** authored gate reports "not yet earnable" rather than reading as unlocked — an empty `condition_set` is true by definition, so absence is modelled by absence from the gate table, never by an empty predicate.

Research points accumulate at the `research_institute` building (BL-332, military points and research) and are read by the `science` condition subject — **reached, not spent** (META_LAYER.md); BL-478 (ancient research spend) owns the spend side. The constellation grain and deeds are BL-087 (era-1 tech/quest system). Verified by `tools/verify/tech_gate_harness.cpp`.

### Policy
Two different things share this word, and BL-094 separates them.

**Automation policy** — standing rules governing automatic behaviour: trade thresholds, workforce allocation preferences, exchange policy. This is a convenience altitude; it changes a number in the player's own cost model.

**Law** — the load-bearing sense of law in Io. Conscription, requisition, embargo, tariff, war powers: rules that bind actors other than the one who passed them, and that reach force rather than only price. Laws are enacted by nations; the player is a law **subject**, not a legislator — the company is not the legislator, it is bound by law. The single exception is the company's own **negotiated** tax or contract terms with its home nation (BL-280, negotiated tax rate; read against the company's bargaining shape, BL-350).

*Authority: [`docs/politics/NATIONS.md`](politics/NATIONS.md) owns the nation as an actor — the law object, the enforcement seam, the treasury, the national budget, and the 2026-08-18 behaviour grant. What follows is the summary; that doc is the detail.*

**The law object.** A `law` (`src/world/law.{hpp,cpp}`) is an id, a `condition_set`, an effect (`law_effect_kind` — a money-flow family: `extraction_levy`, `import_tariff`) and an `enacted` flag, held on `world::laws` in authored order. The founding instance is BL-155's law #1, the **extraction levy**: a per-unit charge on raw output, enacted by the player's home nation, scoped to that nation's territory, with the debit credited to the author's treasury — a conserved transfer (BL-480). Enactment belongs to the nation actor; the Budget ledger's Laws section is browse-only. The levy surfaces as its own **Levies** line in the Finance card's flow chart, beside income, inputs, maintenance, wages and interest — because a law the player cannot see working is indistinguishable from an unimplemented one.

**The enforcement seam:** *a law is a modifier OVER the market, never an override OF it.* This is the principle established when price clamps were vetoed (2026-07-11) — a clamp fights price resolution rather than shifting a flow's cost. So the levy applies where the flow is **accounted** (`apply_budget`), not where the price is **resolved** (`clear_markets`): extraction output is priced by the market exactly as before, and the levy is a separate accounted cost. The market stays the only thing that sets prices, and the player sees the tax as its own number rather than as an unexplained worse price. Predicates are resolved once per law per corp per tick, before the money loop reads them, so ordering is fixed and the determinism invariant holds.

An extraction levy reaches economic outcomes and not military ones. What matters is that **nothing in the record or the effect dispatch assumes an economic subject** — `law_effect_kind` is an open taxonomy a military effect joins without reshaping anything, and the predicate already carries military subjects. BL-155 (law/policy surface) owns the remaining laws and the three other effect families; BL-186 (laws ledger) the browse surface. Verified by `tools/verify/law_harness.cpp`.

**The nation spends.** A nation holds a treasury (`nation_component::treasury`), credited by the levy and the tariff; the **national budget** (BL-537) allocates it each tick over weighted priority lines, every credit out a direct transfer — so corporations are funded by taxes and a nation can fund a survey. The weights are authored by the **nation scorer** (BL-542, `src/world/nation_ai.cpp`): pure, seeded, a scored-utility layer and never a planner, under the 2026-08-18 grant. **Lobbying** (BL-539) is the reverse channel — a corporation's only reach on law: *you do not pass it, you pay someone who does.*

### Conditions

*Authority: [`META_LAYER.md`](META_LAYER.md) owns the predicate and effect substrate in full — both vocabularies, their properties, and the wiring asymmetry between them. What follows is the overview.*

The shared predicate laws, techs and quests all read (BL-342, condition evaluator) — `src/world/condition_set.{hpp,cpp}`. BL-155 (laws) and BL-156 (techs) had independently settled on the same object, *"a flat AND-list of atomic conditions — no nested OR-mesh"*; one small pure evaluator serves both.

An atomic condition is `<subject> <comparator> <operand>` plus the qualifier its subject reads. Three properties are load-bearing:

1. **Pure and deterministic.** `evaluate` reads a `const world&` and nothing else — no RNG, no clock, no cached mutable state. It runs every tick for every enacted law, so it sits directly on the determinism invariant; where a measure sums over an unordered container it sums in ascending entity-id order.
2. **An empty set is true.** Most laws are unconditional once enacted, so the degenerate case is the *common* case and is the cheap path rather than a bolted-on special case. (The corollary matters: "no gate authored" cannot be represented by an empty set, and is carried by its own flag — see § Research.)
3. **A subject may be MILITARY.** BL-094's design test applied at the foundation. The subjects are `tech_tree.hpp`'s original six labels promoted into resolvable quantities — `research`, `structure`, `stockpile`, `market`, `surplus`, `era` — plus `military_units`, `military_strength` and `science`. A subject enum that enumerated only economic quantities is exactly the failure the identity pivot exists to avoid, and it is far cheaper to avoid at the foundation than to unpick.

Counts compare as integers (so `exactly 3` means three, not "within an epsilon of three"); quantities compare as floats. Verified by `tools/verify/condition_set_harness.cpp`.

---

## Relational tier
*The systems governing interactions with other actors.*

### Diplomacy
Each faction holds a sentiment value toward every other, shaped by trade history, territorial conflict, and ideological alignment. Diplomacy modulates the likelihood and cost of conflict and affects what trade arrangements are available. Military takeover by another faction is one of the two primary loss vectors.

Sentiment is the **substrate** (Ben, 2026-08-22): one directed, continuous, derived value from an observer to a subject, corporations and nations alike (`src/world/sentiment.{hpp,cpp}`, BL-545). **Stance** — hostility directed, friendship symmetric — is the declared, discrete layer on top, and sentiment informs a declaration but never makes one. Authority: `docs/politics/RELATIONS.md`.

### AI opponent
Rival corporations act: BL-079 (background-corp agency) gives narrow per-building reflexes, and BL-202 (corp AI stage A) layers a deterministic scored-utility evaluation over the corp-command seam (`src/world/corp_ai.cpp`), with BL-203 (predictive spending) on top of it. Nations act in the same shape (BL-542, nation scorer).

Above that core sits the **language layer** (direction settled 2026-08-03): a small local model playing through the word interface below, socketed by the Io MCP server (BL-278). Cloud models generate the fine-tuning corpus (BL-279); they never play, and the engine ships no network client. Authority: `docs/ai/AI_OPPONENT.md` § 10; the scoped limits live in `.claude/rules/io-standing-rules.md`.

### Comms
The channel-based chat log — the surface of the diplomacy-as-communication principle: since every rival is AI, inter-corp coordination happens in a visible medium, and the mechanical actions of background corps surface as messages first. `src/ui/chat_panel.{hpp,cpp}`; authority: `docs/ui/CHAT.md`.

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
| **Territory / politics** | force without law is banditry | jurisdiction, treasury, lobbying | attention |

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
legs plus a socket: **read** — the corp blackboard export (BL-206, `--export-blackboard`);
**meaning** — the action dictionary (BL-270, `docs/ai/ACTIONS.json`, every control as
`{press, args, preconditions, expected_output, reason_to_select}`); **write** — the corp-command
seam (`src/world/corp_command.hpp`, the `corp_verb` family, rejections returned as data); and the
**socket**, the MCP server (BL-278, `ProjectIo --serve` plus `tools/mcp/`). Any change to a control,
lens, ledger or panel must update its dictionary entry — a stale entry misleads an AI player the
way a stale golden misleads a visual check.

**The broad economy is background firms'; the player and rivals are specialists.** The campaign
opens on a saturated economy whose bulk industrial base is owned and run by **real background
corporations** (`corporation_component.is_background`, BL-365, living-world background industry) —
*not* by a nation actor, and not by an abstract nation substrate. The player and the major AI rivals
are **specialist corporations** occupying a focused slice of the chain — which is what keeps the loop
a contest between a few specialists rather than a full-economy management game. Detail and the
generation consequences live in `docs/generation/GENERATION_STRATEGY.md`.

This premise holds under the player's identity: the company is chartered by one of the generated
nations, and the bulk economy on that nation's territory stays background-firm-run — the company is
an entity type attached to a nation, not the nation itself, so nothing about the background-economy
premise changes. The company gains force directly and reaches law/research through procurement, not
by wielding them (CONCEPT.md § Player identity). The player is not a full-economy management burden
either way.
