# Project Io — Climate

Authority doc for **climate**: the state of a body's habitability as it changes during play, what
changes it, and what that does to the people and corporations living under it. Ben, 2026-08-31:
*"we haven't built a critical system which is climate."*

Climate is a **commons** — shared by every actor on a body, altered by use, owned by nobody — and
it is the one world force that answers back to what corporations do at scale.

---

## 1. Two corrections that shape this document (Ben, 2026-08-31)

**Climate does not run on `hazard`.** *"When we model climate, we will use a more detailed surface
than hazard. Hazard is mostly just a tangible benefit for larger settlements on other bodies."*
`tile_component.hazard_level` is the local operating-difficulty scalar in the production formula,
and its real design job belongs to the **off-world settlement calculus** — what it costs to put a
large settlement somewhere unwelcoming. It is far too coarse to carry a climate: it is one number
per tile with one meaning, and climate is a body-scale state with several.

**Era 1 is entered through nuclear war.** `docs/CONCEPT.md` § Eras already had the Era 0 exit as a
global-rupture-scale war that reshapes the world enough for rapid space expansion to become
plausible. The rupture is **nuclear**, and it is the single most important fact about this system:
climate in Io is not only a slow industrial drift, it is also a **shock with a date**, and the shock
is what makes the era arc happen.

> Both eras' climate is the same model. What differs is whether the state is drifting or was hit.

---

## 2. The surface — planetology's scalars, unfrozen

Io already has a detailed, physically-grounded climate model. `docs/generation/PLANETOLOGY.md` runs
the carbonate–silicate thermostat, the greenhouse budget, insolation, oxygenation and the arable
calculation, and deposits the result in `planetology_state` (`src/world/planetology.hpp`). That
state is computed **once, at generation, and then never changes**.

**Climate change is a defined subset of those scalars being allowed to move.** No parallel model, no
invented vocabulary, and the physics is already written down.

| Scalar | Meaning | Under climate |
|---|---|---|
| `instellation` | S — sunlight reaching the surface, Earth units | **Moves.** The nuclear-winter channel: aerosol loading cuts effective S hard and fast |
| `surface_temp_k` | Surface temperature | **Moves.** The consequence of S and of greenhouse loading |
| `arable_share` | Fraction of land that can be farmed | **Moves.** The number players actually feel |
| `life_stage` / `peak` | Biosphere attainment | **Moves, downward only.** The severe case: a biosphere that loses a rung |
| `o2_fraction` | Atmospheric oxygen | **Moves slowly**, if at all — a long-horizon quantity |
| `endowment` | Per-resource deposit multiplier | **Fixed.** Deposits are geology, not weather |
| `v_esc_kms`, `mass`, `orbit_au`, `mobile_lid` | Planetary facts | **Fixed.** These are what the body *is* |

The dividing line: **climate moves what a world can currently support, never what it is made of.**
A ruined world still has its iron.

**Why this is the right surface and hazard was not.** These scalars already have consumers.
`arable_share` feeds the tile pass and the food chain; `life_stage` gates living resources and the
endemic goods; `instellation` and `surface_temp_k` are what the generation model reasons in. A
climate expressed here is felt correctly across the game because the causal wiring was built by the
generation layer that produced the world in the first place.

---

## 3. Two regimes — the drift and the shock

**The drift (Era 0).** Industrial activity loads the atmosphere. The change is slow, cumulative,
attributable, and shared: every corporation operating on the body contributes in proportion to what
it extracts and processes, and every corporation lives with the result. This is the commons, and it
is the regime that does the AI-brake job in `docs/ai/AI_OPPONENT.md` § Where restraint comes from —
the largest operator pays the largest share of a cost it imposed on the field, with nothing naming
the leader and nothing reading the player's seat.

**The shock (the Era 0 → Era 1 rupture).** A nuclear war is a **step change, not a trend**.
Instellation falls sharply, temperature follows, and the arable share collapses within a handful of
ticks rather than over a campaign. `docs/economy/ERAS.md` § What moves an Era already specifies the
boundary as a seeded event that shocks markets and destroys infrastructure; the climate shock is the
part of it that does not end when the shooting does.

**The aftermath (Era 1).** Winter lifts as aerosols settle — recovery is real and observable — but
it does not return to where it started. What is left is a **degraded floor**: a homeworld that
supports meaningfully less than it did, permanently.

The drift is what makes the shock plausible. A world already under strain, whose corporations could
see the meter and did not coordinate, is a world where the war is a **consequence** rather than a
scheduled event. `docs/economy/ERAS.md` puts it exactly right already: *the backstory establishes
that these powers* can *pull back from the brink; the Era 0 exit is the occasion they do not.*

---

## 4. What actually happens — the consequences, in Io's terms

Climate is only worth building if its effects land on things the player already cares about.

**1. Food fails first, and food is a chain.** A falling `arable_share` cuts agricultural output.
`docs/economy/POPULATION.md` centres generate demand for food rations every tick and already carry
**decline and razing** when supply and habitability fall short. So the first thing a player sees is
not a weather readout — it is their workforce shrinking because the centres feeding it are starving.

**2. The growing bands move, and that is a trade event.** `docs/generation/PLANETOLOGY.md`'s endemic
goods are pinned to **latitude bands** — the climate a good wants — and their whole economic point
is being *valuable for being somewhere else* (`docs/economy/RESOURCES.md` § Mercantile). When bands
migrate, a cash crop's home moves, shrinks, or ceases to exist. A trade route built on an endemic
good is a trade route climate can end.

