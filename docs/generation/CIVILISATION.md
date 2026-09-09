# Project Io — Civilisation

> **Settles:** what the Empires phase is and what it grows from · what a settlement is once
> centres become sparse · how materials are spent without a market · where armies come from
> and which peoples raise them · what a civilisation is, as distinct from a creed · how two
> cultures come to be similar or opposed, and why that is the engine of conquest · what the
> Culture phase must hand forward for any of it to work.
> **Not here:** how people came to live where they live (COLONISATION) · how force resolves
> once two polities contest ground (MILITARY_HISTORY) · what a pantheon is or how a tongue is
> coined (../lore/CREEDS) · the stage ladder this phase sits inside (../lore/HISTORY) · what a
> centre consumes once the campaign starts (../economy/POPULATION).
> **Confused with:** COLONISATION.md, ../lore/HISTORY.md, MILITARY_HISTORY.md.

**How city states become empires.**

`COLONISATION.md` owns the span in which humanity spreads into empty ground, and it ends when
the filling ends. This document owns what happens next: the ground is taken, the peoples are
many, and the interesting question stops being *who arrives* and becomes *who takes what from
whom*.

**The premise, in Ben's words (2026-09-09):** the Empires phase is about *"growing city states
into empires, expanding ancient logistics networks, and abstracting the use of natural resources
for conquest."*

**THE PHASES ARE NOT CONTINUOUS** (`../ui/STARTUP.md` § Rounds — System, Life, Culture, Empires, Industrialisation). Migration and empire have
different subjects, different rules and different terminating conditions. What crosses between
them is a **stated handoff** — see § What the Culture phase must hand forward — and not an
assumption that whatever happens to be in memory will do.

---

## The unit is the city state, and settlements are sparse

**Ben, 2026-09-09: population centres become settlements, and settlements are SPARSE.**

Today every region carries an urban record and centres are drawn broadly across the map
(`../economy/POPULATION.md` § Generation). That is the right model for a *populated* world and
the wrong one for a world of city states: if everywhere is a settlement, nowhere is a seat, and
an empire is just a larger blob of the same stuff.

**Sparse makes the map legible and makes conquest mean something.** A settlement is a place
worth taking — it holds the stores, it is where the muster forms, and taking it is how a war
ends. Ground between settlements is *hinterland*: it feeds a seat, it is walked through, and it
changes hands when its seat does.

**A city state is therefore a settlement plus the hinterland that feeds it**, and an empire is a
city state that holds other settlements. That is the whole political ladder for this phase, and
it needs no new actor type — the sim's `polity` already is one.

---

## Materials are spent when something happens

**Ben, 2026-09-09: materials are spent when actions happen — NOT via markets or buildings.**

This phase has no economy in the campaign sense and must not grow one. There is no price, no
order book, no firm and no building stack; `../economy/MARKETS.md` and `../economy/PRODUCTION.md`
own those and they belong to a later era. What this phase has is **stores and expenditure**: a
polity holds materials, and an action consumes them.

**The equilibrium is the design, and it is a consequence rather than a dial.** Ben's shape: *a
high-population province must reserve population for work in industry to feed the settlements,
raising a natural equilibrium between civilian and military strength.* So a people's labour
divides three ways — subsistence, industry, and muster — and the division is forced by what the
ground and the population can bear, never chosen by an agent.

That is what makes conquest **cost** something without an economy: an army in the field is
labour not farming and not making, so a polity that musters too hard starves or stops producing.
The brake on empire is not a rule that says empires stop; it is that people can only do one
thing at a time.

**It is an in-world force with a visible cause**, which is the standing requirement for any brake
in this project — never a term inside an agent's head.

---

## Armies come from creeds, and only some peoples raise them

**Ben, 2026-09-09: depending on their creed, some cultures grow armies and begin to conquer.**

The input already exists and is already earned: `culture::aggression_q` is derived from the
pantheon's war god (`../lore/CREEDS.md`), and `COLONISATION.md` § Culture arrives by route made
the distribution of pantheons a **record of routes** rather than a scatter. So which peoples
turn warlike is downstream of where their ancestors walked, which is exactly the ordering the
colonisation design set out to establish: *doctrine is downstream of how a people came to live
where it lives.*

**NOT EVERY PEOPLE CONQUERS, AND THAT IS THE POINT.** A world where every culture musters is as
flat as one where none does. The asymmetry is the deliverable
(`GENERATION_STRATEGY.md` § Asymmetry is the deliverable).

> **THIS PHASE HAS ACTORS, AND THAT IS A REAL DIFFERENCE FROM COLONISATION.** The migration span
> has no actor by construction, and `COLONISATION.md` is emphatic that it must never grow one.
> This phase does: a polity choosing to campaign *is* a decision. **That is not a new AI grant** —
> it is the Era −1 sim's existing scored-utility verb set, already covered by the dated register
> in `../ai/AI_OPPONENT.md` § 11, and it stays bound by the same constraints: pure, seeded,
> deterministic, replayable, issuing only legal verbs, never a planner. **Anything this phase
> wants beyond those five verbs is a NEW widening and is Ben's to give** — raised, never assumed.

---

## A civilisation is what mixing makes, and it is not a creed

**Ben, 2026-09-09: some civilisations form, whereby mixing cultures gives rise to philosophy and
ethics, as removed from the initial religion-based creeds.**

A **creed** is what a people brought with them: a pantheon coined at a cradle, carried along a
migration route, inherited by every daughter. A **civilisation** is what happens when peoples
*mix* — it is second-order, it belongs to no single culture, and it is about how one ought to
live rather than which gods there are.

