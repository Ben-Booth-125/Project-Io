# Collapse Roster — polity strategies and culminating events (mechanism reference)

> **Proposes:** six shapes a major polity might play against its own doom · seven ways a doom
> might resolve, and how strategy and culmination pair · the machinery such a roster would need.
> **Not here:** how an empire actually falls in the Era −1 sim — network failure, secession as
> city states, what a successor inherits (../generation/CIVILISATION, the authority) · how a
> realm breaks along faith (../lore/CREEDS § The schism verb) · how a fall is narrated and why
> the sim reads that story back (../generation/CIVILISATION § How a fall is told) · what makes a
> long run affordable (../generation/CIVILISATION § The long run is paid for by the table) · the
> stage ladder the sim plays inside (../lore/HISTORY) · the sibling roster at corp grain
> (../ai/STRATEGIES, also research).
> **Confused with:** generation/CIVILISATION.md, lore/HISTORY.md, ai/STRATEGIES.md.

> **Research scaffolding — a mechanism reference, not authority (Ben, 2026-09-15).** This roster
> was drawn in a design session on 2026-08-20 as the Era −1 collapse metagame: a strain
> accumulator, six polity strategies played against it, seven culminating events, and a matrix
> pairing the two. **The design it described was not the one built.** The collapse the generation
> layer models is **network failure** — a realm fragments when a specific region's supply falls
> under a floor, and the cut-off ground secedes as city states — with the **schism verb** as the
> one other break, along faith rather than distance. Those two are owned by
> `../generation/CIVILISATION.md` § How an empire actually falls and `../lore/CREEDS.md` § The
> schism verb, and no code reads a strategy weighting, a strain accumulator or a culmination from
> this page. What the built model still needed from here — the ideological axis, the affordability
> rungs, contagion as a property of the network — was folded into `CIVILISATION.md` (the fold is
> `BL-1001 (COLLAPSE doc folded and retired)`). What remains below is kept as **history's
> mechanism catalogue**: read it for how a devolution, an absorption or a slow fade works as a
> mechanism, never as a claim about the game. The backlog items the machinery table names were
> purged with the roster; their ids are provenance only.

**Naming rule applies in full.** Every historical name below — Rome, Britain, the tetrarchy — is
a **mechanism reference for the reader**, never content for the game
(`.claude/rules/io-standing-rules.md` § Terms & docs). Nothing here enters generation as a proper
noun; polities and successors are named by the seeded phoneme generator.

*Provenance:* written from Ben's ask (2026-08-20) — *consider different high-level strategies /
metagames for surpassing the Era −1 Collapse*, extended the same session to *different
culminating events — the British empire gave away its colonies; there are tonnes of examples of
cultures facing imminent doom.* Grain note from the same session: each era spans a long period,
so strategies and culminations were defined at full markdown fidelity rather than as a card
schema.

---

## Rulings — design session (2026-08-20)

Ben's answers to the shaping questions, recorded first because the sections below read
differently in their light.

- **Four allegories, not two or three.** The pre-game generation hones in on all four
  mechanism-references: the **Rome arc** (rise → overextension → fragmentation; Pulse/E1),
  **British devolution** (managed release; Hydra/E2), the **dynastic cycle** (mandate reset;
  Temple/E4), and the **Bronze-Age systemic collapse** (the cascade; E7). E7 follows E1–E4 as
  *sequencing*, not scope — it is in.
- **Tuned attractors, not templates.** One parameter space; the allegories are the outcomes
  the sweep asserts occur at stated, reported rates. Nothing seeds an arc directly; fully
  driven-not-narrated. This makes the § matrix the literal tuning target.
- **All four play out sequentially per world, over 4000 years.** The run is long enough to
  chain arcs — a fragmentation whose Phoenix later devolves; a systemic cascade as one epoch
  among them. **Ruled 2026-08-20 (resolves NR-357):** a 400-year band is a placeholder; the
  run is the 4000-year ladder, and generating over it is acknowledged as a hard problem — the
  optimisation task is considered **here** (§ The 4000-year problem), owned by BL-494
  (four-thousand-year ladder) with BL-320 (Era −1 sim perf).
