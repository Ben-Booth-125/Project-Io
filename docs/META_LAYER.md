# Project Io — The Meta Layer

**The predicate and effect substrate** that laws, technologies, quests and embargoes are all built
out of. Two closed vocabularies and about four hundred lines of code, under every rule the game can
express.

The project's own name for it, from `modifier_set.hpp`: *"the condition side of the meta layer"*.
BL-477 (era collapse defines meta) uses the same word.

> **Status: written 2026-08-22 as capture, not design.** § Build status is transcribed from the
> code and is true today. § What is absent names holes. § Open questions are calls nobody has made.

---

## The shape

Every rule in Io is **a predicate plus an effect**. Both halves are **data** — no callback, no Lua
hook, no open-ended string subject — which is what makes a rule deterministic, serialisable, and
legible in a ledger line.

    condition_set   the PREDICATE side   src/world/condition_set.{hpp,cpp}   BL-342
    modifier_set    the EFFECT side      src/world/modifier_set.hpp          BL-479

A law is a `condition_set` plus an effect. A tech gate is a `condition_set` plus an effect. An
embargo is a `condition_set` and nothing else. That is the whole architecture, and its virtue is
that **a new rule family costs an enumerator, not a subsystem**.

### Why it exists at all

`condition_set` was built because **BL-155 (laws) and BL-156 (techs) independently settled on the
same object and neither built it.** Before BL-344 closed the loop, `tech_node::condition` was a
descriptive **string** — a label that said what a gate would be *about* and could not resolve. So
the tech tree was *"a picture of a system rather than the system"*, and no tech had ever been
earned.

That is the failure mode the meta layer exists to prevent, and it is worth remembering when reading
§ What is absent: **a vocabulary entry that nothing reads is the same defect one step earlier.**

---

## Build status

### The predicate side — `condition_set` (BL-342)

**One atomic condition is `<subject> <comparator> <operand>`**, plus the qualifier its subject
reads. A `condition_set` is **a flat AND-list** of them.

**Nine subjects.** Six were promoted from `tech_tree.hpp`'s original descriptive labels — kept
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
   resolution 1. Evaluation order stays deterministic and the predicate stays legible.
4. **A subject may be military.** Two military subjects shipped *before anything read them* —
   BL-094's design test applied at the foundation, because *"a subject enum that enumerates only
   economic quantities is exactly the failure the pivot is trying to avoid."*

**One more thing worth its own line.** `evaluate` carries a **subject corp**, which BL-342's
original design sketch did not. Every consumer is per-corporation — a levy is charged to a corp, an
earned tech is per-corp state and not a world fact — so a world-only predicate could not have
answered either question.

**`condition_text` is shared on purpose.** One human rendering of a predicate, so the tech viewer
and the laws ledger *"cannot word the same predicate differently."* UI callers pass label
resolvers; world-layer and harness callers pass none and get enum indices. Same wording everywhere.

**Three consumers today:**

| Consumer | Predicate asks |
|---|---|
| `law.cpp` | is this law in force on this corp? |
| `tech_gate.cpp` | has this corp earned this tech? |
| `corp_command.cpp` | is this buyer embargoed by this supplier? |

### The effect side — `modifier_set` (BL-479)

**One scalar modifier is `<subject> <op> <magnitude>`.** Three ops — `add`, `subtract`, `multiply`
— *"and no more; an open-ended expression tree would re-open the callback door."*

**Six subjects, of which one is wired:**

| Subject | Status |
|---|---|
| `extraction_rate` | **WIRED** — `extraction_nominal`, economy_system.cpp |
| `processing_yield` | vocabulary only |
| `unit_upkeep` | vocabulary only |
| `logistics_cost` | vocabulary only |
| `wage_floor` | vocabulary only |
| `collapse_strain` | vocabulary only |

**Fold order is the caller's contract.** `world::modified_scalar` folds in stored order, which is
earn order — stated rather than accidental, *because add and multiply do not commute.*

