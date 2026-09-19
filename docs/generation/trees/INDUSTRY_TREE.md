# Project Io — The Industry Tree

> **Settles:** the nodes of the Industry tree — the spire that is fire, its five branches and
> its three milestones · the three forks and what each side trades away · every node's effects
> and the situation in which the scorer buys it · the terms this phase adds to the shared
> scorer · what a polity's held set hands the 1960 campaign.
> **Not here:** the grammar every tree obeys — kinds, rings, the five rules, diffusion, the
> scorer's shape, the JSON schema (TREES) · the phase this tree runs in and what crosses each
> handoff (../GENERATION_STRATEGY) · what a works row does once unlocked (../../lore/HISTORY) ·
> the unit roster and its band boundary (../MILITARY_HISTORY) · the other two trees
> (COLONISATION_TREE, EMPIRE_TREE) · the exploration age's own tree, which now holds the
> maritime and chartering subjects this tree used to (EXPLORATION_TREE) · the 1960+
> campaign tree (../../research/ERA1_TECH_LANDSCAPE).
> **Confused with:** ../../research/ANCIENT_TECH_LADDER.md, ../../research/TECH_EFFECTS.md,
> ../../economy/RESEARCH.md, ../CORPORATION_GENERATION.md.

The Industry tree is the last of the four trees `TREES.md` defines, and it runs inside
**digitisation, 1660 → 1960** (`../DIGITISATION.md`) — not the whole economy pass. The pass split
in two on 2026-09-11 and the exploration half took its own tree (`EXPLORATION_TREE.md`), so
this tree follows Exploration's, never Empire's, and opens at its root to **every living polity at
1660** (`TREES.md` § Milestones, and how a tree unlocks the next). It is
**invested**: the Era −1 sim's polities are the actors, and the Invest verb takes one node a
round, chosen by the scorer.

**This tree is three rings, not four (Ben, 2026-09-11, BL-938).** What was ring 1 — the pit, the
toolroom, the fuel trade, the ocean hull, the joint-stock venture, the press — was the exploration
age wearing an Industry-tree costume, because no Exploration tree existed when this one was first
authored. `EXPLORATION_TREE.md` now owns that ground. The maritime and chartering subjects that
duplicated it outright (a full-rigged ship, celestial navigation, a joint-stock company, chartered
capital) are gone from here entirely — Exploration already prices them. The rest of old ring 1 did
not survive the ring's removal either, except the one piece the tree cannot lint without: Furnace
Practice, the Fuel Doctrine fork's shared minor, which now opens the Metal branch at the new ring 1.
Rings 2–4 shifted down to 1–3 wholesale, so **every id below changed** (an id carries its ring) and
**ring 4's content — Electrification, Internal Combustion & Oil, Flight, Automotive Transport,
Broadcast, Antibiotics & Mass Vaccination, Synthetic Chemistry, Fixed-Nitrogen Synthesis, Tractor &
Fertiliser — survives whole as the new ring 3**, because that is what makes 1960 look like 1960.

The store is `industry_tree.json`; this document is the authority and the store transcribes its
tables. `node tools/session/tree_lint.js industry` holds the two together and enforces the five
rules.

---

## Aims

**Why nations are unequal in 1960 is manufactured here.** A polity enters this tree already
holding whatever `EXPLORATION_TREE.md` gave it — the charter, the road ladder, the ocean reach or
the defended coast. Inside this tree the fuel gate opens gaps of three rings in a century, and
every object below is either a gate that binds or a fork that locks in. The rim hands off to the
campaign: a polity's held set becomes its nation's capacity vector, and the gaps between held
sets are the 1960 spread.

**Capacity gates bind here as nowhere else.** A `fuel` gate on the spire above its root is the
mechanism of the great divergence — a polity with no seam under held ground climbs past the engine
only by trade, which is slower and can be cut. That is the one place the map, rather than the
scorer, decides who industrialises.

**The root is open to every polity (Ben, 2026-09-18).** Every living polity enters this tree at
its root in 1660 (`TREES.md` § Milestones), and the engine carries no gate. So a polity with no seam
still enters, and answers the fuel question with Charcoal Iron, the route that accepts the smelting
ceiling. Without a seam it cannot take Railway, and so cannot reach The Cheap Ton. Furnace
Practice hangs off the root, which is what makes both sides of the Fuel Doctrine reachable.

