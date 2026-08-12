# Kepler — Historical Foundations

Why the campaign world is market-based and non-hegemonic, told as a stage ladder the
generation stack can hang dated history lines off. Companion to `../generation/PLANETOLOGY.md`
(which ends at the civilisation gate) and `../generation/NATION_GENERATION.md` /
`../generation/GENERATION_STRATEGY.md` (which assume the world this document explains).

> **THE CAMPAIGN EPOCH IS 0 CE, NOT 1960 (Ben, 2026-08-12).** Read every "1960" below as "the
> campaign epoch", and the epoch as **0 CE**. The project refocused to the ancient era (NR-177),
> so this ladder no longer runs up to an industrial start — it runs up to an ancient one.
>
> **What this costs the ladder, stated plainly.** Stages 1–4 (agrarian surplus, the enforceable
> promise, fragmentation-with-connectivity, capital disciplines the sovereign) are pre-industrial
> and land inside a 4000 BCE → 0 CE run intact. **Stages 5 and 6 — the energy transition and
> saturation — are past the new epoch entirely** and are no longer campaign backstory; they
> become DLC-era material alongside the parked space arc. They were already flagged superseded
> by BL-223, so this narrows a doc that was already known to be wrong there rather than
> overturning settled prose.
>
> The one-shot pass this document describes is now driven by the stepped year-tick sim
> (`src/world/history_sim.{hpp,cpp}`), 4000 BCE → 0 CE, stepping 100 → 50 → 20 → 10 → 5 → 1
> years. A settle-dominated deep prehistory is the intended shape (NR-178, ruled 2026-08-12).

## Purpose

The campaign premise makes three big historical claims:

1. **Markets, not command.** Every body hosts independent markets with dynamic prices; the
   background economy is saturated and privately legible.
2. **No hegemon.** ~45 nations coexist; none can conquer the rest; power projection happens
   through firms, not armies. *(The invariant itself is owned by **BL-224 (non-hegemony
   invariant, open)** — the "none can conquer the rest" half is asserted, not yet enforced.
   The landed `terrain_combat` scalars — `terrain_defence` / `terrain_attrition` /
   `terrain_resistance`, `src/world/terrain_combat.cpp`, BL-233 measurement landed
   2026-07-31 — are its intended input.)*
3. **Corporations as first-class actors.** An immortal legal fiction can own, trade, and expand
   across borders — and nations tolerate it.

> **Caution (2026-07-31): claims 2 and 3 are only partly settled.** Ben rejected this
> document's Stage 5–6 account on 2026-07-30 (recorded in BL-223, averted rupture): the
> rupture was **averted, not past**, and the player identity is
> **nation-with-chartered-corps** (BL-094, player-nation pivot — designed), not a free-floating
> corporation. Claim 3's mechanism — the enforceable promise, the charter — stands; the
> *why-the-player-is-a-corporation* framing that Stages 5–6 built on it does not. Do not read
> the corporation-player premise below as settled.

None of these are natural defaults. Each is the residue of specific historical stages. This
document names those stages, states the causal lesson each one carries, and marks the hooks
where the generator's endowment variables (oxygen, ocean fraction, arable land, fossil fuels,
ore accessibility) should bend the story per seed.

## The stage ladder

Stages are ordered and causal: each consumes the output of the one before. The planetology
gate chain hands off at Stage 0. Era 0 gameplay begins at the top of the ladder.

> **Stages 0–2 are BUILT** (BL-221, landed 2026-07-30) — `src/world/history_ladder.{hpp,cpp}`,
> verified by `tools/verify/history_ladder_harness.cpp` (H1–H5). **Stages 3–4 are now BUILT too**
> (BL-218 nations rewrite + BL-219 corporations rewrite, landed 2026-08-02) —
> `src/world/settlement.{hpp,cpp}`, verified by `tools/verify/settlement_harness.cpp` (S1–S8);
> see § Implementation — Stages 3–4. **Stages 5–6 as written are SUPERSEDED** — rejected
> 2026-07-30, replacement owed to BL-223 (averted rupture, design-owed); see their banners.
> See § Implementation (after Stage 6) for what the code actually does and what it stands in for.

