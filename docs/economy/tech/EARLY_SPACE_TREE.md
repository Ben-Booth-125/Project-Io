# Project Io — The Early Space Tree

> **Settles:** the nodes of the Early Space age's tech tree — its trees, branches, clusters,
> masteries and quests · which Digitisation quest keys each branch · which branches the
> Digitisation quests key that this tree does not author · every node's advantage, ring and
> requirements · the far-space mechanic keys each node unlocks.
> **Not here:** the grammar every age's tree obeys, research points and the queue (../RESEARCH) ·
> what Era 2 is and the gate that opens it (../ERAS) · the near-space unlocks, which are the
> Digitisation tree's (DIGITISATION_TREE) · what a hull, module or building does once unlocked
> (../SPACE_ASSETS, ../../military/SPACE_FLEETS, ../PRODUCTION).
> **Confused with:** DIGITISATION_TREE.md, ../RESEARCH.md, ../ERAS.md.

The Early Space tree is the tech tree of the age after Digitisation: far space (`../ERAS.md`
§ Far-space technology). It is cut to the grammar in `../RESEARCH.md` § The shape of an age's tree.
Its hulls, drives, the propellant loop's buildings, the orbital defence battery and the station's
last grid upgrade are researched here; the near-space classes, satellite roles, station core,
modules and first grid upgrade are the Digitisation tree's (`DIGITISATION_TREE.md` § Space unlocks).

The store is `early_space_tree.json`, canonical for every node; this document owns the prose and
carries the store's tables, rendered by `node tools/session/tech_tree_store.js early_space`, which
also checks the store against the grammar and against the Digitisation tree's quests (`--check`
fails on a stale table or a broken rule).

**Every advantage, cost and deed here is a placeholder, to be re-priced in play** — the rule the
Digitisation tree carries (Ben, 2026-10-10). The shape is the design; the numbers are where it starts.

---

## Who may research it

**A branch is locked until its Digitisation quest is held**
(`../RESEARCH.md` § Quests key the next age). Each branch names the quest that keys it
(`keyed_by`), and its name is that quest's `keys_next_branch`, so the two trees agree by
construction; the check holds them equal.

**The age opens for a nation when it meets the gate into space**
(`../ERAS.md` § Era 1 → Era 2 gate — space access): Rocketry held, a staffed Launchpad on the home
body, propellant on the home shelf. A nation that has not met the gate researches none of this tree, whatever quests it holds.

## The shape: three trees, one branch each

The tree keeps the age's three trees — *Production*, *Network*, *State* — and authors one branch in
each, the branch that carries far space's load-bearing technology. Each is the next-age branch of a
Digitisation branch, keyed by that branch's quest:

| Tree | Branch | Keyed by | Carries |
|---|---|---|---|
| **Production** | In-Situ Propellant | *Propellant at Scale* (Energy) | prospecting, base founding, the Ice Extractor, the Electrolysis Plant |
| **Network** | Interplanetary Convoys | *First Cargo to Orbit* (Signal & Orbit) | the Hauler, Lander and Tug hulls; drives, tanks, the debris arm |
| **State** | Space Force | *Proven Readiness* (Force) | the Frigate and Cruiser hulls; armament, armour, bombardment, the orbital defence battery, the Shipyard module, the 9 × 7 grid |

**The other seven keyed branches are reserved**: *Asteroid Prospecting*, *Orbital Assembly*,
*Off-World Exchange*, *Orbital Port & Depots*, *Settlement Charters*, *Planetary Accords* and
*Orbital Fleet* (keyed by the naval quest *Blue Water*). Each
names a branch of this age that a Digitisation quest keys; it holds no node, so holding its quest
opens nothing. The store lists them (`reserved_branches`), and the check holds the authored and
reserved branches together to exactly the Digitisation tree's `keys_next_branch` names.

