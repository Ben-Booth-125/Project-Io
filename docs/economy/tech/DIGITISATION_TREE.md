# Project Io — The Digitisation Tree

> **Settles:** the nodes of the Digitisation age's tech tree — its three trees, ten branches,
> clusters, pairs, joints, masteries, quests and placeholders · every node's advantage, ring and
> requirements · which next-age branch each quest keys · the traps the tree carries for Era 1.
> **Not here:** the grammar every age's tree obeys, research points and the queue (../RESEARCH) ·
> what Era 1 is and what opens the next (../ERAS) · what a recipe or building does once
> unlocked (../PRODUCTION).
> **Confused with:** ../RESEARCH.md, ../../generation/trees/TREES.md (retired),
> ../../research/ERA1_TECH_LANDSCAPE.md.

The Digitisation tree is the tech tree of the age the campaign opens in, 1960 onward (`../ERAS.md`
§ Era 1). Every nation starts holding none of it (`../RESEARCH.md` § Who holds technology). It is the
first tree cut to the grammar in `../RESEARCH.md` § The shape of an age's tree: 211 nodes in three
trees of ten branches, on four rings — 160 in the nine original branches' common shape, 33 kit nodes
that lengthen the arms of Force, Processing and Signal & Orbit (§ Kit), and 18 for State's fourth
branch, Naval, and its joint (§ Naval).

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

Every branch has the same seventeen-node spine (`../RESEARCH.md` § Links require); the three
kit-bearing branches lengthen its arms with kit clusters (§ Kit):

| Ring | Nodes |
|---|---|
| 1 | Travel I → Travel II → the first cluster's two minors |
| 2 | The first cluster's major (needs both minors) → two arms, Travel III and Travel V |
| 3 | Travel IV and Travel VI → each arm's cluster minors |
| 4 | Each arm's major (needs both its minors) → the branch's **mastery** (needs both majors) |

The branch's **quest** hangs off one of its three majors and costs at that major's ring. A pair
cluster has one minor and two majors, of which a nation may hold one.

## Travel themes

A branch's six travel nodes all carry its theme; its mastery carries the theme at triple strength.

| Branch | Each travel node | Mastery |
|---|---|---|
| Extraction | +1% extraction rate at every extraction building | Resource Base, +3% |
| Processing | +1% output per batch at every processing building | Industrial Depth, +3% |
| Energy | −1% fuel draw at every building | Energy Security, −3% |
| Signal & Orbit | +1% national research | Research Culture, +3% |
| Markets | +1% trade capacity at every Marketplace | Market Depth, +3% |
| Freight | −1% logistics cost | Logistics Backbone, −3% |
| Force | +1% strength for every unit | Combined Arms, +3% |
| Naval | +1% strength for every ship | Sea Power, +3% |
| Order | −1% unrest in every province | Civic Trust, −3% |
| Accord | −1% Alarm toward you | Diplomatic Standing, −3% |

**Eight joints** sit where neighbouring branches meet, each needing the facing arm of both. Two
boundaries carry no joint — Markets | Freight and Force | Naval — each beside a placeholder cluster;
the espionage and politics systems place their joints there when they land.

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

## Placeholders

| Cluster | Held for |
|---|---|
| Signal & Orbit, right arm | Espionage — signals intelligence |
| Force, right arm | Espionage — covert operations |
| Markets, right arm | Politics — market regulation (lobbying retired, Ben 2026-10-23) |
| Order, right arm | Politics — regime, elections, succession |

## Quests and the next age

Each quest keys the same branch of the next age's tree. The next age is unnamed; its branches are
sketched by what the quests open:

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
motorised and mechanised alike — so 39 nodes open the catalogue's 89 later items
(`../../military/MILITARY.md` § The kit). Thirty-three are kit nodes of their own; six existing
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
two minors feeding a major, a mastery, one quest keying the next age's branch, and a joint on the ring.
No Naval kit node requires a node of another branch.

It sits between Force and Order on the ring. Its clusters follow the slots: the first opens hulls, the
second armament, the third sensors, each with a minor for the surface classes, a minor for the
submarine and a major for the later rung of every class. Nine nodes open the ships' 22 later items.
Its joint, *Coastal Security*, needs Naval's and Order's facing arms. Its travel nodes carry one
stacking buff to every ship, and its quest's deed is keeping a sea zone through a war.

**Kit clusters lengthen the existing arms (Ben, 2026-11-03).** Each is two minors feeding a major,
laid in line along an arm: its minors hang off the arm node before them, and the arm's next node
hangs off its major, so the arm's rim cluster sits later. Each major keeps its two minors; no
cluster is a pair. Minors open gen-1 and gen-2 items; a cluster's surge item sits on its major.

| Branch | Left arm (to its rim major) | Right arm (to its rim major) | Nodes |
|---|---|---|---|
| Force | Travel III → *Anti-Tank Missiles* → *Fire-and-Forget Missiles* → Travel IV → *Airmobile Forces* | Travel V → *Rocket Artillery* → Travel VI → *Long-Range Precision Strike* → *Active Protection* → the espionage cluster | 32 |
| Processing | Travel III → *Reactive Armour* → Travel IV → *Semiconductor Foundry* | Travel V → *Low-Observable Airframes* → Travel VI → *Industrial Robotics* | 23 |
| Signal & Orbit | Travel III → *Thermal Imaging* → Travel IV → *Rocketry* | Travel V → *Integrated Air Defence* → Travel VI → *Networked Warfare* → *Loitering Munitions* → the espionage cluster | 29 |

