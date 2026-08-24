# Project Io — The Meta Layer

**The predicate and effect substrate** that laws, technologies, quests and embargoes are all built
out of. Two closed vocabularies and about four hundred lines of code, under every rule the game can
express.

The project's own name for it, from `modifier_set.hpp`: *"the condition side of the meta layer"*.
BL-477 (era collapse defines meta) uses the same word.

---

## The shape

Every rule in Io is **a predicate plus an effect**. Both halves are **data** — no callback, no Lua
hook, no open-ended string subject — which is what makes a rule deterministic, serialisable, and
legible in a ledger line.

    condition_set   the PREDICATE side   src/world/condition_set.{hpp,cpp}   BL-342 (condition evaluator)
    modifier_set    the EFFECT side      src/world/modifier_set.hpp          BL-479 (tech effect union)

A law is a `condition_set` plus an effect. A tech gate is a `condition_set` plus an effect. An
embargo is a `condition_set` and nothing else. That is the whole architecture, and its virtue is
that **a new rule family costs an enumerator, not a subsystem**.

### Why it exists at all

`condition_set` exists because **BL-155 (laws) and BL-156 (techs) independently settled on the
same object.** A tech gate that is a descriptive **string** — a label that says what a gate would
be *about* and cannot resolve — makes the tech tree *"a picture of a system rather than the
system"*, under which no tech can ever be earned.

That is the failure mode the meta layer exists to prevent, and it is worth remembering when reading
§ The asymmetry: **a vocabulary entry that nothing reads is the same defect one step earlier.**

---

## The predicate side — `condition_set`

**One atomic condition is `<subject> <comparator> <operand>`**, plus the qualifier its subject
reads. A `condition_set` is **a flat AND-list** of them.

**Ten subjects.** Six were promoted from `tech_tree.hpp`'s original descriptive labels — kept
because they were chosen against real authored content, and *"re-deriving them abstractly would be
inventing where evidence already exists."*

| Subject | Measures | Qualified by |
|---|---|---|
| `research` | has the corp earned this tech (1/0) | `key`, a tech id |
| `structure` | count of that building the corp owns | `structure` |
| `stockpile` | units across all the corp's pools | `resource` |
| `market` | mean resolved price across every market | `resource` |
| `surplus` | the corp's cash balance | — |
| `era` | campaign era of the corp | — |
| `military_units` | total unit count fielded | — |
| `military_strength` | summed combat strength | — |
| `science` | accumulated research points | — |
| `province_held` | is this province held by the corp (1/0) | `province`, a province id |

**Five comparators:** `at_least` (the common case), `greater_than`, `exactly`, `at_most`,
`less_than`.

**Four properties, all load-bearing:**

1. **Pure and deterministic.** `evaluate` reads a `const world&` and nothing else — no RNG, no
   clock, no cached mutable state. An enacted law is read every economy tick, so this sits directly
   on the determinism invariant. Where a measure sums over an unordered container, **it sums in
   ascending entity-id order**.
2. **An empty set is TRUE.** BL-155: *"most laws are unconditional once enacted."* The degenerate
   case is the **common** case, so it is the cheap path rather than a bolted-on special case.
3. **Flat, never a tree.** No OR, no nesting, no re-converging mesh — mirroring BL-087's
   resolution 1. Evaluation order stays deterministic and the predicate stays legible. A flat
   AND-list cannot express *"iron ore OR steel"*; where alternatives are needed, **author two
   rules** (Ben, 2026-08-22). The expressive limit is accepted deliberately rather than worked
   around, which is what stops a mesh arriving without anyone choosing it.
4. **A subject may be military.** The two military subjects are BL-094's design test applied at
   the foundation, because *"a subject enum that enumerates only economic quantities is exactly the
   failure the pivot is trying to avoid."*

**`evaluate` carries a subject corp.** Every consumer is per-corporation — a levy is charged to a
corp, an earned tech is per-corp state and not a world fact — so a world-only predicate could not
answer either question.

**`condition_text` is shared on purpose.** One human rendering of a predicate, so the tech viewer
and the laws ledger *"cannot word the same predicate differently."* UI callers pass label
resolvers; world-layer and harness callers pass none and get enum indices. Same wording everywhere.
An AI agent gets **both** (Ben, 2026-08-22): the **structured** form to reason over — a predicate,
not prose it must parse — and `condition_text` renders anything human-readable the agent *emits*.
The one-wording rule holds wherever a human reads the output, and the machine path is not made to
go through prose.

**The `era` subject carries an honest caveat** in `measure_condition` itself: two notions of era —
the campaign's and the history sim's — are distinct quantities (NR-333), and a predicate over a
contested quantity is a predicate whose meaning is contested.

