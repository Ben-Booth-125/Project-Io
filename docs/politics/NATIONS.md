# Project Io — Nations

**The nation as an actor** — what a nation *is* once generation has finished making it, what it
holds, what it may do, and what it actually does today. This document is the authority for that
question, and it is the one `docs/generation/NATION_GENERATION.md` has always explicitly declined:

> *"Nation system design is **an open item**. This document covers only the generation strategy."*
> — NATION_GENERATION.md, line 7

The split between the two is clean and worth keeping. **NATION_GENERATION.md owns how a nation
comes to exist** — territory, resource profile, political character, name. **This document owns
what it does afterwards.** Where they disagree about a field, generation wins on how it is set and
this doc wins on what it means.

> **Status: written 2026-08-22, as capture rather than design.** Everything in § Build status is
> transcribed from the code and is true today. Everything in § What is absent is a named hole.
> § Open questions are calls **nobody has made** — they are for Ben, not for a session to decide.

---

## What a nation is

A nation is a **territorial polity** in the campaign era. It owns tiles, it has a character, it
holds money, and it is the only actor that can author law.

It is **not** the player, and the player is not accountable to it in any way the code models yet.
Under the ancient arc the player is a mercenary company: a **law subject**, never a legislator
(`docs/MANUAL.md` § 1.2, and SYSTEMS.md § Policy). A nation is the thing whose rules you operate
inside, route around, or price in.

It is also **not** a corporation. The two are different kinds of object with different faculties,
and the distinction is load-bearing:

| | Corporation | Nation |
|---|---|---|
| Owns | buildings, stockpiles, orders | **tiles** |
| Money | `balance`, spent every tick | `treasury`, **spent by nothing** |
| Acts through | `corp_command`'s 25 verbs | **no verb exists** |
| Decides | `corp_ai`, every tick | **nothing decides for it** |
| Can author law | no | **yes — and only it can** |

That table is the whole of this document in miniature. A nation has the two faculties a
corporation lacks — territory and legislative authority — and none of the machinery that makes a
corporation an actor.

---

## Build status

Three things about a nation are live. One is generation output; two are campaign state.

### 1. Identity and territory — *generation output, stable thereafter*

`nation_component` carries a generated `name`, an ordered `tiles` list, a summed
`resource_abundance` profile, and three character fields drawn from seeded RNG in Pass 4:
`politics` (an `ideology`), `posture` (an `expansionism`) and `focus` (an `economic_focus`).

**The three character fields are read by generation and by almost nothing else.** They shape the
world that is handed to you at the epoch. They do not currently make a nation behave differently
during play, because nothing makes a nation behave at all.

`world::tile_to_nation` is the reverse index, and it is what gives a law its **jurisdiction**.

### 2. The treasury — *live state, credited, never debited*

`nation_component::treasury` is a float. It is **zero at generation** — deliberately, since a
treasury that started full would be a balance change smuggled in as a field.

Two flows credit it. **Nothing spends it.**

| Flow | Where | Shipped and reachable? |
|---|---|---|
| **Extraction levy** | `apply_budget`, `budget_system.cpp` | **Yes** — enacted at generation |
| **Import tariff** | the clearing tick, `market_clearing.cpp` | Built and proved; **unreachable in play** |

Both are **transfers, not mints**: the payer is debited exactly what the nation is credited, in
the same float and the same tick. The levy's conservation is asserted by
`tools/verify/law_author_harness.cpp`; the tariff's by `tools/verify/money_conservation.cpp`,
which covers it alongside BL-392's procurement flows and against control worlds.

> **The treasury is not serialised and is not covered by `state_hash`.** Nations are hashed
> nowhere. BL-107 (save format version) must pick this field up; until it does, a treasury
> divergence is only detectable through the debit half, on corporation balances.

### 3. Law authorship — *one law, seeded, enacted*

A `law` is an id, a `condition_set` predicate, an effect kind, a rate, a resource scope, an
`enacted` flag and — since BL-480 (law has an author) — an `enacting_nation`.

**A law with no author cannot exist.** Every seeding path assigns one, and a record whose author is
null or dangling is ill-formed and charges *nothing*. That is defensive by design: an authorless
charge is money destruction wearing a law's clothes.

The world ships exactly one law, seeded by `seed_prototype_laws` in `hard_coded_world.cpp`:

- **`LAW-EXTRACTION-LEVY`** — a per-unit charge on raw extraction output, all resources, rate
  **1.0 credits per unit** by default, **enacted at generation**.
- Its author is **the player corporation's home nation** — chosen so the law binds the player and
  the ledger's levy line reads non-vacuously from turn one (NR-369). Fallback is the largest
  territory, ties to the lowest entity id; with no nations, there is no law.
