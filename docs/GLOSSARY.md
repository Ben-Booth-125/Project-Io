# Project Io — Glossary

**Asset**
Any owned entity with economic or military value: a building, installation, unit, or vehicle. A corporation persists as long as it holds at least one asset; total elimination also requires destroying the parent nation.

**Body**
Any discrete celestial object in the simulation — planet, moon, asteroid, or station. Bodies are the primary unit of territorial control and the locations where resources are extracted, colonies are built, and conflict occurs.

**Survey**
The action that reveals a body's tile surface and deposit richness bands. A body starts **unsurveyed** (`survey_phase::hidden`) — only its type, orbital position, and grid size are known. Dispatching a survey debits credits upfront and reveals the body over sim ticks: a **transit** phase (probe en route, nothing revealed) then a **scan** phase that reveals the surface one **survey band** at a time in deterministic raster order until it is **surveyed**. Cost and duration scale with body size and distance from home. The home planet starts surveyed. This is the **geographic fog**; contrast **Activity fog**. See `docs/systems.md` § Exploration.

**Activity fog (commercial sphere)**
The second, independent fog (BL-089, commercial-sphere visibility): a body-level tier — `unknown` / `known_stale` / `known` / `visible` — derived purely from the player's own trade routes, live convoys, and building presence (`body_activity_visibility`, `src/world/world.hpp`; no stored state). Independent of the survey axis: a surveyed-but-unrouted body is still activity-unknown, an unsurveyed-but-routed one is known. See `docs/ui/DISCOVERY.md`.

**Corporation**
The **economic** actor — and today also the player's seat. Unlike nation-states, a corporation begins with no territorial claim or military force and must justify every asset through economic or strategic return. It persists as long as it holds any asset. Under BL-094's rewrite the corporation stays an economic actor but stops being the player's identity **or its own economic arm** — it becomes one of many independent **counterparties** the player's militia contracts with (BL-350). *(Corrected 2026-08-10, NR-120: this entry previously pointed at **Governing body**; that term is superseded — see **Militia**.)* **The operating sense is CONFIRMED and unchanged (Ben, 2026-08-21, resolving NR-492):** a corporation is the firm that holds buildings, runs recipes, carries stockpiles and places orders — `corporation_component`, `generate_corporations`, `corp_ai`, the Corporation lens. The new **ownership tier above it is a Syndicate**, not a second sense of this word.

**Syndicate**
*(Entry added 2026-08-21 — BL-524, the ownership tier. Word chosen on Ben's behalf, NR-497.)* The **ownership** actor: it holds equity in operating corporations, allocates capital, and lives off the profit. It owns no buildings and runs no recipes — that is what distinguishes it from a **Corporation**, which is the operating firm and keeps that meaning unchanged. **Seven syndicates** (the player one of them — Ben, NR-493) to roughly 88 corporations. **Equity confers operating control above a > 50% majority (Ben, 2026-08-21, NR-491, overturning an earlier call that it never did):** a corporation's **controlling holder** is the syndicate above that line, derived from the equity relation and never stored as a flag. A corporation whose controlling holder is the player's syndicate **is** the player's corp for the standing rule and is never auto-acted on; every other corporation runs itself on its own `corp_ai` scorer. **Below the threshold a holding is purely financial** — a claim on profit, no say in the build order — so crossing the majority line is a real strategic objective, and control costs attention, which is the tier's central trade-off. *Designed, not built* — BL-524, priority A, version goal v0.1.23 (proposed, NR-494); the mechanism lives in BL-525 (equity data model) through BL-530 (portfolio ledger). See `docs/development/backlog.json`.

**Militia (national private militia)**
*(Entry added 2026-08-10, replacing "Governing body" — NR-120.)* The seat the player is moving to, per BL-094's 2026-08-10 rewrite: an armed, national-in-allegiance actor that is **not** the state — it does not legislate, tax, or research. Its agency is **procurement and force**: it holds its own treasury (funded by its home nation's mandate, not shared with any corporation), buys equipment from private companies who can quote, delay, or refuse (BL-350), and fields what it buys. Chartered by one of the generated nations (`home_nation`) rather than itself one of the ~43 — nation generation is untouched by the pivot. *Designed, not built* — BL-094, priority A, version goal v0.3.0. Supersedes the 2026-08-03–2026-08-10 **Governing body** framing (a sovereign actor that legislates, taxes, researches and commands force) outright, not as an early reading of it.

