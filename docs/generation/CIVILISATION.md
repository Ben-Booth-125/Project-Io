# Project Io — Civilisation

> **Settles:** what the Empires phase is and what it grows from · what a settlement is once
> centres become sparse · how materials are spent without a market · where armies come from
> and which peoples raise them · what a civilisation is, as distinct from a creed · how two
> cultures come to be similar or opposed, and why that is the engine of conquest · what the
> Culture phase must hand forward for any of it to work · what the phase hands the exploration
> age at its CLOSURE, and the readings that contract is judged on · what a realm is called, and
> why the name never follows its seat · how a fall is told, and why the sim reads that story
> back · what makes a long run affordable.
> **Not here:** how people came to live where they live (COLONISATION) · how force resolves
> once two polities contest ground (MILITARY_HISTORY) · what a pantheon is or how a tongue is
> coined (../lore/CREEDS) · the stage ladder this phase sits inside (../lore/HISTORY) · what a
> centre consumes once the campaign starts (../economy/POPULATION) · the retired roster of polity
> strategies and culminations, a mechanism reference only (../research/COLLAPSE_ROSTER).
> **Confused with:** COLONISATION.md, ../lore/HISTORY.md, MILITARY_HISTORY.md.

**How city states become empires.**

`COLONISATION.md` owns the span in which humanity spreads into empty ground, and it ends when
the filling ends. This document owns what happens next: the ground is taken, the peoples are
many, and the interesting question stops being *who arrives* and becomes *who takes what from
whom*.

**The premise, in Ben's words (2026-09-09):** the Empires phase is about *"growing city states
into empires, expanding ancient logistics networks, and abstracting the use of natural resources
for conquest."*

**THE PHASES ARE NOT CONTINUOUS** (`../ui/STARTUP.md` § Rounds — System, Life, Culture, Empires, Exploration, Industrialisation). Migration and empire have
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

**1200 CE is unmoved**, so everything downstream of it — the Exploration age to 1660, then
Industrialisation to 1960, the 1960 epoch — is untouched (§ The closure of the Empire era carries
the phase table; `GENERATION_STRATEGY.md` § Pass 2 is the economy pass carries the pass map). What
moved is where pass 1 *starts* and where it *divides*.

**400 BCE is NOT a year the engine already knows.** The sim's ancient arc ends at 0 CE, so the
boundary is not free the way the first cut of this section claimed — the split is real work, and
`BL-871 (empire span)` carries it.

**COLONISATION's derived terminating condition survives, and a COAST reconciles the two (Ben,
2026-09-09, confirming NR-818).** That document ends the migration round when every habitable
landmass carries some culture, which is a derived year and not 400 BCE. Both hold. The Culture
round ends when the filling ends, and the world then **coasts** to 400 BCE holding what migration
left it. A coast is the right device for a span whose defining property is that little changes,
and this is the one the design keeps: the far side of 1200 CE is simulated, not coasted (§ The
closure of the Empire era).

**A migration unfinished at 400 BCE is a defect in the migration, not in this boundary.**
Unsettled ground is the exception marking hostile country (`../ui/STARTUP.md`
§ Rounds — System, Life, Culture, Empires, Exploration, Industrialisation), so a world still filling after two
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

**A CITY STATE SPAWNS WHERE A REGION'S POPULATION IS ABOVE A THRESHOLD (Ben, 2026-09-11).** The
phase opens on the culture map the migration left: the ground is peopled and belongs to a culture,
and it is **unorganised** until a city state organises it. A region whose population stands above
the city-state threshold is a city state — at the opening, and at whatever later year it crosses
the line — so city states keep *rising* through the span as later-founded ground grows toward its
ceiling. The threshold is a stated magnitude chosen to select a minority of the regions standing
at 400 BCE (about the best-farmed third) and read against the opening distribution, never a quota
per world. A city state grows by **organising** the unorganised ground its capital can supply,
priced by kinship — its own people cheapest, kin dearer, an opposed people not at all, for they
can only be conquered — and by conquest of what other city states hold. Ground that a founding
adds to the map arrives unorganised; nobody grows for free.

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

**Every share reading is by POPULATION HELD, not regions held (Ben, 2026-09-15, NR-876).** The map
grows several times over inside the run as polities found new ground, so a share of regions counts
founded emptiness the same as a captured city; by population the largest realms are more concentrated
than region count shows and far fewer polities genuinely rise and fall. The region column stays as a
guard beside it, and the wizard's leaderboard ranks by the population column (BL-1000, wizard
leaderboard population share).

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

**THE ARC IS WATCHED, NOT READ OFF AT THE END (Ben, 2026-09-11).** The Empires phase *lives on
top of the culture map*. The round opens on a dull rendering of the peoples the migration left —
the ground is peopled and belongs to a culture, and a **city state** is a seat that has organised
some of it. What the player then watches, inside the time-lapse and not as a closing summary, is:
city states rising and taking control of the regions around them; neighbours who are kin setting
up trade; roads walked into being and bridges thrown across rivers; wars that take *whole
cultures* rather than one border region, and the cultural merge that follows; and polities that
have spread too far **breaking back down into city states**. A run in which the same few powers
consolidate and trade border regions back and forth for sixteen centuries has produced none of
these moments, however many conquests it logs.

