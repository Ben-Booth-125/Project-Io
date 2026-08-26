# Project Io — Relations

**How actors in Io feel about, rate, and gate each other.** This document owns the whole relational
layer, and its first job is to say **which quantity answers which question** — because the code
carries several, and three of them share overlapping names.

Companion to [`NATIONS.md`](NATIONS.md) — nations are actors in this layer too, and their read of a
corporation is one grain of the substrate below.

---

## The quantities, and the one that is not what its name suggests

This table is the reason the document exists. Read it before touching any of them.

| Quantity | Question it answers | Shape | Origin | Gates |
|---|---|---|---|---|
| **Stance** | *How does A feel about B?* | hostile (directed) / friend (symmetric) | **declared** | interdiction, battle, march; passage and immunity |
| **Sentiment** | *What has B's conduct earned with A?* | two floats (Access, Trust) per (observer, subject) | **derived** | procurement quotes via the Reputation view; territory via Access |
| **Reputation** | *Will B do business with A again?* | **a VIEW of sentiment's Trust**, not a store | **earned** | procurement quotes |
| **Embargo** | *Is A forbidden to trade in B's terms?* | a `condition_set` per corp | **authored** | procurement quotes |
| **Standing** | *How strong is A?* | five bands on three axes | **derived** | nothing — it is a readout |

**Settled 2026-08-22 (Ben, ruling on NR-520): sentiment is the substrate.** The model is **two
layers, not four quantities**, and the first two rows are the layers rather than siblings. See
§ The settled model directly below — read it before extending anything here. Reputation is listed
as its own row because it is still the question procurement asks — but it is not a place anything
is stored, and there is no second table it can disagree with.

**Standing is the odd one and the naming is a known hazard.** It is not a relation at all: it is a
coarse public read of how powerful a corporation is, with no second party anywhere in it. NR-304
records the collision being noticed when stance was named — *"called it 'stance', not 'standing' —
the latter is taken by the power read."*

The distinction that keeps them straight, from `stance.hpp`'s own header: **"stance is how a corp
feels about another; standing is how strong it is."**

---

## The settled model — two layers

Ben's ruling on NR-520, 2026-08-22. Putting every quantity in one table made visible what none of
them showed individually: **three designs were converging on the same continuous derived quantity
and none knew about the other two** — CONCEPT.md's sentiment, the nation→corp Access/Trust read,
and procurement's reputation.

They converge deliberately:

    stance      DECLARED, discrete    — an act, always attributable to an ACTOR
    sentiment   DERIVED,  continuous  — a reading, attributable to CONDUCT

**Sentiment** is one directed, continuous, derived value from an **observer** to a **subject**,
where an actor is a corporation or a nation. Directed, because every use needs it — reputation is
keyed (buyer, supplier), a nation reads a corp and never the reverse, a grudge is asymmetric by
nature; symmetry, where a consumer wants it, is a read that asks twice. Continuous, and if it is ever
presented in bands that is a *presentation* choice, never the storage.

**Stance sits on top as the declared layer**, unchanged by the substrate beneath it.

### The invariant: sentiment must never become hostility on its own

Ben ruled on 2026-08-17 that hostility is **a declared state a corp opts into**, and that a rival
*"may score and declare it, never acquire it ambiently."* That ruling stands, and the substrate must
not quietly overturn it.

So **sentiment informs a declaration; it never makes one.** A scorer may read sentiment and choose
to `declare_hostile`. **No threshold anywhere may flip a stance table by itself.** This is enforced
structurally: `sentiment.cpp` is never handed a `world&` — it operates on a `sentiment_table` and
nothing else, so it has no reach to `corp_hostile_pairs`, `corp_friend_pairs` or
`corp_friend_offers`. The signature is the claim. `sentiment_harness` R1 asserts it anyway, since a
structural argument nobody checks is one refactor from being false.

The one direction that *is* legal is the reverse: an observed declaration is an input to sentiment
(`sentiment_factor_kind::hostility_declared`). Declarations move sentiment; sentiment never moves
declarations.

### What one substrate dissolves

| Separate design | Becomes |
|---|---|
| procurement reputation, its own serialised map | a **view** of sentiment at buyer→supplier grain |
| nation→corp Access / Trust, a per-nation table | **dimensions** of sentiment at nation→corp grain |
| Era −1 grudges, with nowhere to live | **seeded** nation→nation sentiment |
| CONCEPT.md's sentiment | **the layer itself** |

