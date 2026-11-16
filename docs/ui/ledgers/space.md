# Space ledger — design Q&A

> **Settles:** the Space ledger's five axes · its views and what each answers · how the station
> designer reads and acts · what the ledger leaves to the canvases and the Selection band.
> **Not here:** the rules it surfaces (`../../economy/SPACE_ASSETS.md`, `../../military/SPACE_FLEETS.md`)
> · the canvases that draw space (`../CIRCUMPLANETARY.md`, `../SOLAR.md`) · the column's geometry
> (`../LAYOUT.md`).
> **Confused with:** military.md, convoys.md.

**Top question.** *What does my nation have in space, and what can it launch next?*

## Views (one question per tab)

| View | Question it answers |
|---|---|
| **Launch** (default) | What can I launch, to where, and what will it cost? A class picker (satellite with its role, station core, crewed mission, rover), a destination picker listing orbital nodes with their free slots and the path's thrust cost, and the quote: the boost the launch spends against what the home pad has banked, credits, travel days, crew drawn. Refused launches say why. |
| **Assets** | What do I have up there? One row per satellite, station and mission: class, role, node, state (in transit / active / decaying), upkeep, and whether its node is in comms coverage. Orders waiting on a signal show as *pending — no signal*. |
| **Stations** | The station designer for the station picked in the Assets view or on the canvas. |
| **Crew** | How many astronauts do I hold, how fast are they trained, and where are they? |

## The station designer

**The grid is drawn as it is**: 5 × 3 at first, with the core in the centre slot and every other slot
empty or holding a module. **Edge slots are marked**; a Dock or Shipyard offered for an inner slot is
shown greyed with the reason. An enclosed edge module shows *idle — needs an edge slot*.

**Pressing an empty slot** opens the module list unlocked by research, each with its power, crew
and life support. Choosing one quotes and launches it. **Pressing a filled slot** offers remove or
move. The station's **budgets** — power, crew room, life support, crew — sit above the grid as
supply against demand; a module that is stopped by a shortfall is marked so.

**Templates** sit beside the grid: save the current layout under a name, or apply a saved one
(queuing a launch per unfilled slot, its total quoted first). **Upgrade** grows the grid when
research allows, quoted like any launch.

## The five axes

- **Sub-levels + default.** Launch / Assets / Stations / Crew; opens on Launch.
- **Lens on open.** None. Space has no map field; the Circumplanetary canvas already draws the nodes.
- **Data sources.** The space-asset store, the station store and templates, the nation's astronaut
  pool, the orbital graph (paths, slots), the nation's comms coverage, the home pad's banked boost.
- **Toggle / close.** The rail icon toggles the ledger; re-clicking the active tab closes it.
- **What it leaves elsewhere.** A picked satellite or station's own card is the Selection band's
  (`selection.md`); the ledger is the list and the designer, not a second card.
