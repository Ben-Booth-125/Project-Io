# Project Io — Glossary

> **Settles:** which word Io uses for a thing · whether two terms name the same thing · what a
> term's canonical scope is and where it stops.
> **Not here:** how the thing behaves or what it is worth (its owning doc) · why it exists at all
> (CONCEPT, SYSTEMS).
> **Confused with:** CONCEPT.md, SYSTEMS.md.

**Asset**
Any owned entity with economic or military value: a building, installation, unit, or vehicle. Every asset is a **nation's** (Ben, 2026-10-23): there is no firm between a nation and what it owns.

**Body**
Any discrete celestial object in the simulation — planet, moon, asteroid, or station. Bodies are the primary unit of territorial control and the locations where resources are extracted, colonies are built, and conflict occurs.

**Survey**
The action that reveals a body's tile surface and deposit richness bands. A body starts **unsurveyed** (`survey_phase::hidden`) — only its type, orbital position, and grid size are known. Dispatching a survey debits credits upfront and reveals the body over sim ticks: a **transit** phase (probe en route, nothing revealed) then a **scan** phase that reveals the surface one **survey band** at a time in deterministic raster order until it is **surveyed**. Cost and duration scale with body size and distance from home. The home planet starts surveyed. This is the **geographic fog**; contrast **Activity fog**. See `docs/SYSTEMS.md` § Exploration.

**Activity fog (commercial sphere)**
The second, independent fog (BL-089, commercial-sphere visibility): a body-level tier — `unknown` / `known_stale` / `known` / `visible` — derived purely from the player's own trade routes, live convoys, and building presence (`body_activity_visibility`, `src/world/world.hpp`; no stored state). Independent of the survey axis: a surveyed-but-unrouted body is still activity-unknown, an unsurveyed-but-routed one is known. See `docs/ui/DISCOVERY.md`.

**Corporation**
**Retired** (Ben, 2026-10-23: *"We can remove the corp/company layer entirely. Just have nations building their own buildings"*). The operating firm that held buildings, ran recipes, set trades and kept a balance is gone. Every one of those jobs is the **Nation**'s, and there is no firm between a nation and its buildings. The player's identity this reached last — a major nation operating its **champion** (2026-10-10) — is now simply the nation (`CONCEPT.md` § Player identity). Do not design against this word; where a retired doc says "corporation", read "nation".

**Company**
**Retired** with **Corporation** (Ben, 2026-10-23). The background firm — the many small actors that made the economy populous — is gone. The buildings they ran are the nation's, on its own ground, and keep running. The **Company lens** and **Corporation lens** retire with the two words (`docs/ui/LENSES.md`).

**Ownership class**
**Retired** with **Corporation** (Ben, 2026-10-23). It said whether a firm's books were open and whether it could be bought; no firm remains to have books or be bought.

**Quarterly return**
The filed record a **nation** writes at the end of every economy tick: the money-loop flows, its closing treasury, its holding count and its book value (Ben, 2026-10-23, re-subjected from the retired corporation). A **retain** over the nation's own budget, not a second computation, so it can never disagree with the loop. Rolling **40 quarters** per nation, world state, serialised. See `docs/economy/FINANCE.md` § The quarterly return.

**Buyout**
**Retired** (Ben, 2026-10-23). It was acquiring a firm whole; with no firms, nothing is bought. Ground and what stands on it change hands only by **conquest** (`docs/politics/NATIONS.md` § Own ground).

**Mercenary company**
**Retired** (Ben, 2026-09-17, NR-885). It was the player's identity from 2026-08-12 (NR-177): armed professionals for hire, paid for outcomes under mercenary contracts, which went on 2026-08-30 (`docs/military/MILITARY.md`). The player is a **Major nation** operating its **Champion** (Ben, 2026-10-10); do not design against this word. See `docs/CONCEPT.md` § Player identity.

**Militia (national private militia)**
The player's seat in the space arc (BL-094, player-identity pivot; Ben, 2026-08-10, NR-120): an armed, national-in-allegiance actor that is **not** the state — it does not legislate, tax, or research. Its agency is **procurement and force**: it holds its own treasury (funded by its home nation's mandate, not shared with any corporation), buys equipment from private companies who can quote, delay, or refuse (BL-350), and fields what it buys. Chartered by one of the generated nations (`home_nation`) rather than itself one of the ~43 — nation generation is untouched by the identity. Supersedes the 2026-08-03–2026-08-10 **Governing body** framing (a sovereign actor that legislates, taxes, researches and commands force) outright, not as an early reading of it.

**Headquarters (HQ)**
**Retired** with **Corporation** (Ben, 2026-10-23). It was a corporation's seat building and the centre of its corporate border; a nation's seat is its **capital**, and its border is its territory.

