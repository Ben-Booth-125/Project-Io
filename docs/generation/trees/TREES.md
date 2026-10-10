# Project Io — Technology Trees

> **Settles:** what a technology tree is, and why there are four · the six domains every tree
> divides into, and the spire beside them · the three kinds of node, the quest major, and what
> each costs · the standard template every tree is cut to — 50 nodes on 4 rings · the five
> adjacency rules · what a milestone is and how a tree unlocks the next · what a fork is · how a
> node's kind decides how it diffuses · who holds a tree and where its research points come from ·
> the shape every tree's scorer shares · the JSON schema and the lint that enforces all of it.
> **Not here:** the nodes of any one tree (EMPIRE_TREE, EXPLORATION_TREE, INDUSTRY_TREE,
> DIGITISATION_TREE) · what produces research points in play, and what besides points research
> consumes (../../economy/RESEARCH) · what a work row or a unit row does once unlocked
> (../../lore/HISTORY, ../MILITARY_HISTORY) · the phases the trees sit inside (../CIVILISATION,
> ../EXPLORATION, ../INDUSTRIALISATION) · what an Era is and what its catastrophe tests
> (../../economy/ERAS).
> **Confused with:** ../../research/ANCIENT_TECH_LADDER.md, ../../economy/RESEARCH.md,
> ../../research/TECH_EFFECTS.md, ../../research/ERA1_TECH_LANDSCAPE.md.

**Four trees, one grammar.** The world passes through four rounds, and each has a technology tree
of its own: **Empire** for the city states, **Exploration** for the exploration age, **Industry**
for Industrialisation, and **Digitisation** for the campaign the player plays. Three are generated
and one is played, and all four are cut to one template (Ben, 2026-10-10). They are four trees
and not four regions of one web (Ben, 2026-09-10, overturning the one-web reading of
`../../research/ANCIENT_TECH_LADDER.md` § Geometry); what they share is everything in this
document, and what differs is in each tree's own document and JSON store.

**The sim reads nodes, not bands (Ben, 2026-09-10).** A polity's technology is the set of nodes
it holds. The seven-domain capacity band that the Era −1 sim carried is a *derived reading* of
that set, kept only where a consumer still wants a scalar — the works roster's band gate, the
unit roster's band boundary, the furnace crossing. Nothing new keys off a band.

| Tree | Round | Played? | How a node is acquired | Document |
|---|---|---|---|---|
| **Empire** | The city states, 400 BCE → 1200 CE (`../CIVILISATION.md`) | Generated | **Invested** — the Invest verb spends the polity's research points on one node, chosen by the tree's scorer | `EMPIRE_TREE.md` · `empire_tree.json` |
| **Exploration** | The exploration age, 1200 → 1660 CE (`../EXPLORATION.md`) | Generated | **Invested**, the same verb | `EXPLORATION_TREE.md` · `exploration_tree.json` |
| **Industry** | Industrialisation, 1660 → 1960 (`../INDUSTRIALISATION.md`) | Generated | **Invested**, the same verb; capacity gates bind here as nowhere else | `INDUSTRY_TREE.md` · `industry_tree.json` |
| **Digitisation** | The campaign, 1960 → (`../../economy/ERAS.md` § Era 1) | **Played** | **Invested** — the player's nation chooses; every other nation's scorer chooses as before | `DIGITISATION_TREE.md` · `digitisation_tree.json` |

**The Colonisation tree is retired (Ben, 2026-10-10).** The migration has no actor and earns no
research points, so a tree it carried could not share this grammar's earning and spending. What a
people carries out of the migration reaches the Empire round through its ground and its culture,
which `../COLONISATION.md` already owns; the Empire tree opens at its root to every polity.

---

## The six domains, and the spire

**Every tree divides into the same six domains (Ben, 2026-10-10).** A domain is a branch that
persists from round to round, so "how strong is this nation at moving things" has one answer
across all four trees — and that answer, read at the handover, is what makes a start asymmetric.

