# Project Io — Claude Reference

Project Io is a near-future space-based 4X grand strategy game. The player controls a corporate entity competing through resource extraction, trade, and military conflict across an Earth-like solar system. The project is in prototype phase, solo-developed in C++ with Lua scripting.

Read the documents below before responding to any request. They are the authoritative source for all design and technical decisions.

---

## Documents

**`docs/CONCEPT.md`**
The player identity, core mechanics, and campaign design. Start here for questions about what the game is and how it should feel.

**`docs/SYSTEMS.md`**
Every game system, how they relate to one another, and which are load-bearing. Read this for questions about scope, system design, or how a feature fits into the whole.

**`docs/GLOSSARY.md`**
Canonical definitions for all project terms. Use these terms consistently. If a term is defined here, do not substitute alternatives.

**`docs/tech/TECH_FOUNDATIONS.md`**
All settled technical decisions: language, framework, architecture, tick model, data model, UI approach, and serialisation. Read this before writing any code or making any architectural suggestion. It also defines the prototype scope and what is explicitly excluded.

**`docs/development/INITIAL_INSTRUCTIONS.md`**
The prototype build sequence and the constraints that govern development assistance. Read this alongside TECH_FOUNDATIONS when working on implementation.

**`docs/development/DEVELOPMENT_PRACTICES.md`**
Testing framework (Catch2), naming conventions, documentation standards, and how Claude should handle each.

**`docs/development/DEVLOG.md`**
Running session log — chronological record of what was built each session and
in-session decisions. Consult when asked about prior work, open items, or why
a specific implementation choice was made.

**`docs/ui/CANVASES.md`**
Overview of the three primary canvases — Solar, Circumplanetary, and Planetary — arranged as a **zoom ladder** (descend by clicking a body in the primary, ascend by clicking the minimap). Covers the shared layout, selection/view state, and implementation approach. Per-canvas detail lives in `SOLAR.md`, `CIRCUMPLANETARY.md`, and `PLANETARY.md`; the minimap chrome and ladder navigation are in `MINIMAP.md`. Authoritative reference for Layer 2 and for later work that adds overlays to these canvases.

**`docs/ui/LAYOUT.md`**
Surface-level description of the application shell — how the screen regions (navigation pane, canvas area, time column, ledger windows) are arranged around the canvases. Read for questions about overall UI layout; CANVASES.md covers the canvas internals.

**`docs/economy/RESOURCES.md`**
The canonical resource list: all 23 resources organised into three tiers (raw → refined → product), their terrain affinity and body availability, the Era 0 / Era 1 split, and the seven-resource prototype subset. Read before any work involving resource types, tile deposits, or market goods.

**`docs/economy/PRODUCTION.md`**
All extraction and processing buildings: placement rules, valid terrain, output resources, and full recipe tables. Also covers the workforce scalar model, stockpile flow, and the Layer 3 prototype scope. Read before any work involving buildings, recipes, or production logic.

**`docs/economy/ERAS.md`**
The Era system — the formal phase structure of the game's industrial arc. Era 0 (Terrestrial) starts at the campaign epoch; Era 1 (Early Space) is unlocked by the Rocketry research + Launchpad + propellant gate. Read for questions about what is accessible when, and how the transition to space is structured.

**`docs/economy/TILES.md`**
Tile classification: the two-axis terrain model (composition × landform), resource deposit profiles per terrain type, ambient resource guarantee, and amenity tile concept. Read before any work involving terrain types, tile generation, or the `terrain_type`/`terrain_composition` enum. Includes an implementation note on the data model change.

**`docs/economy/POPULATION.md`**
Population centres, scale/agglomeration mechanics, land-use trade-offs, and habitability feedback. Deferred from the prototype but designed here so the data model positions correctly. Read for questions about workforce, habitability, or population demand.

**`docs/generation/TILE_GENERATION.md`**
The procedural tile generation strategy and six-pass pipeline used in `hard_coded_world.cpp`. Covers solar parameters (temperature class, atmosphere class, hydrological state, geological activity), prototype body profiles, the hybrid terrain / noise-banded ocean / cluster landform approach, and deposit generation rules. Read before any work on tile generation or the terrain enum expansion.

**`docs/generation/NATION_GENERATION.md`**
The procedural nation generation strategy: Voronoi BFS territory placement over the tile map, resource profile derivation, political character assignment, and naming. Nation system behaviour is an open item; this document covers generation only. Read before any work on the world political layer or campaign setup.

**`docs/generation/CORPORATION_GENERATION.md`**
The procedural corporation generation strategy: nation assignment, industrial focus, starting asset placement, and financial profile. Covers the player corporation marker, the deferred corporation selection screen, and open design items (franchising, nation-seeded privatisation, tax, Era-based sovereignty). Read before any work on faction setup or campaign initialisation.