**The gate reads a seam, not an average.** PROPOSED (listed to Ben 2026-09-18, not overturned):
`fuel` passes when any held region's fuel clears the bar. A mean over held ground fails a large
realm that holds one coalfield, and *no seam under held ground* is a claim about any region. That
is the GATE only; the scorer's pull is a share, not a best (§ The scorer, Ben 2026-09-19, NR-896).

---

## The spire — fire

The spire is Energy: the engine that pumps, the engine that moves, the grid. Every polity climbs
it, every branch leaves it at a major, and its three milestones are the tree's only gates
(`TREES.md` § Milestones, and how a tree unlocks the next).

Ring 1 is the engine at rest, ring 2 the engine that moves, ring 3 the grid. Internal Combustion is
not on the spire; it is Movement's ring-3 major, because oil displaces coal at the margin and never
replaces the grid.

| id | node | kind | ring | links | gate | diffusion | effect | pursued when |
|---|---|---|---|---|---|---|---|---|
| IN-SP-1a | Atmospheric & Rotative Steam | major | 1 | IN-SP-1m | — | capacity | modifier industrial +150; upgrade Water Mill | `spire` — ring 1 has just opened and the fuel trade is running; the engine is the one node every ring-1 branch is adjacent to |
| IN-SP-1m | The Cheap Ton · requires IN-MV-1a; Fuel Doctrine taken | milestone | 1 | IN-SP-1a IN-SP-2a | — | capacity | open ring 2 | `spire` — the smelting doctrine is taken and a rail head is built; the ton moves, and ring 2 is the only place relief remains |
| IN-SP-2a | High-Pressure & Compound Engines | major | 2 | IN-SP-2m | fuel | capacity | modifier industrial +120; modifier reach +60 | `reach_bound` — holdings sit past free_holdings along a rail and a coast; the engine that moves is what administers the same breadth cheaper |
| IN-SP-2m | The Scheduled World · requires IN-MV-2a IN-MV-2b; Works Doctrine taken | milestone | 2 | IN-SP-2a IN-SP-3a | — | capacity | open ring 3 | `spire` — steamship and wire are held and the heavy plant has an owner; arrival is a printed promise |
| IN-SP-3a | Electrification | major | 3 | IN-SP-3m | fuel | capacity | modifier industrial +200; modifier carrying_capacity +80 | `furnace_lit` — the furnace is lit and stores are not the constraint; grid power is the buy that compounds |
| IN-SP-3m | The Renewed Line · requires IN-MT-3a IN-MV-3a IN-HL-3a | milestone | 3 | IN-SP-3a | — | capacity | open campaign tree | `spire` — what the polity fields it can also replace — the shell, the engine and the soldier; the rim is the last relief left |

**Two ring-1 majors hang off the root directly** — Machine Tools (Metal) and Railway (Movement).
Land reaches the spire through Machine Tools by way of Iron Implements; Charter reaches it through
Railway by way of Bond Market; Health reaches it only at the rim, through Coal-Tar Distillates into
Synthetic Chemistry — the demographic transition is the branch nothing else waits on.

---

## Metal

What a nation smelts with, and whether metal is counted by the piece or weighed by the ton. The
Fuel Doctrine fork sits at ring 1 off Furnace Practice, and The Cheap Ton requires it taken.

