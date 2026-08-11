# BL-340 + BL-350 — joint design pass (Sprint 10 prep)

> **Why this file exists, and what to do with it.** Written 2026-08-10. The design belongs in
> `backlog.json`'s `design` field for each item, per DELIVERY.md § Design state. It is staged here
> instead because **the repository is mid-merge**: `claude/infallible-faraday-93b0ed` (`39f8316`,
> BL-293 the order book) is being merged into `main` with 17 files unresolved, `backlog.json`
> among them. Writing into a conflicted store would either corrupt the merge or be discarded by
> its resolution.
>
> **Fold this into `backlog.json` once the merge resolves, then delete this file.** Both items
> flip `design-owed` → `designed`. The corrections in § 5 and the review entries in § 6 land at
> the same time.

---

## 1. Why these two are one design

They are mechanism and content for the same premise, and each is incoherent alone.

BL-094's rewrite says the militia *"uses private companies to build equipment for space"*. BL-350
is the buying; BL-340 is the thing bought. Ship BL-350 alone and the militia contracts for goods
that do not exist. Ship BL-340 alone and seven new resources arrive with no buyer that needs them
— which is exactly how BL-286's eight logistics goods landed inert two minors ago, and still are.

The joint design's load-bearing decision is a single line, stated here and enforced in both halves:

> **`spacecraft_components` carries no substrate demand. The militia's contracts are its only
> demand.**

That is what makes the coupling real rather than thematic. The background economy does not want
space equipment; a militia does. The procurement seam is therefore the *only* reason the top of
BL-340's chain exists, and BL-340 is the only reason the seam has an object.

---

## 2. BL-340 — the processing chain roster

### 2.1 The question the item was filed to settle

> *"How many of RESOURCES.md's 31 resources actually need to exist for the prototype's chains to
> close, versus how many are Era 1 content that can stay unimplemented?"*

**Answer: seven new enum values, plus six base prices on raws that already exist.** The rule that
produces that number is stated below and is the more durable half of the answer.

### 2.2 The admission rule

> **A `resource_type` value must be consumed by an authored recipe, or be a terminal object some
> named actor contracts for or consumes. No orphans.**

BL-286 is the cautionary precedent, and it is worth naming plainly: eight goods were added with
enum, serialisation and base-price wiring, and **no consumer**. Two minors later `grain`, `fodder`,
`salt`, `transport_capacity`, `charcoal`, `iron_blooms` and `bullion` still have no mechanic that
reads them. They cost save-format width and readout space and return nothing. This item does not
repeat that.

Applying the rule to RESOURCES.md's design list:

| Candidate | Verdict | Reason |
|---|---|---|
| Silicon, refined copper, REE alloy | **In** | Consumed by the Tier-3 recipes below; each also rescues a dead raw. |
| Alloys, electronics | **In** | Consumed by the Assembly Plant. |
| Spacecraft components | **In** | Terminal — the militia's contract object (§ 1). |
| Machinery | **In** | Terminal — the Fabricator's *alternative* to alloys, and a substrate-basket good (§ 2.5). |
| Liquid oxygen | **Out** | Already folded into both Chemical Plant recipes (BL-308). Nothing outside the plant would hold it. |
| Clean water, building materials, consumer goods, medical supplies, utilities | **Out** | Habitability goods; POPULATION.md's consumer is deferred from the prototype. No consumer, no value. |

Machinery is the one judgement call, so its reason is stated rather than assumed: it makes the
Fabricator a **choice**. One building, two recipes — machinery sells into the background economy,
alloys feed the space chain. That opportunity cost is the whole reason the Fabricator is
interesting, and without machinery the "choice" is a single forced path.

### 2.3 The real content is pricing, not the enum

BL-340's filed design leads with the enum extension as the expensive part. **That is backwards**,
and the correction matters for sizing the work.

Extending the enum is nearly free, and this is measured rather than assumed: every per-resource
array is already sized off `resource_count` (`components.hpp`, `world_gen_config.hpp`,
`recipe_registry.hpp`, `law.hpp`, `planetology.hpp`), so BL-286's eight-good extension needed *no*
per-array edit. There is no fixed-width `std::array<float, 32>` anywhere in `src/world/`.

The expensive half is that **three of the four raws the new recipes consume cannot be bought.**
`silica`, `copper_ore` and `rare_earth_ore` carry `base_price` 0, and `clear_markets` skips every
resource with no base price. A processing building auto-buys its input shortfall from the market.
So a Refinery pointed at silica draws against a market that will never supply it, and the recipe
is dead on arrival.

