# Project Io — Space Assets

> **Settles:** what makes an asset a space asset rather than a building · the near-space classes ·
> who launches them · the orbital graph, its nodes, links and thrust cost · orbital slots · how an
> asset is launched, placed and kept · satellite roles and what each does · astronauts · what a
> station is, how its modules are fitted and what each module does · signal delay and comms
> coverage.
> **Not here:** fixed surface installations and their recipes (PRODUCTION) · when each class and
> module unlocks (RESEARCH, the Digitisation tree) · which resources space makes reachable
> (RESOURCES) · how a nation chooses to launch (`../ai/AI_OPPONENT.md` § 11) · space units, fleets
> and space combat (`../military/SPACE_FLEETS.md`) · how the canvases draw it
> (`../ui/CIRCUMPLANETARY.md`, `../ui/SOLAR.md`).
> **Confused with:** PRODUCTION.md, ERAS.md, SPACE_FLEETS.md.

**Space assets** are a nation's deployable presence beyond the planetary surface — distinct from
**buildings**, which are fixed surface installations on a body's tile grid. A building is placed on
a tile and draws on local terrain, workforce and adjacency. A space asset occupies orbital or
interplanetary space, is launched or constructed off-world, and is defined by the **orbital node**
it sits at rather than by a tile.

## Era 1 near space (Ben, 2026-10-15)

Through Era 1 (1960 → about 2040) nations explore the bodies near their home world. Space assets
are **full assets**: owned objects with a position, a launch, cargo and upkeep, saved with the
world and drawn on the canvases (`../ui/CIRCUMPLANETARY.md`, `../ui/SOLAR.md`).

**Nations launch (Ben, 2026-10-15).** A space asset belongs to a nation and is paid from its
treasury. Its goods are bought off the ordinary market's shelf, from whichever nations' buildings
made them.

The near-space classes:

- **Satellites** — uncrewed orbital instruments, each with one **role** (§ Satellites).
- **Rovers** — uncrewed surface units landed on another body; they move and survey off any nation's
  tile grid.
- **Crewed missions** — a crew sent to a body and returned, first to the home world's moon.
- **Station cores** and **station modules** — crewed orbital platforms (§ Space stations).
- **Launch infrastructure** — the ground side: launch sites, tracking stations and the industry that
  makes rockets and propellant. These are buildings (PRODUCTION) that a space asset requires.

**Upkeep** is paid each quarter; an asset unpaid decays and is lost. It counts inside the nation's
net (`FINANCE.md` § The quarterly return; Ben, 2026-11-07).

## The orbital graph (Ben, 2026-11-10)

**Space is a graph of named nodes, not a coordinate field.** An asset sits *at* a node; it never
has an altitude, an inclination or a position between nodes. Travel is a path along the graph's
links, and every link carries a **thrust cost**. The graph is a pure function of the bodies, so it
is never saved — only which node an asset occupies is.

**The nodes.** Every body except the star carries three:

| Node | What it is |
|---|---|
| **Surface** | The body's ground. Not a node an orbital asset may occupy. |
| **Low orbit** | Close and cheap to reach from the surface; the most room. |
| **High orbit** | The far, stable orbit — on a home world, where the comms and observation satellites want to be. Scarce. |

Every **planet–moon pair** and every **star–planet pair** adds four **Lagrange points**:

| Point | Where it sits | What it is for |
|---|---|---|
| **L1** | On the line between the pair, inside the smaller body's orbit. | A cheap transfer hub between the two. |
| **L2** | On the same line, beyond the smaller body. | A transfer hub and a quiet observatory site. |
| **L4** | 60° ahead of the smaller body on its orbit. | Stable: a station site. |
| **L5** | 60° behind it. | Stable: a station site. |

L3 is left out: it sits behind the larger body, and nothing wants to go there. A Lagrange point's
position is derived each frame from the pair's live orbital angles, so it moves with them.

**The links and their thrust cost.** Thrust cost is measured in **Δv units**, an abstract measure of
how hard a manoeuvre is. It is derived from the bodies and never authored per link:

