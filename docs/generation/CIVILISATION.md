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

## The span is 400 BCE to 1200 CE

**SETTLED (Ben, 2026-09-09, elicitation; boundary and total revised by Ben the same day): this
phase runs from 400 BCE to 1200 CE, and pass 1 covers three thousand six hundred years.**

The first cut of this ruling put the boundary at 0 CE inside an unchanged four-thousand-year pass.
Both numbers moved. The arithmetic is stated here because every other document takes its figures
from this one:

| | from | to | years |
|---|---|---|---|
| **Culture** — the migration | 2400 BCE | 400 BCE | 2,000 |
| **Empires** — this phase | 400 BCE | 1200 CE | 1,600 |
| **Pass 1**, total | 2400 BCE | 1200 CE | **3,600** |

**1200 CE is unmoved**, so everything downstream of it — the coast to 1560, pass 2, the 1960 epoch
— is untouched (`GENERATION_STRATEGY.md` § Pass 2 is the economy pass, 1560 → 1960). What moved is
where pass 1 *starts* and where it *divides*.

**400 BCE is NOT a year the engine already knows.** The sim's ancient arc ends at 0 CE, so the
boundary is not free the way the first cut of this section claimed — the split is real work, and
`BL-871 (empire span)` carries it.

**COLONISATION's derived terminating condition survives, and a COAST reconciles the two (Ben,
2026-09-09, confirming NR-818).** That document ends the migration round when every habitable
landmass carries some culture, which is a derived year and not 400 BCE. Both hold. The Culture
round ends when the filling ends, and the world then **coasts** to 400 BCE holding what migration
left it — the same device the design already uses for 1200 → 1560, where a span whose defining
property is that little changes is not worth simulating.

**A migration unfinished at 400 BCE is a defect in the migration, not in this boundary.**
Unsettled ground is the exception marking hostile country (`../ui/STARTUP.md`
§ Rounds — System, Life, Culture, Empires, Industrialisation), so a world still filling after two
thousand years has a colonisation problem to fix rather than a boundary to move.

**Sixteen hundred years, not four thousand, is the constraint on everything below.** The arc —
communication, then conquest or union, then a stable dark age — has to fit in it, and a mechanism
that needs millennia to show an effect does not belong in this phase. Moving the boundary from
0 CE to 400 BCE gave this phase four hundred more years and took them from the migration, which is
the practical effect of the revision.

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

**SETTLED (Ben, 2026-09-09, elicitation): a settlement is a SEAT FLAG ON A REGION, and every
region points at the seat it feeds.** No new table and no new id space. The region already carries
everything a seat needs to be worth taking — population, `manpower_stock`, `army_stock`, the four
endowment windows and the urban record — so a separate settlement record would hold pointers back
to a region for every field that matters.

**The hinterland is therefore a pointer, and that is what makes it change hands with its seat.**
Ground is not conquered region by region; taking the seat takes what points at it. A region whose
seat is gone is either re-pointed at the conqueror's seat or falls outside anyone's reach, which
is how an empire's edge becomes ragged without any rule drawing a ragged edge.

**`region::centres` is not retired by this and is not the same question.** Centres are how many
urban places stand on a region; the seat flag is which region a city state is governed from. A
seat with no centre is a poor capital, and that is a legitimate world.

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

**SETTLED (Ben, 2026-09-09, elicitation): the stores sit AT THE SEAT, and fall with it.** Not one
treasury per polity, and not a heap on every region. A seat holds what its hinterland produced,
and a conqueror who takes the seat takes the stores standing in it.

**That is what closes the loop between conquest and materials**, and it is why the seat had to
exist before this section could be settled. A war that ends by taking a city ends by taking that
city's capacity to fight the next one, so a losing polity gets materially poorer rather than
merely smaller — the same shape `polity::cohesion_q` gives defeat, expressed in stock rather than
in morale.

**The labour split stays on the GROUND, because that is where the people are.** Subsistence,
industry and muster divide per region, out of a region's own population; what industry makes flows
to the seat. So the equilibrium Ben describes — a high-population province reserving people for
work — is a property of ground, while the thing conquest captures is a property of the seat. The
two are different questions and they get different homes.

---

## The road is the empire's skeleton, and reach GATES conquest

**Ben, 2026-09-09 (elicitation notes): the point of this phase is a SPARSE ROAD NETWORK which
connects city states, forms empires, and gives actual population centres, derived by possible
supply and governance.**

