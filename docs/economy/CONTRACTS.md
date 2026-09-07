# Project Io — Contracts

> **Settles:** what makes a promise a contract rather than a market trade · who the named
> parties are and what either of them may refuse · what a procurement deliverable is, how it is
> priced, and what marks it complete · what is deliberately out of scope for the form.
> **Not here:** the anonymous, instant, price-only exchange (MARKETS) · what carries a promised
> cargo (SUPPLY) · what the payment does to the balance (FINANCE).
> **Confused with:** MARKETS.md, SUPPLY.md, FINANCE.md.

**A contract is a promise between two named parties, priced, paced, and refusable.** It is the
alternative to the market: where the market is anonymous, instant and price-only, a contract has a
counterparty who can say no.

Io has **one**: procurement, the buy side. The player is a **buyer** of promises, not a seller of
them.

| | **Procurement** — the buy side |
|---|---|
| The player | buys equipment |
| Deliverable | `resource + quantity` — fungible |
| Completes when | `ticks_elapsed` reaches lead time |
| Terminal states | completed, cancelled |
| Item | BL-350 (procurement seam) |

**There is deliberately no sell side** (Ben, 2026-08-30). A symmetric mechanism — the player
*hired*, against a `condition_set` and a deadline — is not part of the design. The military side of the game will be
approached some other way, and this document does not hold a placeholder for it.

---

## Where this sits in the chain

Under SYSTEMS.md § The progression chain — *each system's ceiling is the next system's door*:

    markets → CONTRACTS → force
                 ↑
    the market cannot sell you a lead time, a refusal, or a reputation

**What forces you in.** The market prices goods and nothing else. It cannot express *"I need forty
of these in eight weeks and I need to know now"*, and it has no memory of who you are. The first
time either matters, you need a counterparty.

**What it opens.** Procurement opens equipment you do not manufacture — which is the whole shape of
the mercenary company, who *procures* rather than produces. That is what couples this document to
force: a company that buys its materiel rather than building it reaches military capability through
this seam, and through no other.

**What it caps you at.** Reputation. It is the axis on which a company can price itself out of its
own supply.

**The chain's own link from force to territory is not this document's, and this document does not
claim it.** Contracts buy the *means*; what force is then worth is
[`../military/MILITARY.md`](../military/MILITARY.md)'s and BL-315 (conflict spine)'s.

---

## Procurement — the buy side

A procurement contract is **a build order placed with someone else**: the commit-on-affordability,
draw-materials-per-tick, pay-across-the-build shape of construction pacing, with the materials
drawn against the **supplier's** market and the output delivered to the **buyer's** pool. The
counterparty is a NAMED corp with a price, a lead time, and a possible refusal — not a purchase
order against an unlimited market, and not an order-book entry (the book is price-time priority
over anonymous asks; it has no representation for a named counterparty or a lead time). It joins
the same `corp_command` seam the order book does, for the same reason: the player's press and the
AI's command are one implementation.

- **Three verbs** (`corp_verb::request_quote` / `accept_quote` / `cancel_contract`, `world.hpp`
  §11–14, append-only after `set_workforce_auto`). `request_quote` evaluates four decline
  conditions in order — no capacity (the supplier holds no completed building that produces the
  good), no input access (the supplier's local market cannot supply its recipe's inputs), embargo
  (`world::corp_embargo_conditions`, a `condition_set` per supplier — the generic predicate
  machinery of BL-342 reaching procurement for free), reputation floor
  (`world::procurement_reputation`, a **view** of the relational substrate — see below) — and
  returns a distinguishable `corp_command_result` for each.