| id | node | kind | ring | links | gate | diffusion | effect | pursued when |
|---|---|---|---|---|---|---|---|---|
| IN-MT-1e | Furnace Practice | minor | 1 | IN-SP-1a IN-MT-1a IN-MT-1b | — | practice | modifier industrial +30 | `ground_ore` — ore is held and the blast furnace is worked; the fuel question is next |
| IN-MT-1a | Coke Smelting · excludes IN-MT-1b | major | 1 | IN-MT-1e IN-MT-2a IN-MV-1b | fuel | capacity | unlock Blast Works; modifier industrial +120; retire the charcoal route | `ground_fuel` — coal seams under held ground and the forest thin; scale is the endowment's answer |
| IN-MT-1b | Charcoal Iron · excludes IN-MT-1a | major | 1 | IN-MT-1e | — | capacity | modifier industrial +60; upgrade Ore Pits; modifier defence +40 | `ground_forest` — forest on held ground and no seam worth a pit; quality is the endowment's answer and the ceiling is accepted |
| IN-MT-1c | Machine Tools | major | 1 | IN-MT-1d IN-SP-1a | ore_q | capacity | modifier industrial +100; modifier research +40 | `tariff_pressure` — a neighbour's machine-made goods land cheaper than the polity's own at its market; the toolroom closes the gap a tariff only covers |
| IN-MT-1d | Interchangeable Parts | minor | 1 | IN-MT-2a | — | practice | modifier muster_cost -40 | `threatened` — a levy is being raised and every musket is fitted by hand |
| IN-MT-2a | Converter Steel | major | 2 | IN-MT-1d IN-MT-2b IN-MT-3a IN-SP-2a | ore_q | capacity | modifier industrial +150; unlock Arsenal; unlock unit roster: industrial | `ground_ore` — ore and coke are both held and the rail head wants rails that do not wear; steel by the ton |
| IN-MT-2b | Framed Construction & Cement | major | 2 | IN-MT-2a | — | capacity | modifier carrying_capacity +80; access steeper landforms | `food_bound` — the industrial city is at its walls; the frame lets it climb rather than sprawl |
| IN-MT-3a | Synthetic Chemistry | major | 3 | IN-MT-2a IN-MT-3b IN-MT-3c | fuel | capacity | modifier industrial +100; modifier research +60 | `furnace_lit` — the furnace is lit, coal-tar is a waste stream, and the polity imports every dye, drug and explosive it uses |
| IN-MT-3b | Fixed-Nitrogen Synthesis | major | 3 | IN-MT-3a | fuel | capacity | modifier carrying_capacity +150; resource fixed nitrogen as a bulk good | `food_bound` — the fertiliser cargo arrives by sea and the sea is not safe; the food ceiling is lifted from the air |
| IN-MT-3c | Coal-Tar Distillates | minor | 3 | IN-HL-3a IN-MT-3a | — | practice | modifier plague +30 | `plague_struck` — a drawdown is running and the chemistry is held; the first drugs are a by-product |

**The charcoal road to steel is longer by design.** Converter Steel links Coke Smelting directly
but reaches a Charcoal polity only through Interchangeable Parts, so the quality choice costs a
ring of travel before the ton is cheap. Nothing forbids it; it is behind.

---

## Movement

The collapse of interior distance, sea legs beyond what Exploration already opened, and the wire.
Everything this branch once held below the fuel gate — the ocean hull, celestial navigation, the
coastal coal trade — is `EXPLORATION_TREE.md`'s now; this branch starts at the rail.

| id | node | kind | ring | links | gate | diffusion | effect | pursued when |
|---|---|---|---|---|---|---|---|---|
| IN-MV-1a | Railway | major | 1 | IN-MV-1b IN-MV-2b IN-SP-1a | fuel | capacity | unlock Rail Head; reach interior haul cost; modifier reach +120 | `reach_bound` — inland holdings past free_holdings and a seat that cannot supply them by road |
| IN-MV-1b | Iron Rails | minor | 1 | — | — | practice | modifier reach +30 | `ground_ore` — coke iron is cheap and the wagonways are still wooden |
| IN-MV-1c | Packet Service | minor | 1 | IN-MV-2a | — | practice | modifier reach +30 | `coastal_holdings` — regular sailings between held ports; the schedule precedes the engine |
| IN-MV-2a | Steamship | major | 2 | IN-MV-1c IN-MV-3a IN-SP-2a | coastal | capacity | unlock Coaling Station; access scheduled ocean leg; modifier reach +100 | `colonial_reach` — holdings across water where the season, not the enemy, decides when they are supplied |
| IN-MV-2b | Telegraph | major | 2 | IN-MV-1a | — | capacity | unlock Signal Line; intel activity-fog freshness; modifier cohesion +60 | `cohesion_low` — a realm whose far provinces hear of a decision a season late; the wire is cohesion at range |
| IN-MV-3a | Internal Combustion & Oil | major | 3 | IN-MV-2a IN-MV-3b IN-MV-3c IN-SP-3a | fuel | capacity | modifier industrial +100; resource oil as a bulk fuel | `fuel_bound` — coal landed at the seat runs short of what its engines burn; oil seeps on held ground are the second fuel |
| IN-MV-3b | Automotive Transport | major | 3 | IN-MV-3a | — | artifact | modifier reach +80; modifier forage +40 | `reach_bound` — holdings off the rail and a road net that only carts use |
| IN-MV-3c | Flight | major | 3 | IN-MV-3a | — | capacity | intel aerial reconnaissance; modifier defence +60; unlock unit roster: machine age | `threatened` — a neighbour with a grudge holds the engine and the polity cannot see past its own border |

