# Project Io — Concept Summary

## Player — identity
### Faction leader
The player controls a **corporation** — decided; the prototype is built on it. An open item, BL-094 (player-nation pivot, designed 2026-07-04), moves the player to the nation seat around the v0.2.0 era, with the corp staying the economic actor underneath; its design stays in the backlog until it lands.
### Asset-based existence
The faction persists as long as it holds any asset. Owning a single building keeps the corporation alive; the parent nation must also be destroyed for total elimination.
### Development through ages
Early game involves buying land, building industrial chains, and raising private forces before space becomes viable.

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
The map starts as a solar system but can expand to multiple-systems. The player-parameter generation is now concrete (BL-167, complete): the New World wizard walks player-set parameters through the staged Planetology chain, one decision per generation stage — see `docs/generation/PLANETOLOGY.md`.
### Modular tech trees
Technology is organised into discrete trees, each unlocked by meeting a known precondition. This keeps progression legible and goal-oriented without requiring a single monolithic research path.
### Sentiment-based diplomacy
Each faction maintains a sentiment value toward every other known faction, shaped by a small set of contributing factors such as trade history, territorial conflict, and ideological alignment

## Combat — conflict
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

**Era 0 — Terrestrial.** A Cold War / Information Age footing rather than early industry: you begin on a saturated, procedurally generated Earth, competing for terrestrial resources and market position. The solar system is visible on your map from day one — asteroids, moons, the cold outer bodies — but unreachable. Era 0 is about building the machine that earns you the right to leave. Its exit is not a quiet graduation; a "WW3"-scale global rupture reshapes the world enough that a rapid space expansion becomes plausible.

**Era 1 — Early Space *(identity owed)*.** "Early Space" names the territory, not the feel — and feel is the point. Plenty of works explore space; ours has to be singular in its gameplay and thematically rich, so this Era needs an identity that tells the player what it is *about*, not just where it happens. The strategic content is set: the launchpad fires, the solar system becomes contestable, and logistics becomes the game — fuel is heavy, distance is expensive, supply lines are targets, and corporations that make propellant off-world stop paying Earth's prices. The distinctive name and theme are still open.

**Era 2 — Dimensional *(direction, not a finished theme)*.** Era 2 turns overtly science-fictional. Interstellar travel — another star, light-year trade routes — is **pushed back** out of the early ladder into the parked far-future (below); in its place Era 2 explores a more "dimensional" character. What that concretely means, in mechanics and fiction, is unresolved and owed.

**Era 3 — Megastructure, grounded.** Megastructures, satellites, and space stations are the toys of this Era — but as tools the player wields, not an ontological grandfather clock that plays itself. The reference is *Star Trek* scale and agency, not cosmic inevitability. To keep the game the game: bodies have surfaces; Dyson swarms are capped and nerfed; mega-mining happens only in asteroid belts, drawing a sliding set of resources from the available pool. Resource scarcity loosens, so the strategic question shifts from *what to extract* toward *what to build with it* — without ever leaving the buy/sell core behind.

**Beyond Era 3 — parked.** Interstellar reach, intergalactic expansion, alien contact — interesting, but ages you can't really *play*. They are parked as possible future / DLC material. The bar for anything past Era 3 is the same as everything before it: a gameplay direction faithful to the core loop that lets the player meet familiar sci-fi tropes on the game's own terms.

### How an Era ends — and how the next begins
There is no end-game screen. You don't "lose" an Era; you enter the next one underprepared, and it is *hard*. Each Era necessitates the one before — the techs, infrastructure, and capital you accumulated are the only way through the next gate — which extends "Stagnation as loss" above: falling behind is progressive, not a single failure state. A player who corners a market completely can effectively *win early* within an Era: once trade has nothing left to offer, the only move remaining is to conquer the opponents.

### Progress — a quest-based tech tree
Progression is the actionable spine of the Era system, and it has to stay actionable. It is a **quest-based, disjoint tree** rather than a single research path: the fiction supplies the theory (drawn from real sci-fi tech), the player has to actually *build* it, and building enough reaches the **tech goal** that gates the next Era. Each fictional advancement traces to specific techs and structures — that is the test of whether an Era is real or just flavour — and the whole thing runs through the same intuitive, already-tested UI the rest of the game uses. This makes "Modular tech trees" above concrete: discrete, precondition-gated trees whose payoff is the next Era.

### Opponents — the open problem
The hardest unsolved design question is *interesting opponents*. A ~100-hour grand campaign does not support multiplayer well, so the AI carries the competition, and getting it right matters more than any single mechanic. The unsolved part is difficulty — keeping them off both extremes of unbeatable and trivially beaten.

*(Superseded 2026-07-31.)* The earlier direction here — small, lightweight neural networks — is rejected: NNs sit badly with the determinism rule, and the shipped direction is a **deterministic scored-utility layer** over the corp-command seam (BL-202, landed; see the `src/world/corp_ai.hpp` header). Stage B — BL-203 (predictive spending) — is queued for v0.2.0. Design authority and working notes live in `docs/ai/AI_OPPONENT.md`.