**The reputation deadlock (BL-391) stops existing rather than getting fixed.** A reputation floor
with only two writers — contract completion and cancellation — is unrecoverable, because the
recovery path runs: raise reputation → complete a contract → accept a quote → obtain a quote → *be
above the floor.* A cycle with no entry. A continuous quantity that **decays toward neutral has no
permanent floor by construction** — a burned relationship heals, and "wait" becomes a legitimate
strategic move. The fix is a property of the substrate, not a patch on procurement.

*Owned by BL-545 (sentiment substrate) and BL-546 (reputation migration). BL-540 (nation→corp
stance), BL-541 (directional tariffs) and BL-391 (reputation floor deadlock) are shaped onto it.*

---

## The substrate — `src/world/sentiment.{hpp,cpp}`

`world::sentiment` is a `sentiment_table`: a `std::map` keyed on the ordered pair (observer,
subject), holding two floats per row. Four properties, all load-bearing:

1. **Directed.** A→B is not B→A.
2. **Two dimensions — Access and Trust — at every grain** (Ben, NR-522: not one, not per-grain).
   *A substrate whose shape varies by grain is not a substrate.* They move independently and decay
   independently. A third dimension must gate something before it is added.
3. **Derived from conduct, folded through authored factors.** A row is written only by
   `world::note_conduct(params, observer, subject, factor)`: one occurrence of one
   `sentiment_factor_kind`, multiplied by that factor's authored weight on each dimension. The
   factor is the unit of authorship — **a new way to affect sentiment is a table row, never a code
   branch**, because anything else makes Ben's *"tonnes of realistic ways to affect sentiment"*
   (2026-08-22) unaffordable.
4. **Decays toward neutral, and neutral is erased.** Each tick a dimension sheds an authored
   fraction of its remaining distance to zero; a row that reaches neutral (within an epsilon that
   makes neutral *reachable*) is removed, so a fully-forgotten pair is indistinguishable from one
   that never dealt.

### The factors

`sentiment_factor_kind`, append-only — it is the array bound the fold indexes with:

| Factor | Conduct |
|---|---|
| `contract_completed` | a procurement contract delivered as promised |
| `contract_cancelled` | a procurement contract cancelled by the counterparty |
| `trade_conducted` | goods actually moved between the pair — commerce as its own slow warming |
| `equity_taken` | the observer wanted a firm and the subject took it — a **buyout** (`docs/economy/FINANCE.md` § Whole-firm acquisition); Ben's own worked example, *"buying a background firm that they wanted"*. Sentiment is where an acquisition's *political* cost lands |
| `hostility_declared` | the subject declared hostility toward the observer |
| `friendship_accepted` | a friendship offer from the subject was accepted — both parties observe it, each in their own row |
| `levy_charged` | the observer was charged under a law the subject authored — the corp→nation grain's bread and butter |
| `embargo_imposed` | the subject refused to deal with the observer at all |
| `lobbied_against` | the subject spent to move a nation against the observer — BL-539's political grain |
| `force_used` | force was used against the observer's interest — the sharpest input, and still only an input |

### The authored numbers

Decay lives in `scripts/economy.lua` § `economy.sentiment`, as a **rate, not a half-life** —
deriving it in-engine would put a `std::pow` on the tick. Ben's anchor (NR-568, 2026-08-23): *a
cancellation is forgotten in nine quarters.* One tick is one quarter, so that is a half-life of
nine ticks, and

    rate = 1 − 2^(−1 / 9) = 0.074125

is `trust_decay_per_tick`. `access_decay_per_tick` is the same value — there is no separate ruling
for Access. The loader (`recipe_registry.cpp`) rejects either rate unless it is a finite number in
[0, 1]; an authored rate is never clamped silently.

The factor weights for `contract_completed` and `contract_cancelled` are **seeded from the
procurement rates** (`seed_procurement_sentiment`, `recipe_registry.hpp`: `reputation_on_complete`
1.0, `reputation_on_cancel` −2.0 on Trust) and may be overridden by `economy.sentiment.factors`.
Every other factor's weight is authored at zero — each is a named seat for its emitter, and a
factor whose emitter has nothing to say moves nothing.

The table crosses the serialisation seam in full: `write_sentiment` / `read_sentiment` for the
substrate's own stream, and the per-pair record in the world snapshot. The procurement stream
carries no reputation of its own — a copy there would be the second store the substrate exists to
delete.

---

## The quantities in turn

### 1. Stance — declared, and it gates

