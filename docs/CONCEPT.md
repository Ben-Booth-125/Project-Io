# Project Io — Concept Summary

> **Settles:** who the player is and what it may not do · what the core loop asks of them ·
> how a campaign is shaped and paced · what the era arc is for · how the game should feel.
> **Not here:** how any system works (SYSTEMS) · what a word means (GLOSSARY) · which theme
> comes next (ROADMAP).
> **Confused with:** SYSTEMS.md, GLOSSARY.md, MANUAL.md.

## Player — identity
### Player identity
The player is a **corporation that holds a seat** (Ben, 2026-09-17, NR-885). It is an operating firm, the same kind of actor as its rivals, chosen at Begin from a shortlist over the generated landscape (`docs/ui/STARTUP.md` § The seat). The seat is an identity, not an address: picking one is becoming that firm. It is not the state — it does not legislate or tax — and it **procures** force rather than producing it.

Its role, in Ben's words at the ruling: *"Really the player's role is to find potential locations to industrialise further, building the machinery that gets to space."*

The **mercenary company** (Ben, 2026-08-12, NR-177) is retired by that ruling, and so is the **governing body** framing before it; do not design against either. The mercenary company was modelled on the **national private militia** of the parked space arc (BL-094, player-identity pivot); that arc's identity is not re-ruled here and stays parked with it. **Open direction, not settled** (Ben, same ruling): the player may be positioned as the individual leader of the corporation, able to recruit talent and put people to use (`docs/PEOPLE.md`).

Every system answers one **design test**, kept as written through the identity change (BL-094 § 2026-08-10; Ben, 2026-09-17): **does this change what the company can FIELD, or what it must ANSWER TO?** The company is the player's corporation. It fields the force it procures, and it answers to the nations whose law it operates inside. The test is what keeps Conflict load-bearing on a player whose own levers are economic.

Ownership is separate from identity. A **corporation** is the operating firm, it has exactly one owner, and there is no ownership tier above it (Ben, 2026-08-26). Ownership moves **whole** — a **buyout** takes the firm outright, holdings, pools, balance and filed returns together — never as a fractional stake. Which firms can be priced and bought at all is a generated fact: `docs/GLOSSARY.md` § Ownership class.

**The game does not play itself from the player's point of view** (Ben, 2026-08-29). Ben, declining a proposed build-opportunity ledger — a per-tile best-margin field that would have answered "where should I build next": *"This is because it enters the territory of telling the player what to do."* The line is between **navigation** and **recommendation**: listing the buildings a player already owns so they can reach one is navigation; ordering the tiles they could build on by expected return is a recommendation.

**The test is not "does it rank", it is "is the ranking the answer"** (Ben, same day, qualifying the above for the Market ledger: *"Market prices is a vital pillar of gameplay, but the strategy 'just build the most profitable' is a red herring"*). A surface may rank, sort and compare wherever the top of the list is **one input among several** rather than the move. It may not where the top of the list simply *is* the move, because then the surface has made the decision and the player is only confirming it.

That is why the two calls differ. Best-margin-per-tile answers "where do I build" outright, and a player who follows it has stopped choosing. Market price is load-bearing information that a player must still weigh against reach, inputs, competition and what the price will do next — and the naive read of it is a trap, which is the surest sign the ranking is not the answer. Where a surface is unsure which case it is in, the question to ask is whether a player could follow the top row every time and play well.