**Headquarters (HQ)**
A corporation's seat building, drawn with the `ui::icons::hq` ringed-star glyph. Today it is the player-only home marker — the `hq` star on the building nearest the home-cluster centroid, paired with the home-cluster ring (BL-085). The **design direction (BL-182, deferred)** is that an HQ projects an **influence range**, and the union of a corporation's HQ ranges forms its **corporate border** — a sphere symmetric with nation territory. A corporation opens with one HQ and builds more as it advances (tech/law-gated); investing in fewer, deeper HQs (**tall**) versus more, spread HQs (**wide**) is the corporation's specialisation lever. See `docs/generation/CORPORATION_GENERATION.md` and BL-182.

**Country**
A generated nation; the territorial/political unit that holds tiles on a body. The Country map lens (`overlay_mode::country`, formerly "Faction") tints each nation's tiles by its identity colour (`nation_colour`) with dark borders between owners. See **Nation** and **Faction**.

**Nation**
A generated sovereign state on the homeworld — the territorial/political actor that owns tiles, carves resource-carved markets (BL-096), and hosts corporations. Generated by `src/world/nation_generation.cpp` (Voronoi growth from a seed budget the history ladder sets); the count is a derived consequence of geography, not an authored number. "Country" is the same unit seen from the map lens. **A nation no longer runs the broad background economy itself** — that premise (the "substrate") is superseded by real **background corporations** (BL-365, 2026-08-11; see `docs/economy/MARKETS.md` § Background corporations and `docs/generation/GENERATION_STRATEGY.md` § The economic premise). See `docs/generation/NATION_GENERATION.md`.

**Tick**
The fixed period between economy updates. Supply, demand, and market prices resolve at the end of each Tick. Real-time play continues within a Tick; the boundary is a hard economic checkpoint, not a pause.

**Tick is the INTERNAL term.** It names the simulation step, and it is the right word in code, comments, harnesses, data exports and design docs. It is **not** the word the player sees — see **Quarter**.

**Quarter (`qtr`)**
The player-facing name for one Tick. A Tick is ~3 months of campaign time, so four make a year (BL-095) — the two words denote exactly the same period, and "Quarter" is the more literally accurate of them.

Settled by Ben, 2026-08-01 (NEEDS_REVIEW NR-002): *"Qtr is the preferred term for any economy tick. Tick is too technical of a term for average gamers."* Every rate, duration and empty-state string a player reads uses Quarter or `qtr` — `Profit/qtr`, `payback ~5 qtrs`, `Est. qtrs left`, "run an economy quarter". Use the abbreviation `qtr` where space is tight (column headers, inline rates) and the full word in prose and tooltips.

The split is by audience, not by meaning: a CSV export column stays `tick` because its reader is an analyst, while the header beside it reads `/ qtr` because its reader is a player.

**Faction**
Legacy umbrella term — prefer the specific actor: **Nation** (political/territorial) or **Corporation** (economic). The map lens formerly named Faction is the **Country** lens (renamed by BL-052, faction → country — it always showed nations). "Faction" as a colour concept — player-vs-rival identity — is the **Corporation** identity colour (`corp_colour` / `corp_identity_colour`, `src/ui/presentation.hpp`). Per-faction sentiment is designed, not built.

**Market**
A pooled exchange with a physical boundary through which any faction can buy or sell goods with the delivery cost depending on logistical cost. Price is set by rarity on a body, then modulated by local supply and demand. All tiles belong to a market.

**Sentiment**
A numeric value representing one faction's disposition toward another. Sentiment is shaped by trade history, territorial conflict, and ideological alignment, and governs diplomatic options and the likelihood of conflict. *(Designed, not built — no sentiment value is stored anywhere yet; only data-model comments reference it. 2026-07-31.)*

**Tile**
The smallest subdivision of land on a body. Each tile has a fixed, procedurally generated profile covering resource deposits, terrain type, hazard level, and habitability. Tile properties determine local extraction yields, infrastructure construction costs, and combat conditions. Tiles are the granular unit of environment data, with many properties that are fixed upon generation.

**Region**
The unit of settlement and politics, one step above the tile. A region is founded by `run_settlement` at an **anchor** tile under a separation rule (`sep = max(3, grid_width/40)` — 7 tiles on the shipped 312×145 grid), and carries a culture, a founding culture, ancient endowment windows, works, demography and a generated name. The target is `land_tiles / 80` — roughly 159 on a default world, so a region's implied catchment is ~80 land tiles. Regions are what the Era −1 sim conquers and transfers: territory moves one region at a time. *(Called a "province" until 2026-08-18; renamed to free that word for the smaller locality cell. `src/world/settlement.hpp`.)*