Where a branch carries a placeholder arm, the longer kit runs go down it, so the arm a mastery
needs today is the shorter: Force's mastery waits on two kit clusters, Signal & Orbit's — and
the space gate, *Rocketry* — on one.

**An item may open before the earlier rung of its slot**: a slot offers whatever is researched
(`../../military/MILITARY.md` § The lock rule), so a slot's order is not a research order.

**The link lives in two places.** A kit node's store entry carries an `unlock_kit` effect naming
its catalogue items, for display; the catalogue's `research_node` (`src/world/unit_designs.cpp`) is
what the lock rule reads. `tools/session/tech_tree_store.js --check` holds the two equal both ways.

**The age runs slower with its kit (Ben, 2026-11-03).** The kit raises the whole age from 1087 to
1423 cost units, about 31% more, and the RP unit is not re-tuned to absorb it. Naval raises it again to
1536, about 8%, on the same rule (Ben, 2026-11-05).

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
naming the system it waits on. A placeholder carries one `unread` effect waiting on its system.
Held effects fold in node order. `tools/session/tech_tree_store.js --check` holds the store's
shape; `nation_tech_harness` holds the store to what the code reads.

## Every node

<!-- BEGIN GENERATED NODES (tools/session/tech_tree_store.js) -->

**211 nodes** — 78 travel (8 joints, 10 masteries), 79 minor, 44 major, 10 quest; 12 placeholder nodes. Whole age: 1536 cost units.

### Production — what the nation can extract and make

#### Extraction (`PR-EX`)

| Id | Node | Tier | Ring | Advantage | Requires |
|---|---|---|---|---|---|
| `DG-PR-EX-T1` | Field Practice I | travel | 1 | +1% extraction rate at every extraction building | — (open) |
| `DG-PR-EX-T2` | Field Practice II | travel | 1 | +1% extraction rate at every extraction building | Field Practice I |
| `DG-PR-EX-A1` | Seismic Survey | minor | 1 | deposits in your provinces reveal 1 province further | Field Practice II |
| `DG-PR-EX-A2` | Drill-Bit Metallurgy | minor | 1 | Oil Platforms +5% extraction rate | Field Practice II |
| `DG-PR-EX-A` | Offshore Drilling | major | 2 | Oil Platforms may be placed on shelf sea; Oil Platforms +25% extraction rate | Seismic Survey + Drill-Bit Metallurgy |
| `DG-PR-EX-T3` | Field Practice III | travel | 2 | +1% extraction rate at every extraction building | Offshore Drilling |
| `DG-PR-EX-T4` | Field Practice IV | travel | 3 | +1% extraction rate at every extraction building | Field Practice III |
| `DG-PR-EX-T5` | Field Practice V | travel | 2 | +1% extraction rate at every extraction building | Offshore Drilling |
| `DG-PR-EX-T6` | Field Practice VI | travel | 3 | +1% extraction rate at every extraction building | Field Practice V |
| `DG-PR-EX-B1` | Mine Ventilation | minor | 3 | Mines +5% extraction rate | Field Practice IV |
| `DG-PR-EX-B2` | Rock Bolting | minor | 3 | Mine maintenance -10% | Field Practice IV |
| `DG-PR-EX-B` | Deep-Shaft Mining | major | 4 | province building ceiling +1 for extraction buildings; Mines +10% extraction rate | Mine Ventilation + Rock Bolting |
| `DG-PR-EX-C1` | Hybrid Seed | minor | 3 | Farms +5% output | Field Practice VI |
| `DG-PR-EX-C2` | Desalination | minor | 3 | coastal Water extraction sites; Water extraction +10% | Field Practice VI |
| `DG-PR-EX-C` | Green Revolution | major | 4 | Farms +25% output; Chemical Plants unlock the fertiliser recipe | Hybrid Seed + Desalination |
| `DG-PR-EX-M` | Resource Base | mastery | 4 | +3% extraction rate at every extraction building | Deep-Shaft Mining + Green Revolution |
| `DG-PR-EX-Q` | Strategic Reserve | quest | 2 | Mines and Oil Platforms +5% extraction rate. **Deed:** your own stock of crude oil and iron ore covers 4 quarters of your demand. **Keys next age:** Asteroid Prospecting | Offshore Drilling |

#### Processing (`PR-PC`)

