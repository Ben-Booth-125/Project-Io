# Project Io — Technology Trees

> **Settles:** what a technology tree is inside generation, and why there are three · the three
> kinds of node and what each costs · the spire, the rings and the branches · the five adjacency
> rules every tree obeys · what a milestone is and how a tree unlocks the next · what a fork is ·
> how a node's kind decides how it diffuses · what a polity's tree state is · where research
> points come from before there is an economy · the shape every tree's scorer shares · the JSON
> schema and the lint that enforces all of it.
> **Not here:** the nodes of any one tree (COLONISATION_TREE, EMPIRE_TREE, INDUSTRY_TREE) · the
> campaign's own research loop once there is a market (../../economy/RESEARCH) · what a work row
> or a unit row does once unlocked (../../lore/HISTORY, ../MILITARY_HISTORY) · the phases the
> trees sit inside (../COLONISATION, ../CIVILISATION, ../GENERATION_STRATEGY) · the 1960+
> campaign tree (../../research/ERA1_TECH_LANDSCAPE).
> **Confused with:** ../../research/ANCIENT_TECH_LADDER.md, ../../economy/RESEARCH.md,
> ../../research/TECH_EFFECTS.md.

**Four trees, one grammar.** Generation runs four simulated spans, and each has a technology
tree of its own: **Colonisation** for the migration, **Empire** for the city states,
**Exploration** for the exploration age, **Industry** for digitisation. They are four trees and
not four regions of one web (Ben, 2026-09-10,
overturning the one-web reading of `../../research/ANCIENT_TECH_LADDER.md` § Geometry — the
constellation, for the pre-game layer). What they share is everything in this document; what differs is in each tree's
own document and its own JSON store.

**The sim reads nodes, not bands (Ben, 2026-09-10).** A polity's technology is the set of nodes
it holds. The seven-domain capacity band that the Era −1 sim carried is a *derived reading* of
that set, kept only where a consumer still wants a scalar — the works roster's band gate, the
unit roster's band boundary, the furnace crossing. Nothing new keys off a band.

| Tree | Phase it runs in | Actor? | How a node is acquired | Document |
|---|---|---|---|---|
| **Colonisation** | The migration, 2400 → 400 BCE (`../COLONISATION.md`) | None, by construction | **Carried** — a people acquires a node by time on ground that satisfies its gate, and a daughter inherits its parent's set at coining | `COLONISATION_TREE.md` · `colonisation_tree.json` |
| **Empire** | The city states, 400 BCE → 1200 CE (`../CIVILISATION.md`) | Yes — the five scored verbs | **Invested** — the Invest verb takes one node a round, chosen by the tree's scorer | `EMPIRE_TREE.md` · `empire_tree.json` |
| **Exploration** | The exploration age, 1200 → 1660 CE (`../EXPLORATION.md`) | Yes | **Invested**, the same verb; the first tree with a treasury to pay for a node | `EXPLORATION_TREE.md` · `exploration_tree.json` |
| **Industry** | Digitisation, 1660 → 1960 (`../DIGITISATION.md`) | Yes | **Invested**, the same verb; capacity gates bind here as nowhere else | `INDUSTRY_TREE.md` · `industry_tree.json` |

The Colonisation tree has no scorer because the migration has no actor and must never grow one.
Its nodes are things a people *carries*, and the tree exists so that what a people carries at
400 BCE is a fact with a history rather than a roll.

---

## Nodes — minor, major, milestone

Three kinds, and the kind fixes cost, diffusion and what the node is allowed to do.

| Kind | What it is | Base cost | May carry | Diffuses as |
|---|---|---|---|---|
| **minor** | A travel node: one small continuous modifier, there so that two majors lie adjacent without a jump | 1 | one `modifier` effect | **practice** — free by contact |
| **major** | A meaning node: the thing a history book would name, the ladder's techs | 3 | any effects; an endowment `gate`; an `excludes` (a fork) | its own authored class |
| **milestone** | A ring vertex on the spire: a capability regime, named like a quest, whose holding opens the next ring — and, for the last, the next tree | 6 | exactly one `open` effect | **capacity** — never |

**Cost is base × ring.** A node at ring r costs its kind's base times r, in the same accumulated
progress the Invest verb already spends (`capacity_band_cost` × band is this rule under its old
name). Kind is the only cost vocabulary; the ladder's S/M/L/XL is retired.

**Every node justifies itself by feeding conquest, reach, stores or population.** A node whose
effect nothing in the sim reads is not authored. The reference finding holds: continuous modifiers
outnumber unlocks two to one, and that is what the minors are for.

---

## Geometry — the spire and the branches

