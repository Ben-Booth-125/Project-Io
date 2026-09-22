# Project Io — The Colonisation Tree

> **Settles:** what a people carries out of the migration, node by node · the spire as the
> domestication package · one branch per kind of ground and what that ground teaches · which of
> the twelve farm classes teaches which branch, and why two teach nothing · the three
> milestones and the condition under which a culture seeds a polity · what the tree hands the
> Empire phase.
> **Not here:** the grammar every tree obeys — kinds, rings, the five rules, diffusion by kind,
> the JSON schema (TREES) · the migration this tree runs inside — the package, the stream, the
> walk, predation, culture by route (../COLONISATION) · the phase a seeded polity enters and its
> own tree (../CIVILISATION, EMPIRE_TREE) · how a pantheon or a tongue is coined (../../lore/CREEDS).
> **Confused with:** TREES.md, ../COLONISATION.md, EMPIRE_TREE.md.

---

## Aims

**A people's technology at 400 BCE is a fact with a history, not a roll.** The migration has no
actor (`../COLONISATION.md` § No actor, and no infrastructure), so this tree has no scorer and no
Invest: a culture holds a node because it spent time on ground that satisfies the node's gate, or
because a neighbour handed it over by contact, and a daughter copies its parent's mask at coining
(`TREES.md` § State, and where research comes from). What a people carries is therefore a record of
where it walked and whom it met.

**The tree's one job is to say who can seat a polity.** Its rim milestone is the condition under
which a culture seeds a polity that starts the Empire tree at its root (`TREES.md` § Milestones,
and how a tree unlocks the next); everything below the rim feeds population, spread or the seat,
and nothing here is decorative. A people that never reaches the rim is never excluded — it enters
the Empire tree late, once it holds the prior milestone.

---

## The spire

The spire is the domestication package as a climb: gathering becomes cultivation, cultivation
earns storage, storage makes a village worth staying beside, and a village with a common store is
ground a seat can stand on. One major and one milestone per ring, chained; the five branches leave
the spire at its ring-1 major.

| id | node | kind | ring | links | gate | diffusion | effect | earth_ref |
|---|---|---|---|---|---|---|---|---|
| CO-SP-1a | Seed Selection | major | 1 | CO-SP-1m; the five ring-1 branch majors | — | practice | carrying_capacity +100‰ | the Neolithic package's first act — keeping the best seed back, ~9th millennium equivalent |
| CO-SP-1m | The Fed Band | milestone | 1 | CO-SP-1a, CO-SP-2a | — | capacity | open ring 2 | a band that eats more than it gathers |
| CO-SP-2a | Granary Pit | major | 2 | CO-SP-1m, CO-SP-2m | — | capacity | stores +150‰ · carrying_capacity +60‰ | lined storage pits and raised granaries, ~8th–7th millennium equivalents |
| CO-SP-2m | The Settled Village | milestone | 2 | CO-SP-2a, CO-SP-3a | — | capacity | open ring 3 | the year-round village — a store worth staying beside |
| CO-SP-3a | Common Store | major | 3 | CO-SP-2m, CO-SP-3m | — | capacity | stores +100‰ · manpower +60‰ · assimilation +60‰ | the store held in common under a keeper — the town before the tally, ~4th millennium equivalent |
| CO-SP-3m | The Standing Seat | milestone | 3 | CO-SP-3a | — | capacity | open the Empire tree | a town that can be seated — the condition for a polity, not a polity |

The two capacity majors — the granary pit and the common store — diffuse in generations or never,
needing the ground locally (`TREES.md` § Diffusion follows kind). That is deliberate: storage is
what a people builds where it stays, and it is the one thing a contact cannot simply hand over.

---

## The branches — one per kind of ground