**GROWTH FIRST.** The mechanism that lets a select few powerhouses clearly grow — room in the
culture map for kin to be gathered, force that follows the network so success compounds, ground
that can be lost to over-reach — comes before any deepening of how a battle resolves. Defence
over rivers and mountains is real and wanted (`MILITARY_HISTORY.md` § The resolver), and it waits
until strong growth is visible on the map, because a richer battle inside a stable map is a
richer way of changing nothing.

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
empire that raised it has fragmented. A record persists, and the fragments inherit it. **The record
crosses every span boundary whole and in order** (`BL-1049 (civilisation index reuse at 1200)`): the
civilisation table and the universal-creed table are part of what each handoff carries, a span
resumed from a handoff opens on them, so a region's index names the same record on either side of
1200 or 1660, a record coined in the later span takes the next free index, and a pair an earlier span
already settled is found rather than recorded a second time under a name from the later span's seed.

**Its name is coined from the tongues that mixed, never from a bank of its own.** The naming
substrate exists (`../lore/CREEDS.md`, `world/tongue.hpp`) and the standing rule holds without
exception: generated names are sci-fi, never drawn from an Earth list.

**The coining is a named moment on the record, and it is one of two (Ben, 2026-09-24).** The
instant a civilisation is coined is the `civilisation_formed` event — seated at the coining
region, on the realm whose ground did the mixing. The record names exactly two moments as a
realm's step up: this one, and the realm's rise past the sweep's own ROSE rule (a peak at least
double its start and at least three regions more — `history_sweep`'s definition, so no reader
can drift from it). Any reading that ratchets on a realm's growth ratchets at these two moments
and nowhere else; both are facts of the record keyed by polity id, stable except at the moment,
so they cross a span boundary by id. What is drawn at either is `../ui/STARTUP.md`'s.

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
| **Culture PARENTAGE — the family tree** | `culture::parent` (`creeds.hpp`) — index of the mother culture, −1 at a cradle; a daughter always sits above her mother, so the walk to the root cannot loop. The LIVING tree: after the boundary fold it names only cultures that kept their name | **carried** |
| **The lineage and the fold** — who a people split from, and whether its name outlived the migration | `culture::coined_from` (never rewritten) and `culture::folded_into` (−1, or the ancestor that absorbed it) — § Empty cultures fold into their parent | **carried** |
| **The farm class a culture was coined on** | `culture::origin_farm_class` — the opposition half of culture relations | **carried** |
| **The year a culture was coined** — the kinship clock | `culture::coined_year` — read by `culture_kinship_years` | **carried** |
| Settlement seats and hinterland pointers | `region::is_seat`, `region::seat_region` (`settlement.hpp`) — one seat per polity at its capital, every hinterland region pointing at its seat | **carried** |
| Material stores at the seat | `region::material_stock` — accumulates only where `is_seat`, falls with the seat | **carried** |

**The three culture rows are facts the migration computes and the contract keeps** — parentage,
origin farm class, and the coining year that § The calls makes the kinship measure. Parentage is
the family tree; the origin class is the opposition half of culture relations (a people of the
floodplain and a people of the highlands want different ground); the coining year is what makes
kinship a years-since-common-ancestor measure rather than an adjacency, and kinship in years is
worth nothing without the years.

### Empty cultures fold into their parent

**Ben, 2026-09-16 (NR-879, option A): a culture holding no ground at the Colonisation/Empires
boundary FOLDS BACK INTO ITS PARENT, and the lineage link is kept.** The migration coins far more
peoples than ever settle — measured over 16 seeds, 5,453 coined against 790 holding ground when
the Empires round opens — and almost all of the empty ones are first-generation daughters. The
coining rule is untouched: the split census and the migration's own record still see every split
that happened. What shrinks is only the set of names that outlive the round.

- **Holding ground** means any record the Empires round will read names the culture: a share or
  the founding culture of a region on the map, **or of a region the founding schedule will place
  during the span**. A people whose stream lands after the boundary is a name somebody will live
  under, and folding it would rename ground the sim is about to found.
- **A folded culture folds into its nearest ancestor that holds ground.** It keeps its row and its
  index — nothing names it, so nothing needs remapping — and records both the people it was coined
  from (`coined_from`) and the name that absorbed it (`folded_into`).
- **An empty interior node's living daughters are re-parented** onto that same nearest living
  ancestor. No daughter is orphaned: `parent` never names a folded culture, and re-parenting only
  ever moves up the lineage.
- **A cradle never folds.** It has no parent to fold into.
- **Every reader of the tree walks the living tree** — kinship years, culture opposition, the
  lineage palette, and the Empires round's polity seeding. Two daughters of a folded mother
  therefore date their kinship from the nearest ancestor that kept its name. The migration's split
  events read the lineage instead, because a split is a fact about who a people split from.

The fold's shape is checked on the shipped path: both handoff validators reject a folded culture
with no recorded lineage, an orphaned living culture, and any region share, founding or polity that
names a folded culture.

