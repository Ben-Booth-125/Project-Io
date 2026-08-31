# Project Io — Climate

Authority doc for **climate**: the state of a body's habitability as it changes during play, what
changes it, and what that does to the people and corporations living under it.

**Scope, stated first.** Climate is **Era 2's catastrophe** — Early Space — not Era 1's. Ben,
2026-08-31: *"I am not against climate change being a large problem for Era 2, but our prototype
works solely on Era 1 for now."* This document designs it **ahead**, so the Era ladder has a second
rung that is a real design rather than a placeholder. **Nothing in Era 1 is built against it**, and
no other doc should lean on it as though it were available.

That placement is also the right one on its own terms: climate state is per body (§ 5), so the
catastrophe only has its full shape once more than one body is reachable — which is Era 2's
territory.

---

## 1. The shape it inherits

`docs/economy/ERAS.md` § The point of an Era makes every Era the same kind of object: **one
catastrophe the player is playing to avoid**, resolved deterministically from a quantity the player
can watch and act on, with attractive technologies whose real cost is not on their own stat line.

Climate is that pattern with a different quantity. Era 1's catastrophe is a nuclear war and its watched
scalar is **Alarm** — how threatened other nations feel. Era 2's is a habitability collapse and its
watched scalar is **Load** — how hard the world is being pushed.

The parallel is deliberate and load-bearing. It is what makes the Era ladder a **system** rather
than a series of unrelated set-pieces, and it means the machinery built for the first rung — a
watched accumulating scalar, a seeded test date, red herrings with tells, deed-gated capstones —
is the machinery the second rung reuses.

| | Era 1 — Terrestrial | Era 2 — Early Space |
|---|---|---|
| Catastrophe | Nuclear war | Habitability collapse |
| Watched scalar | **Alarm** — how threatened others feel | **Load** — how hard the world is pushed |
| Raised by | Visible capability, severed trade, posture, unrest | Extraction and processing at scale |
| Relieved by | Trade interdependence, disclosure, inspection | Reduced draw; remediation; moving load off-world |
| Failure lands on | The assets the space programme needed | The ground the population lives on |

---

## 2. The surface — planetology's scalars, unfrozen

Io already has a detailed, physically grounded climate model. `docs/generation/PLANETOLOGY.md` runs
the carbonate–silicate thermostat, the greenhouse budget, insolation, oxygenation and the arable
calculation, then deposits the result in `planetology_state` (`src/world/planetology.hpp`) — where
it is computed **once, at generation, and never changes**.

**Climate change is a defined subset of those scalars being allowed to move.** No parallel model, no
invented vocabulary, and the physics is already written down.

| Scalar | Meaning | Under climate |
|---|---|---|
| `instellation` | S — sunlight reaching the surface | **Moves** |
| `surface_temp_k` | Surface temperature | **Moves** |
| `arable_share` | Fraction of land that can be farmed | **Moves.** The number players feel |
| `life_stage` / `peak` | Biosphere attainment | **Moves, downward only** — the severe case |
| `o2_fraction` | Atmospheric oxygen | **Moves slowly**, if at all |
| `endowment` | Per-resource deposit multiplier | **Fixed.** Deposits are geology |
| `mass`, `orbit_au`, `v_esc_kms`, `mobile_lid` | Planetary facts | **Fixed** |

The dividing line: **climate moves what a world can currently support, never what it is made of.** A
ruined world still has its iron.

**Not `hazard`** (Ben, 2026-08-31): *"we will use a more detailed surface than hazard. Hazard is
mostly just a tangible benefit for larger settlements on other bodies."* One number per tile with one
meaning cannot carry a body-scale state with several, and hazard's design job is the off-world
settlement calculus.

**Why these scalars are the right surface.** They already have consumers. `arable_share` feeds the
tile pass and the food chain; `life_stage` gates living resources and the endemic goods;
`instellation` and `surface_temp_k` are what the generation model reasons in. A climate expressed
here is felt correctly across the game because the causal wiring was built by the layer that produced
the world in the first place.

---

## 3. What actually happens

**1. Food fails first, and food is a chain.** A falling `arable_share` cuts agricultural output.
`docs/economy/POPULATION.md` centres demand food rations every tick and already carry **decline and
razing**. So the first thing a player sees is not a weather readout — it is their workforce
shrinking because the centres feeding it are starving.