**Steamship is a return trip, not a first crossing.** A polity arrives at this tree already holding
`EXPLORATION_TREE.md`'s Hulls fork (a carrack or a fighting fleet); Steamship is what a polity
which crossed with a carrack builds next, and Telegraph hangs off Railway because rail signalling
is what drives the net.

---

## Land

The food ceiling, and what a nation does to its own countryside to fill the works. The Labour
Doctrine fork sits at ring 1 off Field Survey; neither milestone requires it, so a polity may reach
1960 without ever having asked the tenure question.

| id | node | kind | ring | links | gate | diffusion | effect | pursued when |
|---|---|---|---|---|---|---|---|---|
| IN-LD-1a | Field Survey | minor | 1 | IN-LD-1b IN-LD-1c IN-LD-1d | — | practice | modifier carrying_capacity +20 | `food_bound` — the fields are mapped and the tenure question is asked |
| IN-LD-1b | Cleared Holdings · excludes IN-LD-1c | major | 1 | IN-LD-1a | arable | practice | modifier manpower +120; modifier industrial +80; modifier cohesion -60 | `labour_bound` — the works want hands and the fields hold them; the land is consolidated and the labour released in a generation |
| IN-LD-1c | Smallholder Tenure · excludes IN-LD-1b | major | 1 | IN-LD-1a | arable | practice | modifier cohesion +60; modifier stores +80; modifier industrial -40 | `cohesion_low` — the seat's cohesion is low and the fields are the one thing the people hold; the land stays divided |
| IN-LD-1d | Farm Mechanisation | major | 1 | IN-LD-1a IN-LD-1e IN-LD-2a | arable | capacity | modifier manpower +100; modifier carrying_capacity +60 | `manpower_bound` — the manpower ceiling is hit and most of the population is still in the fields |
| IN-LD-1e | Iron Implements | minor | 1 | IN-MT-1c | — | practice | modifier carrying_capacity +30 | `ground_farm` — a toolroom and farm ground in the same polity |
| IN-LD-2a | Soil Chemistry & Fertiliser Trade | major | 2 | IN-LD-2b IN-LD-3a | — | artifact | resource fertiliser as a traded good; modifier carrying_capacity +100 | `food_bound` — yield bounded by the local nitrogen cycle and a port that can land cargo |
| IN-LD-2b | Fertiliser Cargo | minor | 2 | IN-MV-2a | — | practice | modifier carrying_capacity +30 | `ground_port` — a steamship route with a fertiliser source at the far end |
| IN-LD-3a | Tractor & Fertiliser Package | major | 3 | IN-LD-3b | arable | capacity | modifier manpower +150; modifier carrying_capacity +100 | `manpower_bound` — the ceiling is hit again with a tenth of the people still on the land |
| IN-LD-3b | Fuel & Fertiliser Depot | minor | 3 | IN-MT-3b IN-MV-3a | — | practice | modifier forage +30 | `ground_farm` — oil, fixed nitrogen and farm ground on the same road |

**Land is the branch the demography reads.** Every major here moves `carrying_capacity` or
`manpower`, and the fork moves `cohesion` in opposite directions — the human price of the
divergence made into a choice rather than an assumption. **Field Survey is the branch's only route
to the root** — through Farm Mechanisation, Iron Implements and Machine Tools onto the spire —
since the crop-package and convertible-husbandry subjects that once carried Land's own root link
migrated out with the rest of old ring 1.