**The two settlement rows are the city-state unit** (§ The unit is the city state, § Materials
are spent): a seat is a fact about the ground, so conquest changes who governs from it and never
whether it is one; the stores sit at the seat and fall with it.

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

**HOW A GRUDGE FADES INSIDE THE SIM (Ben accepted as built, 2026-09-18, NR-886).** A standing
score sheds 3 per-mille of itself a year (`grudge_decay_per_year_q`), a half-life near 230 years:
a single old wrong is gone by the epoch, a running feud is not. That rate holds only above a
truncation line: below a score of 1000 / (rate × step) the proportional shed rounds to zero, so a
small grudge instead loses one unit a round — it fades **linearly**, faster than the exponential,
and faster on a finer clock (`history_sim_harness` BL842 prints the measured half-lives per step).
Keeping the exact rate would need a stored remainder, which brings back rounds on which a score
stands still; the faster fade of small grudges is accepted instead. Campaign sentiment decays on
its own authored rate (`../politics/RELATIONS.md` § The authored numbers).

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

**So the income is a property of the NETWORK, not of a market.** A network joining ground that
holds *different* things yields materials; a network joining places that all hold the same thing
yields nothing. That is a crude trade model with a visible cause on the map, and it does three jobs at
once:

- it makes **roads worth building** for a reason other than reach;
- it makes **war affordable to the rich** and unaffordable to the poor, which is where asymmetry
  in campaigning comes from without a term inside any actor;
- it makes **network failure expensive**, so the collapse ruling above has teeth — a realm whose
  roads fail loses its income before it loses its ground.

**TRADE IS PAID ON THE KINDS OF GROUND A REALM REACHES, NOT ON ROADED PAIRS (Ben, 2026-09-16,
`NR-827`).** A per-link income is bounded by a region's adjacency, so it can never grow with a
realm the way industry does. The base is therefore *how many different kinds of ground the realm
can reach*:

- **A KIND** is what a region is best at — farm ground, ore ground or a port. It is a class,
  never a quantity.
- **REACHED** means the network carries ordinary trade there: the region's reach from its
  capital stands above the floor at which its towns can still grow. § The network is reach, not
  the road graph applies — roads pay through reach, not per edge.
- **ACROSS AN AMICABLE BORDER**, a walked corridor joining two realms' reached ground makes each
  side's kind one the other reaches. Only the ground the corridor touches counts, never the
  neighbour's whole realm.
- **THE INCOME**: every reached region, every year, earns one fixed amount for each kind its realm
  reaches that is unlike its own. It is paid at the region's seat, as industry is.

So a realm of one kind earns nothing however large it grows, and a realm that reaches all three
earns on every region it reaches. Ground the network cannot reach earns nothing and lends its
kind to nobody — which is what makes network failure cost income as well as ground. The per-edge
magnitude was carried over unchanged; only what it multiplies moved.

**RULED AS BUILT (Ben, 2026-09-18, NR-886).** The reach floor (a region counts as reached only
while its reach stands above the floor at which its towns can still grow) and the cross-border
reading (only the ground an amicable corridor touches lends its kind) stand as written. Measured,
the re-base half-holds and that is accepted: trade helps a realm pay its standing armies but not
its campaigns, and with three kinds a region earns at most 80 a year, so income grows with regions
held more than with kinds of ground reached.

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

**TRADE IS STILL A MINOR INCOME, and this section does not pretend otherwise.** Paid per roaded
pair, network income was **a quarter of one per cent** of production with both sinks live. Paid
on kinds reached, at the same magnitude, it is **3.9%** — measured over 16 seeds at the 0 CE epoch
on the shipped inputs: a median **71,424,160 of 1,846,370,504** produced per world, and **3.15%**
pooled across all sixteen. The re-base multiplied trade twentyfold without touching the constant,
because the shape changed rather than the figure. What makes war affordable is still mostly
industry. Raising the constant until the share looked right would be fitting a figure to a target,
and cutting industry's yield would make the same trade look larger without anything having
changed; both were declined.

**DOES A CONNECTED REALM SUSTAIN CAMPAIGNS A DISCONNECTED ONE CANNOT? PARTLY.** Measured on the
same sweep, within size bands (a connected realm is usually a larger one), against a control run
with the trade income set to zero:

- **Keeping an army paid — yes.** A realm reaching two or more kinds meets more of its army's
  upkeep than a same-sized realm reaching one, in every band. Trade widens that gap over the
  control by 14 to 64 per mille, most for a lone city state.
- **Paying for a campaign — no.** Connected realms of two or more regions already cover more of a
  campaign's material cost with trade off. With trade on, that gap *narrows* in three of the four
  bands. Mixed ground seems to carry the advantage through industry, not through trade.

So trade feeds the strangling channel, not the war chest. The two runs are different worlds
rather than one world with a single lever pulled, so these gaps compare shapes, not causes.

**SUPPLY SITES ARE BOUGHT FROM THE STOCKPILE (Ben, 2026-09-11).** A realm does not only wait for
a road to be walked into being; it can spend the materials stockpiled at its capital — *"gained
through trade or conquest"* — on upgrading a **supply site**: a reach work on a held region, or
the tier of a corridor, chosen where the network is weakest. This is the deliberate half of *the
wall moves when you win*: a realm that trades and wins can afford to reach further, and a realm
that cannot pay watches its far ground break away.

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