- **Surface ↔ low orbit** — the climb out of the gravity well. Its cost grows with the body's mass
  (`mass_earths`) and its atmosphere, so a small, airless body is cheap to leave.
- **Low orbit ↔ high orbit** — a fixed fraction of the climb.
- **High orbit ↔ a pair's L-points** — the parent's high orbit reaches the L-points of every pair
  it is the larger member of; the smaller body's high orbit reaches its own pair's L1 and L2.
- **L-point ↔ L-point** within a pair — cheap.
- **Star–planet L1/L2 ↔ another planet's star–planet L1/L2** — the interplanetary transfer, its cost
  growing with the difference in orbital radius. This is far space's link (v0.5.0).

**A path's cost is the sum of its links' costs**, found by the cheapest path through the graph. A
launch from the home surface pays its whole path; a later move pays from where the asset sits.

**Travel time** follows the path too: each link adds its own days, and a longer link adds more.

**Why a graph.** It keeps space legible and deterministic: the player reads *where* something is
by name, and *what it costs* as a sum of a few visible steps. It is the space analogue of the
province graph, not of the tile grid.

## Orbital slots (Ben, 2026-11-10)

**Every orbital node holds a fixed number of slots.** A satellite or a station core takes one; a
station's modules ride inside its core's slot. When a node is full, a launch to it is refused.

| Node | Slots |
|---|---|
| Low orbit | 8, plus 4 per Earth mass of the body, rounded down |
| High orbit | 4 on a planet, 2 on a moon or an asteroid |
| L4, L5 | 3 each |
| L1, L2 | 2 each |

**First come, first held.** A slot is held by the asset in it and by nothing else: no claim, no
lease. A lost asset frees its slot at once. The high orbit of a crowded home world is the first
place two nations compete for room in space — a Trade question before it is a Conflict one.

## Launching (Ben, 2026-11-10)

