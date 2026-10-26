# Project Io — Concept Summary

> **Settles:** who the player is and what it may not do · what the core loop asks of them ·
> how a campaign is shaped and paced · what the era arc is for · how the game should feel.
> **Not here:** how any system works (SYSTEMS) · what a word means (GLOSSARY) · which theme
> comes next (ROADMAP).
> **Confused with:** SYSTEMS.md, GLOSSARY.md, MANUAL.md.

## Player — identity
### Player identity
The player is a **major nation, and it builds its own buildings** (Ben, 2026-10-23, completing the sprint 53 identity session of 2026-10-10). It is the state: it sets its budget weights, its tariffs and its law (`docs/politics/NATIONS.md`). And it owns and builds **every building on its ground**, in every sector, paid from its treasury — there is no corporation or company between the nation and its industry (Ben, 2026-10-23: *"We can remove the corp/company layer entirely. Just have nations building their own buildings"*). The rival nations, majors and minors alike, are the same kind of actor, run by the scored-utility layer.

**The player's economy grows only by the player's builds** (Ben, 2026-10-23). Every building it starts with keeps running; nothing builds on the player's ground for it. A nation builds only on its **own ground** (`docs/GLOSSARY.md` § Own ground), so no rival builds there either.

**Only a major nation can be played** (Ben, 2026-10-10). A **major nation** is one generation raises to a full actor; a **minor nation** is every other nation (`docs/GLOSSARY.md` § Major nation). How many of each a world holds is a player-set target (`docs/generation/NATION_GENERATION.md` § The nation count is a target).

The seat is still an identity, not an address: picking a major on the selection screen (`docs/ui/STARTUP.md` § The seat) is becoming that nation. Its role, carried from NR-885 and now a nation's: *find potential locations to industrialise further, building the machinery that gets to space.* The Era 1 space programme is the nation's programme, and the nation builds it.

The **champion** the player operated beside the nation (2026-10-10) is retired by this ruling, with every corporation and company. So were the **corporation that holds a seat** (NR-885, 2026-09-17), the **mercenary company** (NR-177) and the **governing body** framing before them; do not design against any of them. The mercenary company was modelled on the **national private militia** of the parked space arc (BL-094, player-identity pivot); that arc's identity is not re-ruled here and stays parked with it. **Open direction, not settled** (Ben, 2026-09-17, carried): the player may be positioned as an individual leader, able to recruit talent and put people to use (`docs/PEOPLE.md`).