**GROUND THAT BREAKS AWAY BREAKS INTO CITY STATES, ONE PER CUT-OFF SEAT (Ben, 2026-09-11).**
Each seat the capital can no longer supply becomes its own polity, taking the hinterland that
points at it. A cut-off region with no seat joins the nearest cut-off seat, and where there is
none it stands alone as a city state of its own — it is never handed to a neighbour, and it is
never released to nobody (§ What the dark age must leave holds: collapse does not re-wild).
This supersedes the earlier build-time call that a breakdown leaves as a contiguous block of two
or more regions: the dark age is asked to hand forward nations of *unequal* strength, and the
inequality that matters is between the surviving core and the many small realms it could no
longer hold. With seats as numerous as city states that have risen, the pieces are real polities,
not specks. The cut is grown by a breadth-first walk from the lowest-indexed region, which is what
makes the allocation order a property of the map.

**THE CAPITAL IS THE STRATEGIC HEADQUARTERS (Ben, 2026-09-11).** Every reading of supply is priced
from the polity's capital — holding, growth, breakdown, the campaign gate, the battle and the levy
alike — over the ground the realm holds, so that a war which takes the ground between a capital
and its far block cuts that block off. *"Each polity sees itself as the strategic headquarters
where any supply, or material gain is stockpiled"*: the material stock lives at the capital, a
captured seat's stock is carried home, and it is from that stockpile that a realm pays to extend
its reach (§ What materials are FOR).

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

### Fear of being next (Ben, 2026-09-11)

A realm is attacked for what it has **done**, not for how big it is. This is the one lever
`GENERATION_STRATEGY.md` marked *dangerous* — a balancing coalition — and it is admissible only in
this exact form.

**WHY THE OBVIOUS VERSION IS FORBIDDEN.** A term that reads a polity's RANK and pushes back is an
agent handicap in a diplomacy costume, and the standing rule against agent handicaps forbids it
outright. "The largest polity" is not a fact about the world; it is a fact about the scoreboard,
and a force that reads the scoreboard is not in the world.

**WHAT IS READ INSTEAD.** When a realm weighs a campaign against another, it may ask what that
other realm has done **to peoples like its own** — the grudges held against it by living realms of
the decider's own culture. That is a behaviour, recorded as named events with a place and a date,
so a coalition forming against a riser is explicable **on the map**: the player can ask why and be
handed the list. It required a dated widening of the AI-behaviour prohibition, raised rather than
assumed, and `../ai/AI_OPPONENT.md` § 11 carries it with its exclusions.

**THREE EXCLUSIONS, EACH LOAD-BEARING.** A realm never reads its **own** grudges to pick a target
— that is revenge, a term inside the actor, and it stays declined. Nothing reads **size or rank**
anywhere. And there are no treaties, no negotiation and no alliance objects: nobody agrees to
anything, each realm reaches the same conclusion separately, which is what makes this a fear
response inside a generation sim rather than a diplomacy layer.

**THE LEAN IS ONE-SIDED, unlike the creed-aggression lean beside it.** A ledger's neutral is the
*absence* of a record, and there is no such thing as fewer than no wrongs done — so a blameless
realm is never leaned *down*. Leaning it down would be a peace bonus nobody granted, and it would
make the peaceful half of the check below pass for the wrong reason.

**THE CHECK THAT THE SCOPE HELD IS BEHAVIOURAL, and it cannot be read off a sweep.** In any real
world a size term and a behaviour term correlate, so aggregates cannot tell them apart. The
assertion is made instead on a built ledger in which **the peaceful realm is the larger one**: a
large peaceful realm must attract no coalition while a smaller aggressive one does, and clearing
the ledger with nothing else changed must drop both to nothing — which a rank term would not do.

**MEASURED, 16 seeds at the 0 CE epoch:** the lean fires in 6 of 16 worlds, clustered rather than
diffuse, and moves conquests about 6%. **It did not move hegemony at all** — 0 of 16 worlds at a
50% share on both arms, largest share median 10% either way. That is reported rather than tuned
away, and the reading is open: hegemony was already at its floor before this lever existed, so
there was no headroom for it to improve (`NR-830`).

---

## The closure of the Empire era (Ben, 2026-09-11)

§ What the dark age must leave names what the fragments have to **carry**. This section names
what **crosses**, in what form, and how the next phase **judges** it. Together they are the
contract, and the contract is what makes a mechanism inside this phase judgeable at all: a force
that moves no reading below is a force nobody asked for.

**THE PHASE THAT FOLLOWS IS EXPLORATION, AND THE COAST IS GONE (Ben, 2026-09-11).** That phase
is `EXPLORATION.md`'s, and the phase after it `INDUSTRIALISATION.md`'s. Pass 2 was
one economy span, 1560 → 1960, with 1200 → 1560 left unsimulated. Splitting exploration from
Industrialisation replaces both:

