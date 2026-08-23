# Project Io — Contracts

**A contract is a promise between two named parties, priced, paced, and refusable.** It is the
alternative to the market: where the market is anonymous, instant and price-only, a contract has a
counterparty who can say no.

Io has two, and they are **siblings rather than one record used twice**:

| | **Procurement** — the buy side | **Mercenary contract** — the sell side |
|---|---|---|
| The player | buys equipment | is hired |
| Deliverable | `resource + quantity` — fungible | **a fact about the world** — a `condition_set` |
| Completes when | `ticks_elapsed` reaches lead time | **the predicate becomes true** |
| Terminal states | completed, cancelled | completed, **failed**, cancelled |
| Status | **shipped** (BL-350) | **designed, not built** (BL-377) |
| Item | BL-350 | BL-377 |

> **Status: written 2026-08-22.** § Procurement is capture — transcribed from shipped code, moved
> here out of MARKETS.md where it had been a subsection of the market doc. § The mercenary contract
> is a *settled design that has never left the backlog*, recorded here because it is the player's
> **income** and was reachable only inside a backlog item — one that spent ten days wrongly marked
> `complete`.

---

## Where this sits in the chain

Under SYSTEMS.md § The progression chain — *each system's ceiling is the next system's door*:

    markets → CONTRACTS → force → territory
                 ↑
    the market cannot sell you a lead time, a refusal, or a reputation

**What forces you in.** The market prices goods and nothing else. It cannot express *"I need forty
of these in eight weeks and I need to know now"*, and it has no memory of who you are. The first
time either matters, you need a counterparty.

**What it opens.** Procurement opens equipment you do not manufacture — which is the whole shape of
the mercenary company, who *procures* rather than produces. The mercenary contract opens **income
that is not extraction**, and with it the reason to hold force at all.

**What it caps you at.** Reputation. Both sides ride the same scalar, and it is the axis on which a
company can price itself out of its own business.

---

## Procurement — the buy side (BL-350, shipped)

**A layer over the market, not a second market.** Request a quote from a named supplier; accept it
to open a contract; pay a deposit plus a paced remainder; take delivery on completion.

**The supplier can decline, with a stated reason** — no capacity, no input access, an embargo, or
your standing being too low. **Refusal is not payable-through.** You route around it.

Asking for a quote also tells you something real about the supplier's capacity, which makes the
question itself an intelligence act rather than only a transaction.

**Split payment** — deposit on acceptance, remainder across the term — reuses BL-095's construction
pacing rather than inventing a second model.

**It is on the serialisation seam.** `procurement.cpp` is the fourth flat-binary stream in
`world/*`, after `history_log` and `order_book`: leading magic + version, count-prefixed records,
**rejection rather than reinterpretation**, and a `static_assert` on record size as the tripwire.

> **What is missing is the UI (BL-445).** Every procurement verb — `request_quote`, `accept_quote`,
> `cancel_contract` — is **seam-only**. A grep of `src/ui` returns nothing. BL-350 landed the seam,
> its world state, its serialisation and its harness, and **no player-facing press**. It is
> reachable only through the corp-command seam: an out-of-process agent, or a harness.
>
> The rival scorer does not emit them either (BL-446) — `corp_ai.cpp` enumerates no procurement
> candidate. So a mechanism that exists to give the player a counterparty currently has **no human
> and no AI using it**.

---

## The mercenary contract — the sell side (BL-377, designed)

> **A contract is a `condition_set` the client will pay to have become true, by a deadline.**

That is the whole spine, and it is short because [`../META_LAYER.md`](../META_LAYER.md)'s predicate
machinery is already generic, pure and deterministic — and procurement already evaluates one for its
embargo decline, so using a predicate as *contract terms* is precedent rather than invention.

A record carries: the client, the company, a **success predicate**, a **deadline in ticks**, a fee,
a deposit fraction, and accrued state.

**The variety of contract types is authored data in the predicate, not a type enum in C++.** Take
this province, hold that one, break a siege, escort a convoy, deny a road — **adding a contract kind
is a Lua change.**

### Why it is a sibling record and not procurement reused

The tempting one-liner — *"a mercenary contract is a procurement contract with the player as
supplier"* — is **wrong in the part that matters**, and stating why is what shapes the design:

- **The deliverable is not fungible.** `resource + quantity` cannot express *"hold the river
  province through the campaign season"*. There is no resource whose quantity means that.