**Two properties mirror the predicate side.** It is **closed** — a subject not named cannot be
moved by the meta layer, and that is the point. And it is **append-only once serialised**:
`modifier_subject` is a `uint8_t` inside records that will cross the save seam, so a value may be
appended but never inserted, renumbered or removed. `condition_subject` carries the same rule, which
is why `science` was appended last in 2026-08-17 rather than filed with its siblings.

---

## "A shape is only proven by an instance"

This principle appears twice in the code, unnamed both times, and it is the most transferable idea
in the substrate. It is worth stating once here.

Two vocabulary entries shipped **before anything read them**, deliberately:

- **The military condition subjects** (BL-342) — *"not because the prototype needs them, but
  because a shape is only proven by an instance."*
- **`collapse_strain`** (BL-479) — *"unwired today, exactly as BL-342 shipped military subjects
  before anything read them."*

The argument is that a vocabulary claiming to be extensible is only *proved* extensible by carrying
an entry of the awkward kind. An enum of purely economic scalars would have looked general and been
economic-only; one military entry proves the shape.

**It cuts both ways, and the doc should say so.** The same reasoning produces § What is absent's
largest entry — five of six modifier subjects wired to nothing. An unwired instance proves a shape;
five unwired instances are a vocabulary waiting for consumers, which is the state
`tech_node::condition` was in when it was a string.

---

## The asymmetry — the effect side is far behind the predicate side

Worth stating plainly, because it is invisible from either file alone.

| | Predicate side | Effect side |
|---|---|---|
| Subjects | **9** | **6** |
| Wired | 9 of 9 | **1 of 6** |
| Consumers | 3 (law, tech gate, embargo) | 1 (tech, via `modified_scalar`) |
| Included by | 7 files | **2 files** |

BL-479's own framing was that the effect vocabulary had been one field — `unlocks_structure` — *"a
9:1 condition-to-effect asymmetry."* Widening it to six subjects narrowed the count and left the
**wiring** asymmetry, which is the one that decides whether a rule can actually *do* anything.

### And the shared-effect-vocabulary intent is not met

`modifier_set.hpp` states its own purpose:

> *"the closed list of scalars a tech (BL-479) **or a law (BL-480)** may move... Authored once and
> shared — tech and law naming different subject enums would be the same defect as tech and law
> wording the same predicate differently, which is exactly what `condition_set` exists to prevent."*

**BL-480 landed with its own enum.** `law_effect_kind` (`extraction_levy`, `import_tariff`) does not
use `modifier_set` at all — `law.{hpp,cpp}` includes `condition_set.hpp` and never
`modifier_set.hpp`, and the only two files that include it are `world.hpp` and `tech_gate.hpp`.

**In fairness, there is a real reason**, and it is not laziness: `modifier_subject` moves
**simulation scalars** (units per tick, yield per batch), while a levy is a **flow of money**. There
is no subject in the list a levy could be expressed as, and inventing one would distort the
vocabulary rather than share it.

So the honest statement is: **the effect side is two vocabularies, for a defensible reason that
nothing had written down** — while the header goes on claiming it is one. That is exactly the drift
a doc should catch, and it is now caught.

### Three effect taxonomies, in fact

1. **BL-155's four families** — margin modifiers *(shipped)*, production, permission, relationship.
   The design taxonomy; nothing in code names it.
2. **`law_effect_kind`** — two enumerators, both family (a).
3. **`modifier_subject`** — six scalars, one wired.

None of the three maps onto either of the others.

---

## What is absent, and known to be

- **Five of six modifier subjects are wired to nothing.** `processing_yield`, `unit_upkeep`,
  `logistics_cost`, `wage_floor` and `collapse_strain` are vocabulary. The policy is explicit and
  defensible — *"wired when an item needs it and NOT before"* — but the ratio is now 5:1 and the
  policy has no stated stopping condition.
- **No OR, ever, by design.** A flat AND-list cannot express *"iron ore OR steel"*. That is
  deliberate (property 3), and it is a real expressive limit that will be met by the first quest
  that wants alternatives.
- **Quests do not exist.** The header says the predicate is what *"laws, techs and quests"* read.
  There is no quest. BL-087 (era-1 tech/quest system) owns them and is parked.
