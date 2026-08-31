# Project Io — Climate

Authority doc for **climate**: the living state of a body's habitability under use, and the one
world force that answers back to what corporations do. Ben, 2026-08-31: *"we haven't built a
critical system which is climate."*

Climate is a **commons** — shared by every actor on a body, degraded by use, owned by nobody. That
single property is what makes it worth building, and everything below follows from it.

---

## 1. What climate is here, and what it is not

**It is not weather**, and it is not the generation-time climate that already exists.
`docs/generation/PLANETOLOGY.md` owns a body's atmosphere, chemistry and biosphere history, and
`docs/generation/TILE_GENERATION.md` uses climate bands to decide what terrain and what deposits a
tile is born with. That layer answers *what kind of world is this*, once, at generation.

This document owns the other half: **what happens to that world while it is being used.** It is
the difference between the climate a world *has* and the climate a world is *left with*.

**It is not a disaster generator.** Discrete things that happen *to* the player — a storm, a
harvest failure, a flood — are events, and `docs/EVENTS.md` owns them. Climate is the slow
continuous state underneath; an event may read climate as a precondition, and climate is never
expressed as one.

---

## 2. Why it exists — one system, three jobs

Io's design test is `docs/SYSTEMS.md` § Structure: *does this change what the company can field, or
what it must answer to?* Climate is squarely the second. It is admitted because it does three
otherwise-unrelated jobs at once, and no other proposed system does any of them as honestly.

**1. It brakes a runaway leader.** `docs/ai/AI_OPPONENT.md` § Where restraint comes from needs the
world, not the agent, to keep a leader from running away. A commons does that by arithmetic: strain
scales with what a corporation operates, and the consequence falls on everyone. The largest operator
pays the largest share of a cost it imposed on the field. Nothing names the leader, nothing reads
the player's seat, and the brake applies identically when the player is the one in front.

**2. It gives the Era boundary a cause.** `docs/economy/ERAS.md` § What moves an Era makes an Era
transition a catastrophic seeded event on the world clock, with a **visible countdown**. Climate
supplies what that countdown is counting: a meter the player can watch, driven by what the world's
corporations are collectively doing, rather than a date with no reason behind it.

**3. It gives the player a motive to leave.** The Era 0 → Era 1 gate is an economic threshold —
rocketry, a staffed launchpad, a propellant reserve. It says what leaving *costs* and nothing about
why anyone would. Climate is the why: a homeworld getting worse turns off-world extraction from an
ambition into an exit.

---

## 3. The seam — hazard and habitability already exist

Climate invents no new quantity. It makes two existing ones **move**.

| Field | Today | Under climate |
|---|---|---|
| `tile_component.hazard_level` | 0–1, written once at tile generation, never mutated | Rises as the body's climate degrades |
| `tile_component.habitability` | 0–1, same shape, sits beside it | Falls with it |

This is the whole reason the system is cheap rather than sprawling: **every consequence is already
wired.** `docs/economy/PRODUCTION.md` already multiplies extraction output by `(1 − hazard)`.
`docs/economy/POPULATION.md` already reads habitability for agglomeration and settlement.
`docs/military/MILITARY.md` already reads terrain and hazard for the difficulty of operations. A
climate that moves these two numbers is felt everywhere in the game on the day it lands, without a
single consequence being authored twice.

The corollary is a constraint on the design, not a convenience: **climate must express itself
through hazard and habitability, and not through a parallel set of penalties.** A climate that
reaches into the market, the budget and the roster directly would be a second economy wearing a
weather costume.

---

## 4. The loop

    strain  →  state  →  consequence  →  legibility  →  decision

**Strain** is what corporations do to the body: the intensity of extraction and processing running
on it, weighted by scale. It is a *rate*, summed across every operator, and it is attributable —
the model knows which corporation contributed what, which is what lets the interface name a cause.

**State** is one value per body, degrading under strain and recovering when strain falls (subject
to § 6). It is a stock, not a flow; it carries the history of how the body has been used.

**Consequence** is hazard rising and habitability falling across that body's tiles.