- **Completion is not a countdown.** A procurement contract finishes when enough ticks pass. This
  finishes when **a fact about the world becomes true** — or fails when the deadline passes and it
  is still false.
- **Failure is distinct from cancellation.** Procurement has *cancel* (you walked away). This needs
  ***lost*** (you fought, you were beaten) — the same money outcome, a very different reputation
  outcome, **and the one that makes contracts risky.**

What genuinely transfers, and is worth having: the command seam, the split-payment pacing, and the
reputation scalar.

### Where offers come from — the stronger reuse

Offers must be deterministic. The answer is already in the tree.

`history_sim.cpp` scores four verbs per polity per year, one of which is **campaign** — a pure,
integer, argmax scorer that already decides *which neighbouring province a polity wants and how
badly*. It also already computes the **stall**: a campaign whose arriving force falls short because
the objective is too far or the defender too strong.

> **A contract offer is a campaign the polity wants and cannot win alone.**

Where the polity's projected force is insufficient but the objective's value is high, it does not
launch — it **offers the objective as a contract**, at a fee derived from the value it scored.
Where it can win alone, it campaigns and offers nothing.

Three things this buys with no new machinery: offers are **deterministic**, because the scorer
already is; **legible**, because the fee tracks a value the simulation actually computed rather than
a designer's table; and the contract market gets **a natural rhythm** — offers appear when the
political map is under tension and dry up when it is settled. **That is a reason for boom and
drought rather than a spawn timer.**

### The four settled answers

**Q1 — Does the player choose the force? The player chooses.** The contract names the *outcome* and
the *fee*, never the force. **This is the single decision that keeps it a strategy game rather than
a mission list:** the whole skill is reading whether the fee covers the force the objective actually
needs, and a contract that specifies its own force has pre-solved that.

It also makes underbidding a real failure mode — take a cheap contract, send too little, lose the
fight, lose the fee *and* the standing. **That is the loop's teeth.**

**Q2 — Three terminal states, deliberately, against procurement's two.**

| State | Money | Reputation |
|---|---|---|
| **completed** | balance of fee paid | up |
| **failed** — deadline passed, predicate false | deposit forfeit, nothing paid | down **hardest** |
| **cancelled** — withdrew early | deposit forfeit | down, but less |

*You are not paid for trying.* And **an honest early exit costs less than a rout.**

Failure must be survivable, per CONCEPT.md § Stagnation as loss — there is no lose screen. The loss
vector is **the spiral**: reputation falls, fees on offer fall with it, and the company can no longer
afford the force that would win the contracts that would restore it. **That is
bankruptcy-by-reputation**, the mercenary reading of CONCEPT.md's two existing loss vectors rather
than a third one.

**Q3 — Can the player hold contracts from opposed clients? Yes, and standing prices it.** Accepting
a contract against a client you also serve is legal, resolves normally, and costs reputation with
the injured party on completion. Making it *illegal* would need a faction-alignment model the game
does not have; making it *free* would delete the only interesting thing about being a mercenary.

*This is the one answer taken on design judgement rather than derived from a shipped mechanism, and
it is recorded as reversible.*

**Q4 — Offers are private per client**, visible only where the activity fog already reaches
(`body_activity_visibility`, BL-089). **A polity you have never dealt with and cannot see does not
offer you work.** This gives the discovery layer a second job for free, and gives reputation a
**reach** dimension: standing opens the map, not just the price.

### The client, and the money — settled 2026-08-22

BL-377 was designed before the nations session, and two of its open ends now have answers:

- **The client is a nation with a treasury.** [`../politics/NATIONS.md`](../politics/NATIONS.md)
  settles that a nation holds money and spends it down weighted priority lines; BL-538's
  **contracted-force** line is where a fee comes from. Before that ruling, offers had no funded
  counterparty.
- **The relationship rides on sentiment's Trust dimension**, not on a parallel axis
  ([`../politics/RELATIONS.md`](../politics/RELATIONS.md) § The settled model). Under BL-545,
  `corp_reputation` becomes a view of the same substrate — so the reputation this loop moves and the
  Trust a nation reads are **one quantity**, and BL-391's floor deadlock stops existing.

### Serialisation

A fifth flat-binary stream, following procurement exactly. **The `condition_set` in the record is
the one novel question** — it is a structured predicate, not a scalar. Two options: store it inline,
or store an **index into an authored contract-template table** and keep the predicate in Lua.

*Recommend the template index:* it keeps the record fixed-size, it makes contract kinds data, and a
save that outlives a template-table change should **reject rather than reinterpret** — which the
version bump already gives.