**The spire is the phase's own clock.** It is a single column from the tree's root outward,
alternating one major and one milestone per ring, and every polity climbs it. Colonisation's spire
is the domestication package; Empire's is the sovereign counting, promising and administering;
Industry's is fire — mining to steam to the grid. A branch leaves the spire at a major, never at a
minor, and the spire carries no minors at all.

**Rings are cost, not dates.** Ring 1 is the centre; the rim is the last ring. A ring's content
is ordered the way history ordered it, but nothing in a ring is dated — the calendar is the
phase's, and how fast a polity climbs is the sweep's to measure.

**A branch is a wedge.** Each tree has four or five, one per concern the phase is about, and a
branch is a chain of alternating minors and majors running from its ring-1 major to the rim.

### The five rules

Each is a lint, not a guideline (`node tools/session/tree_lint.js <tree>`).

1. **An edge spans at most one ring.** A prerequisite two rings away gets a minor inserted between
   them. That is what travel nodes are for, and it is the whole answer to "no large jumps".
2. **Travel is OR; meaning is AND.** Owning any linked neighbour makes a node *available*. A
   major's endowment gate, and a milestone's cross-branch requirement, are additional AND
   conditions on top of availability. The ladder's grammar, unchanged.
3. **Never more than two minors in a row.** A minor exists where two majors would otherwise sit
   two rings or two wedges apart; there is no ratio to hit, and a chain of three is filler with
   no job. The reference's two-or-three smalls per notable is what a 64-node cap deliberately
   does not reproduce.
4. **A milestone requires majors from at least two branches at its own ring.** This single rule
   is what pulls a polity toward breadth: the spire cannot be climbed by running one branch to
   the rim.
5. **A cross-branch link always passes through a minor placed between the wedges, at most one
   per pair of branches per ring.** The interleaving the ladder valued survives — mining begs for
   the engine — and the mesh it feared does not.

Two properties the lint also holds: every node is reachable from the root by links alone, and a
tree holds **at most 64 nodes**. The cap is a discipline, not a limit — a polity's state per tree
is one 64-bit mask, the precedent being `region::works_built` with its 32-row ceiling — and a
tree that does not fit is too big.

---

## Milestones, and how a tree unlocks the next

**A ring is locked until the milestone below it is held.** Ring r+1's nodes are unavailable to a
polity that does not hold milestone r, whatever it owns adjacent to them. So the milestones are
the tree's gates, and the spire is the only road through them.

**A milestone requires, not links.** Its `requires` names majors at its own ring from at least two
branches (rule 4); its `links` are the spire major below it and the spire major above. Two softer
forms exist beside the AND: `requires_fork` names a fork pair of which **either** side satisfies,
and `requires_any` names a set of which **any N** satisfy. The second is what a *carried* tree
needs — a people coined on one ground family holds one branch, and a milestone that demanded two
named branches would be unreachable for every people not coined on both; "any two of four" keeps
rule 4's breadth without naming which breadth.

**The last milestone unlocks the next tree.** The chain is Colonisation → Empire → Exploration
→ Industry (Ben, 2026-09-11: *"Exploration and industry use different trees"*). A polity
holding Empire's rim milestone at 1200 CE enters the **Exploration** tree at its root the
moment that phase opens; a polity holding Exploration's rim at 1660 enters the Industry tree;
a culture holding Colonisation's rim milestone at 400 BCE seeds a polity that starts the Empire
tree at its root. A polity that never
reached the rim starts the next tree **late**, at the root, once it holds the prior milestone —
never excluded, only behind. That is how the 1960 spread is manufactured: not by denying a tree,
but by how many rounds were left when it opened.

**Named like quests.** A milestone is named for the capability regime beyond it — *The Written
Ledger*, *The Cheap Ton* — and its thesis is one line a reader understands without opening a
single node. The ladder's vertex quests were exactly this and most of them survive by name.

---

## Forks

A fork is **two majors off one shared minor, at the same ring, each naming the other in
`excludes`.** Taking one closes the other's availability window permanently; nothing ever
un-completes, so the held set stays monotonic and the closed branch simply goes dark under the
fog. A fork is where a branch's differentiation lives — in the institutional branches almost the
only place it lives, because practice-class nodes flatten into adoption lag rather than absence.

A milestone may require that a fork be *taken* — either side satisfies it — which makes the
choice unavoidable and never dictated. That is the house rule for a fork sitting under a
milestone.

---

## Diffusion follows kind

**Minors diffuse free by contact.** A contact-graph neighbour that holds a minor hands it over at
the receiver's Institutions-scaled rate, for nothing. **Majors diffuse by their authored class** —
`practice` by contact in decades, `artifact` along trade routes at once, `capacity` in generations
or never, needing the gate satisfied locally. **Milestones never diffuse.**

