# Kepler — Creeds

One pantheon per cradle-culture, each in its own generated tongue, and the
globalisation that renders the record in the player's language. Implemented in
`src/world/creeds.{hpp,cpp}` (BL-235, written 2026-07-31); verified by
`tools/verify/creeds_harness.cpp` (C1–C4). Companion to `HISTORY.md` (the
ladder this pass interleaves with) and `../generation/NATION_GENERATION.md`
(the political map it drives).

## The rule: one pantheon, one tongue

Each agrarian cradle (HISTORY.md Stage 0, BL-221) becomes a **culture**. A
culture rolls a small phonology — its own consonant and vowel inventory — and
every proper noun it coins (its own name, its gods) is built from that
inventory. **One pantheon per culture** (Ben, 2026-07-31): the tongue and the
creed are the same act of self-description, which is why two cultures' gods
*sound* different rather than being restyled from a shared list.

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

Inherited from BL-221's rule: **it drives, it does not narrate.** A culture
whose aggression (derived from its war god's zeal and dominion) clears its
neighbour's defence *plus the ladder's conquest cost* marches; a won war
**welds** two cradles and lowers `fragmentation_q` before
`nation_params_from_ladder` reads it. Warlike creeds therefore grow fewer,
larger polities — the war simulation Ben deferred nation-count consolidation
to, in its first honest form. Welding is floored at half the incoming
fragmentation, so creeds alone cannot manufacture a hegemon (BL-224's
invariant is respected, not resolved, here).

## Globalisation and the common tongue

Generation closes with one fixed event (1951): the common trade tongue
spreads. From that hinge the record is rendered in **the player's language**
— English for now, as the development language (Ben, 2026-07-31: the common
tongue is whatever the player picks to play in). Proper names stay native:
the old tongues survive in the names of gods.

> **Reconciliation owed (2026-07-31):** the 1951 date and single-wave mechanism are
> provisional — a common tongue spreading while the rupture *threat* stands must square with
> BL-223 (averted rupture)'s bloc structure. HISTORY.md § "Where the rupture sits" carries the
> same note; BL-223 owns the answer.

## Honest scope

- The culture unit is the **cradle**; BL-218 (culture regions) will refine it
  into clustered sub-national regions rather than replace it.
- Pantheons currently write history and drive fragmentation only — no live
  religion mechanic, no player surface. The Population lens / diplomacy layer
  are the intended future readers.
- The tribal wars are single-round pairwise marches, not a campaign
  simulation; BL-219's sweep is the tuning window for how often they weld.