**Province**
The **locality cell** — a small area grouping below the region and above the tile. Its purpose is to price locality *before* the market does: goods sourced inside one province are cheaper to the buyer than goods hauled in from outside it. **BUILT 2026-08-21** (BL-466 the partition, BL-511 the canvas grain, BL-515 the id scheme): `src/world/province.{hpp,cpp}` owns the partition and the tile→province mapping, and a plain click on the Planetary ground selects the province rather than the bare tile. **The locality DISCOUNT is still owed** — the partition exists, the price effect does not.

Size, measured rather than designed: the original "3–5 tiles" was an intention, and the shipped partition means **8.6 tiles** with a **12-tile preference** growth clamps to and a **20-tile hard cap** (6-seed sweep, 22,389 provinces, largest seen 16; ~4.9% sit above the preference, all of them by absorbing a singleton neighbour rather than by growing there). Read 12 as the preference and 20 as the limit; neither is 5. It is deliberately a *different object* from the region: a region is large, irregular, historically placed and politically owned, while a province is small, uniform and purely spatial. See BL-464 (Logistic Points) for the neighbouring throughput work, and NR-342 for the open duplication question — convoy haulage cost already prices distance, so the province discount must earn its place against that rather than restate it.

**Quarter**
One of nine 3×3 map slots (north→south × west→east) used as a **naming device**: a region's name is its culture's word plus the quarter word for where its anchor falls (`quarter_word`, `src/world/settlement.cpp`; BL-348 supplies nine such words per tongue). A quarter is not a gameplay object and owns nothing — it exists so names carry geography. *(Called a "region" until 2026-08-18.)*

**Deposit**
The per-**tile** quantity of a resource (`resource_deposit`, indexed by `resource_type`). A deposit is what an extraction building draws down, and it depletes. Distinguish from **ore field**, which is the seeded *cluster* of tiles whose deposits are elevated — a field contains deposits, never the reverse.

**The spatial vocabulary (settled 2026-08-18)**
Five nested or overlapping spatial objects, each with exactly one name, after a rename that found "province" and "region" carrying three meanings each. Smallest first: **tile** → **province** (locality, mean ~8.6 tiles, built) → **region** (settlement/politics, ~80 tiles) → **nation** (Voronoi from region anchors) → **body**. Cutting across them: **ore field** (a resource blob in tile generation), **quarter** (a naming slot), **survey band** (the unit of survey reveal), and **market catchment** (the tiles a market centre serves). If a new spatial object is added, name it here first — the collision that forced this rename existed because neither "province" nor "region" was ever defined in this file.

**Landform**
The **shape** axis of terrain. Since BL-519 (2026-08-21) a tile's terrain is three axes, not two:
`terrain_substrate` (what the ground is made OF — 8 values) × `terrain_cover` (what sits ON it — 10
values, with a `cover_density` scalar making cover **graded** rather than binary) × `terrain_landform`
(its shape: plains, highland, mountain, canyon, valley, crater, rift — `src/world/components.hpp`). Landform drives build cost (×1.0 plains up to ×2.0 mountain) and, since BL-231 (landform render) and BL-232 (spanning markers), a relief layer on the Planetary canvas — contiguous runs bridged into one named spanning marker. See `docs/economy/TILES.md`.

