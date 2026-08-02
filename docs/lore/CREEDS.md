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

- The culture unit is the **cradle**; BL-218 refined it into **provinces**
  (landed 2026-08-02, `src/world/settlement.cpp`) rather than replacing it — a
  province inherits its nearest cradle's culture, so a pantheon is now mapped
  onto specific ground and specific ancient deposits. See `HISTORY.md`
  § Implementation — Stages 3–4.
- Pantheons now do three things: write history, drive fragmentation, and — since
  BL-218 — **bias industrialisation timing** (a forge culture's ore provinces
  light up earlier, the charter culture's oath god buys a smaller bonus). Still
  no live religion mechanic and no player surface. The Population lens /
  diplomacy layer are the intended future readers, and BL-218 hands them the
  pair they need: a conquered province records its founders in
  `founding_culture` and its conquerors in `culture`.
- **Conquest spreads a pantheon.** A won rupture-war plants the victor's gods on
  the provinces it takes and rededicates their shrines — and destroys part of
  the loser's written record in the process. The gods travel with the border.
- The tribal wars are single-round pairwise marches, not a campaign
  simulation; BL-219's sweep is the tuning window for how often they weld.
- **Abstract war is on notice (2026-08-02).** Ben overturned the
  scalar-comparison half of the war rule: simulated history is to fight with
  **real typed units and real tactics**, the same engine the main era later
  inherits — BL-272 (unit/doctrine combat model) owns the replacement, and
  BL-271 (Era −1 sim) is where these one-shot marches become a running
  campaign simulation. "Drives, not narrates" survives untouched.
