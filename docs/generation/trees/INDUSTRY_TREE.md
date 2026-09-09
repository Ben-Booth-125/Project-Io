# Project Io — The Industry Tree

> **Settles:** the nodes of the Industry tree — the spire that is fire, its five branches and
> its four milestones · the four forks and what each side trades away · every node's effects
> and the situation in which the scorer buys it · the terms this phase adds to the shared
> scorer · what a polity's held set hands the 1960 campaign.
> **Not here:** the grammar every tree obeys — kinds, rings, the five rules, diffusion, the
> scorer's shape, the JSON schema (TREES) · the phase this tree runs in and what crosses each
> handoff (../GENERATION_STRATEGY) · what a works row does once unlocked (../../lore/HISTORY) ·
> the unit roster and its band boundary (../MILITARY_HISTORY) · the other two trees
> (COLONISATION_TREE, EMPIRE_TREE) · the 1960+ campaign tree
> (../../research/ERA1_TECH_LANDSCAPE).
> **Confused with:** ../../research/ANCIENT_TECH_LADDER.md, ../../research/TECH_EFFECTS.md,
> ../../economy/RESEARCH.md, ../CORPORATION_GENERATION.md.

The Industry tree is the third of the three trees `TREES.md` defines, and it runs inside the
economy pass, 1560 → 1960 (`../GENERATION_STRATEGY.md`). It is **invested**: the Era −1 sim's
polities are the actors, and the Invest verb takes one node a round, chosen by the scorer.

The store is `industry_tree.json`; this document is the authority and the store transcribes its
tables. `node tools/session/tree_lint.js industry` holds the two together and enforces the five
rules.

---

## Aims

**Why nations are unequal in 1960 is manufactured here.** Before this tree, connected polities sit
within a ring of each other; inside it the fuel gate opens gaps of three rings in a century, and
every object below is either a gate that binds or a fork that locks in. The rim hands off to the
campaign: a polity's held set becomes its nation's capacity vector, and the gaps between held
sets are the 1960 spread.

**Capacity gates bind here as nowhere else.** A `fuel` gate on the whole spire is the mechanism of
the great divergence — a polity with no seam under held ground climbs the spire only by trade,
which is slower and can be cut. That is the one place the map, rather than the scorer, decides
who industrialises.

---

## The spire — fire

The spire is Energy: mining to steam to the grid. Every polity climbs it, every branch leaves it
at a major, and its four milestones are the tree's only gates (`TREES.md` § Milestones, and how a
tree unlocks the next).

Ring 1 is the pit that floods, ring 2 the engine that pumps it, ring 3 the engine that moves, ring
4 the grid. Internal Combustion is not on the spire; it is Movement's ring-4 major, because oil
displaces coal at the margin and never replaces the grid.