**Legibility** is non-negotiable and is treated here as part of the mechanism rather than as a
surface that follows it — see § 7.

**Decision** is what the loop is for. A player who can see the meter, see their own contribution to
it, and see what it is doing to their yields has a real choice to make about scale.

---

## 5. Grain — per body

Climate state is held **per body**. Not per tile, and not per world.

**Per tile is a pollution puddle.** It makes the consequence local to the ground that caused it,
which is exactly what a commons is not: it becomes a cost internal to the operator, dodgeable by
moving, and it brakes nobody. It also collapses into a terrain modifier, which the tile model
already has.

**Per world breaks the arc.** The game's territory is a solar system of bodies with genuinely
different atmospheres. A single shared figure would have an asteroid's extraction degrading a
homeworld's air, which is neither legible nor true to the generation layer that produced them.

**Per body is the commons that matters.** Every corporation on the homeworld shares one climate,
and the homeworld is where Era 0 is played and where the pressure needs to bite. It also scales
correctly into the space arc: each off-world body carries its own state, so the strategic content of
Era 1 — spreading production across bodies — is *also* a way of spreading strain.

---

## 6. Reversible or ratcheting — the shape of the curve

**This is the load-bearing open call, and it decides what climate feels like.** Three shapes:

**Reversible.** Degrades under strain, recovers when strain falls. Climate is a live pressure and a
standing negotiation. *Risk:* it becomes a thermostat — a cost to be managed, never a stake, and it
supplies no Era boundary at all.

**Ratcheting.** Degrades only. The countdown has teeth and the Era rupture is inevitable. *Risk:* it
removes agency. If the ending arrives regardless of what anyone does, the rational player ignores it
and the system becomes scenery with a number attached.

**Recommended — reversible in the small, ratcheting in the large.** Recovery is real up to a
threshold. Past that threshold a **floor** is set that never lifts: the body can recover to the
floor and no further, and each further breach sets a higher one.

The recommendation is not a compromise for its own sake. It is the only one of the three that
produces the fiction `docs/economy/ERAS.md` already commits to:

> *The backstory establishes that these powers* can *pull back from the brink; the Era 0 exit is the
> occasion they do not.*

Under a ratchet nobody could ever have pulled back, so the backstory is a lie. Under pure
reversibility nobody ever needed to, so the rupture is unmotivated. Under a floor, pulling back is
genuinely possible, genuinely costly, and genuinely something a field of competing corporations may
fail to coordinate on — which is a tragedy of the commons rather than a scripted apocalypse, and it
is a **consequence of play** rather than a date.

---

## 7. Legibility is part of the mechanism

A brake the player cannot see is a handicap, and `docs/ai/AI_OPPONENT.md` § Where restraint comes
from rejects handicaps as the foundation of this design. Climate therefore carries a legibility
requirement in its *mechanism*, not as a follow-up surface:

- The body's climate state is **visible as a number and as a trend**, not inferred from falling
  yields.
- A corporation's **own contribution** to strain is visible to it. A player must be able to see that
  they are the problem, or that they are not.
- The **consequence is attributable**: when hazard rises, the interface can say why.

If a player experiences climate as "my yields got worse for no stated reason", the system has failed
regardless of how correct its arithmetic is.

---

## 8. The constraint on pressure (Ben, 2026-08-31)

> *"'how far restraint goes' should never exclude extension and construction. If a player loses out,
> they should be able to see that the world doesn't wait for them."*

Climate makes operating **costly**. It never makes it **impossible**.

- It raises hazard and lowers habitability. It does **not** veto construction, remove a tile from
  play, forbid extraction, or gate a building type.
- A degraded body is a worse place to operate, never a closed one.
- A corporation that falls behind finds a world that carried on without it — that is the honest
  consequence the whole design protects, and a climate that freezes the map destroys it.

This binds climate exactly as it binds the coalition brake; the two are the same ruling applied to
the two systemic forces.

---

## 9. Climate and the era arc