- Its predicate is an **empty `condition_set`** — unconditional once enacted, which is BL-155's
  stated common case and the path that exercises BL-342's always-true degenerate branch.

Enactment is **the author nation's act, not a player checkbox.** The Budget ledger's enact control
was removed by BL-480; the Laws section is browse-only, and shows each law's author and rate.

---

## The enforcement seam — the one settled design decision

> **A law is a modifier OVER the market, never an override OF it.**

This is the principle established when price clamps were vetoed on 2026-07-11: a clamp *fights*
price resolution rather than shifting a flow's cost.

So the levy applies where the flow is **accounted** (`apply_budget`), not where the price is
**resolved** (`clear_markets`). Extraction output is priced by the market exactly as before, and
the levy is a separate accounted cost.

Two consequences, both intended. The market stays the only thing that sets prices. And the player
sees the tax as **its own number**, rather than as an unexplained worse price.

**Jurisdiction bounds who pays.** The levy reaches only output on tiles the author nation owns.
A corporation extracting outside that territory pays that nation nothing.

**Evaluation is once per law, per corp, per tick** (`evaluate_laws`), before the money loop reads
it. Laws by the same author stack additively into one schedule. Ordering is fixed, the function is
pure, and the determinism invariant holds.

---

## The import tariff — built, proved, and unreachable

`law_effect_kind::import_tariff` is an ad-valorem duty on a sale made **in the enacting nation's
own market** to a buyer **domiciled elsewhere**. A same-nation sale is charged nothing — a tariff
that taxed domestic trade would be a sales tax wearing the wrong name.

The mechanism is complete. Rates from several enacted laws stack additively and clamp to `[0, 1]`,
so a stack of laws can never charge a buyer more than the goods are worth. The duty is a transfer
into the market nation's treasury. It is deliberately **not** folded into `evaluate_laws`, because
a tariff is a property of a *jurisdiction and a good*, resolved when a trade matches — not a
property of a corp.

**And nothing in the shipped binary can enact one** (NR-400). Outside `law.{hpp,cpp}` and
`market_clearing.cpp` there is not one reference to `import_tariff` anywhere in `src/`. No
`corp_verb`, no UI control, no generation seeding, no agent-seam command creates a tariff law.

So `any_import_tariff_enacted` is permanently false in a real campaign, and the entire tariff pass
is dead code in play. It is proved — in a harness fixture. The whole pass is gated on that flag, so
with no tariff enacted the world is bit-identical to the pre-tariff build.

This is the clearest instance of the pattern this document exists to name: **a nation has a
faculty and no way to exercise it.**

---

## What a nation MAY do — the 2026-08-18 grant

Nation and polity behaviour is **granted**, and the grant is recent. Ruling 4 of NR-331, Ben,
2026-08-18 — recorded in `.claude/rules/io-standing-rules.md`, which is the authority for its
exact terms.

It is the exception BL-054 (nation behaviour passes) had deferred indefinitely, and it covers both
grains Ben named: Era −1 polities inside the generation sim, and campaign-era nations.

**The binding constraints are the whole basis of the grant.** Behaviour must be pure, seeded,
deterministic and replayable — a **scored-utility layer issuing only legal verbs**, in the
BL-202/BL-203 shape that `corp_ai` already runs in. Never a planner.

What the grant **admits**:

- a nation holding a treasury;
- a nation **setting a tariff or tax rate**;
- a nation **enacting a law**;
- a nation carrying pair-state toward another polity.

What it does **not** admit:

- anything whose timing, latency or ordering can vary the generated world — `docs/lore/HISTORY.md`
  is the authority for what the ladder produces, not a licence to randomise it;
- any cloud model in the loop. The no-cloud invariant in `docs/ai/AI_OPPONENT.md` § 10 is untouched.

**Reason for the grant, in Ben's framing:** three of the four systems his 2026-08-18 brief names —
international trade, logistics and diplomacy — are nation-grain, and `GENERATION_STRATEGY.md`'s
economic premise already assumes nations that act.

---

## What is absent, and known to be

Named so the absence is visible rather than discovered. Pattern borrowed from
`docs/military/MILITARY.md` § What is absent.

- **No nation actor.** There is no `nation_ai`, no `nation_verb`, no `nation_command` — a grep of
  `src/` returns nothing for all three. The grant above is a permission nobody has used. A nation
  decides nothing, ever, on any tick.
- **No spend side.** The treasury only ever rises. Nothing anywhere debits it, so a nation's
  balance is currently a scoreboard with no game attached.
- **No authoring path for any law.** One law is seeded at generation and no actor can enact,
  repeal, or amend a law thereafter — not the player (correctly, they are a law subject), not a
  nation (incorrectly, it is the legislator), not the agent seam.
