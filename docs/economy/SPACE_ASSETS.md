# Project Io — Space Assets

**Space assets** are the player's deployable presence beyond the planetary surface —
distinct from **buildings**, which are fixed surface installations on a body's tile grid.
Where a building is placed on a tile and draws on local terrain, workforce, and adjacency,
a space asset occupies orbital or interplanetary space, is launched or constructed off-world,
and is defined by its orbit, trajectory, or roving position rather than by a tile.

They belong to the **space arc** (`docs/economy/ERAS.md` § The two arcs — parked on
`era/space` as DLC scope), and open with the Era 1 space transition.

In theme, the classes are (non-exhaustive):

- **Space stations** — crewed or automated orbital platforms (habitation, refining, docking).
- **Satellites** — uncrewed orbital instruments (survey, comms, observation).
- **Rocketry / convoys** — propellant-driven transports moving payloads between bodies and orbits.
- **Rovers** — surface-mobile units operating off the player's owned tile grid.

This document reserves the subject and marks its scope. The full design — asset types, the
construction/launch path, the orbital/positional model, upkeep, and integration with the Era 1
transition — is the space arc's to write; no backlog item owns it.