| Key | Domain | What it is about | What it feeds |
|---|---|---|---|
| `RE` | **Reach** | How far a holder can move goods, force and word — roads, hulls, rail, flight, launch | `../../economy/LOGISTICS.md`, `../../economy/SUPPLY.md`; the sim's `reach` and `forage` terms; sea legs and crossings |
| `FO` | **Force** | What a holder can field and what it can hold against — rosters, walls, garrisons, deterrence | `../../military/MILITARY.md`, Alarm (`../../economy/ERAS.md`); `defence`, `manpower`, `muster_cost` |
| `LA` | **Land** | Food and health — the ceiling a people grows toward and how hard sickness draws it down | `../../economy/POPULATION.md`; `carrying_capacity`, `plague` |
| `WO` | **Works** | Materials and energy — what can be made, from what, at what scale | `../../economy/PRODUCTION.md`, `../../economy/RESOURCES.md`; `industrial`; works rows and recipes |
| `XC` | **Exchange** | How goods are named, stored, priced and traded between strangers | `../../economy/MARKETS.md`, `../../economy/CONTRACTS.md`; `stores`; trade capacity |
| `OR` | **Order** | How a holder governs what it holds — law, charter, assimilation, regime | `../../politics/NATIONS.md`; `cohesion`, `assimilation`; institutions and doctrines |
| `SP` | *The spire* | The round's own thesis and clock — see § Geometry | the ring gates, and the next tree |

**Medicine is Land and research is an effect.** The ladder's seven domains fold into six: disease
and food are one question — the ceiling a population climbs toward — and the research rate is not
a concern of its own but an effect any domain may carry (paper, schooling, the university).

**The spire is not a domain.** Each tree's spire is the thesis of its round — Empire's is the
sovereign counting, promising and administering; Exploration's is the purse; Industry's is fire;
Digitisation's is computation — and it is the only column every holder climbs.

---

## Nodes — minor, major, milestone

Three kinds, and the kind fixes cost, diffusion and what the node is allowed to do.

| Kind | What it is | Base cost | May carry | Diffuses as |
|---|---|---|---|---|
| **minor** | A travel node: one small continuous modifier, there so that two majors lie adjacent without a jump | 1 | one `modifier` effect | **practice** — free by contact |
| **major** | A meaning node: the thing a history book would name | 3 | any effects; a `gate`; an `excludes` (a fork) | its own authored class |
| **milestone** | A ring vertex on the spire: a capability regime, named like a quest, whose holding opens the next ring — and, for the last, the next tree | 6 | exactly one `open` effect | **capacity** — never |

**Cost is base × ring.** A node at ring r costs its kind's base times r, in research points
(`capacity_band_cost` × band is this rule under its old name). Kind is the only cost vocabulary.

**Every node justifies itself by feeding conquest, reach, stores or population** — and, in the
played tree, Trade or Conflict. A node whose effect nothing reads is not authored. Continuous
modifiers outnumber unlocks two to one, and that is what the minors are for.

### The quest major

**A quest is a major whose gate reads a deed (Ben, 2026-10-10).** A major's `gate` has always
been an AND on top of availability; an endowment gate reads the ground a holder stands on, and a
**deed gate** reads something the holder has *done* or *become* — staged a crossing, held a seat
through a siege, chartered a firm, held Alarm under Ceiling. It is still a major: same cost, same
diffusion rules, same place in the branch. The deed is what makes it a quest.

Three rules keep a quest a reward rather than a wall:

1. **Every deed is a reading of state the sim already carries.** In generation it is a quantity
   the round's sim holds; in play it is a `condition_set` (`../../META_LAYER.md`). Never a roll,
   never a counter kept only to unlock a node.
2. **A milestone never requires a quest major.** The spire stays climbable by every holder on
   research alone; a deed can make a holder *late* to a branch's rim, never shut out of the next
   ring. That is the same promise § Milestones makes about trees: never excluded, only behind.
3. **The deed is visible before it is met.** A quest renders inside the fog like any reachable
   node, and its gate is legible — the tell precedes the commitment (`../../economy/ERAS.md`
   § Mistakes compound).

---

## The template — 50 nodes on 4 rings

**Every tree is cut to one template (Ben, 2026-10-10).** Fifty nodes, four rings, six domain
branches and the spire.