| id | node | kind | ring | links | gate | diffusion | effect | pursued when |
|---|---|---|---|---|---|---|---|---|
| IN-SP-1a | Deep Mining & Drainage | major | 1 | IN-CH-1a IN-CH-1b IN-LD-1a IN-MT-1a IN-MV-1a IN-SP-1m | fuel | capacity | access deposits below the shallow band; modifier industrial +80 | `ground_fuel` — coal or ore lies under held ground and the shallow pits are worked out; the pit floods faster than buckets clear it |
| IN-SP-1m | The Unwearied Fire · requires IN-MT-1a IN-MV-1a | milestone | 1 | IN-SP-1a IN-SP-2a | — | capacity | open ring 2 | `spire` — the pit, the toolroom and the fuel trade are all held; nothing else at ring 1 relieves anything |
| IN-SP-2a | Atmospheric & Rotative Steam | major | 2 | IN-MT-2c IN-MV-2a IN-SP-1m IN-SP-2m | fuel | capacity | modifier industrial +150; upgrade Water Mill | `spire` — ring 2 has just opened and the fuel trade is running; the engine is the one node every ring-2 branch is adjacent to |
| IN-SP-2m | The Cheap Ton · requires IN-MT-2a IN-MT-2b IN-MV-2a | milestone | 2 | IN-SP-2a IN-SP-3a | — | capacity | open ring 3 | `spire` — the smelting doctrine is taken and a rail head is built; the ton moves, and ring 3 is the only place relief remains |
| IN-SP-3a | High-Pressure & Compound Engines | major | 3 | IN-MT-3a IN-MV-3a IN-SP-2m IN-SP-3m | fuel | capacity | modifier industrial +120; modifier reach +60 | `reach_bound` — holdings sit past free_holdings along a rail and a coast; the engine that moves is what administers the same breadth cheaper |
| IN-SP-3m | The Scheduled World · requires IN-MV-3a IN-MV-3b IN-CH-3d IN-CH-3e | milestone | 3 | IN-SP-3a IN-SP-4a | — | capacity | open ring 4 | `spire` — steamship and wire are held and the heavy plant has an owner; arrival is a printed promise |
| IN-SP-4a | Electrification | major | 4 | IN-CH-4a IN-MV-4a IN-SP-3m IN-SP-4m | fuel | capacity | modifier industrial +200; modifier carrying_capacity +80 | `furnace_lit` — the furnace is lit and stores are not the constraint; grid power is the buy that compounds |
| IN-SP-4m | The Renewed Line · requires IN-MT-4a IN-MV-4a IN-HL-4a | milestone | 4 | IN-SP-4a | — | capacity | open campaign tree | `spire` — what the polity fields it can also replace — the shell, the engine and the soldier; the rim is the last relief left |

**Ring-1 branch majors hang off the root.** The root is where pass 2 opens, so five ring-1 majors
link to it directly — Precision Instruments, Coal Haulage, Convertible Husbandry (the same pump
drains a fen), the Press and Joint-Stock (the deep pit is the first pooled venture). Health roots
through Charter, not the spire, because inoculation is a printed practice before it is anything
else.

---

## Metal

What a nation smelts with, and whether metal is counted by the piece or weighed by the ton. The
Fuel Doctrine fork sits at ring 2 off Furnace Practice, and The Cheap Ton requires it taken.