### Stage 0 — Agrarian surplus (hand-off from planetology)

**What happens.** Surplus food → dense settlements → division of labour → strangers who must
trade rather than share.

**Lesson.** Markets are a technology for cooperation between non-kin. They cannot precede
surplus and density.

**Generator hook.** Arable land and ocean fraction set *where* this happens first and how many
independent cradles emerge. More cradles → deeper eventual fragmentation (feeds Stage 2).
History line: `"-XXXX: First granary cities on the {region} floodplain."`

### Stage 1 — The enforceable promise

**What happens.** Contract law extends beyond kinship: partnerships, credit, and eventually the
charter — an entity that outlives its members and can own, sue, and be sued.

**Lesson.** The deep enabler of markets is not coinage but the enforceable promise between
strangers. Corporate personhood is this stage's apex, and the game's core rule — *the
corporation persists while it holds any asset* — is that legal doctrine made mechanical.

**Kepler divergence — the Charter Age.** Kepler needs a founding myth for corporate personhood,
its VOC moment, and it should come *early* relative to Earth. The registered-in-a-home-nation
but operationally-free compromise in `../generation/CORPORATION_GENERATION.md` is exactly the
bargain sovereigns strike with chartered companies: taxability and seizability in exchange for
freedom of operation.
History line: `"YYYY: The {nation} Charter Act — first perpetual company registered."`

### Stage 2 — Fragmentation with connectivity (the non-hegemony ingredient)

**What happens.** Many competing states, close enough to trade, copy, and poach talent; none
able to conquer the rest.

**Lesson.** A hegemon can *choose* stagnation — suppress a technology by decree and it stays
suppressed. Forty-five rivals cannot: a capability banned in one polity re-emerges next door.
**Competition among states is the market mechanism applied to governance itself.**

**Kepler divergence — no unification epoch.** Kepler never had a Rome or a Ming. Its geography
(mountain ranges landing on borders via the weighted Voronoi pass) kept conquest expensive and
exit cheap. This is the single cheapest lore assertion with the largest downstream payoff: it
explains why 1960-Kepler is more multipolar than 1960-Earth.

**Generator hook.** Nation count is already an outcome of landmass. The history pass should
read the final nation count back into the narrative: more nations → earlier and harder
Stages 3–4.
History line: `"YYYY: The {range} Peace — {n} realms confirm mutual borders."`

> **Implementation status:** Stages 0–2 are built (BL-221, landed 2026-07-30) —
> `src/world/history_ladder.{hpp,cpp}`. Full detail in § Implementation, moved below Stage 6.

### Stage 3 — Capital disciplines the sovereign

**What happens.** Interstate competition makes states dependent on credit → dependent on
merchants and bond markets → forced into credible commitment. States that default or
confiscate lose the next war.

**Lesson.** This is how the state that *serves* markets (rather than merely taxing them) is
born. It is also why the campaign's nations respect corporate property in 1960: their
ancestors that didn't were outcompeted.

**Game-mechanical echo.** Asset seizure exists but is costly to the seizing nation (sentiment,
credit access). Progressive loss instead of a lose screen mirrors the historical norm:
sovereigns squeezed companies far more often than they destroyed them.

**Built, partially (BL-218, 2026-08-02).** The stage's *mechanism* — credit disciplining the
sovereign — is not simulated. What landed is its two legible consequences: the charter culture's
**sealed-oath god** buys its provinces an industrialisation-date bonus (contract law reaching
capital, one stage early), and the **war** rupture branch costs both belligerents abundance
rather than paying the winner. The seizure-cost half is still owed to BL-223 (averted rupture).

### Stage 4 — Energy transition

**What happens.** Fossil fuels break the organic-economy ceiling; growth compounds; wealth
stops being zero-sum land.

**Lesson.** Coal-near-cities made Britain — endowment, not virtue. Once wealth compounds,
conquest's return-on-investment collapses relative to trade's, which changes what war is *for*.

**Generator hook.** This is the stage most sensitive to the endowment vector. Fossil fuel and
ore accessibility on owned tiles should let the history pass *name* which nation industrialised
first, purely from tile data. Each seed gets its own Britain.
History line: `"YYYY: {nation} lights the first coke furnaces of the {region} basin."`

