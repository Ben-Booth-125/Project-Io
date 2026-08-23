# Project Io — Events

**Things that happen to the player rather than because of them.** Without an event system every
change in the world is either a player action, a rival's scored decision, or arithmetic. This
document is the event system, from Ben's ask (2026-08-22): *"we also want to begin including random
events."* Everything below § Precedent is a **proposal awaiting Ben's ruling** except where
§ Settled records one. § The questions as they were asked are the calls. Owned by BL-548 (event
system).

---

## The claim, and the word "random"

The interesting design problem is in the word.

**Io is deterministic and that is non-negotiable** (`.claude/rules/io-standing-rules.md`). A truly
random event is prohibited. So an event system either breaks the invariant or it does not exist —
except that **the game already solved this exact problem once, and the solution is a ruling.**

> **Ben, 2026-08-13, BL-315 ruling 4 — battle resolution:** *"Resolution of battles should have an
> element of randomness, and the option to withdraw."* The campaign resolver draws from a **seeded
> stream folded from the battle's own identity**, so the same save replayed produces the same battle
> — while the player, who cannot see the seed, faces a genuinely uncertain fight.
>
> **Uncertain to the player, deterministic to the engine.**

That ruling transfers wholesale. An event is drawn from a stream folded from the world seed, the
tick, and the subject's identity. It is unpredictable to the player, replayable to the engine, and
it needs no new argument — only the discipline of never reaching for a wall clock.

---

## Precedent — the meta layer already supplies two thirds of an event

An event is **a trigger, a predicate, and an effect.**

[`META_LAYER.md`](META_LAYER.md) supplies the second and third as closed, serialisable,
deterministic data vocabularies:

| Part | Supplied by |
|---|---|
| **Predicate** — when is this event *eligible*? | `condition_set` (BL-342), 9 subjects, 3 consumers |
| **Effect** — what does it *do*? | `modifier_set` (BL-479), 6 subjects |
| **Trigger** — does it fire *this tick*? | **the only new part** |

That is the whole cost of the system, and it is exactly the promise the meta layer makes:
*a new rule family costs an enumerator, not a subsystem.* A law is a predicate plus an effect. A
tech gate is a predicate plus an effect. **An event is a predicate plus an effect plus a roll.**

> **And it is the reader `collapse_strain` names.** `modifier_subject` carries `collapse_strain` —
> *"the era's collapse pressure, the ancient era's imperial strain"* — and BL-477 (era collapse
> defines meta) says *"each era has a 'collapse' state to avoid, and that's what defines meta."*
> **Events are how strain manifests**: BL-477 keeps the accumulator and this system reads it, so the
> vocabulary entry has a consumer rather than the vocabulary growing a sixth.

**Two items already sit in this space:**

- **BL-289 (sky events as extinction drivers)** — deliberately generation-side (Ben, 2026-08-04:
  *"just flavor, keep a deferred item with your thoughts"*). Its reasoning is the best argument in
  the repo for events: *"A supernova is the first cause that is NOT about the world: it arrives from
  outside, it is nobody's fault, and it is the only mechanism in the design that could make two
  otherwise-identical worlds diverge."*
- **`docs/lore/COLLAPSE.md`** — culminating events for the Era −1 collapse metagame. Research
  scaffolding, generation-side. **This document is the campaign-era counterpart**, and the two
  share a vocabulary rather than inventing one each.

---

## Where this sits in the chain

Under SYSTEMS.md § The progression chain — *each system's ceiling is the next system's door* —
events are the mechanism that **stops a chain from being a staircase.**

    extraction → logistics → markets → contracts → force → territory
                            ↑                                   ↓
                            └────────── EVENTS ─────────────────┘

**What forces you in.** Nothing. **What it prevents is the real argument:** a chain of systems each
unlocked by the last is a solved sequence once the player learns it. **An event is what makes the
same rung feel different on a second campaign** — and the game's stated target is ~100 hours across
many campaigns, where a fixed sequence stops paying long before that. The bound (Ben, 2026-08-22):
**variance in texture, never in the sequence itself.**

**What it opens.** Reasons to hold reserves. Reasons for a market to move that no corporation
chose. A use for slack the optimiser would otherwise remove.

