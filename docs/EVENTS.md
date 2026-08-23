# Project Io — Events

**Things that happen to the player rather than because of them.** Io has no event system. Every
change in the world today is either a player action, a rival's scored decision, or arithmetic.

> **Status: DESIGN-OWED, written 2026-08-22 from Ben's ask** — *"we also want to begin including
> random events."* Everything below § Precedent is a **proposal awaiting Ben's ruling**.
> § Open questions are the calls. Owned by BL-548.

---

## The claim, and the word "random"

The interesting design problem is in the word.

**Io is deterministic and that is non-negotiable** (`.claude/rules/io-standing-rules.md`). A truly
random event is prohibited. So an event system either breaks the invariant or it does not exist —
except that **the game already solved this exact problem once, and the solution is a ruling.**

> **Ben, 2026-08-13, BL-315 ruling 4 — battle resolution:** *"Resolution of battles should have an
> element of randomness, and the option to withdraw."* The resolver draws from a **seeded stream
> folded from the battle's own identity**, so the same save replayed produces the same battle —
> while the player, who cannot see the seed, faces a genuinely uncertain fight.
>
> **Uncertain to the player, deterministic to the engine.**

That ruling transfers wholesale. An event is drawn from a stream folded from the world seed, the
tick, and the subject's identity. It is unpredictable to the player, replayable to the engine, and
it needs no new argument — only the discipline of never reaching for a wall clock.

---

## Precedent — the meta layer already supplies two thirds of an event

An event is **a trigger, a predicate, and an effect.**

[`META_LAYER.md`](META_LAYER.md) already ships the second and third as closed, serialisable,
deterministic data vocabularies:

| Part | Supplied by | Status |
|---|---|---|
| **Predicate** — when is this event *eligible*? | `condition_set` (BL-342) | **shipped**, 9 subjects, 3 consumers |
| **Effect** — what does it *do*? | `modifier_set` (BL-479) | **shipped**, 6 subjects, 1 wired |
| **Trigger** — does it fire *this tick*? | — | **the only new part** |

That is the whole cost of the system, and it is exactly the promise the meta layer makes:
*a new rule family costs an enumerator, not a subsystem.* A law is a predicate plus an effect. A
tech gate is a predicate plus an effect. **An event is a predicate plus an effect plus a roll.**

> **And it supplies the consumer `collapse_strain` has been waiting for.** `modifier_subject`
> carries `collapse_strain` — *"the era's collapse pressure, the ancient era's imperial strain"* —
> **wired to nothing**, one of five vocabulary-only subjects NR-524 raises. BL-477 says *"each era
> has a 'collapse' state to avoid, and that's what defines meta."* **Events are how strain
> manifests**, so building them turns an unwired vocabulary entry into a working mechanic rather
> than adding a sixth.

**Two items already sit in this space:**

- **BL-289 (sky events as extinction drivers)** — `design-owed`, parked. Deliberately deferred by
  Ben, 2026-08-04: *"just flavor, keep a deferred item with your thoughts."* Its reasoning is the
  best argument in the repo for events: *"A supernova is the first cause that is NOT about the
  world: it arrives from outside, it is nobody's fault, and it is the only mechanism in the design
  that could make two otherwise-identical worlds diverge."*
- **`docs/lore/COLLAPSE.md`** — culminating events for the Era −1 collapse metagame. Research
  scaffolding, generation-side. **This document is the campaign-era counterpart**, and the two
  should share a vocabulary rather than inventing one each.

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
many campaigns, where a fixed sequence stops paying long before that.

**What it opens.** Reasons to hold reserves. Reasons for a market to move that no corporation
chose. A use for slack the optimiser would otherwise remove.

**What it caps you at — and this is a real risk.** An event system with no ceiling is a difficulty
dial that fires arbitrarily, which is the failure mode of every random event system ever built.
See § What must not happen.

---

## The proposal