**Every branch is the seventeen-node spine**: Travel I → Travel II → the first cluster's two minors
→ its major → two arms, Travel III and Travel V → Travel IV and Travel VI → each arm's two minors →
each arm's major → the mastery. This is the spine `../RESEARCH.md`'s grammar had before the
Digitisation trim (Ben, 2026-11-19: three travel nodes a branch, a cluster of one minor or two);
whether this tree takes the same cut is BL-1720 (Early Space trim). Space Force carries one kit cluster on its left arm, *Orbital Bombardment*, so its arm runs
Travel III → the cluster → Travel IV → the battery: 20 nodes. No node requires a node of another
branch. The tree has no joints, pairs or placeholders.

## Travel themes

| Branch | Theme |
|---|---|
| In-Situ Propellant | +1% propellant output |
| Interplanetary Convoys | −1% launch propellant |
| Space Force | +1% strength for every space unit |

## The trap

**Orbital Bombardment** opens striking the ground from low orbit, and every strike raises Alarm as a
missile strike does (`../../military/SPACE_FLEETS.md` § Bombardment and fire at orbit). It sits
before the orbital defence battery on its arm, so the nation that wants the battery buys the
weapon first.

## Quests and the age after

Each branch carries one quest, which keys the same branch of the age after this one (Era 3,
`../ERAS.md` § Era 3 and beyond). That age is not designed; its branch names are placeholders:

| Branch | Quest | Keys |
|---|---|---|
| In-Situ Propellant | Refuelled Off-World | Outer-System Propellant |
| Interplanetary Convoys | Interplanetary Trade | Deep-Space Convoys |
| Space Force | Orbit Held | Deep-Space Fleets |

## Effects

The effect kinds are the Digitisation tree's (`DIGITISATION_TREE.md` § Effects). Every unlock here
is a `mechanic` effect with a stable key, named as a constant in `src/world/space_tech_keys.hpp`;
the system that builds, launches or designs the thing reads the key through the nation's research.

| Key | Node |
|---|---|
| `prospecting` | Remote Prospecting |
| `base_founding` | Off-World Bases |
| `ice_extractor` | Ice Extraction |
| `electrolysis_plant` | Electrolysis Plants |
| `space_tanks_standard` | Storable Propellants |
| `space_drive_chemical` | Restartable Engines |
| `space_hull_hauler` | Space Haulers |
| `space_tanks_extended` | Extended Tankage |
| `space_hull_lander` | Landers |
| `space_debris_arm` | Rendezvous and Capture |
| `space_drive_nuclear_thermal` | Nuclear Thermal Rockets |
| `space_hull_tug` | Orbital Tugs |
| `space_armament_kinetic` | Kinetic Interceptors |
| `space_armour_whipple` | Whipple Shielding |
| `space_hull_frigate` | Frigates |
| `space_armament_missile` | Orbital Missiles |
| `space_armour_composite` | Composite Hull Armour |
| `orbital_bombardment` | Orbital Bombardment |
| `orbital_defence_battery` | Orbital Defence Battery |
| `space_module_shipyard` | Orbital Shipyards |
| `space_grid_9x7` | Heavy Truss |
| `space_hull_cruiser` | Cruisers |

## Every node

<!-- BEGIN GENERATED NODES (tools/session/tech_tree_store.js) -->

**54 nodes** — 21 travel (0 joints, 3 masteries), 20 minor, 10 major, 3 quest; 0 placeholder nodes. Whole age: 362 cost units.

### Production — what the nation can extract and make off-world

#### In-Situ Propellant (`PR-IP`) — keyed by *Propellant at Scale* (`DG-PR-EN-Q`)

