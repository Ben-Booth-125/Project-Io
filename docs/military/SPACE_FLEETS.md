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

- **docked** at a station with a Dock and a Fuel depot, from the depot's stock — stocked each month
  off the market of the owner's off-world base on the station's body, else off the home shelf
  (`../economy/ERAS.md` § The propellant loop);
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
**Launchpad** on a body the nation holds, and launched to that body's low orbit; the climb spends
the pad's boost (`../economy/SPACE_ASSETS.md` § Boost), and the hull's kit and tank are bought off
that body's shelf. Any hull may be raised at a station's **Shipyard**, and
appears docked there.

**The climb is what makes a body cheap or dear.** A launch's boost scales with the
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

**Loading and unloading.** A carrier — a Hauler or a Lander — idle in a body's low orbit **loads**
goods off that body's shelf through its owner's staffed Launchpad there, buying the goods and
spending the pad's boost for the lift: the climb's Δv × the launch rate × the cargo's mass (Ben,
2026-11-14). It **unloads** its whole cargo down through a staffed Launchpad on the body below,
**burning the descent from its own tank** at the same rate (Ben, 2026-11-14): nothing is bought at
the far end, and a tank too low for the descent cannot unload. Unloaded cargo **lands** on that market and is sold there, as any convoy's
cargo is (`../economy/SUPPLY.md` § A shipment). A carrier holds one good at a time. Neither happens
while a hostile armed unit stands at the node.

**How a trade's Hauler runs** (the first cut; the dials are the space-unit dials):

- **The hull.** An idle, empty Hauler of the trading nation already in the source's low orbit
  takes the trade, refuelled over the source's Launchpad; with none, one is raised at that pad.
  So a nation's yard of Haulers grows only as fast as its trade outruns them.
- **The cap.** A Hauler carries no more than lets its laden outbound trip, the descent **and** its
  empty return all fit one tank, and no more than its pad's boost can lift. How much a trade can carry between two bodies is a number the player
  reads off the design, not a guess.
- **The flight.** It flies the cheapest path **hop by hop**, standing at every node of it, so
  every node of the path is a place it can be met.
- **The cost.** The trade pays the space side as it is bought — the hull readied, the lift spent
  from the source pad's boost and the descent burned from the tank — rather than a priced haul. The cargo is
  bought and sold as any trade's.
- **Interdiction.** At a node holding an armed unit of a nation at war with its owner, a Hauler
  with no armed unit of its own there is **lost with its cargo** (an unarmed hull has nothing to
  fight with). With an escort there, it **waits** at that node until the battle is decided, then
  flies on.
- **Who ships this way.** A nation holding a live Hauler design ships every inter-body trade as a
  Hauler. A nation holding none keeps the older straight-line space lane
  (`../economy/SUPPLY.md` § Logistical cost).

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

**The rules in full** (BL-1597 (space battles), first cut, 2026-11-10):

- **Who fights.** "At war" is the hostility the ground war reads: either nation hostile to the other
  (`../politics/RELATIONS.md` § 1). A nation is **armed** at a node when it has an armed unit there
  that is not in transit, or a station there with a working Defence module. A battle is between two
  nations at one node; every unit of either side at the node takes part each day, so a fleet that
  arrives joins it.
- **The day.** Battles open, then each is fought once. Each side's **pool** is fixed at the day's
  start: its units' firepower plus each working Defence module's fire. The two pools land at once.
  On each target in order the pool first spends the target's armour, then lands up to its hull
  points; a destroyed target passes the rest of the pool on, and a pool that cannot clear a target's
  armour stops there. So armour is paid once per target per day.
- **Defence modules** carry their own fire, armour and hull points in a battle. A module at zero is
  lost. A station is lost when its side has no armed unit left at the node, it has no working
  Defence module, and the enemy is still armed there.
- **A withdrawal is checked whole** when ordered, as any move is, and refused if the budget cannot
  pay it. It flies after the next day's fire, paying its Δv then. A side whose units have all left
  **withdrew**; one with no armed party left otherwise is **destroyed**. Peace closes a battle before
  the day's fire, and a recorded withdrawal flies at once.
- **The battle report** is the closed battle's record: per side, units, crew, Defence modules and
  stations lost and damage landed; days fought; and the **mass destroyed** — each lost unit's dry mass
  and cargo, and a fixed mass per lost module and core. Debris is made from that mass
  (`../economy/SPACE_ASSETS.md` § Debris).