| Id | Node | Tier | Ring | Advantage | Requires |
|---|---|---|---|---|---|
| `DG-PR-PC-T1` | Shop-Floor Practice I | travel | 1 | +1% output per batch at every processing building | — (open) |
| `DG-PR-PC-T2` | Shop-Floor Practice II | travel | 1 | +1% output per batch at every processing building | Shop-Floor Practice I |
| `DG-PR-PC-A1` | Catalysts | minor | 1 | Chemical Plants +5% output per batch | Shop-Floor Practice II |
| `DG-PR-PC-A2` | Polymer Chemistry | minor | 1 | plastics recipe inputs -10% | Shop-Floor Practice II |
| `DG-PR-PC-A` | Bulk Petrochemicals | major | 2 | Chemical Plants unlock the plastics recipe; Refineries +20% output per batch | Catalysts + Polymer Chemistry |
| `DG-PR-PC-T3` | Shop-Floor Practice III | travel | 2 | +1% output per batch at every processing building | Bulk Petrochemicals |
| `DG-PR-PC-T4` | Shop-Floor Practice IV | travel | 3 | +1% output per batch at every processing building | Reactive Armour |
| `DG-PR-PC-T5` | Shop-Floor Practice V | travel | 2 | +1% output per batch at every processing building | Bulk Petrochemicals |
| `DG-PR-PC-T6` | Shop-Floor Practice VI | travel | 3 | +1% output per batch at every processing building | Low-Observable Airframes |
| `DG-PR-PC-B1` | Clean Rooms | minor | 3 | microchip recipe +10% yield | Shop-Floor Practice IV |
| `DG-PR-PC-B2` | Photolithography | minor | 3 | Electronics Labs +5% output per batch | Shop-Floor Practice IV |
| `DG-PR-PC-B` | Semiconductor Foundry | major | 4 | Electronics Labs unlock the microchip recipe; Electronics Labs +10% output per batch | Clean Rooms + Photolithography |
| `DG-PR-PC-C1` | Numerical Control | minor | 3 | Fabricators +5% output per batch; opens lightweight composite tracks (armour) | Shop-Floor Practice VI |
| `DG-PR-PC-C2` | Process Automation | minor | 3 | wages in processing buildings -5% | Shop-Floor Practice VI |
| `DG-PR-PC-C` | Industrial Robotics | major | 4 | every processing building +20% output per batch and wages -10% | Numerical Control + Process Automation |
| `DG-PR-PC-M` | Industrial Depth | mastery | 4 | +3% output per batch at every processing building | Semiconductor Foundry + Industrial Robotics |
| `DG-PR-PC-Q` | Domestic Chips | quest | 4 | Electronics Labs +10% output per batch. **Deed:** your microchip output covers your own electronics demand for 4 quarters. **Keys next age:** Orbital Assembly | Semiconductor Foundry |
| `DG-PR-PC-D1` | Composite Armour | minor | 2 | opens composite armour (armour, the light-armour applique) | Shop-Floor Practice III |
| `DG-PR-PC-D2` | Ceramic Plates | minor | 2 | opens the ceramic plate carrier (infantry, motorised and mechanised armour) | Shop-Floor Practice III |
| `DG-PR-PC-D` | Reactive Armour | major | 3 | opens reactive armour (armour) | Composite Armour + Ceramic Plates |
| `DG-PR-PC-E1` | Jet Airframes | minor | 2 | opens jet airframes (the twin-engine fighter, the multirole, the supersonic bomber, the high-altitude recon aircraft) | Shop-Floor Practice V |
| `DG-PR-PC-E2` | Armoured Helicopters | minor | 2 | opens the armoured tandem-seat helicopter (helicopter airframe) | Shop-Floor Practice V |
| `DG-PR-PC-E` | Low-Observable Airframes | major | 3 | opens low-observable airframes (fighter, strike, bomber) | Jet Airframes + Armoured Helicopters |

#### Energy (`PR-EN`)

| Id | Node | Tier | Ring | Advantage | Requires |
|---|---|---|---|---|---|
| `DG-PR-EN-T1` | Fuel Economy I | travel | 1 | -1% fuel draw at every building | — (open) |
| `DG-PR-EN-T2` | Fuel Economy II | travel | 1 | -1% fuel draw at every building | Fuel Economy I |
| `DG-PR-EN-A1` | Pipeline Networks | minor | 1 | fuel logistics cost -10% | Fuel Economy II |
| `DG-PR-EN-A2` | Heat Recovery | minor | 1 | fuel draw of processing buildings -5% | Fuel Economy II |
| `DG-PR-EN-A` | Catalytic Cracking | major | 2 | Refineries +20% refined fuel per batch | Pipeline Networks + Heat Recovery |
| `DG-PR-EN-T3` | Fuel Economy III | travel | 2 | -1% fuel draw at every building | Catalytic Cracking |
| `DG-PR-EN-T4` | Fuel Economy IV | travel | 3 | -1% fuel draw at every building | Fuel Economy III |
| `DG-PR-EN-T5` | Fuel Economy V | travel | 2 | -1% fuel draw at every building | Catalytic Cracking |
| `DG-PR-EN-T6` | Fuel Economy VI | travel | 3 | -1% fuel draw at every building | Fuel Economy V |
| `DG-PR-EN-B1` | Reactor Safety | minor | 3 | each Nuclear Plant raises rivals’ Alarm by 1 instead of 2 | Fuel Economy IV |
| `DG-PR-EN-B2` | Uranium Enrichment | minor | 3 | Nuclear Plant output +25% | Fuel Economy IV |
| `DG-PR-EN-B` | Civil Nuclear Power | major | 4 | unlocks the Nuclear Plant: powers 4 province buildings with no fuel draw. Trap: each one raises every rival’s Alarm toward you by 2 | Reactor Safety + Uranium Enrichment |
| `DG-PR-EN-C1` | Cryogenic Storage | minor | 3 | propellant holding losses -50% | Fuel Economy VI |
| `DG-PR-EN-C2` | Rocket-Fuel Chemistry | minor | 3 | propellant recipes +5% yield | Fuel Economy VI |
| `DG-PR-EN-C` | Propellant Synthesis | major | 4 | propellant recipes +20% yield | Cryogenic Storage + Rocket-Fuel Chemistry |
| `DG-PR-EN-M` | Energy Security | mastery | 4 | -3% fuel draw at every building | Civil Nuclear Power + Propellant Synthesis |
| `DG-PR-EN-Q` | Propellant at Scale | quest | 4 | propellant output +15%. **Deed:** a propellant plant fully staffed for 8 quarters. **Keys next age:** In-Situ Propellant | Propellant Synthesis |