- **`science` is REACHED, not SPENT.** A `tech_gate` is a predicate over corp state: it asks whether
  a condition holds, and holds again next tick. **Nothing in the gate system debits anything.** So
  science is a threshold a corp passes, not a currency it pays down — and making it spendable *"would
  need an entirely different mechanism and would be a design change, not a wiring job."* That is
  precisely what BL-478 (ancient research spend) is blocked on (NR-387), and what the research
  budget lines in BL-538 would supply.
- ~~**No serialiser.**~~ **CORRECTED 2026-08-23 — the seam exists, and both vocabularies are now
  ON it.** BL-536 landed a whole-world snapshot, and `condition_subject`, `condition_comparator`,
  `modifier_subject`, `modifier_op` and `law_effect_kind` all cross it as a `uint8` per enum, read
  back range-gated.

  **So the append-only rule stops being a forward-looking discipline and becomes a live constraint.**
  Everything these two headers say about appending-never-inserting was written against a seam that
  did not exist yet; it binds now, and a mis-ordered append breaks a save rather than a future one.
- **The `era` subject carries an honest caveat** in `measure_condition` itself, and NR-333 records
  that **two independent notions of era have drifted apart**. A predicate over a contested quantity
  is a predicate whose meaning is contested.

---

## Open questions

All four were settled on 2026-08-22 (Ben, design register).

1. ~~**One vocabulary or two?**~~ **Two, and the header is corrected.** The split is right; only
   the claim was wrong. `modifier_set.hpp` now describes a **money-flow effect family alongside the
   scalar family**, so a future author extending the vocabulary reads the shape that exists rather
   than the shape once intended. `modifier_subject` does **not** grow subjects a levy could be
   expressed as — a levy is a flow, not a scalar the simulation computes, and forcing it into the
   list would distort the vocabulary rather than share it.

2. ~~**The stopping condition for vocabulary-only subjects?**~~ **Each unwired subject names the
   backlog item that will wire it** — so vocabulary-only is a **promise**, not a state. It costs a
   comment per subject, keeps *a shape is only proven by an instance* intact, and converts an
   unbounded condition into a tracked one. The header carries the named items; a subject that
   cannot name one has not earned its row.

3. ~~**Does anything ever need OR?**~~ **No — keep the flat list.** Where alternatives are needed,
   author two rules. The expressive limit is accepted deliberately rather than worked around, which
   is what stops a mesh arriving without anyone choosing it.

4. ~~**Should `condition_text` be the AI's rendering too?**~~ **Both.** The agent gets the
   **structured** form — a predicate it can reason over, not prose it must parse — and
   `condition_text` renders anything human-readable the agent *emits*. So the one-wording rule still
   holds wherever a human reads the output, and the machine path is not made to go through prose.

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
| The one wired modifier | `src/world/economy_system.cpp` § `extraction_nominal` |

**A note on the split that looks odd and is not.** The gate table lives in the Lua-free
`tech_gate.cpp` rather than in `tech_tree.cpp`, because `tech_tree.cpp` loads Lua and is excluded
from the SDL/Lua-free world superset — and a gate that gates construction must be linkable by
`construction.cpp` and by a headless harness. **The Lua file authors identity and topology; it never
authors the predicate.** One authority for what a gate requires.

**Related authorities.** [`docs/politics/NATIONS.md`](politics/NATIONS.md) (laws, and the
enforcement seam), [`SYSTEMS.md`](SYSTEMS.md) § Conditions (the one-paragraph overview this document
expands), `docs/research/TECH_EFFECTS.md` (research scaffolding for what a tech should *do*),
`docs/economy/MARKETS.md` § Procurement (where the embargo predicate is read).

**Backlog.** BL-342 (condition evaluator) and BL-479 (tech effect union) built the two halves.
BL-155 (law/policy surface) holds the four-family taxonomy nothing in code names. BL-156 (tech
system) owns the gate/quest object. BL-478 (ancient research spend) is blocked on science being
reached rather than spent. BL-087 (era-1 tech/quest system) owns quests and is parked.
BL-107 (save format) owns the seam both vocabularies are designed against.