**Three consumers:**

| Consumer | Predicate asks |
|---|---|
| `law.cpp` | is this law in force on this corp? |
| `tech_gate.cpp` | has this corp earned this tech? |
| `corp_command.cpp` | is this buyer embargoed by this supplier? |

**Quests** are the fourth reader the header names; BL-087 (era-1 tech/quest system) owns the quest
object. **The mercenary contract is a fifth**, and it is what `province_held` exists for: BL-570
gave `condition_set` its first location-qualified subject because `docs/economy/CONTRACTS.md`'s
spine — *"a contract is a condition_set the client will pay to have become true"* — could not be
expressed against the nine world-wide corp scalars above. A contract stores an INDEX into an
authored template table (`scripts/contracts.lua`) rather than a free `condition_set`, so the
predicate itself is still authored data, never a type enum in C++.

## The effect side — `modifier_set`

**One scalar modifier is `<subject> <op> <magnitude>`.** Three ops — `add`, `subtract`, `multiply`
— *"and no more; an open-ended expression tree would re-open the callback door."*

**Six subjects.** Each names one scalar the simulation already computes, and each is read at
exactly one place — **a subject earns its row by naming its consumer** (Ben, 2026-08-22):

| Subject | Moves | Read by |
|---|---|---|
| `extraction_rate` | units/tick an extraction site draws | `extraction_nominal`, economy_system.cpp — the single definition of one site's nominal draw |
| `processing_yield` | output units per recipe batch | BL-513 (province building limit): denser facilities raise the province ceiling's effective cap |
| `unit_upkeep` | a unit's per-tick upkeep draw (BL-454) | BL-543 (unit value anchor): the value anchor turns the authored rates on |
| `logistics_cost` | per-unit haulage on the reach/convoy network | BL-464 (logistic points): active LP resolution costs credits |
| `wage_floor` | the wage term of building opex (BL-049) | BL-538 (treasury priority lines): the schooling line raises labour productivity |
| `collapse_strain` | the era's collapse pressure (BL-477) — the ancient era's imperial strain, the industrial era's rupture proximity | BL-477 keeps the accumulator; BL-548 (event system) reads it, since events **express** the collapse metagame rather than driving it (Ben, 2026-08-22) |

Vocabulary-only is a **promise**, not a state: a subject that cannot name the item that reads it has
not earned its row, and should be removed rather than left waiting. That is what keeps *a shape is
only proven by an instance* (below) from justifying a fifth unread entry the way it justifies the
first.

**Fold order is the caller's contract.** `world::modified_scalar` folds in stored order, which is
earn order — stated rather than accidental, *because add and multiply do not commute.*

**Two properties mirror the predicate side.** It is **closed** — a subject not named cannot be
moved by the meta layer, and that is the point. And it is **append-only once serialised**:
`modifier_subject` is a `uint8_t` inside records that cross the save seam (BL-107, save format;
`src/world/world_save.cpp`), so a value may be appended but never inserted, renumbered or removed.
`condition_subject` carries the same rule, which is why `science` sits last in its enum rather than
filed with its siblings. `condition_subject`, `condition_comparator`, `modifier_subject`,
`modifier_op` and `law_effect_kind` all cross the seam as a `uint8` per enum, read back
range-gated — so a mis-ordered append breaks a save, not a future one.

---

## "A shape is only proven by an instance"

This principle appears twice in the code, unnamed both times, and it is the most transferable idea
in the substrate. It is worth stating once here.

Two vocabulary entries exist ahead of any reader, deliberately:

- **The military condition subjects** (BL-342) — *"not because the prototype needs them, but
  because a shape is only proven by an instance."*
- **`collapse_strain`** (BL-479) — *"exactly as BL-342 shipped military subjects before anything
  read them."*

The argument is that a vocabulary claiming to be extensible is only *proved* extensible by carrying
an entry of the awkward kind. An enum of purely economic scalars would have looked general and been
economic-only; one military entry proves the shape.

**It cuts both ways, and the doc should say so.** An unread instance proves a shape; five unread
instances are a vocabulary waiting for consumers, which is the state `tech_node::condition` was in
when it was a string. The named-consumer rule above is the stopping condition.

---

## The asymmetry — the effect side is narrower than the predicate side

Worth stating plainly, because it is invisible from either file alone.

| | Predicate side | Effect side |
|---|---|---|
| Subjects | **10** | **6** |
| Consumers | 3 (law, tech gate, embargo) | 1 (tech, via `modified_scalar`) |
| Included by | 7 files | **2 files** (`world.hpp`, `tech_gate.hpp`) |