A branch is what a class of ground teaches a people that farms it. The migration classifies every
tile by farm class and coins a daughter culture *of* the class it settles
(`../CIVILISATION.md` § Similarity is kinship; opposition needs a second axis), so a branch's gate
is the class its people were coined on, and a people that crosses onto new country learns that
country's branch. Practice majors diffuse by contact in decades, so a people also learns what its
neighbours know — which is how a valley people comes to hold the boat.

### Which ground teaches which branch

The migration's classifier (`classify_farm_class`) names twelve farm classes. Five branches own
ten of them; two are owned by nothing, on purpose (Ben, 2026-09-16).

| Farm class | What the classifier reads | Branch it teaches |
|---|---|---|
| floodplain | marsh cover on soft ground | Wet Ground (FL) |
| valley | valley landform | Wet Ground (FL) |
| highland | highland or crater landform | High Ground (HL) |
| montane | mountain, canyon or rift landform | High Ground (HL) |
| volcanic | volcanic substrate or ash cover | High Ground (HL) |
| grassland | grass cover | Open Ground (GR) |
| steppe | open ground nothing else claimed — scrub or bare soil | Open Ground (GR) |
| coastal | the shoreline ring | The Shore (CS) |
| woodland | forest cover | The Forest (FO) |
| boreal | snow cover or icy substrate | The Forest (FO) |
| arid | dunes, salt crust, barren substrate | **none — hostile country** |
| stone | rocky, metallic or regolith substrate under thin cover | **none — hostile country** |

**Arid and stone are hostile country, and hostile country teaches nothing.** This is a design
statement, not a gap waiting for a sixth branch. Dunes, salt and bare rock are ground almost
nothing is farmed on; the walk already prices a desert as a barrier (`../COLONISATION.md` § The
stream — what moves, and what it costs), and the tree makes it a barrier in what a people
carries too. A people coined on hostile country starts with no branch major. It climbs only by
walking onto ground that teaches, or by meeting a people that knows, so it reaches The Fed Band
late by construction — poor for a reason the map shows, never poor by accident. This is not a
missing branch, and it is not to be filed as one.

### Wet Ground (FL)

Floodplain and valley ground teach the flood, the channel and the basin. The branch feeds
population almost alone, and its two upper majors are capacity: irrigation works stay where they
were dug.

| id | node | kind | ring | links | gate | diffusion | effect | earth_ref |
|---|---|---|---|---|---|---|---|---|
| CO-FL-1a | Flood-Basin Sowing | major | 1 | CO-SP-1a, CO-FL-1b | arable | practice | carrying_capacity +120‰ | sowing into the receding flood's silt, ~7th millennium equivalent |
| CO-FL-1b | Silt Reading | minor | 1 | CO-FL-1a, CO-FL-2a | — | practice | carrying_capacity +30‰ | knowing where the flood will lay good ground next year |
| CO-FL-2a | Channel Cutting | major | 2 | CO-FL-1b, CO-FL-2b, CO-FL-2c | arable | capacity | carrying_capacity +150‰ · stores +40‰ | the first cut channels leading water to a field, ~6th millennium equivalent |
| CO-FL-2b | Levee Raising | minor | 2 | CO-FL-2a, CO-FL-3a | — | practice | plague +30‰ | banking the flood off the village — drained ground breeds less |
| CO-FL-2c | Reed Bundling | minor | 2 | CO-FL-2a, CO-CS-2a | — | practice | stores +30‰ | bundled reed as basket, wall and raft — the wet ground's gift to the shore |
| CO-FL-3a | Basin Irrigation Works | major | 3 | CO-FL-2b | arable | capacity | carrying_capacity +200‰ · manpower +60‰ | basin systems that hold the flood on the field for weeks, ~4th millennium equivalent |

Reed Bundling is the between-wedge minor joining Wet Ground to The Shore at ring 2 (`TREES.md`
§ The five rules, rule 5): the reed raft is where the floodplain and the coast meet.

### High Ground (HL)

Highland, montane and volcanic ground teach the terrace, the flock and the beast that carries.
The branch ends on `access` — the pack animal is what turns a range from a wall into a road, which
is the migration's whole cost model applied to one kind of ground.