| Phase | from | to | years |
|---|---|---|---|
| **Culture** — the migration | 2400 BCE | 400 BCE | 2,000 |
| **Empires** — this phase | 400 BCE | 1200 CE | 1,600 |
| **Exploration** | 1200 CE | 1660 CE | 460 |
| **Industrialisation** | 1660 CE | 1960 CE | 300 |

**1960 is unmoved and 1200 is unmoved**, so this phase's own span (§ The span is 400 BCE to
1200 CE) is untouched. What moved is what sits on the far side of 1200: a simulated exploration
age rather than three and a half centuries of nothing. `GENERATION_STRATEGY.md` § Pass 2 is the
economy pass carries the pass map and takes its figures from here.

**THE READINGS ARE TAKEN AT 1200 CE.** Not at the year the phase happens to stop, and not at the
epoch. A reading taken at a quarter of the span is a reading of a different world, and every
number in this section means nothing until the phase runs its 1,600 years.

---

### Some peoples gain the capacity to explore, and others do not

**Ben, 2026-09-11: this is the output the exploration age is waiting for.** A world in which
everyone can cross has no age of exploration in it, and neither has a world in which nobody can.

**THE CAPACITY IS A THRESHOLD CROSSED, NOT A SPREAD (Ben, 2026-09-11).** A realm either holds the
institutions that let it mount a voyage or it does not, and the contract states it that way — a
per-polity boolean at 1200 CE, read off the Empire tree's rim milestone
(`trees/EMPIRE_TREE.md`, *The Enforceable Promise*, whose only effect opens the next tree). What
is *tuned* is the forces that decide who reaches it; what is *stated* is the threshold.

**TUNED FOR, NEVER CLAMPED, and read distributionally.** The requirement is a property of a seed
spread: across a sweep, some surviving realms hold the rim and not all of them do. No world is
steered to that outcome, no floor is raised to guarantee it, and a world where nobody crosses is a
legitimate seed — it is the *spread* that must contain both, exactly as
`GENERATION_STRATEGY.md` § Asymmetry is the deliverable requires.

**THE SIM READS NODES (Ben, 2026-09-10, `trees/TREES.md`), AND THIS IS WHERE IT STARTS PAYING.**
The seven-domain capacity band cannot express this: a band is a ladder everyone climbs in the same
order, so difference under it is only ever *how far*, never *which way*. The rim is a node with
named `requires`, and holding it is a fact about what a realm built.

**THE FORK IS WHAT MAKES DIFFERENCE INDEPENDENT OF MIGHT (Ben, 2026-09-11).** A closed fork side
goes dark permanently (`trees/TREES.md` § Diffusion follows kind), so two wealthy realms can end
the phase incapable of the same things because they *chose* differently — not because one was
poorer. Without a fork, technological difference collapses back into a restatement of size, which
is the one thing this output must not be.

**THE TREE IS NOT RENDERED DURING GENERATION (Ben, 2026-09-11).** It is surfaced later through the
campaign's tech ledger, and there is no per-polity tree view to render it into. Wiring the tree
into the sim and showing it to a player are separate decisions, and only the first is taken.

---

### Contact is polity to polity

**SETTLED (Ben, 2026-09-11): the contact record holds between POLITIES.** Who has met whom is a
directed record in the same shape as `pass_one_output::grudges` — a named pair, with the event
that joined them.

**Contact is what makes ignorance a fact rather than an absence.** Without it every region sits in
one connected graph and there is nothing for an exploration age to discover; with it, a landmass
nobody has crossed to is a **recorded** state, and so is the first crossing.

**WHAT IS DISCOVERED IS PEOPLE, NOT EMPTY LAND.** § What the dark age must leave declined ground
released back to nobody, and that ruling binds here: the migration fills every habitable landmass
(`COLONISATION.md`), so a far continent at 1200 CE is **settled and unmet**. The new world is a
world of strangers, not of vacancies.

**Landmass identity falls out of contact rather than standing beside it.** A polity-grain record
answers the landmass question by walking it; a second landmass-grain table would be a competing
truth with nothing extra in it.

---

### The directed want — knowing where a good is, is an incentive

**Ben, 2026-09-11, overturning the reading that a want needs a price:** a good you know exists,
that you do not have, is a motive on its own. Coercion is what happens when you know where it is
and can reach it. This phase still has **no market during its span** — § Materials are spent when
something happens is unchanged — and the want does not need one.

**THE WANT IS DIRECTED, in the grudge table's shape: A knows B holds what A lacks.** Not a
per-polity scarcity list, which says only that a realm is short of something and names no object
for the shortage. The directed form is what makes the next phase's first act explicable — *this
realm sailed to that one, for that* — rather than a roll over a list of absences.

**Two inputs, and the second is why the closure spawns markets.** The first is endowment: what
B's ground holds that A's reachable ground does not. The second is **what B's market shows**
(Ben, 2026-09-11) — the conditions a neighbour's market makes visible, which is a richer signal
than geology and the one a trading age actually acts on.

**A want requires contact.** A realm cannot want what it has never heard of, so the want table is
defined only over pairs the contact record already joins — which is what stops it becoming
omniscience with extra steps.

---

### Capitals exist at the close, and markets stand on them

