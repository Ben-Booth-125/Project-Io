# Kepler — Creeds

> **Settles:** how a cradle culture rolls its own phonology and coins its lexicon · why one
> pantheon per culture · what a creed's drives do at the tribal-conflict stage · where a
> pantheon sits on the ground · how globalisation renders the record into a common tongue.
> **Not here:** which names each generation pass draws from a tongue, and on what terms
> (../generation/NATION_GENERATION § Pass 5 — Naming, the register of naming sites) · the
> stage ladder this pass interleaves with (HISTORY) · how a polity narrates its doom
> (COLLAPSE).
> **Confused with:** HISTORY.md, COLLAPSE.md, ../generation/NATION_GENERATION.md.

> **The tongue is coined here and CONSUMED elsewhere.** This doc owns the phonology and the
> lexicon; it does not own the naming sites. Which passes draw on a tongue — and the one
> sanctioned place an English structural word survives — is registered in
> [NATION_GENERATION.md](../generation/NATION_GENERATION.md) § Pass 5 — Naming.

One pantheon per cradle-culture, each in its own generated tongue, and the
globalisation that renders the record in the player's language. The pass is
`src/world/creeds.{hpp,cpp}` (BL-235, creeds), verified by
`tools/verify/creeds_harness.cpp` (C1–C4). Companion to `HISTORY.md` (the
ladder this pass interleaves with) and `../generation/NATION_GENERATION.md`
(the political map it drives).

## The rule: one pantheon, one tongue

Each agrarian cradle (HISTORY.md Stage 0) becomes a **culture**. A culture
rolls a small phonology — its own consonant and vowel inventory — and every
proper noun it coins (its own name, its gods) is built from that inventory.
**One pantheon per culture** (Ben, 2026-07-31): the tongue and the creed are
the same act of self-description, which is why two cultures' gods *sound*
different rather than being restyled from a shared list.

The archetype table is distilled from the Pantheon content base (the sibling
Pantheon project): gods carry two temperament axes — **zeal** (how much the
god relishes battle) and **dominion** (how surely it expects to prevail) —
and the pantheon's *shape* is the land's portrait:

| Land signal | Seat raised |
|---|---|
| Coastal cradle | chief god of **the sea** |
| Wetland-dominant | chief god of **the river** |
| Forest-dominant | chief god of **the green dark** |
| Otherwise | chief god of **the storm** |
| Always | a **war** god and **the door of the dead** |
| Ore in the cradle's window | **the forge** |
| The charter cradle | **the sealed oath** — the creed Stage 1's Charter Act grows from |