| id | node | kind | ring | links | gate | diffusion | effect | earth_ref |
|---|---|---|---|---|---|---|---|---|
| CO-HL-1a | Hill Terracing | major | 1 | CO-SP-1a, CO-HL-1b | — | practice | carrying_capacity +100‰ | stepped hillside plots holding soil against the slope, ~7th millennium equivalent |
| CO-HL-1b | Stone Clearing | minor | 1 | CO-HL-1a, CO-HL-2a | — | practice | carrying_capacity +30‰ | the cleared stone becomes the field wall |
| CO-HL-2a | Flock Keeping | major | 2 | CO-HL-1b, CO-HL-2b, CO-HL-2c | — | practice | carrying_capacity +80‰ · forage +60‰ | small stock penned on ground too steep to sow, ~8th millennium equivalent |
| CO-HL-2b | Seasonal Pasture | minor | 2 | CO-HL-2a, CO-HL-3a | — | practice | forage +40‰ | the flock walked up in summer and down in winter |
| CO-HL-2c | Wool Working | minor | 2 | CO-HL-2a, CO-GR-2a | — | practice | carrying_capacity +30‰ | fleece spun and woven — cold ground becomes country a people can winter on |
| CO-HL-3a | Pack Animal | major | 3 | CO-HL-2b | — | practice | access: mountain · forage +60‰ | a beast that carries what a person cannot, ~4th millennium equivalent |

Wool Working is the between-wedge minor joining High Ground to Open Ground at ring 2: the flock
and the herd share a fleece. The branch's majors carry no gate because the gate vocabulary has no
highland atom (§ Open questions).

### Open Ground (GR)

Grassland and steppe teach the herd followed, the herd kept, and the horse. The branch feeds
forage and manpower more than capacity, and its rim major is the second `access` in the tree —
open ground crossed in days rather than seasons.

| id | node | kind | ring | links | gate | diffusion | effect | earth_ref |
|---|---|---|---|---|---|---|---|---|
| CO-GR-1a | Herd Following | major | 1 | CO-SP-1a, CO-GR-1b | grassland | practice | forage +100‰ · carrying_capacity +60‰ | walking behind the wild herd through its year |
| CO-GR-1b | Fire Clearing | minor | 1 | CO-GR-1a, CO-GR-2a | — | practice | carrying_capacity +30‰ | burning old grass so the herd comes back to the new |
| CO-GR-2a | Cattle Penning | major | 2 | CO-GR-1b, CO-GR-2b, CO-HL-2c | grassland | practice | carrying_capacity +100‰ · manpower +40‰ | large stock held rather than followed, ~7th–6th millennium equivalents |
| CO-GR-2b | Milk Keeping | minor | 2 | CO-GR-2a, CO-GR-3a | — | practice | carrying_capacity +40‰ | a living animal fed on twice — dairying as a second harvest |
| CO-GR-3a | Horse Taming | major | 3 | CO-GR-2b, CO-GR-3b | grassland | practice | access: steppe · manpower +80‰ · forage +60‰ | the ridden horse, ~4th millennium equivalent |
| CO-GR-3b | Cart Haulage | minor | 3 | CO-GR-3a | — | practice | stores +40‰ | the drawn cart — a harvest moves to the store instead of the store to the harvest |

### The Shore (CS)

The coast teaches the net, the dried catch and the boat. The shore is the migration's road
(`../COLONISATION.md` § Coastal and overseas routes are the ones that need emphasis), and the
branch's rim major is the crude hop that doc asks for — a strait crossed, never a sea sailed.

