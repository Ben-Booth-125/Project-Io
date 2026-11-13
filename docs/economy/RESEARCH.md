# Project Io — Research

> **Settles:** who holds technology and how every nation starts · where research points come from
> and what they buy · the shape every age's tech tree shares — four tiers, links that require,
> clusters, pairs, quests that key the next age, placeholders · how a node is priced · the research
> queue, and how an AI nation fills it · how an effect no system reads yet is carried.
> **Not here:** the nodes of one age's tree (tech/DIGITISATION_TREE) · what an Era is and what
> opens the next (ERAS) · what a recipe or building does once unlocked (PRODUCTION) · which centre
> may host a university (POPULATION) · the national budget (../politics/NATIONS), which buys no
> research.
> **Confused with:** ERAS.md, tech/DIGITISATION_TREE.md, ../generation/trees/TREES.md (retired).

Research is the loop that turns a nation's learning into **technology**: research points (RP)
accrue to a nation and buy nodes of the age's tech tree, one at a time, in the order the nation's
research queue names.

---

## Who holds technology, and where everyone starts

**The nation holds technology (Ben, 2026-10-10).** A nation's tech state is the set of nodes it
holds in the current age's tree, its research-point pool and its queue. What the nation holds, its
buildings may use: every building is the nation's (Ben, 2026-10-23).

**Every nation starts the campaign at the same tech level, holding nothing (Ben, 2026-10-10).**
Research is not part of generation: the pre-game history runs no tech trees and spends no research,
so no nation opens ahead of another in technology. A start is asymmetric through its geography, its
history, its population and its neighbours — never through its tech, and, for a major, never
through its opening kit (Ben, 2026-11-02; `../generation/CORPORATION_GENERATION.md` § A major's
opening kit). Every nation is playable,
and every nation opens on the industrial recipe band (`ERAS.md` § Where the ladder starts).

---

## Research points

**RP accrue to the nation each month and are spent only on nodes. Research is free; a research
building is what it costs (Ben, 2026-11-13).** Three sources, and no credit buys RP:

| Source | What it is |
|---|---|
| **A base** | Every nation earns **100 RP a month**, whatever its treasury or size. Every nation starts equal (§ Who holds technology) and reaches its power surges on its own |
| **Research institutes** | Each research institute the nation runs adds **5% of the base**, 5 RP a month |
| **Universities** | Each University the nation runs adds **10% of the base**, 10 RP a month. A University may stand only in a City (4)+ centre (`POPULATION.md` § Strata gate buildings), and it also raises the nation's qualification fraction |

A building adds its share only while it runs: staffed and not idled, paying its maintenance and wages as
any building does.
**That upkeep is the price of research.** A building's share is small against its running bill, so
research stays expensive at the margin while the base keeps every nation on the tree.

**Why free, and why no economy source (Ben, 2026-11-13; superseding the 2026-10-10 square-root line
and the 2026-10-15 economy source).** RP bought with treasury drained majors that earned less than
their research line asked, so a nation ran its savings down on learning it could not afford. A free
base cannot drain a treasury. Wealth no longer buys a research lead at all: nations separate by the
research buildings they choose to keep, and by the techs that raise national research. The economy
source is dropped with the line, so a growing economy does not research faster (§ Cost).

A held node's research modifier (*+N% national research*) scales the total. RP never trade between
nations.

**Research consumes goods, not only credits** (Ben, 2026-08-26): research is a demand channel
([`MARKETS.md`](MARKETS.md) § Demand channels). RP accrual draws an era-banded basket of real goods,
which gives the top of the production chain a buyer and makes the tech ladder an economic decision
rather than a free accumulator. **A shortfall slows research; it does not void it** — the shortfall
shape every goods draw takes (`FINANCE.md`). The basket's design is BL-645 (research consumes
goods).

**Deterministic like everything else.** RP accrual, spending and the queue are seeded, pure and
replayable; an AI nation researches through the same verbs and the same arithmetic as the player.

---

## The shape of an age's tree

One tree per age, cut to one grammar (Ben, 2026-10-10). The Digitisation tree is the first
(`tech/DIGITISATION_TREE.md`); the next age's tree shares this grammar and is keyed by the
Digitisation tree's quests.

