# Project Io — The Digitisation Tree

> **Settles:** the nodes of the Digitisation age's tech tree — its three trees, ten branches,
> clusters, pairs, masteries and quests · every node's advantage, ring and
> requirements · which next-age branch each quest keys · which node unlocks each near-space
> thing · the traps the tree carries for Era 1.
> **Not here:** the grammar every age's tree obeys, research points and the queue (../RESEARCH) ·
> what Era 1 is and what opens the next (../ERAS) · what a recipe or building does once
> unlocked (../PRODUCTION).
> **Confused with:** ../RESEARCH.md, ../../generation/trees/TREES.md (retired),
> ../../research/ERA1_TECH_LANDSCAPE.md.

The Digitisation tree is the tech tree of the age the campaign opens in, 1960 onward (`../ERAS.md`
§ Era 1). Every nation starts holding none of it (`../RESEARCH.md` § Who holds technology). It is the
first tree cut to the grammar in `../RESEARCH.md` § The shape of an age's tree: its nodes lie in three
trees of ten branches, on four rings — each branch's common spine, the kit nodes that lengthen the
arms of Force, Processing and Signal & Orbit (§ Kit), and State's fourth branch, Naval (§ Naval). The
generated table below gives the count.

**The tree was trimmed to fewer, heavier nodes (Ben, 2026-11-19, BL-1712 TECH_TREE_TRIM).** Each
branch's six travel nodes became three at double strength; a minor that only repeated its major's
buff at a smaller size was folded into the major; kit minors on one subject became one node; the
joints and the placeholder clusters were cut. Every kit item and every space unlock survived, on
the node that absorbed it. The scored review the cut was taken from is
`../../development/drafts/tech_tree_trim_review.json`.

The store is `digitisation_tree.json`, canonical for every node; this document owns the prose and
carries the store's tables, rendered by `node tools/session/tech_tree_store.js digitisation`, which
also checks the store against the grammar (`--check` fails on a stale table or a broken rule).

**Every advantage, cost and deed here is a placeholder, to be re-priced in play (Ben, 2026-10-10).**
The shape is the design; the numbers are where it starts.

---

## The three trees

| Tree | Thesis | Branches |
|---|---|---|
| **Production** | what the nation can extract and make | Extraction · Processing · Energy |
| **Network** | how far and fast the nation moves goods and word | Signal & Orbit · Markets · Freight |
| **State** | what the nation can field, hold and keep calm | Force · Naval · Order · Accord |

Arranged in a ring, the trees sit Production, Network, State, and each branch's neighbours are the
branches beside it — including the branch across a tree boundary.

## One branch, node by node

Every branch has the same spine (`../RESEARCH.md` § Links require); the three kit-bearing branches
lengthen its arms with kit clusters (§ Kit):

| Ring | Nodes |
|---|---|
| 1 | Travel I → the first cluster's minors |
| 2 | The first cluster's major (needs its minors) → two arms, each opened by a travel node (II and III) |
| 3 | Each arm's cluster minors |
| 4 | Each arm's major (needs its minors) → the branch's **mastery** (needs both majors) |

A cluster has one minor or two (`../RESEARCH.md` § Links require). The branch's **quest** hangs off
one of its cluster majors and costs at that major's ring. A pair cluster has one minor and two
majors, of which a nation may hold one.

**Wind Power is a major (Ben, 2026-11-07), off Energy's spine.** It opens a recipe, so it costs what
a ring-2 major costs. It has no minors of its own: it needs Fuel Economy II and Fuel Economy III, the
first node of each arm, so the two paths into it are the branch's own. No minor or mastery needs it.

## Travel themes

A branch's three travel nodes all carry its theme; its mastery carries it at one and a half times
a travel node's strength. A branch walked whole gives nine points of its theme, as before the trim.

| Branch | Each travel node | Mastery |
|---|---|---|
| Extraction | +2% extraction rate at every extraction building | Resource Base, +3% |
| Processing | +2% output per batch at every processing building | Industrial Depth, +3% |
| Energy | −2% fuel draw at every building | Energy Security, −3% |
| Signal & Orbit | +2% national research | Research Culture, +3% |
| Markets | +2% trade capacity at every Marketplace | Market Depth, +3% |
| Freight | −2% logistics cost | Logistics Backbone, −3% |
| Force | +2% strength for every unit | Combined Arms, +3% |
| Naval | +2% strength for every ship | Sea Power, +3% |
| Order | −2% unrest in every province | Civic Trust, −3% |
| Accord | −2% Alarm toward you | Diplomatic Standing, −3% |

**No joints.** Nothing sits between two branches (`../RESEARCH.md` § Links require).

## The traps

The tree carries Era 1's red herrings — technologies whose cost is the Alarm they raise
(`../ERAS.md` § Mistakes compound):