**What it caps you at — and this is a real risk.** An event system with no ceiling is a difficulty
dial that fires arbitrarily, which is the failure mode of every random event system ever built.
See § What must not happen.

---

## The design

### An event is data, in the same shape as a law

    event {
        id          stable identifier
        eligibility condition_set  — WHEN it may fire (the meta layer's predicate)
        weight      relative likelihood, once eligible
        cooldown    ticks before it — or the chain it belongs to — may fire again
        effect      one or more scalar_modifiers, and/or a named consequence
        scope       WHO it happens to — a corp, a nation, a body, a province
        choice      a bounded set of responses, each with a priced outcome
        successor   the event this one may chain into
        text        what the player is told
    }

**Authored in Lua**, exactly as recipes and the tech tree are. **Adding an event is a data change.**

### Four families, ranked by how much machinery exists

**1. Environmental** — a bad season, a flood, a deposit seam running thin. Reads terrain,
hydrology and deposits, all generated. Effects land on `extraction_rate`, the modifier subject
`extraction_nominal` reads. *Cheapest family by machinery.*

**2. Economic** — a glut, a shortage, a supplier's failure. Reads market state and procurement.
Effects land on price, or on a contract's terms. Riskiest to balance, because the economy is
already the most tuned part of the game.

**3. Political** — a law enacted, a tariff raised, a grudge remembered. BL-537's national budget,
BL-539's lobbying and BL-541's directional tariffs all produce discrete, dateable changes a player
should be **told about**. **An event is the notification layer those systems need**, and that half
is arguably not "random" at all.

**4. Personal** — a succession, a death, a defection. Depends on [`PEOPLE.md`](PEOPLE.md), and is
the family that generates change **on its own schedule** rather than in response to world state.
**First by ruling** (Ben, 2026-08-22) — so PEOPLE.md gates this system.

### Rhythm and register

**Seasonal** (Ben, 2026-08-22): one every few years, so a decade has a shape. An economy tick is a
quarter, so that is single digits per decade, not per year.

> **"Events should usually be boring. Occasional high stress chains."** — Ben, 2026-08-22

**The default register is LOW.** A baseline event is unremarkable — texture, not incident. That is
the opposite of how most event systems are tuned, and it is what makes the occasional one land.

**Events CHAIN.** A high-stress period is a *linked sequence*, not one large roll. That is why the
record above carries a `successor` — an event can name the one it leads to — and why `cooldown`
gates **the chain** rather than each link. Without those, "occasional high stress" is just a rarer
roll of the same die.

### The player chooses

**Yes, the player chooses** (Ben, 2026-08-22) — a bounded choice with priced outcomes. This is the
answer that moves events closest to BL-087's quests, and the boundary is: **an event's choice is
priced and immediate; a quest is a predicate held open over time.** Nothing else separates them.

### The trigger — the only new machinery

Once per economy tick, per scope, in sorted order:

1. Filter the authored event table by `eligibility` (a `condition_set` evaluation — already pure and
   deterministic).
2. Filter by cooldown, at chain grain.
3. Draw one, or none, from a **stream folded from `(world_seed, tick, scope_id)`**.
4. Apply the effect through `modified_scalar` or a named consequence, or hold it pending the
   player's choice.
5. **Tell the player.**

Step 5 is not optional and is listed as machinery rather than presentation, because of the
precedent below.

### An event that is not surfaced did not happen

> BL-458 (interdiction) is the cautionary precedent (NR-407): an interception that works and that
> nothing tells the player about. *An interception is the most consequential thing that can happen
> to a player's economy without them pressing anything.*

That is the strongest available argument for a rule this system adopts outright: **an event lands
with its message, in the same change.** Io already has the surface — the comms dock
(`CHAT.md`) — so this costs a channel, not a system.

### Symmetric

**Everyone is subject** (Ben, 2026-08-22): rivals and nations suffer events as the player does, and
watching a rival get hit is content. Harder to balance, and it removes the *difficulty dial pointed
at the player* failure entirely.

### Events express the collapse metagame; they do not drive it

Strain accumulates elsewhere — BL-477 keeps the accumulator — and events are how the player learns
about it (Ben, 2026-08-22). **That bounds this system's size**: it is a notification and
consequence layer, not the metagame's engine.

---

## What must NOT happen

- **No event may be unavoidable and unsignalled.** A player must be able to see, in advance, that a
  *class* of thing can happen — even if not which or when. Otherwise an event reads as the game
  cheating.
- **No event may fire from a wall clock or an unseeded draw.** Ever.
- **No event may act.** An effect moves a scalar or sets a flag. An event that *makes a decision* —
  places a building, declares hostility, enacts a law — is a planner, and planners are prohibited.
  **An event may create the conditions for an actor to decide; it may never decide.**
- **No event may be purely negative.** A system that only takes is a tax with a story. Ben's
  interconnectivity ask — *"a player only progresses so far using one system before the next becomes
  a natural consequence"* — argues that the best events **open a door rather than close one**: a
  shortage that makes a trade route worth building, a succession that makes a nation lobbyable.
- **No event may bypass the fog.** What a player is told about a rival must respect BL-068.

---

## Settled — Ben's rulings, 2026-08-22

All six questions answered from the design register.

**PERSONAL first**, not environmental — succession, death, defection. **So [`PEOPLE.md`](PEOPLE.md)
gates this system**, and the family that ranked fourth by available machinery is first by ruling.

**SEASONAL** — one every few years, so a decade has a shape.

**YES, the player chooses** — a bounded choice with priced outcomes. An event's choice is priced and
immediate; a quest is a predicate held open over time.

**Events EXPRESS the collapse metagame; they do not drive it.** BL-477 keeps the accumulator.

**SYMMETRIC.** Everyone is subject, and watching a rival get hit is content.

**BL-289 stays generation-side only**, as a pre-history divergence rather than a campaign event.

**The tone ruling:** *"Events should usually be boring. Occasional high stress chains."* Two things
follow and neither was in the proposal — the low default register, and chaining, which is why the
event record carries a successor and a chain-grain cooldown.

---

## The questions as they were asked

1. **Which family first?** Environmental is cheapest — it reads generated state and lands on the
   modifier subject the economy already reads. Political is most valuable, because BL-537/BL-539/
   BL-541 produce dateable changes with no notification layer, and **that half is arguably not
   "random" at all.**
2. **How often is an event?** The single number that decides whether this reads as texture or as
   chaos. A campaign is ~100 hours; nothing had established a target rhythm.
3. **Does the player ever choose a response?** A notification is cheap and passive. A choice is a
   different genre — and it is the point at which events become quests, which BL-087 owns.
4. **Do events drive the collapse metagame, or merely express it?** BL-477 says *"each era has a
   collapse state to avoid — and that's what defines meta."* If strain accumulates from events, this
   system is the metagame's engine. If events only *report* strain, something else accumulates it.
   **This is the question that decides how large this system is.**
5. **Are events symmetric?** Do rivals and nations suffer them? Symmetric is fairer and much harder
   to balance; asymmetric is a difficulty dial pointed at the player.
6. **Does BL-289 come off the shelf?** It was set aside as *"just flavor"*, and its own reasoning is
   the strongest case for events in the repo. A general event system is the thing that would make it
   cheap.

---

## Where the parts live

| Concern | File |
|---|---|
| The predicate | `src/world/condition_set.{hpp,cpp}` |
| The effect | `src/world/modifier_set.hpp`, `world.hpp` § `modified_scalar` |
| The seeded-stream precedent | `src/world/campaign_battle.cpp` |
| The surface | `src/ui/chat_panel.cpp` (the comms dock) |
| Authored event data | `scripts/`, alongside the recipes and the tech tree |
| The trigger | the economy tick, once per scope in sorted order |

**Related authorities.** [`META_LAYER.md`](META_LAYER.md) (two thirds of an event),
[`PEOPLE.md`](PEOPLE.md) (the personal family), [`lore/COLLAPSE.md`](lore/COLLAPSE.md)
(the Era −1 counterpart, which shares this vocabulary),
[`ui/CHAT.md`](ui/CHAT.md) (where an event is told), [`economy/LOGISTICS.md`](economy/LOGISTICS.md)
(§ Interdiction, the cautionary precedent for a consequence with no message).

**Backlog.** **BL-548 (event system)** owns this document. **BL-289 (sky events)** is the specific
event whose reasoning argues best for the general one. **BL-477 (era collapse defines meta)** owns
the strain accumulator events express. **BL-087 (era-1 tech/quest system)** owns quests, which is
where events stop being events.