| id | node | kind | ring | links | gate | diffusion | effect | earth_ref |
|---|---|---|---|---|---|---|---|---|
| CO-CS-1a | Shore Foraging | major | 1 | CO-SP-1a, CO-CS-1b | coastal | practice | forage +100‰ · carrying_capacity +60‰ | the tideline that feeds a people while it walks — the shell middens, Mesolithic equivalent |
| CO-CS-1b | Net Weaving | minor | 1 | CO-CS-1a, CO-CS-2a | — | practice | carrying_capacity +30‰ | the knotted net — a catch taken by the hundred rather than the one |
| CO-CS-2a | Fish Drying | major | 2 | CO-CS-1b, CO-CS-2b, CO-FL-2c | coastal | practice | stores +100‰ · carrying_capacity +60‰ | the catch racked and smoked — the shore's answer to the granary |
| CO-CS-2b | Salt Gathering | minor | 2 | CO-CS-2a, CO-CS-3a | — | practice | stores +40‰ | salt from the pan — what is cured keeps |
| CO-CS-3a | Hollow-Log Boat | major | 3 | CO-CS-2b | coastal | practice | access: strait · forage +40‰ | the dugout — a visible island becomes a crossing of days, Mesolithic equivalent |

The boat is a practice, not an artifact. There are no trade routes in the migration for an
artifact to leapfrog along, and a dugout is a thing a neighbour shows you how to make.

### The Forest (FO)

Woodland and boreal ground teach the forest floor, the clearing and the axe — and how to carry a
winter through. Forest is the ground the walk prices twice, dear to cross and dangerous to live
in (`../COLONISATION.md` § Predation — what caps the pressure), so the branch answers both: its
ring-2 major clears the danger back from the hearth, and its rim major is the tree's fourth
`access`, the stand as ground a people cuts a way through. The winter half — the smoke house and
the runner sledge — is why boreal ground belongs here: cold country teaches what a people eats
when nothing grows, and how it moves when the ground is snow.

| id | node | kind | ring | links | gate | diffusion | effect | earth_ref |
|---|---|---|---|---|---|---|---|---|
| CO-FO-1a | Mast Gathering | major | 1 | CO-SP-1a, CO-FO-1b | — | practice | forage +100‰ · carrying_capacity +50‰ | the forest floor's autumn fall of nut and acorn, gathered and pitted against the winter, Mesolithic equivalent |
| CO-FO-1b | Snare Setting | minor | 1 | CO-FO-1a, CO-FO-2a | — | practice | forage +30‰ | a line of snares walked each morning — small game taken under cover without the chase |
| CO-FO-2a | Ring-Bark Clearing | major | 2 | CO-FO-1b, CO-FO-2b, CO-FO-2c | — | practice | carrying_capacity +120‰ · plague +40‰ | trees girdled to die standing and the dead stand burned and sown in its own ash, ~6th millennium equivalent |
| CO-FO-2b | Smoke House | minor | 2 | CO-FO-2a, CO-FO-3a | — | practice | stores +40‰ | meat hung in the hearth smoke under the roof — the winter carried in the rafters |
| CO-FO-2c | Pitch Boiling | minor | 2 | CO-FO-2a, CO-CS-2a | — | practice | stores +30‰ | resin boiled down to pitch — a sealed basket, a sealed seam; the forest's gift to the shore |
| CO-FO-3a | Felling Axe | major | 3 | CO-FO-2b, CO-FO-3b | — | practice | access: forest · carrying_capacity +60‰ | a ground-stone blade hafted to fell rather than girdle, ~5th–4th millennium equivalent |
| CO-FO-3b | Runner Sledge | minor | 3 | CO-FO-3a | — | practice | stores +40‰ | the sledge on packed snow — a winter's haul moves over cold ground no cart could cross |

Pitch Boiling is the between-wedge minor joining The Forest to The Shore at ring 2: the sealed
seam is what the forest hands the coast, as the reed raft is what the wet ground hands it. So
The Shore sits between Wet Ground and The Forest, and the five wedges run Wet Ground, The Shore,
The Forest, High Ground, Open Ground. The branch's majors carry no gate, for the same reason High
Ground's do not: the gate vocabulary has no woodland atom (§ Open questions). Every Forest major
is a practice — an axe is a thing a neighbour shows you how to haft, and the migration has no
route for an artifact to leapfrog along.