- **Reach feeds strain (resolves NR-356).** *"Logistics should be capable of determining
  reach. By that I mean reach feeds strain."* The Metropole's inevitability hole (§ 5) is
  closed by option A — a second, slower inflow on the same accumulator, reading the same
  logistics/reach substrate per BL-325 ruling 3 (one reach field). Filed as **BL-510**
  (reach-fed slow strain).
- **The story surfaces in the history tab, quietly.** Telling the story is secondary to
  *seeing it in the game*; don't overload the player with easy-access information on
  everything the game does. The focus is playing — deep-dig is optional, for the players who
  want it. So: creed data and culmination bias land as sim substance; narration is a
  history-tab layer (and the history playback), never chrome pushed at the player.

## The framing ruling

Collapse is **inevitable for a major** — a deterministic consequence of upstream
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

Six shapes. Each names its verb weighting (over the sim's verbs — Campaign, Settle, Invest,
Consolidate, Build Work — plus verbs the strategy *demands*), what it needs from the strain
accumulator, and its natural culmination. The weightings themselves are authored data — BL-488
(polity strategy weightings).

### 1. The Tortoise — never become a major

Stay under the burden knee (`holdings_burden_q`); win by Invest density, not breadth.
Surpasses collapse by refusing its precondition.

- **Weighting:** Invest ≫ Consolidate > Settle; Campaign only defensively-adjacent.
- **Needs:** nothing beyond the four base verbs.
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
- **Needs:** the strain **accumulator** (BL-504). A stateless burden recomputed from current
  holdings (`holdings_burden_q` over holdings past `free_holdings`) actively punishes pulsing —
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
  successor-seeding path — the same fragmentation machinery as E1 (BL-505), triggered
  voluntarily. This is the cheapest second consumer of that code. Owned by BL-507 (Release verb).
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
- **Needs:** the eliminated-polity / `owner_none` emission (BL-505) as a scoreable
  signal; the tech-ladder regression rule already supports the economics — collapse burns
  *capacity*, never *awareness*, so successors rebuild faster than inventors
  (`ANCIENT_TECH_LADDER.md` § The diffusion axis).
- **Culmination:** it *consumes* culminations rather than having one — until it becomes a
  major itself and re-enters the wheel.
- **Payoff:** the strategy that makes collapse **generative** — the map's next age is
  authored by who played Phoenix. The campaign nations' grudge scalar is largely Phoenix residue.

### 5. The Metropole — hegemony by reach, not holdings

Dominate through trade lanes and dependency rather than owned provinces. Burden counts
holdings, so reach-based power carries no strain — and economic reach *is* military reach
(BL-325, ruling 3), so this is already the design's grain.

- **Weighting:** Invest + road/route verbs ≫ Campaign; Settle for
  entrepôts, not depth.
- **Needs:** nation-grain trade/tariff verbs before it is expressible at all.
- **Inevitability (NR-356, Ben, 2026-08-20):** a major-by-influence with 30 holdings never
  trips a holdings-fed accumulator, so a holdings-only strain would let the Metropole dodge
  the wheel. It does not: reach feeds a *separate, slower* strain inflow on the same
  accumulator (over-commitment abroad), read from the logistics network itself per BL-325
  ruling 3 — so the Metropole is the longest-lived shape and still breaks. BL-510 (reach-fed
  slow strain); `../lore/HISTORY.md`'s Stage 5 is a parameterisation of it, not a second system.
- **Culmination:** Devolution (§ E2) or the Slow Fade (§ E6).

### 6. The Temple — raise the ceiling

Spend on cohesion infrastructure — the Era −1 works roster (`../lore/HISTORY.md` § The works roster), and
myth/theology (BL-300) — to move the strain threshold itself. Collapse timing becomes an
institutions race rather than a geometry problem.

