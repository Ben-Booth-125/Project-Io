# Project Io — Systems Overview

## Structure

Two pillars define the game's end goals: **Trade** and **Conflict**. Every other system creates the conditions, constraints, or capabilities that flow into one or both. The player is a corporate entity — an owner of assets and operations — and this shapes the economic layer throughout: profit, not policy, is the motive.

Systems are grouped into three supporting tiers below the pillars.

---

## Pillars

### Trade

The player's corporation sells goods from any location to any market. Markets are pooled exchanges — buyers and sellers anywhere can transact — but logistical cost determines whether a sale is profitable. Base prices are set by global rarity; local supply and demand shift them each Tick. The player exploits price differentials by controlling production locations and minimising the cost of getting goods to market. The player can also place **standing sell orders** — a per-(body, resource) quantity with a **floor price** — which are honoured at market clearing and take precedence over the anonymous auto-sell path, so the player governs how their controlled stock is released rather than dumping it at the resolved price.

Choosing **counterparties** preferentially (whether a buyer can pick or bias which seller it matches) is an open Brief under active design — see OPENS § Trade.

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

> **Open design note (2026-06-15).** A convoy carries a **mode** — land / sea / air / space — and each mode rides the **infrastructure** that supports it (see § Infrastructure note below). The space mode's distance is Euclidean, body-centre to body-centre. The convoy/supply model is settled at prototype depth in OPENS § Supply [S5]; the infrastructure that *gates* the modes is not yet designed.

### Infrastructure
The physical substrate that makes all other systems possible: surface installations, orbital facilities, ports, and relay stations. Infrastructure determines the efficiency and capacity of every system in a given location. Damage has cascading consequences distinct from battlefield losses, and construction cost is shaped by local environment.

> **Open design note (2026-06-15) — the logistics network is undesigned.** The convoy-mode model (§ Supply / OPENS [S5]) makes each mode depend on infrastructure, but *what that infrastructure is* has no design yet: what a **road** is (a tile attribute, a built network, or a per-edge capacity?), whether **sea routes** are implicit navigable water or built (ports + lanes), **airfields**, and the **launchpad / spaceport** that gates the space mode (already named by the ERAS.md Era-1 gate). Tracked as `[B4 ~]` in OPENS § Infrastructure.

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
<!-- ⟳ Pending review (2026-06-15) — transient. New cross-cutting note recording the
     saturated-base / specialist-corporation premise; detailed in GENERATION_STRATEGY.md.
     Remove once reviewed. See OPENS § Documentation [S1]. -->