Every system answers one **design test** (Ben, 2026-10-10, re-subjected from the corporation's): **does this change what the NATION can FIELD, or what it must ANSWER TO?** The nation fields the units it raises and the industry it builds. It answers to two parties (Ben, 2026-10-23, the corporations it once answered to retired): **other nations** (stance, treaty, war) and **its population** (labour, demand, and what they will bear). The test is what keeps Conflict load-bearing on a player whose levers are both political and economic.

Ownership is the nation's (Ben, 2026-10-23). Every building is owned by the nation whose ground it stands on, and nothing is bought: the **buyout** retired with the firms it bought. Ground and what stands on it change hands only by conquest, and the buildings go with the ground (`docs/politics/NATIONS.md` § Own ground).

**The game does not play itself from the player's point of view.** The interface may inform, compare and rank; it never **acts** for the player. Nothing is built, bought, placed or queued on the player's nation by the interface, and no surface takes a decision the player has not pressed. **Ranking the tiles a player could build on by estimated profit is information, not an act** (Ben, 2026-10-25, overturning his 2026-08-29 call): *"It's not clear which places would be most profitable"* — so the Construction view's site ledger lists every tile that takes a chosen building, sorted by estimated profit, and the player still picks the row and presses Build (`docs/ui/ledgers/construction.md` § The site ledger). The 2026-08-29 line it replaces declined a build-opportunity ledger as *"the territory of telling the player what to do"* and drew the boundary between navigation and recommendation; the boundary that stands is between **showing** and **acting**.

**Showing versus acting is the only line (Ben, 2026-10-26).** The 2026-08-29 test that a surface may not rank where "the top of the list simply *is* the move" is retired with the navigation line it qualified. A surface may rank, sort and compare anything the player can read; what it may never do is press for them.

That is also why the site ledger's ranking passes the test. Its figure is an estimate at today's prices, at an assumed staffing and with the tile's Logistic Point cover; it says nothing about what the price will do once the building sells into its market, what its inputs will cost, or what a neighbour builds next, so the top row is one input among several and not the move. Market price is load-bearing information that a player must still weigh against reach, inputs, competition and what the price will do next — and the naive read of it is a trap, which is the surest sign the ranking is not the answer. Where a surface is unsure which case it is in, the question to ask is whether a player could follow the top row every time and play well.

This is the player-facing twin of the AI-behaviour prohibition in `.claude/rules/io-standing-rules.md`, and the two are one idea seen from either end. That rule stops the *simulation* acting on the player's nation; this one stops the *interface* deciding for them. Both leave the same thing intact — the player's own judgement about what to do next — which is why the one sanctioned auto-action on the player's nation is a single micromanagement dial with an opt-out, and not a planner — and why nothing builds on the player's ground for it (Ben, 2026-10-23).
### Territorial existence
The player persists as long as its nation holds any territory (Ben, 2026-10-10). A nation that holds no tile is eliminated; since it owns buildings only on its own ground, losing the last tile takes the last building with it. This replaces asset-based existence, under which owning a single building kept a firm alive.
### Development through ages
Early game involves buying land, building industrial chains, and raising forces before space becomes viable. The nation **raises its own units and buys their equipment on the market** (Ben, 2026-10-10): it musters, and its equipment comes off the shelf like any good — made by its own works or another nation's (Ben, 2026-10-23, the corporations that sold it retired). It buys that equipment **as a real buyer on the order book** (Ben, 2026-10-10, NR-1028), bidding and clearing like any other buyer (`docs/politics/NATIONS.md` § 2. Conservation). The Era 1 → Era 2 gate — ERAS.md's rocketry + launchpad + propellant set — is what the player is equipping toward from the opening tick.

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
Technology is organised into one tree per age, each age's branches unlocked by the previous age's quests. This keeps progression legible and goal-oriented without requiring a single monolithic research path. **The nation holds technology, and every nation starts the campaign at the same tech level (Ben, 2026-10-10)**; a corporation reaches it through its nation (`docs/economy/RESEARCH.md`). The player reaches weapons and logistics by buying the OUTPUT of research, through the same procurement seam as any other equipment (BL-350, procurement seam). Whether a corporation can fund research into its nation's pool is open (`docs/economy/RESEARCH.md` § What is open).
### Sentiment-based diplomacy
Each faction holds a sentiment value toward every other known faction, shaped by a small set of contributing factors such as trade history, territorial conflict, and ideological alignment. **Sentiment is the SUBSTRATE** (Ben, 2026-08-22, ruling on NR-520): the model is **two layers** — *sentiment* derived and continuous, one directed value from an observer to a subject, corporations and nations alike; *stance* declared and discrete on top. The invariant: sentiment informs a declaration and may never make one — no threshold flips a stance table by itself. `corp_reputation`, a nation's Access/Trust read of a corp, and the Era −1 grudges are all reads of this one quantity. BL-545 (sentiment substrate) owns it; the full model is `docs/politics/RELATIONS.md` § The settled model.

## Combat — conflict

Conflict is the pillar the design test keeps load-bearing. The player's **nation** commands force: it raises the units and buys their equipment (§ Development through ages); BL-315 (conflict spine) owns the loop that makes this pillar load-bearing. Two battle resolvers exist — `resolve_battle` (`src/world/combat.cpp`, nation scale, one scored evaluation, the Era −1 history sim's) and `resolve_campaign_battle` (`src/world/campaign_battle.cpp`, campaign scale, seeded rounds and priced withdrawal) — and `run_battles` opens an engagement whenever hostile units share a province. Authority: `docs/military/MILITARY.md`.

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
No hard lose screen in normal play. Losing ground, an empty treasury, or ground seized are impactful and progressive forms of defeat.
### Bankruptcy and seizure
Financial collapse and military takeover are the two primary loss vectors, both directly tied to the trade and combat systems. A nation never runs into debt (Ben, 2026-10-23): its financial collapse is an empty treasury, under which its buildings idle one by one (`docs/politics/NATIONS.md` § What a nation owns). Seized ground carries its buildings to the nation that takes it.

## Eras — the arc

Think of an Era like a gear shift. The engine is always running, but every so often the whole game changes what it's *about*.

The live product is the **industrial arc** (Ben, 2026-09-16, NR-807): the campaign epoch is **1960**, and generation runs 2400 BCE → 1960 to produce the world it opens on — an ancient span that settles the ground and its peoples, then an exploration span and an industrial one that leave nations, wealth and demand behind. Every nation opens on the industrial recipe band and at the same tech level, so every nation is playable (Ben, 2026-10-10; `docs/economy/ERAS.md` § Where the ladder starts); the history makes a start asymmetric through its geography, people and corporate web, never through its tech. The Era ladder below **begins at Era 1 — Terrestrial**, which is that epoch; the Eras after it are the **space arc**, the same engine further along, intended as downloadable content (ROADMAP § The two arcs). All of them share the core loop and the design test above.

The 0 CE ancient arc this paragraph named until 2026-09-16 is **superseded as the live product**, not parked: the ancient span survives as *generation* — the millennia the history sim runs before the campaign opens — rather than as the century the player plays in. **Who the player is was ruled on its own, not inferred from the arc:** a major nation operating its champion corporation (Ben, 2026-10-10, overturning NR-885; § Player identity).

Two rules hold across the whole ladder. **The core loop stays singular and consistent** — buy, sell, build, contest — so the player learns one game, not a hundred systems, even as the territory and stakes change. And **the science fiction stays grounded**: capped and nerfed wherever fantasy would break the game, validated by strict testing and dev review, and prototyped Era by Era. Era 1 is the MVP — the prototype proves itself there before any later Era is trusted.