**2. The growing bands move, and that is a trade event.** Endemic goods are pinned to **latitude
bands** — the climate a good wants — and their whole economic point is being *valuable for being
somewhere else* (`docs/economy/RESOURCES.md` § Mercantile). When bands migrate, a cash crop's home
moves, shrinks, or ceases to exist. A trade route built on an endemic good is one climate can end.

**3. The coast is a real place.** Population footprints are already coastal-clipped and ports sit on
water access. The slowest channel, and the least reversible.

**4. The point is redistribution, not penalty.** Climate change does not make everything worse for
everyone by the same amount — **it moves where value is.** Ground that was too cold becomes arable;
a band that fed a continent stops. A corporation holding the right ground gains; one holding the
wrong ground must move or trade. That is what makes climate a **Trade** system rather than a tax,
and it is what satisfies `docs/SYSTEMS.md`'s test: it changes what the company must answer to.

---

## 4. Red herrings — the same discipline

Per `docs/economy/ERAS.md`, the technologies that raise the watched scalar must be the ones that look
best, and **every trap carries a tell that precedes commitment.** The herring kinds transfer
directly; only their content changes. Sketched, not settled:

- **Escalator** — a genuinely better extraction or processing method whose throughput is the load.
- **Brittle optimisation** — a method that is dominant in a stable climate and fails as bands move.
- **Contextual dud** — advanced, and wrong for *this* body's chemistry.
- **Interdependence severer** — local substitution that removes the trade which would have fed you
  when your own bands failed.
- **Tempo trap** — a fine technology that costs the clock while load accumulates.
- **Inverse herring** — at least one that looks dirty and is stabilising, so "looks industrial"
  cannot become the tell.

---

## 5. Grain — per body

Climate state is held **per body**. Not per tile — that is a pollution puddle: local to the ground
that caused it, dodgeable by moving, and a brake on nobody. Not per world — the game's territory is
a solar system of genuinely different atmospheres, and a shared figure would have an asteroid's
extraction degrading a homeworld's air.

Per body is the commons that matters, and it scales correctly: spreading production across bodies is
also **spreading load**, which is why this catastrophe belongs to an Era where more than one body is
reachable.

---

## 6. Legibility is part of the mechanism

Same rule the Alarm scalar carries. The body's climate state is **visible as a number and a trend**;
a corporation's **own contribution** is visible to it; the consequence is **attributable**. A player
who experiences climate as "my output got worse for no stated reason" has been cheated, however
correct the arithmetic.

---

## 7. The constraint on pressure (Ben, 2026-08-31)

> *"'how far restraint goes' should never exclude extension and construction. If a player loses out,
> they should be able to see that the world doesn't wait for them."*

Climate makes operating **costly** and **relocates value**. It never makes operating impossible — no
vetoed construction, no tile removed from play, no gated building type. A degraded body is a worse
place to operate, never a closed one.

---

## 8. What this document does not settle

1. **The recovery curve** — how much load is reversible, and where a permanent floor sits.
3. **What exactly constitutes load**, and the weighting across extraction, processing and throughput.
4. **Whether remediation is buildable**, which would make it a market rather than only a cost.
5. **Whether nations act on it** — a levy or a law answering climate would reach the 2026-08-18
   nation grant (`.claude/rules/io-standing-rules.md`).
6. **The test's shape** — whether climate's catastrophe resolves on a seeded date like Alarm's, or
   on a threshold crossing whenever it happens.

---

## 9. Where the parts live

| Subject | Doc |
|---|---|
| The Era model this inherits — catastrophe, scalar, test, herrings | `docs/economy/ERAS.md` § The point of an Era |
| The physical model and its scalars | `docs/generation/PLANETOLOGY.md` |
| Climate bands as a terrain and deposit input | `docs/generation/TILE_GENERATION.md`, `docs/economy/TILES.md` |
| Endemic goods and their latitude bands | `docs/generation/PLANETOLOGY.md` § Endemism |
| Centres, habitability, decline and razing | `docs/economy/POPULATION.md` |
| Hazard as the off-world settlement calculus | `docs/economy/PRODUCTION.md` |
| The drafted first-rung tree and herring roster | `docs/research/ERA1_TECH_LANDSCAPE.md` (research) |