**BUILT (BL-218, landed 2026-08-02).** The hook is closed: `run_settlement` scores each
province's ancient fuel endowment against the world's own mean, gates industrialisation on
*above-average* fuel, and `derive_national_character` names the three earliest furnaces once
nations exist. Each seed does get its own Britain, chosen from tile data alone. The creed sits
on top of the endowment rather than beside it — a people who raised a forge god did so because
their cradle held ore (`CREEDS.md`), so that god's provinces light up earlier. Endowment, not
virtue, in both directions.

### Stage 5 — The Rupture (hegemony fails its final audition)

> **SUPERSEDED (2026-07-31).** Ben rejected this stage as written on 2026-07-30 (recorded in
> BL-223, averted rupture): the rupture was **averted, not past** — the campaign opens under
> the *threat*, not the residue, of the catastrophe — and the player identity heads for
> **nation-with-chartered-corps** (BL-094, player-nation pivot; corp now, nation at the
> v0.2.0 era), so the "framing rule" below
> (corporations as the only intact actors, why the player is a corporation) is dead as
> written. The replacement design is **owed to BL-223** — it is not written here, and this
> section must not be built against. Kept for the record only.

**What happens.** The WW3-scale global rupture that closes Era 0's backstory. Every claimant to
hegemony fights; every one is exhausted, winners included.

**Lesson.** Non-hegemony is not chosen — it is the residue of hegemony failing repeatedly and
expensively. On Earth this took two world wars and a nuclear ceiling; on Kepler the Rupture
compresses it into one discrediting catastrophe.

**Framing rule (load-bearing).** The Rupture should not merely *permit* space expansion — it
should be the event that:

- discredits territorial conquest as an instrument of policy,
- leaves corporations as the only actors with intact capacity and cross-border legitimacy,
- creates the deterrence ceiling under which ambition is forced into economic channels.

That single framing answers, in one stroke: why the player is a corporation, why nations
tolerate it, and why rivals compete through markets rather than armies.

### Stage 6 — Saturation and the last frontier (campaign start, 1960)

> **SUPERSEDED (2026-07-31), with Stage 5.** The saturation economics below survive on their
> own merits (they restate `../generation/GENERATION_STRATEGY.md`), but the "post-hegemonic
> condition" framing hangs on Stage 5's past rupture, which was rejected 2026-07-30.
> Replacement owed to BL-223 (averted rupture, design-owed).

**What happens.** Kepler's markets are old, margins thin, frontiers closed. The saturated
background economy of `../generation/GENERATION_STRATEGY.md` — with its live "opportunity
gap" — is the *post-hegemonic condition*, not a neutral starting state.

**Lesson.** Expansion happens from saturation, not strength. The specialists who go to space go
because the terrestrial pie is fully claimed, and the Rupture cracked open the only frontier
left: upward.

### Where the rupture sits — three docs disagree *(reconciliation owed to BL-223)*

Noted 2026-07-31: three documents currently place the rupture at three different points in
time. This doc's Stage 5 put it in the **past** (now superseded); `docs/CONCEPT.md` treats it
as the **Era 0 exit**; `docs/research/ERA1_TECH_LANDSCAPE.md` frames it as a **visible
countdown**. **BL-223 (averted rupture, design-owed) owns the reconciliation** — none of the
three should be built against until it lands.

---

## Implementation — Stages 0–2 *(BL-221, landed 2026-07-30)*

`src/world/history_ladder.{hpp,cpp}`, a sibling pass that runs after the tile pipeline and
**interleaves with** nation generation:

```
generate_body_tiles                 terrain exists
run_history_ladder            ->    cradles, fragmentation, Stage 0's line
nation_params_from_ladder     ->    fragmentation DRIVES the seed budget
generate_nations                    polities grow
record_institutional_history  ->    Stages 1-2, which need the outcome
```

Two entry points rather than one, because the Charter Act names a nation and the border accord
counts them — neither exists until the political pass has run.

