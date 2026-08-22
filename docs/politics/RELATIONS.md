# Project Io — Relations

**How actors in Io feel about, rate, and gate each other.** This document owns the whole relational
layer, and its first job is to say **which quantity answers which question** — because the code
carries four of them, three share overlapping names, and none of them was documented anywhere
before 2026-08-22.

> **Status: written 2026-08-22 as capture, not design.** § Build status is transcribed from the
> code and is true today. § What is absent names holes. § Open questions are calls nobody has made.
> Companion to [`NATIONS.md`](NATIONS.md) — nations are actors in this layer too, and BL-540 adds
> the fifth quantity below.

---

## The four quantities, and the one that is not what its name suggests

This table is the reason the document exists. Read it before touching any of them.

| Quantity | Question it answers | Shape | Origin | Gates |
|---|---|---|---|---|
| **Stance** | *How does A feel about B?* | hostile (directed) / friend (symmetric) | **declared** | interdiction, battle, march |
| **Reputation** | *Will B do business with A again?* | one float per (buyer, supplier) | **earned** | procurement quotes |
| **Embargo** | *Is A forbidden to trade in B's terms?* | a `condition_set` per corp | **authored** | **nothing — no author exists** |
| **Standing** | *How strong is A?* | five bands on three axes | **derived** | nothing — it is a readout |

> **Settled 2026-08-22 (Ben, ruling on NR-520): sentiment is the substrate.** The table above is
> what the code holds *today*; the target is **two layers, not four quantities**. See § The
> settled model directly below — read it before extending anything in this document.

**Standing is the odd one and the naming is a known hazard.** It is not a relation at all: it is a
coarse public read of how powerful a corporation is, with no second party anywhere in it. NR-304
records the collision being noticed when stance was named — *"called it 'stance', not 'standing' —
the latter is taken by BL-262's power read."*

The distinction that keeps them straight, from `stance.hpp`'s own header: **"stance is how a corp
feels about another; standing is how strong it is."**

---

## The settled model — two layers

Ben's ruling on NR-520, 2026-08-22. Putting all four quantities in one table made visible what none
of them showed individually: **three designs were converging on the same continuous derived
quantity and none knew about the other two** — CONCEPT.md's sentiment, BL-540's nation→corp
Access/Trust, and `corp_reputation`.

They converge deliberately now:

    stance      DECLARED, discrete    — an act, always attributable to an ACTOR
    sentiment   DERIVED,  continuous  — a reading, attributable to CONDUCT

**Sentiment** is one directed, continuous, derived value from an **observer** to a **subject**,
where an actor is a corporation or a nation. Directed, because every existing use already needs it
— reputation is keyed (buyer, supplier), a nation reads a corp and never the reverse, a grudge is
asymmetric by nature. Continuous, with bands as a *presentation* choice (the BL-262 precedent),
never the storage.

**Stance stays exactly as it is**, sitting on top as the declared layer.

### The invariant: sentiment must never become hostility on its own

Ben ruled on 2026-08-17 that hostility is **a declared state a corp opts into**, and that a rival
*"may score and declare it, never acquire it ambiently."* That ruling stands, and the substrate must
not quietly overturn it.

So **sentiment informs a declaration; it never makes one.** A scorer may read sentiment and choose
to `declare_hostile`. **No threshold anywhere may flip a stance table by itself.** A harness row
asserts it directly: driving sentiment to any extreme, in either direction, mutates no stance table.

### What the ruling dissolves

| Was | Becomes |
|---|---|
| `corp_reputation`, its own serialised map | a **view** of sentiment at buyer→supplier grain (BL-546) |
| BL-540's Access / Trust, a new per-nation table | **dimensions** of sentiment at nation→corp grain |
| Era −1 grudges, read by BL-541 with nowhere to live | **seeded** nation→nation sentiment |
| CONCEPT.md's sentiment, designed and never built | **the layer itself** |

**BL-391's deadlock stops existing rather than getting fixed.** The reputation floor is
unrecoverable because its only two writers are contract completion and cancellation, so the recovery
path is a cycle with no entry. A continuous quantity that **decays toward neutral has no permanent
floor by construction** — a burned relationship heals, and "wait" becomes a legitimate strategic
move. The fix is a property of the substrate rather than a patch on procurement.

*Owned by BL-545 (sentiment substrate) and BL-546 (reputation migration, which is the one relational
quantity that IS serialised, so its migration is a save-format change rather than a rename).
BL-540, BL-541 and BL-391 are reshaped onto it and all three now `require` it.*

---

## Build status

### 1. Stance — *shipped, and it gates three things*

`src/world/stance.{hpp,cpp}` (BL-448, landed 2026-08-19). Three tables on `world`, four verbs on
the corp-command seam.

**The hybrid reading is the design, settled by Ben on 2026-08-17 (NR-302):**

- **Hostility is DIRECTED.** `corp_hostile_pairs`, keyed `(from, to)`. You can be attacked without
  agreeing — *a corp can be at war and not know it yet*. That ambush property is not incidental; it
  is what interdiction needs to be a threat rather than a negotiation.
