# Project Io — Contracts

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

**There is deliberately no sell side.** A symmetric mechanism — the player *hired*, against a
`condition_set` and a deadline — is not part of the design. The military side of the game will be
approached some other way, and this document does not hold a placeholder for it.

**Nothing of it remains in the code.** The records, the two tick passes, their serialisation, the
authored template table and the comms traffic are gone (Ben, 2026-08-30). Two things survive by
necessity rather than by design, and both are inert: the `accept_offer` and `abandon_contract`
values in `corp_verb`, which **reject**, because that enum is append-only and deleting a value
would renumber every verb below it; and the word *contract* in `procurement_contract`, which is
this document's other subject and is live.

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

**Related authorities.** [`MARKETS.md`](MARKETS.md) (the anonymous alternative, and where
procurement's clearing-side interaction lives), [`../politics/RELATIONS.md`](../politics/RELATIONS.md)
(reputation, and the sentiment substrate it is a view of),
[`../META_LAYER.md`](../META_LAYER.md) (the predicate an embargo decline is expressed in),
[`../military/MILITARY.md`](../military/MILITARY.md) (the force this seam equips).

**Owning items.** BL-350 (procurement seam) — the buy side; BL-445 (procurement UI) and BL-446
(scorer procures) its two users. BL-391 (reputation floor) — the standing that gates a supplier's
refusal.