### Network — how far and fast the nation moves goods and word

#### Signal & Orbit (`NE-SO`)

| Id | Node | Tier | Ring | Advantage | Requires |
|---|---|---|---|---|---|
| `DG-NE-SO-T1` | Research Grant I | travel | 1 | +1% national research | — (open) |
| `DG-NE-SO-T2` | Research Grant II | travel | 1 | +1% national research | Research Grant I |
| `DG-NE-SO-A1` | Telex Network | minor | 1 | rivals’ prices and stocks seen 1 quarter fresher | Research Grant II |
| `DG-NE-SO-A2` | Mass Higher Education | minor | 1 | universities +20% research | Research Grant II |
| `DG-NE-SO-A` | Mainframe Computing | major | 2 | national research +15% | Telex Network + Mass Higher Education |
| `DG-NE-SO-T3` | Research Grant III | travel | 2 | +1% national research | Mainframe Computing |
| `DG-NE-SO-T4` | Research Grant IV | travel | 3 | +1% national research | Thermal Imaging |
| `DG-NE-SO-T5` | Research Grant V | travel | 2 | +1% national research | Mainframe Computing |
| `DG-NE-SO-T6` | Research Grant VI | travel | 3 | +1% national research | Integrated Air Defence |
| `DG-NE-SO-B1` | Pad Ground Support | minor | 3 | Launchpad build cost -25% | Research Grant IV |
| `DG-NE-SO-B2` | Guidance Systems | minor | 3 | launch propellant -5% | Research Grant IV |
| `DG-NE-SO-B` | Rocketry | major | 4 | the Launchpad may operate (with the staffed pad and propellant reserve); convoys may dispatch to other bodies | Pad Ground Support + Guidance Systems |
| `DG-NE-SO-C1` | Reserved minor | minor (placeholder) | 4 | reserved for espionage: signals intelligence | Loitering Munitions |
| `DG-NE-SO-C2` | Reserved minor | minor (placeholder) | 4 | reserved for espionage: signals intelligence | Loitering Munitions |
| `DG-NE-SO-C` | [Espionage] | major (placeholder) | 4 | placeholder cluster: signals intelligence; effects authored when the espionage system exists | Reserved minor + Reserved minor |
| `DG-NE-SO-M` | Research Culture | mastery | 4 | +3% national research | Rocketry |
| `DG-NE-SO-Q` | First Cargo to Orbit | quest | 4 | launch propellant -5%. **Deed:** one convoy completed to another body. **Keys next age:** Interplanetary Convoys | Rocketry |
| `DG-NE-SO-D1` | Airborne Radar | minor | 2 | opens airborne radar (pulse-doppler, terrain-following, side-looking) | Research Grant III |
| `DG-NE-SO-D2` | Long-Endurance Drones | minor | 2 | opens the long-endurance drone and its satellite datalink | Research Grant III |
| `DG-NE-SO-D` | Thermal Imaging | major | 3 | opens thermal sights (armour and light-armour armament, helicopter night vision, the targeting pod) | Airborne Radar + Long-Endurance Drones |
| `DG-NE-SO-E1` | Scanned-Array Radar | minor | 2 | opens scanned-array radar (fighter, multirole sensor fusion, helicopter mast radar) | Research Grant V |
| `DG-NE-SO-E2` | Jet Combat Drones | minor | 2 | opens the jet-powered combat drone (drone airframe) | Research Grant V |
| `DG-NE-SO-E` | Integrated Air Defence | major | 3 | opens the integrated air-defence network and the airborne early-warning aircraft | Scanned-Array Radar + Jet Combat Drones |
| `DG-NE-SO-G1` | Autonomous Targeting | minor | 3 | opens autonomous targeting (drone avionics) | Research Grant VI |
| `DG-NE-SO-G2` | Electronic Warfare | minor | 3 | opens electronic warfare (air-defence fire control, bomber countermeasures, towed decoys, the signals-intelligence suite) | Research Grant VI |
| `DG-NE-SO-G` | Networked Warfare | major | 4 | opens networked soldier kit and gun-launched missiles with networked fire control | Autonomous Targeting + Electronic Warfare |
| `DG-NE-SO-F1` | FPV Drones | minor | 4 | opens FPV attack drones (motorised and mechanised weapon) | Networked Warfare |
| `DG-NE-SO-F2` | Counter-Drone Defence | minor | 4 | opens layered defence with counter-drone (air defence launcher) | Networked Warfare |
| `DG-NE-SO-F` | Loitering Munitions | major | 4 | opens loitering drones (artillery munitions, drone armament) | FPV Drones + Counter-Drone Defence |