| Trap | Where |
|---|---|
| **Escalator** — real defence, and every rival's Alarm rises | *Strategic Deterrent*, paired with *Survivable Forces* (Accord) |
| **Inverse herring** — looks aggressive, stabilises | *Survivable Forces* |
| **Legibility trap** — the capability is fine; being seen to hold it is the cost | *Civil Nuclear Power* (Energy), softened by *Reactor Safety* |
| **Interdependence severer** — autarky that cuts the ties holding Alarm down | *Import Substitution*, paired with *Free-Trade Accords* (Accord) |
| **Domestic destabiliser** — order now, unrest later | *Mass Surveillance*, paired with *Mass Television* (Order) |

## No placeholders

The tree reserves no room for espionage or politics (`../RESEARCH.md` § No placeholders). When either
system lands it brings its cluster: espionage on the right arms of Signal & Orbit and Force, politics
on the right arms of Markets and Order, where the reserved clusters stood.

## Quests and the next age

Each quest keys the same branch of the next age's tree, the Early Space tree
(`EARLY_SPACE_TREE.md`). It authors In-Situ Propellant, Interplanetary Convoys and Space Force; the
other six branches are reserved there. The branches, sketched by what the quests open:

| Branch | Quest | Keys |
|---|---|---|
| Extraction | Strategic Reserve | Asteroid Prospecting — iron-nickel ore, platinum group metals, regolith |
| Processing | Domestic Chips | Orbital Assembly — the Assembly Plant |
| Energy | Propellant at Scale | In-Situ Propellant — the Ice Extractor |
| Signal & Orbit | First Cargo to Orbit | Interplanetary Convoys — the outer bodies in reach |
| Markets | Open Markets | Off-World Exchange — markets on other bodies |
| Freight | Ten Lanes | Orbital Port & Depots — staging and refuelling above the home body |
| Force | Proven Readiness | Space Force — units that hold orbit |
| Naval | Blue Water | Orbital Fleet — ships that keep the lanes between bodies |
| Order | Broad Consensus | Settlement Charters — governing off-world population |
| Accord | Quiet Borders | Planetary Accords — shared limits on climate Load |

## Kit

**Military kit is researched here (Ben, 2026-10-27), one node per technology (Ben, 2026-11-03).**
A kit node opens its technology's item in every class it fits — the assault rifle for infantry,
motorised and mechanised alike — so 30 nodes open the catalogue's 90 later land and air items
(`../../military/MILITARY.md` § The kit). Twenty-four are kit nodes of their own; six existing
nodes also open kit beside their own advantage: *Armoured Carriers*, *Early Warning Radar*,
*Mechanised Divisions*, *Precision Munitions* and *Airmobile Forces* in Force, *Numerical Control*
in Processing.

**Kit lives by subject in four branches**: small arms, section weapons, missiles, guns and
vehicles in **Force**; armour materials and airframes in **Processing**; sensors, fire control,
drones and networked kit in **Signal & Orbit**; every ship's kit in **Naval**. No kit node requires a
node of another branch (Ben, 2026-11-03); the tie between them is the goods — later kit draws
electronics off the shelf.

## Naval