**It drives, it does not narrate** (Ben, 2026-07-30). Stage 0's cradle count and Stage 2's
fragmentation are computed *before* the political map and shape it: a fragmented world seeds
more densely and lets smaller nations survive the merge. Retiring Voronoi outright is BL-218's;
this is the honest hook until then. Asserted, not assumed — harness group H2.

**Nation count: 14 → 43.** Settled by Ben, 2026-07-30: *"Ignore the previous assertions. We will
simulate war to narrow down the count if needed. Just let naturally different cultures emerge
here."* That effectively meets this document's own "~45 nations" claim, which BL-224 had flagged
as an unowned 3× gap — but note it was met by **letting cultures emerge**, with consolidation
deferred to a future war/conflict stage, *not* by tuning to hit 45. `world_audit`'s BL-053 R1/R3
were repointed accordingly: R1 now asserts the ladder's construction guarantee (the derived floor
can never fall below half the base) instead of a literal that is no longer constant, and R3's
ceiling became a runaway guard rather than a target.

**Substituted inputs, named rather than faked.** Two designed inputs do not exist in `src/` yet:
river connectivity (BL-170) and domesticable clades (BL-217). Stage 0 scores cradles from arable
terrain, landform, habitability, coastal access and the biosphere's generated `endemics` instead.
Both items **refine** this score when they land rather than replacing it, so nothing needs
rewiring. `agrarian_score` marks exactly where each missing term slots in.