| id | node | kind | ring | links | gate | diffusion | effect | pursued when |
|---|---|---|---|---|---|---|---|---|
| IN-MT-1a | Precision Instruments | major | 1 | IN-MT-1b IN-MT-1c IN-MT-2c IN-MV-1d IN-SP-1a | — | capacity | modifier research +60; modifier industrial +40 | `spire` — the ring-1 milestone wants it and nothing binds; the toolroom is bought for what it opens |
| IN-MT-1b | Cast Ordnance | major | 1 | IN-MT-1a | ore_q | capacity | unlock Powder Mill; unlock Bastion Fort; unlock unit roster: gunpowder | `threatened` — a neighbour with a grudge fields gunpowder rows and the polity's walls are the old circuit |
| IN-MT-1c | Furnace Practice | minor | 1 | IN-MT-1a IN-MT-2a IN-MT-2b | — | practice | modifier industrial +30 | `ground_ore` — ore is held and the blast furnace is worked; the fuel question is next |
| IN-MT-2a | Coke Smelting · excludes IN-MT-2b | major | 2 | IN-MT-1c IN-MT-3a IN-MV-2b | fuel | capacity | unlock Blast Works; modifier industrial +120; retire the charcoal route | `ground_fuel` — coal seams under held ground and the forest thin; scale is the endowment's answer |
| IN-MT-2b | Charcoal Iron · excludes IN-MT-2a | major | 2 | IN-MT-1c | — | capacity | modifier industrial +60; upgrade Ore Pits; modifier defence +40 | `ground_forest` — forest on held ground and no seam worth a pit; quality is the endowment's answer and the ceiling is accepted |
| IN-MT-2c | Machine Tools | major | 2 | IN-LD-2e IN-MT-1a IN-MT-2d IN-SP-2a | ore_q | capacity | modifier industrial +100; modifier research +40 | `tariff_pressure` — a neighbour's machine-made goods land cheaper than the polity's own at its market; the toolroom closes the gap a tariff only covers |
| IN-MT-2d | Interchangeable Parts | minor | 2 | IN-MT-2c IN-MT-3a | — | practice | modifier muster_cost -40 | `threatened` — a levy is being raised and every musket is fitted by hand |
| IN-MT-3a | Converter Steel | major | 3 | IN-CH-3c IN-MT-2a IN-MT-2d IN-MT-3b IN-MT-4a IN-SP-3a | ore_q | capacity | modifier industrial +150; unlock Arsenal; unlock unit roster: industrial | `ground_ore` — ore and coke are both held and the rail head wants rails that do not wear; steel by the ton |
| IN-MT-3b | Framed Construction & Cement | major | 3 | IN-MT-3a | — | capacity | modifier carrying_capacity +80; access steeper landforms | `food_bound` — the industrial city is at its walls; the frame lets it climb rather than sprawl |
| IN-MT-4a | Synthetic Chemistry | major | 4 | IN-MT-3a IN-MT-4b IN-MT-4c | fuel | capacity | modifier industrial +100; modifier research +60 | `furnace_lit` — the furnace is lit, coal-tar is a waste stream, and the polity imports every dye, drug and explosive it uses |
| IN-MT-4b | Fixed-Nitrogen Synthesis | major | 4 | IN-LD-4b IN-MT-4a | fuel | capacity | modifier carrying_capacity +150; resource fixed nitrogen as a bulk good | `food_bound` — the fertiliser cargo arrives by sea and the sea is not safe; the food ceiling is lifted from the air |
| IN-MT-4c | Coal-Tar Distillates | minor | 4 | IN-HL-4a IN-MT-4a | — | practice | modifier plague +30 | `plague_struck` — a drawdown is running and the chemistry is held; the first drugs are a by-product |

**The charcoal road to steel is longer by design.** Converter Steel links Coke Smelting directly
but reaches a Charcoal polity only through Interchangeable Parts, so the quality choice costs a
ring of travel before the ton is cheap. Nothing forbids it; it is behind.

---

## Movement

Sea legs, colonial reach across water, the collapse of interior distance, and the wire. This is
the branch the economy pass reads for *what colonisation carried where*: an `access` effect on a
sea leg is what lets a polity hold ground it cannot walk to.