That sentence names the phase's output. Not a border map and not a battle count — a **network**,
whose nodes are seats and whose edges are roads, and whose extent is the reason an empire is the
size it is.

**SETTLED (Ben, 2026-09-09, elicitation): reach GATES a campaign; it does not merely price it.**
Beyond sustainable reach a campaign is not an expensive option, it is not an option. The
alternative — pricing overreach so a polity may overextend and then collapse — was the
recommendation and was declined, and the declined version is worth recording because it explains
what the ruling buys.

**Gating makes the road the mechanism instead of the flavour.** If reach only priced a campaign, a
rich polity could buy its way past geography and roads would be a discount. Gated, the only way to
reach further is to **build further**, so an empire's shape is the shape of what it built, and
`BL-837 (ancient logistics and roads)` stops being a modifier and becomes the phase's spine.

**It is also the shape the sim already uses.** Gating rather than pricing is what
`MILITARY_HISTORY.md` § Forage — the simplification, and what it is a simplification OF already
does: a force forages where it is adjacent to water or to land its own polity owns, and starves
where it is not. Reach-gating is that rule generalised off the coast and onto the road, not a new
kind of constraint.

**And it is a brake with a visible cause**, which is the standing requirement. An empire stops
growing at the end of its roads, on the map, for a reason a player can point at — never because a
term inside a scorer grew with its rank.

### Centres are derived by supply and governance

**A population centre is not a demographic accident in this phase; it is what the network can feed
and rule.** Ben's phrasing is exact — *derived by possible supply and governance* — and it gives
`region::centres` a cause it currently lacks: a centre stands where supply can reach it and where
a seat can govern it, so the urban map is downstream of the road map.

**This is what makes the sparse settlement pattern hold rather than drift.** Centres do not appear
because population crossed a threshold in isolation; they appear where a network put them, which
is why the map stays legible as the population grows.

