# Kepler — Historical Foundations

Why the campaign world of 1960 is market-based and non-hegemonic, told as a stage ladder the
generation stack can hang dated history lines off. Companion to `../generation/PLANETOLOGY.md`
(which ends at the civilisation gate) and `../generation/NATION_GENERATION.md` /
`../generation/GENERATION_STRATEGY.md` (which assume the world this document explains).

## Purpose

The campaign premise makes three big historical claims:

1. **Markets, not command.** Every body hosts independent markets with dynamic prices; the
   background economy is saturated and privately legible.
2. **No hegemon.** ~45 nations coexist; none can conquer the rest; power projection happens
   through firms, not armies.
3. **Corporations as first-class actors.** An immortal legal fiction can own, trade, and expand
   across borders — and nations tolerate it.

None of these are natural defaults. Each is the residue of specific historical stages. This
document names those stages, states the causal lesson each one carries, and marks the hooks
where the generator's endowment variables (oxygen, ocean fraction, arable land, fossil fuels,
ore accessibility) should bend the story per seed.

## The stage ladder

Stages are ordered and causal: each consumes the output of the one before. The planetology
gate chain hands off at Stage 0. Era 0 gameplay begins at the top of the ladder.

> **Stages 0–2 are BUILT** (BL-221, landed 2026-07-30) — `src/world/history_ladder.{hpp,cpp}`,
> verified by `tools/verify/history_ladder_harness.cpp` (H1–H5). Stages 3–6 remain design only.
> See § Implementation, below, for what the code actually does and what it stands in for.

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
but unreached. Ben asked to *see* failure cases, so this is a tuning target for BL-219's sweep,
not a defect to hide — the harness prints the split every run.

---

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

### Stage 4 — Energy transition

**What happens.** Fossil fuels break the organic-economy ceiling; growth compounds; wealth
stops being zero-sum land.

**Lesson.** Coal-near-cities made Britain — endowment, not virtue. Once wealth compounds,
conquest's return-on-investment collapses relative to trade's, which changes what war is *for*.

**Generator hook.** This is the stage most sensitive to the endowment vector. Fossil fuel and
ore accessibility on owned tiles should let the history pass *name* which nation industrialised
first, purely from tile data. Each seed gets its own Britain.
History line: `"YYYY: {nation} lights the first coke furnaces of the {region} basin."`

### Stage 5 — The Rupture (hegemony fails its final audition)

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

**What happens.** Kepler's markets are old, margins thin, frontiers closed. The saturated
background economy of `../generation/GENERATION_STRATEGY.md` — with its live "opportunity
gap" — is the *post-hegemonic condition*, not a neutral starting state.

**Lesson.** Expansion happens from saturation, not strength. The specialists who go to space go
because the terrestrial pie is fully claimed, and the Rupture cracked open the only frontier
left: upward.

## The compact thesis

> **Markets scale where no one can dominate, and no one dominates where exit is cheap** — a
> rival state next door, a rival market next planet.

Kepler in 1960 is a world that learned this one catastrophe earlier than Earth did.

## Integration checklist

- [ ] Planetology history pass emits Stage 0 lines from the endowment vector.
- [ ] Nation generation reads final nation count back into Stage 2 narrative text.
- [ ] History pass names the first-industrialiser nation from tile fossil/ore data (Stage 4).
- [ ] Charter Age line names the player corporation's home-nation legal tradition (Stage 1).
- [ ] Rupture event text (Era 0 → Era 1 transition) uses the Stage 5 framing rule.
- [ ] Asset-seizure mechanics carry a sentiment/credit cost consistent with Stage 3.

## Open questions

- Should the Rupture's belligerents and outcome be seeded (varying per campaign) or canonical?
  Seeded is consistent with the rest of the generation stack; canonical is cheaper to write
  quest/tech fiction against.
- Does Kepler get a nuclear-equivalent deterrence ceiling explicitly, or is the Rupture's
  memory alone the ceiling? (Affects Era 1+ conflict design.)
- How much of this ladder surfaces to the player — a codex, generated history lines only, or
  ambient flavour in quest text?