| Id | Node | Tier | Ring | Advantage | Requires |
|---|---|---|---|---|---|
| `ES-PR-IP-T1` | Cryogenics I | travel | 1 | +1% propellant output | — (open) |
| `ES-PR-IP-T2` | Cryogenics II | travel | 1 | +1% propellant output | Cryogenics I |
| `ES-PR-IP-A1` | Remote Prospecting | minor | 1 | a surveyed body may be prospected for its deposits | Cryogenics II |
| `ES-PR-IP-A2` | Habitat Shielding | minor | 1 | off-world base upkeep -10% | Cryogenics II |
| `ES-PR-IP-A` | Off-World Bases | major | 2 | a base may be founded on another body | Remote Prospecting + Habitat Shielding |
| `ES-PR-IP-T3` | Cryogenics III | travel | 2 | +1% propellant output | Off-World Bases |
| `ES-PR-IP-T4` | Cryogenics IV | travel | 3 | +1% propellant output | Cryogenics III |
| `ES-PR-IP-T5` | Cryogenics V | travel | 2 | +1% propellant output | Off-World Bases |
| `ES-PR-IP-T6` | Cryogenics VI | travel | 3 | +1% propellant output | Cryogenics V |
| `ES-PR-IP-B1` | Regolith Drilling | minor | 3 | off-world extraction rate +5% | Cryogenics IV |
| `ES-PR-IP-B2` | Thermal Ice Mining | minor | 3 | Ice Extractor water output +5% | Cryogenics IV |
| `ES-PR-IP-B` | Ice Extraction | major | 4 | unlocks the Ice Extractor: water from an icy body | Regolith Drilling + Thermal Ice Mining |
| `ES-PR-IP-C1` | Cryogenic Transfer | minor | 3 | off-world propellant holding losses -25% | Cryogenics VI |
| `ES-PR-IP-C2` | Solar Electrolysis | minor | 3 | Electrolysis Plant power draw -10% | Cryogenics VI |
| `ES-PR-IP-C` | Electrolysis Plants | major | 4 | unlocks the Electrolysis Plant: water and power into propellant on its body’s market | Cryogenic Transfer + Solar Electrolysis |
| `ES-PR-IP-M` | Closed Propellant Loop | mastery | 4 | +3% propellant output | Ice Extraction + Electrolysis Plants |
| `ES-PR-IP-Q` | Refuelled Off-World | quest | 4 | propellant output +10%. **Deed:** a space unit of yours refuels on propellant made off your home world. **Keys next age:** Outer-System Propellant | Electrolysis Plants |

### Network — how far and fast the nation moves goods between bodies

#### Interplanetary Convoys (`NE-IC`) — keyed by *First Cargo to Orbit* (`DG-NE-SO-Q`)

| Id | Node | Tier | Ring | Advantage | Requires |
|---|---|---|---|---|---|
| `ES-NE-IC-T1` | Transfer Orbits I | travel | 1 | -1% launch propellant | — (open) |
| `ES-NE-IC-T2` | Transfer Orbits II | travel | 1 | -1% launch propellant | Transfer Orbits I |
| `ES-NE-IC-A1` | Storable Propellants | minor | 1 | opens standard propellant tanks (space hull tanks) | Transfer Orbits II |
| `ES-NE-IC-A2` | Restartable Engines | minor | 1 | opens the chemical drive (space hull drive) | Transfer Orbits II |
| `ES-NE-IC-A` | Space Haulers | major | 2 | opens the Hauler hull: the space convoy | Storable Propellants + Restartable Engines |
| `ES-NE-IC-T3` | Transfer Orbits III | travel | 2 | -1% launch propellant | Space Haulers |
| `ES-NE-IC-T4` | Transfer Orbits IV | travel | 3 | -1% launch propellant | Transfer Orbits III |
| `ES-NE-IC-T5` | Transfer Orbits V | travel | 2 | -1% launch propellant | Space Haulers |
| `ES-NE-IC-T6` | Transfer Orbits VI | travel | 3 | -1% launch propellant | Transfer Orbits V |
| `ES-NE-IC-B1` | Extended Tankage | minor | 3 | opens extended propellant tanks (space hull tanks) | Transfer Orbits IV |
| `ES-NE-IC-B2` | Precision Landing | minor | 3 | Lander descent propellant -10% | Transfer Orbits IV |
| `ES-NE-IC-B` | Landers | major | 4 | opens the Lander hull: cargo, crew and colonists down to a surface and back | Extended Tankage + Precision Landing |
| `ES-NE-IC-C1` | Rendezvous and Capture | minor | 3 | opens the debris arm (Tug payload) | Transfer Orbits VI |
| `ES-NE-IC-C2` | Nuclear Thermal Rockets | minor | 3 | opens the nuclear thermal drive (space hull drive) | Transfer Orbits VI |
| `ES-NE-IC-C` | Orbital Tugs | major | 4 | opens the Tug hull: clears debris, moves an unpowered asset | Rendezvous and Capture + Nuclear Thermal Rockets |
| `ES-NE-IC-M` | Transfer Network | mastery | 4 | -3% launch propellant | Landers + Orbital Tugs |
| `ES-NE-IC-Q` | Interplanetary Trade | quest | 2 | launch propellant -5%. **Deed:** a Hauler of yours delivers cargo between two bodies, neither your home world. **Keys next age:** Deep-Space Convoys | Space Haulers |

