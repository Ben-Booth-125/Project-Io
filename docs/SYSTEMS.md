# Project Io — Systems Overview

## Structure

Two pillars define the game's end goals: **Trade** and **Conflict**. Every other system creates the conditions, constraints, or capabilities that flow into one or both.

The player is a corporate entity today — an owner of assets and operations — and that shapes the economic layer throughout: for the chartered corp, profit is the motive. The stated aim is to play a **national private militia** (BL-094, rewritten 2026-08-10 per NR-120), whose motive is narrower than a governing body's would have been — it does not legislate — but whose agency reaches force directly: what it can field, procured from independent suppliers.

> **Corrected 2026-08-10 (dated note, authority time-slice — DELIVERY.md § Design state).** This section read "governing body" from 2026-08-04. Superseded, not extended — see BL-094's 2026-08-10 rewrite. **The corp-as-shared-economic-arm assumption in the old framing is specifically retracted**: private companies (including today's chartered corp) become arm's-length counterparties the militia contracts with, not a linked treasury. Full propagation waits for the work to land.

Each system below therefore answers a **replaced** test: **does it change what the militia can field, or what it must answer to?** A system that can only ever move a cost or a price, touching neither, is built for the player we have now pivoted away from twice.

Systems are grouped into three supporting tiers below the pillars.

---

## Pillars

### Trade

The player's corporation sells goods from any location to any market. Markets are pooled exchanges — buyers and sellers anywhere can transact — but logistical cost determines whether a sale is profitable. Base prices are set by global rarity; local supply and demand shift them each Tick. The player exploits price differentials by controlling production locations and minimising the cost of getting goods to market. The player can also place **standing sell orders** — a per-(body, resource) quantity with a **floor price** — which are honoured at market clearing and take precedence over the anonymous auto-sell path, so the player governs how their controlled stock is released rather than dumping it at the resolved price.

Choosing **counterparties** preferentially shipped with BL-037 (preferential purchasing): a buyer's `preferred_seller` hint wins ties at clearing and is honoured when up to 10% more expensive than the cheapest alternative (`src/world/market_clearing.cpp`).

