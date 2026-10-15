# Project Io — Research

> **Settles:** who holds technology and how every nation starts · where research points come from
> and what they buy · the shape every age's tech tree shares — four tiers, links that require,
> clusters, pairs, quests that key the next age, placeholders · how a node is priced · the research
> queue, and how an AI nation fills it · how an effect no system reads yet is carried.
> **Not here:** the nodes of one age's tree (tech/DIGITISATION_TREE) · what an Era is and what its
> catastrophe tests (ERAS) · what a recipe or building does once unlocked (PRODUCTION) · which centre
> may host a university (POPULATION) · the national budget the research line sits in
> (../politics/NATIONS).
> **Confused with:** ERAS.md, tech/DIGITISATION_TREE.md, ../generation/trees/TREES.md (retired).

Research is the loop that turns a nation's investment in learning into **technology**: research
points (RP) accrue to a nation and buy nodes of the age's tech tree, one at a time, in the order the
nation's research queue names.

---

## Who holds technology, and where everyone starts

**The nation holds technology (Ben, 2026-10-10).** A nation's tech state is the set of nodes it
holds in the current age's tree, its research-point pool and its queue. A corporation reaches
technology through its nation: what the nation holds, its firms may use.

**Every nation starts the campaign at the same tech level, holding nothing (Ben, 2026-10-10).**
Research is not part of generation: the pre-game history runs no tech trees and spends no research,
so no nation opens ahead of another in technology. A start is asymmetric through its geography, its
history, its population and its corporate web — never through its tech. Every nation is playable,
and every nation opens on the industrial recipe band (`ERAS.md` § Where the ladder starts).

---

## Research points

**RP accrue to the nation each quarter and are spent only on nodes.** Four sources; the rates are
dials:

| Source | What it is |
|---|---|
| **A base** | Every nation earns a fixed RP a quarter whatever its treasury, so every nation reaches its power surges |
| **The research budget line** | The national budget's academic-research priority (`../politics/NATIONS.md`): RP grow with the **square root** of what the line spends, so wealth buys a lead with diminishing returns. The line pays only what the queue still owes; an empty queue spends nothing |
| **Research institutes** | Each staffed research institute adds RP a quarter to its owner's nation |
| **Universities** | Each staffed University in the nation adds RP per quarter. A University may stand only in a City (4)+ centre (`POPULATION.md` § Strata gate buildings), and it also raises the nation's qualification fraction |

**Why a base and a square root (Ben, 2026-10-10).** National treasuries span orders of magnitude.
Research linear in spending let the richest nation buy the whole tree in a few years while half the
field never reached its first major; a base with a concave line keeps every nation on the surge
cadence and lets spending separate nations by a factor of about three, not a hundred thousand.

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
*State* — each a sector of the ring, each split into three **branches**. Rings run from the inside
out and stand for cost: a node on an outer ring costs more. There is no central node; every
branch's first travel node is open from the start.

### Four tiers

| Tier | What it is | Base cost |
|---|---|---|
| **Travel** | A small stacking buff themed to its branch — *+1% extraction rate at every extraction building* — that lengthens the paths between clusters. Joints and masteries are travel nodes too | 1 |
| **Minor** | A narrower buff or a small utility, beside a major | 2 |
| **Major** | A named technology: unlocks a building, a recipe, a unit or a mechanic. The power surge | 6 |
| **Quest** | A deed-gated node: a small bonus now, and the key to the same branch in the next age | 3 |

**Many nodes are small on purpose.** Travel is the bulk of the tree, and a branch's travel nodes
carry the same buff, so a nation that walks a branch feels it stack.

### Links require

**A link is a requirement, and a node with several requirement groups needs all of them (Ben,
2026-10-10).** A node's requirements are groups; each group is satisfied by holding any one of its
members, and every group must be satisfied. So:

- **A major needs both of its minors.** The weight of an important node is the two paths into it.
- **A joint needs both of its neighbouring branches.** Joints sit where two branches — of one tree
  or of two — meet, and buy a combined buff.
- **A mastery needs both of its branch's rim majors**, and caps the branch with its travel buff at
  triple strength.
- Everything else hangs off one parent.

**Branches fan out.** A branch runs travel → its first **cluster** (two minors feeding a major) →
two arms → a cluster at the end of each arm → its mastery. The outer rings hold twice the majors of
the inner, so the tree is widest at the rim.

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
reaches its first ring-2 major in about five years.

**A node costs its tier's base × its ring × the RP unit.** The unit is a dial, set so that a nation
spending a typical research share reaches a power surge — a major — at steady intervals through the
age. That pacing is the property the price is tuned to and the sweep measures (Ben, 2026-10-10:
*"surges in player perceived power levels at similar intervals"*); the node count is never changed
to meet it.

**Prices rise through the age; research points rise to meet them (Ben, 2026-10-15).** A node's
cost climbs with its date in the age, not only its ring. A nation's RP grows with its economy, so
an average nation's unlock cadence stays roughly level from 1960 to the age's close. A nation whose
economy outgrows the average overtakes the price curve and unlocks faster; one that stagnates falls
behind. The speed-up is earned by growth, never handed out by the calendar: *"we don't really want
unlocks to speed up for average games."* The sweep measures the median nation's interval between
majors as flat, and the spread between richest and median as growth-driven.

### What technology is for

Three touchpoints the rest of the game already names: the space-access gate (*Rocketry research
purchased*, `ERAS.md` § Era 2 → Era 3 gate); recipe unlocks (the `unlock_recipe` tech arm); and
province-ceiling relief — deeper mines and denser facilities are the sanctioned reliever of the
building ceiling (`../generation/PROVINCES.md` § The building ceiling).

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

**An AI nation's queue is filled by its scorer (Ben, 2026-10-10).** The scorer appends one frontier
node at a time, chosen by the relief it gives the nation's binding terms; it plans nothing beyond the
next node and issues no verb but *queue*. The grant is recorded in `../ai/AI_OPPONENT.md` § 11.

---

## Effects no system reads yet

**A node's effect is authored in a machine form or declared unread.** Each effect names the lever it
moves — a recipe unlock, a structure unlock, a modifier on a named scalar, a mechanic key. An effect
whose lever no system carries yet is **declared unread** in code, and a harness holds the store to
that declaration, so a node never silently does nothing: it moves its lever or is listed as waiting
for one. Placeholders are the deliberate case of the same rule.

---

## What is open

- The research line's conversion rate, the University's RP per quarter, and the RP unit — dials for
  the sweep, set against the pacing rule.
- The goods basket research draws, and whether it sits on the University or the budget line.
- Whether a corporation can fund research into its nation's pool, or license a node its nation holds.