> **This is the item's actual centre of gravity: BL-340 closes the minable-but-unsellable
> asymmetry.** The enum extension is a precondition for it, not the work.

### 2.4 The authored tranche

**Seven new enum values** (32 → 39), appended in tier order:

| Value | Tier | Recipe | Building |
|---|---|---|---|
| `silicon` | 2 | silica 2.0 → 1.0 | Refinery |
| `refined_copper` | 2 | copper_ore 2.0 → 1.0 | Smelter |
| `ree_alloy` | 2 | rare_earth_ore 2.0 → 1.0 | Refinery |
| `machinery` | 3 | steel 1.0 + refined_copper 1.0 → 1.0 | Fabricator |
| `alloys` | 3 | steel 1.0 + ree_alloy 1.0 → 1.0 | Fabricator |
| `electronics` | 3 | silicon 1.0 + refined_copper 1.0 + ree_alloy 0.5 → 1.0 | Electronics Lab |
| `spacecraft_components` | 3 | alloys 2.0 + electronics 1.0 → 1.0 | Assembly Plant |

**Six base prices on existing raws**, closing the asymmetry:

| Resource | Price | Note |
|---|---|---|
| coal | 2.0 | Also the steel reagent — see § 5.3. |
| silica | 2.0 | Silicon input. |
| copper_ore | 3.0 | Refined-copper input. |
| rare_earth_ore | 6.0 | "Low concentration, high base price" (RESOURCES.md). |
| iron_nickel_ore | 3.0 | Recipe 6 already consumes it; it had no market. |
| platinum_group_metals | 40.0 | Terminal by design — RESOURCES.md calls it "the primary high-value trade good of the asteroid belt". Its job is to be sold. |

**Seven base prices for the new goods**, on a deliberate ladder:

| Good | Input cost | Price | Margin |
|---|---|---|---|
| silicon | 4.0 | 5.0 | 1.25× |
| refined_copper | 6.0 | 7.5 | 1.25× |
| ree_alloy | 12.0 | 16.0 | 1.33× |
| machinery | 15.5 | 22.0 | 1.42× |
| alloys | 24.0 | 34.0 | 1.42× |
| electronics | 20.5 | 29.0 | 1.41× |
| spacecraft_components | 97.0 | 140.0 | 1.44× |

The margin widens up the tiers, which is the intended shape: Tier 3 is where RESOURCES.md promises
"widest price divergence", and `spacecraft_components` at 140.0 sits **56× iron ore**. That ladder
is the value gradient the whole space-equipment premise rests on.

*These are first-cut authored constants in the existing style, not balanced figures.* Note the
shipped table is not internally consistent either — `food_rations` at 6.0 from 2×3.0 of
agricultural produce is exactly break-even — so the tranche should not be held to a standard the
existing seven do not meet. Retune by playtest.

### 2.5 Substrate demand — and the one real hazard

Add to `scripts/economy.lua` § `substrate.demand_basket`:

```
machinery       = 0.20   -- background industry
electronics     = 0.15
refined_copper  = 0.12
silicon         = 0.10
alloys          = 0.08
```

`spacecraft_components` is **deliberately absent** (§ 1).

> **The hazard, named because it will not be obvious at build time.** Substrate *supply* is
> deposit-derived (`resource_abundance` → capacity). The new Tier-2/3 goods have no deposit, so
> their capacity is **zero**. `resolve_price` maps demand-with-zero-supply to `base_price × 4.0`,
> the band ceiling. Every basket good in that list would therefore **peg at 4× base forever**,
> and the price readout would look plausible while being a constant.

The fix is to give the substrate an abstract capacity for refined goods — the background economy
manufactures these too, it does not only dig them. That is a change to `inject_substrate_demand`,
not to the basket, and it is the single largest piece of real logic in this item.

`spacecraft_components` needs neither, since it has no substrate demand to strand.

**Sprint 9's retro is the reason this is flagged in the design rather than discovered in the
build:** three bugs there were each a five-minute fix once found, and each invisible until a real
300-tick rollout ran against it. This one is the same shape.

### 2.6 Scope explicitly excluded

- Named `building_type` enum values (Refinery, Fabricator, Electronics Lab, Assembly Plant). They
  stay recipes on the generic `processing_facility`, exactly as the shipped five do. Introducing
  named types is a separate item and is not needed for any of this.