- **The stall** (BL-1598 (debris), first cut, 2026-11-10; NR-1178). A day of fire on which neither
  side lands anything — each pool spent on the other's armour — is a **quiet day**; a day that lands
  anything resets the count. After a set number of quiet days in a row (a dial, first cut ten), with
  both sides still armed, the battle closes as a **standoff**: both sides hold, a recorded withdrawal
  flies at once, and the report is kept like any other. The same two nations do not re-engage at that
  node while their newest battle there is that standoff and neither side's fire could land; a fleet
  arriving, or a Defence module coming to work, is what breaks it. Why: two fleets that cannot hurt
  each other would otherwise hold each other engaged for ever, every move a withdrawal.

## Blockade (Ben, 2026-11-10)

**A body is blockaded** when an armed fleet of a nation at war with the body's holder sits at the
body's low orbit and the holder has no armed unit there. A blockaded body's space trade stops: no
convoy loads or unloads there, and no launch from it reaches orbit. This is CONCEPT.md's "active
conflict on a body cuts off all imports and exports", made positional.

**The rules in full** (BL-1599 (blockade and bombardment), first cut, 2026-11-10):

- **Derived, never stored.** A body is blockaded *for a nation* when an armed unit of a nation hostile
  to it (either direction, as § Battles reads "at war") stands in the body's low orbit, not in
  transit, and the nation has no armed unit there itself. An armed unit of its own **contests** the
  orbit and lifts the blockade; the battle that follows is § Battles'.
- **No launch.** The blockaded nation raises nothing at a Launchpad on the body, buys no propellant up
  through it, and launches no asset from it.
- **No load or unload.** A carrier loads or unloads through a pad on the body only when the body is
  blockaded neither for its own nation nor for the nation whose ground the pad's market stands on. A
  blockade stops the body's trade, whoever ships.
- **No trade between bodies.** A trade is not priced — by Hauler or by the straight-line lane — out
  of a body blockaded for the trader, or into or out of a market whose holder is blockaded there. A
  trade Hauler at a blockaded destination waits in orbit.
- **The escort's meeting.** An escorted trade Hauler that meets a hostile armed unit **opens the
  battle** there that day, and waits at the node until it is decided.

## Bombardment and fire at orbit (Ben, 2026-11-10)

**Orbit may strike the ground.** An armed unit in a body's low orbit may **bombard** a province or a
building on that body, firing a munition it carries (`MILITARY.md` § Missiles). Bombardment raises
Alarm as a missile strike does (`../politics/RELATIONS.md` § 6).

**The ground may fire at orbit.** An **orbital defence battery** — a building — fires each day at the
hostile units in its body's low orbit, with a munition from its own stock.

**The rules in full** (BL-1599 (blockade and bombardment), first cut, 2026-11-10):

- **A standing order.** A bombard order names one unit and one target, a building or a province. The
  unit must hold the Orbital Bombardment key, carry the orbital-missile armament (the munition), and
  stand in the low orbit of the target's body. It fires **once a day** until its unit, its target or
  the hostility is gone, or the unit leaves that orbit; then the order lapses. Peace lapses it.
- **What a strike does.** A strike takes the unit's firepower, times a dial, off the building's
  **condition** — the channel strategic bombing wears (`MILITARY.md` § Air): throughput scales with
  it and the daily repair mends it. Only a building whose condition bites is a target. A province order
  strikes the hostile building in best condition there, ties to the lowest id.
- **The Alarm.** Each strike is **force used** (`../politics/RELATIONS.md` § Who emits each factor):
  the struck building's owner observes the bomber, magnitude the condition lost times a dial.
- **The battery** is a building, opened by the Orbital Defence Battery key. Each day it fires its
  firepower, scaled by its condition, at the hostile units in its body's low orbit, in a battle's
  target order — escorts, capital ships, unarmed hulls — armour first, then hull points. A unit at
  zero is destroyed, with its cargo and any order it held. It is a daily strike, not a battle side.
- **Simultaneous.** A day's strikes and a day's battery fire are fixed at the day's start, after the
  day's battles: a bomber the battery destroys still lands that day's strike.

## Rivals

Rival nations design, raise, move and fight space units through the scored-utility nation layer
(`../ai/AI_OPPONENT.md` § 11, the 2026-11-10 grant).