| id | node | kind | ring | links | gate | diffusion | effect | pursued when |
|---|---|---|---|---|---|---|---|---|
| IN-MV-1a | Coal Haulage | major | 1 | IN-MV-1b IN-MV-2a IN-SP-1a | fuel | capacity | resource coal as a bulk traded good; reach coastal bulk haulage; unlock Cut Canal | `fuel_bound` — a coalfield near a coast or river and a seat burning wood it must haul further every year |
| IN-MV-1b | Full-Rigged Ship | major | 1 | IN-MV-1a IN-MV-1c IN-MV-2c | coastal | capacity | access ocean sea leg; unlock Naval Yard; modifier reach +60 | `ground_port` — the seat is a port, the coast is held, and every near sea leg is already taken |
| IN-MV-1c | Celestial Navigation | major | 1 | IN-MV-1b IN-MV-1d | coastal | practice | access open-ocean legs; intel route reliability; modifier reach +40 | `colonial_reach` — ground is held across water and a third of the hulls sent to it do not arrive |
| IN-MV-1d | Chronometer | minor | 1 | IN-MT-1a IN-MV-1c | — | practice | modifier reach +20 | `coastal_holdings` — an instrument maker and a port in the same polity |
| IN-MV-2a | Railway | major | 2 | IN-CH-2c IN-MV-1a IN-MV-2b IN-MV-3b IN-SP-2a | fuel | capacity | unlock Rail Head; reach interior haul cost; modifier reach +120 | `reach_bound` — inland holdings past free_holdings and a seat that cannot supply them by road |
| IN-MV-2b | Iron Rails | minor | 2 | IN-MT-2a IN-MV-2a | — | practice | modifier reach +30 | `ground_ore` — coke iron is cheap and the wagonways are still wooden |
| IN-MV-2c | Packet Service | minor | 2 | IN-MV-1b IN-MV-3a | — | practice | modifier reach +30 | `coastal_holdings` — regular sailings between held ports; the schedule precedes the engine |
| IN-MV-3a | Steamship | major | 3 | IN-LD-3b IN-MV-2c IN-MV-4a IN-SP-3a | coastal | capacity | unlock Coaling Station; access scheduled ocean leg; modifier reach +100 | `colonial_reach` — holdings across water where the season, not the enemy, decides when they are supplied |
| IN-MV-3b | Telegraph | major | 3 | IN-MV-2a | — | capacity | unlock Signal Line; intel activity-fog freshness; modifier cohesion +60 | `cohesion_low` — a realm whose far provinces hear of a decision a season late; the wire is cohesion at range |
| IN-MV-4a | Internal Combustion & Oil | major | 4 | IN-LD-4b IN-MV-3a IN-MV-4b IN-MV-4c IN-SP-4a | fuel | capacity | modifier industrial +100; resource oil as a bulk fuel | `fuel_bound` — coal landed at the seat runs short of what its engines burn; oil seeps on held ground are the second fuel |
| IN-MV-4b | Automotive Transport | major | 4 | IN-MV-4a | — | artifact | modifier reach +80; modifier forage +40 | `reach_bound` — holdings off the rail and a road net that only carts use |
| IN-MV-4c | Flight | major | 4 | IN-MV-4a | — | capacity | intel aerial reconnaissance; modifier defence +60; unlock unit roster: machine age | `threatened` — a neighbour with a grudge holds the engine and the polity cannot see past its own border |

**Two roads to the ton.** Railway is adjacent to Coal Haulage (the wagonway) and to Iron Rails
(the coke bridge), so a polity with a coalfield reaches the rail head from either side. Telegraph
hangs off Railway because rail signalling is what drives the net.

---

## Land

The food ceiling, and what a nation does to its own countryside to fill the works. The Labour
Doctrine fork sits at ring 2 off Field Survey; neither milestone requires it, so a polity may reach
1960 without ever having asked the tenure question.