**Three trees in a ring, no centre.** An age's tree is three trees — *Production*, *Network*,
*State* — each a sector of the ring, each split into **branches**: three each, and a fourth in State,
**Naval** (Ben, 2026-11-05). Rings run from the inside
out and stand for cost: a node on an outer ring costs more. There is no central node; every
branch's first travel node is open from the start.

### Four tiers

| Tier | What it is | Base cost |
|---|---|---|
| **Travel** | A small stacking buff themed to its branch — *+1% extraction rate at every extraction building* — that lengthens the paths between clusters. Joints and masteries are travel nodes too | 1 |
| **Minor** | A narrower buff or a small utility, beside a major — or a kit item (Ben, 2026-11-03) | 2 |
| **Major** | A named technology: unlocks a building, a recipe, a unit, a mechanic or a cluster's surge kit item. The power surge | 6 |
| **Quest** | A deed-gated node: a small bonus now, and the key to the same branch in the next age | 3 |

**Many nodes are small on purpose.** Travel is the bulk of the tree, and a branch's travel nodes
carry the same buff, so a nation that walks a branch feels it stack.

### Links require

**A link is a requirement, and a node with several requirement groups needs all of them (Ben,
2026-10-10).** A node's requirements are groups; each group is satisfied by holding any one of its
members, and every group must be satisfied. So:

- **A major needs both of its minors.** The weight of an important node is the two paths into it.
  A major off the spine with no cluster of its own takes those two paths from the branch's two arms,
  both of their first nodes, and needs both. Wind Power is the one such major.
- **A joint needs both of its neighbouring branches.** Joints sit where two branches — of one tree
  or of two — meet, and buy a combined buff.
- **A mastery needs both of its branch's rim majors**, and caps the branch with its travel buff at
  triple strength.
- Everything else hangs off one parent.

**Branches fan out.** A branch runs travel → its first **cluster** (two minors feeding a major) →
two arms → a cluster at the end of each arm → its mastery. The outer rings hold twice the majors of
the inner, so the tree is widest at the rim.

**Kit clusters lengthen the arms (Ben, 2026-11-03).** A branch that carries military kit is wider
than the seventeen-node spine — the Digitisation tree's Force holds 32 nodes, Signal & Orbit 29,
Processing 23. Each kit cluster is two minors feeding a major, laid in line along an existing arm:
its minors hang off the arm node before them, and the arm's next node hangs off its major, so the
arm's rim cluster sits later. A kit cluster keeps the two-minors rule and is never a pair. **No kit
node requires a node of another branch**: the tie between kit branches is economic, later kit
drawing electronics off the shelf. A kit node opens one technology's item in every class it fits.

### Pairs

**A pick-one pair is two majors in one cluster, each excluding the other.** Taking one closes the
other permanently. A pair cluster has one minor rather than two. Pairs are where an Era's traps live
(`ERAS.md` § Mistakes compound): the escalator against its stabilising twin, autarky against
interdependence, order now against unrest later.

### Quests key the next age

**Every branch carries one quest, at whichever cluster suits it** — not necessarily its last (Ben,
2026-10-10). A quest needs its cluster's major, its RP cost, and a **deed**: something the nation has
done or become, read from state the game already carries (`../META_LAYER.md`, `condition_set`).
Holding it gives a small bonus now and **unlocks the same branch in the next age's tree**, which is
otherwise locked. A deed is visible before it is met.

### Placeholders

**A placeholder cluster reserves room for a system the tree is designed to grow into (Ben, 2026-10-10).** It has a
cluster's shape — two minors and a major — no effect, and it names the system that will fill it
(espionage, politics). It may be researched and does nothing until that system exists. A mastery
whose branch carries a placeholder needs only the branch's real rim major until the system lands.

### Cost

**An age lasts about eighty years (Ben, 2026-10-10)**, and the tree is priced to it: the richest
nation, researching without pause, completes the whole tree inside the age, and the median nation
reaches its first ring-2 major in about four years, its fastest tenth in no less than about a year
and a quarter (Ben, 2026-11-10: re-pinned from five years once nations owned their ground).

**Research costs three times the 2026-10-10 price (Ben, 2026-11-11: *"the game's pacing is far too fast"*).** The RP unit is tripled in the same change that makes the tick a month; RP accrual keeps its per-year value. The age's length and the first-major interval are re-measured against the new price, and the figures in this section are the measured ones.