**SETTLED (Ben, 2026-09-11): near the end of the Empire pass, ensure capital cities exist, and
spawn markets centred on them.** That gives the exploration age a market substrate in simplified
form, without giving this phase a market during its span.

**This extends a design the phase already carries rather than opening a new one.**
`../economy/MARKETS.md` § Market centres and seeding records Ben's 2026-09-03 line — *markets
should begin to emerge towards the end of this phase* — and carves markets from the history's
corridor record. What moves is **when**: that carve is a world-gen act at the epoch, and the
closure needs markets standing at **1200 CE**, as an output of this phase rather than an artifact
produced long after it.

**The capital is the seat the polity already governs from** (§ The unit is the city state), so a
market centre is a promotion of an existing fact, never a new placement pass. A polity that ends
the phase without a governable seat has no market, which is the same permission a city state gets
and not a quota.

**A market at the close is a PLACE AND A VISIBLE CONDITION, not an order book.** No firms, no
clearing tick, no price band — those belong to the economy pass. What it carries is enough for a
stranger to look at it and want something.

---

### The network is the estate, and it crosses

**The roads cross the handoff (Ben, 2026-09-11).** § What the dark age must leave names roads that
outlive their builders as one of three things the fragments carry, and a network that does not
cross cannot be inherited by anybody.

**This is what makes collapse consequential rather than merely terminal.** Collapse is network
failure (§ How an empire actually falls), so what an empire leaves is precisely the part of its
network that still stands — and whoever holds that ground starts the exploration age ahead of
whoever does not. Inherited unevenly is the point; an evenly-inherited network is the flat map
§ Asymmetry is the deliverable exists to prevent.

---

### What the closure is judged on

The contract's readings, all taken at **1200 CE** and all read over a **seed spread**, never
per-world:

| Reading | What it must show |
|---|---|
| **Explorer set** | Some surviving realms hold the rim milestone; not all do |
| **Strength spread** | Surviving realms of unequal size — some able to colonise, some only to be colonised |
| **Contact** | At least one pair unmet, so there is somewhere to go |
| **Directed wants** | Non-empty, and pointing across contact rather than within it |
| **Markets** | Standing on capitals, carrying conditions a stranger can read |
| **Inherited network** | Surviving roads held unevenly across the fragments |
| **Grudges** | Still biting at 1200 CE — carried, not decayed to nothing |

**A reading is a requirement, not a target.** No number is set ahead of the measured spread, and a
seed that refuses one of these is a legitimate world; what must not happen is a *spread* that
refuses one. This is the same distributional rule § The arc the phase must produce already states,
applied to the handoff instead of to the arc.

**A mechanism inside this phase is judged by whether it moves a reading here.** That is the whole
purpose of settling the contract before building anything else against it.

---

## What this phase hands the industrial era

**Ben, 2026-09-09 (elicitation notes): this phase feeds directly into the industrial era sim,
where resources are seen as CAPITAL — so it matters specifically how larger empires persist,
expand their reach, and then collapse into smaller nations ready for an industrial boom.**

**That names the arc's ENDING as a deliverable, and the ending is FRAGMENTATION.** Pass 2 is an
economy pass over nations, in two phases — Exploration then Industrialisation, 1200 → 1660 → 1960
(`GENERATION_STRATEGY.md` § Pass 2 is the economy pass) — and nations are what an empire leaves
behind when it stops being able to hold itself. So *a stable dark age* is not merely where the
arc runs out — it is the state in which the industrial era finds its actors.

**A world that ends pass 1 as one hegemon has failed this handoff**, however plausible its
numbers, and so has a world that never assembled anything larger than a city state. Both ends of
that range produce an industrial era with nothing to industrialise against. This is the same
non-hegemony constraint the anti-hegemon levers serve (`BL-823 (anti-hegemon levers)`), read from
the far end: it is not only that a hegemon is dull to watch, it is that the next pass has no input.

**Reach-gating is what makes the collapse mechanical rather than scripted, as designed** — but
`history_sweep`'s own instrumentation (`BL-905`, 2026-09-11, measured against generation's full
1,600-year Empires span, `BL-906`) shows `sustainable_campaign_floor_q` (80, in the 0–1000
currency) refusing a **median of 0 campaigns per world** against ~71,000 legal contacts examined.
Raising the floor to 700 still refuses under 0.2%; only at 900+ does it start biting (94% refused
at 999, collapsing battles from ~815/world to ~34/world) — there is no middle value that refuses a
non-trivial share without refusing nearly everything. The cause: `campaign_supply` is priced from
the STAGING HUB, not the capital (§ SUPPLY PROJECTS FROM THE STAGING HOLDING, NOT THE CAPITAL,
below), so an ordinary neighbour-adjacent campaign's distance term is bounded by
`neighbour_radius` and barely decays the currency — the floor would have to sit near the ceiling
to gate an *ordinary* march at all, which is a different mechanism than "an empire outruns its
own network." This is a genuine design question (does the gate need a different reach basis, an
empire-size-scaled burden, or a much higher floor with a redesigned currency?), not a constant to
re-guess; it is left unfixed here rather than tuned to force a result.

