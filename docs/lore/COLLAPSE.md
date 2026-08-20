# Collapse — polity strategies and culminating events for Era −1

> **Status: research scaffolding — the design conversation's home, not authority.** Written
> 2026-08-20 from Ben's ask: *consider different high-level strategies / metagames for
> surpassing the Era −1 Collapse*, extended same session to *different culminating events —
> the British empire gave away its colonies; there are tonnes of examples of cultures facing
> imminent doom.* Sits on Sprint 30 ("Collapse", the arc's central mechanic —
> `docs/development/sprints.json`), `docs/lore/HISTORY.md` (the institutional ladder this
> plays out inside), and `docs/ai/STRATEGIES.md` (the sibling roster at corp grain — this doc
> reuses its discipline, not its content). Consumers: Sprint 29 (character inheritance —
> NEW-13), Sprint 30 (the strain accumulator and the collapse verb), BL-277 (Era −1 military
> strategy), BL-300 (myth/theology), BL-299 (great-power seed).
>
> **Grain note (Ben, 2026-08-20):** each era spans a long period, so there is little need to
> compress — strategies and culminations are defined at full markdown fidelity here, not as
> a card schema. If the sim later needs a compact encoding, derive it from this doc.
>
> **Naming rule applies in full.** Every historical name below — Rome, Britain, the Meiji
> pivot — is a **mechanism reference for the reader**, never content for the game
> (`.claude/rules/io-standing-rules.md` § Terms). Nothing here enters generation as a proper
> noun; polities and successors are named by the seeded phoneme generator.

---

## The framing ruling

Sprint 30 holds collapse **inevitable for a major** — a deterministic consequence of upstream
scalars, never a die roll. So "surpassing" cannot mean *avoiding*.

The metagame therefore lives in three questions, not one:

1. **When** it breaks — how long a major can hold its peak.
2. **How** it breaks — which culmination (§ Culminating events) resolves the strain.
3. **Who inherits** — which polities are positioned when it does.

Every viable strategy is an answer to one of those. A strategy that answers *whether* is
either the Tortoise (refuse the precondition) or a bug against inevitability.

Two invariants bound the whole space:

- **BL-224 (non-hegemony).** Every strategy must lose eventually; 1960 arrives multipolar.
  The strategies below are ways of losing *well*, at different tempos, leaving different maps.
- **Ruling 4 (nation-behaviour grant, 2026-08-18).** A strategy is a deterministic weighting
  over legal polity verbs — pure, seeded, replayable. Never a planner.

---

## The strategy roster

Six shapes. Each names its verb weighting (over the shipped verbs — Campaign, Settle, Invest,
Consolidate — plus verbs the strategy *demands*), what it needs from Sprint 30's accumulator,
and its natural culmination.

### 1. The Tortoise — never become a major

Stay under the burden knee (`holdings_burden_q`); win by Invest density, not breadth.
Surpasses collapse by refusing its precondition.

- **Weighting:** Invest ≫ Consolidate > Settle; Campaign only defensively-adjacent.
- **Needs:** nothing new — expressible today.
- **Culmination:** none of its own. The Tortoise's ending is written by its neighbours —
  it is prime Absorption prey (§ E3) when a Pulse major peaks next door.
- **Balance role:** the baseline every other strategy is priced against. Must be *viable but
  capped* — if it dominates, no hegemon ever rises and BL-384 (sim conquers nothing)
  recurs by another route.

### 2. The Pulse — conquer, then digest

Alternate Campaign bursts with Consolidate plateaus, paying strain down between expansions.
Dies later and bigger, never differently.

- **Weighting:** phase-alternating — Campaign-heavy while strain is low, Consolidate-heavy
  above a strain threshold. The threshold is the strategy's one tunable.
- **Needs:** the strain **accumulator** (Sprint 30 NEW-6). The current stateless burden
  (`burden = (held − 40) × 4`, recomputed from current holdings) actively punishes pulsing —
  shrinking helps immediately, digestion pays nothing. The roster is thus a spec for the
  accumulator's shape: **digestion must retire strain; loss must not.**
- **Culmination:** Fragmentation (§ E1) at the largest scale the map has seen — the Pulse is
  the strategy that makes the Fall *big*.
- **Reference mechanism:** the punctuated-conquest pattern — expansion waves separated by
  consolidation generations, ending in the classic overextension break (western Rome).

### 3. The Hydra — pre-fragment on your own terms

Shed the periphery deliberately — client polities, planned partition — before strain chooses
the break line for you. The collapse happens, but authored: the core survives with cohesion
intact.

- **Weighting:** Pulse-like rise, then a **Release** verb fired above a strain threshold but
  below the break point.