| Part | Majors | Minors | Milestones | Nodes |
|---|---|---|---|---|
| The spire — one major and one milestone per ring | 4 | — | 4 | 8 |
| Six domain branches × (4 majors + 2 minors) | 24 | 12 | — | 36 |
| Bridges — cross-branch minors at rings 2 and 3, three per ring | — | 6 | — | 6 |
| **Total** | **28** | **18** | **4** | **50** |

**A branch is a major per ring, with its travel in the middle.** Ring 1 holds the branch's root
major; rings 2 and 3 each hold a major and a minor; ring 4 holds the branch's rim major. So the
rings run **8 / 17 / 17 / 8** — narrow at the root where every holder starts, wide in the middle
where branches meet, narrow at the rim where the round's capstones sit.

**Two forks per tree.** A forked branch trades one of its minors for a second major at the same
ring, the pair off one shared minor and each naming the other in `excludes`. The count stays at
fifty: a forked branch holds five majors and one minor. One fork sits at ring 2 and one at ring 3,
so the diamond holds.

**Six quests per tree, one per domain, at rings 3 and 4.** Each is one of its branch's majors
carrying a deed gate. The spire carries none.

**Bridges sit between adjacent wedges.** The six branches are arranged in the order of the domain
table, and a bridge links a major of one branch to a major of its neighbour. Rings 2 and 3 carry
three bridges each, alternating pairs, so every adjacent pair of wedges meets once.

**The full cost is 315 research points' worth of base × ring per tree** (spire 90, branch majors
180, branch minors 30, bridges 15, before a fork's closed side is subtracted). The sizing rule
below is met by pricing a round's research — `capacity_band_cost` per tree and the research
fraction — never by changing the node count.

---

## Geometry — the spire and the branches

**The spire is the round's own clock.** It is a single column from the tree's root outward,
alternating one major and one milestone per ring, and every holder climbs it. A branch leaves the
spire at its ring-1 major, never at a minor, and the spire carries no minors at all.

**Rings are cost, not dates.** Ring 1 is the centre; ring 4 is the rim. A ring's content is ordered
the way history ordered it, but nothing in a ring is dated — the calendar is the round's, and how
fast a holder climbs is the sweep's to measure.

### The five rules

Each is a lint, not a guideline (`node tools/session/tree_lint.js <tree>`).

1. **An edge spans at most one ring.** A prerequisite two rings away gets a minor inserted between
   them. That is what travel nodes are for, and it is the whole answer to "no large jumps".
2. **Travel is OR; meaning is AND.** Owning any linked neighbour makes a node *available*. A
   major's gate — endowment or deed — and a milestone's cross-branch requirement are additional
   AND conditions on top of availability.
3. **Never more than two minors in a row.** A minor exists where two majors would otherwise sit
   two rings or two wedges apart; a chain of three is filler with no job.
4. **A milestone requires majors from at least two branches at its own ring.** This single rule
   pulls a holder toward breadth: the spire cannot be climbed by running one branch to the rim.
5. **A cross-branch link always passes through a minor placed between the wedges, at most one
   per pair of branches per ring.** The interleaving the ladder valued survives — mining begs for
   the engine — and the mesh it feared does not.

Two properties the lint also holds: every node is reachable from the root by links alone, and a
tree holds **at most 64 nodes**. The template's fifty sits under the cap; the cap remains because
a holder's state per tree is one 64-bit mask.

---

## Milestones, and how a tree unlocks the next

**A ring is locked until the milestone below it is held.** Ring r+1's nodes are unavailable to a
holder that does not hold milestone r, whatever it owns adjacent to them. So the milestones are the
tree's gates, and the spire is the only road through them.

**A milestone requires, not links.** Its links are the spire major below it and the spire major
above. Its requirement is `requires_any` — **any two of the six branch majors at its own ring**,
which keeps rule 4's breadth without naming which breadth, so a holder whose ground taught it one
concern is never stranded. `requires_fork` names a fork pair of which either side satisfies; a
fork sitting under a milestone is taken, never dictated. Quest majors are excluded from every
milestone's requirement (§ The quest major).