This is the player-facing twin of the AI-behaviour prohibition in `.claude/rules/io-standing-rules.md`, and the two are one idea seen from either end. That rule stops the *simulation* acting on the player's corp; this one stops the *interface* deciding for them. Both leave the same thing intact — the player's own judgement about what to do next — which is why the one sanctioned auto-action on the player's corp is a single micromanagement dial with an opt-out, and not a planner.
### Asset-based existence
The player persists as long as they hold any asset. Owning a single building keeps the company alive; the parent nation must also be destroyed for total elimination.
### Development through ages
Early game involves buying land, building industrial chains, and raising forces before space becomes viable. The player is the private force throughout, procuring rather than internally building its own materiel. The Era 1 → Era 2 gate — ERAS.md's rocketry + launchpad + propellant set — is what the player is equipping toward from the opening tick.

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
The map starts as a solar system but can expand to multiple-systems. The New World wizard walks player-set parameters through a staged chain — planetology, continents and plates, tiles, population centres, the institutional history ladder, nations, roads, corporations. Each stage is a deterministic consequence of the one above it. `docs/generation/GENERATION_STRATEGY.md` maps the chain; `PLANETOLOGY.md` owns its head.
### Modular tech trees
Technology is organised into discrete trees, each unlocked by meeting a known precondition. This keeps progression legible and goal-oriented without requiring a single monolithic research path. The player reaches weapons and logistics by buying the OUTPUT of research, through the same procurement seam as any other equipment (BL-350, procurement seam). Whether research capability sits with the player's corporation itself, rather than entirely with its suppliers and its home nation, is an open design question. The reason it was once denied — a mercenary company runs no laboratories — retired with that identity (NR-885).
### Sentiment-based diplomacy
Each faction holds a sentiment value toward every other known faction, shaped by a small set of contributing factors such as trade history, territorial conflict, and ideological alignment. **Sentiment is the SUBSTRATE** (Ben, 2026-08-22, ruling on NR-520): the model is **two layers** — *sentiment* derived and continuous, one directed value from an observer to a subject, corporations and nations alike; *stance* declared and discrete on top. The invariant: sentiment informs a declaration and may never make one — no threshold flips a stance table by itself. `corp_reputation`, a nation's Access/Trust read of a corp, and the Era −1 grudges are all reads of this one quantity. BL-545 (sentiment substrate) owns it; the full model is `docs/politics/RELATIONS.md` § The settled model.

## Combat — conflict

Conflict is the pillar the design test keeps load-bearing. The player's **corporation** commands force, having procured it; BL-315 (conflict spine) owns the loop that makes this pillar load-bearing. Two battle resolvers exist — `resolve_battle` (`src/world/combat.cpp`, nation scale, one scored evaluation, the Era −1 history sim's) and `resolve_campaign_battle` (`src/world/campaign_battle.cpp`, campaign scale, seeded rounds and priced withdrawal) — and `run_battles` opens an engagement whenever hostile units share a province. Authority: `docs/military/MILITARY.md`.

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

The live product is the **industrial arc** (Ben, 2026-09-16, NR-807): the campaign epoch is **1960**, and generation runs 2400 BCE → 1960 to produce the world it opens on — an ancient span that settles the ground and its peoples, then an exploration span and an industrial one that leave nations, wealth and demand behind. The campaign's recipe band is derived from that history's industry state at the 1960 fold, never from the epoch year (Ben, 2026-09-24; `docs/economy/ERAS.md` § Where the ladder starts), so the arc the generator runs and the arc the player meets are the same arc. The Era ladder below **begins at Era 1 — Terrestrial**, which is that epoch; the Eras after it are the **space arc**, the same engine further along, intended as downloadable content (ROADMAP § The two arcs). All of them share the core loop and the design test above.

The 0 CE ancient arc this paragraph named until 2026-09-16 is **superseded as the live product**, not parked: the ancient span survives as *generation* — the millennia the history sim runs before the campaign opens — rather than as the century the player plays in. **Who the player is was ruled on its own, not inferred from the arc:** a corporation that holds a seat (Ben, 2026-09-17, NR-885; § Player identity).

Two rules hold across the whole ladder. **The core loop stays singular and consistent** — buy, sell, build, contest — so the player learns one game, not a hundred systems, even as the territory and stakes change. And **the science fiction stays grounded**: capped and nerfed wherever fantasy would break the game, validated by strict testing and dev review, and prototyped Era by Era. Era 1 is the MVP — the prototype proves itself there before any later Era is trusted.

**Era 1 — Terrestrial. Its catastrophe is nuclear war.** A Cold War / Information Age footing rather than early industry — the **1960s start** (Ben, 2026-08-31) — on which you begin on a saturated, procedurally generated **earth-like homeworld**, competing for terrestrial resources and market position. The solar system is visible on your map from day one — asteroids, moons, the cold outer bodies — but unreachable. Era 1 is about building the machine that earns you the right to leave. Its exit is not a quiet graduation: it is a **test**, and its failure branch is a **global-rupture-scale war — a nuclear one** (Ben, 2026-08-31). Each Era poses one catastrophe for the player to avoid, and this Era's is that war. The technologies that reach orbit are the same ones that frighten every other nation, so the space programme is itself the largest thing raising the temperature — and the skill is not dodging the dangerous technology but buying the reassurance that lets you hold it. Pass and the next Era opens; fail and the rupture lands on exactly the assets the space programme needed, so you enter the next Era crippled rather than reset. `docs/economy/ERAS.md` § The point of an Era owns the model.