**What is actually filtering campaigns today, measured on the same run:** the water-gate
traversal-legality test (`BL-778`) refuses ~1.4% of contacts; of the ~28% of scored candidates
that clear `campaign_threshold_q`, most that do are chosen, so the load-bearing filters in
practice are the **score threshold** and **verb competition** (Settle/Invest/Consolidate
outscoring Campaign), not reach. The number of nations at 1200 CE is currently a consequence of
those two, not of a network outrunning itself.

**RESOURCES BECOME CAPITAL IN PASS 2, NOT HERE.** This phase abstracts natural resources into what
conquest consumes; the reading of ground as capital belongs to the economy pass and must not leak
backwards into a phase that has no price
(`../economy/MARKETS.md`, and § Materials are spent when something happens).

---

## A realm's name

**A realm is named once, in its founding culture's tongue, and the name is carried by id (Ben,
2026-09-24).** `polity::name` is coined at `founded` by `coin_lexicon` over the founding region's
culture, its `speech` — the same pure function of the tongue that coins that people's cities and
region words, so a realm and everything else in its sound system read as kin
(`NATION_GENERATION.md` § Pass 5 — Naming holds the register of naming sites). Nothing renames it
afterwards: not a re-seating, not the loss of its seat, not a span boundary. The seat a polity rose
from is where the name is coined, never what the name IS.

**The record carries the name beside the region table.** The time-lapse record holds a
`polity_name` table parallel to its polities, as `region_name` is parallel to its regions
(`era_timelapse.hpp`), and every reader — the board, the ticker, and the seat briefing after them —
prints a realm by that table and never by its seat. Because the key is the polity id, a span
resumed from the last handoff carries every living realm's name forward unchanged: a resumed span
inherits, it does not coin.

**Where the seat stands is a fact about a year; the name is not.** A realm's capital at a given
year is a read-side fold of `founded`, the resume's `inherited` restatement and `capital_moved`, and
every mark that sits on a seat takes its place from that fold (`../ui/STARTUP.md` owns what is drawn). **The name never follows the
capital.** A realm that re-seats is the realm it was — "X re-seats itself at Y" — and the name on
the board does not move.

**A resumed span states each living realm's seat without re-founding it.** A span resumed from the
last handoff opens by restating where every living realm sits, so the record replays from that
span alone. That restatement is an `inherited` kind — region = the capital, dated at the span's
start year — and it is ticker-silent, because nothing rose. A `founded` at a resume would tell the
player that a realm they watched hold ground for centuries was born at the span's opening year.

**An ownerless founding is a people settling, not a realm rising.** Ground that organises with no
polity to own it is told as "A people settle at X" — the people, on the region's own name — never
as "A realm rises", because none has.

**The nation inherits the realm's name verbatim.** At the 1960 fold a realm becomes a nation under
the name it was founded with; Pass 5 consumes the name rather than coining another, and which
realm names the nation where the size floor folds two together is that pass's rule
(`NATION_GENERATION.md` § Pass 5 — Naming).

---

## How a fall is told — the ideological axis

**A collapse the player only sees as border changes is half a collapse (Ben, 2026-08-20).** The
story of a fall is told with a dual focus: the **material** side — supply, roads, war, the region
that could no longer be reached — *and* the way the people involved **told it to themselves**.
Real cultures narrated their own doom in a handful of recurring ways, and those narrative moves
transfer under the naming rule exactly as institutions do: the **mechanism** crosses, the
**noun** never does (`.claude/rules/io-standing-rules.md` § Terms & docs). The seeded template
banks that name a polity also mint its self-story, from the library below.

This axis was folded here out of the retired collapse roster; the fold is owned by
`BL-1001 (COLLAPSE doc folded and retired)`, and the roster itself — six polity strategies,
seven culminations, a pairing matrix — is a mechanism reference in
`../research/COLLAPSE_ROSTER.md`, not authority.

### The exits a story can attach to

A fall in this phase has three causes, each recorded under its own name, and a story is seated on
one of them:

- **Secession** — ground the realm can no longer reach walks away as city states (§ How an empire
  actually falls). Recorded as ground taken, parent → successor.
- **Schism** — ground of another people's faith answers together and leaves along kinship rather
  than distance (`../lore/CREEDS.md` § The schism verb). Recorded as faith sundered.
- **Conquest** — a seat taken by a rival's campaign. Recorded as ground taken, loser → conqueror.

There is no voluntary release, no regime reset that leaves the map unchanged, and no exodus: a
realm that breaks always breaks on the map. A story pattern with no exit to seat on stays in the
research roster until the design grows one.

### The pattern library — how a people narrates its doom

Each names the narrative move, then its seat: what generation writes it onto, and what the sim
reads back.

- **Translation of empire.** The centre is not destroyed, it *moves* — every successor claims to
  BE the continuation, not the replacement. *Seat:* the successor's creed after a secession; a
  restoration grudge ("we are the true heir") aimed at sibling successors and not only at the
  parent.
- **The declinist mirror.** A culture narrates its own fall in advance — lost virtue, a corrupt
  centre — for generations before the material break. *Seat:* the narrated history line. Supply
  under a floor is readable before the cut lands, so the chronicle can say "their own writers
  wrote of decay" years before the secession; the player should be able to read the doom coming
  the way the culture's own writers did.