**Measured at the tripled price, with credits still buying RP (2026-11-12, the sixteen library seeds, 577 nations with a source, 240 live months):** the median nation reaches its first ring-2 major in about **ten years** (124 months), its fastest tenth in about **four** (50 months), and about a quarter of nations (159 of 577) do not reach one inside twenty years. The four-year median and year-and-a-quarter floor above are the quarter tick's pins at the old price; they are not re-pinned here, because what the new pacing should be is Ben's to rule.

**A node costs its tier's base × its ring × the RP unit.** The unit is a dial, set so that a nation
spending a typical research share reaches a power surge — a major — at steady intervals through the
age. That pacing is the property the price is tuned to and the sweep measures (Ben, 2026-10-10:
*"surges in player perceived power levels at similar intervals"*); the node count is never changed
to meet it. **Kit makes an age slower, and is not priced away (Ben, 2026-11-03):** the RP unit is
not re-tuned to absorb the cost the kit clusters add.

**Prices rise through the age, and research points do not rise to meet them (Ben, 2026-11-13;
superseding the 2026-10-15 economy source).** A node's cost climbs with its date in the age, not
only its ring, while the base is fixed. So unlocks slow as the age runs on, for every nation alike,
and a nation keeps pace only by adding research buildings or the techs that raise national research.
Unlocks still never speed up by the calendar alone: *"we don't really want unlocks to speed up for
average games"* (Ben, 2026-10-15).

### What technology is for

Three touchpoints the rest of the game already names: the space-access gate (*Rocketry research
purchased*, `ERAS.md` § Era 1 → Era 2 gate); recipe unlocks (the `unlock_recipe` tech arm); and
province-ceiling relief — deeper mines and denser facilities are the sanctioned reliever of the
building ceiling (`../generation/PROVINCES.md` § The building ceiling).

**Military kit is researched in the age tree (Ben, 2026-10-27)** — rifles, launchers, guns, armour,
sensors and drones are nodes of the tree like any other technology, not a ladder of their own
(`../military/MILITARY.md` § Land — the classes). A kit item is what a unit's design may take; a
node that opens one names it as its unlock (`unlock_kit`), and the kit catalogue names the node —
two records of one link, held equal by the tree's check (Ben, 2026-11-03).

---

## The research queue

**A nation researches from a queue it can plan ahead (Ben, 2026-10-10).**

- **Anything may be queued**, however far out. Queuing a node queues its unheld requirements ahead
  of it — every group, the cheapest member where a group offers a choice.
- **RP flow to the head of the queue**; a node completes when its cost is met, and the remainder
  flows on to the next entry. With nothing queued the pool accrues and waits. The pool *is* the
  head's progress — RP the head has not consumed stay in the pool — so reordering the queue never
  strands research.
- **A quest whose deed is unmet is passed over, not blocking**: RP flow past it to the next entry,
  and it completes when its deed is met and its cost is paid.
- **Pick-one pairs are guarded**: a queue holding one side refuses the other.
- **Every entry shows its arrival date** at the current research rate.
- **The next age's tree shares the one queue** (BL-1612, early space researchable). Once its gate
  is met and a branch is keyed, that branch's nodes queue **behind every entry of the current age's
  tree**, and the one
  pool reaches them only when no current-age entry waits on funding. Remove last, Clear and the
  arrival dates read the whole queue, the next age's tail included. A next-age entry whose gate
  lapses is passed over, not blocking, like a quest waiting on its deed.

**An AI nation's queue is filled by its scorer (Ben, 2026-10-10).** The scorer appends one frontier
node at a time, chosen by the relief it gives the nation's binding terms; it plans nothing beyond the
next node and issues no verb but *queue*. Once the nation meets the next age's gate, that tree's open
nodes are weighed beside the current tree's by the same rule, and a tie keeps the current tree's
node. The grant is recorded in `../ai/AI_OPPONENT.md` § 11.

---

## Effects no system reads yet

**A node's effect is authored in a machine form or declared unread.** Each effect names the lever it
moves — a recipe unlock, a structure unlock, a modifier on a named scalar, a mechanic key. An effect
whose lever no system carries yet is **declared unread** in code, and a harness holds the store to
that declaration, so a node never silently does nothing: it moves its lever or is listed as waiting
for one. Placeholders are the deliberate case of the same rule.

---

## What is open

- The RP unit — a dial for the sweep, set against the pacing rule under the free base.
- The goods basket research draws, and which research building draws it.