- **One effect family of four.** `law_effect_kind` has two enumerators and BL-155 (law/policy
  surface) settled a **four-family taxonomy**: margin modifiers *(shipped)*, production,
  permission, relationship. The enum is the extension point and adding one is a new enumerator
  plus a branch in `apply_law_effect` — no change to the record, the predicate, or the seam.
- **No military effect.** BL-094's design test asks whether a system changes what the player can
  **field** or must **answer to**. An extraction levy reaches economic outcomes only, and
  pretending otherwise would be dishonest. What the seam owes instead is that *nothing* in the law
  record or effect dispatch assumes an economic subject — the predicate is BL-342's, which already
  carries military subjects. The answer can become yes; it is not yes.
- **No serialisation.** `nation_component::treasury` and `law::enacting_nation` have no serialiser.
  Nations and laws are not saved, so a treasury survives nothing (NR-399). There is no game
  save/load path at all today, so this costs nothing yet — and it is the first thing BL-107 must
  pick up when there is one.
- **No answerability.** BL-399 (company answerability) owns which law classes the company is
  *subject to* — an embargo it must route procurement around, basing rights it must be granted.
  It is `design-owed`.

---

## Open questions

For Ben. Each is a call nobody has made, with the entry that raised it.

1. **What is a treasury FOR?** (NR-398) It is the first money a non-corporate actor has ever held,
   and nothing spends it. Until there is a spend side, the levy is a sink that happens to have a
   name attached.

2. **Is money conservation a goal?** (NR-398) It is **not** a global property of this economy
   today and never was: `clear_markets` makes the market a **buyer of last resort that pays sellers
   with nobody's money**, so total corporate cash rises and falls every tick by design. The two
   flows in this document conserve, and `money_conservation.cpp` says so at length precisely so
   nobody reads it as a global claim. The open call is whether global
   conservation is the target, or whether the market is deliberately an infinite counterparty.

3. **What rate should the levy be?** (NR-382) The shipped `1.0 cr/unit, all resources` was a
   placeholder that never bit while the law was un-enacted. Enacted, it sends **every rival
   insolvent**: net-worth finals move from 78K–499K to −3.6M–−5.2M, with solvency below zero on
   30/30 corps across all five benchmark seeds. This deepens an already-attributed red rather than
   breaking a green, but it needs a ruling before any golden is re-blessed.

4. **Who enacts a tariff, and when?** (NR-400) Seeding one at generation changes every generated
   world, which is a design call and not a fix. The alternative is that the first nation actor
   enacts it — which makes this question wait on question 5.

5. **What is the first nation verb?** The grant admits tax rates, law enactment and pair-state.
   Nothing says which comes first, or what a nation's scorer would be *maximising*. A corporation
   maximises profit; a nation's objective has never been stated.

6. **Does a nation have a stance toward a corporation?** `stance.cpp` is corp-to-corp only. A
   mercenary company's whole business is polities hiring it, and BL-377 (mercenary contract seam)
   needs a client. Whether that client relationship rides on stance, on standing, or on something
   nation-specific is unsettled — and it is the question this document most immediately blocks.

---

## Where the parts live

| Concern | File |
|---|---|
| The nation record | `src/world/components.hpp` § `nation_component` |
| Territory reverse index | `src/world/world.hpp` § `tile_to_nation` |
| The law object, evaluation, tariff rates | `src/world/law.{hpp,cpp}` |
| The predicate every law reads | `src/world/condition_set.{hpp,cpp}` |
| The levy transfer | `src/world/budget_system.cpp` § `apply_budget` |
| The tariff transfer | `src/world/market_clearing.cpp` |
| Law seeding | `src/world/hard_coded_world.cpp` § `seed_prototype_laws` |
| The browse-only Laws surface | `src/ui/balance_ledger.cpp` |
| Checks | `tools/verify/law_harness.cpp`, `law_author_harness.cpp`, `money_conservation.cpp` |

**Related authorities.** `docs/generation/NATION_GENERATION.md` (how a nation is made),
`docs/economy/FINANCE.md` (the money loop the levy is accounted in), `docs/economy/MARKETS.md`
(§ Tariffs, the clearing-tick half), `docs/SYSTEMS.md` (§ Policy, § Conditions),
`.claude/rules/io-standing-rules.md` (the grant's exact terms).

**Backlog.** BL-155 (law/policy surface) holds the four-family taxonomy and the ten-law list.
BL-480 (law has an author) shipped and its item still reads `designed` — see NR-514.
BL-186 (laws ledger UI), BL-399 (company answerability), BL-345 (politics relationship axis) and
BL-158 (politics datamodel stub) are the open neighbours.