So travel is what your neighbours give you, and meaning is what you pay for. The ladder's rule of
thumb — *artifacts leapfrog, practices follow contact, capacity follows the map* — is now a
property of the shape rather than a tag on each node.

**The fog.** A node renders only within reach of a held node, and a closed fork side goes dark
permanently. By the epoch a nation cannot see the tree it did not climb, which is why its 1960
economy tolerates the player's corporation on the terms it does.

---

## State, and where research comes from

**A polity's tree state is one 64-bit mask per tree**, plus one accumulated-progress integer for
the node it is currently investing in. For the Colonisation tree the holder is the **culture**, not
a polity, because the migration has no polities; a daughter copies the mask at coining.

**Research is a flow derived from the labour split, never a resource.** `../CIVILISATION.md`
§ Materials are spent divides a region's labour three ways — subsistence, industry, muster. A
polity's research rate is a **fraction of its industry slice**, scaled by the spire ring it holds
and by its contact degree. It costs labour, so a polity that musters hard researches slowly; a
polity with nothing binding it researches fastest; and a starved one is behind, never broken —
the same shortfall shape every goods draw in the campaign takes.

Before universities and printing the concept of a research point is anachronistic, which is why it
is a derived flow here and a produced resource only once `../../economy/RESEARCH.md` has a
university to produce it.

---

## The scorer — one shape, four trees

**Define the scorer once and "research to spare" falls out.** A frontier node — unheld, available
by rule 2, at an unlocked ring — is scored as:

```
score(node) = Σ over effects  value(effect) × binding(term)     the relief a node gives, weighted by how hard that term binds NOW
            + ground(node)                                       the endowment pull: ore to metal, arable to harvest, coast to sail
            + spire(node)                                        progress toward the next milestone
            + known(node)                                        a neighbour already holds it
            − cost(node)
```

A polity at a ceiling buys relief; a comfortable polity has small binding terms, so ground and
spire dominate and it buys what compounds — roads, because holdings grow under them; the research
rate, because the tree is not finished — and what insures, because it can afford to be wrong. No
special case is needed for a surplus.

**Every term is an in-world quantity with a visible cause.** A term is a reading of the polity's
state the sim already carries: how far past `free_holdings` it is, how close its ground sits to
carrying capacity, how low the seat's stores or cohesion are, what a neighbour's power and grudge
say. Never a rank, never a term inside the scorer that grows with the polity's own size.

**Each invested tree's document carries the per-node situation** — for every node, the term that
makes it the top pick and the one-line state of the world in which that happens. That table is
the scorer's specification, and the sweep's assertion.

**Within the grant.** Choosing a node is an argument to the Invest verb the register already
covers (`../../ai/AI_OPPONENT.md` § 11); it issues no new verb and plans nothing. Pathing at a
fork is the seeded, scored choice the ladder's geometry already ruled.

---

## Effects and the sim's terms

An effect is `(kind, target, per_mille)`. The eleven kinds are `../../research/TECH_EFFECTS.md`'s
closed union — `unlock · upgrade · retire · modifier · access · reach · intel · institution ·
doctrine · resource · open`. A `modifier`'s target is one of the sim's terms:

| Term | What it multiplies | Where it lands today |
|---|---|---|
| `reach` | supply reach of the staging hub, and the burden of breadth | `reach_mod` |
| `carrying_capacity` | the asymptote a region's population climbs toward | `capacity_mod` |
| `manpower` | the manpower ceiling | `manpower_mod` |
| `defence` | readiness on the defender's stack | `defence_mod` |
| `industrial` | the climb toward the furnace crossing | `industrial_mod` |
| `stores` | what a seat holds and how fast it decays | the seat's stores (`../CIVILISATION.md`) |
| `cohesion` | recovery on Consolidate | `polity::cohesion_q` |
| `assimilation` | how fast held ground's shares move toward the holder | the assimilation machinery |
| `plague` | resistance to drawdown | `advance_region_demography` |
| `research` | the research rate itself | this document, § State |
| `forage` | what a stack finds where it stands | `../MILITARY_HISTORY.md` § Forage |
| `muster_cost` | labour a levy takes from the split | the labour split |

Non-modifier effects target a works row (`unlock`, `scripts/works.lua`), a unit row (`unlock`,
the roster), a verb target class (`access` — a sea leg, a river crossing), or the next ring or
tree (`open`, milestones only). **Magnitudes are authored by judgement and are placeholders**,
exactly as the works magnitudes are; the sweep re-prices them.