#### Markets (`NE-MK`)

| Id | Node | Tier | Ring | Advantage | Requires |
|---|---|---|---|---|---|
| `DG-NE-MK-T1` | Trade Desk I | travel | 1 | +1% trade capacity at every Marketplace | — (open) |
| `DG-NE-MK-T2` | Trade Desk II | travel | 1 | +1% trade capacity at every Marketplace | Trade Desk I |
| `DG-NE-MK-A1` | Commodity Futures | minor | 1 | trade capacity +5% at your Marketplaces | Trade Desk II |
| `DG-NE-MK-A2` | Consumer Credit | minor | 1 | household demand in your markets +3% | Trade Desk II |
| `DG-NE-MK-A` | Electronic Settlement | major | 2 | trades settle the tick they clear; Ports earn +25% trade points | Commodity Futures + Consumer Credit |
| `DG-NE-MK-T3` | Trade Desk III | travel | 2 | +1% trade capacity at every Marketplace | Electronic Settlement |
| `DG-NE-MK-T4` | Trade Desk IV | travel | 3 | +1% trade capacity at every Marketplace | Trade Desk III |
| `DG-NE-MK-T5` | Trade Desk V | travel | 2 | +1% trade capacity at every Marketplace | Electronic Settlement |
| `DG-NE-MK-T6` | Trade Desk VI | travel | 3 | +1% trade capacity at every Marketplace | Trade Desk V |
| `DG-NE-MK-B1` | Communication Satellites | minor | 3 | activity fog lifts on every market you trade with | Trade Desk IV |
| `DG-NE-MK-B2` | Just-in-Time Inventory | minor | 3 | stock holding cost and spoilage -20% | Trade Desk IV |
| `DG-NE-MK-B` | Packet Networks | major | 4 | trade capacity +30% at every Marketplace; national research +10% | Communication Satellites + Just-in-Time Inventory |
| `DG-NE-MK-C1` | Reserved minor | minor (placeholder) | 3 | reserved for politics: lobbying and market regulation | Trade Desk VI |
| `DG-NE-MK-C2` | Reserved minor | minor (placeholder) | 3 | reserved for politics: lobbying and market regulation | Trade Desk VI |
| `DG-NE-MK-C` | [Politics] | major (placeholder) | 4 | placeholder cluster: lobbying and market regulation; effects authored when the politics system exists | Reserved minor + Reserved minor |
| `DG-NE-MK-M` | Market Depth | mastery | 4 | +3% trade capacity at every Marketplace | Packet Networks |
| `DG-NE-MK-Q` | Open Markets | quest | 2 | trade points +5%. **Deed:** trade with the markets of 3 other nations in one quarter. **Keys next age:** Off-World Exchange | Electronic Settlement |

#### Freight (`NE-FR`)

| Id | Node | Tier | Ring | Advantage | Requires |
|---|---|---|---|---|---|
| `DG-NE-FR-T1` | Route Planning I | travel | 1 | -1% logistics cost | — (open) |
| `DG-NE-FR-T2` | Route Planning II | travel | 1 | -1% logistics cost | Route Planning I |
| `DG-NE-FR-A1` | Pallet Standards | minor | 1 | handling cost -5% | Route Planning II |
| `DG-NE-FR-A2` | Gantry Cranes | minor | 1 | Port throughput +10% | Route Planning II |
| `DG-NE-FR-A` | Container Shipping | major | 2 | convoys carry +50% cargo per dispatch; Ports upgrade to Container Ports | Pallet Standards + Gantry Cranes |
| `DG-NE-FR-T3` | Route Planning III | travel | 2 | -1% logistics cost | Container Shipping |
| `DG-NE-FR-T4` | Route Planning IV | travel | 3 | -1% logistics cost | Route Planning III |
| `DG-NE-FR-T5` | Route Planning V | travel | 2 | -1% logistics cost | Container Shipping |
| `DG-NE-FR-T6` | Route Planning VI | travel | 3 | -1% logistics cost | Route Planning V |
| `DG-NE-FR-B1` | Asphalt Paving | minor | 3 | road maintenance -10% | Route Planning IV |
| `DG-NE-FR-B2` | Trucking Fleets | minor | 3 | inland convoy capacity +10% | Route Planning IV |
| `DG-NE-FR-B` | Motorway Grid | major | 4 | road-tier upgrades cost -30%; road travel time -15% | Asphalt Paving + Trucking Fleets |
| `DG-NE-FR-C1` | Air Cargo Hubs | minor | 3 | high-value convoy capacity +10% | Route Planning VI |
| `DG-NE-FR-C2` | Satellite Navigation | minor | 3 | travel time -5% everywhere | Route Planning VI |
| `DG-NE-FR-C` | Jet Freight | major | 4 | perishables and electronics travel time -40% | Air Cargo Hubs + Satellite Navigation |
| `DG-NE-FR-M` | Logistics Backbone | mastery | 4 | -3% logistics cost | Motorway Grid + Jet Freight |
| `DG-NE-FR-Q` | Ten Lanes | quest | 4 | logistics cost -5%. **Deed:** 10 convoy routes served every quarter for 4 quarters. **Keys next age:** Orbital Port & Depots | Motorway Grid |