- **The mandate withdrawn.** Legitimacy is a grant from the cosmos, and disaster is evidence the
  grant has moved — regime change thinkable without cultural death. *Seat:* the parent's account
  of what it lost. Under this design it narrates a surviving core, since no exit re-founds a
  realm in place.
- **The golden age behind us.** The material regression is real; the story of greatness is kept
  losslessly. This is the twin of the ladder rule that capacity burns and awareness never does
  (§ How an empire actually falls — what a successor inherits). *Seat:* the restoration creed of
  any successor, and the legitimising claim of whoever later gathers the fragments.
- **The chosen remnant.** A people who walked out narrate smallness as election — purity proved by
  departure. *Seat:* the realm a schism founds; its grudge is faith sundered, and the myth
  outlives any realistic hope of return.
- **The apocalypse reframed as test.** Imminent doom read as trial: a people who expect the end
  can march *through* it. *Seat:* a creed lean that holds ground under strain at the price of
  never letting any of it go — the last stand a realm fights for a region its supply cannot hold.
- **The garden given away.** Release retold as maturity, ties of kinship replacing ties of rule.
  *No seat:* it needs a voluntary release, which this design does not have; it lives in the
  research roster.

### The sim reads the story back

The patterns are not flavour. Two returns:

1. **The creed already chooses the exit.** Which cause a realm breaks under is a property of its
   people, not a roll: ground of the realm's own culture leaves by secession when supply fails,
   ground of another faith leaves by schism when it reasserts. Same strain, different people,
   different ending — which is how the real cases diverged, and it is deterministic throughout.
2. **The story is the inherited character.** Posture, creed and grudge are exactly "the way the
   culture tells its story" carried to the handoff (§ What the dark age must leave, § What the
   closure is judged on). A generated nation does not only have a name; it has an account of
   itself, traceable to a break the player can find in the history log.

**Every break carries its own narrated line.** A secession and a schism are recorded under
different causes so the two readings never blend, and each is told: a realm that lost a third of
its ground in silence reads as a bug, not as a dark age.

**The story surfaces in the history tab, quietly (Ben, 2026-08-20).** Telling it is secondary to
*seeing* it: creed data and the recorded causes are sim substance; narration is a history-tab
layer and the playback, never chrome pushed at the player. Deep-dig is optional, for the players
who want it. The discipline is `../lore/HISTORY.md`'s — **driven, not narrated**: the story is
emitted by what happened, never scripted over it.

### A break travels along the network

Contagion needs no second mechanism. Supply is priced from the capital over the ground the realm
holds (§ How an empire actually falls — the capital is the strategic headquarters), so a region
that secedes takes with it the corridor every region beyond it was supplied through. One cut can
therefore become a cascade, and a whole system of realms can fail in one breath — but only where
the map made them rest on the same ground. That is systemic collapse read as a consequence of the
network rather than as an event of its own, and it is asserted by sweep, never scripted.

---

## The long run is paid for by the table, not the fighting

The sim runs inside world generation, and its budget is the generating screen's wait. The profile
fact that decides everything: **cost tracks the region table, not the battles** — a seed with
fewer fights can run several times longer. Ten times the years is not ten times the cost, because
settling keeps growing the table, so late years cost more than early ones; and the recording of a
long run can cost more than the run itself.

**The shape of the fix, in the order the rungs pay.** Each is independent; measure after each.

1. **Kill the scans that walk the whole table.** A cell → region spatial index removes the
   occupancy scan from settling; per-polity holdings and aggregates are maintained on change,
   never rebuilt per round. This turns per-year cost from O(regions) toward O(changes), and it is
   the prerequisite for everything below.
2. **Event-driven quiet ground.** Most of a long span, most regions do nothing. A stable interior
   region wakes on events — a border change, a supply band crossing, a road lost — not per year.
   Wake conditions are state-derived, never sliced by wall clock.
3. **Banded year grain.** The premise of the span is that ages are long and quiet, punctuated by
   arcs (§ The arc the phase must produce). Quiet bands run at coarse grain with scaled verb
   effects and drop to yearly grain when any realm's supply, war state or nearness to a break
   crosses a band. The grain switch must be a **pure function of sim state** — seeded, replayable,
   asserted by the determinism harness — or it is a die roll wearing a timestep.
4. **Record on change, at the reader's grain.** The playback and the history tab scrub decades.
   Snapshot ownership when it changes and per decade otherwise; narrated lines are events, not
   per-year state.
5. **Never parallelise the sim.** Threading the polity loop trades a seconds problem for a
   determinism problem the standing rules forbid. Single-threaded plus algorithmic fixes is the
   whole toolbox.

**What not to trade away.** Never fix by capping regions — table growth *is* the history running
longer. Every number is measured on the sim that generates a world, at the optimised build,
through the same parameter derivation, never on a harness divergence.

**Fit check.** Rungs 1–2 target the dominant cost, rung 3 the year count, rung 4 the recording
half. If all four together still miss the budget, the honest fallback is fewer completed arcs
per world with every arc shape still asserted across the seed spread — degrading density, never
determinism or legibility.

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