**Era 1 — Terrestrial. Its pressure is Alarm.** A Cold War / Information Age footing rather than early industry — the **1960s start** (Ben, 2026-08-31) — on which you begin on a saturated, procedurally generated **earth-like homeworld**, competing for terrestrial resources and market position. The solar system is visible on your map from day one — asteroids, moons, the cold outer bodies — but unreachable. Era 1 is about building the machine that earns you the right to leave, and its exit is an **achievement**: reaching space (Ben, 2026-10-26: *"we want to encourage successes, rather than avoiding failure"*, retiring the Era catastrophe). The technologies that reach orbit are the same ones that frighten every other nation, so the space programme is itself the largest thing raising **Alarm** — and Alarm is how other nations treat you: embargoes, coalitions, wars. The skill is not dodging the dangerous technology but buying the reassurance that lets you hold it while you race for orbit. `docs/economy/ERAS.md` § The point of an Era owns the model.

**Era 2 — Early Space. Its pressure is climate** (`docs/CLIMATE.md`). "Early Space" names the territory, not the feel — and feel is the point. Plenty of works explore space; ours has to be singular in its gameplay and thematically rich, so this Era needs an identity that tells the player what it is *about*, not just where it happens. The strategic content is set: the launchpad fires, the solar system becomes contestable, and logistics becomes the game — fuel is heavy, distance is expensive, supply lines are targets, and corporations that make propellant off-world stop paying the homeworld's prices. The distinctive name and theme are open design questions.

**Era 3 — Dimensional.** Era 3 turns overtly science-fictional. Interstellar travel — another star, light-year trade routes — is **pushed back** out of the early ladder into the parked far-future (below); in its place Era 3 explores a more "dimensional" character. What that concretely means, in mechanics and fiction, is an open design question: a direction, not a finished theme.

**Era 4 — Megastructure, grounded.** Megastructures, satellites, and space stations are the toys of this Era — but as tools the player wields, not an ontological grandfather clock that plays itself. The reference is *Star Trek* scale and agency, not cosmic inevitability. To keep the game the game: bodies have surfaces; Dyson swarms are capped and nerfed; mega-mining happens only in asteroid belts, drawing a sliding set of resources from the available pool. Resource scarcity loosens, so the strategic question shifts from *what to extract* toward *what to build with it* — without ever leaving the buy/sell core behind.

**Beyond Era 4 — parked.** Interstellar reach, intergalactic expansion, alien contact — interesting, but ages you can't really *play*. They are parked as possible future / DLC material. The bar for anything past Era 4 is the same as everything before it: a gameplay direction faithful to the core loop that lets the player meet familiar sci-fi tropes on the game's own terms.

### How an Era ends — and how the next begins
There is no end-game screen and no Era test. A nation enters the next Era when it achieves the gate, and a nation that gets there late enters a territory its rivals already work, and it is *hard*. Each Era necessitates the one before — the techs, infrastructure, and capital you accumulated are the only way through the next gate — which extends "Stagnation as loss" above: falling behind is progressive, not a single failure state. A player who corners a market completely can effectively *win early* within an Era: once trade has nothing left to offer, the only move remaining is to conquer the opponents.

### Progress — a quest-based tech tree
Progression is the actionable spine of the Era system, and it has to stay actionable. It is a **quest-based, disjoint tree** rather than a single research path: the fiction supplies the theory (drawn from real sci-fi tech), the player has to actually *build* it, and building enough reaches the **tech goal** that gates the next Era. Each fictional advancement traces to specific techs and structures — that is the test of whether an Era is real or just flavour — and the whole thing runs through the same intuitive, already-tested UI the rest of the game uses. This makes "Modular tech trees" above concrete: discrete, precondition-gated trees whose payoff is the next Era. BL-087 (era-1 tech/quest system) owns the quest object.

### Opponents — the open problem
The hardest unsolved design question is *interesting opponents*. A ~100-hour grand campaign does not support multiplayer well, so the AI carries the competition, and getting it right matters more than any single mechanic. The unsolved part is difficulty — keeping them off both extremes of unbeatable and trivially beaten.

Neural networks are rejected as the opponent's core (2026-07-31): they sit badly with the determinism rule. The rival is a **deterministic scored-utility layer** over the corp-command seam (BL-202, corp AI stage A; BL-203, predictive spending — `src/world/corp_ai.hpp`). Design authority and working notes live in `docs/ai/AI_OPPONENT.md`.

Above the utility core sits a **small local model** driving the game through its word interface — read (blackboard export, BL-206), meaning (action dictionary, BL-270) and write (the corp-command seam), socketed by the **Io MCP server** (BL-278 — `ProjectIo --serve` plus `tools/mcp/`). Cloud models generate the fine-tuning corpus (BL-279); they do not play. The engine ships no network client. See `docs/ai/AI_OPPONENT.md` § 10.