**3. The coast is a real place.** Population footprints are already coastal-clipped and ports sit on
water access. Sea level is the slowest of these channels and the least reversible.

**4. Winter is a famine, not a heat wave.** The nuclear case is fast, deep, and food-shaped. It is
not the same experience as the drift and should not read as more of it.

**5. Contamination is spatial, severe, and temporary.** Fallout makes ground bad to work for a time
and then decays. It is the one climate effect with a *map*, and it is how the rupture is felt
locally rather than only as a global number.

**6. The point is redistribution, not penalty.** This is the design's centre of gravity. Climate
change does not make everything worse for everyone by the same amount — **it moves where value
is.** Ground that was too cold becomes arable; a band that fed a continent stops. A corporation
holding the right ground gains, and one holding the wrong ground must move or trade. That is what
makes climate a **Trade** system rather than a tax, and it is what satisfies
`docs/SYSTEMS.md`'s test: it changes what the company must answer to.

---

## 5. Why this is the reason to enter space

`docs/economy/ERAS.md`'s Era 0 → Era 1 gate is an economic threshold — rocketry, a staffed
launchpad, a propellant reserve. It says what leaving **costs** and nothing about why anyone would.

Climate is the why, and the shape of the argument matters: **space does not get better — home gets
worse.** Off-world bodies are exactly as hostile after the war as before it. What changes is the
*gap*. A homeworld with a collapsed arable share and a wrecked biosphere is no longer sufficiently
better than a cold rock to justify staying on it exclusively.

This is also where `hazard` does its real job, per Ben's correction: the decision to put a **large
settlement on another body** is a hazard calculus, and it becomes worth making only once the
homeworld has stopped being the obvious answer. Climate creates the demand; hazard prices the
alternative.

**The gate's three conditions are unchanged.** Climate moves the motive to pass it, never the
conditions.

---

## 6. Grain — per body

Climate state is held **per body**. Not per tile — that is a pollution puddle: local to the ground
that caused it, dodgeable by moving, and a brake on nobody. Not per world — the game's territory is
a solar system of genuinely different atmospheres, and a shared figure would have an asteroid's
extraction degrading a homeworld's air.

Per body is the commons that matters. It also scales into Era 1 correctly: each off-world body
carries its own state, so spreading production across bodies is *also* spreading strain.

**Contamination is the exception** and is spatial by nature — it belongs to ground, not to a body.

---

## 7. Legibility is part of the mechanism

A force the player cannot see is a handicap, and `docs/ai/AI_OPPONENT.md` rejects handicaps as the
foundation of this design. So:

- The body's climate state is **visible as a number and a trend**, not inferred from falling yields.
- A corporation's **own contribution** to the drift is visible to it.
- The consequence is **attributable** — when the harvest fails, the interface can say why.

If a player experiences climate as "my output got worse for no stated reason", the system has failed
however correct its arithmetic.

---

## 8. The constraint on pressure (Ben, 2026-08-31)

> *"'how far restraint goes' should never exclude extension and construction. If a player loses out,
> they should be able to see that the world doesn't wait for them."*

Climate makes operating **costly** and **relocates value**. It never makes operating impossible.

- It does not veto construction, remove a tile from play, forbid extraction, or gate a building
  type.
- A degraded body is a worse place to operate, never a closed one.
- Contamination is the sharpest effect and is still **temporary** by construction.

---

## 9. Where the era arc stands

**Era 0's exit is a nuclear war**, and climate is both its precondition and its aftermath.

**Era 1's climate is the recovery** — winter lifting to a permanently degraded floor — and per-body
state makes off-world expansion a way of spreading strain as well as reaching resources.

**Era 1 → Era 2 is not designed here.** `docs/economy/ERAS.md` marks that gate undesigned and this
document does not fill it. The only shape it observes: if a wrecked homeworld is why the player
left, Era 1 ends when the homeworld stops being what they depend on — which is the threshold Era 1's
own strategic question already names.

---

## 10. What this document does not settle

1. **The recovery curve** — how far the drift is reversible, and where the permanent floor sits.
   Recommended shape: recoverable up to a threshold, then a floor each further breach raises.
2. **What exactly constitutes strain**, and the weighting across extraction, processing and
   throughput.
3. **Whether anything a corporation builds accelerates recovery** — which would make remediation a
   market rather than only a cost.
4. **Whether nations act on it** — a levy, a tariff or a law answering climate would reach the
   2026-08-18 nation grant (`.claude/rules/io-standing-rules.md`).
5. **Whether the player can influence the war** at all, or only its aftermath.
6. **The surface** — lens, ledger, body readout, or a pair (`docs/ui/LENSES.md`).

---

## 11. Where the parts live

| Subject | Doc |
|---|---|
| The physical model and its scalars | `docs/generation/PLANETOLOGY.md` |
| Climate bands as a terrain and deposit input | `docs/generation/TILE_GENERATION.md`, `docs/economy/TILES.md` |
| Endemic goods and their latitude bands | `docs/generation/PLANETOLOGY.md` § Endemism, `docs/economy/RESOURCES.md` |
| Centres, habitability, decline and razing | `docs/economy/POPULATION.md` |
| Hazard as the off-world settlement calculus | `docs/economy/PRODUCTION.md`, `docs/economy/TILES.md` |
| The Era boundary, the war, the space gate | `docs/economy/ERAS.md`, `docs/CONCEPT.md` |
| Why a systemic brake rather than an agent-side one | `docs/ai/AI_OPPONENT.md` |
| Discrete occurrences that read climate | `docs/EVENTS.md` |