**The last milestone unlocks the next tree.** The chain is Empire → Exploration → Industry →
Digitisation (Ben, 2026-09-11: *"Exploration and industry use different trees"*; extended to the
played tree 2026-10-10). A polity holding Empire's rim milestone at 1200 CE enters the Exploration
tree at its root the moment that round opens.

**Two trees open to everyone.** The Empire tree opens at its root to every polity, since no tree
precedes it. The Industry tree opens at its root to every living polity at 1660 (Ben, 2026-09-18):
Exploration's rim needs held coast, so entering on that rim would exclude every landlocked polity
for good. Every other tree is entered on the prior rim, and a holder that never reached that rim
starts the next tree **late**, at the root, once it holds the prior milestone — never excluded,
only behind. That is how the opening spread is manufactured: not by denying a tree, but by how
many rounds were left when it opened.

**Named like quests.** A milestone is named for the capability regime beyond it — *The Written
Ledger*, *The Cheap Ton* — and its thesis is one line a reader understands without opening a
single node.

---

## Forks

A fork is **two majors off one shared minor, at the same ring, each naming the other in
`excludes`.** Taking one closes the other's availability window permanently; nothing ever
un-completes, so the held set stays monotonic and the closed side simply goes dark under the fog.
A fork is where a branch's differentiation lives — in the institutional branches almost the only
place it lives, because practice-class nodes flatten into adoption lag rather than absence.

---

## Diffusion follows kind

**Minors diffuse free by contact.** A contact-graph neighbour that holds a minor hands it over at
the receiver's Institutions-scaled rate, for nothing. **Majors diffuse by their authored class** —
`practice` by contact in decades, `artifact` along trade routes at once, `capacity` in generations
or never, needing the gate satisfied locally. A quest major diffuses only to a holder that meets
its deed. **Milestones never diffuse.**

So travel is what your neighbours give you, and meaning is what you pay for. The ladder's rule of
thumb — *artifacts leapfrog, practices follow contact, capacity follows the map* — is a property of
the shape rather than a tag on each node.

**The fog.** A node renders only within reach of a held node, and a closed fork side goes dark
permanently. By the epoch a nation cannot see the tree it did not climb, which is why its 1960
economy tolerates the player's corporation on the terms it does.

---

## Who holds a tree, and where research comes from

**The nation holds technology (Ben, 2026-10-10, overturning the ruling of 2026-09-18 that a nation
held no tech field of its own).** In generation the holder is the polity; at the 1960 handover each
nation inherits the masks of the realm it grew from, and in play it holds the Digitisation tree and
spends on it. How a corporation reaches its nation's technology is
`../../politics/NATIONS.md`'s and `../../economy/RESEARCH.md`'s to settle.

**A holder's tree state is one 64-bit mask per tree and one research-point pool.** Points are
earned into the pool each round and spent by the Invest verb on one frontier node once the pool
covers its cost. The pool belongs to the holder, not the tree: what is left when one tree's rim is
held carries into the next.

**One rate, four inputs.** Every tree earns by the same formula:

```
points per year = input × research fraction
                × (1 + 0.15 × spire ring held)
                × (1 + 0.04 × contact degree, capped at 10)
                × (1 + the research modifier the held set carries)
```

What differs between rounds is only the input, and the input is what that round's people actually
thought with:

| Tree | Input | Why |
|---|---|---|
| Empire | the polity's industry slice of the labour split (`../CIVILISATION.md` § Materials are spent) | research costs labour; a polity that musters hard researches slowly |
| Exploration | the industry slice, as Empire | the purse funds nodes through stores, not through a separate input |
| Industry | urban mass — the three largest held centres' urban population, summed and capped, to the power 1.5 (Ben, 2026-09-18, NR-891) | innovation gathers where people do; one centre would let a sacked capital zero a century |
| Digitisation | universities (`../../economy/RESEARCH.md`) | the first round in which a research point is produced rather than derived |

It costs labour or people, so a starved holder is behind, never broken — the shortfall shape every
goods draw takes. The rate never enters the scorer, which may not carry a term that grows with the
holder's size (§ The scorer).

---

## The scorer — one shape, four trees