**The distinction is load-bearing.** Creeds are the migration's output and they are already
built. Civilisations are the empire phase's output and they need mixing to exist at all, which
is why they cannot be generated at a cradle: there is nothing yet to mix.

**Where the mixing already is:** `region::culture` is a `culture_shares` distribution, and the
assimilation machinery already moves it when ground changes hands. A region carrying two peoples
in quantity, for a long time, is precisely the condition a civilisation should arise from.

**What is NOT settled** and should not be guessed at build time: whether a civilisation is a
named record like a culture, a set of axes over the cultures that compose it, or a lens on the
existing shares. What it means for a polity to *belong* to one. Whether a civilisation can
outlive the polities that formed it.

---

## Culture relations — the design aside

**Ben, 2026-09-09: cultures have similarities, and sometimes directly opposing views. This leads
to conflict, and it becomes the engine for our simulated city states to begin an idea of
conquest.**

This is the section that decides whether the empire phase has anything to be *about*, and it is
the one opened as an aside rather than settled.

### The substrate exists and is currently thrown away

**The migration already builds a family tree of peoples.** `BL-856` coins each daughter culture
from a named parent, and `colonisation_field::spawns` records every `(culture, parent)` edge —
then discards it. `creed_state::culture` has no parent field, so by the time the empire phase
runs, the fact that two peoples are cousins is **unrecoverable**.

**RETAINED 2026-09-09 (BL-865).** `culture` now carries its `parent` and the `origin_farm_class`
it was coined on, and the tree has real depth: deepest descent **9 and 10** across 676 and 929
cultures on seeds 0 and 1, every one walking back to a cradle. That depth is what makes kinship
worth reading — a flat set of siblings off twelve cradles would carry no information. Kinship distance in that tree is then a similarity measure that is *earned by the
migration* rather than rolled — two peoples are alike because they share an ancestor and parted
recently, which is a fact about the world's history rather than a number assigned to it.

### Similarity is kinship; opposition needs a second axis

Kinship alone gives *distant* and not *opposed*, and those are different. Two peoples on opposite
sides of a continent who have never met are distant and have no quarrel. Two who share a frontier
and disagree about how to live have one.

**Three candidate axes for opposition, all from quantities that already exist:**

- **The pantheon's own temperament.** `culture_god` carries `zeal` and `dominion`; a people whose
  war god relishes battle and expects to prevail sits opposite one whose does neither. Already
  built, already earned.
- **The country a people was coined on.** BL-864 makes a daughter *of* the farm class it settled,
  so a people of the floodplain and a people of the highlands differ in how they live and what
  they need — which is a material disagreement rather than a stated one.
- **Contact itself.** `culture_shares` records interpenetration at every founding, and
  `BL-852` already reads it for fragmentation. Peoples who mix heavily are not the same as peoples
  who merely border.

### The open calls

None of these should be answered at build time:

1. **Is opposition symmetric?** A grudge is directed (`grudge` is a directed table); a
   disagreement about how to live may not be. Whichever is chosen decides whether relations are a
   matrix or a set of pairs.
2. **Does kinship decay, or is the tree enough?** Two peoples five generations apart may be as
   foreign as two unrelated ones, or kinship may hold indefinitely.
3. **SETTLED (Ben, 2026-09-09): opposition PERMITS conquest, it does not cause it.** Relations
   feed the existing `w_cult` weight rather than raising a score of their own. One scorer,
   measurable against the sweep already in place, and raisable to *cause* later if the histories
   come out flat — whereas a second scorer is hard to remove once other things read it.
4. **How does a civilisation relate to the cultures inside it?** If two opposed peoples end up in
   one civilisation, does the civilisation resolve the opposition, inherit it, or fracture?

---

## What the Culture phase must hand forward

This is the data contract, and it is the reason this document exists before sprint 38 rather
than during it. `pass_one_output` (`history_sim.hpp`) is the stated handoff and it should carry
what the empire phase actually needs, rather than the empire phase reaching into whatever
survived.

| What | Where it is now | State |
|---|---|---|
| The region table, with culture shares | `pass_one_output::regions` | **carried** |
| Polities, with capacity and cohesion | `pass_one_output::polities` | **carried** |
| Cultures: pantheon, tongue, aggression | `creed_state::cultures` | **carried** |
| Which provinces each polity holds | `pass_one_output::holdings` | **carried** |
| Directed grudges | `pass_one_output::grudges` | **carried** |
| Region endowment (farm / ore / energy / port) | `region` | **carried** |
| **Culture PARENTAGE — the family tree** | `colonisation_field::spawns` | **DISCARDED — the gap** |
| **The farm class a culture was coined on** | `front_entry::origin_class` | **DISCARDED** |
| Settlement seats, once centres are sparse | `region::centres` (dense today) | **owed, § The unit is the city state** |
| Material stores per polity | — | **owed, § Materials are spent** |

**Two of those are discarded facts the migration already computed**, and both are cheap to
retain. Everything else is either carried today or is honest new work for sprint 38.

---

## Open questions

- **Everything in § Culture relations — the design aside.** Four calls, none of them work.
- **What a settlement IS**, once centres are sparse: a threshold on the existing urban record, a
  separate record, or a promotion of certain regions.
- **Whether materials are per polity or per province.** Ben's phrasing — *a high population
  province must reserve population for work in industry* — reads as per province, which makes
  the labour split a property of ground rather than of a realm.
- **What a civilisation is as a record**, per § A civilisation is what mixing makes.
- **Whether logistics reach bounds conquest in this phase.** `BL-837` (ancient logistics and
  roads) is open and is where "expanding ancient logistics networks" lands; whether reach *gates*
  a campaign or merely prices it is unsettled.