**Determinism.** Fresh stage tags (`0x5A11` / `0xC4A7` / `0xF2A6` — none collides, and
PLANETOLOGY.md's warning about `0x4A71012u` being folded twice still stands). All scoring is
integer with an explicit tie-break by tile index; the cradle argmax keeps the lowest index on a
tie. No transcendentals, and the nation walk is over a sorted key list rather than raw
`unordered_map` order.

**Known tuning gap, honestly recorded.** Across a 12-seed spread every world produced the
multipolar accord and none produced a hegemon, so Stage 2's failure branch is currently written
but unreached. Ben asked to *see* failure cases, so this is a tuning target for BL-219's sweep
(rarity tuning), not a defect to hide — the harness prints the split every run.

---

## Implementation — Stages 3–4 *(BL-218 + BL-219, landed 2026-08-02)*

`src/world/settlement.{hpp,cpp}`, sequenced between the creeds and the political map:

```
run_history_ladder            ->  cradles, fragmentation
run_creeds / tribal conflict  ->  one pantheon per cradle; welding
run_settlement                ->  PROVINCES: culture, ancient endowment, furnaces
generate_nations                  seeded on the province anchors
derive_national_character     ->  the three axes, as outputs
resolve_historical_ruptures   ->  collapse / war / revolution, and the erasure
generate_corporations         ->  focus from the corp's home province
```

**The province is the new unit, and it exists to carry two things at once.** A cradle was a
*people*; a nation is a *territory*; neither can say "these particular fields, under these
particular gods, sitting on this particular ore". The province can, which is why belief,
endowment and industrial timing all hang off it and why BL-219 could read corporate focus
straight out of it without a new mechanism.

**Pantheons are mapped, not re-rolled.** A province inherits its nearest cradle's culture, so
the distribution of gods across the map is a record of who walked where. That mapping then feeds
back into the material history: a forge god only exists where the cradle window held ore, so
"the forge god's country industrialises early" is not flavour laid over the data — it is the
same fact read twice, one stage apart.

**Scores are world-relative.** Every endowment class scores 500 at the world's mean and 1000 at
twice it. An absolute gain either saturates one class or never fires another (the first
implementation classified all 75 provinces `farm`); relative scoring also survives the
`deposit_scalar` abundance tier without re-tuning.

**Seeding changes, expansion does not.** `nation_params::seed_tiles` carries the province
anchors into Pass 1 and the BL-053-tuned growth machinery is reused untouched. The size variance
now emerges from where people settled instead of being dialled in — and the cheap alternative
(keep Voronoi, narrate over it) was rejected as the lying-figure problem: prose asserting a
settlement history the territory does not reflect.

**The record is destructible, and the hole is visible.** A won war plants the victor's pantheon
on the provinces taken and erases the lines naming them, leaving a dated lacuna with a count of
what was lost (Ben, 2026-08-02). A conquered province keeps its founders in `founding_culture`
and its conquerors in `culture` — the erasure is of the record, never of the fact, which is the
pair a later religion or diplomacy layer needs to describe a grievance. Across a six-seed spread,
four worlds lost part of their record to a war.

**BL-217's mechanism, reused unchanged.** The ruptures are the second checkpoint class, drawing
through `resolve_checkpoint` with eligibility as a filter and never a weight — exactly what
BL-217 predicted this pass would need, so no second branch mechanism was written.

**Honest scope.** A green harness means the pass is self-consistent, deterministic and wired
into the political map — *not* that its dates or thresholds are calibrated against anything.
Rarity tuning is still BL-219's sweep. The ruptures are bounded to the six most-contested
nations, so their count is a property of the design rather than of the map size.

> **Successor filed (2026-08-02): the Era −1 history sim.** Ben's steer, same day this landed:
> promote this one-shot pass to a **running year-tick simulation from 0 CE to the campaign
> epoch**, as the proving ground for the nation AI and the mil-sim, tuned against a seed spread
> of earth-like worlds. And an **overturned decision**: simulated wars stop resolving as
> abstract scalar comparisons and fight with **real typed units and doctrine-parameter tactics**
> — the same combat engine the main era inherits ("drives, not narrates" survives; the *abstract*
> half does not). Filed as **BL-271 (Era −1 sim)** · **BL-272 (unit/doctrine combat — records
> the overturn)** · **BL-273 (province demography)** · **BL-274 (era-keyed unit rosters)** ·
> **BL-275 (history sweep — BL-210's batch-sweep payload)**; Sprint 5's theme.

---

## The compact thesis

> **Markets scale where no one can dominate, and no one dominates where exit is cheap** — a
> rival state next door, a rival market next planet.

The thesis itself stands — Stages 0–2 generate it. But its closing corollary as previously
written here ("learned this one catastrophe earlier than Earth did") leaned on Stage 5's past
rupture and is **superseded with it** (2026-07-31, pending BL-223 — averted rupture). What
Kepler learned, and when, is BL-223's to restate.

## Integration checklist *(status 2026-07-31)*

- [x] Planetology history pass emits Stage 0 lines from the endowment vector — **landed**
  (BL-221): `run_history_ladder` writes the granary-cities line from arable share + cradles.
- [x] Nation generation reads final nation count back into Stage 2 narrative text — **landed**
  (BL-221): `record_institutional_history` counts surviving realms into the accord line.
- [x] History pass names the first-industrialiser nation from tile fossil/ore data (Stage 4) —
  **landed** (BL-218): `derive_national_character` names the three earliest furnaces from the
  province endowment scores. BL-222 (industrial ladder) is thereby substantially overtaken.
- [ ] Charter Age line names the player corporation's home-nation legal tradition (Stage 1) —
  the nation-naming half landed with BL-221 (the Charter Act names the charter cradle's
  nation), and BL-218 added the creed half (the charter culture's sealed-oath god buys its
  provinces an industrialisation bonus); the *player-corp* linkage remains owed, and is
  complicated by **BL-094 (player-nation pivot)**.
- [ ] Rupture event text (Era 0 → Era 1 transition) — the Stage 5 framing rule it referenced
  is superseded; owed to **BL-223 (averted rupture, design-owed)**.
- [ ] Asset-seizure mechanics carry a sentiment/credit cost consistent with Stage 3 — owed to
  **BL-223** alongside the diplomacy origin it defines.

## Open questions

- Should the Rupture's belligerents and outcome be seeded (varying per campaign) or canonical?
  Seeded is consistent with the rest of the generation stack; canonical is cheaper to write
  quest/tech fiction against. *(Question survives the Stage 5 supersession in altered form —
  an averted rupture still has claimants; now BL-223's to answer.)*
- Does Kepler get a nuclear-equivalent deterrence ceiling explicitly, or is the Rupture's
  memory alone the ceiling? (Affects Era 1+ conflict design.)
- How much of this ladder surfaces to the player — a codex, generated history lines only, or
  ambient flavour in quest text?