| id | node | kind | ring | links | gate | diffusion | effect | pursued when |
|---|---|---|---|---|---|---|---|---|
| IN-LD-1a | Convertible Husbandry | major | 1 | IN-LD-1b IN-LD-2a IN-LD-2d IN-SP-1a | arable | practice | modifier carrying_capacity +60; modifier stores +40 | `ground_grass` — grass and arable on the same held ground and fallow still a third of the fields |
| IN-LD-1b | Crop-Package Exchange | major | 1 | IN-LD-1a | — | practice | modifier carrying_capacity +80; modifier forage +30 | `colonial_reach` — a sea leg to ground with a different crop package; what grows there grows here |
| IN-LD-2a | Field Survey | minor | 2 | IN-LD-1a IN-LD-2b IN-LD-2c | — | practice | modifier carrying_capacity +20 | `food_bound` — the fields are mapped and the tenure question is asked |
| IN-LD-2b | Cleared Holdings · excludes IN-LD-2c | major | 2 | IN-LD-2a | arable | practice | modifier manpower +120; modifier industrial +80; modifier cohesion -60 | `labour_bound` — the works want hands and the fields hold them; the land is consolidated and the labour released in a generation |
| IN-LD-2c | Smallholder Tenure · excludes IN-LD-2b | major | 2 | IN-LD-2a | arable | practice | modifier cohesion +60; modifier stores +80; modifier industrial -40 | `cohesion_low` — the seat's cohesion is low and the fields are the one thing the people hold; the land stays divided |
| IN-LD-2d | Farm Mechanisation | major | 2 | IN-LD-1a IN-LD-2e IN-LD-3a | arable | capacity | modifier manpower +100; modifier carrying_capacity +60 | `manpower_bound` — the manpower ceiling is hit and most of the population is still in the fields |
| IN-LD-2e | Iron Implements | minor | 2 | IN-LD-2d IN-MT-2c | — | practice | modifier carrying_capacity +30 | `ground_farm` — a toolroom and farm ground in the same polity |
| IN-LD-3a | Soil Chemistry & Fertiliser Trade | major | 3 | IN-LD-2d IN-LD-3b IN-LD-4a | — | artifact | resource fertiliser as a traded good; modifier carrying_capacity +100 | `food_bound` — yield bounded by the local nitrogen cycle and a port that can land cargo |
| IN-LD-3b | Fertiliser Cargo | minor | 3 | IN-LD-3a IN-MV-3a | — | practice | modifier carrying_capacity +30 | `ground_port` — a steamship route with a fertiliser source at the far end |
| IN-LD-4a | Tractor & Fertiliser Package | major | 4 | IN-LD-3a IN-LD-4b | arable | capacity | modifier manpower +150; modifier carrying_capacity +100 | `manpower_bound` — the ceiling is hit again with a tenth of the people still on the land |
| IN-LD-4b | Fuel & Fertiliser Depot | minor | 4 | IN-LD-4a IN-MT-4b IN-MV-4a | — | practice | modifier forage +30 | `ground_farm` — oil, fixed nitrogen and farm ground on the same road |

**Land is the branch the demography reads.** Every major here moves `carrying_capacity` or
`manpower`, and the fork moves `cohesion` in opposite directions — the human price of the
divergence made into a choice rather than an assumption.

---

## Charter

Who may own, promise, invent and read — the terms a corporation is chartered on. Two forks live
here: Sovereign Doctrine at ring 1 off Treasury Audit, Works Doctrine at ring 3 off Plant
Registry, and The Scheduled World requires the second taken.