`src/world/stance.{hpp,cpp}` (BL-448, corp stance). Three tables on `world`, four verbs on the
corp-command seam.

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
partial-apply. Stance is in the world snapshot (`w_enum` / `r_enum` against a named maximum, so an
out-of-range byte is rejected rather than cast): a declared war outlives the session.

**What hostility gates:**

| Consumer | What hostility does |
|---|---|
| `supply_system.cpp` | A hostile unit on a convoy's tile **intercepts it** (BL-458, interdiction) |
| `battle_system.cpp` | Hostile pairs are what **open a battle** (BL-467, engagement trigger) |
| `economy_system.cpp` | War **flips the march queue** (BL-470, NR-344) |

Note the care taken in the interdiction check: it tests `is_hostile(unit_owner, cargo_owner)`, in
that direction only. *"A corp that has been declared against but has not answered is a victim, not
a raider."*

**What friendship permits — two things** (Ben, 2026-08-22): **passage through territory** and
**immunity from interdiction**. Both consumers read an existing predicate at an existing site.
Immunity is the cheap half and the invariant makes it safe: `declare_hostile` dissolves a
friendship row atomically, so a friendship check in the interception path is not a competing
predicate but an early-out on a pair hostility has already excluded. Passage is the larger half and
needs a reading: a friend's anchors counting toward your reach field (recommended — it makes
friendship a *logistics* fact, and cannot be confused with Access), or placement permission (which
overlaps Access at a different grain). *Owned by BL-549 (friendship permits two things).*

**Who declares.** A rival scores stance and may declare hostility toward any corp, the player's
included — BL-450 (rivals score stance), granted 2026-08-18 on the standing terms: deterministic,
seeded, scored-utility, legal verbs only. Hostility stays a declared state a corp opts into; a
rival may score and declare it, never acquire it ambiently.

**A declaration against the player is SIGNALLED** (Ben, 2026-08-22). This overturns NR-350, which
had a declaration *"stay silent, discovered on contact rather than announced"*; newest-dated wins,
and the reversal is recorded rather than quietly applied. The cost is real: directed hostility
exists so *"a corp can be at war and not know it yet"* — the ambush property interdiction was
designed around — and signalling removes that property *for the player*. Interdiction becomes a
known risk rather than a surprise. Directedness itself is untouched — hostility still needs no
reciprocation. Whether rival-vs-rival declarations are equally visible is a BL-068
(competitor-visibility) question this ruling does not settle. The surface is BL-449 (stance needs
a surface).

### 2. Reputation — a view of sentiment

Reputation is the **Trust dimension of `world::sentiment` at (buyer, supplier) grain.** There is
one read point, `world::procurement_reputation(buyer, supplier)`, and two writes, both one
occurrence of authored conduct folded into the substrate — `contract_completed` from
`economy_system.cpp` on delivery, `contract_cancelled` from `corp_command.cpp` on cancellation.

**The floor is a temporary state, and every consumer must read it that way.** `request_quote`
declines a buyer below `reputation_floor` (−5.0), and treats "below the floor" as a condition of
today rather than a verdict: a −2 cancellation halves in nine ticks and its row is erased inside
~130, with no new verb, no reparations mechanic, no special case (`sentiment_harness` R3h–R3m;
`procurement_harness` R2 asserts both halves). That reading must hold for every later consumer of
the axis — BL-377's contract seam reads the same one.

**The floor and the current standing must be VISIBLE.** A player reasoning about a number they
cannot see is the deadlock's other half; the seam's read-back is BL-390 (the seam has no
read-back).

### 3. Embargo — an authored refusal

`world::corp_embargo_conditions` — a `condition_set` per corp, checked by `request_quote` in
`corp_command.cpp`, serialised in the world snapshot. When the predicate fires the supplier refuses
the buyer outright, and the refusal is conduct: `embargo_imposed` is its sentiment factor.

The read path proves `condition_set` reaches procurement for free. An embargo's **author** is a
law — it is the relationship family of BL-155's four-family taxonomy, and which embargoes the
company must route around is BL-399 (company answerability). A per-counterparty allow/deny at the
corp's own hand is BL-161 (counterparty allow/deny).

### 4. Standing — a readout, read by one surface