**Intra-body market structure (multiple tile-centred markets).** A market is **centred on a tile** (usually a body's principal/capital tile), and a body may carry **several** markets. A tile clears against the market whose centre is nearest by grid distance — the market's *catchment* (`market_for_tile`, `src/world/market_clearing.cpp`); clearing routes each corp's body-aggregate supply/demand to the market nearest its representative holding. A body with a single market routes there unconditionally. Multiple centres are seeded for real (landed across v0.0.5–v0.0.6): `hard_coded_world.cpp` anchors a market to each qualifying population centre once nations and population centres exist, with the split **resource-carved per nation** (BL-096, resource-carved markets) — a resource-rich nation fractures into more markets, a barren one folds into a neighbour's catchment.

**Inter-body markets (landed v0.0.6 — Selene worked example).** Each body resolves its market **in isolation**; there is no abstract cross-body price-coupling term, and that isolation is the *point*. Kepler and Selene each keep their **own** locally-resolved market, coupled **only through Supply convoys** (`src/world/supply_system.cpp`): a convoy debits the source pool at dispatch and credits the destination market's supply on arrival, so prices converge or diverge purely as a function of what logistics physically carry, net of logistical cost. The convoy *is* the coupling, and a profitable arbitrage is simply *source price + per-unit logistical cost < destination price*. The convoy mechanics are § Supply below and `docs/economy/SUPPLY.md`; this records the market-side framework.

### Conflict
The player claims, defends, and invades territory through military force. Combat runs concurrently with the economy: supply routes are live targets, and active conflict on a body inhibits its trade connections. Territorial control is both a strategic objective and a source of ongoing economic pressure on opponents.

The **national private militia** *(corrected 2026-08-10, NR-120; was "governing body")* is the actor that commands force — a corporation's levers stop at the economic, which is why this pillar has stayed thin. BL-094's rewrite and BL-315 (the militia's conflict spine) are Conflict's route to being load-bearing.

**Build status.** A battle resolver ships — BL-272, `src/world/combat.cpp`: class-matchup matrix × formation doctrine × terrain × supply × season, integer arithmetic, deterministic tie-break. It is consumed by the Era −1 history sim (BL-271), not by the campaign layer. Nothing in Era 0 commands a unit yet; BL-157 (military datamodel stub) is the first campaign-side rung.

---

## Productive tier
*The systems the player actively builds and maintains to keep both pillars running.*

### Budget
The corporation's operating balance. Revenue comes from trade margins and asset returns; outgoings cover construction, maintenance, research, workforce contracts, and military operations. Budget is the direct converter between economic output and military capability, and financial collapse is one of the two primary loss vectors.

### Resources
Raw materials are extracted from bodies and processed into higher-tier goods through industrial chains. Output feeds both trade networks and military supply. Starting resource conditions differ between factions, making early industrial priorities a meaningful strategic choice.

### Supply
The physical movement of goods, materiel, and forces through space. Supply is cheap on the homeworld but becomes costly and vulnerable with distance and conflict. A disrupted route halts production, construction, and operations at the destination. The player cannot project power or sustain a colony beyond what their logistics can support.

Layer 5 of the economy is the **convoy layer** — the mechanism that couples bodies through physical cargo movement. The prototype model (BL-039, the convoy layer; landed v0.0.6, `src/world/supply_system.cpp`) is documented in `docs/economy/SUPPLY.md` and summarised here.

**Convoy entity.** A convoy is a world component carrying `(source market, destination market, mode {land|sea|air|space}, cargo {resource, qty}, progress 0–1, speed)`. The coupling is market-to-market. A convoy is created when goods are dispatched toward a destination shortfall; it advances a fixed fraction of `progress` per Tick (linear; no orbital mechanics in the prototype); on arrival it credits the destination pool and is retired. Cargo leaves the source pool at **dispatch**, not arrival — goods in transit are committed.

**Logistical cost.** Each convoy incurs a `base_logistics_cost × distance × qty` budget term, drawn from the Lua economy-constants registry. For space convoys, distance is Euclidean body-centre to body-centre. The cost is the term that makes distant arbitrage marginal and grounds the rule that *logistics affects margin*.

**Dispatch trigger.** The **auto path is the default**: the system fills a destination shortfall from the cheapest reachable source without player intervention. Player-direction of individual convoys is reserved for sell-order / buy-match counterparties on another body. **Exception:** space launches — leaving the gravity well — are **always player-directed** and never auto-dispatched, in Era 0 and Era 1 alike. Terrestrial (land / sea / air) convoys auto-dispatch.

**Infrastructure gates per mode.**
- **Land** — ungated; always available across contiguous land on a body. Roads are a per-tile cost-reducer, landed as the three-tier Track/Road/Highway ladder (BL-146–149, road network + hubs; generated lattice in `src/world/road_generation.cpp` plus player placement; `tile_component.road_level` discounts A* traversal).
- **Sea** — gated on a **Port** building at both endpoints.
- **Air** — gated on an **Airfield** building at both endpoints; the Airfield is designed but not in the prototype building set (air mode is deferred).
- **Space** — gated on a **Launchpad** at the origin and an **Orbital Port** at the destination; requires **Era 1** (per `docs/economy/ERAS.md`).

> **Design-reference note — the target logistics feel (2026-06-15).** The long-term direction for the **land / sea / air** logistic-strength model is an **emanation / cross-section "fuel" model**: supply radiates from sources and **attenuates across distance and terrain** (a continuous supply *field*, contested along its path), and the same logistics carry **goods, unit supply, and population supply** — not just discrete point-to-point goods convoys. **Space is a separate, larger consideration** (the convoy/launch model stands for it). The reference for the desired feel — despite the genre, theme, and tonal difference — is **Shadow Empire**'s logistics. This is a durable design-direction note, not a backlog item; the prototype keeps the simple per-mode-cost convoy model and grows toward this.

### Infrastructure
The physical substrate that makes all other systems possible: surface installations, orbital facilities, ports, and relay stations. Infrastructure determines the efficiency and capacity of every system in a given location. Damage has cascading consequences distinct from battlefield losses, and construction cost is shaped by local environment.

> **Infrastructure gates settled (2026-06-15, [B4]).** Each convoy mode's gate is now settled: land is **ungated** (road is a cost-reducer tile attribute — landed since as the three-tier ladder, BL-146–149, road network + hubs; 2026-07-31); sea is gated on a **Port** at both endpoints; air on an **Airfield** (designed, deferred); space on a **Launchpad** at the origin and an **Orbital Port** at the destination (Era 1 required). Per-mode `base_logistics_cost` multipliers live in `scripts/economy.lua` (land < sea < air < space). Capacity (per-node throughput cap) is deferred. Full detail in `docs/economy/SUPPLY.md`.

### Workforce
The labour layer that operates extraction, production, and military assets. The player allocates workforce across competing priorities; shortages create bottlenecks throughout the productive tier. Military units draw from the same pool, creating tension between economic output and force size.

---

## Discovery and modifier tier
*The systems that reveal what is available and set the local cost profile of action.*

### Exploration
Exploration reveals bodies, their resource profiles, and their suitability for settlement or extraction. It is a prerequisite for expansion and an early-game decision: where to look first, and how much to invest in surveying versus exploiting what is already known.

**Survey system (implemented).** A body starts *unsurveyed* — the player sees only its type, orbital position, and grid size, never its tiles or deposits. Dispatching a survey debits credits from the player corporation **upfront**, then plays out over sim ticks (one tick = one day): a **transit** phase (probe en route, nothing revealed) followed by a **scan** phase that reveals the surface **one region at a time** in deterministic raster order until the whole body and its deposit *richness bands* (rich / moderate / sparse) are known. Exact deposit amounts still wait for first extraction-site placement. Both **cost and duration scale with body size (tile count) and distance** (heliocentric orbital radius from home; a moon uses its parent's radius) — a near small asteroid is cheap and fast, a far large planet expensive and slow. There is **no concurrency cap**: many bodies may be surveyed at once, with credit cost the only throttle. The reveal is RNG-free (a pure function of grid dimensions), so it is deterministic. The home planet (and the star) start surveyed. Surfaced on the Solar canvas (a survey badge per body), the Planetary canvas (unrevealed regions masked), and the Selection panel (a Survey section with the cost/ETA preview and live progress). Logic lives in `src/world/survey_system.{hpp,cpp}`; the `survey_state` is carried inline on `body_component`.

### Environment
Each body, and each subdivision of land within it, has a procedurally generated profile of resources, terrain, hazard, and habitability. Environment sets the local cost of construction, the difficulty of military operations, and extraction yields.

### Generation (Planetology & the chain)
The world is generated, not authored — a deterministic, seeded chain (added 2026-07-31): planetology (atmosphere, chemistry, evolution history — `src/world/planetology.cpp`, BL-167 planetology) → continents → tiles → population centres → history ladder → nations → roads → markets → corporations. Each stage is a consequence of the one upstream, entered through the New World wizard. Authority: `docs/generation/GENERATION_STRATEGY.md` and `docs/generation/PLANETOLOGY.md`.

### History ladder
The institutional history that makes the campaign premise causal rather than asserted (added 2026-07-31). The pre-national ladder (BL-221, `src/world/history_ladder.cpp`) runs upstream of nation generation and *drives* it — counting agrarian cradles, pricing conquest against exit, and setting the nation seed budget (`nation_params_from_ladder`). Later stages (BL-222 industrial ladder, BL-223 averted rupture) are open. Authority: `docs/lore/HISTORY.md`.

### Force
Units, doctrine, and the ceiling on what the player can raise and sustain. *Designed, not built at campaign scale.* The resolver ships (BL-272 — see § Conflict) and province manpower ships (BL-273: `manpower_ceiling`, `replenish_manpower`, `raise_manpower`), both consumed by the Era −1 sim. BL-157 (military datamodel stub, v0.1.4) is the campaign-side rung. The first *campaign* lever to reach it is a technology, not a building: BL-344 gates the Military Base behind `E0-ML-01` (§ Research). *(Corrected 2026-08-10, NR-120: under the previous framing, "the levers that reach this system — conscription, requisition, war powers — are laws, which is precisely why they need a governing body to pass them." Under the rewrite the militia does not pass these laws — it is subject to them. Conscription and war powers are conditions the militia's recruitment and operations must satisfy or route around, not instruments it wields; see BL-315's 2026-08-10 update.)*

### Research
Technology is organised into discrete, modular trees, each unlocked by a visible precondition. Research raises capability ceilings across all systems and competes directly with other budget priorities. *(Corrected 2026-08-10, NR-120: was "owned at the governing-body tier" — a militia does not run laboratories. It reaches weapons, logistics and intelligence by buying their output through BL-350's procurement seam, the same as any other equipment; whether the militia itself holds any research capability is Sprint 8's open question, § v0.3.0, CONCEPT.md.)*

**The loop is closed (BL-344, landed 2026-08-10, v0.1.4).** Until then `tech_node::condition` was a descriptive *string* — a label that described what a gate would be about but could not resolve — so no tech had ever been earned and the F9 constellation viewer (BL-310) was a picture of a system rather than the system. The field is now a real `condition_set` (§ Conditions below), earned state is **per-corporation** (`world::earned_techs`), and `advance_tech_gates` runs once per economy tick: monotonic (a tech is never un-earned by a later dip below its threshold) and deterministic.

One gate is live, and it is a **military** one on purpose. `E0-ML-01` "Standing Garrison Doctrine" unlocks `building_type::military_base` (BL-325), gated on two extraction sites plus a Cr 2,000 balance — quantities reachable through ordinary play. That choice is BL-094's design test applied: *a technology that can only unlock a building is being designed for the corporate player the pivot is moving away from*. The predicate lives in `src/world/tech_gate.cpp` rather than in `scripts/tech_tree.lua`, because a gate that gates construction has to be linkable from the SDL/Lua-free world superset; the Lua file authors identity, topology and prose, and the viewer reads the same predicate the simulation enforces. A node with **no** authored gate reports "not yet earnable" rather than reading as unlocked — an empty `condition_set` is true by definition, so absence is modelled by absence from the gate table, never by an empty predicate.

Still open: research points and their economy (BL-332), the constellation grain and deeds (BL-087, v0.3.0), quest trees. Verified by `tools/verify/tech_gate_harness.cpp`.

### Policy
Two different things share this word, and BL-094 separates them.

**Automation policy** — standing rules governing automatic behaviour: trade thresholds, workforce allocation preferences, exchange policy. This is a convenience altitude; it changes a number in the player's own cost model.

**Law** — *(corrected 2026-08-10, NR-120: was "the governing body's instrument"; the militia is not the legislator, it is bound by law, which sharpens rather than weakens this sentence's own point)* the load-bearing sense of law in Io. Conscription, requisition, embargo, tariff, war powers: rules that bind actors other than the one who passed them, and that reach force rather than only price. Laws are enacted by nations; the player is a law **subject**, not a legislator, **and stays one after the pivot lands** — the single exception being a private militia's own **negotiated** tax or contract terms with its home nation (BL-280, itself likely re-read against the militia's new bargaining shape, BL-350).

*(Authority since 2026-08-22: [`docs/politics/NATIONS.md`](politics/NATIONS.md) owns the nation as an actor — the law object, the enforcement seam, the treasury, and the 2026-08-18 behaviour grant. What follows is the summary; that doc is the detail.)*

**One law is real (BL-343, landed 2026-08-10, v0.1.3).** A `law` (`src/world/law.{hpp,cpp}`) is an id, a `condition_set`, an effect and an `enacted` flag, held on `world::laws` in authored order. The shipped instance is BL-155's law #1, the **extraction levy**: a per-unit charge on raw output. *(Corrected 2026-08-19, BL-480: the levy now ships **enacted**, authored by the player's home nation, scoped to that nation's territory, and the debit is credited to the author's treasury — a conserved transfer. The Budget-ledger enact control is gone; enactment belongs to the nation actor, and the ledger's Laws section is browse-only.)* It surfaces as its own **Levies** line in the Finance card's flow chart, beside income, inputs, maintenance, wages and interest — because a law the player cannot see working is indistinguishable from an unimplemented one.

**The enforcement seam, settled:** *a law is a modifier OVER the market, never an override OF it.* This is the principle already established when price clamps were vetoed (2026-07-11) — a clamp fights price resolution rather than shifting a flow's cost. So the levy applies where the flow is **accounted** (`apply_budget`), not where the price is **resolved** (`clear_markets`): extraction output is priced by the market exactly as before, and the levy is a separate accounted cost. The market stays the only thing that sets prices, and the player sees the tax as its own number rather than as an unexplained worse price. Predicates are resolved once per law per corp per tick, before the money loop reads them, so ordering is fixed and the determinism invariant holds.

Honestly, an extraction levy reaches economic outcomes and not military ones. What the item owed instead is that **nothing in the record or the effect dispatch assumes an economic subject** — `law_effect_kind` is an open taxonomy a military effect joins without reshaping anything, and the predicate already carries military subjects. Still open: the other nine laws, the other three effect families, enactment politics (BL-186 laws ledger, BL-345 relationship axis), negotiated rates (BL-280). Verified by `tools/verify/law_harness.cpp`.

### Conditions

*(Authority since 2026-08-22: [`META_LAYER.md`](META_LAYER.md) owns the predicate and effect substrate in full — both vocabularies, their properties, and the wiring asymmetry between them. What follows is the overview.)*
The shared predicate laws, techs and quests all read (**BL-342**, landed 2026-08-10) — `src/world/condition_set.{hpp,cpp}`. BL-155 and BL-156 had independently settled on the same object, *"a flat AND-list of atomic conditions — no nested OR-mesh"*, and neither built it; one small pure evaluator turned two design-forward minors into shippable ones.

An atomic condition is `<subject> <comparator> <operand>` plus the qualifier its subject reads. Three properties are load-bearing:

1. **Pure and deterministic.** `evaluate` reads a `const world&` and nothing else — no RNG, no clock, no cached mutable state. It runs every tick for every enacted law, so it sits directly on the determinism invariant; where a measure sums over an unordered container it sums in ascending entity-id order.
2. **An empty set is true.** Most laws are unconditional once enacted, so the degenerate case is the *common* case and is the cheap path rather than a bolted-on special case. (The corollary matters: "no gate authored" cannot be represented by an empty set, and is carried by its own flag — see § Research.)
3. **A subject may be MILITARY.** BL-094's design test applied at the foundation *(2026-08-10: the specific test named at the time has since been retired and replaced — see BL-094 § 2026-08-10 — but this founding decision, that a subject enum must be able to name a military quantity at all, is exactly the part that does not depend on which version of the test was in force)*. The eight subjects are `tech_tree.hpp`'s original six labels promoted into resolvable quantities — `research`, `structure`, `stockpile`, `market`, `surplus`, `era` — plus `military_units` and `military_strength`. A subject enum that enumerated only economic quantities is exactly the failure the identity pivot is trying to avoid, and it is far cheaper to avoid now than to unpick.

Counts compare as integers (so `exactly 3` means three, not "within an epsilon of three"); quantities compare as floats. Verified by `tools/verify/condition_set_harness.cpp`.

---

## Relational tier
*The systems governing interactions with other actors.*

### Diplomacy
Each faction maintains a sentiment value toward every other, shaped by trade history, territorial conflict, and ideological alignment. Diplomacy modulates the likelihood and cost of conflict and affects what trade arrangements are available. Military takeover by another faction is one of the two primary loss vectors.

### AI opponent
Rival corporations act (added 2026-07-31): BL-079 (background-corp agency) gives narrow per-building reflexes, and BL-202 (corp AI stage A) layers a deterministic scored-utility evaluation over the corp-command seam (`src/world/corp_ai.cpp`). BL-203 (predictive spending) has landed on top of it.

Above that core sits the **language layer** (direction settled 2026-08-03): a small local model playing through the word interface below, socketed by the Io MCP server (BL-278, landed). Cloud models generate the fine-tuning corpus (BL-279); they never play, and the engine ships no network client. Authority: `docs/ai/AI_OPPONENT.md` § 10; the scoped limits live in `.claude/rules/io-standing-rules.md`.

### Comms
The channel-based chat log (added 2026-07-31) — the surface of the diplomacy-as-communication principle: since every rival is AI, inter-corp coordination happens in a visible medium, and the mechanical actions of background corps surface as messages first. `src/ui/chat_panel.{hpp,cpp}`; authority: `docs/ui/CHAT.md`.

---

## The progression chain

> **Each system's ceiling is the next system's door.**
> — the shape Ben named on 2026-08-22: *"we really want interconnectivity, so a player only
> progresses so far using one system before the next becomes a natural consequence."*

This is a **design test**, not a description of what is built. It applies to every system in this
document and to every new one: *what forces a player into it, what does it open, and what does it
cap them at?* A system that answers only the middle question is a feature. A system that answers all
three is a rung.

The intended chain, and where each rung's ceiling is today:

| Rung | You enter because | It opens | Its ceiling |
|---|---|---|---|
| **Extraction** | you start with a tile | output, and a balance | one tile's deposits run thin |
| **Logistics** | the good tile is far from the market | distant markets and deposits | **reach is binary** — the network says *can this be reached*, never *how much can move* |
| **Markets** | output is worth more elsewhere | price, arbitrage, scale | anonymous, instant, price-only — no lead time, no memory |
| **Contracts** | you need a counterparty who can refuse | equipment you cannot make; **income that is not extraction** | reputation |
| **Force** | a contract asks for an outcome, not a good | territory, interdiction, a third name | supply — and supply is the logistics rung again |
| **Territory / politics** | force without law is banditry | jurisdiction, treasury, lobbying | attention |

**Two things the table is meant to make visible.**

**The chain closes rather than ending.** Force's ceiling is supply, which is the logistics rung
again at a higher grain — which is exactly BL-325 ruling 3 (*economic reach IS military reach*)
read as progression rather than as architecture. That is why Logistic Points (BL-464) is a bigger
item than its size suggests: it is the rung the whole chain currently plateaus at.

**A staircase is a solved sequence.** A chain of systems each unlocked by the last is learned once
and then followed. [`EVENTS.md`](EVENTS.md) is the argument for something that cuts across it —
so the same rung feels different on a second campaign, which a ~100-hour target across many
campaigns needs.

---

## Cross-cutting notes

**Supply spans both pillars.** It is the connective tissue between economic production and military projection, and a target in both. It deserves particular attention during prototype scoping.

**Environment is load-bearing but silent.** Procedurally generated per tile, it sets the difficulty and character of every other system's operation locally, and directly drives replay variance.

**Budget is the primary pressure point.** All systems compete for it. Allocation decisions — research versus military versus infrastructure — define strategic identity and vulnerability to both loss conditions.

**The word interface is a system in its own right.** The game is drivable by text through three
legs plus a socket: **read** — the corp blackboard export (BL-206, `--export-blackboard`);
**meaning** — the action dictionary (BL-270, `docs/ai/ACTIONS.json`, every control as
`{press, args, preconditions, expected_output, reason_to_select}`); **write** — the corp-command
seam (`src/world/corp_command.hpp`, eight verbs, rejections returned as data); and the **socket**,
the MCP server (BL-278, `ProjectIo --serve` plus `tools/mcp/`). It is v0.1.1's named theme. Any
change to a control, lens, ledger or panel must update its dictionary entry — a stale entry
misleads an AI player the way a stale golden misleads a visual check.

**The broad economy is background firms'; the player and rivals are specialists.** The campaign
opens on a saturated, earth-like economy whose bulk industrial base is owned and run by **real
background corporations** (`corporation_component.is_background`, BL-365, 2026-08-11) — *not* by
a nation actor, and no longer by the abstract nation substrate that preceded them. The player and
the major AI rivals are **specialist corporations** occupying a focused slice of the chain,
differentiated by an interest in expanding to space — which is what keeps the loop a contest
between a few space-interested specialists rather than a full-economy management game. Detail and
the generation consequences live in `docs/generation/GENERATION_STRATEGY.md`.

This premise survives BL-094's rewrite (2026-08-10, NR-120): when the player becomes a national
private militia chartered by one of the generated nations, the bulk economy on that nation's
territory stays background-firm-run exactly as before — the militia is a new entity type attached
to a nation, not the nation itself, so nothing about the background-economy premise changes. *(Corrected
2026-08-10: was "when the player takes the governing-body seat... the player gains law, force and
research as levers" — the militia gains force directly and reaches law/research through
procurement, not by wielding them; see CONCEPT.md § Player identity.)* The player is not a
full-economy management burden either way.