| id | node | kind | ring | links | gate | diffusion | effect | pursued when |
|---|---|---|---|---|---|---|---|---|
| IN-CH-1a | Movable-Type Press | major | 1 | IN-CH-1f IN-SP-1a | — | capacity | modifier research +80; modifier assimilation +40 | `many_peoples` — several peoples under one seat and the law copied by hand; the press is one text everywhere |
| IN-CH-1b | Joint-Stock & Public Credit | major | 1 | IN-CH-1c IN-CH-2c IN-SP-1a | — | practice | institution public credit; debt on the sovereign's promise; unlock Counting House; modifier stores +60 | `credit_bound` — the deep pit or the fleet costs more than a round's surplus; the venture is pooled and the sovereign borrows |
| IN-CH-1c | Treasury Audit | minor | 1 | IN-CH-1b IN-CH-1d IN-CH-1e | — | practice | modifier cohesion +20 | `stores_low` — the seat's stores are counted against its promises for the first time |
| IN-CH-1d | Chartered Capital · excludes IN-CH-1e | major | 1 | IN-CH-1c IN-CH-2b | — | practice | institution courts bind the sovereign; cheaper state credit, seizure carries a lasting credit cost; modifier stores +60; modifier research +40 | `surplus` — stores are comfortable and the lenders are the seat's own merchants; the sovereign binds itself to borrow cheaper |
| IN-CH-1e | Command Estate · excludes IN-CH-1d | major | 1 | IN-CH-1c | — | practice | institution crown monopolies and confiscation rights; immediate revenue, chronic credit penalty; modifier stores +100; modifier cohesion -40 | `stores_low` — stores are low, a war is on, and the merchants' silver is nearer than any lender's |
| IN-CH-1f | Learned Correspondence | minor | 1 | IN-CH-1a IN-CH-2a IN-HL-1a | — | practice | modifier research +20 | `known` — a neighbour's presses are running and letters cross the border |
| IN-CH-2a | Empirical Method | major | 2 | IN-CH-1f IN-CH-2b IN-CH-3b IN-HL-2a | — | practice | modifier research +100 | `surplus` — nothing binds; the research rate is what compounds |
| IN-CH-2b | Patent Grants | major | 2 | IN-CH-1d IN-CH-2a | — | practice | modifier research +60; institution property in invention | `known` — a neighbour invents and the polity copies; the grant is what makes its own toolrooms invent |
| IN-CH-2c | Bond Market | minor | 2 | IN-CH-1b IN-CH-3a IN-MV-2a | — | practice | modifier stores +30 | `credit_bound` — a rail head or a fleet priced beyond a round's surplus |
| IN-CH-3a | General Incorporation | major | 3 | IN-CH-2c IN-CH-3c | — | practice | institution chartered corporate form by registration, not by grant | `surplus` — stores comfortable, public credit held, and more ventures asking for a charter than the sovereign grants in a year |
| IN-CH-3b | Mass Schooling | major | 3 | IN-CH-2a IN-CH-4a | — | practice | modifier research +80; modifier assimilation +80; modifier industrial +40 | `many_peoples` — held ground whose shares still charge the holder; the school is the assimilation machine |
| IN-CH-3c | Plant Registry | minor | 3 | IN-CH-3a IN-CH-3d IN-CH-3e IN-MT-3a | — | practice | modifier industrial +20 | `furnace_lit` — the furnace is lit and the question of who owns it is asked |
| IN-CH-3d | State Arsenal · excludes IN-CH-3e | major | 3 | IN-CH-3c | — | capacity | institution heavy plant nation-owned; corporations lease; modifier industrial +120; modifier muster_cost -40 | `threatened` — a neighbour with a grudge lit its furnace first; the plant must arrive fast and answer to the seat |
| IN-CH-3e | Private Works · excludes IN-CH-3d | major | 3 | IN-CH-3c | — | capacity | institution corporations own processing capacity; modifier industrial +60; modifier research +60; modifier stores +40 | `surplus` — no grudge at the border and the counting houses hold the credit; the plant is chartered and compounds |
| IN-CH-4a | Broadcast | major | 4 | IN-CH-3b IN-SP-4a | — | capacity | modifier cohesion +100; modifier assimilation +60 | `cohesion_low` — a realm spanning water whose provinces hear the seat only by wire |

**Works Doctrine is not Sovereign Doctrine.** The first decides who owns the furnaces; the second
decides whether courts bind the sovereign. A nation can honour every debt and still own every
works in it, and at 1960 that nation is one a specialist corporation cannot be chartered in.

---

## Health

The demographic transition: mortality falling ahead of wealth. A sparse branch, rooted through
Charter and bridged to Metal at the rim, whose every node is bought under `plague_struck`.

| id | node | kind | ring | links | gate | diffusion | effect | pursued when |
|---|---|---|---|---|---|---|---|---|
| IN-HL-1a | Preventive Inoculation | major | 1 | IN-CH-1f IN-HL-2a | — | practice | modifier plague +60 | `plague_struck` — a drawdown running in the seat and a neighbour whose towns are spared |
| IN-HL-2a | Microscopy | minor | 2 | IN-CH-2a IN-HL-1a IN-HL-3a | — | practice | modifier plague +20 | `plague_struck` — the method is held and the lens shows the cause |
| IN-HL-3a | Germ Theory & Sanitation | major | 3 | IN-HL-2a IN-HL-4a | — | practice | modifier plague +150; modifier carrying_capacity +60 | `plague_struck` — the industrial city outgrows its wells; mortality falls ahead of wealth |
| IN-HL-4a | Antibiotics & Mass Vaccination | major | 4 | IN-HL-3a IN-MT-4c | — | artifact | modifier plague +200; modifier manpower +60 | `plague_struck` — the chemistry is held and the wards are full; the transition completes |