**State's fourth branch is Naval (Ben, 2026-11-05).** It opens ships' hulls, armament and sensors in the
three slots of `../../military/MILITARY.md` § Sea, for the four classes. It obeys the grammar every branch
does (`../RESEARCH.md` § The shape of an age's tree): travel nodes with one stacking buff, clusters of
one or two minors feeding a major, a mastery, and one quest keying the next age's branch.
No Naval kit node requires a node of another branch.

It sits between Force and Order on the ring. Its clusters follow the slots: the first opens hulls, the
second armament, the third sensors, each with one minor opening the slot's first items for the
surface classes and the submarine alike, and a major for the later rung of every class. Six nodes
open the ships' 22 later items. Its travel nodes carry one
stacking buff to every ship, and its quest's deed is keeping a sea zone through a war.

**Kit clusters lengthen the existing arms (Ben, 2026-11-03).** Each is one or two minors feeding a
major, laid in line along an arm: its minors hang off the arm node before them, and the arm's next
node hangs off its major, so the arm's rim cluster sits later. No cluster is a pair. Minors open
gen-1 and gen-2 items; a cluster's surge item sits on its major, and so does a single item that
would otherwise be a minor of its own (Ben, 2026-11-19).

| Branch | Left arm (to its rim major) | Right arm (to its rim major) | Nodes |
|---|---|---|---|
| Force | Travel II → *Anti-Tank Missiles* → *Fire-and-Forget Missiles* → *Airmobile Forces* | Travel III → *Rocket Artillery* → *Long-Range Precision Strike* → *Active Protection* | 21 |
| Processing | Travel II → *Reactive Armour* → *Semiconductor Foundry* | Travel III → *Low-Observable Airframes* → *Industrial Robotics* | 15 |
| Signal & Orbit | Travel II → *Thermal Imaging* → *Rocketry* | Travel III → *Integrated Air Defence* → *Networked Warfare* → *Loitering Munitions* | 18 |

**Artillery's kit rides the clusters it belongs to.** Self-propelled guns are *Tracked Chassis*'s and
*Active Protection*'s chassis, precision shells are *Precision Munitions*' guided shells, and
counter-battery radar rides *Rocket Artillery* (BL-1643, missile silos; a lone minor until the trim).

On Force and Signal & Orbit the longer kit runs go down the arm once reserved for espionage, so the
arm a mastery needs is the shorter: Force's mastery waits on two kit clusters, Signal & Orbit's — and
the space gate, *Rocketry* — on one. That arm's rim major is no requirement of the mastery.

**An item may open before the earlier rung of its slot**: a slot offers whatever is researched
(`../../military/MILITARY.md` § The lock rule), so a slot's order is not a research order.

**The link lives in two places.** A kit node's store entry carries an `unlock_kit` effect naming
its catalogue items, for display; the catalogue's `research_node` (`src/world/unit_designs.cpp`) is
what the lock rule reads. `tools/session/tech_tree_store.js --check` holds the two equal both ways.

**The age runs slower with its kit (Ben, 2026-11-03).** The kit raises the whole age from 1087 to
1423 cost units, about 31% more, and the RP unit is not re-tuned to absorb it. Naval raises it again to
1536, about 8%, on the same rule (Ben, 2026-11-05). The trim (Ben, 2026-11-19) takes it to 1160, about
24% less; the RP unit is not re-tuned for that either, and the first-major interval is re-measured
against the trimmed tree (`../RESEARCH.md` § Cost).

## Space unlocks

**Near space is researched here** (`../SPACE_ASSETS.md`): the launch, every satellite role, the
Astronaut Training Centre, the station core, all but one of the station modules, and the first grid
upgrade. Far space — hulls, drives, the propellant loop's buildings, the Shipyard module and the
last grid upgrade — is the next age's (`EARLY_SPACE_TREE.md`). Each unlock is a `mechanic` effect
whose key is a constant in `src/world/space_tech_keys.hpp`, and each key sits on one node.

**No node is added for space.** Each unlock rides an existing node whose subject it shares, beside
that node's own advantage: the role satellites on the technologies they carry, the modules on the
industry that makes their parts.

| Key | Unlocks | Node | Branch |
|---|---|---|---|
| `space_dispatch` | Launch; convoys to other bodies | Rocketry | Signal & Orbit |
| `space_role_comms` | The Comms satellite | Communication Satellites | Markets |
| `space_role_navigation` | The Navigation satellite | Satellite Navigation | Freight |
| `space_role_observation` | The Observation satellite | Thermal Imaging | Signal & Orbit |
| `space_role_survey` | The Survey satellite | Seismic Survey | Extraction |
| `astronaut_training` | The Astronaut Training Centre | Mass Higher Education | Signal & Orbit |
| `space_station_core` | The station core | Rocketry | Signal & Orbit |
| `space_module_research` | The Research module | Research Culture | Signal & Orbit |
| `space_module_defence` | The Defence module | Integrated Air Defence | Signal & Orbit |
| `space_module_observatory` | The Observatory module | Scanned-Array Radar | Signal & Orbit |
| `space_module_power` | The Power module | Photolithography | Processing |
| `space_module_habitation` | The Habitation module | Polymer Chemistry | Processing |
| `space_module_life_support` | The Life support module | Desalination | Extraction |
| `space_module_fuel_depot` | The Fuel depot module | Cryogenic Storage | Energy |
| `space_module_dock` | The Dock module | Gantry Cranes | Freight |
| `space_module_cargo_hold` | The Cargo hold module | Container Shipping | Freight |
| `space_module_microgravity_factory` | The Microgravity factory module | Industrial Robotics | Processing |
| `space_module_comms_relay` | The Comms relay module | Packet Networks | Markets |
| `space_module_market` | The Market module | Electronic Settlement | Markets |
| `space_grid_7x5` | The 5 × 3 → 7 × 5 station upgrade | Numerical Control | Processing |

**The station core sits on Signal & Orbit's mastery**, *Research Culture*, the one node past
Rocketry that is not a quest: a core comes after the launch, and a quest's deed would hold it
behind a deed rather than research. A launch still needs Rocketry whatever else is held
(`../SPACE_ASSETS.md` § Launching), so a role or module held before Rocketry waits on it.

## Effects

A node's **advantage** is the player-facing prose; its **effects** in the store are the same
advantage in machine form, one entry per lever (`../RESEARCH.md` § Effects no system reads yet):

| Kind | Fields | Moves |
|---|---|---|
| `modifier` | `subject`, `scope`, `op`, `value` | A named scalar (`modifier_subject`) where the scope matches: `all`, `resource:<good>`, `group:<recipe group>`, `recipe:<recipe>`, `building:<type>` or `site:<named extraction site>` (`site:farm` is the Farm, not a Fishing Wharf drawing the same good). A list of scopes is one modifier per member. `+5%` is `multiply 1.05`; a *+N% national research* share is `add 0.0N`, summed |
| `unlock_recipe` | `recipe` | Closes a registry recipe until the nation holds the node |
| `unlock_structure` | `structure` | The same, for a building type |
| `mechanic` | `key` | A named rule a system switches on |
| `unlock_kit` | `items` | The kit catalogue items the node opens (§ Kit); the catalogue reads the node |
| `unread` | `what`, `waits_on` | Nothing: the lever is a system the game does not carry; `waits_on` names it |

An effect has two states and no third: a system reads it, or it is `unread`. A modifier's sign and
size match its advantage's prose. A quest's **deed** has the same pair: a machine form, or a `deed_effect` of kind `unread`
naming the system it waits on.
Held effects fold in node order. `tools/session/tech_tree_store.js --check` holds the store's
shape; `nation_tech_harness` holds the store to what the code reads.

## Every node

<!-- BEGIN GENERATED NODES (tools/session/tech_tree_store.js) -->

**132 nodes** — 40 travel (0 joints, 10 masteries), 41 minor, 41 major, 10 quest; 0 placeholder nodes. Whole age: 1160 cost units.

### Production — what the nation can extract and make

#### Extraction (`PR-EX`)

| Id | Node | Tier | Ring | Advantage | Requires |
|---|---|---|---|---|---|
| `DG-PR-EX-T1` | Field Practice I | travel | 1 | +2% extraction rate at every extraction building | — (open) |
| `DG-PR-EX-A1` | Seismic Survey | minor | 1 | deposits in your provinces reveal 1 province further; Survey satellites may be launched | Field Practice I |
| `DG-PR-EX-A` | Offshore Drilling | major | 2 | Oil Platforms may be placed on shelf sea; Oil Platforms +25% extraction rate | Seismic Survey |
| `DG-PR-EX-T3` | Field Practice II | travel | 2 | +2% extraction rate at every extraction building | Offshore Drilling |
| `DG-PR-EX-T5` | Field Practice III | travel | 2 | +2% extraction rate at every extraction building | Offshore Drilling |
| `DG-PR-EX-B2` | Rock Bolting | minor | 3 | Mine maintenance -10% | Field Practice II |
| `DG-PR-EX-B` | Deep-Shaft Mining | major | 4 | province building ceiling +50%; Mines +10% extraction rate | Rock Bolting |
| `DG-PR-EX-C2` | Desalination | minor | 3 | coastal Water extraction sites; Water extraction +10%; station Life support modules may be fitted | Field Practice III |
| `DG-PR-EX-C` | Green Revolution | major | 4 | Farms +25% output; Chemical Plants unlock the fertiliser recipe | Desalination |
| `DG-PR-EX-M` | Resource Base | mastery | 4 | +3% extraction rate at every extraction building | Deep-Shaft Mining + Green Revolution |
| `DG-PR-EX-Q` | Strategic Reserve | quest | 2 | Mines and Oil Platforms +5% extraction rate. **Deed:** your own stock of crude oil and iron ore covers 12 months of your demand. **Keys next age:** Asteroid Prospecting | Offshore Drilling |

#### Processing (`PR-PC`)

| Id | Node | Tier | Ring | Advantage | Requires |
|---|---|---|---|---|---|
| `DG-PR-PC-T1` | Shop-Floor Practice I | travel | 1 | +2% output per batch at every processing building | — (open) |
| `DG-PR-PC-A2` | Polymer Chemistry | minor | 1 | plastics recipe inputs -10%; station Habitation modules may be fitted | Shop-Floor Practice I |
| `DG-PR-PC-A` | Bulk Petrochemicals | major | 2 | Chemical Plants unlock the plastics recipe; Fuel Refineries +20% output per batch | Polymer Chemistry |
| `DG-PR-PC-T3` | Shop-Floor Practice II | travel | 2 | +2% output per batch at every processing building | Bulk Petrochemicals |
| `DG-PR-PC-T5` | Shop-Floor Practice III | travel | 2 | +2% output per batch at every processing building | Bulk Petrochemicals |
| `DG-PR-PC-B2` | Photolithography | minor | 3 | Silicon Works +5%, Rare Earth Refineries +5% and Electronics Labs +5% output per batch; station Power modules may be fitted | Reactive Armour |
| `DG-PR-PC-B` | Semiconductor Foundry | major | 4 | Electronics Labs unlock the microchip recipe; Silicon Works +10%, Rare Earth Refineries +10% and Electronics Labs +10% output per batch | Photolithography |
| `DG-PR-PC-C1` | Numerical Control | minor | 3 | Machine Works +5%, Alloy Works +5%, Ordnance Works +5% and Spacecraft Assembly Plants +5% output per batch; opens lightweight composite tracks (armour); station grids may be upgraded to 7 x 5 | Low-Observable Airframes |
| `DG-PR-PC-C` | Industrial Robotics | major | 4 | every processing building +20% output per batch and wages -10%; station Microgravity factory modules may be fitted | Numerical Control |
| `DG-PR-PC-M` | Industrial Depth | mastery | 4 | +3% output per batch at every processing building | Semiconductor Foundry + Industrial Robotics |
| `DG-PR-PC-Q` | Domestic Chips | quest | 4 | Silicon Works +10%, Rare Earth Refineries +10% and Electronics Labs +10% output per batch. **Deed:** your microchip output covers your own electronics demand for 12 months. **Keys next age:** Orbital Assembly | Semiconductor Foundry |
| `DG-PR-PC-D1` | Composite Armour | minor | 2 | opens composite armour (armour, the light-armour applique); opens the ceramic plate carrier (infantry, motorised and mechanised armour) | Shop-Floor Practice II |
| `DG-PR-PC-D` | Reactive Armour | major | 3 | opens reactive armour (armour) | Composite Armour |
| `DG-PR-PC-E1` | Jet Airframes | minor | 2 | opens jet airframes (the twin-engine fighter, the multirole, the supersonic bomber, the high-altitude recon aircraft); opens the armoured tandem-seat helicopter (helicopter airframe) | Shop-Floor Practice III |
| `DG-PR-PC-E` | Low-Observable Airframes | major | 3 | opens low-observable airframes (fighter, strike, bomber) | Jet Airframes |

#### Energy (`PR-EN`)

| Id | Node | Tier | Ring | Advantage | Requires |
|---|---|---|---|---|---|
| `DG-PR-EN-T1` | Fuel Economy I | travel | 1 | -2% fuel draw at every building | — (open) |
| `DG-PR-EN-A1` | Pipeline Networks | minor | 1 | fuel logistics cost -10% | Fuel Economy I |
| `DG-PR-EN-A` | Catalytic Cracking | major | 2 | Refineries +20% refined fuel per batch | Pipeline Networks |
| `DG-PR-EN-T3` | Fuel Economy II | travel | 2 | -2% fuel draw at every building | Catalytic Cracking |
| `DG-PR-EN-T5` | Fuel Economy III | travel | 2 | -2% fuel draw at every building | Catalytic Cracking |
| `DG-PR-EN-B1` | Reactor Safety | minor | 3 | each Nuclear Plant raises rivals’ Alarm by 1 instead of 2 | Fuel Economy II |
| `DG-PR-EN-B2` | Uranium Enrichment | minor | 3 | Nuclear Plant output +25% | Fuel Economy II |
| `DG-PR-EN-B` | Civil Nuclear Power | major | 4 | unlocks the Nuclear Plant: powers 4 province buildings with no fuel draw. Trap: each one raises every rival’s Alarm toward you by 2 | Reactor Safety + Uranium Enrichment |
| `DG-PR-EN-C1` | Cryogenic Storage | minor | 3 | propellant holding losses -50%; station Fuel depot modules may be fitted | Fuel Economy III |
| `DG-PR-EN-C` | Propellant Synthesis | major | 4 | propellant recipes +20% yield | Cryogenic Storage |
| `DG-PR-EN-M` | Energy Security | mastery | 4 | -3% fuel draw at every building | Civil Nuclear Power + Propellant Synthesis |
| `DG-PR-EN-Q` | Propellant at Scale | quest | 4 | propellant output +15%. **Deed:** a propellant plant fully staffed for 24 months. **Keys next age:** In-Situ Propellant | Propellant Synthesis |
| `DG-PR-EN-W` | Wind Power | major | 2 | unlocks the Wind Turbine: land power with no fuel | Fuel Economy II + Fuel Economy III |

### Network — how far and fast the nation moves goods and word

#### Signal & Orbit (`NE-SO`)

| Id | Node | Tier | Ring | Advantage | Requires |
|---|---|---|---|---|---|
| `DG-NE-SO-T1` | Research Grant I | travel | 1 | +2% national research | — (open) |
| `DG-NE-SO-A2` | Mass Higher Education | minor | 1 | universities +20% research; the Astronaut Training Centre may be built | Research Grant I |
| `DG-NE-SO-A` | Mainframe Computing | major | 2 | national research +15%; rivals’ prices and stocks seen 3 months fresher | Mass Higher Education |
| `DG-NE-SO-T3` | Research Grant II | travel | 2 | +2% national research | Mainframe Computing |
| `DG-NE-SO-T5` | Research Grant III | travel | 2 | +2% national research | Mainframe Computing |
| `DG-NE-SO-B1` | Pad Ground Support | minor | 3 | Launchpad build cost -25% | Thermal Imaging |
| `DG-NE-SO-B` | Rocketry | major | 4 | the Launchpad may operate (with the staffed pad and propellant reserve); convoys may dispatch to other bodies; station cores may be launched | Pad Ground Support |
| `DG-NE-SO-M` | Research Culture | mastery | 4 | +3% national research; Research modules may be fitted | Rocketry |
| `DG-NE-SO-Q` | First Cargo to Orbit | quest | 4 | launch propellant -5%. **Deed:** one convoy completed to another body. **Keys next age:** Interplanetary Convoys | Rocketry |
| `DG-NE-SO-D1` | Airborne Radar | minor | 2 | opens airborne radar (pulse-doppler, terrain-following, side-looking) | Research Grant II |
| `DG-NE-SO-D2` | Long-Endurance Drones | minor | 2 | opens the long-endurance drone and its satellite datalink | Research Grant II |
| `DG-NE-SO-D` | Thermal Imaging | major | 3 | opens thermal sights (armour and light-armour armament, helicopter night vision, the targeting pod); Observation satellites may be launched | Airborne Radar + Long-Endurance Drones |
| `DG-NE-SO-E1` | Scanned-Array Radar | minor | 2 | opens scanned-array radar (fighter, multirole sensor fusion, helicopter mast radar); station Observatory modules may be fitted | Research Grant III |
| `DG-NE-SO-E` | Integrated Air Defence | major | 3 | opens the integrated air-defence network and the airborne early-warning aircraft; station Defence modules may be fitted; opens the jet-powered combat drone (drone airframe) | Scanned-Array Radar |
| `DG-NE-SO-G2` | Electronic Warfare | minor | 3 | opens electronic warfare (air-defence fire control, bomber countermeasures, towed decoys, the signals-intelligence suite) | Integrated Air Defence |
| `DG-NE-SO-G` | Networked Warfare | major | 4 | opens networked soldier kit and gun-launched missiles with networked fire control; opens autonomous targeting (drone avionics) | Electronic Warfare |
| `DG-NE-SO-F1` | FPV Drones | minor | 4 | opens FPV attack drones (motorised and mechanised weapon) | Networked Warfare |
| `DG-NE-SO-F` | Loitering Munitions | major | 4 | opens loitering drones (artillery munitions, drone armament); opens layered defence with counter-drone (air defence launcher) | FPV Drones |

#### Markets (`NE-MK`)

| Id | Node | Tier | Ring | Advantage | Requires |
|---|---|---|---|---|---|
| `DG-NE-MK-T1` | Trade Desk I | travel | 1 | +2% trade capacity at every Marketplace | — (open) |
| `DG-NE-MK-A2` | Consumer Credit | minor | 1 | household demand in your markets +3% | Trade Desk I |
| `DG-NE-MK-A` | Electronic Settlement | major | 2 | trades settle the tick they clear; Ports earn +25% trade points; station Market modules may be fitted | Consumer Credit |
| `DG-NE-MK-T3` | Trade Desk II | travel | 2 | +2% trade capacity at every Marketplace | Electronic Settlement |
| `DG-NE-MK-T5` | Trade Desk III | travel | 2 | +2% trade capacity at every Marketplace | Electronic Settlement |
| `DG-NE-MK-B1` | Communication Satellites | minor | 3 | activity fog lifts on every market you trade with; Comms satellites may be launched | Trade Desk II |
| `DG-NE-MK-B` | Packet Networks | major | 4 | trade capacity +30% at every Marketplace; national research +10%; station Comms relay modules may be fitted; stock holding cost and spoilage -20% | Communication Satellites |
| `DG-NE-MK-M` | Market Depth | mastery | 4 | +3% trade capacity at every Marketplace | Packet Networks |
| `DG-NE-MK-Q` | Open Markets | quest | 2 | trade points +5%. **Deed:** trade with the markets of 3 other nations in three months. **Keys next age:** Off-World Exchange | Electronic Settlement |

#### Freight (`NE-FR`)

| Id | Node | Tier | Ring | Advantage | Requires |
|---|---|---|---|---|---|
| `DG-NE-FR-T1` | Route Planning I | travel | 1 | -2% logistics cost | — (open) |
| `DG-NE-FR-A2` | Gantry Cranes | minor | 1 | Port throughput +10%; station Dock modules may be fitted | Route Planning I |
| `DG-NE-FR-A` | Container Shipping | major | 2 | convoys carry +50% cargo per dispatch; Ports upgrade to Container Ports; station Cargo hold modules may be fitted | Gantry Cranes |
| `DG-NE-FR-T3` | Route Planning II | travel | 2 | -2% logistics cost | Container Shipping |
| `DG-NE-FR-T5` | Route Planning III | travel | 2 | -2% logistics cost | Container Shipping |
| `DG-NE-FR-B1` | Asphalt Paving | minor | 3 | road maintenance -10% | Route Planning II |
| `DG-NE-FR-B` | Motorway Grid | major | 4 | road-tier upgrades cost -30%; road travel time -15%; infrastructure buildings raise the province building ceiling +50% | Asphalt Paving |
| `DG-NE-FR-C2` | Satellite Navigation | minor | 3 | travel time -5% everywhere; Navigation satellites may be launched | Route Planning III |
| `DG-NE-FR-C` | Jet Freight | major | 4 | perishables and electronics travel time -40% | Satellite Navigation |
| `DG-NE-FR-M` | Logistics Backbone | mastery | 4 | -3% logistics cost | Motorway Grid + Jet Freight |
| `DG-NE-FR-Q` | Ten Lanes | quest | 4 | logistics cost -5%. **Deed:** 10 convoy routes served every month for 12 months. **Keys next age:** Orbital Port & Depots | Motorway Grid |

### State — what the nation can field, hold and keep calm

#### Force (`ST-FO`)

| Id | Node | Tier | Ring | Advantage | Requires |
|---|---|---|---|---|---|
| `DG-ST-FO-T1` | Drill & Training I | travel | 1 | +2% strength for every unit | — (open) |
| `DG-ST-FO-A1` | Armoured Carriers | minor | 1 | land unit upkeep -5%; opens protected trucks, the truck-mounted air-defence carrier and spaced light armour | Drill & Training I |
| `DG-ST-FO-A2` | Early Warning Radar | minor | 1 | home-province defence +10%; opens search radar (air-defence fire control) | Drill & Training I |
| `DG-ST-FO-A` | Mechanised Divisions | major | 2 | unlocks mechanised infantry; every land unit +20% strength; opens the autocannon fighting vehicle and the light-armour autocannon | Armoured Carriers + Early Warning Radar |
| `DG-ST-FO-T3` | Drill & Training II | travel | 2 | +2% strength for every unit | Mechanised Divisions |
| `DG-ST-FO-T5` | Drill & Training III | travel | 2 | +2% strength for every unit | Mechanised Divisions |
| `DG-ST-FO-B2` | Precision Munitions | minor | 3 | units +10% strength when attacking; opens guided shells, laser-guided and guided bombs, and drone guided missiles; opens the radar-laid howitzer (artillery gun): counter-battery radar finds hostile artillery in range and answers it first | Fire-and-Forget Missiles |
| `DG-ST-FO-B` | Airmobile Forces | major | 4 | unit movement +50% along reach; opens the dedicated attack helicopter (helicopter airframe) | Precision Munitions |
| `DG-ST-FO-M` | Combined Arms | mastery | 4 | +3% strength for every unit | Airmobile Forces |
| `DG-ST-FO-Q` | Proven Readiness | quest | 2 | muster time -20%. **Deed:** every home province held through a war, or a war won. **Keys next age:** Space Force | Mechanised Divisions |
| `DG-ST-FO-D1` | Assault Rifles | minor | 2 | opens the assault rifle (infantry, motorised and mechanised weapon); opens the flak vest (infantry, motorised and mechanised armour) | Drill & Training II |
| `DG-ST-FO-D` | Anti-Tank Missiles | major | 2 | opens the wire-guided anti-tank missile (infantry and motorised weapon, helicopter armament) | Assault Rifles |
| `DG-ST-FO-E2` | Short-Range Missiles | minor | 2 | opens short-range missiles (air defence launcher, fighter radar-guided armament, recon self-defence) | Anti-Tank Missiles |
| `DG-ST-FO-E` | Fire-and-Forget Missiles | major | 3 | opens the fire-and-forget anti-tank missile (infantry and mechanised weapon, the missile IFV, the light-armour mount, helicopter armament) | Short-Range Missiles |
| `DG-ST-FO-G1` | Modern Gun Barrels | minor | 2 | opens the smoothbore tank gun and the long-barrel howitzer; opens the light mortar (infantry weapon) | Drill & Training III |
| `DG-ST-FO-G2` | Tracked Chassis | minor | 2 | opens tracked chassis (self-propelled artillery, the air-defence carrier, the tracked scout, hydropneumatic suspension) | Drill & Training III |
| `DG-ST-FO-G` | Rocket Artillery | major | 3 | opens the multiple rocket launcher (artillery gun) | Modern Gun Barrels + Tracked Chassis |
| `DG-ST-FO-F1` | Medium-Range Missiles | minor | 3 | opens medium-range missiles (air defence launcher, fighter beyond-visual-range armament) | Rocket Artillery |
| `DG-ST-FO-F` | Long-Range Precision Strike | major | 4 | opens guided long-range rockets, precision stand-off weapons and air-launched cruise missiles; opens anti-armour submunitions (artillery munitions) | Medium-Range Missiles |
| `DG-ST-FO-H1` | Wheeled Carriers | minor | 4 | opens wheeled carriers (motorised vehicle, the run-flat six-wheeler) | Long-Range Precision Strike |
| `DG-ST-FO-H` | Active Protection | major | 4 | opens active protection (armour, the heavy fighting vehicle); opens the armoured self-propelled chassis (artillery tow) | Wheeled Carriers |

#### Naval (`ST-NV`)

| Id | Node | Tier | Ring | Advantage | Requires |
|---|---|---|---|---|---|
| `DG-ST-NV-T1` | Seamanship I | travel | 1 | +2% strength for every ship | — (open) |
| `DG-ST-NV-A1` | Gas-Turbine Propulsion | minor | 1 | opens gas-turbine hull, compartmented hull, roll-on assault hull; opens streamlined snorkel hull | Seamanship I |
| `DG-ST-NV-A` | Reactor Propulsion | major | 2 | opens reactor pressure hull, quieted composite hull, reactor capital hull, fast sealift hull | Gas-Turbine Propulsion |
| `DG-ST-NV-T3` | Seamanship II | travel | 2 | +2% strength for every ship | Reactor Propulsion |
| `DG-ST-NV-T5` | Seamanship III | travel | 2 | +2% strength for every ship | Reactor Propulsion |
| `DG-ST-NV-B1` | Naval Fire Control | minor | 3 | opens anti-submarine rockets, radar-laid main battery; opens homing torpedoes | Seamanship II |
| `DG-ST-NV-B` | Anti-Ship Missiles | major | 4 | opens wire-guided hunter torpedoes, anti-ship missiles, vertical-launch missile cells | Naval Fire Control |
| `DG-ST-NV-C1` | Search Radar | minor | 3 | opens variable-depth sonar, surface search radar, warning radar; opens passive sonar array | Seamanship III |
| `DG-ST-NV-C` | Fleet Data Link | major | 4 | opens towed sonar array, towed array and data link, phased-array radar, convoy data link | Search Radar |
| `DG-ST-NV-M` | Sea Power | mastery | 4 | +3% strength for every ship | Anti-Ship Missiles + Fleet Data Link |
| `DG-ST-NV-Q` | Blue Water | quest | 2 | ship muster time -20%. **Deed:** a fleet keeps a sea zone through a war. **Keys next age:** Orbital Fleet | Reactor Propulsion |

#### Order (`ST-OR`)

| Id | Node | Tier | Ring | Advantage | Requires |
|---|---|---|---|---|---|
| `DG-ST-OR-T1` | Civil Service I | travel | 1 | -2% unrest in every province | — (open) |
| `DG-ST-OR-A2` | Social Insurance | minor | 1 | tax yield +3% | Civil Service I |
| `DG-ST-OR-A` | Welfare State | major | 2 | unrest -10% in every province; wage floor +5% (a cost) | Social Insurance |
| `DG-ST-OR-T3` | Civil Service II | travel | 2 | -2% unrest in every province | Welfare State |
| `DG-ST-OR-T5` | Civil Service III | travel | 2 | -2% unrest in every province | Welfare State |
| `DG-ST-OR-B1` | Opinion Polling | minor | 3 | unrest forecast 6 months ahead | Civil Service II |
| `DG-ST-OR-Ba` | Mass Television | major (pair) | 4 | tax yield +10%; unrest -15%; assimilation +20% | Opinion Polling |
| `DG-ST-OR-Bb` | Mass Surveillance | major (pair) | 4 | unrest -30% now, but the unrest floor rises 5 points a year. Destabiliser | Opinion Polling |
| `DG-ST-OR-M` | Civic Trust | mastery | 4 | -3% unrest in every province | Mass Television or Mass Surveillance |
| `DG-ST-OR-Q` | Broad Consensus | quest | 4 | tax yield +5%. **Deed:** unrest below the floor in every province for 24 months. **Keys next age:** Settlement Charters | Mass Television or Mass Surveillance |

#### Accord (`ST-AC`)

| Id | Node | Tier | Ring | Advantage | Requires |
|---|---|---|---|---|---|
| `DG-ST-AC-T1` | Diplomatic Corps I | travel | 1 | -2% Alarm toward you | — (open) |
| `DG-ST-AC-A1` | Trade Missions | minor | 1 | sentiment +5 with every nation you trade with | Diplomatic Corps I |
| `DG-ST-AC-Aa` | Free-Trade Accords | major (pair) | 2 | tariffs with accord partners fall to 0; their Alarm toward you -10 | Trade Missions |
| `DG-ST-AC-Ab` | Import Substitution | major (pair) | 2 | tariffs +20%; domestic processing +10%. Severer: rivals’ Alarm +5, export routes thin | Trade Missions |
| `DG-ST-AC-T3` | Diplomatic Corps II | travel | 2 | -2% Alarm toward you | Free-Trade Accords or Import Substitution |
| `DG-ST-AC-T5` | Diplomatic Corps III | travel | 2 | -2% Alarm toward you | Free-Trade Accords or Import Substitution |
| `DG-ST-AC-B1` | Hotline | minor | 3 | Alarm spikes decay 20% faster | Diplomatic Corps II |
| `DG-ST-AC-Ba` | Strategic Deterrent | major (pair) | 4 | attackers’ odds -50% on your home provinces. Escalator: every rival’s Alarm toward you +10 | Hotline |
| `DG-ST-AC-Bb` | Survivable Forces | major (pair) | 4 | units +25% defence; Alarm toward you drifts -5% a year. Looks aggressive, stabilises | Hotline |
| `DG-ST-AC-C2` | Confidence Measures | minor | 3 | Alarm toward you -5% | Diplomatic Corps III |
| `DG-ST-AC-C` | Arms Control Verification | major | 4 | every nation’s Alarm toward you -25%; every nation’s Alarm toward every other decays 10% faster | Confidence Measures |
| `DG-ST-AC-M` | Diplomatic Standing | mastery | 4 | -3% Alarm toward you | Strategic Deterrent or Survivable Forces + Arms Control Verification |
| `DG-ST-AC-Q` | Quiet Borders | quest | 2 | Alarm toward you decays 10% faster. **Deed:** no nation’s Alarm toward you rises for 24 months. **Keys next age:** Planetary Accords | Free-Trade Accords or Import Substitution |


<!-- END GENERATED NODES -->