**The Era 0 exit is unchanged in kind, and gains a cause.** `docs/CONCEPT.md` § Eras says Era 0 ends
in a global-rupture-scale war that reshapes the world enough for rapid space expansion to become
plausible. Climate does not replace that rupture — it **drives** it. Competition over a degrading
commons sharpens until it breaks, which is what a rupture is. `docs/economy/ERAS.md`'s seeded date
and visible countdown remain the mechanism of the boundary; climate is what the countdown measures.

**The space-access gate is unchanged.** Rocketry, a staffed launchpad, a propellant reserve
(`docs/economy/ERAS.md` § Era 0 → Era 1 gate). Climate changes the **motive** to pass it, never the
conditions. The gate stays an economic threshold; what changes is that staying put stops being free.

**Era 1 is where climate becomes a strategy rather than a pressure.** Era 1's strategic question is
already off-world self-sufficiency. With per-body climate, spreading production across bodies is
also spreading strain — so the Era's existing content acquires a second reason to do it, and the
homeworld's accumulated floor becomes a permanent fact the player carries rather than escapes.

**Where Era 1 ends is not settled here.** `docs/economy/ERAS.md` marks the Era 1 → Era 2 gate
undesigned, and this document does not design it. What it observes is the shape climate suggests:
if climate is why you left, then the end of Era 1 is when the homeworld stops being the thing you
depend on — and that is the same threshold Era 1's own strategic question already names.

---

## 10. The two arcs — climate scales with the era, and the vocabulary changes with it

The live product is the **ancient arc** at 0 CE; the industrial and space arc is DLC scope
(`docs/economy/ERAS.md` § The two arcs). Climate belongs to **both**, at different scale, and this is
what keeps it from being a system that only pays off in a parked product.

| Arc | What strains it | How it reads |
|---|---|---|
| **Ancient (0 CE, live)** | Timber felling, soil exhaustion under continuous cropping, salinisation under irrigation, silt and overgrazing | Local, agrarian, and slow. Land that stops giving what it gave |
| **Industrial / space (parked)** | Extraction and processing at industrial throughput | Global and atmospheric. A whole body getting worse at once |

It is **one mechanism** — strain against a shared stock, expressed through hazard and habitability —
whose magnitude and vocabulary follow the era. That matters practically: the AI brake in
`docs/ai/AI_OPPONENT.md` is needed on the **live** arc, so climate has to bite at 0 CE and not only
after the launchpad fires.

Real history is the mechanism reference here and never a name source
(`.claude/rules/io-standing-rules.md` § Terms & docs): what transfers is how a commons fails, never
a proper noun.

---

## 11. What this document does not settle

Named so they are chosen rather than accreted:

1. **The curve** (§ 6) — reversible, ratcheting, or floored. Recommended: floored.
2. **What exactly constitutes strain**, and the weighting between extraction, processing and
   throughput.
3. **Recovery rate**, and whether anything a corporation can *build* accelerates it — which would
   make climate a market rather than only a tax.
4. **Whether nations act on it.** A tariff, a law, or a levy answering climate would reach the
   2026-08-18 nation grant (`.claude/rules/io-standing-rules.md`) and is the natural home for
   collective action failing.
5. **The surface**: a lens, a ledger, a body-level readout, or some pair of them
   (`docs/ui/LENSES.md`, `docs/ui/ledgers/`).
6. **Whether habitability moving disturbs population** in ways `docs/economy/POPULATION.md` has to
   answer for, since agglomeration reads it.

---

## 12. Where the parts live

| Subject | Doc |
|---|---|
| Generation-time climate, atmosphere, chemistry | `docs/generation/PLANETOLOGY.md` |
| Climate bands as a terrain and deposit input | `docs/generation/TILE_GENERATION.md`, `docs/economy/TILES.md` |
| The `(1 − hazard)` production multiplier | `docs/economy/PRODUCTION.md` |
| Habitability and agglomeration | `docs/economy/POPULATION.md` |
| The Era boundary, its countdown, the space gate | `docs/economy/ERAS.md` |
| Why a systemic brake rather than an agent-side one | `docs/ai/AI_OPPONENT.md` § Where restraint comes from |
| Discrete occurrences that read climate | `docs/EVENTS.md` |