**The rim requires it.** Antibiotics is one of The Renewed Line's three requirements, because a
nation that can replace what it fields must replace the soldier as well as the shell.

---

## Forks

Four, each two majors off one shared minor at the same ring, each side naming the other in
`excludes` (`TREES.md` § Forks). Taking one closes the other permanently and it goes dark under the
fog.

| fork | shared minor | side | trades |
|---|---|---|---|
| **Fuel Doctrine** (Metal, ring 2) | IN-MT-1c Furnace Practice | IN-MT-2a Coke Smelting | scale and the Blast Works; wants a coalfield, and the charcoal route retires |
| | | IN-MT-2b Charcoal Iron | quality and better arms; wants forest, and accepts the smelting ceiling |
| **Labour Doctrine** (Land, ring 2) | IN-LD-2a Field Survey | IN-LD-2b Cleared Holdings | an urban workforce in a generation; cohesion falls and the countryside hollows |
| | | IN-LD-2c Smallholder Tenure | a deeper domestic market and cohesion; industrialisation starts late |
| **Sovereign Doctrine** (Charter, ring 1) | IN-CH-1c Treasury Audit | IN-CH-1d Chartered Capital | cheaper state credit and corporate autonomy; seizure carries a lasting credit cost |
| | | IN-CH-1e Command Estate | immediate revenue; a chronic credit penalty and corporations at the sovereign's pleasure |
| **Works Doctrine** (Charter, ring 3) | IN-CH-3c Plant Registry | IN-CH-3d State Arsenal | the heavy plant arrives fast and answers to the seat; corporations lease what they cannot own |
| | | IN-CH-3e Private Works | a slower start that compounds; corporations own capacity outright |

Two forks sit under a milestone and are therefore unavoidable — Fuel under The Cheap Ton, Works
under The Scheduled World. Either side satisfies the milestone; the choice is never dictated.

---

## Milestones

One per ring, on the spire, named for the regime beyond it. Each requires majors from at least two
branches at its own ring; where a fork is listed, either side satisfies it.

| id | milestone | ring | thesis | requires |
|---|---|---|---|---|
| IN-SP-1m | **The Unwearied Fire** | 1 | heat becomes motion that never tires and does not care where the river runs | IN-MT-1a Precision Instruments · IN-MV-1a Coal Haulage |
| IN-SP-2m | **The Cheap Ton** | 2 | metal stops being counted by the piece and starts being weighed by the ton | Fuel Doctrine taken (IN-MT-2a ⊘ IN-MT-2b) · IN-MV-2a Railway |
| IN-SP-3m | **The Scheduled World** | 3 | arrival stops being a matter of season and wind and becomes a printed promise | IN-MV-3a Steamship · IN-MV-3b Telegraph · Works Doctrine taken (IN-CH-3d ⊘ IN-CH-3e) |
| IN-SP-4m | **The Renewed Line** | 4 | what a nation fields it can also replace, from its own works and not by purchase abroad | IN-MT-4a Synthetic Chemistry · IN-MV-4a Internal Combustion & Oil · IN-HL-4a Antibiotics & Mass Vaccination |

The Unwearied Fire's requirements are exactly the engine's prerequisites — the pit below it on the
spire, the toolroom and the fuel trade — so ring 2 opens the moment the engine is buildable. The
Renewed Line opens the campaign tree; a polity that never reaches it enters 1960 with what it
holds, never excluded, only behind.

---

## The scorer

The shape is `TREES.md` § The scorer — one shape, three trees; this tree adds no term to the
formula, only readings. Every reading below is an in-world quantity with a visible cause, never a
rank and never a quantity that grows with the polity's own size.

