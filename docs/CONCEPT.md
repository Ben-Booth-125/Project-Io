# Project Io — Concept Summary

## Player — identity
### Player identity
The player controls a **corporation** today, and the prototype is built on it. The stated aim is to play a **national private militia** — BL-094 (player-identity pivot, rewritten 2026-08-10 per NR-120), armed and national in allegiance but not the state. It does not legislate; it **procures**: it raises and fields force, buying its equipment from private companies that are counterparties, not its own economic arm.

> **Corrected 2026-08-10 (dated note, not a rewrite — authority time-slice, DELIVERY.md § Design state).** This section read "governing body" from 2026-08-04 to 2026-08-10. That framing is **superseded**, not extended — Ben's ruling was explicit that the militia replaces it rather than reading as an early form of it. Full propagation into this doc's own voice waits for BL-094 to land; this note exists so the doc does not actively mislead in the meantime.

The reason for the pivot still holds, restated for the militia (Ben's original point, 2026-08-03, re-read 2026-08-10): a corporation's levers are all economic, so law, policy and science stay flavour on an economy. The militia's levers are narrower than a governing body's would have been — it does not wield law or research — but they reach force directly: what it can **field**, bought from suppliers, pointed at Conflict. That is still Conflict's route to being load-bearing, by a shorter path than "wields law and science" implied.

It follows that every system in the v0.1.x stub band answers a **replaced** design test (BL-094 § 2026-08-10): **does this change what the militia can FIELD, or what it must ANSWER TO?** A system that only ever changes a cost or a price, touching neither procurement nor accountability, is being designed for the player we pivoted away from twice — first the corporation, now the governing body.

The pivot's rewrite carries version goal **v0.3.0** (Ben, 2026-08-04, unaffected by the 2026-08-10 refocus). Its design lives in `backlog.json` until the work lands.
### Asset-based existence
The player persists as long as they hold any asset. Owning a single building keeps the corporation alive; the parent nation must also be destroyed for total elimination.
### Development through ages
Early game involves buying land, building industrial chains, and raising forces before space becomes viable. *(Corrected 2026-08-10, NR-120: "private forces are the Era 0 corporate reading; a governing body raises standing ones" is superseded — the player IS the private force throughout, procuring rather than internally building its own materiel. The Era 0 → Era 1 gate, ERAS.md's rocketry + launchpad + propellant set, is what the militia is equipping toward from the opening tick, per the refocus's "space equipment" framing.)*

## Trade — economy
### Dynamic pricing
Base prices are set by global rarity; market prices respond to local supply and demand. Each market is independent.
### Dual profit paths
Expanding production and building trade networks are both viable strategies. Starting resource conditions meaningfully differentiate factions.
### Tick-gated economy
Supply/demand calculations resolve per Tick, keeping simulation cost bounded while allowing real-time play between updates.

## World — simulation
### Symmetrical rules
All non-player factions operate under the same ruleset as the player, or a deliberate, defined subset. No hidden AI exceptions.
### Solar to galactic scope
The map starts as a solar system but can expand to multiple-systems. The player-parameter generation is now concrete: the New World wizard walks player-set parameters through a staged chain — planetology, continents and plates, tiles, population centres, the institutional history ladder, nations, roads, corporations. Each stage is a deterministic consequence of the one above it. `docs/generation/GENERATION_STRATEGY.md` maps the chain; `PLANETOLOGY.md` owns its head.
### Modular tech trees
Technology is organised into discrete trees, each unlocked by meeting a known precondition. This keeps progression legible and goal-oriented without requiring a single monolithic research path. *(Corrected 2026-08-10, NR-120: research is no longer "owned at the governing-body tier" — a militia does not run laboratories. It reaches weapons and logistics by buying the OUTPUT of research, through the same procurement seam as any other equipment, BL-350. Whether any research capability sits with the militia itself, rather than entirely with its suppliers and its home nation, is Sprint 8's open question and not settled here.)*
### Sentiment-based diplomacy
Each faction maintains a sentiment value toward every other known faction, shaped by a small set of contributing factors such as trade history, territorial conflict, and ideological alignment. *Designed, not built* — no sentiment layer exists in code.

## Combat — conflict

Conflict is the least-designed of the three pillars. *(Corrected 2026-08-10, NR-120: was "the governing body is the actor that commands force" — now the **national private militia** commands it, having procured it. BL-315, the conflict spine, is Sprint 8's route to making this pillar load-bearing.)* A battle resolver ships (BL-272, `src/world/combat.cpp`), used by the Era −1 history sim; nothing in the campaign layer commands it yet.

### Concurrent with trade
Combat and the economy run simultaneously. Freighters and supply routes are live targets.
### Planetary isolation
Active conflict on a body cuts off all imports and exports to and from it, making supply line control a strategic priority.
### Unit-based logistics
Orbital logistics are volatile by design. Moving units and materiel through space is a primary source of risk and strategic depth.

## Pacing — campaign
### Grand campaign
Target of ~100 hours of real-world play. Depth comes from qualitative, consistent rewards rather than raw mechanical complexity.
### Stagnation as loss
No hard lose screen in normal play. Losing ground, going bankrupt, or having assets seized are impactful and progressive forms of defeat.
### Bankruptcy and seizure
Financial collapse and military takeover are the two primary loss vectors, both directly tied to the trade and combat systems.

## Eras — the arc

Think of an Era like a gear shift. The engine is always running, but every so often the whole game changes what it's *about*.

Two rules hold across the whole ladder. **The core loop stays singular and consistent** — buy, sell, build, contest — so the player learns one game, not a hundred systems, even as the territory and stakes change. And **the science fiction stays grounded**: capped and nerfed wherever fantasy would break the game, validated by strict testing and dev review, and prototyped Era by Era. Era 0 is the MVP — the prototype proves itself there before any later Era is trusted.

**Era 0 — Terrestrial.** A Cold War / Information Age footing rather than early industry: you begin on a saturated, procedurally generated **earth-like homeworld**, competing for terrestrial resources and market position. The solar system is visible on your map from day one — asteroids, moons, the cold outer bodies — but unreachable. Era 0 is about building the machine that earns you the right to leave. Its exit is not a quiet graduation; a **global-rupture-scale war** reshapes the world enough that a rapid space expansion becomes plausible.

**Era 1 — Early Space *(identity owed)*.** "Early Space" names the territory, not the feel — and feel is the point. Plenty of works explore space; ours has to be singular in its gameplay and thematically rich, so this Era needs an identity that tells the player what it is *about*, not just where it happens. The strategic content is set: the launchpad fires, the solar system becomes contestable, and logistics becomes the game — fuel is heavy, distance is expensive, supply lines are targets, and corporations that make propellant off-world stop paying the homeworld's prices. The distinctive name and theme are still open.

**Era 2 — Dimensional *(direction, not a finished theme)*.** Era 2 turns overtly science-fictional. Interstellar travel — another star, light-year trade routes — is **pushed back** out of the early ladder into the parked far-future (below); in its place Era 2 explores a more "dimensional" character. What that concretely means, in mechanics and fiction, is unresolved and owed.

**Era 3 — Megastructure, grounded.** Megastructures, satellites, and space stations are the toys of this Era — but as tools the player wields, not an ontological grandfather clock that plays itself. The reference is *Star Trek* scale and agency, not cosmic inevitability. To keep the game the game: bodies have surfaces; Dyson swarms are capped and nerfed; mega-mining happens only in asteroid belts, drawing a sliding set of resources from the available pool. Resource scarcity loosens, so the strategic question shifts from *what to extract* toward *what to build with it* — without ever leaving the buy/sell core behind.

**Beyond Era 3 — parked.** Interstellar reach, intergalactic expansion, alien contact — interesting, but ages you can't really *play*. They are parked as possible future / DLC material. The bar for anything past Era 3 is the same as everything before it: a gameplay direction faithful to the core loop that lets the player meet familiar sci-fi tropes on the game's own terms.

### How an Era ends — and how the next begins
There is no end-game screen. You don't "lose" an Era; you enter the next one underprepared, and it is *hard*. Each Era necessitates the one before — the techs, infrastructure, and capital you accumulated are the only way through the next gate — which extends "Stagnation as loss" above: falling behind is progressive, not a single failure state. A player who corners a market completely can effectively *win early* within an Era: once trade has nothing left to offer, the only move remaining is to conquer the opponents.

### Progress — a quest-based tech tree
Progression is the actionable spine of the Era system, and it has to stay actionable. It is a **quest-based, disjoint tree** rather than a single research path: the fiction supplies the theory (drawn from real sci-fi tech), the player has to actually *build* it, and building enough reaches the **tech goal** that gates the next Era. Each fictional advancement traces to specific techs and structures — that is the test of whether an Era is real or just flavour — and the whole thing runs through the same intuitive, already-tested UI the rest of the game uses. This makes "Modular tech trees" above concrete: discrete, precondition-gated trees whose payoff is the next Era.

### Opponents — the open problem
The hardest unsolved design question is *interesting opponents*. A ~100-hour grand campaign does not support multiplayer well, so the AI carries the competition, and getting it right matters more than any single mechanic. The unsolved part is difficulty — keeping them off both extremes of unbeatable and trivially beaten.

*(Superseded 2026-07-31.)* The earlier direction here — small, lightweight neural networks — is rejected: NNs sit badly with the determinism rule, and the shipped direction is a **deterministic scored-utility layer** over the corp-command seam (BL-202, landed; see the `src/world/corp_ai.hpp` header). BL-203 (predictive spending) has since landed too. Design authority and working notes live in `docs/ai/AI_OPPONENT.md`.

*(Extended 2026-08-03.)* The utility core stays, and gains a layer above it: a **small local model** driving the game through its word interface — read (blackboard export, BL-206), meaning (action dictionary, BL-270) and write (the corp-command seam), socketed by the **Io MCP server** (BL-278, landed: `ProjectIo --serve` plus `tools/mcp/`). Cloud models generate the fine-tuning corpus (BL-279); they do not play. The engine ships no network client. See `docs/ai/AI_OPPONENT.md` § 10.
