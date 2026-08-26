# Project Io — Research

> **Stub authority doc** (Ben, 2026-08-25: *"Write a stub authority doc for RESEARCH.md now, it
> can be something we will develop in the future, but it helps to have the concept in writing."*)
> This is the concept in writing; the full system is a design session of its own (BL-619,
> research system). What is stated here is settled; everything else is open.

Research is the loop that converts an educated population into **technology**: universities
produce **research points (RP)**, and RP purchases unlocks.

## What is settled

- **Universities produce RP.** The university is a building, placeable only in a City (4)+
  population centre (`docs/economy/POPULATION.md` § Strata gate buildings), and it does two
  things at once: raises the host nation's qualification fraction, and generates RP.
- **RP purchases unlocks.** The consumption side already has three live touchpoints:
  - the **era gate** — "Rocketry research purchased" is one of the keystone-quest conditions
    (`docs/economy/ERAS.md`; BL-087, keystone quest);
  - **recipe unlocks** — the `unlock_recipe` tech arm (BL-588, unlock recipe tech arm);
  - **province-ceiling relief** — technology for deeper mines and denser facilities is the
    sanctioned reliever of the building ceiling (`docs/generation/PROVINCES.md` § The building
    ceiling; BL-513, province building ceiling).
- **Deterministic like everything else.** RP accrual and unlock resolution are seeded, pure and
  replayable; a rival corp or nation reaching an unlock does so through the same verbs and the
  same arithmetic.

## Research consumes goods, not only credits

Settled 2026-08-26 (Ben): research is a **demand channel**
([`MARKETS.md`](MARKETS.md) § Demand channels), not only a credit sink. RP accrual draws a basket of
real goods alongside whatever it costs in credits, which does two things at once — it gives the top
of the production chain a buyer, and it makes the tech ladder an *economic* decision rather than a
free accumulator that only time gates.

Two properties this must keep, and they are why it belongs here rather than in the budget:

- **The basket is era-banded**, like every other demand basket — an ancient ladder consumes tools
  and cloth where an industrial one consumes electronics and machinery.
- **A shortfall slows research; it does not void it.** The same shape every other goods draw takes
  (§ the shortfall rule in FINANCE.md): unmet input weakens the output rather than cancelling it, so
  a research programme starved of inputs is *behind*, never broken.

Design: BL-645 (research consumes goods). The rates, and whether the draw sits on the university or
on the nation's budget lines, belong to this doc's own design session (BL-619) rather than being
settled ahead of it.

## What is open

- The RP economy: accrual rates, whether RP is per-corp, per-nation, or both, and whether it
  trades.
- The tech graph: `scripts/tech_tree.lua` holds ~150 nodes of deliberately inert data with one
  resolving gate — whether that data becomes this system's graph or is replaced is undecided.
- The relationship between corp research and nation research, and whether a nation's budget
  (`docs/politics/NATIONS.md`) can fund it.