**How the sim reads an effect.** The generated table (`gen_empire_tree_table.js`) carries every
node's effects, and one generic fold (`apply_tree_effects`) turns a polity's held masks into its
effect surface each round: every `modifier` sums per term into `polity::tree_mod_q`; an `open`
gates the ring or the next tree; a non-modifier effect the sim gates a verb on carries a machine
`key` (`sea_legs`, `post_roads`) beside its prose target and is read by that key alone. **No node
is ever named inside the sim** — not by index, not by id. A kind or term the sim carries no
surface for is declared unread in code (`tree_effect_declared_unread`) and a harness holds the
store to that declaration, so a new kind cannot be authored into a store and silently do nothing.

---

## Sizes

The sizing rule: **a tree is sized to the rounds its phase gives a leading polity.** The best
polity in a world finishes the tree just before the phase ends; the median reaches halfway. That
is an assertion the sweep makes, not a number anyone picks.

| Tree | Rings | Branches | Milestones | Target nodes | Cap |
|---|---|---|---|---|---|
| Colonisation | 3 | 4, one per origin farm class | 3 | ~28 | 64 |
| Empire | 4 | 4 | 4 | ~48 | 64 |
| Exploration | 3 | 4 | 3 | 31 | 64 |
| Industry | 3 | 5 | 3 | ~44 | 64 |

One milestone per ring follows from the spire's shape (one major and one milestone per ring); the
node targets follow from the branch × ring × (one major + one-and-a-half minors) arithmetic plus
the spire. Where a tree comes in under target, that is a sparse branch doing what the ladder's
sparse-sector rule was adopted for.

**Industry's old ring 1 was the exploration age wearing this tree's costume (Ben, 2026-09-11,
BL-938).** *Full-Rigged Ship*, *Celestial Navigation*, *Joint-Stock & Public Credit*, *Chartered
Capital* and the rest of that ring existed here only because no Exploration tree existed when this
one was first authored; two of them duplicated `EXPLORATION_TREE.md` nodes outright. The ring was
removed rather than trimmed, every remaining ring shifted down one, and **ring 4's content survives
whole as the new ring 3** — electrification, oil, flight, broadcast and antibiotics, the things
that make 1960 look like 1960. Industry is now 3 rings of 45 nodes; `INDUSTRY_TREE.md` carries the
detail of what moved, what duplicated Exploration, and what did not survive the ring's removal.

---

## The store — `*_tree.json`

One file per tree, machine-readable, the doc's tables transcribed. `tree_lint.js` cross-checks doc
and store both ways and enforces every rule above.

```
tree            "colonisation" | "empire" | "industry"
cap             64
branches        [{ key, name, concern }]      key is two upper-case letters; "SP" is the spire
nodes[]
  id            "<EM|CO|IN>-<branch>-<ring><seq>"   e.g. EM-RD-2b; the spire is EM-SP-1, EM-SP-1m (milestone)
  name          generic mechanism noun, never a proper noun
  kind          minor | major | milestone
  ring          1..n
  branch        branch key
  links         ids of neighbours at ring−1..ring+1 (undirected; list each edge once, on the node
                farther from the root — the root lists none, and that empty list is how the
                generated table derives `is_root`; the lint holds exactly one per tree)
  gate          null | one of gate_atoms  (majors only)
  diffusion     practice | artifact | capacity   (majors only; minors are practice, milestones capacity)
  excludes      null | id   (a fork; symmetric)
  requires      [ids]       (milestones only: majors at this ring from ≥2 branches, all held)
  requires_fork [a, b]      (milestones only, optional: a fork pair at this ring, EITHER side held)
  requires_any  {count, of} (milestones only, optional: ANY `count` of the majors in `of`, all at this ring)
  effects       [{ kind, target, per_mille, key }]   key: optional, non-modifier only — the machine name
                the sim reads the effect by (sea_legs, post_roads); an `open` target is "ring N" or
                "<tree> tree"
  pursued_when  { term, situation }   (invested trees only — the scorer's per-node specification)
  earth_ref     calibration only, reader-facing
```

Node ids carry branch and ring so that a table row is checkable against its store entry by eye;
the lint checks that the fields agree with the id. Names obey the standing rule without exception:
real history is a mechanism reference, never a name source.

---

## Open questions

- **The awareness discount.** Whether a major a neighbour already holds costs less to invest in,
  and by how much — the `known` term above. A measurement for the sweep, not a judgement.
- **Whether a Colonisation node is held per culture or per region.** Per culture is the cheaper
  and the design lean; per region would let a practice be lost when ground is re-wilded.
- **The research fraction.** What share of the industry slice research draws, per spire ring. A
  sweep dial, to be set against the sizing rule rather than picked.
- **Whether the derived band reading survives at all**, or whether the works and unit rosters
  re-key on nodes directly. The former is cheaper; the latter is the honest end state.