### State — what the nation can field, hold and keep calm

#### Force (`ST-FO`)

| Id | Node | Tier | Ring | Advantage | Requires |
|---|---|---|---|---|---|
| `DG-ST-FO-T1` | Drill & Training I | travel | 1 | +1% strength for every unit | — (open) |
| `DG-ST-FO-T2` | Drill & Training II | travel | 1 | +1% strength for every unit | Drill & Training I |
| `DG-ST-FO-A1` | Armoured Carriers | minor | 1 | land unit upkeep -5%; opens protected trucks, the truck-mounted air-defence carrier and spaced light armour | Drill & Training II |
| `DG-ST-FO-A2` | Early Warning Radar | minor | 1 | home-province defence +10%; opens search radar (air-defence fire control) | Drill & Training II |
| `DG-ST-FO-A` | Mechanised Divisions | major | 2 | unlocks mechanised infantry; every land unit +20% strength; opens the autocannon fighting vehicle and the light-armour autocannon | Armoured Carriers + Early Warning Radar |
| `DG-ST-FO-T3` | Drill & Training III | travel | 2 | +1% strength for every unit | Mechanised Divisions |
| `DG-ST-FO-T4` | Drill & Training IV | travel | 3 | +1% strength for every unit | Fire-and-Forget Missiles |
| `DG-ST-FO-T5` | Drill & Training V | travel | 2 | +1% strength for every unit | Mechanised Divisions |
| `DG-ST-FO-T6` | Drill & Training VI | travel | 3 | +1% strength for every unit | Rocket Artillery |
| `DG-ST-FO-B1` | Helicopter Lift | minor | 3 | muster time -10% | Drill & Training IV |
| `DG-ST-FO-B2` | Precision Munitions | minor | 3 | units +10% strength when attacking; opens guided shells, laser-guided and guided bombs, and drone guided missiles | Drill & Training IV |
| `DG-ST-FO-B` | Airmobile Forces | major | 4 | unit movement +50% along reach; opens the dedicated attack helicopter (helicopter airframe) | Helicopter Lift + Precision Munitions |
| `DG-ST-FO-C1` | Reserved minor | minor (placeholder) | 4 | reserved for espionage: covert operations | Active Protection |
| `DG-ST-FO-C2` | Reserved minor | minor (placeholder) | 4 | reserved for espionage: covert operations | Active Protection |
| `DG-ST-FO-C` | [Espionage] | major (placeholder) | 4 | placeholder cluster: covert operations; effects authored when the espionage system exists | Reserved minor + Reserved minor |
| `DG-ST-FO-M` | Combined Arms | mastery | 4 | +3% strength for every unit | Airmobile Forces |
| `DG-ST-FO-Q` | Proven Readiness | quest | 2 | muster time -20%. **Deed:** every home province held through a war, or a war won. **Keys next age:** Space Force | Mechanised Divisions |
| `DG-ST-FO-D1` | Assault Rifles | minor | 2 | opens the assault rifle (infantry, motorised and mechanised weapon) | Drill & Training III |
| `DG-ST-FO-D2` | Body Armour | minor | 2 | opens the flak vest (infantry, motorised and mechanised armour) | Drill & Training III |
| `DG-ST-FO-D` | Anti-Tank Missiles | major | 2 | opens the wire-guided anti-tank missile (infantry and motorised weapon, helicopter armament) | Assault Rifles + Body Armour |
| `DG-ST-FO-E1` | Light Mortars | minor | 2 | opens the light mortar (infantry weapon) | Anti-Tank Missiles |
| `DG-ST-FO-E2` | Short-Range Missiles | minor | 2 | opens short-range missiles (air defence launcher, fighter radar-guided armament, recon self-defence) | Anti-Tank Missiles |
| `DG-ST-FO-E` | Fire-and-Forget Missiles | major | 3 | opens the fire-and-forget anti-tank missile (infantry and mechanised weapon, the missile IFV, the light-armour mount, helicopter armament) | Light Mortars + Short-Range Missiles |
| `DG-ST-FO-G1` | Modern Gun Barrels | minor | 2 | opens the smoothbore tank gun and the long-barrel howitzer | Drill & Training V |
| `DG-ST-FO-G2` | Tracked Chassis | minor | 2 | opens tracked chassis (self-propelled artillery, the air-defence carrier, the tracked scout, hydropneumatic suspension) | Drill & Training V |
| `DG-ST-FO-G` | Rocket Artillery | major | 3 | opens the multiple rocket launcher (artillery gun) | Modern Gun Barrels + Tracked Chassis |
| `DG-ST-FO-F1` | Medium-Range Missiles | minor | 3 | opens medium-range missiles (air defence launcher, fighter beyond-visual-range armament) | Drill & Training VI |
| `DG-ST-FO-F2` | Submunitions | minor | 3 | opens anti-armour submunitions (artillery munitions) | Drill & Training VI |
| `DG-ST-FO-F` | Long-Range Precision Strike | major | 4 | opens guided long-range rockets, precision stand-off weapons and air-launched cruise missiles | Medium-Range Missiles + Submunitions |
| `DG-ST-FO-H1` | Wheeled Carriers | minor | 4 | opens wheeled carriers (motorised vehicle, the run-flat six-wheeler) | Long-Range Precision Strike |
| `DG-ST-FO-H2` | Armoured Self-Propelled Guns | minor | 4 | opens the armoured self-propelled chassis (artillery tow) | Long-Range Precision Strike |
| `DG-ST-FO-H` | Active Protection | major | 4 | opens active protection (armour, the heavy fighting vehicle) | Wheeled Carriers + Armoured Self-Propelled Guns |

