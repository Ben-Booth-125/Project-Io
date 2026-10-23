# Project Io — Space Assets

> **Settles:** what makes an asset a space asset rather than a building · the Era 1 near-space
> classes · who launches them · how they are built, launched, placed and kept.
> **Not here:** fixed surface installations and their recipes (PRODUCTION) · when each class
> unlocks (RESEARCH, the Digitisation tree) · which resources space makes reachable (RESOURCES) ·
> how a nation chooses to launch (`../ai/AI_OPPONENT.md` § 11).
> **Confused with:** PRODUCTION.md, ERAS.md.

**Space assets** are a nation's deployable presence beyond the planetary surface — distinct from
**buildings**, which are fixed surface installations on a body's tile grid. A building is placed on
a tile and draws on local terrain, workforce and adjacency. A space asset occupies orbital or
interplanetary space, is launched or constructed off-world, and is defined by its orbit,
trajectory or roving position rather than by a tile.

## Era 1 near space (Ben, 2026-10-15)

Through Era 1 (1960 → about 2040) nations explore the bodies near their home world. Space assets
are **full assets**: owned objects with a position, a launch, cargo and upkeep, saved with the
world and drawn on the canvases (`../ui/CIRCUMPLANETARY.md`, `../ui/SOLAR.md`).

**Nations launch (Ben, 2026-10-15).** A space asset belongs to a nation and is paid from its
treasury. Its goods are bought off the ordinary market's shelf, from whichever nations' buildings
made them.

The Era 1 classes:

- **Satellites** — uncrewed orbital instruments: survey, comms, navigation, observation.
- **Rovers** — uncrewed surface units landed on another body; they move and survey off any nation's
  tile grid.
- **Crewed missions** — a crew sent to a body and returned, first to the home world's moon.
- **Space stations** — crewed orbital platforms, assembled from successive launches.
- **Launch infrastructure** — the ground side: launch sites, tracking stations and the industry that
  makes rockets and propellant. These are buildings (PRODUCTION) that a space asset requires.

**A launch** spends a rocket, propellant and the payload's goods at a launch site, and places the
asset after a travel time. Orbits and bodies carry a cost to reach; the further body costs more.
**Upkeep** is paid each quarter; an asset unpaid decays and is lost.

## Open

The orbital model's grain, per-class costs and payloads, and what each class yields (survey data,
research points, prestige, deeds for quests) are set at the sprint 55 cut.