- Habitability goods and their buildings.
- Retuning the shipped seven base prices.
- Deposit generation changes — every raw this item prices already has authored deposits (BL-040).

---

## 3. BL-350 — the procurement / contract seam

### 3.1 The spine

> **A procurement contract is a build order placed with someone else.**

BL-095 already ships the whole pacing model: commit on affordability, draw materials per tick as
real market demand, progress at the rate the market can supply, stretch up to ~10× or pause, pay
across the build rather than up front. A contract is that same object with two changes — the
materials are drawn against the **supplier's** market, and the output is delivered to the
**buyer's** pool.

This is the design's main claim to legibility. It reuses a shipped, understood, already-tuned
mechanism instead of inventing a second payment-and-pacing shape, and it means a stalled contract
stalls for a reason the player already understands from construction.

### 3.2 The four filed questions, answered

**Q1 — Does the treasury debit on order, on delivery, or split?**

**Split: a deposit on order, the remainder drawn per tick across the lead time** — mirroring
BL-095 exactly. Deposit fraction is a Lua constant (`economy.procurement.deposit_fraction`, first
cut **0.25**).

The two alternatives both fail concretely. On-delivery-only makes ordering free, so the dominant
strategy is to order from every supplier and cancel all but the fastest. On-order-only makes the
contract a spot purchase with a delay — the counterparty's price and refusal do no work, and it
collapses back into the order book.

**Q2 — Is refusal a hard block or a payable penalty?**

**A hard decline at quote time, with a stated reason. Not payable-through.**

Refusal is not a wall, though — it is *priced information*. A declining supplier states why, and
other suppliers may still quote, so the player routes around a refusal rather than buying through
it.

The reason this matters: a payable override collapses refusal into a surcharge, which makes the
counterparty a vending machine with a markup. That is precisely what BL-094's rewrite retracted
the shared treasury to avoid. **If everything is buyable, there is no counterparty.**

Four decline conditions:

| Condition | Test |
|---|---|
| No capacity | Supplier holds no completed building that can produce the good. |
| No input access | Supplier's market cannot supply the recipe inputs — reuses BL-095's `paused` test. |
| Law / embargo | A `condition_set` (BL-342, shipped v0.1.3) evaluates false. |
| Reputation floor | Pair standing below a threshold. |

The third is the useful one: `condition_set` is shipped generic predicate machinery, so the law
layer reaches procurement **for free**. That is BL-315's "what the militia must answer to" axis
arriving without new machinery — and it is the strongest single argument that this seam is sitting
in the right place.

**Q3 — Does reputation persist?**

**Yes, minimally: one scalar per `(buyer, supplier)` pair, moved by completed and cancelled
contracts only.**

Effects are deliberately narrow — it shifts the quoted price within a band, and it breaks ties in
quote ordering. It does **not** unlock exclusive goods and does **not** by itself gate access
(beyond the floor in Q2).

The reasoning both ways: a persistent scalar is what makes a *sequence* of contracts a strategy
rather than each one an isolated purchase. But the moment it gates access to goods, it is a tech
tree wearing a relationship's clothes, and BL-087 already owns that job.

**Read alongside BL-345** (politics MVP relationship axis, v0.1.6, `designed`). If BL-345 lands
first, this reads its axis instead of storing a second one. Two relationship scalars between the
same pair of entities is the kind of duplication that is cheap to avoid now and expensive later.

**Q4 — Parallel to, or an extension of, BL-037's preferred-seller routing?**

**A parallel object on the same seam — not an order-book entry.**

*This answer is written against the incoming BL-293 branch (`39f8316`), which changes its
premises.* Post-merge the order book is world state, `clear_markets` reads it directly, and three
verbs joined `corp_command` (enum now 11 wide, append-only).

A contract cannot be a book entry: the book is price-time priority over anonymous asks, with no
representation for a named counterparty or a lead time. But procurement should join the **same
command seam**, for the reason BL-293 gives in its own commit message — so the player's press and
the AI's command are one implementation rather than two that agree.