#### Naval (`ST-NV`)

| Id | Node | Tier | Ring | Advantage | Requires |
|---|---|---|---|---|---|
| `DG-ST-NV-T1` | Seamanship I | travel | 1 | +1% strength for every ship | — (open) |
| `DG-ST-NV-T2` | Seamanship II | travel | 1 | +1% strength for every ship | Seamanship I |
| `DG-ST-NV-A1` | Gas-Turbine Propulsion | minor | 1 | opens gas-turbine hull, compartmented hull, roll-on assault hull | Seamanship II |
| `DG-ST-NV-A2` | Streamlined Hulls | minor | 1 | opens streamlined snorkel hull | Seamanship II |
| `DG-ST-NV-A` | Reactor Propulsion | major | 2 | opens reactor pressure hull, quieted composite hull, reactor capital hull, fast sealift hull | Gas-Turbine Propulsion + Streamlined Hulls |
| `DG-ST-NV-T3` | Seamanship III | travel | 2 | +1% strength for every ship | Reactor Propulsion |
| `DG-ST-NV-T4` | Seamanship IV | travel | 3 | +1% strength for every ship | Seamanship III |
| `DG-ST-NV-T5` | Seamanship V | travel | 2 | +1% strength for every ship | Reactor Propulsion |
| `DG-ST-NV-T6` | Seamanship VI | travel | 3 | +1% strength for every ship | Seamanship V |
| `DG-ST-NV-B1` | Naval Fire Control | minor | 3 | opens anti-submarine rockets, radar-laid main battery | Seamanship IV |
| `DG-ST-NV-B2` | Homing Torpedoes | minor | 3 | opens homing torpedoes | Seamanship IV |
| `DG-ST-NV-B` | Anti-Ship Missiles | major | 4 | opens wire-guided hunter torpedoes, anti-ship missiles, vertical-launch missile cells | Naval Fire Control + Homing Torpedoes |
| `DG-ST-NV-C1` | Search Radar | minor | 3 | opens variable-depth sonar, surface search radar, warning radar | Seamanship VI |
| `DG-ST-NV-C2` | Passive Sonar | minor | 3 | opens passive sonar array | Seamanship VI |
| `DG-ST-NV-C` | Fleet Data Link | major | 4 | opens towed sonar array, towed array and data link, phased-array radar, convoy data link | Search Radar + Passive Sonar |
| `DG-ST-NV-M` | Sea Power | mastery | 4 | +3% strength for every ship | Anti-Ship Missiles + Fleet Data Link |
| `DG-ST-NV-Q` | Blue Water | quest | 2 | ship muster time -20%. **Deed:** a fleet keeps a sea zone through a war. **Keys next age:** Orbital Fleet | Reactor Propulsion |

#### Order (`ST-OR`)

| Id | Node | Tier | Ring | Advantage | Requires |
|---|---|---|---|---|---|
| `DG-ST-OR-T1` | Civil Service I | travel | 1 | -1% unrest in every province | — (open) |
| `DG-ST-OR-T2` | Civil Service II | travel | 1 | -1% unrest in every province | Civil Service I |
| `DG-ST-OR-A1` | Public Housing | minor | 1 | unrest -3% in every province | Civil Service II |
| `DG-ST-OR-A2` | Social Insurance | minor | 1 | tax yield +3% | Civil Service II |
| `DG-ST-OR-A` | Welfare State | major | 2 | unrest -10% in every province; wage floor +5% (a cost) | Public Housing + Social Insurance |
| `DG-ST-OR-T3` | Civil Service III | travel | 2 | -1% unrest in every province | Welfare State |
| `DG-ST-OR-T4` | Civil Service IV | travel | 3 | -1% unrest in every province | Civil Service III |
| `DG-ST-OR-T5` | Civil Service V | travel | 2 | -1% unrest in every province | Welfare State |
| `DG-ST-OR-T6` | Civil Service VI | travel | 3 | -1% unrest in every province | Civil Service V |
| `DG-ST-OR-B1` | Opinion Polling | minor | 3 | unrest forecast 2 quarters ahead | Civil Service IV |
| `DG-ST-OR-Ba` | Mass Television | major (pair) | 4 | tax yield +10%; unrest -15%; assimilation +20% | Opinion Polling |
| `DG-ST-OR-Bb` | Mass Surveillance | major (pair) | 4 | unrest -30% now, but the unrest floor rises 5 points a year. Destabiliser | Opinion Polling |
| `DG-ST-OR-C1` | Reserved minor | minor (placeholder) | 3 | reserved for politics: regime, elections, succession | Civil Service VI |
| `DG-ST-OR-C2` | Reserved minor | minor (placeholder) | 3 | reserved for politics: regime, elections, succession | Civil Service VI |
| `DG-ST-OR-C` | [Politics] | major (placeholder) | 4 | placeholder cluster: regime, elections, succession; effects authored when the politics system exists | Reserved minor + Reserved minor |
| `DG-ST-OR-M` | Civic Trust | mastery | 4 | -3% unrest in every province | Mass Television or Mass Surveillance |
| `DG-ST-OR-Q` | Broad Consensus | quest | 4 | tax yield +5%. **Deed:** unrest below the floor in every province for 8 quarters. **Keys next age:** Settlement Charters | Mass Television or Mass Surveillance |

