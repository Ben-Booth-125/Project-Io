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
| Item | BL-350 (procurement seam) | BL-377 (mercenary contract seam) |

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

## Procurement — the buy side

**A layer over the market, not a second market.** Request a quote from a named supplier; accept it
to open a contract; pay a deposit plus a paced remainder; take delivery on completion. The three
verbs are `request_quote`, `accept_quote` and `cancel_contract` on the corp-command seam.

**The supplier can decline, with a stated reason** — no capacity, no input access, an embargo, or
your standing being too low. **Refusal is not payable-through.** You route around it.

Asking for a quote also tells you something real about the supplier's capacity, which makes the
question itself an intelligence act rather than only a transaction.

**Split payment** — deposit on acceptance, remainder across the term — reuses BL-095's (construction
pacing) model rather than inventing a second one.

**It is on the serialisation seam.** `procurement.cpp` is a flat-binary stream in `world/*`,
alongside `history_log` and `order_book`: leading magic + version, count-prefixed records,
**rejection rather than reinterpretation**, and a `static_assert` on record size as the tripwire.

**Both a human and a rival use it.** The player reaches the three verbs through a procurement
surface in the app (BL-445, procurement UI), and the rival scorer enumerates a procurement
candidate alongside build, survey and hire (BL-446, scorer procures). A counterparty mechanism with
no human and no AI user is a seam with no subject.

**Reputation is readable before a refusal.** The blackboard export carries the pair's reputation
(BL-390, seam read-back), so neither a player nor an agent meets the standing floor as a surprise.
Reputation is a view of the sentiment substrate (BL-545, sentiment substrate; BL-546, reputation
becomes a sentiment view), and because sentiment decays there is no permanent floor: falling below
the procurement threshold is recoverable (BL-391, reputation floor).

---

## The mercenary contract — the sell side

> **A contract is a `condition_set` the client will pay to have become true, by a deadline.**

That is the whole spine, and it is short because [`../META_LAYER.md`](../META_LAYER.md)'s predicate
machinery is generic, pure and deterministic — and procurement already evaluates one for its
embargo decline, so using a predicate as *contract terms* is precedent rather than invention.

A record carries: the client, the company, a **success predicate**, a **deadline in ticks**, a fee,
a deposit fraction, and accrued state.

**The variety of contract types is authored data in the predicate, not a type enum in C++.** Take
this province, hold that one, break a siege, escort a convoy, deny a road — **adding a contract kind
is a Lua change**, to the contract-template table.

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

### Where offers come from

> **A contract offer is a want the client nation cannot meet alone.**

