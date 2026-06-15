# Project Io — Glossary

**Asset**
Any owned entity with economic or military value: a building, installation, unit, or vehicle. A corporation persists as long as it holds at least one asset.

**Body**
Any discrete celestial object in the simulation — planet, moon, asteroid, or station. Bodies are the primary unit of territorial control and the locations where resources are extracted, colonies are built, and conflict occurs.

**Corporation**
The player's controlling entity. Unlike nation-states, a corporation begins with no territorial claim or military force and must justify every asset through economic or strategic return. The corporation persists as long as it holds any asset.

**Tick**
The fixed period between economy updates. Supply, demand, and market prices resolve at the end of each Tick. Real-time play continues within a Tick; the boundary is a hard economic checkpoint, not a pause.

**Faction**
A named actor with a distinct starting position, ideology, and resource profile. Factions include the player's corporation and all AI-controlled entities. Each faction maintains sentiment values toward every other known faction.

**Market**
A pooled exchange with a physical boundary through which any faction can buy or sell goods with the delivery cost depending on logistical cost. Price is set by rarity on a body, then modulated by local supply and demand. All tiles belong to a market.

**Sentiment**
A numeric value representing one faction's disposition toward another. Sentiment is shaped by trade history, territorial conflict, and ideological alignment, and governs diplomatic options and the likelihood of conflict.

**Tile**
The smallest subdivision of land on a body. Each tile has a fixed, procedurally generated profile covering resource deposits, terrain type, hazard level, and habitability. Tile properties determine local extraction yields, infrastructure construction costs, and combat conditions. Tiles are the granular unit of environment data, with many properties that are fixed upon generation.

**Building**
A surface installation placed on a tile. Buildings are either **extraction** (harvesting raw materials from tile deposits) or **processing** (consuming inputs and producing outputs via a recipe) or **infrastructure** (affecting logistical or economic capacity). Each building holds a `building_component` and a `stockpile_component`.

**Canvas**
A screen that the player navigates to inform decision making and understand what's happening on a body or in space.

**Active (state)**
The navigation **anchor** — the body or tile the canvas zoom ladder is currently framed around. Persists until the player navigates. Distinct from Selection: selecting an entity does not change what is Active. Backed by `ui_state.active_body` / `active_tile`. See `docs/ui/SELECTION.md`.

**Focus (state)**
The entity **under the pointer** in the current frame — the transient hover target that drives the tooltip / hover card. Distinct from both Active and Selection. See `docs/ui/SELECTION.md`.

**Selection (state)**
The entity the player **single-clicked to inspect**. Persists until another entity is selected (or the selection is cleared). Drives the **Selection info element** and its 'go to' target, and does not move the canvas. Backed by `ui_state.selected_entity`. See `docs/ui/SELECTION.md`.

**Brief**
A unit of **described intent** in the backlog (`docs/development/TODO.md`): a problem to solve, a feature to build, or a doc to write, captured with enough context and file pointers to plan later — but deliberately carrying *no* implementation breakdown. A Brief is the design-level view of a single piece of work; **promoting** it into `docs/development/TASKS.md` decomposes it into a **task group** (one Brief ↔ one group of tasks). Distinct from a **task**, which is one file-scoped, individually-buildable step within that group. See `docs/development/TODO.md` (§ TODO vs. TASKS).

**Complete (task state)**
A development task is **complete** only when every requirement it satisfies has been **reviewed**, **implemented**, and **tested** — completeness is measured against the requirements, not against "the code is written". A task that is implemented and builds but whose requirements have not all been reviewed and verification-run is *code-complete*, not complete. See `docs/development/TASKS.md` (§ Definition of "complete") and `docs/development/req/REQUIREMENTS.md`.

**Cancelled (task state)**
A task group that could not be driven to *complete* in one working block and is reverted rather than left half-tracked. Cancelling marks its requirements `failed`, rewrites its intent back into `TODO.md` (merging into a related Brief where possible), and removes the task stubs from `TASKS.md`. It reverts *tracking*, not committed code — landed code stays in the tree; its intent returns to the backlog. Distinct from *paused* (a clean, resumable handoff that keeps the group in `TASKS.md`). See `docs/development/TASKS.md` (§ Cancelling a task group).

**Paused (task state)**
A task group deliberately stopped at a session boundary before completion, as a scoping choice rather than a failure. Unlike *cancelled*, a paused group stays in `TASKS.md` for the next session to resume; it is legitimate only when the stop is **clean and resumable** — `TASKS.md` is true to state (the in-flight task marked as the resume point), the build is green or the breakage is noted, and a one-line handoff records where to resume. A paused group is an explicit, recorded intermission, not a terminal state. See `docs/development/TASKS.md` (§ Pausing a task group) and `CLAUDE.md` (§ Proportionality and session boundaries).

**Era**
A named phase in the game's industrial arc, defined by the accessible territory, available buildings, and dominant strategic challenge. The game begins in **Era 0** (Terrestrial) and transitions to **Era 1** (Early Space) by meeting an explicit gate condition. See `docs/economy/ERAS.md`.

**ISRU**
In-situ resource utilisation. The practice of producing resources — particularly propellant — from materials extracted at the operating location rather than shipped from the home planet. The primary logistical lever in Era 1.

**Ledger**
A view which provides a report on a sub-system to give detail for decision making.

**Recipe**
The configured input/output specification of a processing building. One building type may support multiple recipes; the active recipe is set per building. Recipe conversion rates are authored in Lua.

**Resource**
Any tradeable good in the economy. Resources occupy one of three tiers: raw materials (extracted from tile deposits), refined goods (produced by processing buildings), or products (manufactured from refined goods). See `docs/economy/RESOURCES.md`.

**Stockpile**
A per-entity store of resource quantities, held in a `stockpile_component`. Extraction and processing outputs accumulate in the building's stockpile each simulation step. At the economy tick boundary, all building stockpiles on a body aggregate into the body's market supply.