**Define the scorer once and "research to spare" falls out.** A frontier node — unheld, available
by rule 2, its gate met, at an unlocked ring — is scored as:

```
score(node) = Σ over effects  value(effect) × binding(term)     the relief a node gives, weighted by how hard that term binds NOW
            + ground(node)                                       the endowment pull: ore to metal, arable to harvest, coast to sail
            + spire(node)                                        progress toward the next milestone
            + known(node)                                        a neighbour already holds it
            − cost(node)
```

A holder at a ceiling buys relief; a comfortable holder has small binding terms, so ground and
spire dominate and it buys what compounds — roads, because holdings grow under them; the research
rate, because the tree is not finished — and what insures, because it can afford to be wrong.

**Every term is an in-world quantity with a visible cause.** A term is a reading of the holder's
state the sim already carries. Never a rank, never a term inside the scorer that grows with the
holder's own size.

**Each tree's document carries the per-node situation** — for every node, the term that makes it
the top pick and the one-line state of the world in which that happens. That table is the scorer's
specification, and the sweep's assertion. In the played tree the scorer chooses for every nation
but the player's.

**Within the grant.** Choosing a node is an argument to the Invest verb the register already covers
(`../../ai/AI_OPPONENT.md` § 11); it issues no new verb and plans nothing. A deed gate is read, never
pursued: the scorer does not take actions to meet a quest's deed.

---

## Effects and the sim's terms

An effect is `(kind, target, per_mille)`. The eleven kinds are `../../research/TECH_EFFECTS.md`'s
closed union — `unlock · upgrade · retire · modifier · access · reach · intel · institution ·
doctrine · resource · open`. A generated tree's `modifier` targets one of the sim's terms:

| Term | What it multiplies | Domain it mostly sits in | Where it lands |
|---|---|---|---|
| `reach` | supply reach of the staging hub, and the burden of breadth | Reach | `reach_mod` |
| `forage` | what a stack finds where it stands | Reach | `../MILITARY_HISTORY.md` § Forage |
| `defence` | readiness on the defender's stack | Force | `defence_mod` |
| `manpower` | the manpower ceiling | Force | `manpower_mod` |
| `muster_cost` | labour a levy takes from the split | Force | the labour split |
| `carrying_capacity` | the asymptote a region's population climbs toward | Land | `capacity_mod` |
| `plague` | resistance to drawdown | Land | `advance_region_demography` |
| `industrial` | the climb toward the furnace crossing | Works | `industrial_mod` |
| `stores` | what a seat holds and how fast it decays | Exchange | the seat's stores (`../CIVILISATION.md`) |
| `cohesion` | recovery on Consolidate | Order | `polity::cohesion_q` |
| `assimilation` | how fast held ground's shares move toward the holder | Order | the assimilation machinery |
| `research` | the research rate itself | any | § Who holds a tree |

The played tree's effects target the campaign's surfaces instead — a recipe (`unlock_recipe`), a
building, a law, a condition another node or quest reads — and `DIGITISATION_TREE.md` names them.

Non-modifier effects target a works row (`unlock`, `scripts/works.lua`), a unit row (`unlock`,
the roster), a verb target class (`access` — a sea leg, a river crossing), or the next ring or
tree (`open`, milestones only). **Magnitudes are authored by judgement and are placeholders**; the
sweep re-prices them.

**How the sim reads an effect.** The generated table carries every node's effects, and one generic
fold (`apply_tree_effects`) turns a holder's masks into its effect surface each round: every
`modifier` sums per term into `polity::tree_mod_q`; an `open` gates the ring or the next tree; a
non-modifier effect the sim gates a verb on carries a machine `key` (`sea_legs`, `post_roads`)
beside its prose target and is read by that key alone. **No node is ever named inside the sim** —
not by index, not by id. A kind or term the sim carries no surface for is declared unread in code
(`tree_effect_declared_unread`) and a harness holds the store to that declaration.

---

## Sizes

The sizing rule: **a tree is sized to the rounds its phase gives a leading holder.** The best holder
in a world finishes the tree just before the round ends; the median reaches halfway. That is an
assertion the sweep makes, and it is met by the research price, not the node count.

