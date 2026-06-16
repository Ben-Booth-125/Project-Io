# Project Io — Systems Overview

## Structure

Two pillars define the game's end goals: **Trade** and **Conflict**. Every other system creates the conditions, constraints, or capabilities that flow into one or both. The player is a corporate entity — an owner of assets and operations — and this shapes the economic layer throughout: profit, not policy, is the motive.

Systems are grouped into three supporting tiers below the pillars.

---

## Pillars

### Trade

The player's corporation sells goods from any location to any market. Markets are pooled exchanges — buyers and sellers anywhere can transact — but logistical cost determines whether a sale is profitable. Base prices are set by global rarity; local supply and demand shift them each Tick. The player exploits price differentials by controlling production locations and minimising the cost of getting goods to market. The player can also place **standing sell orders** — a per-(body, resource) quantity with a **floor price** — which are honoured at market clearing and take precedence over the anonymous auto-sell path, so the player governs how their controlled stock is released rather than dumping it at the resolved price.

Choosing **counterparties** preferentially (whether a buyer can pick or bias which seller it matches) is an open Brief under active design — see OPENS § Trade.

**Intra-body market structure (multiple tile-centred markets).** A market is **centred on a tile** (usually a body's principal/capital tile), and a body may carry **several** markets. A tile clears against the market whose centre is nearest by grid distance — the market's *catchment* (`market_for_tile`, `src/world/market_clearing.cpp`); clearing routes each corp's body-aggregate supply/demand to the market nearest its representative holding. A body with a single market routes there unconditionally, so the present world — which seeds **one market per body** — behaves as before. Seeding genuinely multiple centres (from capitals / population centres) is owed and follows the population layer — see OPENS § Trade.

**Inter-body markets (structural design — open, Selene worked example).** Each body resolves its market **in isolation**; there is no cross-body price linkage today, and that divergence is the *point*. The settled *structure* for when a second body matters — **Selene** (Kepler's moon) as the worked example: Kepler and Selene each keep their **own** locally-resolved market, and are coupled **only through Supply convoys** (Layer 5): a convoy carrying a good from Kepler to Selene adds it to Selene's supply on arrival and removes it from Kepler's at dispatch, so prices converge or diverge purely as a function of what logistics physically carry, net of logistical cost. There is **no abstract price-coupling term** — the convoy *is* the coupling, and a profitable arbitrage is simply *source price + per-unit logistical cost < destination price*. The convoy mechanics are the [S5] Supply Brief; this records the market-side framework. See OPENS § Trade / § Supply.

### Conflict
The player claims, defends, and invades territory through military force. Combat runs concurrently with the economy: supply routes are live targets, and active conflict on a body inhibits its trade connections. Territorial control is both a strategic objective and a source of ongoing economic pressure on opponents.

---

## Productive tier
*The systems the player actively builds and maintains to keep both pillars running.*

### Budget
The corporation's operating balance. Revenue comes from trade margins and asset returns; outgoings cover construction, maintenance, research, workforce contracts, and military operations. Budget is the direct converter between economic output and military capability, and financial collapse is one of the two primary loss vectors.

### Resources
Raw materials are extracted from bodies and processed into higher-tier goods through industrial chains. Output feeds both trade networks and military supply. Starting resource conditions differ between factions, making early industrial priorities a meaningful strategic choice.

### Supply
The physical movement of goods, materiel, and forces through space. Supply is cheap on Earth but becomes costly and vulnerable with distance and conflict. A disrupted route halts production, construction, and operations at the destination. The player cannot project power or sustain a colony beyond what their logistics can support.

Layer 5 of the economy is the **convoy layer** — the mechanism that couples bodies through physical cargo movement. The settled prototype model (BL-039 / [S5]; full build is v0.0.7's theme) is documented in `docs/economy/SUPPLY.md` and summarised here.

**Convoy entity.** A convoy is a world component carrying `(source market, destination market, mode {land|sea|air|space}, cargo {resource, qty}, progress 0–1, speed)`. The coupling is market-to-market. A convoy is created when goods are dispatched toward a destination shortfall; it advances a fixed fraction of `progress` per Tick (linear; no orbital mechanics in the prototype); on arrival it credits the destination pool and is retired. Cargo leaves the source pool at **dispatch**, not arrival — goods in transit are committed.

**Logistical cost.** Each convoy incurs a `base_logistics_cost × distance × qty` budget term, drawn from the Lua economy-constants registry. For space convoys, distance is Euclidean body-centre to body-centre. The cost is the term that makes distant arbitrage marginal and grounds the rule that *logistics affects margin*.

**Dispatch trigger.** The **auto path is the default**: the system fills a destination shortfall from the cheapest reachable source without player intervention. Player-direction of individual convoys is reserved for sell-order / buy-match counterparties on another body. **Exception:** space launches — leaving the gravity well — are **always player-directed** and never auto-dispatched, in Era 0 and Era 1 alike. Terrestrial (land / sea / air) convoys auto-dispatch.

**Infrastructure gates per mode.**
- **Land** — ungated; always available across contiguous land on a body. Roads are an optional cost-reducer (deferred to a follow-on pass).
- **Sea** — gated on a **Port** building at both endpoints.
- **Air** — gated on an **Airfield** building at both endpoints; the Airfield is designed but not in the prototype building set (air mode is deferred).
- **Space** — gated on a **Launchpad** at the origin and an **Orbital Port** at the destination; requires **Era 1** (per `docs/economy/ERAS.md`).

> **Design-reference note — the target logistics feel (2026-06-15).** The long-term direction for the **land / sea / air** logistic-strength model is an **emanation / cross-section "fuel" model**: supply radiates from sources and **attenuates across distance and terrain** (a continuous supply *field*, contested along its path), and the same logistics carry **goods, unit supply, and population supply** — not just discrete point-to-point goods convoys. **Space is a separate, larger consideration** (the convoy/launch model stands for it). The reference for the desired feel — despite the genre, theme, and tonal difference — is **Shadow Empire**'s logistics. This is a durable design-direction note, not a Brief; the prototype keeps the simple per-mode-cost convoy model and grows toward this.

### Infrastructure
The physical substrate that makes all other systems possible: surface installations, orbital facilities, ports, and relay stations. Infrastructure determines the efficiency and capacity of every system in a given location. Damage has cascading consequences distinct from battlefield losses, and construction cost is shaped by local environment.

> **Infrastructure gates settled (2026-06-15, [B4]).** Each convoy mode's gate is now settled: land is **ungated** (road is an optional cost-reducer tile attribute, deferred); sea is gated on a **Port** at both endpoints; air on an **Airfield** (designed, deferred); space on a **Launchpad** at the origin and an **Orbital Port** at the destination (Era 1 required). Per-mode `base_logistics_cost` multipliers live in `scripts/economy.lua` (land < sea < air < space). Capacity (per-node throughput cap) is deferred. Full detail in `docs/economy/SUPPLY.md`.

### Workforce
The labour layer that operates extraction, production, and military assets. The player allocates workforce across competing priorities; shortages create bottlenecks throughout the productive tier. Military units draw from the same pool, creating tension between economic output and force size.

---

## Discovery and modifier tier
*The systems that reveal what is available and set the local cost profile of action.*

### Exploration
Exploration reveals bodies, their resource profiles, and their suitability for settlement or extraction. It is a prerequisite for expansion and an early-game decision: where to look first, and how much to invest in surveying versus exploiting what is already known.

### Environment
Each body, and each subdivision of land within it, has a procedurally generated profile of resources, terrain, hazard, and habitability. Environment sets the local cost of construction, the difficulty of military operations, and extraction yields.

### Research
Technology is organised into discrete, modular trees, each unlocked by a visible precondition. Research raises capability ceilings across all systems and competes directly with other budget priorities.

### Policy
The player sets standing rules that govern automatic behaviour: trade thresholds, workforce allocation preferences, and military posture. Policy shapes the character of operations across a playthrough and is a lever for managing systemic pressure without direct intervention.

---

## Relational tier
*The system governing interactions with other actors.*

### Diplomacy
Each faction maintains a sentiment value toward every other, shaped by trade history, territorial conflict, and ideological alignment. Diplomacy modulates the likelihood and cost of conflict and affects what trade arrangements are available. Military takeover by another faction is one of the two primary loss vectors.

---

## Cross-cutting notes

**Supply spans both pillars.** It is the connective tissue between economic production and military projection, and a target in both. It deserves particular attention during prototype scoping.

**Environment is load-bearing but silent.** Procedurally generated per tile, it sets the difficulty and character of every other system's operation locally, and directly drives replay variance.

**Budget is the primary pressure point.** All systems compete for it. Allocation decisions — research versus military versus infrastructure — define strategic identity and vulnerability to both loss conditions.

**The broad economy is the nations'; corporations are specialists.** The campaign opens on a
saturated, earth-like economy whose bulk industrial base is owned and run by the **Nation AI**
as background. The player and the major AI rivals are **specialist corporations** occupying a
focused slice of the chain, differentiated by an interest in expanding to space — which is what
keeps the loop a contest between a few space-interested specialists rather than a full-economy
management game. Detail and the generation consequences live in
`docs/generation/GENERATION_STRATEGY.md`.