### Explicitly out of scope

The combat that resolves a contract (BL-315 — this owns what is *at stake*, never how a fight
resolves). Multi-round fee haggling — take-it-or-leave-it in this cut. Contracts between the player
and another *corporation* rather than a polity. Any narrative or dialogue layer.

---

## What is absent, and known to be

- **The whole sell side.** BL-377 has no code. Its named files were never created, `corp_verb`
  carries no contract verb, and the item spent ten days marked `complete` in error before being
  reopened 2026-08-22 (NR-510). **This is the player's income under the ancient arc.**
- **Procurement has no UI** (BL-445) and **no AI user** (BL-446). See above.
- **No contract templates.** The Lua table that would make contract kinds data does not exist.
- **Reputation is invisible** (BL-390) — no blackboard predicate exports it, so neither a player nor
  an agent can see its standing before a refusal.
- **Nothing verifies an offer ever fires.** BL-377's requirement sketch demands one observed
  instance of *each* terminal state, written that way because `hire_unit` shipped correct and was
  never observed firing across five seeds (NR-121).

---

## Open questions

All four were settled on 2026-08-22 (Ben, design register).

1. ~~**Budget or minted?**~~ **From the budget.** A nation cannot offer what it cannot pay, and
   **offers dry up when treasuries do** — which couples offer generation to BL-537 and preserves
   the conservation rule with no exception. It also gives the contract market a second source of
   rhythm besides political tension: a war-poor nation stops hiring.
2. ~~**Inline or template index?**~~ **Template index.** The record stays fixed-size, contract kinds
   stay data, and a save outliving a template change rejects rather than reinterprets.
3. ~~**Should rivals take contracts?**~~ **Yes — they compete for the same work, and losing a bid is
   a real outcome.** An offer stops being addressed to the player and becomes **contested**. It also
   forecloses BL-446's *shipped and unexercised* defect before it can repeat at a larger scale, and
   it makes reputation bite in a second direction: today it gates whether you may *request* work,
   under bidding it decides whether you *win* it. *Owned by BL-551.*
4. ~~**What does it look like on screen?**~~ **A ledger and the map** — offers, active contracts and
   terminal states in the fold-out column, with the objective province marked. Ben added a third
   thing neither option named:

   > *"Contracts consume resources, I believe we should be able to render moving contracts
   > (convoys)."*

   So a contract is not only an agreement, it is **materiel in motion**: fulfilment draws goods, and
   those goods move on the logistics network as convoys. **The contract layer and the supply layer
   share a surface**, and a player watching a convoy is watching a contract being delivered. That
   couples this document to [`LOGISTICS.md`](LOGISTICS.md) far more tightly than the original design
   assumed — and is the strongest argument yet that a contract is a *logistics* object rather than
   only a financial one.

---

## Where the parts live

| Concern | File |
|---|---|
| Procurement records, quotes, contracts | `src/world/components.hpp`, `world.hpp` |
| Procurement serialisation | `src/world/procurement.{hpp,cpp}` |
| The three procurement verbs | `src/world/corp_command.cpp` |
| The predicate a contract is made of | `src/world/condition_set.{hpp,cpp}` |
| The scorer offers would come from | `src/world/history_sim.cpp` § campaign |
| The mercenary contract | **nowhere — BL-377 is unbuilt** |

**Related authorities.** [`MARKETS.md`](MARKETS.md) (the anonymous alternative, and where
procurement's clearing-side interaction lives), [`../politics/NATIONS.md`](../politics/NATIONS.md)
(the client and its treasury), [`../politics/RELATIONS.md`](../politics/RELATIONS.md) (reputation,
and the sentiment substrate it becomes), [`../META_LAYER.md`](../META_LAYER.md) (the predicate),
[`../military/MILITARY.md`](../military/MILITARY.md) (the force a contract is won with),
[`../ui/DISCOVERY.md`](../ui/DISCOVERY.md) (the fog that decides who offers you work).

**Backlog.** **BL-377 (mercenary contract seam)** is the sell side — `designed`, A, v0.1.15,
reopened 2026-08-22 and on Sprint 16's critical path. BL-350 (procurement) shipped the buy side;
BL-445 (procurement has no UI) and BL-446 (scorer cannot procure) are its missing halves.
BL-391 (reputation floor) and BL-392 (what a contract is worth) are the economics.
BL-315 (conflict spine) resolves what a contract puts at stake.
