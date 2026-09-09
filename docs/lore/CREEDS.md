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