**Country**
A generated nation; the territorial/political unit that holds tiles on a body. The Country map lens (formerly "Faction") is retired: a nation is read on the Planetary plain canvas from the **national border band**, an inset stroke in its identity colour (`nation_colour`) on its side of each boundary, with no tint inside the territory (UI: `docs/ui/PLANETARY.md` § The national border band). See **Nation** and **Faction**.

**Nation**
A generated sovereign state on the homeworld — the territorial/political actor that owns tiles, carves resource-carved markets (BL-096), and **owns and builds every building on its ground** (Ben, 2026-10-23). Generated by `src/world/nation_generation.cpp` (Voronoi growth from a seed budget the history ladder sets); the count is a **player-set target**, hit by a lean and in-world reconciliation (Ben, 2026-10-10; `docs/generation/NATION_GENERATION.md` § The nation count is a target). Every nation is either a **Major nation** or a **Minor nation**, and the player plays a major. "Country" is the same unit seen from the map lens. **A nation runs its whole economy itself** (Ben, 2026-10-23, overturning 2026-10-11's "a nation does not run the broad background economy"): there are no corporations and no companies, and a nation builds **only on its own ground** (`docs/politics/NATIONS.md` § Own ground). A nation also **acts**: it holds a treasury, allocates a national budget over priority lines and enacts law — see `docs/politics/NATIONS.md`. Generation: `docs/generation/NATION_GENERATION.md`.

**Major nation**
A full nation actor — treasury, budget, law, tariff, the nation scorer — and the only kind a player may play (Ben, 2026-10-10). Which nations are majors is generation's call: the count target picks them from the nations whose cities accumulated the most capital (`docs/generation/NATION_GENERATION.md` § The nation count is a target). Choosing a major on the selection screen is taking its seat (`docs/ui/STARTUP.md` § The seat). A world holds 1 to 6, a player-set target that is always hit. `docs/politics/NATIONS.md` § Major and minor nations owns what a major does.

**Minor nation**
Every nation that is not a **Major nation** (Ben, 2026-10-10). A minor runs **shallow** — a treasury, budget weights and tariffs on a slow cadence, and no law of its own beyond what generation leaves it — but it **builds its own economy** on its own ground like any nation (Ben, 2026-10-23). It is the backdrop that is traded with and fought over. It cannot be played. `docs/politics/NATIONS.md` § Major and minor nations.

**Champion**
**Retired** with **Corporation** (Ben, 2026-10-23). It was a major nation's one seat-eligible corporation, which the player operated beside governing the nation. The player now simply is the nation and builds as the nation.

**State industry**
**Retired as a separate class** (Ben, 2026-10-23). It named the strategic-sector buildings a major owned outright beside its private industry; every building is now the nation's outright, in every sector, so the split is gone. `docs/politics/NATIONS.md` § What a nation owns.

**Private industry**
**Retired** with **State industry** (Ben, 2026-10-23): there is no industry a nation does not own.

**Own ground**
The tiles a nation owns. **A nation builds only on its own ground** (Ben, 2026-10-23): never on another nation's, and never on ground no nation owns. A road is the one reach across: it may be laid one crossing *into* foreign ground so two networks can join, never *from* it. Ground that changes hands carries its buildings to its new nation. `docs/politics/NATIONS.md` § Own ground.

**History pattern**
One of five shapes written into every generated history on purpose (Ben, 2026-10-10): **the rise and fall**, **the late culture** and **the world war** are targets, always hit; **civil war** and **revolution** are leans. Each has a **detector** over the history record, and the polities it names carry a **pattern tag** into their nation, shown on the nation selection card. Fixed by design, not player-set. `docs/generation/GENERATION_STRATEGY.md` § History patterns.

**Metropole / colony**
In the exploration age (`docs/generation/EXPLORATION.md` § Two ways to claim ground across water), the **metropole** is the polity that claimed a province across water and the **colony** is the province claimed — by **purchase** (bought for trade, its culture shares kept) or by **conquest** (taken, its people digested). Both are polity verbs of the generation sim, never campaign-era actions. The colony grows the endemic good; the metropole wants it. What the pair leaves on the campaign map is a **sea lane** and, at setup, a taste, a treasury and a tariff — never a preferred-seller relationship in the order book (superseded, Ben, 2026-09-09).

**Sea lane**
A tier on water tiles that discounts the sea-leg traversal cost, stamped from the exploration age's sea-leg table — the crossings it actually made (`docs/generation/EXPLORATION.md` § The colonial tie is a sea lane, and the map reads it) — the water analogue of an ancient road (`docs/economy/LOGISTICS.md` § 4b). It is the **colonial tie** as a force on the map: read by convoys, reach and placement alike because traversal cost is one weight function. A standing **port** also lays lanes, to every other port within the sea's reach (`docs/economy/LOGISTICS.md` § 4b, "Every port lays lanes"); a lane **ends in its port**, and is drawn on into the port's tile (`docs/ui/RENDERING.md` § Roads and sea lanes). Not a shipping route in play, and not a road — land is untouched.

**Tick**
The fixed period between economy updates. Supply, demand, and market prices resolve at the end of each Tick. Real-time play continues within a Tick; the boundary is a hard economic checkpoint, not a pause.

**Tick is the INTERNAL term.** It names the simulation step, and it is the right word in code, comments, harnesses, data exports and design docs. It is **not** the word the player sees — see **Quarter**.

**Quarter (`qtr`)**
The player-facing name for one Tick. A Tick is ~3 months of campaign time, so four make a year (BL-095) — the two words denote exactly the same period, and "Quarter" is the more literally accurate of them.

Settled by Ben, 2026-08-01 (NEEDS_REVIEW NR-002): *"Qtr is the preferred term for any economy tick. Tick is too technical of a term for average gamers."* Every rate, duration and empty-state string a player reads uses Quarter or `qtr` — `Profit/qtr`, `payback ~5 qtrs`, `Est. qtrs left`, "run an economy quarter". Use the abbreviation `qtr` where space is tight (column headers, inline rates) and the full word in prose and tooltips.

The split is by audience, not by meaning: a CSV export column stays `tick` because its reader is an analyst, while the header beside it reads `/ qtr` because its reader is a player.

**Faction**
Legacy umbrella term — prefer the specific actor: **Nation** (political/territorial) or **Corporation** (economic). The map lens formerly named Faction is the **Country** lens (renamed by BL-052, faction → country — it always showed nations). "Faction" as a colour concept — player-vs-rival identity — is the **Corporation** identity colour (`corp_colour` / `corp_identity_colour`, `src/ui/presentation.hpp`). Per-faction sentiment is **Sentiment**.

**Market attraction**
What decides which market a tile clears against (Ben, 2026-10-16): the **logistics cost** from the tile to a market's centre divided by that centre's **pull**, lowest winning. **Pull** is a multiplier on a centre, 1 by default, raised by a technology its nation holds or a building standing on it. Every tile has an attraction to every market on its body, so every tile belongs to one — land with no road is priced through the **off-road leg** rather than excluded. Distinct from **reach**, which is a placement permission and does not spread off the road. See `docs/economy/MARKETS.md` and `docs/economy/LOGISTICS.md` § 3b.

**Market**
A pooled exchange with a physical boundary through which any faction can buy or sell goods with the delivery cost depending on logistical cost. Price is set by rarity on a body, then modulated by local supply and demand. All tiles belong to a market.

**Sentiment**
The relational **substrate** (BL-545, sentiment substrate; Ben, 2026-08-22, on NR-520): one directed, continuous, *derived* value from an observer to a subject, where an actor is a corporation or a nation — two dimensions, Access and Trust, at every grain. Shaped by trade history, territorial conflict, and ideological alignment, it decays, and it informs diplomatic options and the likelihood of conflict. **Stance** (hostility, friendship) is the declared, discrete layer above it, and the invariant is that sentiment informs a declaration and never makes one — no threshold flips a stance table by itself (`src/world/sentiment.{hpp,cpp}`). See `docs/politics/RELATIONS.md`.

**Tile**
The smallest subdivision of land on a body. Each tile has a fixed, procedurally generated profile covering resource deposits, terrain type, hazard level, and habitability. Tile properties determine local extraction yields, infrastructure construction costs, and combat conditions. Tiles are the granular unit of environment data, with many properties that are fixed upon generation.

**Region**
The unit of settlement and politics, one step above the tile. A region is founded by `run_settlement` at an **anchor** tile under a separation rule (`sep = max(3, grid_width/40)` — 7 tiles on the default 312×145 grid), and carries a culture, a founding culture, ancient endowment windows, works, demography and a generated name. The target is `land_tiles / 80` — roughly 159 on a default world, so a region's implied catchment is ~80 land tiles. Regions are what the Era −1 sim conquers and transfers: territory moves one region at a time. `src/world/settlement.hpp`. *(Renamed from "province" 2026-08-18, to free that word for the smaller locality cell.)*

**Province**
The **locality cell** — a small area grouping below the region and above the tile, and the game's spatial unit of consequence: where a battle happens, where a unit *is*, what a building ceiling counts against. `src/world/province.{hpp,cpp}` owns the partition and the tile→province mapping (BL-466, province partition; BL-511 the canvas grain; BL-515 the id scheme), and a plain click on the Planetary ground selects the province rather than the bare tile. Its pricing purpose is to price locality *before* the market does: goods sourced inside one province are cheaper to the buyer than goods hauled in from outside it — a discount that must earn its place against convoy haulage cost, which already prices distance (NR-342).

Size is a **growth budget, not a clamp**: 7 tiles soft, 12 preferred, 20 hard (asserted rather than imposed), giving a mean of about **8.6 tiles** on a default world. It is deliberately a *different object* from the region: a region is large, irregular, historically placed and politically owned, while a province is small, uniform and purely spatial. See `docs/generation/PROVINCES.md`, and BL-464 (Logistic Points) for the neighbouring throughput work.

**Quarter**
One of nine 3×3 map slots (north→south × west→east) used as a **naming device**: a region's name is its culture's word plus the quarter word for where its anchor falls (`quarter_word`, `src/world/settlement.cpp`; BL-348 supplies nine such words per tongue). A quarter is not a gameplay object and owns nothing — it exists so names carry geography. *(Renamed from "region" 2026-08-18.)*

**Deposit**
The per-**tile** quantity of a resource (`resource_deposit`, indexed by `resource_type`). A deposit is what an extraction building draws down, and it depletes. Distinguish from **ore field**, which is the seeded *cluster* of tiles whose deposits are elevated — a field contains deposits, never the reverse.

**The spatial vocabulary (settled 2026-08-18)**
Five nested or overlapping spatial objects, each with exactly one name, after a rename that found "province" and "region" carrying three meanings each. Smallest first: **tile** → **province** (locality, mean ~8.6 tiles) → **region** (settlement/politics, ~80 tiles) → **nation** (Voronoi from region anchors) → **body**. Cutting across them: **ore field** (a resource blob in tile generation), **quarter** (a naming slot), **survey band** (the unit of survey reveal), and **market catchment** (the tiles a market centre serves). If a new spatial object is added, name it here first — the collision that forced this rename existed because neither "province" nor "region" was ever defined in this file.

**Landform**
The **shape** axis of terrain. A tile's terrain is three axes (BL-519, terrain axes):
`terrain_substrate` (what the ground is made OF — 8 values) × `terrain_cover` (what sits ON it — 10
values, with a `cover_density` scalar making cover **graded** rather than binary) × `terrain_landform`
(its shape: plains, highland, mountain, canyon, valley, crater, rift — `src/world/components.hpp`). Landform drives build cost (×1.0 plains up to ×2.0 mountain) and a relief layer (BL-231, landform render; BL-232, spanning markers) on the Planetary canvas — contiguous runs bridged into one named spanning marker. See `docs/economy/TILES.md`.

**Substrate / Cover**
The two axes of terrain that, with landform, replace a single composition axis that did three unrelated jobs at
once (BL-519). **Substrate** is what the ground is made of and is what deposits, defence and build cost read.
**Cover** is what sits on it — grass, scrub, forest, marsh, snow, dunes, ash, salt, **urban** — carried
with a `cover_density` byte, so a thin wood and a dense one are the same cover at different densities
rather than two enum values. The invariant: density is `0` **iff** cover is `none`. `urban` is a COVER
value (Ben's call), which means paving a tile does not destroy the geology under it. There is no
`tundra` — it is scrub on cold ground, which the three axes say directly. See
`docs/economy/TILES.md` and `src/world/components.hpp`.

**Continent / Tectonic plate**
The Continents/Drift generation pass (`src/world/continents.cpp`) simulates a small number of drifting **tectonic plates** — count, drift, and speed all derived from Planetology's outputs, not rolled — whose boundaries bias the tile heightmap and append dated lines to the body's biography. The Continent lens (`overlay_mode::continent`, BL-226 continent lens) draws the plates the height bias was derived from, rather than inferring landmasses back out of the finished terrain.

**Planetology (and the generation chain)**
The body-level generation pass (BL-167, planetology; `src/world/planetology.cpp`): generated atmosphere, chemistry, and a simulated abiogenesis/evolution history, upstream of tile generation. "The chain" is the ordered, seeded, deterministic generation sequence: planetology → continents → tiles → population centres → history ladder → nations → roads → markets → corporations. Its front door is the **New World wizard** — the setup walk "New Game" opens; the world is not built until the wizard finishes (`start_new_game`, `src/core/app.cpp`). See `docs/generation/PLANETOLOGY.md` and `docs/generation/GENERATION_STRATEGY.md`.

**Ore field**
A seeded blob of tiles holding a disproportionate share of one resource, so an endowment lands *somewhere* rather than dusting evenly (tile generation Pass 6, `ore_fields_for` / `ore_field_map`). Copper, petroleum, iron and coal each redistribute roughly 45–65% of the world total into 2–3 fields. Conservation is over the resource's **bearing set**, not over all land. The mechanism is a deterministic post-multiply, not a roll. *(Renamed from "ore province" 2026-08-18 so "province" carries exactly one meaning. See § The spatial vocabulary.)*

**Word interface**
The text-drivable face of the game — the substrate an AI player uses, and v0.1.1's theme. Three legs plus a socket: **read**, the corp blackboard export (BL-206, `--export-blackboard`); **meaning**, the action dictionary (BL-270, `docs/ai/ACTIONS.json`); **write**, the corp-command seam (`src/world/corp_command.hpp`); and the **socket**, the Io MCP server (BL-278, `ProjectIo --serve` plus `tools/mcp/`). The write channel is narrower than the dictionary — some documented presses have no command verb. See `docs/ai/AI_OPPONENT.md` § 10.

**History ladder**
The institutional-history generation stage (`src/world/history_ladder.cpp`). The **pre-national ladder** (BL-221) runs after tiles and *before* nations, and drives them: it counts the agrarian cradles the land supported, prices conquest against exit, and sets the nation seed budget (`nation_params_from_ladder`) — the ladder is upstream of the map, not a narration of it. Its events carry historical-epoch timestamps (BL-220, dated epochs) and merge into the body's biography. The industrial stages are BL-222 (industrial ladder) and the averted rupture BL-223. See `docs/lore/HISTORY.md`.

**Qualification**
A nation-grain fraction: the share of its workforce that is **qualified** (Ben, 2026-08-25; BL-613, qualification fraction). Raised by schooling buildings and universities, consumed by buildings running complicated production methods, seeded from Era −1 industrialisation timing, carried by migrants (brain drain), and read by road generation. See `docs/economy/POPULATION.md` § Qualification.

**Stratum (population centre)**
The centre scale ladder — Outpost / Settlement / Town / City / Metropolis (scale 1–5) — read as a building-placement gate: some buildings are placeable only in a centre, some only at a minimum stratum (a university needs City+). Not a new axis; the same `population_centre_component.scale`. See `docs/economy/POPULATION.md` § Strata gate buildings.

**Building**
A surface installation placed on a tile. Buildings are either **extraction** (harvesting raw materials from tile deposits) or **processing** (consuming inputs and producing outputs via a recipe) or **infrastructure** (affecting logistical or economic capacity). Each building holds a `building_component` and a `stockpile_component`.

**Discontent**
A nation's population mood toward its taxes (Ben, 2026-10-16): one reading per nation between 0 and 1, raised by sustained high tax tiers and lowered by low ones. Above its threshold it cuts output on every building on the nation's ground, up to 15%. Distinct from **Sentiment**, which is a relation between two actors. See `docs/politics/NATIONS.md` § Taxes and Discontent.

**Domain**
The three exclusive tile sets a province is partitioned over — **land**, **coastal water**, **open ocean** — and the axis a unit's traversal is declared against. A tile's substrate names exactly one, so the domains never mix and **a province never spans two of them**. Domain is derived from the substrate of any member tile, never stored. It answers *what kind of place is this*; ownership and traversal are separate questions asked of it. See `docs/generation/PROVINCES.md` § Who owns water and `docs/military/MILITARY.md` § Domains and traversal.

**Coastal water**
The `coast` and `lake` substrates — the shore's apron. **Owned**, by whoever owns the shore that claims it, with the owner derived from the tiles exactly as on land (`world::tile_to_nation`). Coastal and naval units may occupy it; a **land** unit may cross it only where its own polity owns it, which is the middle case that makes a held shoreline walkable and a rival's not. It is the only water a building may sit on, and the only building is a **port**.

**Open ocean**
The `ocean` substrate — the deep sea. **Unowned, structurally**: it is never territory, and no nation carve claims it. Only coastal and naval units may be there, and nothing may be built on it. The asymmetry against coastal water is deliberate — holding requires standing somewhere, and the deep sea is *crossed* rather than held, so what makes it matter is traffic rather than title. Its provinces are larger than any land province (80-tile guard, mean ~41) and exist as addressable empty space.

**Road (Track / Road / Highway)**
A per-tile land cost-reducer, not a building — the `tile_component.road_level` field (0 = none) discounts a tile's intra-body A* traversal cost. Three tiers form a ladder (BL-172): **Track** (`road_level` 1, ×0.67 traversal — minor / low-throughput), **Road** (2, ×0.50 — regular), **Highway** (3, ×0.40 — high-throughput backbone). "Throughput" is *cost-discount*, not a capacity cap. Roads are laid by world generation between cities (Highway/Road/Track by centre scale) and placed by the player from the build front door (any tier, upgrade-in-place). A **railroad** is a distinct transport *mode*, not a road tier (BL-173, railroad transport mode). See `docs/economy/LOGISTICS.md`.

**Canvas**
A screen that the player navigates to inform decision making and understand what's happening on a body or in space.

**Lens**
An overlay mode a canvas draws over its base render, selected from the canvas control strip — one active at a time (`overlay_mode`, `src/ui/ui_state.hpp`). Roster: Supply, Market, Resource, Population, Continent, Scarcity, Industry, Reach, Supply-routes, Logistics, Culture, Trade flow and Nations (`docs/ui/LENSES.md` § Roster is the authority). The **Logistics** lens is the one formerly named Throughput (Ben, 2026-10-24) — the player nation's Logistic Points, its roads coloured by supplied over requested. See `docs/ui/LENSES.md` for each lens's surface and key.

**Active (state)**
The navigation **anchor** — the body or tile the canvas zoom ladder is currently framed around. Persists until the player navigates. Distinct from Selection: selecting an entity does not change what is Active. Backed by `ui_state.active_body`; there is no per-tile anchor (BL-363). See `docs/ui/SELECTION.md`.

**Focus (state)**
The entity **under the pointer** in the current frame — the transient hover target that drives the tooltip / hover card. Distinct from both Active and Selection. See `docs/ui/SELECTION.md`.

**Economic focus**
A **nation's** character field — extraction, processing or trade — derived from the regions it settled during industrialisation (`docs/generation/NATION_GENERATION.md`). The build scorer's focus weight reads it (Ben, 2026-10-26; `docs/ai/AI_OPPONENT.md` § 5, Scoring). It replaces the retired corporation's *industrial focus*. Never shortened to "focus", which is the hover state above.

**Sticky card (glance-then-stick)**
The hover-card model (BL-230, glance-then-stick; `src/ui/hover_card.hpp`). A stable hover first shows a **glance** card that tracks the live cursor (0.5 s of stable hover before it appears); the card then **sticks** — its anchor freezes so the pointer can travel onto it — while never capturing the pointer, so canvas hover and click still resolve to whatever lies beneath. Hovering never opens the Selection band; that is the click's job alone.

**Selection (state)**
The entity the player **single-clicked to inspect**. Persists until another entity is selected (or the selection is cleared). Drives the **Selection info element** and its 'go to' target, and does not move the canvas. Backed by `ui_state.selected_entity`. See `docs/ui/SELECTION.md`.

**Item (backlog item)** *(formerly "Brief")*
A unit of **described intent** in the backlog (`docs/development/backlog.json` — the metadata index, with its design prose in the `design` field; `BACKLOG.md` holds only a tombstone plus pointer stubs; a landed item's prose moves to `docs/development/archive/backlog-design-<quarter>.json`): a problem to solve, a feature to build, or a doc to write, captured with enough context and file pointers to plan later — but deliberately carrying *no* implementation breakdown. An item is the design-level view of a single piece of work; **promoting** it into `docs/development/REFINED.md` decomposes it into a **task group** (one item ↔ one group of tasks). Distinct from a **task**, which is one file-scoped, individually-buildable step within that group. Every item carries a **design state** (see below): `designed` (✓) or `design-owed` (~). See `docs/development/DELIVERY.md`.

**Design state (item)**
Whether an **item**'s design is settled. The authoritative value is the `status` field in `backlog.json`; the glyph in `BACKLOG.md` mirrors it 1:1. **`designed` (✓) — not implemented:** the design is settled and the item is **promote-ready** (it may still be *blocked* on a dependency existing — a sequencing fact, not a design gap). **`design-owed` (~) — not implemented:** design is still owed and must be settled *before* the item is promoted. Orthogonal to priority and difficulty. Only `designed` items are promotable. See `docs/development/DELIVERY.md` (§ Design state).

**Open (adjective)**
Of an item: **not yet implemented**. An open item describes intent that has not landed in `src/` — it lives in the backlog. An open item may additionally be **owed** to design (its explanation debt is unpaid; see below), or already settled. See `docs/development/DELIVERY.md`.

**Owed (adjective)**
Of an item: one that **owes the design documentation an explanation** — the design behind it has not yet been written into the subject's authority doc. An owed item carries the unpaid explanation debt itself (the backlog is the most up-to-date design source while the debt stands). Contrast **settled**. See `docs/development/DELIVERY.md` (§ Design state).

**Settled (adjective)**
Of an item: one that has **repaid its explanation debt** — the design has been written into the authority doc, so the documentation now carries the explanation rather than the item. Contrast **owed**. See `docs/development/DELIVERY.md` (§ Design state).

**Deliver / Delivery** *(formerly "Publish")*
The lifecycle for taking a `designed` item through to a committed, verified change — item-spanning requirement, task creation, requirements, parallelisation planning, completion, commit. *("Cut" is reserved for cutting a release; see below.)* See `docs/development/DELIVERY.md`.

**Batch Delivery** *(formerly "Batch Publish")*
Delivering **more than one item in a single work block**, run breadth-first under
**barrier semantics** — every item clears each Delivery step before *any* item begins the
next (see `docs/development/DELIVERY.md` § Batch Delivery).

A Batch Delivery **usually executes already-designed items — it does not normally include
design work.** Design is settled into the items *beforehand*; **pausing to design is preferred
to redesigning in place**, which is costly. The design-direction Q&A (below) catches *incidental*
calls a batch made — it is not a substitute for up-front design. When a broken-up session forces
**rapid redesign** of work already attempted, **refactor the in-flight tasks back into items**
(intent returned to the backlog, as when cancelling a group) and make a clean **second attempt**
from the item, rather than redesigning mid-batch.

Beyond the single-item lifecycle, a Batch Delivery carries a **documentation-coverage
discipline** a lone Delivery does not:

- **Doc-coverage determination (first).** Before execution, determine for each item whether
  the design docs already record the implementation it will produce — or whether that
  implementation is a **direct consequence of already-documented behaviour**. Items that
  pass need no doc work; items that fail are flagged **doc-changing**.
- **Per-item documentation collision map.** For each doc-changing item, build a collision map
  of the **documents** it will change (the doc analogue of the source-file collision map).
  Disjoint-doc items are parallel-safe — **fan out sub-agents to write the doc changes**;
  items touching the same doc stay sequential.
- **Transient change note per doc.** Every changed doc carries a **minor transient
  "what was changed" note** — a dated breadcrumb (a **visible `> ⟳` blockquote**, the standard
  form) recording the edit, removed once the user has reviewed it.
- **Standing review reminders.** A Batch Delivery **always adds an `S`-tier item** (one per
  changed doc) under § Documentation, reminding the user to review the doc changes.
- **Design-direction Q&A (proportional).** When the batch made non-trivial or ambiguous design
  calls, it **closes by raising a Q&A** clarifying the design direction those calls surfaced,
  recorded with the session in the DEVLOG (see `docs/development/DEVELOPMENT_PRACTICES.md`
  § Design-direction Q&A). Skipped for a batch that surfaced nothing worth asking.

A Batch Delivery is a **strategy, not a code-sprint**: collision mapping (which file
write-sets may fan out vs. stay serial) and **session boundaries as checkpoints** (a large set is
*paused* at a clean, resumable boundary rather than forced to complete — REFINED.md § Pausing a task
group) are first-class parts of it, not afterthoughts. When a set spans more than one work block,
the standing slice plan is `docs/development/ROADMAP.md` § Near-term publish plan.

Distinct from a single-item **Delivery**, which carries no batch-level doc-coverage step. See
`docs/development/DELIVERY.md` and `CLAUDE.md` § Delivery pipeline.

**Complete (task state)**
A development task is **complete** only when every requirement it satisfies has been **reviewed**, **implemented**, and **tested** — completeness is measured against the requirements, not against "the code is written". A task that is implemented and builds but whose requirements have not all been reviewed and verification-run is *code-complete*, not complete. See `docs/development/REFINED.md` (§ Definition of "complete"), the requirement records in `docs/development/req/requirements.json`, and the policy in `docs/development/req/REQUIREMENTS.md`.

**Cancelled (task state)**
A task group that could not be driven to *complete* in one working block and is reverted rather than left half-tracked. Cancelling marks its requirements `failed`, rewrites its intent back into the backlog (merging into a related item where possible), and removes the task stubs from `REFINED.md`. It reverts *tracking*, not committed code — landed code stays in the tree; its intent returns to the backlog. Distinct from *paused* (a clean, resumable handoff that keeps the group in `REFINED.md`). See `docs/development/REFINED.md` (§ Cancelling a task group).

**Paused (task state)**
A task group deliberately stopped at a session boundary before completion, as a scoping choice rather than a failure. Unlike *cancelled*, a paused group stays in `REFINED.md` for the next session to resume; it is legitimate only when the stop is **clean and resumable** — `REFINED.md` is true to state (the in-flight task marked as the resume point), the build is green or the breakage is noted, and a one-line handoff records where to resume. A paused group is an explicit, recorded intermission, not a terminal state. See `docs/development/REFINED.md` (§ Pausing a task group) and `CLAUDE.md` (§ Delivery pipeline).

**Cut (release process)**
The ritual of finalising a version: merge the working branch into `main`, snapshot `src/` to a local `backups/vX.Y.Z/`, stamp `CHANGELOG.md` and the README "Latest release" line, commit, apply an annotated git tag (`vX.Y.Z`), and push with `--follow-tags`. The **tag** is the authoritative version-history record; the local backup is a convenience rollback point, not the record. "Cutting v0.0.5." See `docs/development/DEVELOPMENT_PRACTICES.md` (§ Cutting a release).

**Era**
A named phase in the game's industrial arc, defined by the accessible territory, available buildings, dominant strategic challenge, and **the one catastrophe the player is playing to avoid**. The ladder is 1-based and each number matches its catastrophe (renumbered 2026-08-31): **Era 1** (Terrestrial — nuclear war) transitions to **Era 2** (Early Space — climate collapse) by meeting an explicit gate condition. Note this is the *document's* Era axis; `era_band` and `condition_subject::era` are separate axes that did not renumber with it. See `docs/economy/ERAS.md` § The ladder and `docs/CONCEPT.md` § Eras.

**ISRU**
In-situ resource utilisation. The practice of producing resources — particularly propellant — from materials extracted at the operating location rather than shipped from the home planet. The primary logistical lever in Era 1. See `docs/economy/ERAS.md`.

**Ledger**
A view which provides a report on a sub-system to give detail for decision making.

**Drill-through**
The single progressive-disclosure idiom intended to be shared by every dense surface — a summarised figure opens the detail that produced it, rather than each panel inventing its own. The term is BL-214's (drill-through concept).

**Comms (comms log)**
The channel-based chat panel — the surface of the diplomacy-as-communication principle, where the mechanical actions of AI corps surface as messages before any AI "speaks" for real. Docked bottom-left of the shell at the Selection-band height (BL-227, comms dock; `src/ui/chat_panel.{hpp,cpp}`). See `docs/ui/CHAT.md`.

**Recipe**
The configured input/output specification of a processing building. One building type may support multiple recipes; the active recipe is set per building. Recipe conversion rates are authored in Lua.

**Resource**
Any tradeable good in the economy. Resources occupy one of three tiers: raw materials (extracted from tile deposits), refined goods (produced by processing buildings), or products (manufactured from refined goods). See `docs/economy/RESOURCES.md`.

**Stockpile**
**Retired as a corporation's store (Ben, 2026-10-10; `docs/economy/MARKETS.md` § The shelf economy).** Corporations hold no stockpiles: every good is on a market's **shelf**, which the market owns. The word survives only for a nation's stores in the history passes. The `(corp, market)` pool it once named (Ben, 2026-09-15) is gone.

**Shelf**
A market's own inventory of goods for sale. Production lands on it and is paid on landing; every buyer — processor, construction site, household, trade — buys from it (`docs/economy/MARKETS.md` § The shelf economy).

**Planetary Marketplace**
The trade building: a corporation's building that makes **trade points** (`docs/economy/TRADE.md`).

**Trade points**
An owner's capacity to move goods between markets, made by its Planetary Marketplaces each tick — a rate, never banked. Distinct from **Logistic Points**, the network's cap on what moves through a place (`docs/economy/TRADE.md`). In the history passes a **nation** earns them.

**Trade capacity**
The units of a resource one trade point moves per tick, authored per resource (`docs/economy/TRADE.md`).

**Trade**
A standing route an owner sets — buy a resource on one market's shelf, haul it, land it on another — run with trade points; the only way goods move between markets (`docs/economy/TRADE.md`). Not a ledger position (`MARKETS.md` § Trades on the ledger — positions and history).


**Culture share**
A per-mille distribution over peoples, carried on a **region** and on a **population centre** — never on a province or a tile. It crosses each generation handoff whole, so a conquest arrives half-digested and a city full of migrants reads mixed. A **province** *reads* culture by blending the shares beneath it; it holds none of its own. See `docs/generation/INDUSTRIALISATION.md` § 4 and `docs/ui/LENSES.md` § Culture lens.

**Industrialisation (generation phase)**
The fourth and last simulated span of generation, **1660 → 1960 CE**: it opens the instant Exploration closes and ends at the campaign epoch, so the world it produces is the world the campaign opens on. Its boundary is the epoch — everything after 1960 is the campaign era, played rather than generated. Computing and networks, what ordinary speech calls *digitisation*, belong to the campaign, never to this phase. Capitalised, it names the phase; do not confuse it with a **region** industrialising (its furnace crossing, a dated event: `region::industrial_year`, `docs/lore/HISTORY.md` § Stage 4). The phase runs no technology tree (`docs/generation/trees/TREES.md`). See `docs/generation/INDUSTRIALISATION.md`.

**Industry point**
The Industrialisation span's located industrial stock: one number per population centre, built by large cities from their scale, fuel within reach and capital paid in. Spent on **rail**, **mechanised force** or **works**; what is left at the epoch is the centre's **charter budget**. Located, so a conqueror who takes the city takes it. Not a campaign quantity. See `docs/generation/INDUSTRIALISATION.md` § Beat 1.

**Charter budget**
A population centre's unspent industry points at the epoch — the capital the landscape search spends on **charters** around that centre: priced sets of buildings owned by the centre's nation, on its own ground (Ben, 2026-10-23, the firms it once bought retired). It is what makes industrial density follow cities. See `docs/generation/INDUSTRIALISATION.md` § This phase sets budgets; the search spends them.

**Decolonisation**
A subject polity leaving its overlord in the Industrialisation span — by refusing renewal of its tribute clause once it can pay for itself, or by a war over empire. Holding a subject grows dearer as the subject's cities and industry grow, whether or not anyone fights. See `docs/generation/INDUSTRIALISATION.md` § Beat 3.

**Proxy war**
A war in a weak or decolonised polity with a great power as **patron**, paying into one side. At the epoch it crosses into play as a **standing war condition** on the provinces it covers — contested ground, interdicted supply, raised ordnance demand, a named patron — not as a war any actor plays. See `docs/generation/INDUSTRIALISATION.md` § 3.

**Listed value**
A **market's** cap: the summed buyout valuation of the firms headquartered in its catchment. Derived, never stored, and a sum over firms — it gives no corporation equity or a share price. See `docs/economy/MARKETS.md` § A market's listed value.

**GDP**
A **nation's** valued production — output on its tiles at local market prices. Derived, never stored. GDP per head is the axis of inequality between nations. See `docs/politics/NATIONS.md` § GDP.
