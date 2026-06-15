# Project Io — Systems Overview

## Structure

Two pillars define the game's end goals: **Trade** and **Conflict**. Every other system creates the conditions, constraints, or capabilities that flow into one or both. The player is a corporate entity — an owner of assets and operations — and this shapes the economic layer throughout: profit, not policy, is the motive.

Systems are grouped into three supporting tiers below the pillars.

---

## Pillars

### Trade
> **⟳ Pending review (2026-06-15) — transient.** Added the standing sell-order sentence below
> to record the player-sell-order mechanic landed in the >C pass. Remove once reviewed.
> See TODO § Documentation [S-tier review].

The player's corporation sells goods from any location to any market. Markets are pooled exchanges — buyers and sellers anywhere can transact — but logistical cost determines whether a sale is profitable. Base prices are set by global rarity; local supply and demand shift them each Tick. The player exploits price differentials by controlling production locations and minimising the cost of getting goods to market. The player can also place **standing sell orders** — a per-(body, resource) quantity with a **floor price** — which are honoured at market clearing and take precedence over the anonymous auto-sell path, so the player governs how their controlled stock is released rather than dumping it at the resolved price. *(Choosing **counterparties** preferentially is deferred — it needs a matched order book the pooled clearing lacks; see TODO § Trade.)*

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

### Infrastructure
The physical substrate that makes all other systems possible: surface installations, orbital facilities, ports, and relay stations. Infrastructure determines the efficiency and capacity of every system in a given location. Damage has cascading consequences distinct from battlefield losses, and construction cost is shaped by local environment.

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