### State — what the nation can field and hold in space

#### Space Force (`ST-SF`) — keyed by *Proven Readiness* (`DG-ST-FO-Q`)

| Id | Node | Tier | Ring | Advantage | Requires |
|---|---|---|---|---|---|
| `ES-ST-SF-T1` | Orbital Drill I | travel | 1 | +1% strength for every space unit | — (open) |
| `ES-ST-SF-T2` | Orbital Drill II | travel | 1 | +1% strength for every space unit | Orbital Drill I |
| `ES-ST-SF-A1` | Kinetic Interceptors | minor | 1 | opens kinetic armament (warship armament) | Orbital Drill II |
| `ES-ST-SF-A2` | Whipple Shielding | minor | 1 | opens Whipple shielding (warship armour) | Orbital Drill II |
| `ES-ST-SF-A` | Frigates | major | 2 | opens the Frigate hull: escort and interception | Kinetic Interceptors + Whipple Shielding |
| `ES-ST-SF-T3` | Orbital Drill III | travel | 2 | +1% strength for every space unit | Frigates |
| `ES-ST-SF-K1` | Orbital Missiles | minor | 2 | opens orbital missiles (warship armament) | Orbital Drill III |
| `ES-ST-SF-K2` | Composite Hull Armour | minor | 2 | opens composite hull armour (warship armour) | Orbital Drill III |
| `ES-ST-SF-K` | Orbital Bombardment | major | 3 | an armed unit in low orbit may bombard a province or a building below. Trap: every strike raises Alarm as a missile strike does | Orbital Missiles + Composite Hull Armour |
| `ES-ST-SF-T4` | Orbital Drill IV | travel | 3 | +1% strength for every space unit | Orbital Bombardment |
| `ES-ST-SF-T5` | Orbital Drill V | travel | 2 | +1% strength for every space unit | Frigates |
| `ES-ST-SF-T6` | Orbital Drill VI | travel | 3 | +1% strength for every space unit | Orbital Drill V |
| `ES-ST-SF-B1` | Space Surveillance | minor | 3 | hostile space units seen at every node in your comms coverage | Orbital Drill IV |
| `ES-ST-SF-B2` | Ground-Launched Interceptors | minor | 3 | orbital defence battery fire +10% | Orbital Drill IV |
| `ES-ST-SF-B` | Orbital Defence Battery | major | 4 | unlocks the orbital defence battery: the ground fires at hostile units in low orbit | Space Surveillance + Ground-Launched Interceptors |
| `ES-ST-SF-C1` | Orbital Shipyards | minor | 3 | station Shipyard modules may be fitted | Orbital Drill VI |
| `ES-ST-SF-C2` | Heavy Truss | minor | 3 | station grids may be upgraded to 9 x 7 | Orbital Drill VI |
| `ES-ST-SF-C` | Cruisers | major | 4 | opens the Cruiser hull: the capital ship, built at a Shipyard only | Orbital Shipyards + Heavy Truss |
| `ES-ST-SF-M` | Space Superiority | mastery | 4 | +3% strength for every space unit | Orbital Defence Battery + Cruisers |
| `ES-ST-SF-Q` | Orbit Held | quest | 2 | space unit upkeep -5%. **Deed:** an armed fleet of yours holds your home world’s low orbit for 12 months. **Keys next age:** Deep-Space Fleets | Frigates |


<!-- END GENERATED NODES -->