**Era 2 — Early Space. Its catastrophe is climate collapse** (`docs/CLIMATE.md`). "Early Space" names the territory, not the feel — and feel is the point. Plenty of works explore space; ours has to be singular in its gameplay and thematically rich, so this Era needs an identity that tells the player what it is *about*, not just where it happens. The strategic content is set: the launchpad fires, the solar system becomes contestable, and logistics becomes the game — fuel is heavy, distance is expensive, supply lines are targets, and corporations that make propellant off-world stop paying the homeworld's prices. The distinctive name and theme are open design questions.

**Era 3 — Dimensional.** Era 3 turns overtly science-fictional. Interstellar travel — another star, light-year trade routes — is **pushed back** out of the early ladder into the parked far-future (below); in its place Era 3 explores a more "dimensional" character. What that concretely means, in mechanics and fiction, is an open design question: a direction, not a finished theme.

**Era 4 — Megastructure, grounded.** Megastructures, satellites, and space stations are the toys of this Era — but as tools the player wields, not an ontological grandfather clock that plays itself. The reference is *Star Trek* scale and agency, not cosmic inevitability. To keep the game the game: bodies have surfaces; Dyson swarms are capped and nerfed; mega-mining happens only in asteroid belts, drawing a sliding set of resources from the available pool. Resource scarcity loosens, so the strategic question shifts from *what to extract* toward *what to build with it* — without ever leaving the buy/sell core behind.

**Beyond Era 4 — parked.** Interstellar reach, intergalactic expansion, alien contact — interesting, but ages you can't really *play*. They are parked as possible future / DLC material. The bar for anything past Era 4 is the same as everything before it: a gameplay direction faithful to the core loop that lets the player meet familiar sci-fi tropes on the game's own terms.

### How an Era ends — and how the next begins
There is no end-game screen. You don't "lose" an Era; you enter the next one underprepared, and it is *hard*. Each Era necessitates the one before — the techs, infrastructure, and capital you accumulated are the only way through the next gate — which extends "Stagnation as loss" above: falling behind is progressive, not a single failure state. A player who corners a market completely can effectively *win early* within an Era: once trade has nothing left to offer, the only move remaining is to conquer the opponents.

### Progress — a quest-based tech tree
Progression is the actionable spine of the Era system, and it has to stay actionable. It is a **quest-based, disjoint tree** rather than a single research path: the fiction supplies the theory (drawn from real sci-fi tech), the player has to actually *build* it, and building enough reaches the **tech goal** that gates the next Era. Each fictional advancement traces to specific techs and structures — that is the test of whether an Era is real or just flavour — and the whole thing runs through the same intuitive, already-tested UI the rest of the game uses. This makes "Modular tech trees" above concrete: discrete, precondition-gated trees whose payoff is the next Era. BL-087 (era-1 tech/quest system) owns the quest object.

### Opponents — the open problem
The hardest unsolved design question is *interesting opponents*. A ~100-hour grand campaign does not support multiplayer well, so the AI carries the competition, and getting it right matters more than any single mechanic. The unsolved part is difficulty — keeping them off both extremes of unbeatable and trivially beaten.

Neural networks are rejected as the opponent's core (2026-07-31): they sit badly with the determinism rule. The rival is a **deterministic scored-utility layer** over the corp-command seam (BL-202, corp AI stage A; BL-203, predictive spending — `src/world/corp_ai.hpp`). Design authority and working notes live in `docs/ai/AI_OPPONENT.md`.

Above the utility core sits a **small local model** driving the game through its word interface — read (blackboard export, BL-206), meaning (action dictionary, BL-270) and write (the corp-command seam), socketed by the **Io MCP server** (BL-278 — `ProjectIo --serve` plus `tools/mcp/`). Cloud models generate the fine-tuning corpus (BL-279); they do not play. The engine ships no network client. See `docs/ai/AI_OPPONENT.md` § 10.