Three new verbs, appended (enum 11 → 14, append-only per BL-293's convention):

| Verb | Effect |
|---|---|
| `request_quote` | Ask a named supplier for price + lead time on a quantity of a good. |
| `accept_quote` | Convert a live quote into a contract; debits the deposit. |
| `cancel_contract` | Terminate in flight; forfeits the deposit, moves reputation. |

And a **fourth flat-binary stream** after `history_log` and `order_book`, following both exactly —
magic `IOPC`, a version, count-prefixed records, and rejection rather than reinterpretation on
anything that does not parse.

The composition, stated once: **the contract's per-tick material draw enters market demand exactly
as BL-095's construction draw does.** So the supplier's own inputs clear through the existing order
book at market prices. Procurement is a *layer over* the market, not a second market.

### 3.3 Lead time is derived, not authored

```
lead_time_ticks = base_ticks × ceil(quantity / supplier_throughput_per_tick)
```

where throughput is what the supplier's relevant buildings actually produce. A bigger order takes
longer; a supplier with real capacity is faster.

This gives the mechanic a second job worth having. **The quote is an intelligence channel** — you
learn something about a rival's industrial capacity by asking it for a price. That is legitimate
under BL-068 (competitor internals private, markets public), because the supplier *volunteers* the
figure in its own quote rather than the player reading its internals. It is a new, diegetic route
through the activity fog, and DISCOVERY.md should pick it up when this lands.

### 3.4 The buyer is not the militia yet — and does not need to be

BL-350 is filed `blocked_on: [BL-094]`. **That block should be dropped**, and the reason is a
version inversion worth stating plainly: BL-350 targets **v0.1.14**, BL-094 targets **v0.3.0**. An
item cannot be blocked on one that lands three minors later without being silently unbuildable.

The block dissolves on inspection, because **the seam is buyer-agnostic**. A contract is between
two corp-like entities holding a treasury and a pool. Today both are `corporation`. When BL-094's
militia entity lands, it becomes the natural buyer with **no change to the seam** — it holds a
treasury and a pool like any other.

So: `requires: [BL-094]` stays as a read-alongside; `blocked_on` clears. What BL-094 supplies is
the *narrative* — why the buyer does not simply build the thing itself — and narrative does not
block a mechanism.

### 3.5 Relationship to BL-280 (negotiated tax rate)

The shared shape is real: **quote → accept/decline → standing term**, pointed at different
counterparties.

But BL-280's actual blocker is untouched by this design. Its open question is *what the nation
wants in return* for a lower rate — the counterparty cost, without which the tax dial is free
money. BL-350 answers the analogous question structurally (the supplier wants price, lead time and
standing), which does not transfer to a sovereign granting a tax concession.

**Recommendation: BL-350 does not wait for BL-280.** Extract a shared `negotiation` primitive when
the second user actually exists — not speculatively, on one user, against an item that is still
`design-owed` and difficulty 5.

### 3.6 Scope explicitly excluded from v0.1.14

- The militia entity itself (BL-094, v0.3.0).
- Unit materiel consumption — what a fielded unit *draws* per tick. BL-332's military points are a
  separate currency and stay separate.
- A negotiation dialogue (BL-334, Stage C). The quote is a number with a reason string, not a
  conversation.
- Tax bargaining (BL-280).
- Multi-round haggling. A quote is take-it-or-leave-it in this cut; counter-offers are a later item
  if the seam proves worth deepening.

---

## 4. Build order, once promoted

BL-340 lands first, and it is a hard sequence rather than a preference — BL-350 has nothing to
contract for until `spacecraft_components` exists.

1. **BL-340 S1** — enum + base prices for the six existing raws. Independently verifiable: the
   dead raws become tradeable with no new goods in play.
2. **BL-340 S2** — the seven recipes + their base prices.
3. **BL-340 S3** — substrate basket + the abstract refined-goods capacity (§ 2.5). *The risk step.*
4. **BL-350 S1** — the contract record, its serialisation stream, the three verbs.
5. **BL-350 S2** — quote generation (price, lead time, the four decline conditions).
6. **BL-350 S3** — per-tick draw + delivery, reusing BL-095's pacing.
7. **BL-350 S4** — reputation scalar.

Steps 1–3 and 4–7 are two coherent vertical slices, which is the natural sub-agent split if the
work fans out.

## 4a. Requirement sketches (DELIVERY.md step 0, owed at promotion)

**BL-340** — `headless`, extending `econ_harness` or a new `resource_chain_harness`: a rollout in
which every one of the seven new goods is produced, priced and cleared at least once, **and no
priced good sits pegged at the 4× band ceiling for the whole run**. That second clause is the
§ 2.5 hazard, and it is the assertion that would have caught it.

**BL-350** — `headless`, a new `procurement_harness`: one contract quoted, accepted, paced and
delivered end to end with the treasury debited in the split shape; plus one decline observed for
**each** of the four refusal conditions. Sprint 9 shipped a v0.1.5 done-definition that honestly
recorded a path never observed firing (NR-121); this requirement is written so that does not
recur here.

---

## 5. Corrections owed to existing records

**5.1 — BL-340's `files` list names a file that does not exist.** It lists
`src/world/serialisation.cpp`; `find src -iname "*serial*"` returns nothing. The real file scope
is `src/world/components.hpp`, `src/world/world_gen_config.hpp`, `scripts/recipes.lua`,
`scripts/economy.lua`, `docs/economy/RESOURCES.md`, `docs/economy/PRODUCTION.md` — plus, after the
merge, `src/world/order_book.{hpp,cpp}`, whose reader validates a resource byte against the known
`resource_type` range and therefore moves when the enum widens.

**5.2 — BL-107's design is stale, and BL-340's dependency on it needs re-reading.** BL-107 says
*"no flat-binary serialiser exists in `world/*` yet"* and is marked BLOCKED on that. As of the
incoming branch there are **two** such streams — `history_log.{hpp,cpp}` and
`order_book.{hpp,cpp}` — and both already carry the magic + version header BL-107 specifies, each
citing BL-107 as their reason. BL-107's remaining scope is the *world-snapshot* header, not the
first-ever one.

The consequence for BL-340 is a genuine reversal, and it cuts against what this pass first
concluded: BL-340's filed note says it *"wants a decision on BL-107 first"*, and on the pre-merge
tree that read as vacuous. Post-merge it is **live again** — `read_order_book` rejects a resource
byte outside the known range, so widening the enum moves a validation boundary in a shipped
serialised stream. Not a blocker, but the version bump is now a real step rather than a
hypothetical one.

**5.3 — PRODUCTION.md and `recipes.lua` disagree on the steel recipe.** The doc's Smelter table
says *iron ore + coal (reagent) → steel*; `recipes.lua` id 1 is `{ iron_ore = 2.0 }` with no coal.
Pricing coal (§ 2.4) makes this worth reconciling in the same pass — either add the reagent or
correct the doc. Recommend adding it: it gives coal a consumer, which the admission rule (§ 2.2)
would otherwise require.

**5.4 — BL-350's `blocked_on: [BL-094]` should clear.** See § 3.4.

**5.5 — MARKETS.md § Known limitations needs a post-merge re-read.** It states the buy-order book
is engine-only with no UI path. The incoming branch changes the sell side and moves the book into
world state; the buy-side claim may or may not survive.

**5.6 — BL-293 is still open in `backlog.json` but appears shipped on the incoming branch.** It
reads `design-owed`, priority A, v0.2.0. Flip it at the merge.

---

## 6. Review-log entries owed (NEEDS_REVIEW.json)

`NEEDS_REVIEW.json` is itself conflicted and does not parse, so these are staged here and land with
the fold-in. Four are `decision-taken` — decisions made on Ben's behalf, which per Rule 0c must not
be left in a closing summary.

| Kind | Entry |
|---|---|
| `decision-taken` | **BL-350 unblocked from BL-094.** A v0.1.14 item cannot block on a v0.3.0 one; the seam is buyer-agnostic (§ 3.4). |
| `decision-taken` | **Tranche sized at seven values by an admission rule** — no orphan resources, with BL-286's eight inert goods as the stated precedent (§ 2.2). |
| `decision-taken` | **`spacecraft_components` gets no substrate demand**, making the militia its only buyer (§ 1). This is the joint design's load-bearing choice. |
| `decision-taken` | **Refusal is not payable-through** (§ 3.2 Q2). Chosen over a surcharge model because a buyable refusal is not a counterparty. |
| `observation` | **A merge was left unresolved in the working tree**, with conflict markers in `backlog.json`, `requirements.json`, `REFINED.md`, `NEEDS_REVIEW.json` and `DEVLOG.md`. `backlog_query.js` does a plain `JSON.parse` and would fail hard; a conflicted store is invisible until something reads it. |
| `observation` | **Machinery's inclusion is the one soft call** in § 2.2. It earns its place as the Fabricator's opportunity cost, not as a chain link. Worth Ben's eye. |