---

## Charter

Who may own, promise and read — the terms a corporation is chartered on. One fork lives here, Works
Doctrine, at ring 2 off Plant Registry, and The Scheduled World requires it taken. The joint-stock
venture, chartered capital and the press that once rooted this branch are gone: the first two
duplicated `EXPLORATION_TREE.md`'s Chartered Company outright, and the rest did not survive the
ring's removal — a polity arrives at this tree already holding Exploration's charter.

| id | node | kind | ring | links | gate | diffusion | effect | pursued when |
|---|---|---|---|---|---|---|---|---|
| IN-CH-1a | Empirical Method | major | 1 | IN-CH-1b IN-CH-2b IN-HL-1a | — | practice | modifier research +100 | `surplus` — nothing binds; the research rate is what compounds |
| IN-CH-1b | Patent Grants | major | 1 | — | — | practice | modifier research +60; institution property in invention | `known` — a neighbour invents and the polity copies; the grant is what makes its own toolrooms invent |
| IN-CH-1c | Bond Market | minor | 1 | IN-CH-2a IN-MV-1a | — | practice | modifier stores +30 | `credit_bound` — a rail head or a fleet priced beyond a round's surplus |
| IN-CH-2a | General Incorporation | major | 2 | IN-CH-2c | — | practice | institution chartered corporate form by registration, not by grant | `surplus` — stores comfortable, public credit held, and more ventures asking for a charter than the sovereign grants in a year |
| IN-CH-2b | Mass Schooling | major | 2 | IN-CH-3a | — | practice | modifier research +80; modifier assimilation +80; modifier industrial +40 | `many_peoples` — held ground whose shares still charge the holder; the school is the assimilation machine |
| IN-CH-2c | Plant Registry | minor | 2 | IN-CH-2d IN-CH-2e IN-MT-2a | — | practice | modifier industrial +20 | `furnace_lit` — the furnace is lit and the question of who owns it is asked |
| IN-CH-2d | State Arsenal · excludes IN-CH-2e | major | 2 | — | — | capacity | institution heavy plant nation-owned; corporations lease; modifier industrial +120; modifier muster_cost -40 | `threatened` — a neighbour with a grudge lit its furnace first; the plant must arrive fast and answer to the seat |
| IN-CH-2e | Private Works · excludes IN-CH-2d | major | 2 | — | — | capacity | institution corporations own processing capacity; modifier industrial +60; modifier research +60; modifier stores +40 | `surplus` — no grudge at the border and the counting houses hold the credit; the plant is chartered and compounds |
| IN-CH-3a | Broadcast | major | 3 | IN-SP-3a | — | capacity | modifier cohesion +100; modifier assimilation +60 | `cohesion_low` — a realm spanning water whose provinces hear the seat only by wire |

**Works Doctrine is the branch's one live question now.** Sovereign Doctrine — courts binding the
sovereign against crown monopoly — went out with the rest of old ring 1's Charter content; a 1960
nation's credit regime is now `EXPLORATION_TREE.md`'s Purse spire to answer, not this tree's.

---

## Health

The demographic transition: mortality falling ahead of wealth. A sparse branch, whose every node is
bought under `plague_struck`, that reaches the spire only at the rim.

| id | node | kind | ring | links | gate | diffusion | effect | pursued when |
|---|---|---|---|---|---|---|---|---|
| IN-HL-1a | Microscopy | minor | 1 | IN-HL-2a | — | practice | modifier plague +20 | `plague_struck` — the method is held and the lens shows the cause |
| IN-HL-2a | Germ Theory & Sanitation | major | 2 | IN-HL-3a | — | practice | modifier plague +150; modifier carrying_capacity +60 | `plague_struck` — the industrial city outgrows its wells; mortality falls ahead of wealth |
| IN-HL-3a | Antibiotics & Mass Vaccination | major | 3 | IN-MT-3c | — | artifact | modifier plague +200; modifier manpower +60 | `plague_struck` — the chemistry is held and the wards are full; the transition completes |