| Tree | Rings | Branches | Milestones | Nodes | Forks | Quests |
|---|---|---|---|---|---|---|
| Empire | 4 | 6 + spire | 4 | 50 | 2 | 6 |
| Exploration | 4 | 6 + spire | 4 | 50 | 2 | 6 |
| Industry | 4 | 6 + spire | 4 | 50 | 2 | 6 |
| Digitisation | 4 | 6 + spire | 4 | 50 | 2 | 6 |

**Industry's old ring 1 was the exploration age wearing this tree's costume (Ben, 2026-09-11,
BL-938, the industry tree's exploration ring).** Its sail, navigation and chartered-capital nodes
belong to `EXPLORATION_TREE.md` and must not return when Industry is cut to four rings: Industry's
ring 1 is steam, coke and the machine tool, and its ring 4 is what makes 1960 look like 1960.

**Computing belongs to Digitisation.** Computing and networks, what ordinary speech calls
digitisation, are the played tree's thesis and never the Industry tree's (`../../GLOSSARY.md`,
Industrialisation).

---

## The store — `*_tree.json`

One file per tree, machine-readable, the doc's tables transcribed. `tree_lint.js` cross-checks doc
and store both ways and enforces every rule above, the template's counts included.

```
tree            "empire" | "exploration" | "industry" | "digitisation"
cap             64
branches        [{ key, name, concern }]      key is one of RE FO LA WO XC OR, and SP for the spire
nodes[]
  id            "<EM|EX|IN|DG>-<branch>-<ring><seq>"   e.g. EM-RE-2b; the spire is EM-SP-1a, EM-SP-1m (milestone)
  name          generic mechanism noun, never a proper noun
  kind          minor | major | milestone
  ring          1..4
  branch        branch key
  links         ids of neighbours at ring−1..ring+1 (undirected; list each edge once, on the node
                farther from the root — the root lists none, and that empty list is how the
                generated table derives `is_root`; the lint holds exactly one per tree)
  gate          null | an endowment atom | a deed   (majors only)
  quest         true when `gate` is a deed           (majors only; exactly one per domain branch)
  diffusion     practice | artifact | capacity   (majors only; minors are practice, milestones capacity)
  excludes      null | id   (a fork; symmetric)
  requires_any  {count: 2, of}  (milestones only: branch majors at this ring, quest majors excluded)
  requires_fork [a, b]      (milestones only, optional: a fork pair at this ring, EITHER side held)
  effects       [{ kind, target, per_mille, key }]   key: optional, non-modifier only — the machine name
                the sim reads the effect by (sea_legs, post_roads); an `open` target is "ring N" or
                "<tree> tree"
  pursued_when  { term, situation }   (the scorer's per-node specification)
  earth_ref     calibration only, reader-facing
```

A deed is written as `{ deed, subject, threshold }` — the in-world quantity, whose it is, and the
level that meets it — so the lint can hold every deed to a quantity the round's sim or the campaign's
`condition_set` vocabulary names. Names obey the standing rule without exception: real history is a
mechanism reference, never a name source.

---

## Open questions

- **The deed vocabulary per round.** Which quantities each generated round carries that a deed may
  read, and the threshold each quest sets. A deed the sim has no reading for is not authored.
- **Rocketry as a quest major.** `../../economy/ERAS.md` § Era 2 → Era 3 gate — space access calls Rocketry a
  single unlock with no prerequisite; under this template it is the Reach branch's ring-4 quest in
  the Digitisation tree, its deed the staffed launchpad and the propellant reserve. One of the two
  documents is wrong.
- **What a corporation holds** once the nation holds the tree — whether a firm licenses, inherits or
  is merely permitted its nation's nodes, and whether a firm's spend adds to its nation's pool.
- **The awareness discount.** Whether a major a neighbour already holds costs less, and by how much
  — the `known` term above. A measurement for the sweep, not a judgement.
- **The research fraction.** What share of a round's input research draws. A sweep dial, set against
  the sizing rule rather than picked.
- **Whether the derived band reading survives at all**, or whether the works and unit rosters
  re-key on nodes directly. The former is cheaper; the latter is the honest end state.
