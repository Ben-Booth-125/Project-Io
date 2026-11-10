# Project Io — Space Fleets

> **Settles:** what a space unit is and how it is designed · thrust and the Δv budget · launching,
> building, docking, refuelling and idling · moving along the orbital graph · space convoys as a
> kind of space unit and what a launch costs from a body · battles between fleets · blockade ·
> bombardment of the ground and fire from the ground at orbit.
> **Not here:** the orbital graph, slots, stations and modules (`../economy/SPACE_ASSETS.md`) · land,
> air and sea force (`MILITARY.md`) · what a war is and who may declare it (`../politics/RELATIONS.md`)
> · trades between markets (`../economy/TRADE.md`) · debris (`../economy/SPACE_ASSETS.md` § Debris).
> **Confused with:** MILITARY.md, SPACE_ASSETS.md, SUPPLY.md.

A **space unit** is a nation's ship: an owned object that sits at an orbital node or travels the
links between nodes (`../economy/SPACE_ASSETS.md` § The orbital graph). A **fleet** is the space
units of one nation at one node, under one order. Space units are the far-space half of the game:
they carry cargo, crew and colonists between bodies, and they fight.

## A space unit is raised to a design (Ben, 2026-11-10)

**Space units are designed the way land units are** (`MILITARY.md` § Land — the classes): a **hull
class** fixes the role, and each of its **slots** takes an item research has made available. One
live design per class; a unit keeps the design it was raised to.

| Hull class | Role | Launch from a surface |
|---|---|---|
| **Hauler** | Carries cargo — the space convoy. | Yes |
| **Lander** | Carries cargo, crew or colonists down to a surface and back up. | Yes |
| **Tug** | Clears debris; moves an unpowered asset. | Yes |
| **Frigate** | Escort and interception. | Yes |
| **Cruiser** | The capital ship. | No — built at a Shipyard only |

**The slots.** Every hull has a **drive**, **tanks** and a class-dependent set of **payload** slots:
armament and armour on the warships, a cargo or passenger bay on the hauler and lander, a debris arm
on the tug. A design's stats are its class's plus its items': mass, Δv capacity, cargo, crew,
firepower, armour, hull points.

**Kit is goods.** Raising a space unit buys its hull's and items' goods off the shelf, as a land hire
does; a crewed hull also draws astronauts (`../economy/SPACE_ASSETS.md` § Astronauts).

## Thrust — the Δv budget (Ben, 2026-11-10)

**Every space unit carries a Δv budget**: its tanks' propellant, through its drive, against its mass.
A move along a path costs the path's Δv (`../economy/SPACE_ASSETS.md` § The orbital graph). **A
move the budget cannot pay is refused**, so how far a ship can go is a number the player reads before
ordering it.

**Loaded is heavier.** Cargo and passengers add mass, so a laden hauler's budget goes less far.

**Refuelling restores the budget**, by buying propellant at the unit's node:

- **docked** at a station with a Dock and a Fuel depot, from the depot's stock;
- **in a body's low orbit** where its owner holds a Launchpad on that body, off that body's shelf.

A unit with an empty budget can still sit where it is; it cannot leave.

## Where a space unit is (Ben, 2026-11-10)

A space unit is in one of four states:

- **Idle in orbit** — at a node, paying its upkeep. It holds no slot: slots are for satellites and
  station cores.
- **Docked** — at a node, berthed at a station with a free Dock. A Dock berths two units; a docked
  unit refuels, unloads into a Cargo hold, and is repaired a little each day. **A station docks its
  owner's units, and an ally's** (`../politics/RELATIONS.md`).
- **In transit** — between two nodes on a link, arriving after the link's days. It cannot be ordered
  until it arrives.
- **Landed** — a lander at a surface node.

## Raising and launching (Ben, 2026-11-10)

**A hull is raised in one of two places.** A hull that may launch from a surface is raised at a
**Launchpad** on a body the nation holds, and launched to that body's low orbit; the launch buys the
climb's propellant off that body's shelf. Any hull may be raised at a station's **Shipyard**, and
appears docked there.

**The climb is what makes a body cheap or dear.** A launch's propellant scales with the
surface-to-low-orbit link's Δv, which grows with the body's mass and its atmosphere. **A launch from
a moon or an asteroid costs a fraction of a launch from the home world**, which is why the moons
become the yards of a mature space power.

## Space convoys (Ben, 2026-11-10)

**A space convoy is a Hauler.** A trade between markets on different bodies (`../economy/TRADE.md`)
ships as a Hauler that is raised for it, loaded at the source body's low orbit, flown along the
cheapest path, and unloaded at the destination's low orbit or Dock, after which it returns. The
trade system dispatches it; the player may also order a Hauler by hand.

**A convoy is interdicted where it travels.** A hostile armed fleet at a node on a convoy's path
stops it there: the convoy fights or is lost with its cargo (§ Battles). Escort is a Frigate fleet
ordered to travel with a convoy.

**Orbit to ground is a landing.** Cargo between a low orbit and a surface market moves by the body's
Launchpad (up) and by a Lander or the Launchpad (down), at that body's climb cost.

## Battles (Ben, 2026-11-10)

**A battle happens at a node** when armed units of two nations at war are both there. It is fought
once a day until one side has no armed unit left at the node, or withdraws.

**The exchange is deterministic.** Each day, each side's total firepower is applied to the other
side's units in order of their hull class (escorts first, then capital ships, then unarmed hulls),
less the target's armour. A unit at zero hull points is destroyed and leaves **debris** at the node
(`../economy/SPACE_ASSETS.md` § Debris). A station's **Defence** modules fight on its owner's side
and take damage last; a station with no working module left is lost.

**Withdrawal** is an order to move away; a unit leaving pays its move's Δv and takes one more day of
fire.

## Blockade (Ben, 2026-11-10)

**A body is blockaded** when an armed fleet of a nation at war with the body's holder sits at the
body's low orbit and the holder has no armed unit there. A blockaded body's space trade stops: no
convoy loads or unloads there, and no launch from it reaches orbit. This is CONCEPT.md's "active
conflict on a body cuts off all imports and exports", made positional.

## Bombardment and fire at orbit (Ben, 2026-11-10)

**Orbit may strike the ground.** An armed unit in a body's low orbit may **bombard** a province or a
building on that body, firing a munition it carries (`MILITARY.md` § Missiles). Bombardment raises
Alarm as a missile strike does (`../politics/RELATIONS.md` § 6).

**The ground may fire at orbit.** An **orbital defence battery** — a building — fires each day at the
hostile units in its body's low orbit, with a munition from its own stock.

## Rivals

Rival nations design, raise, move and fight space units through the scored-utility nation layer
(`../ai/AI_OPPONENT.md` § 11, the 2026-11-10 grant).