**The rim requires it.** Antibiotics is one of The Renewed Line's three requirements, because a
nation that can replace what it fields must replace the soldier as well as the shell. **Preventive
Inoculation, the branch's old ring-1 root, is gone** — Microscopy now reaches the spire only through
Coal-Tar Distillates into Synthetic Chemistry, at the rim, which is why Health is the branch a
median polity is least likely to finish.

---

## Forks

Three, each two majors off one shared minor at the same ring, each side naming the other in
`excludes` (`TREES.md` § Forks). Taking one closes the other permanently and it goes dark under the
fog. **Sovereign Doctrine is retired** — its Chartered Capital side duplicated Exploration's
Chartered Company and is gone, so Command Estate lost its only fork partner and is gone with it; a
1960 nation's credit regime now reads off `EXPLORATION_TREE.md` instead.

| fork | shared minor | side | trades |
|---|---|---|---|
| **Fuel Doctrine** (Metal, ring 1) | IN-MT-1e Furnace Practice | IN-MT-1a Coke Smelting | scale and the Blast Works; wants a coalfield, and the charcoal route retires |
| | | IN-MT-1b Charcoal Iron | quality and better arms; wants forest, and accepts the smelting ceiling |
| **Labour Doctrine** (Land, ring 1) | IN-LD-1a Field Survey | IN-LD-1b Cleared Holdings | an urban workforce in a generation; cohesion falls and the countryside hollows |
| | | IN-LD-1c Smallholder Tenure | a deeper domestic market and cohesion; industrialisation starts late |
| **Works Doctrine** (Charter, ring 2) | IN-CH-2c Plant Registry | IN-CH-2d State Arsenal | the heavy plant arrives fast and answers to the seat; corporations lease what they cannot own |
| | | IN-CH-2e Private Works | a slower start that compounds; corporations own capacity outright |

Works sits under a milestone and is therefore unavoidable — The Scheduled World requires it taken.
Fuel is likewise required, by The Cheap Ton. Either side satisfies its milestone; the choice is
never dictated.

---

## Milestones

One per ring, on the spire, named for the regime beyond it. Each requires majors from at least two
branches at its own ring; where a fork is listed, either side satisfies it.

| id | milestone | ring | thesis | requires |
|---|---|---|---|---|
| IN-SP-1m | **The Cheap Ton** | 1 | metal stops being counted by the piece and starts being weighed by the ton | Fuel Doctrine taken (IN-MT-1a ⊘ IN-MT-1b) · IN-MV-1a Railway |
| IN-SP-2m | **The Scheduled World** | 2 | arrival stops being a matter of season and wind and becomes a printed promise | IN-MV-2a Steamship · IN-MV-2b Telegraph · Works Doctrine taken (IN-CH-2d ⊘ IN-CH-2e) |
| IN-SP-3m | **The Renewed Line** | 3 | what a nation fields it can also replace, from its own works and not by purchase abroad | IN-MT-3a Synthetic Chemistry · IN-MV-3a Internal Combustion & Oil · IN-HL-3a Antibiotics & Mass Vaccination |

The Cheap Ton opens the moment the smelting doctrine and the rail head are both buildable — the
tree's first relief. The Renewed Line opens the campaign tree; a polity that never reaches it
enters 1960 with what it holds, never excluded, only behind.

---

## The scorer

The shape is `TREES.md` § The scorer — one shape, four trees; this tree adds no term to the
formula, only readings. Every reading below is an in-world quantity with a visible cause, never a
rank and never a quantity that grows with the polity's own size.

**The shared core.** `reach_bound`, `manpower_bound`, `food_bound`, `stores_low`, `cohesion_low`,
`threatened`, `plague_struck`, `ground_ore`, `ground_farm`, `ground_fuel`, `ground_port`,
`ground_grass`, `many_peoples`, `coastal_holdings`, `spire`, `known`, `surplus` — the vocabulary
the Empire tree shares, each a reading of polity or region state the sim already carries.

**Added by this phase.**