---

## Milestones

Each milestone requires the spire major at its ring **and** any two of the five branch majors at
its ring — `requires` for the first, `requires_any` for the second (`TREES.md` § Milestones, and
how a tree unlocks the next). Rule 4's breadth holds without naming which breadth. **The count
stays two with a fifth branch.** The Forest widens *which* breadth a people may show, not *how
much* it must show: a forest people now climbs by learning one more ground, where before it had
to learn two. A people coined on one class and never in contact
holds one branch and stops below the fed band, which is the sessile outcome `../COLONISATION.md`
§ What a cradle stopping actually means calls normal; a people that learned two grounds, by
walking or by neighbours, climbs.

| id | name | thesis | requires |
|---|---|---|---|
| CO-SP-1m | The Fed Band | A people that eats more than it gathers, and has learned a second ground. | CO-SP-1a and any two of CO-FL-1a, CO-HL-1a, CO-GR-1a, CO-CS-1a, CO-FO-1a |
| CO-SP-2m | The Settled Village | A store worth staying beside, on ground that keeps two kinds of harvest. | CO-SP-2a and any two of CO-FL-2a, CO-HL-2a, CO-GR-2a, CO-CS-2a, CO-FO-2a |
| CO-SP-3m | The Standing Seat | A town whose common store feeds hands it does not need in the field — ground a seat can stand on. | CO-SP-3a and any two of CO-FL-3a, CO-HL-3a, CO-GR-3a, CO-CS-3a, CO-FO-3a |

**A milestone never diffuses.** A people holds The Standing Seat because it climbed to it; it
cannot be handed a seat by a neighbour, however close the contact.

---

## What the tree hands the Empire phase

| Consumer | What it receives | What it does with it |
|---|---|---|
| The seeding of polities at 400 BCE | Which cultures hold CO-SP-3m | A culture holding the rim seeds a polity at the Empire tree's root; one that does not enters late, once it holds the milestone below |
| The Empire tree's scorer | The held mask — which `access` a people carries, which grounds it learned | The endowment pull and the binding terms read a polity's inherited practice, so a horse people and a boat people open the next tree differently |
| `../CIVILISATION.md` § Similarity is kinship; opposition needs a second axis | The branch a people climbed furthest | The material disagreement between two peoples is visible in their masks, not only in their origin class |
| `../COLONISATION.md` § The stream — what moves, and what it costs | The four `access` effects and the capacity modifiers | The walk prices a range, a steppe, a strait or a forest cheaper for a people that carries the crossing, and a region's asymptote rises with what its people know |

---

## Open questions

- **A highland gate atom, and a woodland one.** The gate vocabulary is
  `ore_q · fuel · arable · coastal · grassland`, and none of them says *high ground* or *forest*.
  High Ground's and The Forest's majors carry no gate, so any people could hold the terrace or
  the felling axe by time on any ground. The atom is added only if a sweep shows those nodes held
  by peoples who never stood on that ground — and **no sweep can show that while the sim does not
  read this tree**. Without that evidence neither atom is added; the first sweep over a sim that
  reads the tree is where the question is decided.
- **What `access` targets name.** `mountain`, `steppe`, `strait` and `forest` are written here as
  terrain classes the walk prices; whether the walk reads them as farm classes, landforms or the
  region domain is the implementation's to settle.
- **Whether nodes are held per culture or per region.** `TREES.md` § Open questions carries this
  one; the tables above assume per culture, which is why a mask copies at coining.
- **Magnitudes.** Every per_mille here is a placeholder by judgement, and the sweep re-prices
  them against the sizing rule (`TREES.md` § Sizes): the leading people reaches the rim just
  before 400 BCE, the median reaches the settled village.