BL-479's own framing was that the effect vocabulary had been one field — `unlocks_structure` — *"a
9:1 condition-to-effect asymmetry."* Widening it to six subjects narrowed the count and left the
**wiring** asymmetry, which is the one that decides whether a rule can actually *do* anything.

### The effect side is two families, deliberately

`modifier_set.hpp` once claimed to be *"the closed list of scalars a tech (BL-479) or a law
(BL-480) may move... authored once and shared."* It is not, and the header says so (Ben,
2026-08-22, design register): **the effect side is two vocabularies, for a defensible reason.**

- **Scalar effects** (`modifier_subject`). A subject names one scalar the simulation already
  computes, and a modifier moves it. Read by tech (`tech_gate.hpp`'s effect union) and by
  `world::modified_scalar`.
- **Money-flow effects** (`law_effect_kind` in `law.hpp` — `extraction_levy`, `import_tariff`). A
  levy or a tariff is a **transfer between two balances**, not a scalar the simulation computes.
  There is no subject in the scalar list it could be expressed as, and inventing one would distort
  the vocabulary rather than share it. `law.{hpp,cpp}` includes `condition_set.hpp` and never
  `modifier_set.hpp`.

The one-vocabulary rule still binds **within** each family: two effects of the same kind must not
name different subject enums. Across the two families it does not apply, because they are not the
same kind of thing. `modifier_subject` does **not** grow subjects a levy could be expressed as.

### Three effect taxonomies, in fact

1. **BL-155's four families** — margin modifiers, production, permission, relationship. The design
   taxonomy; nothing in code names it.
2. **`law_effect_kind`** — two enumerators, both family (a).
3. **`modifier_subject`** — six scalars.

None of the three maps onto either of the others.

---

## `science` is REACHED, not SPENT

A `tech_gate` is a predicate over corp state: it asks whether a condition holds, and holds again
next tick. **Nothing in the gate system debits anything.** So science is a threshold a corp passes,
not a currency it pays down — and making it spendable *"would need an entirely different mechanism
and would be a design change, not a wiring job."* That is precisely what BL-478 (ancient research
spend) owns (NR-387), and what the research budget lines in BL-538 (treasury priority lines) would
supply.

---

## Where the parts live

| Concern | File |
|---|---|
| The predicate: subjects, comparators, `evaluate`, `measure_condition`, `condition_text` | `src/world/condition_set.{hpp,cpp}` |
| The effect: subjects, ops, `apply_scalar_modifier` | `src/world/modifier_set.hpp` |
| Per-corp modifier fold | `src/world/world.hpp` § `modified_scalar` |
| Tech gates and the effect union | `src/world/tech_gate.{hpp,cpp}` |
| The tree that authors identity and topology (never the predicate) | `src/world/tech_tree.{hpp,cpp}`, `scripts/tech_tree.lua` |
| Laws | `src/world/law.{hpp,cpp}` |
| Embargo | `src/world/corp_command.cpp` § `request_quote` |
| The scalar modifier's reader | `src/world/economy_system.cpp` § `extraction_nominal` |
| The save seam both vocabularies cross | `src/world/world_save.{hpp,cpp}` |

**A note on the split that looks odd and is not.** The gate table lives in the Lua-free
`tech_gate.cpp` rather than in `tech_tree.cpp`, because `tech_tree.cpp` loads Lua and is excluded
from the SDL/Lua-free world superset — and a gate that gates construction must be linkable by
`construction.cpp` and by a headless harness. **The Lua file authors identity and topology; it never
authors the predicate.** One authority for what a gate requires.

**Related authorities.** [`docs/politics/NATIONS.md`](politics/NATIONS.md) (laws, and the
enforcement seam), [`SYSTEMS.md`](SYSTEMS.md) § Conditions (the one-paragraph overview this document
expands), `docs/research/TECH_EFFECTS.md` (research scaffolding for what a tech should *do*),
`docs/economy/MARKETS.md` § Procurement (where the embargo predicate is read),
[`EVENTS.md`](EVENTS.md) (the rule family that adds a trigger to a predicate and an effect).

**Backlog.** BL-342 (condition evaluator) and BL-479 (tech effect union) own the two halves.
BL-155 (law/policy surface) holds the four-family taxonomy nothing in code names. BL-156 (tech
system) owns the gate/quest object. BL-478 (ancient research spend) owns the spend side of science.
BL-087 (era-1 tech/quest system) owns quests. BL-107 (save format) owns the seam both vocabularies
cross. BL-570 (condition province subject) owns `province_held` and the contract-template table.