**A launch needs a staffed Launchpad on the nation's home body**, and the nation must hold the
**Rocketry** technology (`RESEARCH.md`; the Digitisation tree's `DG-NE-SO-B`).

**A launch buys its goods off the home market's shelf** — `spacecraft_components` and
`propellant`, at the market's price, debited from the treasury. A shelf that cannot fill the whole
lot refuses the launch whole; nothing is half-bought. The lot scales with the payload's class and
the path's thrust cost: a further node burns more propellant.

**A launch is refused whole** — the world is not touched — when the nation has no staffed pad, lacks
the technology, cannot pay, the shelf cannot fill the lot, the class may not go to that node, or
the node has no free slot.

## Satellites (Ben, 2026-11-10)

**A satellite has one role**, chosen at launch:

| Role | What it does while active |
|---|---|
| **Comms** | Puts its body's nodes inside its owner's comms coverage (§ Signal delay). With the *Communication Satellites* technology, it also lifts the activity fog on every market its owner trades with. |
| **Navigation** | Cuts its owner's logistics cost on the body it orbits. |
| **Observation** | Lifts the vision fog over the body it orbits, for its owner. |
| **Survey** | Surveys the body it orbits from orbit (`../ui/DISCOVERY.md` § The geographic fog). |

**A role works from where the satellite sits.** A satellite's body is the body whose node it
occupies; an L-point satellite serves the pair's smaller body.

## Astronauts (Ben, 2026-11-10)

**An astronaut is a trained labour pool, not a person.** A nation holds a count of astronauts, as it
holds a count of workforce. `../PEOPLE.md` makes a person only where a role gates something, and
one astronaut among a hundred gates nothing.

**Astronauts are trained on the ground** by an **Astronaut Training Centre**, a building
(PRODUCTION) that turns its staffed workforce into astronauts at a fixed rate per quarter, up to a
pool cap.

**A crewed asset takes its astronauts with it.** A station core, a crewed module or a crewed mission
draws its crew from the pool at launch, and is refused when the pool is short. Crew on a lost
asset are lost with it.

## Space stations (Ben, 2026-11-10)

**A station is not a body.** It has no tile grid, terrain or population centre, and nothing
generates on it. It is fitted the way a unit is designed (`../military/MILITARY.md` § Land — the
classes; `../ui/ledgers/military.md` § The designer): the player picks which module goes in which
slot.

**The core comes first.** Launching a **station core** places it at an orbital node, in one of that
node's slots, and opens a grid of **5 × 3 slots**. The core takes the centre slot, leaving 14 empty
for modules. The core itself supplies a little power, room for two crew and life support for two.

**A module is launched into a slot.** Fitting a module launches it from the home surface to the
station's node; it occupies its grid slot when it arrives. An occupied slot refuses a second
module; removing a module frees its slot and returns nothing.

**Templates are saved layouts.** A **template** is a named layout: a module for each slot of a grid
size. Applying a template to a station queues a launch for every slot the station has not yet
filled to match it. A template is the nation's own, like a unit design.

**Upgrades grow the grid outward.** Each upgrade adds a ring of slots around the grid: 5 × 3 →
7 × 5 → 9 × 7. An upgrade is a launch (a truss segment) and is unlocked by research. The core stays
at the centre; existing modules keep their place.

**Edge modules.** A **Dock** and a **Shipyard** must sit on an **edge slot** — a slot on the grid's
outer ring — because ships come and go from them. Every other module may sit anywhere. When an
upgrade grows the grid, an edge module that is no longer on the edge **stops working** until it is
moved to an edge slot; moving a module is a launch-free refit that takes one quarter.

**Power and crew are budgets.** Each module draws power and needs crew. The station's power is what
its Power modules and its core supply; its crew room is what its Habitation modules and core hold;
its life support covers a number of crew. **A module works only when the station's power, crew
room, life support and crew all cover it**; when they fall short, modules stop in reverse order of
fitting, so the newest stops first. **Solar power falls with distance from the star** — by the
square of the body's orbital radius relative to the home world's — so the same Power module
yields less further out.

**The module roster.** Unlocked by research:

| Module | What it does while working |
|---|---|
| **Research** | Earns research points for its owner each quarter. |
| **Defence** | Protects the station and its node in space combat (`../military/SPACE_FLEETS.md`). |
| **Power** | Supplies power. |
| **Dock** | Lets space units dock, refuel and unload (`../military/SPACE_FLEETS.md`). Edge only. |
| **Habitation** | Adds crew room. |
| **Life support** | Adds life support; buys food rations off the home shelf each quarter. |
| **Fuel depot** | Holds propellant, for refuelling docked space units. |
| **Shipyard** | Builds space units too large to launch from a surface. Edge only. |
| **Microgravity factory** | Turns goods bought off the home shelf into goods only orbit can make, sold onto the home market. |
| **Observatory** | Surveys a body faster: a survey of any body in its owner's comms coverage completes sooner. |
| **Comms relay** | Puts the nodes around its station inside its owner's comms coverage (§ Signal delay). |
| **Cargo hold** | A stockpile in orbit, for space convoys (`../military/SPACE_FLEETS.md`). |
| **Market** | Makes the station a market centre, trading with its owner's home market by convoy. |

**A station's upkeep is the sum of its parts**: the core's and each fitted module's.

## Signal delay (Ben, 2026-11-10)

**A nation's orders reach space only through its comms coverage.** Coverage is a set of orbital
nodes:

- A nation's **home body's** low and high orbit are always covered — the ground stations see them.
- A **Comms satellite** covers every node of the body it orbits.
- A **Comms relay** module covers its station's node and every node one link from it.

**An order to an asset outside coverage waits one quarter** before it takes effect: fitting a
module, applying a template, moving a satellite or retasking it. The order is shown as *pending —
no signal* until then. An order inside coverage takes effect at once.

**Data comes home only through coverage.** A Research module's points, an Observation satellite's
fog lift and a Survey satellite's progress are earned only while their node is covered. Out of
coverage the asset still works, and its upkeep is still paid, but what it learns waits for a signal.

**Why.** Coverage is what makes the Comms satellite and the relay worth their slots, and it makes
the first relay at a moon the step that turns a flag-planting into a working presence.

## Open

**The Market module's trade link** waits on space convoys (`../military/SPACE_FLEETS.md`). Until
convoys move between orbit and the surface, a Market module is fitted but has nothing to clear.