Harsh ground (the barrier share of the cradle's own window) raises every
god's zeal floor: mountains breed harder creeds.

## The creed drives — the tribal-conflict stage

Inherited from the ladder's rule: **it drives, it does not narrate.** A
culture's `aggression_q` is derived from its war god's zeal and dominion and
its chief god's zeal, and it is the **doctrine input** the Era −1 sim reads.
That is the whole of what a creed does to force.

**A creed no longer fights its own war, and no longer sets the nation count
(Ben, 2026-09-09).** The tribal marches — single-round pairwise comparisons
at cradle grain, a scalar attack against a scalar defence, welding two
cradles and lowering `fragmentation_q` — are retired.
[`../generation/COLONISATION.md`](../generation/COLONISATION.md)
§ Fragmentation comes from contact owns what replaced them.

Two reasons, and the second is the load-bearing one. Their **timing** was
wrong: they are wars, inside a span whose premise is that spreading rather
than fighting is the process. Their **grain** was wronger still: a cradle
under diffusion is a source, not a point, so the objects the marches compared
had stopped describing where two peoples actually meet.

What this separates is worth stating, because it was tangled before.
`aggression_q` used to drive both *how consolidated the political map is* and
*how a polity fights once it exists*. It now drives only the second; how far
a people spread drives the first. Two causes for two effects.

The overturning that stands unchanged: Ben replaced abstract war with
**simulated history** on 2026-08-02, so every war in Kepler fights with real
typed units and doctrine through `resolve_battle` (HISTORY.md § The Era −1
sim). The marches were the last scalar-comparison war anywhere in the design;
retiring them finishes that ruling rather than amending it.

## Where a pantheon sits on the ground

The culture unit is the **cradle**; settlement refines it into **regions**
(`src/world/settlement.cpp`) without replacing it. A region inherits the
culture of **the stream that reached it**, so a pantheon is mapped onto
specific ground and specific ancient deposits, and the distribution of gods
across the map is a record of who walked where.
[`../generation/COLONISATION.md`](../generation/COLONISATION.md) § Culture
arrives by route owns which stream that is — and supersedes the straight-line
nearest-cradle assignment this section previously stated (Ben, 2026-09-09).

Pantheons do three things: write history, drive fragmentation, and **bias
industrialisation timing** — a forge culture's ore regions light up earlier,
and the charter culture's oath god buys a smaller bonus. Each bias is the
same fact as the endowment read one stage apart: a forge god only exists
where the cradle window held ore (HISTORY.md § Settlement).

**Conquest spreads a pantheon.** A won war plants the victor's gods on the
regions it takes and rededicates their shrines — and destroys part of the
loser's written record in the process. The gods travel with the border. A
conquered region records its founders in `founding_culture` and its
conquerors in `culture`, which is the pair a religion or diplomacy layer
needs to describe a grievance. The Population lens and the diplomacy layer
are that pair's intended readers; a live religion mechanic — creed axes that
bias which culmination a strained polity falls toward, and the myth bank that
tells it — is BL-487 (polity creed axes) and BL-300 (myth/theology), designed
in `COLLAPSE.md` § Telling the story.

## A creed that spans cultures — the universalising turn (Ben, 2026-09-11)

> *"We should use this phase to deepen religious understanding and turmoil... the prior work done
> on pantheons, being subsumed by [a universal faith], and providing a unity which spans through
> culture and language difference. This also provides another angle from which empires fall."*

**THE NAME IN THE QUOTE IS AN ANALOGY FOR THE READER AND NEVER CONTENT.** The standing rule is
unconditional: real history is a mechanism reference, never a name source. What transfers is the
*mechanism* — a creed that stops being one people's pantheon and starts being an answer for
everyone. What must never transfer is the proper noun. The universalising creed is generated and
named by the same phoneme banks and template tables as every other creed in this document.

**WHAT IT IS, AS A DEPARTURE FROM § The rule: one pantheon, one tongue.** Every creed above is
*local by construction* — a pantheon belongs to a cradle culture and travels only as that people
travels, which is what made the distribution of pantheons a record of routes
(`../generation/COLONISATION.md` § Culture arrives by route). A universalising creed inverts
exactly that property: it **subsumes** the pantheons it meets rather than displacing their
peoples, and it therefore spreads along contact rather than along ancestry.

**WHY IT EARNS ITS PLACE — it does three jobs no existing mechanism does.**

1. **UNITY ACROSS DIFFERENCE.** `../generation/CIVILISATION.md` § Culture relations makes kinship
   and opposition the engine of conquest, and both are computed from *descent* — how long ago two
   peoples parted. A shared creed is the first thing in the design that can bind two peoples who
   are **not kin and do not share a tongue**. That is a genuinely new axis, not a re-weighting of
   an existing one, and it is what makes an empire able to hold ground its own culture never
   walked.
2. **A MOTIVE FOR EXPANSION THAT IS NOT HUNGER FOR LAND.** The colonial era the Empires phase must
   set up (§ What the dark age must leave) is historically driven as much by mission as by
   material want. A creed that believes it is *for everyone* supplies a reason to cross water
   toward people rather than toward ground — which is the reason a colonial era looks different
   from a border war.
3. **A NEW WAY FOR AN EMPIRE TO FALL.** This is the half Ben names last and it is the sharpest.
   § What the dark age must leave settles collapse as **network failure** — a realm that cannot
   reach itself. A creed adds a second, independent fracture: an empire whose subject peoples
   share its faith is held by something its roads do not carry, and an empire that **splits over
   the faith** — schism, or a subsumed pantheon reasserting itself — fractures along lines that
   have nothing to do with distance. Two failure modes that can fire separately is what stops the
   dark age having a single shape.

**THE TURMOIL IS THE POINT, NOT A SIDE EFFECT.** "Deepen religious understanding AND turmoil"
names both halves. A creed that only ever unified would flatten the world — the same objection
`../generation/CIVILISATION.md` § A civilisation is what mixing makes raised against *resolve* as
a formation rule, and rejected there for the same reason. Subsumption must leave residue: the
pantheon that was absorbed is still underneath, and it is what a schism is made of.

### The four calls, settled (Ben, 2026-09-11)

**IT IS COINED NEW, AND BELONGS TO NOBODY.** A universalising creed is not one pantheon that grew
— it is a new creed, generated by the same phoneme banks, with no cradle culture behind it. This is
the choice that makes the departure real rather than cosmetic: a creed risen *from* a pantheon
would still be that people's creed wearing a larger name, and every other people would read its
spread as that people's spread. Belonging to nobody is what lets it bind peoples who are not kin,
and it is what makes the subsumed pantheons **residue underneath** rather than ancestors above.

**IT ARISES FROM HUMILIATION AND FROM DENSITY, never from a roll or a date.** Two upstream causes,
and a creed needs them together:

- **A realm has been shattered or humbled.** The obvious cause runs the other way — a triumphant
  empire declaring its god universal — and it is the wrong one. A creed that is *for everyone*
  answers a question the victorious do not have. Collapse is already mechanical here
  (`../generation/CIVILISATION.md` § How an empire actually falls), so this reads a fact the sim
  already produces and adds no new event.
- **A dense trade network.** An answer for everyone needs everyone to be in contact. The network
  is what carries the creed, and it is already measured — `region::network_supply_q` per held
  region, and § War is paid for's unlike-pair count per realm.

Together they are a *consequence of upstream scalars*, which is the standing constraint on this
whole phase. A world of intact, isolated realms produces no universalising creed, and that is a
legitimate world rather than a failed one.

**A PEOPLE HOLDS BOTH, AND THEN IT RESOLVES.** Conversion is not a flip. A people carries its
pantheon *and* the universal creed for a span, and the pair then settles one of two ways — the
old pantheon fades into residue, or it **reasserts** and the people falls out of the creed. Which
way it goes is what a schism is made of, and it is why the turmoil half of the design has anything
to work with. A single-state model — converted or not — would have left the fracture nothing to
fracture along.

**THE POLITY ADOPTS IT; ITS PEOPLES CONVERT UNEVENLY AFTER.** The two grains are separate states
and they are expected to disagree. A realm adopts the creed as an institution — that is what makes
it able to hold ground its own culture never walked — while the peoples under it convert at their
own pace, or refuse. **The gap between the two is the fault line**: an empire whose institution is
universal and whose subject peoples are not is exactly the empire that splits over faith rather
than over distance.

---

## Sea legs — a creed that can feed a force across water (Ben, 2026-09-11)

> *"I think we should have room for forces surviving a crossing. This could be something which we
> give to certain regions as their creed develops."*

**THE REFERENCE IS A MECHANISM AND THE NAME IS NEVER CONTENT** — the same unconditional rule this
document opens with. What transfers is a raiding tradition that makes an overseas landing
sustainable; the creed is generated and named like every other.

**THE PROBLEM IT SOLVES, MEASURED.** `../generation/CIVILISATION.md`'s water gate was opened by
weight — wet contacts became **legal** and refusals fell 45% — and *no outcome changed*. A force
crossing water it does not own starves, because forage requires dry ground or a shore, so the
campaign is allowed, scored, scores terribly on supply, and loses every verb contest. Legality and
desirability are separate gates and only the first had been opened.

**THE STARVATION IS NOT A BUG.** If a crossing foraged normally, coastal ground would be *cheaper*
to take than inland ground, which inverts the whole intent. So the fix must make **some** crossings
survivable without making any of them free.

**A REDUCED RATION, SCALED BY HOW SEAFARING THE CREED IS.** Not full forage — that restores the
inversion — and not a flat allowance either. A creed with deep sea legs lands nearly fed; one with
shallow sea legs lands hungry but alive; one with none still starves. That makes the ability a
spectrum a player can read rather than a switch, and it keeps the ordinary case — a landlocked
people trying a crossing — refused or beaten exactly as it is today.

**IT IS EARNED FROM THREE UPSTREAM FACTS, ALL OF THEM ALREADY RECORDED** (Ben, 2026-09-11):

- a **coastal cradle** — the culture began on the water;
- **a crossing in its migration** — `../generation/COLONISATION.md` § the coastal and overseas
  routes already records which peoples actually went over water to get where they are;
- **a sea or storm god in its pantheon** — the creed's own temperament, read through the same
  channel `aggression_q` already uses.

None of the three is a roll. Each is a fact about what happened to these people, which is what
makes sea legs a **force with a visible cause** rather than a term inside a scorer.

**THE POLITY HOLDS IT, BUT ONLY FROM A PORT.** The property is used by the polity staging the
crossing, not by the creed in the abstract — a realm either has the tradition available to it or
does not. Two conditions, both required: the staging region's people must carry the sea legs their
creed earned, **and** the staging ground must actually touch water. A realm that inherits a
seafaring people and holds no port cannot use what it inherited.

**AND IT REQUIRES A NAVY AS WELL.** Sea legs do not substitute for `can_field_naval`. The two
answer different questions — *can this realm put hulls on the water at its military band* and *can
these people feed a force once it lands* — and a crossing needs both. Keeping them independent
conditions rather than one merged test is what stops an ancient people out-raiding a realm that has
actually reached the naval rung.

**WHAT IT PAIRS WITH.** § A creed that spans cultures gives a creed a reason to cross water toward
*people*; this gives it the means to arrive fed. Together they are what makes a colonial era
mechanically possible rather than merely motivated.

---

## Globalisation and the common tongue

For a modern-era epoch, generation closes with one fixed event (1951): the
common trade tongue spreads (`record_globalisation`). From that hinge the
record is rendered in **the player's language** — English for now, as the
development language (Ben, 2026-07-31: the common tongue is whatever the
player picks to play in). Proper names stay native: the old tongues survive
in the names of gods.

Under the ancient epoch (0 CE — HISTORY.md § The epoch and the run) there is
no globalisation hinge: the record is rendered in the player's language
throughout, and proper names stay native exactly as above. How a common
tongue squares with the bloc structure of the averted rupture belongs to
BL-223 (averted rupture) with the rest of the post-epoch world.