### An event is data, in the same shape as a law

    event {
        id          stable identifier
        eligibility condition_set  — WHEN it may fire (the meta layer's predicate)
        weight      relative likelihood, once eligible
        cooldown    ticks before it may fire again
        effect      one or more scalar_modifiers, and/or a named consequence
        scope       WHO it happens to — a corp, a nation, a body, a province
        text        what the player is told
    }

**Authored in Lua**, exactly as recipes and the tech tree are. **Adding an event is a data change.**

### Four families, ranked by how much machinery already exists

**1. Environmental** — a bad season, a flood, a deposit seam running thin. Reads terrain,
hydrology and deposits, all generated. Effects land on `extraction_rate` — **the one wired modifier
subject.** *Cheapest family and the natural first cut.*

**2. Economic** — a glut, a shortage, a supplier's failure. Reads market state and procurement.
Effects land on price, or on a contract's terms. Riskiest to balance, because the economy is
already the most tuned part of the game.

**3. Political** — a law enacted, a tariff raised, a grudge remembered. This family is *already
half-built and does not know it*: BL-537's national budget, BL-539's lobbying and BL-541's
directional tariffs all produce discrete, dateable changes a player should be **told about**.
**An event is the notification layer those systems are missing.**

**4. Personal** — a succession, a death, a defection. Depends on [`PEOPLE.md`](PEOPLE.md), and is
the family that generates change **on its own schedule** rather than in response to world state.

### The trigger — the only new machinery

Once per economy tick, per scope, in sorted order:

1. Filter the authored event table by `eligibility` (a `condition_set` evaluation — already pure and
   deterministic).
2. Filter by cooldown.
3. Draw one, or none, from a **stream folded from `(world_seed, tick, scope_id)`**.
4. Apply the effect through `modified_scalar` or a named consequence.
5. **Tell the player.**

Step 5 is not optional and is listed as machinery rather than presentation, because of the
precedent below.

### An event that is not surfaced did not happen

> BL-458 (interdiction) shipped **silent** (NR-407). It works, and nothing tells the player their
> convoy was taken. *An interception is the most consequential thing that can happen to a player's
> economy without them pressing anything.*

That is the strongest available argument for a rule this system should adopt outright: **an event
lands with its message, in the same change.** Io already has the surface — the comms dock
(`CHAT.md`) — so this costs a channel, not a system.

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

All six answered from the design register.

**PERSONAL first**, not environmental — succession, death, defection. **So [`PEOPLE.md`](PEOPLE.md)
gates this system**, and the family that ranked fourth by available machinery is first by ruling.

**SEASONAL** — one every few years, so a decade has a shape. An economy tick is a quarter, so that
is single digits per decade, not per year.

**YES, the player chooses** — a bounded choice with priced outcomes. This is the answer that moves
events closest to BL-087's quests, and the boundary now needs stating: **an event's choice is priced
and immediate; a quest is a predicate held open over time.** Nothing else separates them.

**Events EXPRESS the collapse metagame; they do not drive it.** Strain accumulates elsewhere and
events are how the player learns about it. **That bounds this system's size** — it is a notification
and consequence layer, not the metagame's engine, and BL-477 keeps the accumulator.

**SYMMETRIC.** Everyone is subject, and watching a rival get hit is content. Harder to balance, and
it removes the *difficulty dial pointed at the player* failure entirely.

**BL-289 stays generation-side only**, as a pre-history divergence rather than a campaign event.

### The tone ruling — the most useful line in the answers

> **"Events should usually be boring. Occasional high stress chains."**

Two things follow and neither was in the proposal.

**The default register is LOW.** A baseline event should be unremarkable — texture, not incident.
That is the opposite of how most event systems are tuned, and it is what makes the occasional one
land.

**Events CHAIN.** A high-stress period is a *linked sequence*, not one large roll. That needs two
things the flat authored table above does not have: an event able to **name its successor**, and a
cooldown that gates **the chain** rather than each link. Without those, "occasional high stress" is
just a rarer roll of the same die.

---

## The questions as they were asked

1. **Which family first?** Environmental is cheapest — it reads generated state and lands on the one
   wired modifier. Political is most valuable, because BL-537/BL-539/BL-541 are producing dateable
   changes with no notification layer, and **that half is arguably not "random" at all.**
2. **How often is an event?** The single number that decides whether this reads as texture or as
   chaos. A campaign is ~100 hours; nothing has established a target rhythm.
3. **Does the player ever choose a response?** A notification is cheap and passive. A choice is a
   different genre — and it is the point at which events become quests, which BL-087 owns and which
   is parked.
4. **Do events drive the collapse metagame, or merely express it?** BL-477 is `design-owed` and says
   *"each era has a collapse state to avoid — and that's what defines meta."* If strain accumulates
   from events, this system is the metagame's engine. If events only *report* strain, something else
   accumulates it. **This is the question that decides how large this system is.**
5. **Are events symmetric?** Do rivals and nations suffer them? Symmetric is fairer and much harder
   to balance; asymmetric is a difficulty dial pointed at the player.
6. **Does BL-289 come off the shelf?** It was deferred as *"just flavor"*, and its own reasoning is
   the strongest case for events in the repo. A general event system is the thing that would make it
   cheap.

---

## Where the parts would live

| Concern | File |
|---|---|
| The predicate | `src/world/condition_set.{hpp,cpp}` |
| The effect | `src/world/modifier_set.hpp`, `world.hpp` § `modified_scalar` |
| The seeded-stream precedent | `src/world/campaign_battle.cpp` |
| The surface | `src/ui/chat_panel.cpp` (the comms dock) |
| Authored event data | `scripts/` — does not exist |
| The trigger | **nowhere — this system does not exist** |

**Related authorities.** [`META_LAYER.md`](META_LAYER.md) (two thirds of an event, already
shipped), [`PEOPLE.md`](PEOPLE.md) (the personal family), [`lore/COLLAPSE.md`](lore/COLLAPSE.md)
(the Era −1 counterpart, which should share this vocabulary),
[`ui/CHAT.md`](ui/CHAT.md) (where an event is told), [`economy/LOGISTICS.md`](economy/LOGISTICS.md)
(§ Interdiction, the cautionary precedent for shipping silent).

**Backlog.** **BL-548 (event system)** owns this document. **BL-289 (sky events)** is the specific
event whose reasoning argues best for the general one. **BL-477 (era collapse defines meta)** is
`design-owed` and decides whether events drive the metagame or report it. **BL-087** owns quests,
which is where events stop being events.