| term | the reading | its visible cause |
|---|---|---|
| `furnace_lit` | the polity holds Coke Smelting — a coal-fired furnace (Ben, 2026-09-18, wave 1 form; a Fuel Doctrine side held read 1000 at every pick that reads the term, since all three nodes sit behind The Cheap Ton) | a lit engine house on the map; the furnace crossing already recorded |
| `colonial_reach` | at least one held region is reachable from the seat only across a sea leg — read as the seat's line to it crossing sea, the same test that makes a campaign a sea leg | a hull on the route; the tie the campaign reads as a colonial preferred-seller |
| `tariff_pressure` | at a market the polity's holdings trade through, a neighbour's landed price for a good the polity itself makes is below its own | the price field on the route; the reading that becomes the nation's tariff posture |
| `fuel_bound` | fuel demanded by held works and engines exceeds fuel landed at the seat this round — read as the seat market's unmet energy want after inbound trade | a lit furnace running short; coal or oil on the order book |
| `labour_bound` | of the held surplus above subsistence, the share standing under arms — the hands the works cannot have | works standing idle for hands while the fields are full |
| `ground_forest` | forest on held ground — the Charcoal endowment, which no gate atom carries; surveyed from tile cover and scored as fuel is (Ben, 2026-09-18, NR-891 and the wave 1 form) | the terrain under the seat |
| `credit_bound` | how far the capital's treasury falls short of the one venture the sim prices in capital, a post road | a venture the seat cannot pay for out of its purse |

**How the sim reads them.** SETTLED (Ben, 2026-09-18, NR-891: accepted as the BL-1038 build read
them, with forest given a source below): the sim
carries no per-work labour need and prices no node in stores, so `labour_bound` and `credit_bound`
read the nearest quantity it does carry, as above. `tariff_pressure` has no source — the sim
carries no landed price — and is pinned at 0.

**Forest is surveyed, so the forest polity chooses Charcoal (Ben, 2026-09-18, NR-891).** A region
carries a forest share: the share of land tiles in its survey window under forest cover, read from
the tiles as its fuel is. **It is scored as fuel is (Ben, 2026-09-18, wave 1 form), and both read
a SHARE (Ben, 2026-09-19, NR-896):** `ground_forest` is the share of held regions whose forest score
clears the world's bar at the span open, and `ground_fuel` is the same share over fuel scores, so
the Fuel Doctrine compares like with like. The bar is set below (NR-899). A best-of-held maximum can only rise as a realm
grows, so it let breadth rather than ground decide the fork (Charcoal outnumbered Coke 319 to 200 on
the lane's run) and broke the rule that no term grows with the polity's size. A held mean was
rejected because a large realm with one coalfield reads low. The `fuel` gate is unchanged: any held
seam still opens it. **The bar is each resource's own top third, not the mean (Ben, 2026-09-19,
NR-899):** a held region counts toward a pull when its span-open score for that resource is in the
top third of every region's score for it, and a region scoring 0 never counts. Against the mean,
coal's rarity starved its pull — few regions clear a mean that a handful of coalfields set, while
forest is widespread — and Charcoal led Coke 373 to 142. With a per-resource top third, fuel and
forest clear at the same rate world-wide, so the fork compares like with like literally. The bar
is fixed once at the span open from the world's regions: it is a property of the map, never a
rank among polities, and it does not move as a realm grows. Both pulls count the same held
regions: those carrying the span-open survey of both scores. Ground the span founds inherits fuel
but carries no forest reading, so it is left out of both (Ben, 2026-09-19, NR-899). A plain average share read about 212 on the median region and let almost any
seam outweigh any forest. So a polity takes Charcoal Iron because its ground is wooded, not only because Coke is gated
out. Inside the span every fuel read in this tree — the gate, `ground_fuel` and the seam flag —
takes the same survey (Ben, 2026-09-18). Of the
shared core, `threatened` is the heaviest grudge any living polity holds against this one,
`many_peoples` is the share of held regions whose plurality people is not the realm's, `known` is
per node (held by a living polity this one has met), and `plague_struck` is pinned at 0 because
the history sim runs no plague.

**Three worked situations**, each assuming the polity already holds whatever
`EXPLORATION_TREE.md` gave it at 1660.

*The coalfield polity.* Seams under held ground, ore nearby, a river to the coast. `ground_fuel`
buys Coke Smelting first under the Fuel Doctrine fork, `reach_bound` buys Railway, and it reaches
The Cheap Ton by the shortest road in the tree; its 1960 spread is the widest.