- **Weighting:** Consolidate + works-construction ≫ Campaign.
- **Needs:** a coupling only between works and `cohesion_q`; the Temple is the strategy
  that gives BL-300 (myth/theology) a mechanical reason to exist.
- **Culmination:** Transformation (§ E4) — institutions strong enough to outlive the polity
  re-found it rather than fragment it.

---

## Culminating events — how the doom resolves

The refocus (Ben, 2026-08-20): fragmentation is **one** culmination, not the definition.
History offers a family of ways a culture meets imminent doom, and the sim is richer if the
strain accumulator can resolve through more than one exit. Each event below names its
mechanism, its reader-analogy, its trigger shape, and — critically — **what it writes onto
the survivors** (the character payload the campaign nations inherit: posture, creed, grudges).

### E1. Fragmentation — the break

The default (BL-505, fragmentation verb). Strain exceeds cohesion; the polity shatters into successor
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
  regions gain population and capacity they did not earn. Feeds the map texture the campaign
  wants — capitals in strange places, with reasons. BL-484 (Exodus).

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
- **Sequencing:** this is the one event that couples every polity's state; it follows E1–E4,
  as sequencing rather than scope (§ Rulings). BL-486 (cohesion interdependence).

---

## Telling the story — the ideological axis

Folded into `../generation/CIVILISATION.md` § How a fall is told — the ideological axis: the
dual-focus rule, the pattern library reseated on the exits the design actually has (secession,
schism, conquest), and why the sim reads the story back. The one pattern with no seat there —
*the garden given away*, which needs a voluntary release — belongs to the Hydra and Devolution
rows above.

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

## The machinery, and who owns each part

| Mechanism | What it is | Owner |
|---|---|---|
| **The strain accumulator** | stateful; digestion retires strain, loss does not | BL-504 (strain accumulator) |
| **The fragmentation verb** | E1 — successors along province/culture seams, `owner_none` where nothing coheres | BL-505 (fragmentation verb) |
| **The hegemony measure** | the in-engine read BL-224's invariant is asserted against | BL-506 (hegemony measure) |
| **The Release verb** | voluntary fragmentation; a second trigger on E1's machinery | BL-507 (Release verb) |
| **Strain readability** | rivals score against observable weakness (E3), no telepathy | BL-508 (readable strain) |
| **Strain transfer on conquest** | absorption is never free (E3) | BL-509 (strain transfer) |
| **A reach-fed slow strain** | the Metropole's inevitability (§ 5) | BL-510 (reach-fed slow strain) |
| **The transformation path** | regime reset without map change (E4); the cheapest culmination after E1 | BL-483 (Transformation) |
| **Exodus, Slow Fade** | E5, E6 | BL-484, BL-485 |
| **Cohesion interdependence** | E7's substrate — a real design decision, not free | BL-486 (cohesion interdependence) |
| **Creed axes, weightings, narration** | the ideological axis, the strategy table, the story bank | BL-487, BL-488, BL-489 |
| **The attractor sweep** | the § matrix as a harness | BL-490 (attractor sweep) |

## The 4000-year problem — making the run affordable

Folded into `../generation/CIVILISATION.md` § The long run is paid for by the table, not the
fighting: cost tracks the region table, the five rungs in the order they pay, what not to trade
away, and the fit check.

## Open questions

1. Does Release score as a polity verb, or fire as a threshold rule? Verb keeps it inside
   ruling 4's shape; lean verb.
2. Is the culmination *chosen* (highest-scoring exit at break time) or *determined* (by the
   state vector at break)? Lean determined — "inevitable" reads cleaner as consequence than
   as choice, and it keeps the exit deterministic by construction.
3. How much of this vocabulary survives into campaign-era nations? Ruling 4 grants both
   grains; the strategies are polity-agnostic, but the culminations assume the sim's
   verb set.
4. Do culminations E2/E4 need their own narrated line in the history log? Almost certainly — a devolution that plays back as silence looks like a bug.