#### Accord (`ST-AC`)

| Id | Node | Tier | Ring | Advantage | Requires |
|---|---|---|---|---|---|
| `DG-ST-AC-T1` | Diplomatic Corps I | travel | 1 | -1% Alarm toward you | — (open) |
| `DG-ST-AC-T2` | Diplomatic Corps II | travel | 1 | -1% Alarm toward you | Diplomatic Corps I |
| `DG-ST-AC-A1` | Trade Missions | minor | 1 | sentiment +5 with every nation you trade with | Diplomatic Corps II |
| `DG-ST-AC-Aa` | Free-Trade Accords | major (pair) | 2 | tariffs with accord partners fall to 0; their Alarm toward you -10 | Trade Missions |
| `DG-ST-AC-Ab` | Import Substitution | major (pair) | 2 | tariffs +20%; domestic processing +10%. Severer: rivals’ Alarm +5, export routes thin | Trade Missions |
| `DG-ST-AC-T3` | Diplomatic Corps III | travel | 2 | -1% Alarm toward you | Free-Trade Accords or Import Substitution |
| `DG-ST-AC-T4` | Diplomatic Corps IV | travel | 3 | -1% Alarm toward you | Diplomatic Corps III |
| `DG-ST-AC-T5` | Diplomatic Corps V | travel | 2 | -1% Alarm toward you | Free-Trade Accords or Import Substitution |
| `DG-ST-AC-T6` | Diplomatic Corps VI | travel | 3 | -1% Alarm toward you | Diplomatic Corps V |
| `DG-ST-AC-B1` | Hotline | minor | 3 | Alarm spikes decay 20% faster | Diplomatic Corps IV |
| `DG-ST-AC-Ba` | Strategic Deterrent | major (pair) | 4 | attackers’ odds -50% on your home provinces. Escalator: every rival’s Alarm toward you +10 | Hotline |
| `DG-ST-AC-Bb` | Survivable Forces | major (pair) | 4 | units +25% defence; Alarm toward you drifts -5% a year. Looks aggressive, stabilises | Hotline |
| `DG-ST-AC-C1` | Inspection Regimes | minor | 3 | Alarm raised by your visible capability -10% | Diplomatic Corps VI |
| `DG-ST-AC-C2` | Confidence Measures | minor | 3 | Alarm toward you -5% | Diplomatic Corps VI |
| `DG-ST-AC-C` | Arms Control Verification | major | 4 | every nation’s Alarm toward you -25%; every nation’s Alarm toward every other decays 10% faster | Inspection Regimes + Confidence Measures |
| `DG-ST-AC-M` | Diplomatic Standing | mastery | 4 | -3% Alarm toward you | Strategic Deterrent or Survivable Forces + Arms Control Verification |
| `DG-ST-AC-Q` | Quiet Borders | quest | 2 | Alarm toward you decays 10% faster. **Deed:** no nation’s Alarm toward you rises for 8 quarters. **Keys next age:** Planetary Accords | Free-Trade Accords or Import Substitution |

### Joints

| Id | Node | Ring | Advantage | Requires |
|---|---|---|---|---|
| `DG-J-PREX-PRPC` | Integrated Mills | 3 | +1% extraction rate and +1% processing output | Field Practice VI + Shop-Floor Practice IV |
| `DG-J-PRPC-PREN` | Cogeneration | 3 | +1% processing output and -1% fuel draw | Shop-Floor Practice VI + Fuel Economy IV |
| `DG-J-PREN-NESO` | Launch Logistics | 3 | +2% propellant output and +1% national research | Fuel Economy VI + Research Grant IV |
| `DG-J-NESO-NEMK` | Market Data | 3 | +1% national research and +1% trade capacity | Research Grant VI + Trade Desk IV |
| `DG-J-NEFR-STFO` | Military Logistics | 3 | +3% unit movement and -1% logistics cost | Route Planning VI + Drill & Training IV |
| `DG-J-STOR-STAC` | Public Diplomacy | 3 | -1% unrest and -1% Alarm toward you | Civil Service VI + Diplomatic Corps IV |
| `DG-J-STAC-PREX` | Resource Diplomacy | 3 | +1% extraction rate and -1% Alarm toward you | Diplomatic Corps VI + Field Practice IV |
| `DG-J-STNV-STOR` | Coastal Security | 3 | +1% strength for every ship and -1% unrest | Seamanship VI + Civil Service IV |

<!-- END GENERATED NODES -->