- **Friendship is SYMMETRIC.** `corp_friend_pairs`, keyed canonical `(min id, max id)`. You cannot
  be befriended without agreeing, so a friendship row is always evidence **both** corps chose it.

**Three invariants hold the shape together**, and none is optional:

1. **A pending offer is not a stance.** `corp_friend_offers` is its own table, so no consumer that
   checks friendship can accidentally read an unaccepted offer as one.
2. **Declaring hostility dissolves friendship atomically**, in the same call — a corp is never left
   readable as both hostile and friendly toward one target.
3. **Consumers ask the right table.** `is_hostile` is directional, `are_friends` is not, and the two
   are deliberately **never** collapsed behind one `stance_between()` accessor that would hide
   which is which.

A rejected mutation leaves every table untouched — the command-seam rule from
`.claude/rules/io-standing-rules.md`: validate before mutating, reject the whole command, never
partial-apply.

**What it gates, as of 2026-08-21** *(this list was empty when stance landed — the doc note that
"stance gates nothing yet" is now stale)*:

| Consumer | What hostility does |
|---|---|
| `supply_system.cpp` | A hostile unit on a convoy's tile **intercepts it** (BL-458) |
| `battle_system.cpp` | Hostile pairs are what **open a battle** (BL-467) |
| `economy_system.cpp` | War **flips the march queue** (BL-470, NR-344) |

Note the care taken in the interdiction check: it tests `is_hostile(unit_owner, cargo_owner)`, in
that direction only. *"A corp that has been declared against but has not answered is a victim, not
a raider."*

### 2. Reputation — *shipped, and deadlocked*

`world::corp_reputation` — one float per `(buyer, supplier)` pair, from BL-350 (procurement).
Moved by exactly two writers: **completion** raises it, **cancellation** lowers it.
`request_quote` refuses below a floor.

**It is a live design fault, filed as BL-391 (reputation floor is a deadlock), priority A.** The
recovery path from below the floor runs: raise reputation → complete a contract → accept a quote →
obtain a quote → **be above the floor.** It is a cycle with no entry.

A player measured it at reputation −6 against a floor of −5 and found no mechanic anywhere that
could move it back up. That is not harsh balance; it is a permanent, unrecoverable, per-counterparty
blacklist earned by an ordinary in-game action, invisible until you hit it and unsignalled when you
do.

**It matters more under the ancient arc than when it was written.** BL-377's contract loop is built
on this same reputation axis, and a loss spiral with no exit is a much bigger problem for a company
whose whole livelihood is being hired.

*Proposed fix in BL-391: decay toward neutral, so a burned relationship heals with time. One line in
the tick, no new verb, and it gives "wait" a legitimate strategic use.*

### 3. Embargo — *a read path with no writer*

`world::corp_embargo_conditions` — a `condition_set` per corp, checked by procurement.

The mechanism is real and it works. **Nothing authors an entry.** Its own comment is honest about
why: the read path exists to prove `condition_set` reaches procurement *for free*, and content — an
enacted law that populates it — is a follow-on.

This is the same shape as the import tariff in [`NATIONS.md`](NATIONS.md): built, proved, and with
no actor able to reach it. Both are waiting on the same thing — a nation that can enact.

### 4. Standing — *shipped, honest, and read by one surface*