- **Split payment.** A deposit (`economy.procurement.deposit_fraction`, 0.25) debits at
  `accept_quote`; the remainder is drawn evenly across the quote's `lead_time_ticks`
  (`economy_system.cpp`'s contract-pacing pass, right after the capability-points pass). The pace
  is fixed rather than market-gated (stretch/pause on the supplier's live throughput) — a known
  simplification against construction pacing's own model.
- **Lead time is derived, not authored**: `base_lead_ticks × ceil(quantity / supplier_throughput)`
  — a bigger order takes longer, a capable supplier is faster, and the quote is incidentally an
  intelligence channel (legitimate under BL-068, competitor visibility: the supplier volunteers
  its own throughput in the price it quotes).

**It is on the serialisation seam.** `procurement.cpp` is a flat-binary stream in `world/*`,
alongside `history_log` and `order_book`: leading magic + version, count-prefixed records,
**rejection rather than reinterpretation**, and a `static_assert` on record size as the tripwire.
**No relational value crosses it at all** — the sentiment substrate carries its own leg of the
seam, and the whole-world snapshot carries the per-pair record.

**Both a human and a rival use it.** The player reaches the three verbs through a procurement
surface in the app (BL-445, procurement UI), and the rival scorer enumerates a procurement
candidate alongside build, survey and hire (BL-446, scorer procures). A counterparty mechanism with
no human and no AI user is a seam with no subject.

**Reputation is readable before a refusal.** The blackboard export carries the pair's reputation
(BL-390, seam read-back), so neither a player nor an agent meets the standing floor as a surprise.
Reputation is a view of the sentiment substrate (BL-545, sentiment substrate; BL-546, reputation
becomes a sentiment view), and because sentiment decays there is no permanent floor: falling below
the procurement threshold is recoverable (BL-391, reputation floor). It moves on **completion (+)
or cancellation (−)** and on nothing else — narrow by design: it shifts price and tie-breaking,
and never gates access beyond the decline floor above.

### Terms — what a contract is actually worth

The seam is only half the deal; the other half is the terms, and three of them are load-bearing
(BL-392, contract terms).

1. **Goods land on the BUYER's body.** A contract carries a **`delivery_body`** — the buyer's own
   body, taken as the body of the lowest-id building they own (lowest id, not first-in-`assets`,
   because a demolish permutes that list and the quote must be reproducible). It degrades to the
   supplier's fulfilment body only when the buyer owns nothing anywhere. Delivering to the
   supplier's body instead would land goods on a body where the buyer holds no processor
   reservation, and the auto-surplus path would liquidate the whole delivery the tick it arrived.
2. **A commitment buys a discount.** The quote is spot less a **volume discount**, asymptotic in
   the order size — `volume_discount_max × q / (q + volume_discount_half_quantity)`, authored in
   `scripts/economy.lua` under `economy.procurement` — so no order however large drives the price
   to zero, and the terms improve monotonically with the size of the commitment. A quote at the
   live spot price would settle at break-even minus friction, strictly worse than buying on the
   market.
3. **Lead time reads the SUPPLIER.** The throughput divisor is that supplier's real per-tick
   output of that good — extraction sites targeting it at their own rate, processing facilities
   at their recipe's yield of it times theirs, summed in ascending building id (a float sum needs
   a fixed order). Floored at 1 tick: a contract completing in zero ticks is a spot purchase
   wearing a contract's name.

**Freight is the price of the distance.** Delivering across bodies costs
`offbody_freight_fraction` of the order's pre-discount goods value, carried on the contract as
`freight_cost` and included in the total the deposit and the instalments are computed from. It is
set **below** `volume_discount_max` on purpose, so a genuine volume order still beats spot after
carriage; a same-body delivery pays nothing.

**Every credit this seam moves is a TRANSFER.** The supplier is credited exactly what the buyer is
debited, in the same statement, deposit, instalments and freight alike — the supplier arranges the
carriage, so paying them for it keeps the flow closed. On completion the goods are **drawn from
the supplier's pool** at the fulfilment body as far as their stock goes, with any shortfall built
to order (which is what a build order placed with someone else means).

Verified by `tools/verify/money_conservation.cpp`.

### Explicitly out of scope

**Multi-round fee haggling.** A quote is take-it-or-leave-it: accept it, or route around the
refusal. **Any narrative or dialogue layer** over the counterparty. And **the player as supplier** —
see § the opening: there is no sell side, and a request to add one is a design question, not a gap
in this section.

---

## Where the parts live

| Concern | File |
|---|---|
| Procurement records, quotes, contracts | `src/world/components.hpp`, `world.hpp` |
| Procurement serialisation | `src/world/procurement.{hpp,cpp}` |
| The three procurement verbs | `src/world/corp_command.cpp` |
| The predicate a decline is evaluated with | `src/world/condition_set.{hpp,cpp}` |

**Related authorities.** [`MARKETS.md`](MARKETS.md) (the anonymous alternative),
[`../politics/RELATIONS.md`](../politics/RELATIONS.md) (reputation, and the sentiment substrate it is a view of),
[`../META_LAYER.md`](../META_LAYER.md) (the predicate an embargo decline is expressed in),
[`../military/MILITARY.md`](../military/MILITARY.md) (the force this seam equips).

**Owning items.** BL-350 (procurement seam) — the buy side; BL-445 (procurement UI) and BL-446
(scorer procures) its two users. BL-391 (reputation floor) — the standing that gates a supplier's
refusal.