`src/world/standing.{hpp,cpp}`. Three axes: **reach** (distinct bodies with a building),
**capital** (balance), **market share** (this corp's clearing income over the total).

**The five bands are retired** (Ben, 2026-08-26: *"We don't need company information to be
invisible"*). Every axis now reads as an exact figure, and what varies between corporations is
not how coarse the number is but **whether there is one at all**:

- **Reach and market share are public for everyone.** Both are derived from facts already
  observable — buildings are visible on canvas, market supply/demand aggregates are the deliberate
  public signal (`DISCOVERY.md` § Competitor visibility). Nothing was ever hidden about them
  except by the banding, so nothing is lost by printing them.
- **Capital is a filed figure and follows the ownership class — for everyone but you.** The
  observer's **own** corporation always shows exact figures: disclosure is a rule about reading
  *another* firm, and a corporation that could not read its own balance sheet could not be run.
  For every other corporation it is exact where the firm is `public` and absent where it is
  `private` or `closed` — the same binary disclosure the quarterly return takes
  (`docs/economy/FINANCE.md` § Disclosure). A dash means *this firm does not file*, never *you have
  not earned this*.

**It is still deliberately NOT unified with `corp_ai`'s scorer.** The AI optimises an internal
ground-truth quantity; this is a published read. Unifying them would make every rival directly
optimise the published number — **a Goodhart trap**, and the reason the duplication is correct
rather than lazy. Retiring the bands does not touch that property.

*Production is intentionally absent from the axes. A rival's true output needs their recipe and
workforce dial, and neither is a filed figure — there is no honest source for it, so the axis is
left out rather than faked.*

### 5. The nation's read of a corporation

**BL-540 (nation→corp stance)** is a nation's read of a corporation, settled in the
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
state — despite sharing the word. It is the nation→corp grain of the substrate: Access gates who
may operate inside the borders, and `levy_charged`, `embargo_imposed` and `lobbied_against` are
the factors that move it. Its emitters are the first writers into the Access dimension.

---

## Open questions

1. **How many dimensions does sentiment carry?** (NR-522) Two — Access and Trust — at every grain
   is the ruling; the open half is the admission rule for a third, which must gate something before
   it is added.

2. **Which reading does passage take** — reach-field contribution (recommended) or placement
   permission? Owned by BL-549 (friendship permits two things).

3. **Are rival-vs-rival declarations visible to the player?** A BL-068 (competitor-visibility)
   question the signalling ruling leaves open.

---

## Where the parts live

| Concern | File |
|---|---|
| Stance tables, verbs, invariants | `src/world/stance.{hpp,cpp}` |
| The three tables on the world | `src/world/world.hpp` § `corp_hostile_pairs`, `corp_friend_pairs`, `corp_friend_offers` |
| The relational substrate | `src/world/sentiment.{hpp,cpp}`; the table on `world.hpp` § `sentiment` |
| Reputation (a **view**) | `src/world/world.hpp` § `procurement_reputation` / `note_conduct`; written from `economy_system.cpp` (completion) and `corp_command.cpp` (cancellation) |
| The authored factor weights and decay | `src/world/recipe_registry.hpp` § `seed_procurement_sentiment`, `sentiment()`; loaded from `economy.sentiment` in `scripts/economy.lua` |
| Embargo | `src/world/world.hpp` § `corp_embargo_conditions`; read in `corp_command.cpp` § `request_quote` |
| Standing bands | `src/world/standing.{hpp,cpp}` |
| Interdiction gate | `src/world/supply_system.cpp` |
| Battle engagement gate | `src/world/battle_system.cpp` |
| The Stance column | `src/ui/corporation_panel.cpp` |
| Checks | `tools/verify/sentiment_harness.cpp`, `stance_determinism.cpp`, `procurement_harness.cpp` |

**Related authorities.** [`NATIONS.md`](NATIONS.md) (the nation as an actor, and its read of a
corp), `docs/military/MILITARY.md` (what hostility permits militarily),
`docs/ui/DISCOVERY.md` (BL-068, the competitor-visibility rule both stance and standing obey),
`docs/economy/MARKETS.md` (§ Procurement, where reputation is spent),
`docs/ai/AI_OPPONENT.md` (the scorer that declares).

**Owning items.** BL-545 (sentiment substrate) is the spine; BL-546 (reputation migration) is the
view. BL-540 (nation→corp stance), BL-541 (directional tariffs) and BL-391 (reputation floor
deadlock) sit on it. BL-450 (rivals score stance) owns the rival's declaration; BL-449 (stance
needs a surface) is the UI half; BL-390 (the seam has no read-back) owns making the value visible;
BL-549 (friendship permits two things) owns passage and immunity.