`src/world/standing.{hpp,cpp}` (BL-262 first slice). Five bands — negligible, minor, notable,
major, dominant — on three axes: **reach** (distinct bodies with a building), **capital** (balance),
**market share** (this corp's clearing income over the total).

Two design properties worth keeping:

**It is visibility-honest.** The player's own figures are exact; every other corporation is shown
only as a **band, never a number** — `DISCOVERY.md`'s competitor-visibility rule (BL-068). The exact
figures are always computed; the UI decides what to show.

**It is deliberately NOT unified with `corp_ai`'s scorer** (BL-262 call 5). The AI optimises an
internal ground-truth quantity; this is a public coarse read. Unifying them would make every rival
directly optimise the published number — **a Goodhart trap**, and the reason the duplication is
correct rather than lazy.

*Production is intentionally absent from the axes, though BL-262's full design calls for four. A
rival's true output needs their recipe and workforce dial, both private under BL-068 — there is no
honest visible-information source for it, so the axis is left out rather than faked.*

---

## What is absent, and known to be

- **Nothing survives a save.** `src/world/serialization.cpp` **does not exist anywhere in the
  repo** (NR-349). Stance, friendship and pending offers have no serialiser, so a declared war does
  not outlive the session. Reputation is the exception — it rides BL-350's procurement stream. There
  is no game save/load path at all today, so this costs nothing yet; BL-107 must pick it up.
- **No rival ever declares anything.** BL-450 (rivals score stance) was **granted on 2026-08-18**
  and `corp_ai.cpp` contains no stance scoring at all. So every hostility in a played game is one
  the player declared. The grant is a permission nobody has used — the same state
  [`NATIONS.md`](NATIONS.md) records for the nation grant.
- **Friendship permits nothing.** Hostility now gates three things. Friendship gates zero. It can be
  offered, accepted and dissolved, and no consumer reads it. *What friendship permits was explicitly
  left as "a later call" when BL-448 landed, and it is still later.*
- **No sentiment layer.** `docs/CONCEPT.md` § Sentiment-based diplomacy describes each faction
  holding a sentiment value toward every other, shaped by trade history, territorial conflict and
  ideological alignment. `MANUAL.md` § 5 lists it `[OWED]`. Nothing in code holds it — the only
  `sentiment` reference in `src/` is a comment.
- **No nation in the layer at all.** Every quantity above is corp-to-corp. BL-540 (nation→corp
  stance) is designed and unbuilt; see below.
- **Reputation is invisible.** No blackboard predicate exports it (BL-390), so a corp cannot see its
  standing with anyone — before or after a refusal. A player below the floor is reasoning about a
  number they cannot read.

---

## The fifth quantity, designed 2026-08-22

**BL-540 (nation→corp stance)** adds a nation's read of a corporation, settled in the
[`NATIONS.md`](NATIONS.md) design session. It differs from corp-to-corp stance on every axis Ben
named, and the differences are the design:

| | Stance (BL-448) | Nation → corp (BL-540) |
|---|---|---|
| Shape | discrete tri-state | **graded** |
| Dimensions | one | **two: Access and Trust** |
| Origin | **declared** — a corp opts in | **derived** — nobody declares it |

The last row is load-bearing. Hostility is a state a corp opts into and *may never acquire
ambiently* (Ben, 2026-08-17). A nation's stance is the opposite by construction: a recomputed read
of what a company has actually done inside the jurisdiction, never stored as a flag.

**Which makes it the same kind of object as Standing, not as Stance** — derived, honest, no hidden
state — despite sharing the word. That is the fifth entry in the table at the top of this document
and the second time the vocabulary has collided.

---

## Open questions

1. ~~**Is one relational substrate right, or four?**~~ **Settled 2026-08-22** — see § The settled
   model. Two layers: sentiment derived and continuous, stance declared and discrete. What remains
   open is **how many dimensions sentiment carries** (NR-522): BL-540 needs Access and Trust
   expressible separately, reputation reads as one, and a third must gate something before it
   is added.

2. **What does friendship permit?** Left as "a later call" when BL-448 landed 2026-08-19. Candidates
   with existing machinery: passage through territory, a preferential price, shared visibility under
   BL-068, immunity from interdiction. Until it permits something it is a button with no effect.

3. ~~**Does sentiment replace stance, or sit under it?**~~ **Settled 2026-08-22: under it**, and
   never able to reach up into it on its own. The invariant above is the whole answer.

4. **At what rate does sentiment decay?** *Whether* it decays is settled — the substrate does, which
   is what dissolves BL-391's deadlock. The **rate** is unset and wants the treatment BL-543 gave
   value: an anchor, not a guess. *How long should a burned relationship take to heal?* is a
   question a player can answer.

5. **Does a rival get to declare hostility on the player?** The 2026-08-18 grant says yes. Nothing
   has used it, so the first implementation is also the first test of whether it feels fair — and
   `is_hostile` being **silent** by design (NR-350: a declaration is discovered on contact, never
   announced) means the player's first signal will be a lost convoy.

---

## Where the parts live

| Concern | File |
|---|---|
| Stance tables, verbs, invariants | `src/world/stance.{hpp,cpp}` |
| The three tables on the world | `src/world/world.hpp` § `corp_hostile_pairs`, `corp_friend_pairs`, `corp_friend_offers` |
| Reputation | `src/world/world.hpp` § `corp_reputation`; written in `corp_command.cpp` |
| Embargo | `src/world/world.hpp` § `corp_embargo_conditions` |
| Standing bands | `src/world/standing.{hpp,cpp}` |
| Interdiction gate | `src/world/supply_system.cpp` |
| Battle engagement gate | `src/world/battle_system.cpp` |
| The Stance column | `src/ui/corporation_panel.cpp` |

**Related authorities.** [`NATIONS.md`](NATIONS.md) (the nation as an actor, and BL-540),
`docs/military/MILITARY.md` (what hostility permits militarily),
`docs/ui/DISCOVERY.md` (BL-068, the competitor-visibility rule both stance and standing obey),
`docs/economy/MARKETS.md` (§ Procurement, where reputation is spent),
`docs/ai/AI_OPPONENT.md` (the scorer that will one day declare).

**Backlog.** **BL-545 (sentiment substrate)** is the spine, filed 2026-08-22 on Ben's NR-520
ruling; **BL-546** migrates reputation onto it. **BL-540** (nation→corp stance), **BL-541**
(directional tariffs) and **BL-391** (reputation floor deadlock) are all reshaped onto BL-545 and
`require` it. BL-450 (rivals score stance) holds the unused 2026-08-18 grant; BL-449 (stance needs
a surface) is the UI half; BL-390 (the seam has no read-back) owns making the value visible.
BL-158 (politics datamodel stub) and BL-345 (politics relationship axis) are the older neighbours.