**Substrate / Cover**
The two axes BL-519 split out of the old `terrain_composition`, which was doing three unrelated jobs at
once. **Substrate** is what the ground is made of and is what deposits, defence and build cost read.
**Cover** is what sits on it — grass, scrub, forest, marsh, snow, dunes, ash, salt, **urban** — carried
with a `cover_density` byte, so a thin wood and a dense one are the same cover at different densities
rather than two enum values. The invariant: density is `0` **iff** cover is `none`. `urban` is a COVER
value (Ben's call), which means paving a tile no longer destroys the geology under it. `tundra` was
**dropped** — it is scrub on cold ground, which the three axes now say directly. See
`docs/economy/TILES.md` and `src/world/components.hpp`.

**Continent / Tectonic plate**
The Continents/Drift generation pass (`src/world/continents.cpp`) simulates a small number of drifting **tectonic plates** — count, drift, and speed all derived from Planetology's outputs, not rolled — whose boundaries bias the tile heightmap and append dated lines to the body's biography. The Continent lens (`overlay_mode::continent`, BL-226 continent lens) draws the plates the height bias was derived from, rather than inferring landmasses back out of the finished terrain.

**Planetology (and the generation chain)**
The body-level generation pass (BL-167, planetology; `src/world/planetology.cpp`): generated atmosphere, chemistry, and a simulated abiogenesis/evolution history, upstream of tile generation. "The chain" is the ordered, seeded, deterministic generation sequence: planetology → continents → tiles → population centres → history ladder → nations → roads → markets → corporations. Its front door is the **New World wizard** — the setup walk "New Game" opens; the world is not built until the wizard finishes (`start_new_game`, `src/core/app.cpp`). See `docs/generation/PLANETOLOGY.md` and `docs/generation/GENERATION_STRATEGY.md`.

**Ore field**
A seeded blob of tiles holding a disproportionate share of one resource, so an endowment lands *somewhere* rather than dusting evenly (tile generation Pass 6, `ore_fields_for` / `ore_field_map`). Copper, petroleum, iron and coal each redistribute roughly 45–65% of the world total into 2–3 fields. Conservation is over the resource's **bearing set**, not over all land. The mechanism is a deterministic post-multiply, not a roll. *(Was "ore province" until 2026-08-18 — renamed so "province" carries exactly one meaning. See § The spatial vocabulary.)*

**Word interface**
The text-drivable face of the game — the substrate an AI player uses, and v0.1.1's theme. Three legs plus a socket: **read**, the corp blackboard export (BL-206, `--export-blackboard`); **meaning**, the action dictionary (BL-270, `docs/ai/ACTIONS.json`); **write**, the corp-command seam (`src/world/corp_command.hpp`); and the **socket**, the Io MCP server (BL-278, `ProjectIo --serve` plus `tools/mcp/`). The write channel is narrower than the dictionary — some documented presses have no command verb. See `docs/ai/AI_OPPONENT.md` § 10.

**History ladder**
The institutional-history generation stage (`src/world/history_ladder.cpp`). The **pre-national ladder** (BL-221) runs after tiles and *before* nations, and drives them: it counts the agrarian cradles the land supported, prices conquest against exit, and sets the nation seed budget (`nation_params_from_ladder`) — the ladder is upstream of the map, not a narration of it. Its events carry historical-epoch timestamps (BL-220, dated epochs) and merge into the body's biography. The industrial stages (BL-222) and the averted rupture (BL-223) are open. See `docs/lore/HISTORY.md`.

**Building**
A surface installation placed on a tile. Buildings are either **extraction** (harvesting raw materials from tile deposits) or **processing** (consuming inputs and producing outputs via a recipe) or **infrastructure** (affecting logistical or economic capacity). Each building holds a `building_component` and a `stockpile_component`.

**Road (Track / Road / Highway)**
A per-tile land cost-reducer, not a building — the `tile_component.road_level` field (0 = none) discounts a tile's intra-body A* traversal cost. Three tiers form a ladder (BL-172): **Track** (`road_level` 1, ×0.67 traversal — minor / low-throughput), **Road** (2, ×0.50 — regular), **Highway** (3, ×0.40 — high-throughput backbone). "Throughput" is *cost-discount*, not a capacity cap. Roads are laid by world generation between cities (Highway/Road/Track by centre scale) and placed by the player from the build front door (any tier, upgrade-in-place). A **railroad** is a distinct transport *mode*, not a road tier (deferred, BL-173). See `docs/economy/SUPPLY.md`.

**Canvas**
A screen that the player navigates to inform decision making and understand what's happening on a body or in space.

**Lens**
An overlay mode a canvas draws over its base render, selected from the canvas control strip — one active at a time (`overlay_mode`, `src/ui/ui_state.hpp`). Current roster: Supply, Market, Country, Corporation, Resource, Population, Opportunity, Production, Scarcity, Industry, Reach, Continent, and Supply-routes. See `docs/ui/LENSES.md` for each lens's surface and key.

**Active (state)**
The navigation **anchor** — the body or tile the canvas zoom ladder is currently framed around. Persists until the player navigates. Distinct from Selection: selecting an entity does not change what is Active. Backed by `ui_state.active_body` (a per-tile anchor proved dead and was removed, BL-363). See `docs/ui/SELECTION.md`.

**Focus (state)**
The entity **under the pointer** in the current frame — the transient hover target that drives the tooltip / hover card. Distinct from both Active and Selection. See `docs/ui/SELECTION.md`.

**Sticky card (glance-then-stick)**
The hover-card model (BL-230, glance-then-stick; `src/ui/hover_card.hpp`). A stable hover first shows a **glance** card that tracks the live cursor (0.5 s of stable hover before it appears); the card then **sticks** — its anchor freezes so the pointer can travel onto it — while never capturing the pointer, so canvas hover and click still resolve to whatever lies beneath. Hovering never opens the Selection band; that is the click's job alone. Retired BL-200's dwell-to-open progress bar.

**Selection (state)**
The entity the player **single-clicked to inspect**. Persists until another entity is selected (or the selection is cleared). Drives the **Selection info element** and its 'go to' target, and does not move the canvas. Backed by `ui_state.selected_entity`. See `docs/ui/SELECTION.md`.

**Item (backlog item)** *(formerly "Brief")*
A unit of **described intent** in the backlog (`docs/development/backlog.json` — the metadata index, with its design prose in the `design` field; `BACKLOG.md` was drained on 2026-07-31 and now holds only a tombstone plus seven pointer stubs; a landed item's prose moves on to `docs/development/archive/backlog-design-<quarter>.json`): a problem to solve, a feature to build, or a doc to write, captured with enough context and file pointers to plan later — but deliberately carrying *no* implementation breakdown. An item is the design-level view of a single piece of work; **promoting** it into `docs/development/REFINED.md` decomposes it into a **task group** (one item ↔ one group of tasks). Distinct from a **task**, which is one file-scoped, individually-buildable step within that group. Every item carries a **design state** (see below): `designed` (✓) or `design-owed` (~). See `docs/development/DELIVERY.md`.

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
A named phase in the game's industrial arc, defined by the accessible territory, available buildings, and dominant strategic challenge. The game begins in **Era 0** (Terrestrial) and transitions to **Era 1** (Early Space) by meeting an explicit gate condition. See `docs/economy/ERAS.md`.

**ISRU**
In-situ resource utilisation. The practice of producing resources — particularly propellant — from materials extracted at the operating location rather than shipped from the home planet. The primary logistical lever in Era 1. *(Designed, not built. 2026-07-31.)*

**Ledger**
A view which provides a report on a sub-system to give detail for decision making.

**Drill-through**
The single progressive-disclosure idiom intended to be shared by every dense surface — a summarised figure opens the detail that produced it, rather than each panel inventing its own. The term is coined by BL-214 (drill-through concept), which is open; the idiom is owed, not built (2026-07-31).

**Comms (comms log)**
The channel-based chat panel — the surface of the diplomacy-as-communication principle, where the mechanical actions of AI corps surface as messages before any AI "speaks" for real. Docked bottom-left of the shell at the Selection-band height (BL-227, comms dock; `src/ui/chat_panel.{hpp,cpp}`). See `docs/ui/CHAT.md`.

**Recipe**
The configured input/output specification of a processing building. One building type may support multiple recipes; the active recipe is set per building. Recipe conversion rates are authored in Lua.

**Resource**
Any tradeable good in the economy. Resources occupy one of three tiers: raw materials (extracted from tile deposits), refined goods (produced by processing buildings), or products (manufactured from refined goods). See `docs/economy/RESOURCES.md`.

**Stockpile**
A per-entity store of resource quantities, held in a `stockpile_component`. Extraction and processing outputs accumulate in the building's stockpile each simulation step. At the economy tick boundary, all building stockpiles on a body aggregate into the body's market supply.

---

<!-- BEGIN GLOSSARY AUDIT (review only) — auto-managed block, safe to delete; the scheduled audit replaces everything between these markers -->

## Doc-drift sweep — 2026-07-31

> ⟳ This sweep replaced the 2026-06-16 audit (run 1 of 4). Actioned from it: **Nation** now has an entry; **Faction** vs **Corporation**/**Country** is disambiguated in the Faction entry. Added this sweep: Landform, Activity fog, Continent/Tectonic plate, Planetology, History ladder, Lens, Sticky card, Drill-through, Comms; Sentiment and ISRU marked designed-not-built. The old audit's naming flags (Focus/Canvas/Ledger, the Asset collision, Selection info element) were review-only suggestions and stand unactioned — Ben's calls, not drift.

<!-- END GLOSSARY AUDIT (review only) -->