- **Needs:** a fifth verb (Release / Partition), scored like the others; a client-polity or
  successor-seeding path — the same fragmentation machinery as Sprint 30 NEW-7, triggered
  voluntarily. This is the cheapest second consumer of that code.
- **Culmination:** Devolution (§ E2) — this strategy *is* the British-empire mechanism made
  playable.
- **Design note:** the most interesting shape, because it turns collapse from an ending into
  a **move**. It also gives "inevitable" its honest reading: the strain always resolves; the
  Hydra merely chooses the resolution.

### 4. The Phoenix — position to inherit

A peripheral polity that never contests the major; it accumulates capacity and adjacency,
then absorbs fragments when the neighbour breaks.

- **Weighting:** Invest + Settle toward the major's border; Campaign gated on the
  neighbour's collapse event (an observable, not telepathy — the fragmentation is on the map).
- **Needs:** the eliminated-polity / `owner_none` emission (Sprint 30 NEW-7) as a scoreable
  signal; the tech-ladder regression rule already supports the economics — collapse burns
  *capacity*, never *awareness*, so successors rebuild faster than inventors
  (`docs/research/ANCIENT_TECH_LADDER.md` § diffusion rule 4).
- **Culmination:** it *consumes* culminations rather than having one — until it becomes a
  major itself and re-enters the wheel.
- **Payoff:** the strategy that makes collapse **generative** — the map's next age is
  authored by who played Phoenix. Sprint 29's grudge scalar is largely Phoenix residue.

### 5. The Metropole — hegemony by reach, not holdings

Dominate through trade lanes and dependency rather than owned provinces. Burden counts
holdings, so reach-based power carries no strain — and economic reach *is* military reach
(BL-325, ruling 3), so this is already the design's grain.

- **Weighting:** Invest + road/route verbs (Lane B/D vocabulary) ≫ Campaign; Settle for
  entrepôts, not depth.
- **Needs:** nation-grain trade/tariff verbs (Lane D) before it is expressible at all.
- **The hole (⚠ NR-flagged):** as stated, the Metropole **dodges inevitability** — a
  major-by-influence with 30 holdings never trips a holdings-fed accumulator. Either reach
  feeds a *separate, slower* strain (over-commitment abroad), or the Metropole gets its own
  rupture — which is exactly HISTORY.md Stage 5 (hegemony fails its final audition). Lean:
  the separate slow strain, so the Metropole is the longest-lived shape and still breaks.
- **Culmination:** Devolution (§ E2) or the Slow Fade (§ E6).

### 6. The Temple — raise the ceiling

Spend on cohesion infrastructure — the Era −1 works roster (HISTORY.md § works), and
myth/theology (BL-300) — to move the strain threshold itself. Collapse timing becomes an
institutions race rather than a geometry problem.

- **Weighting:** Consolidate + works-construction ≫ Campaign.
- **Needs:** a coupling only — works and `cohesion_q` both exist; the Temple is the strategy
  that gives BL-300 (myth/theology) a mechanical reason to exist.
- **Culmination:** Transformation (§ E4) — institutions strong enough to outlive the polity
  re-found it rather than fragment it.

---

## Culminating events — how the doom resolves