*The forest polity that took Charcoal.* No seam worth a pit, forest everywhere, good ore.
`ground_forest` takes Charcoal Iron, `threatened` buys Interchangeable Parts, and steel arrives a
ring late through Machine Tools — a quality arms-maker whose furnace lights a generation after its
neighbour's.

*The trading power that crossed with a carrack.* Arrives at 1660 already holding Exploration's
Ocean Carrack and Chartered Company. `colonial_reach` buys Steamship early, `credit_bound` builds
Bond Market and General Incorporation, and it climbs the spire on landed coal and freight revenue
rather than its own ore; its 1960 nation is a trading and chartering power whose heavy plant is
somebody else's.

---

## What the tree hands the 1960 campaign

**The capacity vector.** A polity's held mask crosses the handoff whole; the seven-domain band the
works and unit rosters still read is derived from it, never stored beside it. Every `unlock` that
names a works row (`../../lore/HISTORY.md` § The works roster) or a unit roster band lands as the
nation's starting rows.

**Corporate terms.** Works Doctrine × General Incorporation decides on what terms a specialist
corporation exists in a nation — the input `../CORPORATION_GENERATION.md` reads for nation
assignment.

| | General Incorporation held | not held |
|---|---|---|
| **Private Works** | a specialist corporation may be chartered here, by registration | may be chartered, by sovereign grant — a guest, not a citizen |
| **State Arsenal** | no specialist corporation may be chartered; firms register and lease the plant | no specialist corporation; firms operate at the sovereign's pleasure |
| **neither side taken** | the nation never faced the question; reads as State Arsenal without the plant | the same |

**A charter-budget world does not read these terms.** PROPOSED (listed to Ben 2026-09-18, not
overturned): where a centre's budget charters the web, a specialist stands wherever its centre can
afford one (`../DIGITISATION.md` § 1). Read as written, the bottom row would deny a specialist to
every nation that never took a Works side, and how many those are is unmeasured.

**Demography.** Labour Doctrine × Farm Mechanisation × Soil Chemistry set a nation's growth rate,
urban share and the unrest it inherits; Health sets the mortality side of the same transition.

**Tariff posture and colonial ties.** A polity that ended the pass under `tariff_pressure` arrives
protective, enacted as an ordinary import tariff at world setup; a polity with `colonial_reach`
arrives with its colony's chains routed through its metropole first.

**War endurance.** Machine-age artifacts diffuse — Automotive, Antibiotics — but Converter Steel
with Machine Tools is what lets a nation *replace* what it fields. That split is legible from the
held set alone, and The Renewed Line names it.

---

## Open questions

- **The rim through Health.** Requiring Antibiotics at The Renewed Line makes the sparse branch
  load-bearing, and it now reaches the spire only at the rim, later than any other branch. Whether
  a median polity reaches it is the sizing rule's assertion, and the sweep may move the requirement
  to Land instead.
- **`ground_forest` as a term, not a gate.** Charcoal wants forest and the gate vocabulary has no
  forest atom, so the endowment pull lives in the scorer alone. A gate atom would be the honest
  fix if the sweep finds forest polities taking Coke.
- **Early Computing.** The ladder's epoch frontier has no node here; the campaign tree's own root
  owns it, and adding it would spend two of the cap's spare nodes on a leaf nothing in the sim
  reads.
- **What was lost with old ring 1.** Cast Ordnance's gunpowder-roster unlock, Preventive
  Inoculation's earlier Health root, and Learned Correspondence's research trickle went out with
  the ring rather than finding a place in either tree. None was structurally required and none had
  a clean fit among Exploration's Purse/Hulls/Port & Garrison/Ways/Goods branches, but the call was
  made once, quickly, by one pass (BL-938) rather than argued node by node — worth a second look if
  the sweep finds either roster gate or the research curve wanting early.
- **Magnitudes.** Every `per_mille` is a placeholder by judgement; the sweep re-prices them
  against the sizing rule, and the ratio of minors to majors (12 to 30, with
  3 milestones, 45 nodes) is the only number here that is a design choice.