**SETTLED (Claude, 2026-09-10, taken on Ben's behalf under BL-872): ONE quantity, not two.** A
region's own `network_supply_q` — the terrain-and-road Dijkstra reach from its polity's seat that
`BL-837` already prices a campaign at and attrites an unsustained garrison with — answers both
questions at once. Governance ("can the seat rule this ground") and supply ("can materials reach
it") are the same fact in a sim with one network, one seat per region and one Dijkstra: a second
number would only ever restate the first, and building one would be inventing a distinction this
sim's own model of the world does not carry. Two numbers become worth separating only if a later
item gives them genuinely different inputs — a naval-only supply line, say, that governs nothing —
and that is a new design question for whoever proposes it, not a gap in this one.

**SETTLED (Claude, 2026-09-10, taken on Ben's behalf under BL-872): FROZEN, not razed.** A region
whose `network_supply_q` falls at or below `sustainable_settlement_floor_q` stops growing new
centres — `region::centres` holds at whatever it already stood, promoted no further — but nothing
already standing is destroyed. Razing was the candidate `centres_razed` already exists for, and it
is the wrong shape here: that field and `sack_region_urban` represent a deliberate act of history
with an actor and a date, and a road falling into disuse is neither. Freezing is also the reading
`POPULATION.md`'s own asymmetry already uses for the ordinary demographic case (a shrinking city
keeps its centre); this extends the same asymmetry to a second kind of passive failure rather than
inventing a harsher one for the network specifically.

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

## Land is already settled when this phase opens (Ben, 2026-09-11)

> *"For our stage, land should already be settled. Settle in our stage is just a consequence of
> when polities decide to raze settlements and destroy cultures."*

**This phase does not colonise.** § The span is 400 BCE to 1200 CE hands this round a world the
migration has already filled; `COLONISATION.md` owns the filling and is emphatic that it is a
diffusion with no actor. What arrives here is a settled map, and an empire is built by taking
someone else's ground rather than by finding more.

**SO `Settle` MEANS SOMETHING DIFFERENT HERE, AND ONLY ONE THING.** It is the RE-founding of
ground that was emptied — a razed seat resettled, a culture destroyed and replaced. It is a
*consequence* of violence, downstream of a sack, and it cannot fire on ground nobody has
harmed. What it must NOT be in this phase is the growth-without-war axis it is in the migration
span: a population-pressure verb that manufactures new ground indefinitely.

**WHY THIS IS THE FIX AND A SCORER WEIGHT IS NOT.** `history_sim.cpp` § Settle currently fires
on population pressure against carrying capacity, with a comment stating its purpose plainly —
*"without this verb a 2000-year run has a frozen region count"*. That is a migration-era
justification applied to an empire-era round, and it makes peaceful expansion permanently
available. A verb that is always available and always cheap wins the argmax, which is exactly
what the sweep measures: Campaign is in the running thousands of rounds per world and wins
about one in a hundred.

The tempting repair — weight Campaign higher, or drop its threshold — is a term inside the
actor, which `.claude/rules/io-standing-rules.md` forbids and which Ben declined for the reach
gate on 2026-09-09 (NR-823). **Re-scoping what `Settle` MEANS in this phase is not a weight.**
It changes what is legal, not what is preferred, and it leaves the scorer honest: with no free
ground to take, Campaign clears on its own merits because it is the only way to grow.

**The frontier closing is the in-world force with the visible cause** that § The arc the phase
must produce asks for. Empires form when expansion stops being free.

---

## The arc the phase must produce (Ben, 2026-09-10)

> *"Really I want to see polities be eliminated and empires to form, before collapsing back
> into those smaller polities - with some surviving as larger kingdoms."*

This is the phase's **outcome shape**, and it is what every mechanism above is ultimately
answerable to. `GENERATION_STRATEGY.md` § The asymmetry is POLITICAL as well as
economic owns the full statement; what belongs here is what it demands of *this* phase.

**Conquest must COMPOUND.** Territory changing hands is necessary and nowhere near sufficient. A
transfer that leaves the winner no better placed to win the next one produces churn — a map that
moves constantly and ends the shape it started. The arc needs holding ground to make the next
campaign easier, so that success runs away with itself into an empire and failure runs away into
a collapse. Every one of the four moments — elimination, empire, collapse, uneven survival — is a
consequence of that one property, not a separate dial to be tuned.

**THE WALL MOVES WHEN YOU WIN (Ben, 2026-09-10, ruling on NR-823).** § The road is the empire's
skeleton makes reach GATE a campaign rather than price it, on Ben's 2026-09-09 ruling, so that a
rich polity cannot buy past geography. Read naively that caps every polity at the neighbourhood
it started in, which is the opposite of compounding — and the two rulings would contradict each
other. They do not, because **the gate is not fixed**: what a polity can reach is a function of
the road network it has built and walked, so **winning extends the wall outward** rather than
dissolving it. Geography still cannot be BOUGHT past. It has to be BUILT past, and building past
it is what an empire is.

The two rejected readings, so they are not re-litigated. **Softening the gate into a steep price**
was declined — it restores the low-probability killing blow by reversing the 2026-09-09 ruling
outright, and a rich polity could buy past geography again. **Reaching for centre-chain
propagation** (`BL-887`) was declined *for now* — it is the eventual reach model and remains
deliberately deferred until tech progression is wired into these rounds.

**WHAT THAT MAKES LOAD-BEARING.** If success must widen a polity's reach, then the mechanism by
which reach widens is the mechanism the whole arc rests on — and `BL-892` records it failing
today: `history_sweep`'s `W7b` shows a pre-built reach work does not change the supply path at
all. A wall that cannot move is the fixed wall this ruling declines. Diagnose the two together;
`BL-889` owns the arc, `BL-892` owns the lever.

**A peaceable world is a legitimate outcome (Ben, 2026-09-10).** The claim is distributional: the
arc must appear across a spread of worlds, never in every one of them. A seed that refuses war is
not a failure case and must not be made into one.

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

**SETTLED (Ben, 2026-09-09, elicitation): a civilisation is a NAMED RECORD, like a culture, with
member cultures and an ethic.** Not a set of axes over the cultures that compose it, and not a
lens on the existing shares. It is a thing with a name, and it can be pointed at.

**A record is what lets a civilisation OUTLIVE the polities that formed it**, which is the
property the other two candidates cannot supply. A lens dies the moment the shares beneath it
move, so a civilisation would evaporate at exactly the moment the design most wants it — after the
empire that raised it has fragmented. A record persists, and the fragments inherit it.

**Its name is coined from the tongues that mixed, never from a bank of its own.** The naming
substrate exists (`../lore/CREEDS.md`, `world/tongue.hpp`) and the standing rule holds without
exception: generated names are sci-fi, never drawn from an Earth list.

**The ETHIC is the field that makes it not-a-creed.** A creed answers *which gods there are*; the
ethic answers *how one ought to live*. It is second-order by construction — derived from what the
member cultures disagreed about and settled, so it can only exist where mixing happened.

**What is NOT settled:** what an ethic is as data, and what it means for a polity to *belong* to a
civilisation as opposed to merely standing on ground that carries one.

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

**SETTLED (Ben, 2026-09-09, elicitation): opposition is supplied by the PANTHEON'S TEMPERAMENT
and the ORIGIN FARM CLASS, together. Contact is not an axis of opposition.**

**Two axes, and they disagree about different things.** The pantheon gives a *stated*
disagreement — a people whose war god relishes battle and expects to prevail sits opposite one
whose does neither. The farm class gives a *material* one — a people of the floodplain and a
people of the highlands want different ground and live differently. Neither alone is enough:
temperament without material interest is a quarrel about nothing, and material interest without
temperament is a quarrel nobody is willing to fight.

**Contact was considered and left out on purpose.** Interpenetration says two peoples MET, which
is a precondition for a quarrel rather than a cause of one — and `w_cult` already reads the shares,
so counting contact again inside opposition would charge the same fact twice.

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

### The calls, now settled

1. **OPPOSITION IS SYMMETRIC** (Claude, 2026-09-09, taken on Ben's behalf; NR-815). Two peoples
   disagree about how to live, and a disagreement has two sides. Relations are therefore a
   triangular matrix over cultures, not a set of directed pairs. **The directed layer already
   exists and is doing a different job**: `grudge` records who wronged whom, at a place and a
   date. Two layers, two questions — one asks *are we opposed*, the other *what did you do to me*.
2. **KINSHIP IS MEASURED IN YEARS SINCE THE COMMON ANCESTOR, not in hops** (Claude, 2026-09-09,
   taken on Ben's behalf; NR-816). The tree runs 9–10 deep over 676 and 929 cultures, so a hop
   count is coarse, and it is blind to the thing that actually matters — two peoples four hops
   apart who parted three thousand years ago are not the cousins of two who parted three hundred
   years ago. **Time is decay without a decay constant**, and it is earned by the migration rather
   than assigned: it needs one retained field, the year a culture was coined, in exactly the shape
   `BL-865 (culture descent retained)` used for `parent`.
3. **OPPOSITION PERMITS CONQUEST, IT DOES NOT CAUSE IT** (Ben, 2026-09-09). Relations feed the
   existing `w_cult` weight rather than raising a score of their own. One scorer, measurable
   against the sweep already in place, and raisable to *cause* later if the histories come out
   flat — whereas a second scorer is hard to remove once other things read it.
4. **A CIVILISATION CANNOT FORM ACROSS AN OPPOSITION ABOVE A BAR, AND INHERITS WHAT IS LEFT AS
   STRAIN** (Claude, 2026-09-09, taken on Ben's behalf; NR-817). The three candidates were
   *resolve*, *inherit* and *fracture*, and the answer uses two of them at different moments.
   **Fracture is the formation rule**: peoples too opposed to agree on how to live do not produce
   a shared answer about it, so no civilisation is coined there. **Inheritance is the
   consequence**: where one does form over residual opposition, it carries that opposition as
   internal strain rather than dissolving it. Resolve was rejected outright — a civilisation that
   makes its members agree is a forced outcome, and it would flatten the world at exactly the
   scale the design wants asymmetry.

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
| **The year a culture was coined** — the kinship clock | `front_entry` / arrival order | **DISCARDED** |
| Settlement seats and hinterland pointers | `region::centres` (dense today) | **owed, § The unit is the city state** |
| Material stores at the seat | — | **owed, § Materials are spent** |

**Three of those are discarded facts the migration already computed**, and all three are cheap to
retain — parentage, origin farm class, and now the coining year that § The calls makes the kinship
measure. Everything else is either carried today or is honest new work.

**The coining year is the newest of the three and it is owed by a ruling, not by an oversight.**
Kinship in years is worth nothing without the years.

---

## What the dark age must leave (Ben, 2026-09-11)

The section below names *fragmentation* as the ending. This one names what the fragments have to
**carry**, and it is settled by the outcome test Ben set: the phase must leave a world in which a
**colonial era** is possible, as the precursor to global trade.

**THREE OUTPUTS, ALL CHOSEN; ONE CANDIDATE DECLINED.**

1. **Nations of unequal strength.** Some large enough to colonise, some only large enough to be
   colonised. This is `GENERATION_STRATEGY.md` § The asymmetry is POLITICAL as well as economic
   read from the far end — the asymmetry is not decoration, it is the input the next phase needs.
2. **Roads that outlive their builders.** The network survives the empire that built it, so
   whoever inherits the ground starts ahead of whoever does not. Infrastructure is the empire's
   estate, and it is inherited unevenly.
3. **Grudges that still bite.** `pass_one_output::grudges` is already *carried* (§ What the
   Culture phase must hand forward) and is currently inert. It must be consequential: who
   colonises whom is not a fresh roll, it is the last quarrel continued by other means.

**GROUND RELEASED BACK TO NOBODY WAS DECLINED, and the reason matters.** Collapse does not
re-wild what it cannot hold. That looks at first like a contradiction with `BL-887`'s note about
leaving "a new world" for a later phase to find — and it is not, because **a colonial era
colonises INHABITED ground.** The thing a coloniser wants is not empty land; it is land held by
someone weaker, with people on it already. Unequal nations *are* the new world. Nothing needs to
be emptied for the next phase to have somewhere to go.

**THE CAUSE OF COLLAPSE IS NETWORK FAILURE (Ben, 2026-09-11).** An empire persists while its
network holds and fragments when it does not — which is the mechanism § What this phase hands the
industrial era already names ("reach-gating is what makes the collapse mechanical rather than
scripted... never a collapse event fired at a date"). That is now a ruling rather than an
incumbent reading. Succession, exhaustion and external shock were all available and none was
chosen: collapse is what happens when a realm can no longer reach itself.

### War is paid for, and trade is what pays (Ben, 2026-09-11)

**A campaign costs materials** — `BL-867 (materials spent on action)`, already built. What was
missing is where a polity gets them, and the answer is **rich trade**, with a hard constraint:
this phase still has **no market**. § Materials are spent when something happens is unchanged —
no order book, no firm, no building stack, no price.

**So the income is a property of the NETWORK, not of a market.** A road joining ground that holds
*different* things yields materials; a road joining two places that hold the same thing yields
little. That is a crude trade model with a visible cause on the map, and it does three jobs at
once:

- it makes **roads worth building** for a reason other than reach;
- it makes **war affordable to the rich** and unaffordable to the poor, which is where asymmetry
  in campaigning comes from without a term inside any actor;
- it makes **network failure expensive**, so the collapse ruling above has teeth — a realm whose
  roads fail loses its income before it loses its ground.

**The resource reading stays abstract.** § What this phase hands the industrial era is explicit
that resources become capital in pass 2 and that reading must not leak backwards. What this phase
needs is only *difference* — that two places hold unlike things — never a price for either.

---

### What materials are FOR (Ben, 2026-09-11)

§ War is paid for, and trade is what pays gave this phase an income. Measurement then found the
other half missing: **campaigns are the only thing materials are ever spent on** — one site in the
whole sim — and industry produces about 241,000,000 per world against campaign spending in the
*hundreds*. Materials were not merely abundant, they had **no sink**, and an income that nothing
competes for cannot make war affordable to the rich and unaffordable to the poor however large it
is.

**TWO SINKS, BOTH CHOSEN.**

- **A STANDING ARMY EATS MATERIALS EVERY YEAR**, not only when it marches. An army is a permanent
  claim on a realm's production rather than a one-off purchase, which is what makes a large one a
  *decision* instead of a free accumulation. It also gives a losing realm a way to be strangled
  rather than only beaten.
- **ROADS AND WORKS COST MATERIALS TO BUILD.** This is the one that closes a loop: trade pays for
  the network, and the network is what carries trade. It also turns `NR-823`'s ruling — *the wall
  moves when you win* — from a purely geographic statement into an economic one, because a realm
  that cannot afford roads cannot extend its reach however much ground it takes.

**STOCK STAYS UNBOUNDED, and that is consistent rather than an oversight.** With two live sinks a
realm's stock is limited by what it chooses to keep paying for; a cap or a decay would be a second
mechanism doing a job the sinks already do, and would need a magnitude nobody has a reason to pick.

**UNPAID TROOPS GO HOME, which is what makes upkeep a strangling channel rather than a drain.** A
seat that cannot pay does not run a deficit: the unpaid share of the garrison walks off, and the
heads return to the recruitable pool rather than to the civilian count, exactly as an over-target
disband already sends them. So a realm cut off from its income **loses its army without losing a
battle** — which is the second job § How an empire actually falls needs from this mechanism.

**A REFUSED ROAD IS DELAYED, NEVER FORBIDDEN.** A corridor whose promotion the seat cannot afford
holds its use count one short of the threshold instead of discarding the walk, so it is promoted
the next time it is walked with the materials standing. The walk itself is always recorded — a
party that walked a line walked it whether or not anyone widened it. What money buys is the *tier*,
which is the half that does something to reach.

**THE MAGNITUDES ARE MEASURED AGAINST A STATED TARGET, not felt.** The target: the sinks should
claim a **visible minority** of production, so stock is a constraint a realm manages, while road
building stays a *choice* rather than something poverty forbids. At an upkeep of 200 per 1,000
heads the sinks claimed **63%** of production and **324** corridor promotions a world were refused
for want of materials — that is poverty governing the network, not a realm choosing. At **20** they
claim **8%** with 43 refusals, and 2.7 million heads a world are still sent home unpaid.

**TRADE IS STILL NOT WHAT PAYS FOR WAR, and this section does not pretend otherwise.** With both
sinks live, network income is **281,280 of 113,922,140** produced — a quarter of one per cent. What
makes war affordable is industry. A per-link constant is bounded by a realm's adjacency and so can
never grow with a realm the way industry does, which is a shape problem rather than a magnitude
one; raising the constant until the share looked right would be fitting a figure to a target.
`NR-827` carries the open call.

### How an empire actually falls (Ben, 2026-09-11)

§ What the dark age must leave settles the CAUSE as network failure. These are the two calls that
make it a mechanism.

**THE NETWORK IS REACH, NOT THE ROAD GRAPH.** § The road is the empire's skeleton calls roads the
skeleton, and reach is what a skeleton *does*: `rebuild_reach` already discounts the corridors a
history has actually walked, so reach **is** the road network, measured densely instead of
sparsely. The practical difference is decisive — the road graph is about **fifty edges per world**
(1,347 of 1,607 recorded corridors are walked once and never promoted), which is not a structure an
empire can be said to hang on, while `region::network_supply_q` is computed for **every held
region every decision round** and today gates nothing but town growth (`BL-872`).

**GROUND THE REALM CAN NO LONGER REACH SECEDES.** It does not fall to a neighbour, and that choice
is the arc's. A region lost to whoever can reach it **concentrates** the map and works against
everything this phase is for; a region that secedes becomes a **successor** — a new polity, holding
real ground, carrying the culture and the grudges it already had. That is precisely what § What the
dark age must leave asks the phase to hand forward: *nations of unequal strength*, some large
enough to colonise and some only to be colonised.

**MECHANICAL, NEVER SCHEDULED.** No collapse fires on a date or a counter. A realm fragments
because a specific region's supply fell under a floor, for reasons a player could read off the map
— distance, terrain, a road that was never built, a war that emptied the ground between.

**SECESSION MUST BE DETERMINISTIC**, which is a real constraint rather than a note: a seceding
polity needs an id and a seat allocated in an order that depends only on the sim's own state, never
on iteration order over a container whose order is undefined.

**A CONTIGUOUS BLOCK LEAVES TOGETHER, NEVER ONE REGION ALONE.** This is the dial between a
*shattered* realm and a *split* one, and only the second produces what the dark age is asked to
hand forward. A province leaving on its own reduces an empire to specks — many polities, all of
them too small to colonise anyone — while a cut-off block leaving as one produces a successor with
real ground and a real disparity against its parent. The block is grown by a breadth-first walk
from its lowest-indexed region, which is also what makes the allocation order a property of the map.

**THE SUCCESSOR IS ITS OWN PEOPLE'S REALM, NOT A COPY OF ITS PARENT.** Its culture is the **new
seat's plurality**, not the parent's — a province that walks away is the realm of whoever actually
lives on it, and that is what carries the pantheon record (`../generation/COLONISATION.md` § Culture
arrives by route) forward through the fragmentation instead of cloning the empire that lost it.

**WHAT IT INHERITS, AND WHAT IT DOES NOT.** It inherits the parent's **capacity and progress
ladders** and its **cohesion**: a province of an empire knows what the empire knew, and that is the
whole reason a dark age leaves *unequal* nations rather than a blank map. It inherits **nothing of
the parent's stock**, because that stock stands in a capital it no longer holds. The parent takes a
cohesion loss through the same channel every other defeat uses, and a grudge is raised parent →
successor as *ground taken* — ground did change hands, and that is the honest cause to tell a
player.

**MEASURED, 16 seeds at the 0 CE epoch:** a median of **2 secessions and 14 regions walking away**
per world, with the arc intact — 15 of 16 worlds still show a polity rise, peak and fall, largest
share at a median 10%, and hegemony reached in 0 of 16.

---

## What this phase hands the industrial era

**Ben, 2026-09-09 (elicitation notes): this phase feeds directly into the industrial era sim,
where resources are seen as CAPITAL — so it matters specifically how larger empires persist,
expand their reach, and then collapse into smaller nations ready for an industrial boom.**

**That names the arc's ENDING as a deliverable, and the ending is FRAGMENTATION.** Pass 2 is an
economy pass over nations (`GENERATION_STRATEGY.md` § Pass 2 is the economy pass, 1560 → 1960),
and nations are
what an empire leaves behind when it stops being able to hold itself. So *a stable dark age* is
not merely where the arc runs out — it is the state in which the industrial era finds its actors.

**A world that ends pass 1 as one hegemon has failed this handoff**, however plausible its
numbers, and so has a world that never assembled anything larger than a city state. Both ends of
that range produce an industrial era with nothing to industrialise against. This is the same
non-hegemony constraint the anti-hegemon levers serve (`BL-823 (anti-hegemon levers)`), read from
the far end: it is not only that a hegemon is dull to watch, it is that the next pass has no input.

**Reach-gating is what makes the collapse mechanical rather than scripted.** An empire persists
while its network holds and fragments when it does not, so the number of nations at 1200 CE is a
consequence of what got built and what stopped being maintainable — never a target count, and
never a collapse event fired at a date.

**RESOURCES BECOME CAPITAL IN PASS 2, NOT HERE.** This phase abstracts natural resources into what
conquest consumes; the reading of ground as capital belongs to the economy pass and must not leak
backwards into a phase that has no price
(`../economy/MARKETS.md`, and § Materials are spent when something happens).

---

## Open questions

The six calls this section used to carry were settled on 2026-09-09 and now live in the sections
that own them — the span, the seat, the stores, the two opposition axes, the civilisation record
and reach-gating. What is left is genuinely downstream of those.

- **What an ETHIC is as data**, and what it means for a polity to *belong* to a civilisation
  rather than merely stand on ground carrying one (§ A civilisation is what mixing makes).
- **Where the opposition BAR sits** — the threshold above which no civilisation is coined. A
  measurement, not a judgement: it should be set from a sweep that produces both alliance-shaped
  and enmity-shaped worlds, never from a number picked to make one seed look right.
- **Whether a seat can be FOUNDED mid-span**, or whether the set is fixed at 400 BCE. An empire that
  can raise a new city has a second growth mode; one that cannot has a fixed board.
- **How the sixteen-hundred-year span interacts with the capacity ladder.** The ladder was
  calibrated over four thousand years, and § The span now gives this phase 1,600 inside a
  3,600-year pass — whether the rungs still turn over at a believable rate is a measurement owed
  before any of them are tuned.
- **Reach as a single capital radius versus a network of centres (Ben, 2026-09-10, watching Round
  4 live).** `BL-837 (ancient logistics and roads)` gates a campaign off ONE Dijkstra from the
  polity's capital, and the visible cost is real: too few small polities survive the round, and
  B384c (a death spiral must terminate somewhere in a sweep) fails because a shrinking realm's
  last holdout can end up permanently beyond its own contracting reach. Ben's proposed fix is
  architectural, not a recalibration — reach PROPAGATES through CHAINS of population centres
  rather than radiating from one capital, with Logistic Points (`../economy/LOGISTICS.md`) as a
  byproduct of larger centres feeding that propagation. This also bears on how many polities
  survive into a "new world" a later exploration/colonisation phase could still find unclaimed.
  DELIBERATELY NOT BUILT YET: it wants tech progression wired into these generation rounds first,
  so a later sprint owns it (`BL-887`, filed, no sprint yet). B384c stays a known, accepted
  failure until then.