The refocus (Ben, 2026-08-20): fragmentation is **one** culmination, not the definition.
History offers a family of ways a culture meets imminent doom, and the sim is richer if the
strain accumulator can resolve through more than one exit. Each event below names its
mechanism, its reader-analogy, its trigger shape, and — critically — **what it writes onto
the survivors** (Sprint 29's character payload: posture, creed, grudges).

### E1. Fragmentation — the break

The default, Sprint 30 NEW-7. Strain exceeds cohesion; the polity shatters into successor
polities along province/culture seams; `owner_none` where nothing coheres.

- **Analogy:** the western-Roman break; the warlord interregnum.
- **Trigger:** strain > cohesion with no mitigating verb fired — the *unmanaged* exit.
- **Writes:** many small successors, each carrying a grudge toward whichever neighbour fed
  on the break; a creed lean toward restoration ("we were the centre once").

### E2. Devolution — the managed release

The polity sheds holdings **voluntarily**, converting territory into client relationships and
retained reach. Sovereignty contracts; influence and character persist.

- **Analogy:** the British imperial wind-down — colonies released, a commonwealth of ties
  retained; also the tetrarchic self-partition as the deliberate variant.
- **Trigger:** the Hydra's Release verb — strain high, cohesion still solvent. The *managed*
  exit; strictly better outcomes than E1 for the core, which is what makes Release worth
  scoring.
- **Writes:** one diminished-but-intact core with high reach and a mercantile/diplomatic
  creed; released clients with *low* grudge toward the core — devolution buys goodwill
  fragmentation never does. This asymmetry is the mechanic's whole point.

### E3. Absorption — eaten at the peak of weakness

The strained major is not broken from within but taken from without — a rival's Campaign
lands while cohesion is depleted.

- **Analogy:** the Achaemenid fall to a smaller, sharper rival; late Byzantium.
- **Trigger:** external — an adjacent polity's scored Campaign against a high-strain target.
  Needs strain to be **readable by rivals** (an observable, consistent with no-telepathy: a
  strained empire's weakness shows in lost battles and stalled supply).
- **Writes:** the conqueror inherits holdings *and* imports the victim's strain (conquest of
  a strained body should transfer burden, or absorption becomes a free lunch); the absorbed
  culture persists as a grudge and creed inside the new borders — the conquered capturing
  the conqueror.

### E4. Transformation — the re-founding

Same territory, new polity: a succession crisis resolves by replacing the regime rather than
the map. Continuity of culture, reset of the polity's clock.

- **Analogy:** the dynastic cycle — mandate lost, mandate claimed; the map barely moves.
- **Trigger:** strain > cohesion **and** high works/institutions (the Temple's exit) — the
  institutions survive the polity and re-found it.
- **Writes:** one successor with the ancestor's borders, reset strain, partial capacity
  regression, and a legitimist creed. The cheapest culmination to implement (no map change)
  and the one that keeps region counts stable.

### E5. Exodus — the rump and the migration

The polity abandons its core and survives displaced — a rump state on the periphery, or a
people in motion who re-settle elsewhere.

- **Analogy:** the post-1204 rump that outlived the sack of its own capital; the migrating
  confederations of the late-antique frontier.
- **Trigger:** absorption or fragmentation *with an open frontier* — Settle fired as a
  survival verb rather than a growth verb.
- **Writes:** a small, high-cohesion survivor far from home with a maximal grudge; frontier
  regions gain population and capacity they did not earn. Feeds the map texture Sprint 29
  wants — capitals in strange places, with reasons.

### E6. The Slow Fade — senescence

No rupture at all: the major decays below relevance while borders recede piecemeal. The doom
is real but arrives as erosion, over a very long period — which the era's timescale (Ben's
grain note) makes legible rather than boring.

- **Analogy:** the long "sick man" recession; the thousand-year mercantile republic ending
  with a whimper.
- **Trigger:** strain chronically *near* cohesion, never over it — every mitigating verb
  fired, none sufficient. The Metropole's default ending.
- **Writes:** a minor with a proud creed, high grudges, disproportionate reach — the sim's
  best source of "interesting small nations" at 0 CE.

### E7. Systemic collapse — the contagion

Multiple polities break together: interconnection turns one culmination into a cascade, as
each break severs the trade and tribute its neighbours' cohesion rested on.

- **Analogy:** the Bronze-Age general collapse — a whole state system failing in one
  archaeological breath.
- **Trigger:** an E1/E3 event that removes routes/works other polities' cohesion terms read.
  Needs cohesion to have an **interdependence input** — a real design decision, not free.
- **Writes:** a dark-age band — broad `owner_none`, deep capacity regression, awareness
  intact. The most expensive culmination and the most dramatic; candidate for a *rare*
  outcome band, asserted by sweep, never scripted.
- **Scope caution:** this is the one event that couples every polity's state; hold it until
  E1–E4 are landed and measured.

---

## Telling the story — the ideological axis

**The dual-focus rule (Ben, 2026-08-20):** when these stories are told, focus dually on the
**material** necessities and warfare, *and* on **the way real cultures told their own story** —
the ideologies preserved in real history — and how those map onto our fictional parallel
worlds. A collapse the player only sees as border changes is half a collapse.

The naming rule already gives the transfer principle: the **mechanism** crosses, the **noun**
never does. That holds for ideology exactly as for institutions — *how a culture narrates its
doom* is a mechanism; the specific gods and dynasties are nouns. The seeded template banks
that name polities should also mint their **self-stories**, from the pattern library below.

### The pattern library — how real cultures narrated the doom

Each pattern names the real narrative move, then its fictional-world seat (what generation
writes it onto, what the sim reads back from it).

- **The mandate withdrawn.** Legitimacy is a grant from heaven/the cosmos; disaster is
  evidence the grant has moved. Makes regime change *thinkable without cultural death* —
  the ideological substrate of Transformation (E4). *Seat:* a creed axis
  (mandate-holds ↔ mandate-forfeit); high works + this creed biases the break toward E4.
- **Translation of empire.** The centre is not destroyed, it *moves* — successors claim to
  BE the continuation, not the replacement. Every E1 fragment claiming the whole. *Seat:*
  successor creeds after Fragmentation; the restoration grudge ("we are the true heir")
  aimed at sibling successors, not just predators.
- **The declinist mirror.** The culture narrates its own fall in advance — moral corruption,
  lost virtue of the ancestors — often for generations before any material break. *Seat:*
  a narrated-history line that *precedes* the culmination (Sprint 31): strain past a band
  emits "their own chroniclers wrote of decay" years before the break. The player should be
  able to read the doom coming the way the culture's own writers did.
- **The apocalypse reframed as test.** Imminent doom read as trial or purification — the
  millenarian response. Fuels last stands and Exodus (E5): a people who expect the end can
  march *through* it. *Seat:* a creed that raises cohesion under extreme strain (a
  deathbed rally term) at the price of never choosing Release — the anti-Hydra.
- **The garden given away.** Devolution (E2) needs its own story or it reads as defeat: the
  real mechanism is recasting release as *maturity* — empire retold as stewardship completed,
  ties of kinship replacing ties of rule. *Seat:* the low-grudge asymmetry E2 already
  writes, now with its narrative cause; the core's creed shifts imperial → mercantile.
- **The golden age behind us.** Post-collapse cultures curate an idealised memory of the
  peak — the material regression is real, the *story* of greatness is preserved losslessly.
  This is the ideological twin of the tech ladder's regression rule: capacity burns,
  **awareness never** — and neither does the myth. *Seat:* the restoration creed of E1/E5
  survivors; the Phoenix's legitimising claim when it gathers fragments.
- **The chosen remnant.** Exodus (E5) survivors narrate displacement as election — smallness
  as proof of purity. *Seat:* the rump polity's high-cohesion term IS this story; creed
  locks legitimist + maximal grudge, and the myth outlives any realistic hope of return.

### Why the sim should read the story back

These are not flavour. Two mechanical returns:

1. **Ideology biases the culmination.** The state vector that determines the exit
   (§ Open questions, Q2) should include the creed: mandate-creeds fall toward E4,
   remnant/test-creeds toward E5, stewardship-creeds toward E2. Same strain, different
   story, different ending — which is precisely how the real cases diverged. Deterministic
   throughout: creed is seeded data, the bias a scored term.
2. **The story is the Sprint 29 payload.** Posture, creed and grudge are exactly "the way
   the culture tells its story" carried to 0 CE. The pattern library gives the phoneme/
   template banks a second vocabulary to mint from — so a generated nation doesn't just
   have a name, it has an account of itself, traceable to a culmination the player can
   find in the history log.

BL-300 (myth/theology) is the natural home for the authored bank; Sprint 31 (watch it fall)
is where the narration surfaces. The discipline stays HISTORY.md's: **driven, not
narrated** — the story is emitted by what happened, never scripted over it.

## The matrix — strategy × culmination

| strategy | natural culmination | failure culmination |
|---|---|---|
| Tortoise | (none — outlives the wheel) | E3 Absorption |
| Pulse | E1 Fragmentation | E3 Absorption at peak strain |
| Hydra | E2 Devolution | E1 if Release fires too late |
| Phoenix | consumes others'; later re-enters as Pulse/Hydra | E5 Exodus if it moves too soon |
| Metropole | E6 Slow Fade | E7 Systemic (its routes are the contagion medium) |
| Temple | E4 Transformation | E1 if works lag expansion |

The matrix is the harness spec: across N seeds, each strategy should be observably distinct
in lifespan, fragment count, and successor share — and each culmination should occur at a
reported (never clamped) rate. Two strategies with indistinguishable histories mean one is
dead weight; a culmination that never fires is scope to cut.

## What the sim must grow (consolidated)

1. **The strain accumulator** (Sprint 30 NEW-6) — stateful; digestion retires strain, loss
   does not. Already asserted by the arc's adversarial pass.
2. **The Release verb** — voluntary fragmentation; second trigger on NEW-7's machinery.
3. **Strain readability** — rivals score against observable weakness (E3), no telepathy.
4. **Strain transfer on conquest** — absorption is never free (E3).
5. **A reach-fed slow strain** — closes the Metropole inevitability hole (⚠ § 5).
6. **A transformation path** — regime reset without map change (E4); cheapest first landing
   after E1.
7. **(Deferred)** cohesion interdependence — E7's substrate; explicitly later.

## Open questions

1. Does Release score as a polity verb, or fire as a threshold rule? Verb keeps it inside
   ruling 4's shape; lean verb.
2. Is the culmination *chosen* (highest-scoring exit at break time) or *determined* (by the
   state vector at break)? Lean determined — "inevitable" reads cleaner as consequence than
   as choice, and it keeps the exit deterministic by construction.
3. How much of this vocabulary survives into campaign-era nations? Ruling 4 grants both
   grains; the strategies are polity-agnostic, but the culminations assume the sim's
   verb set.
4. Do culminations E2/E4 need their own narrated line in the history log (Sprint 31, watch
   it fall)? Almost certainly — a devolution that plays back as silence looks like a bug.