That framing is the original design and stands; what it runs on does not. `history_sim.cpp`'s
campaign scorer is an Era −1 year-loop calculation over polity/region data — unreachable from the
campaign tick and typed on data the campaign world does not carry — so it cannot be the live
source. The campaign-tick mechanism runs on what IS live:
[`../politics/NATIONS.md`](../politics/NATIONS.md)'s nation budget pass, whose `contracted_force`
priority line is the treasury weight a threatened nation cannot spend on its own garrison
(the budget scorer's threat term). `derive_contract_offers`, called from `run_nation_step` after
the budget pass, turns that weight into named offers:

- **Target.** Among provinces held by the nation's highest-grudge neighbour, the border province
  with the **lowest garrison strength** — deterministic argmin, ties broken by ascending province
  id. ([`../military/MILITARY.md`](../military/MILITARY.md) § Nation garrisons owns what a garrison
  and its strength are.)
- **Fee.** The `contracted_force` line's spendable share is a claim on *that tick's* budget, not a
  pot — too thin, most ticks, to clear an indivisible offer in one pass. It accumulates instead into
  a per-offer **`offer_escrow`**, a visible treasury line, until it clears the contract template's
  minimum fee; the deadline runs from the template, independent of how long the escrow took to
  fill.
- **Cadence.** A nation may hold **several offers open concurrently**, one per threatened border
  province (Ben, 2026-08-23, elicitation), rather than one at a time. Each still expires
  independently after `offer_ttl_ticks` with no taker, returning its escrow to the treasury.
  Because more than one offer can be filling at once, a tick's `contracted_force` share is applied
  to open offers **in issued-tick order** (oldest first) until exhausted, so a younger offer waits
  behind an older one rather than splitting the share — an offer's fill time reads off its place in
  the queue, not a fraction of a fraction.

Three things this buys with no new machinery: offers are **deterministic**, because the budget
scorer already is; **legible**, because the fee tracks a treasury weight the simulation actually
computed; and the contract market gets a rhythm from the same source as the garrison it will
fight — a nation threatened enough to garrison a border and unable to fund it alone is exactly a
nation that offers the province as a contract.

*Owner: BL-572 (contract offers).*

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

*This is the one answer taken on design judgement rather than derived from an existing mechanism, and
it is recorded as reversible.*

**Q4 — Offers are private per client**, visible only where the activity fog already reaches
(`body_activity_visibility`, BL-089 (activity fog)). **A polity you have never dealt with and cannot
see does not offer you work.** This gives the discovery layer a second job for free, and gives
reputation a **reach** dimension: standing opens the map, not just the price.

### The client, and the money

- **The client is a nation with a treasury.** [`../politics/NATIONS.md`](../politics/NATIONS.md)
  settles that a nation holds money and spends it down weighted priority lines; the
  **`contracted_force`** priority line is where a fee comes from (§ Where offers come from, above).
- **Fees come from the budget, never minted** (Ben, 2026-08-22, design register). A nation cannot
  offer what it cannot pay, and **offers dry up when treasuries do**, preserving the conservation
  rule with no exception. It gives the contract market a second source of rhythm besides political
  tension: a war-poor nation stops hiring.
- **The relationship rides on sentiment's Trust dimension**, not on a parallel axis
  ([`../politics/RELATIONS.md`](../politics/RELATIONS.md) § The settled model). `corp_reputation` is
  a view of the same substrate — so the reputation this loop moves and the Trust a nation reads are
  **one quantity**.

### Rivals bid for the same work (Ben, 2026-08-22)

**Rivals compete for the same contracts, and losing a bid is a real outcome** — BL-551 (contract
bidding). An offer is not addressed to the player; it is **contested**. Reputation therefore bites
in a second direction: it gates whether you may *request* work, and under bidding it decides whether
you *win* it. A rival taking contracts is also what keeps the mechanism exercised rather than built
and idle.

### On screen (Ben, 2026-08-22)

**A ledger and the map** — offers, active contracts and terminal states in the fold-out column, with
the objective province marked. And a third thing:

> *"Contracts consume resources, I believe we should be able to render moving contracts
> (convoys)."*

So a contract is not only an agreement, it is **materiel in motion**: fulfilment draws goods, and
those goods move on the logistics network as convoys. **The contract layer and the supply layer
share a surface**, and a player watching a convoy is watching a contract being delivered. That
couples this document to [`LOGISTICS.md`](LOGISTICS.md) tightly, and is the strongest argument that
a contract is a *logistics* object rather than only a financial one.

### Serialisation

A flat-binary stream, following procurement exactly. The `condition_set` in the record is a
structured predicate, not a scalar, and it is stored as an **index into the authored
contract-template table** with the predicate itself in Lua (Ben, 2026-08-22). The record stays
fixed-size, contract kinds stay data, and a save that outlives a template-table change **rejects
rather than reinterprets** — which the version bump gives.

### Verification

The item's requirement demands one observed instance of *each* terminal state, written that way
because a verb can ship correct and never be observed firing across five seeds (NR-121, `hire_unit`).
An offer that never fires is indistinguishable from a mechanism that does not exist.

### Explicitly out of scope

The combat that resolves a contract (BL-315 (conflict spine) — this owns what is *at stake*, never
how a fight resolves). Multi-round fee haggling — take-it-or-leave-it in this cut. Contracts between
the player and another *corporation* rather than a polity. Any narrative or dialogue layer.

---

## Where the parts live

| Concern | File |
|---|---|
| Procurement records, quotes, contracts | `src/world/components.hpp`, `world.hpp` |
| Procurement serialisation | `src/world/procurement.{hpp,cpp}` |
| The three procurement verbs | `src/world/corp_command.cpp` |
| The predicate a contract is made of | `src/world/condition_set.{hpp,cpp}` |
| Offer derivation from the nation budget | `src/world/nation_ai.cpp`, `nation_step.cpp` § `derive_contract_offers` |

**Related authorities.** [`MARKETS.md`](MARKETS.md) (the anonymous alternative, and where
procurement's clearing-side interaction lives), [`../politics/NATIONS.md`](../politics/NATIONS.md)
(the client and its treasury), [`../politics/RELATIONS.md`](../politics/RELATIONS.md) (reputation,
and the sentiment substrate it is a view of), [`../META_LAYER.md`](../META_LAYER.md) (the predicate),
[`../military/MILITARY.md`](../military/MILITARY.md) (the force a contract is won with; § Nation
garrisons is what it fights), [`../ui/DISCOVERY.md`](../ui/DISCOVERY.md) (the fog that decides who
offers you work).

**Owning items.** BL-377 (mercenary contract seam) — the sell side. BL-350 (procurement seam) — the
buy side; BL-445 (procurement UI) and BL-446 (scorer procures) its two users. BL-551 (contract
bidding) — contested offers. BL-391 (reputation floor) and BL-392 (what a contract is worth) — the
economics. BL-315 (conflict spine) — what a contract puts at stake. BL-572 (contract offers) — offer
derivation, target, fee/escrow and cadence. BL-571 (nation garrisons) — the force a contract is won
against. BL-569 (province holder) — what "held" means for targeting.