**The shared core.** `reach_bound`, `manpower_bound`, `food_bound`, `stores_low`, `cohesion_low`,
`threatened`, `plague_struck`, `ground_ore`, `ground_farm`, `ground_fuel`, `ground_port`,
`ground_grass`, `many_peoples`, `coastal_holdings`, `spire`, `known`, `surplus` — the vocabulary
the Empire tree shares, each a reading of polity or region state the sim already carries.

**Added by this phase.**

| term | the reading | its visible cause |
|---|---|---|
| `furnace_lit` | the polity holds the spire's ring-2 major | a lit engine house on the map; the furnace crossing already recorded |
| `colonial_reach` | at least one held region is reachable from the seat only across a sea leg | a hull on the route; the tie the campaign reads as a colonial preferred-seller |
| `tariff_pressure` | at a market the polity's holdings trade through, a neighbour's landed price for a good the polity itself makes is below its own | the price field on the route; the reading that becomes the nation's tariff posture |
| `fuel_bound` | fuel demanded by held works and engines exceeds fuel landed at the seat this round | a lit furnace running short; coal or oil on the order book |
| `labour_bound` | the labour split's subsistence share leaves the industry slice below what the held works need | works standing idle for hands while the fields are full |
| `ground_forest` | forest share on held ground — the Charcoal endowment, which no gate atom carries | the terrain under the seat |
| `credit_bound` | the node or work the scorer wants costs more than one round's surplus of stores | a venture the seat cannot pay for out of one harvest |

**Three worked situations.**

*The coalfield polity.* Seams under held ground, ore nearby, a river to the coast. `ground_fuel`
buys Deep Mining first, then Coal Haulage under `fuel_bound`, and Coke at the fork; it reaches The
Cheap Ton by the shortest road in the tree and its 1960 spread is the widest.

*The forest polity that took Charcoal.* No seam worth a pit, forest everywhere, good ore.
`ground_forest` takes Charcoal Iron, `threatened` buys Cast Ordnance and Interchangeable Parts,
and steel arrives a ring late through the toolroom — a quality arms-maker whose furnace lights a
generation after its neighbour's.

*The sea-facing polity with no ore.* A port seat, coast held, nothing under it. `ground_port`
buys Full-Rigged Ship, `colonial_reach` buys Celestial Navigation and later Steamship, and
`credit_bound` builds the Charter branch early; it climbs the spire by landed coal alone, and its
1960 nation is a trading and chartering power whose heavy plant is somebody else's.

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

- **Sovereign Doctrine untaken.** No milestone forces it, so a 1960 nation can hold neither side.
  Whether that reads as Command Estate by default, or as a third credit regime, is a call for the
  campaign's finance seam rather than this tree.
- **Cast Ordnance as a node.** The unit roster turns over on the band boundary and needs no node;
  this one exists for the two Gunpowder works rows it unlocks. If the works roster re-keys on
  nodes directly, it is the first node to re-examine.
- **The rim through Health.** Requiring Antibiotics at The Renewed Line makes the sparse branch
  load-bearing. Whether a median polity reaches it is the sizing rule's assertion, and the sweep
  may move the requirement to Land instead.
- **`ground_forest` as a term, not a gate.** Charcoal wants forest and the gate vocabulary has no
  forest atom, so the endowment pull lives in the scorer alone. A gate atom would be the honest
  fix if the sweep finds forest polities taking Coke.
- **Early Computing.** The ladder's epoch frontier has no node here; the campaign tree's own root
  owns it, and adding it would spend two of the cap's spare nodes on a leaf nothing in the sim
  reads.
- **Magnitudes.** Every `per_mille` is a placeholder by judgement; the sweep re-prices them
  against the sizing rule, and the ratio of minors to majors (15 to 43, with
  4 milestones, 62 nodes) is the only number here that is a design choice.
