# Strategies — the meta, authored ahead of the game

> **Status: research scaffolding — the design conversation's home, not authority.** Written
> 2026-08-06 from Ben's ask: *develop strategies and meta ahead of the game, to make it easier
> for AI to play properly — we will have to compress strategy to a local machine.* Sits on
> `AI_OPPONENT.md` § 10 (the small-local-model direction), `docs/research/ERA1_TECH_LANDSCAPE.md`
> (the Era 1 tree draft and its danger model) and `docs/research/TECH_EFFECTS.md` (the effect
> vocabulary). Siblings: **BL-279** (trace corpus — consumes the strategy labels), **BL-207**
> (persona packs — become weightings over this roster), **BL-210** (oral-history pivot — the
> loop where new strategies get minted). A backlog item for the library itself is minted when
> this lands. Nothing is transcribed into a data store yet, deliberately — the roster review
> shouldn't review something that already looks settled (the Era 1 tree draft's own rule).
>
> **Resolved 2026-08-06 (§ Resolutions, below):** the deck is **AI-invented**, the schema is
> authored; cards ride in context at runtime; no telepathy; meta is judged by the existing
> reward schema. Where earlier prose reads as an authored roster, the resolution wins.
>
> **Honesty markers:** every card is tagged by the vocabulary it runs on — **[shipped]**
> (today's engine), **[draft-tree]** (references the Era 1 first draft, which Ben has not yet
> reviewed). A **⚠** marks a signal the blackboard cannot yet express (§ The design-gap yield).

---

## Resolutions — design session (2026-08-06)

Ben's answers to the draft's open questions, recorded first because everything below reads
differently in their light.

- **The deck is invented; the schema is authored.** *"I prefer a deck of cards which the AI
  invents."* Authored content shrinks to the card contract (schema, vocabulary discipline,
  token ceiling, lint) plus seed hypotheses; the shipped deck is minted by the invention loop
  (§ below). The rationale is player-facing: *"if we use cards, then they have to be in the
  tutorial"* — an authored card library would be canonical game content the tutorial owes the
  player. An invented, private deck keeps meta *discovered*, symmetrically, by AI and player.
- **No telepathy.** *"No additional information for the AI."* A card may cite only observables
  the player also has — blackboard facts, node stat-lines, tells that precede commitment. Trap
  **verdicts** are never authored into cards; the loop must earn them by falling in. The AI may
  learn what it cannot be told. Seed lines violating this are marked **⊘** below.
- **Meta is judged by the existing reward schema.** *"It should be obvious when meta works via
  the existing reward schema."* A card earns and keeps its deck slot only by same-seed
  superiority on the game's own scoreboard — net-worth trajectory, era entry, rupture survival —
  never by a bespoke success metric. Signature metrics demote to *compliance labels*.
- **Runtime shape: cards in context**, as recommended — the fine-tune learns to follow cards,
  so a deck patch is a data update, not a retrain.
- **first_footing stays: race the player.** Side-effect for the tree review: a race needs a
  single trophy, so this leans tree Q2 toward **world-scoped** keystone deeds.
  **Superseded 2026-08-06 (NR-069/NR-070):** tree Q2 actually settled **personal**, killing the
  single-trophy premise. ST-10 was reworked into a tempo race (fire your own deed before a
  rival fires theirs) rather than cut — see its card below.
- **The deterministic layer stays unbound.** The `weights` line remains in the schema but does
  not compile into `corp_ai` until the invention loop has validated the deck.

## The thesis — strategy is data, not weights

The § 10c research gives one instruction for state: *never ask a small model to carry world
state in its head — hand it the state, every time.* This doc applies the same instruction one
layer up: **never ask a small model to derive strategy — hand it the strategy, every time.**

A 120B open-weight model reached parity with a tuned algorithmic 4X AI because strategy is
latent in weights that size. A local 3–8B model has no such latent — and the documented failure
modes (myopia, the knowing-doing gap) are precisely what "derive your own strategy" looks like
when it fails. The compression move is to take strategy **out of the weights and put it in the
context**: a closed library of authored strategy cards, exactly as the blackboard took world
state out of the model's head.

That makes the word interface **four legs**, not three:

| leg | artifact | answers |
|---|---|---|
| read | blackboard export (BL-206) | what is |
| meaning | action dictionary (BL-270) | what can be done |
| **intent** | **the strategy library (this doc)** | **what to want** |
| write | corp-command seam | do it |

What this doc authors is the **container and the loop** — the deck's *contents* are minted by
play (§ Resolutions), the way a player's own meta is.

## One artifact, four consumers

1. **The runtime model.** The index rides in every decision prompt; the *active* card rides in
   full. The model executes a named plan instead of improvising one — goal persistence as data,
   the § 10c.5 fix ranked cheapest-but-one.
2. **The trace corpus (BL-279).** Every logged decision carries its strategy id. That factorises
   the learning problem: `select(strategy | state-digest)` is a small classification;
   `execute(action | state, card)` is generation at far lower entropy than
   `action | state` with strategy latent. The factorisation **is** the compression to a local
   machine — the fine-tune learns to *follow* cards, not to contain them, so a meta patch is a
   data update, not a retrain.
3. **The deterministic layer.** `corp_strategy` (BL-203) is today a three-value alias of
   `industrial_focus`. Each card carries a `weights` line that compiles to focus/bucket biases —
   the library *extends* the shipped enum rather than rivalling it, and the algorithmic corps
   could run the same deck without a model in the loop. **Deferred** (§ Resolutions): the
   compilation stays uncoupled until the invention loop has validated the deck.
4. **The eval harness.** Each card names trace-checkable *compliance* signatures
   (§ The invention loop), so "is the AI actually playing its card" is a mechanical question —
   the same move `corp_command_result`'s typed rejections make for legality. Success is never
   judged here; that is the reward gate's job.

## Compression disciplines

The library only compresses if these hold; they are rules, not aspirations.

1. **Closed and small.** A capped deck, ~10 live cards. The runtime model never invents a card
   mid-campaign; minting happens *offline*, in the invention loop — cloud post-mortems and the
   Project-Rival oral-history discipline (BL-210) — and ships as pack updates. The meta evolves
   between releases, not mid-campaign.
2. **Two-tier context.** A `STRATEGY_INDEX` (one line per card, ~20 tokens each) always in
   context; the full card fetched by id — the `ACTIONS_INDEX.json` / `actions_query.js` pattern,
   reused verbatim. Standing cost: index + one active card ≈ 600 tokens, against the ~20M-token
   games the Vox numbers report. Negligible by construction.
3. **Vocabulary discipline.** `when` / `watch` / `abandon` are written in **blackboard
   predicates** (`cash`, `price`, `supply`, `demand`, `tile_deposit`, `rival_building_type`,
   `route`, `body_activity`, `survey`, …); openings in **action-dictionary ids**; tech paths in
   **tree node names**. No free prose the model must ground itself — and the lint can check
   every referenced symbol against its store.
4. **Predicates are mechanical.** Because of (3), the harness — not the model — evaluates
   `when` and `abandon` each evaluation window and *pushes* the results ("abandon-2 fired: your
   export routes went quiet"). Everything predicate-shaped moves into the harness; the model
   keeps only judgment. The utility-AI philosophy, extended up one layer.
5. **Token ceiling per card.** ≤ 400 tokens, lint-enforced. A card that needs more is two cards
   or a worse card.

## The card contract

```
card:  id          ST-nn
       name        snake_case handle
       family      economy | space | survival | race   (military: reserved)
       thesis      one sentence — why this wins
       when        preconditions, blackboard predicates (mechanical)
       opening     the first moves, action-dictionary ids / tree nodes, ordered
       doctrine    which keystone branches it wants, and the map-reading that decides
       posture     build / market / survey / alarm stance
       watch       counter-signals — rival visible state + own tells
       abandon     mechanical predicates; firing is pushed to the model, not discovered by it
       pivot       named fallback cards — a strategy ends into another strategy, never into nothing
       wins_by     the end-state it drives toward
       weights     the corp_strategy / focus-weight compilation for the deterministic layer
```

**The persistence protocol.** The active strategy lives in a harness-side `strategy_state` —
`{active_id, adopted_tick, phase, fired_flags}` — never in the model's head. At the staggered
cadence the deterministic layer already uses, the harness re-presents the card with its
evaluated flags and asks one bounded question: **hold, pivot to a named fallback, or reselect
from the index.** That is SAGA's dual-horizon loop and Richelieu's re-evaluation as *protocol*,
with the model unable to forget its goal because the goal is pushed to it every window.

---

## The seed deck — ten hypotheses

Under the 2026-08-06 resolution these are **seeds, not a roster**: starting hypotheses for the
invention loop and worked demonstrations of the schema. Each must beat the reward gate to keep
its slot; the loop may rewrite or kill any of them. Lines marked **⊘** carry an authored trap
verdict the no-telepathy rule bars from shipping — the loop must rediscover them or they die
with the seed.

### Era 0 — the economic base *(all three run on shipped vocabulary)*

```
ST-01 · deep_seam — economy · [shipped]
thesis:  own the richest ground before rivals know it is rich; sell raw, bank the rent.
when:    surveyable regions remain (survey) · cash clears survey cost + reserve floor
opening: gameplay.survey best-affinity region → gameplay.build extractors on top tile_deposit
         tiles → gameplay.place_sell_order at the nearest market
posture: capex-lean, survey-led, alarm-neutral ⚠
watch:   rival_building_type extractor beside an owned seam · price of the seam resource sliding
abandon: core tile_deposits depleting · price under margin for N consecutive windows
pivot:   mill_gate (refine your own ore) · long_haul (sell reach, not rock)
wins_by: resource rent — the cash engine an Era 1 arc is paid from
weights: corp_strategy=extraction; survey_expand ↑ · extractor builds ↑
```

```
ST-02 · mill_gate — economy · [shipped]
thesis:  the margin lives between raw and refined; stand at the gate and take it.
when:    price spread raw→refined clears input + wage cost · a supplied hub exists (supply)
opening: gameplay.build processors at the hub → gameplay.set_recipe to the widest live margin
         → gameplay.set_workforce_auto on
posture: mid-capex, market-led; input security is the known weakness
watch:   input supply thinning · rival processors at the same hub · the brittle-optimisation
         tell — an optimised input you import
abandon: spread inverted for N windows · the input market vanishes (supply/demand facts gone)
pivot:   long_haul (haul your input) · deep_seam (own your input)
wins_by: margin capture at scale; the steadiest income curve in the roster
weights: corp_strategy=processing; recipe dials ↑ · hub builds ↑
```

```
ST-03 · long_haul — economy · [shipped]
thesis:  the map is a price surface; carry goods across its gradients, and the fog lifts
         as a side effect.
when:    cross-market price gaps exceed haul cost · cash clears road capex floor
opening: gameplay.dispatch_convoy across the widest gap → gameplay.place_road on the proven
         trunk → repeat; body_activity lights as routes persist
posture: asset-thin, information-rich — every route is also intelligence
watch:   the single-corridor herring: one cheap chokepoint carrying everything · rival routes
         shadowing yours
abandon: spreads collapse below haul cost network-wide
pivot:   mill_gate (process at the cheap end) · first_footing (reach becomes a race asset)
wins_by: arbitrage + the widest activity-fog picture in the game — it sees everyone
weights: corp_strategy=trade; convoy dispatch ↑ · road builds ↑
```

### The Era 1 arc *(all [draft-tree] — written against the unreviewed first draft)*

```
ST-04 · propellant_first — space · [draft-tree · reworked 2026-08-06 per NR-071]
thesis:  the space economy has no customer until propellant is made off-world; fire your OWN
         First Tank before rivals fire theirs, and be the demand sink everyone else sells into.
when:    Era 1 quests open ⚠ · launch capacity owned · a volatile-bearing body surveyed
         (tile_deposit ice bands ⚠)
opening: Volatile Prospecting → Ice Extraction → Electrolysis → fire The First Tank ⚠, paced
         against the visible maturity of rivals' volatiles programmes
doctrine: read the substrate, not the brochure — water-ice body → Hydrolox; carbon source →
         Methalox. The contextual-dud herring is exactly this choice made off-map.
posture: mid-capex; deliberately builds the demand sink before any supply play
watch:   rival volatiles buildings on the same body (rival_building_type) — a pacing clock, not
         a race clock; a rival's own Tank does not close your fork (deeds are personal, NR-069)
abandon: a rival's mature volatiles programme makes your own Tank pointlessly late — the
         early-mover window has closed, not the fork itself
pivot:   yard_master (sell structure instead of fuel) · cadence (lift what you cannot make)
wins_by: the propellant loop — every other space card's abandon test is "is there a buyer";
         this card IS the buyer, and firing first means being the *cheapest* buyer rivals sell into
weights: volatiles-sector builds ↑ · ice-band survey ↑
```

```
ST-05 · cadence — space · [draft-tree]
thesis:  cost-to-orbit is the master variable and it falls with flight rate; fly often,
         sell lift, let the curve do the work.
when:    launch capacity owned · lift demand visible ⚠ (no space-market predicate yet)
opening: repeat dispatch to orbit → fire Ten Flights ⚠ → Reusable Chemical branch →
         Recovery & Refurbishment → Reusable Booster
posture: tempo-maximal, alarm-heavy ⚠ — launches frighten nations; pair with Open Launch
         Inspection and accept its tempo cost
watch:   own alarm contribution ⚠ · the rupture countdown ⚠ — this is the card most likely
         to end the world
abandon: alarm nearing ceiling with the countdown close → throttle or pivot · the propellant
         loop fails to appear and lift demand stays absent
pivot:   quiet_foothold (same assets, opposite posture) · bedrock (if site + runway appear)
wins_by: cheapest marginal kg to orbit before anyone else; lift-as-a-service to every card
weights: launchpad builds ↑ · refit dials ↑
```

```
ST-06 · bedrock — space · [draft-tree]
thesis:  pay once, lift forever — fixed infrastructure beats cadence past the knee, if the
         world lets you finish it.
when:    deep cash reserves · a qualifying site surveyed · alarm low and stable ⚠ · runway
         longer than the build (the asteroid-mining bust was a runway failure; this is the
         bank bet)
opening: accumulate → Fixed Infrastructure branch at the Lift Doctrine fork → Mass Driver
posture: capital-maximal, tempo-minimal — the tempo-trap herring aimed at yourself, on purpose
watch:   the countdown ⚠ — bedrock's one killer is the rupture arriving mid-build; committed
         spend vs remaining runway, every window
abandon: projected completion crosses the seeded date · cash floor breached mid-build — the
         sunk-cost exit is authored in, not left to judgment
pivot:   cadence (cut losses, fly chemical) — an expensive, honest retreat
wins_by: near-zero marginal lift; whoever holds the driver prices everyone else's Era 1
weights: single-site capex ↑ · everything else ↓
```

```
ST-07 · quiet_foothold — survival · [draft-tree]
thesis:  more advanced is not better; arrive in orbit slightly late, into a world that still
         exists, ahead of rivals who spent their era on alarm.
when:    aggregate alarm trending toward ceiling ⚠ · rivals visibly escalating
         (rival_building_type dual-use classes)
opening: keep every cross-border route live (interdependence is the cheapest alarm suppressant)
         → Civil Telemetry Network → Open Launch Inspection → modest cadence inside the
         inspected envelope
posture: alarm-minimal by construction; runs mill_gate-lean economics underneath
watch:   severed routes anywhere, yours or rivals' · rivals taking escalator nodes · your own
         import lines ⊘ (— never take Autarkic Substitution: authored trap verdict, barred
         from shipping; the loop must earn it)
abandon: the rupture passes averted → convert to cadence or yard_master with the ceiling slack
pivot:   cadence (post-rupture) · yard_master
wins_by: the rupture check itself — the one card whose win condition is the world's survival,
         positioned first in the era that follows
weights: trade routes ↑ · mitigation nodes ↑ · escalator nodes vetoed
```

> **Why ST-07 justifies the whole library:** a step-wise greedy policy can never find it —
> every local move scores below the escalating alternative, and the payoff arrives only at the
> seeded date. It exists because someone wrote it down after a world burned — authored today as
> a seed, re-derivable by the loop from any lost campaign's post-mortem. It is the § 10c myopia
> finding answered in one card, and the invention loop's argument in miniature.

```
ST-08 · yard_master — space · [draft-tree]
thesis:  whoever owns the yard taxes the whole space economy; structure is the second demand
         sink after fuel.
when:    lift available, owned or bought · the Yards sector open ⚠
opening: Orbital Rendezvous → Modular Assembly → fire The First Truss ⚠ → Orbital Port;
         the port joins the trade graph
doctrine: Orbital Assembly beside a cadence lifter; Surface Assembly beside on-body extraction
posture: mid-capex, alliance-shaped — this card wants a partner card in the world
watch:   rival orbital classes (rival_building_type ⚠ designed-only) · lift price rising
         against you
abandon: no lift partner and no owned lift — the yard starves
pivot:   propellant_first (fuel is the other sink) · iron_belt inverted (feed your own yard)
wins_by: throughput fees; the port is the market centre of Era 1
weights: yards-sector builds ↑ · orbital-site access ↑
```

```
ST-09 · iron_belt — space · [draft-tree]
thesis:  off-world metal is worthless on Earth and priceless at a yard; mine for the yard
         that exists, never for the market you imagine.
when:    a VISIBLE space-side buyer — an operating yard or port with demand ⚠ (no space-market
         predicate exists; this card is currently unwritable, which is a finding)
opening: Regolith Excavation → Metallic Body Working → feed the yard's input chain
posture: the disciplined version of the oversupply trap — Platinum-Group Separation only for
         in-space use; returning PGM Earth-side crashes its own price
watch:   yard demand thinning · lift cost rising (your ore rides someone else's rockets)
abandon: STRUCTURAL — the buyer disappears → out, immediately. This line is the
         Planetary-Resources lesson written as data.
pivot:   yard_master (become your own buyer) · deep_seam (go home; rock is rock)
wins_by: monopoly input position on Era 1's construction economy
weights: space-extraction builds ↑, gated hard on visible demand
```

```
ST-10 · first_footing — tempo race · [draft-tree · reworked 2026-08-06 per NR-070]
thesis:  firsts are tempo, not territory. Deeds are personal (NR-069) — there is no shared
         trophy — but firing your keystone deed before a rival fires theirs still buys an
         early-mover lead on your chosen fork's economy, ahead of theirs coming online.
when:    rival programme maturity is visible (BL-068) · a deed is contestable at low cost
opening: minimal viable probes — the smallest landing, tank, truss that fires your OWN deed,
         paced against the visible maturity of rivals' programmes rather than contesting them
posture: asset-light, tempo-pure, benchmarked openly against rivals' visible programmes
         (rival_building_type as a pacing clock, not a race clock)
watch:   rival programme maturity per keystone (relative pace) · own burn on probes with no
         economy behind them
abandon: two consecutive keystones where a faster opening would not have changed the payoff ·
         programme costs outrun the early-mover gain
pivot:   long_haul (the probe network is also a route network) · whichever fork you land on
wins_by: doctrine-timing control — your fork's ramp is running before rivals' own deeds fire
weights: probe builds ↑ · everything durable ↓
note:    Ben (2026-08-06, NR-070): reworked from a world-scoped race to a tempo race after
         NR-069 settled deeds as personal — no single trophy to contest, but "fire first" still
         reads as a real strategy, just against your own clock instead of a shared one.
```

**Military family — reserved.** L-MIL is unenumerated until BL-157 (units) is mapped; the tree
doc's own rule holds here. A card written against a stub would be fiction wearing a schema.

---

## Reading the rival

BL-068 makes buildings visible and internals private, and the blackboard already carries
`rival_building_owner/tile/type`. So every card has a **visible signature**, and strategy
inference is a lookup, not a mind-read — this table is itself card content (`watch` lines):

| visible signature | likely card | the pressure point |
|---|---|---|
| dense extractors, one body, few processors | deep_seam | beat it to the survey; its rent dies with the price |
| processors clustered on one hub | mill_gate | corner its raw input — its margin is your sell order |
| routes everywhere, assets thin | long_haul | it sees the whole map; watch what it suddenly buys |
| early launchpads + volatiles buildings on ice | propellant_first | it's pacing toward its own First Tank — beat it there or become its customer |
| high launch tempo, rising alarm ⚠ | cadence | let its alarm spend itself; quiet_foothold beats it at the rupture |
| enormous capex parked on one site | bedrock | it has bet the runway — apply tempo pressure elsewhere |
| minimal probes paced against your programme | first_footing | it wants to fire its own keystone deed first; accelerate or feint |

## The invention loop — the AI writes its own deck

The deterministic seeds make the meta *empirical* ahead of release — and under the 2026-08-06
resolution this loop is not the validator of an authored roster, it is the **only mint**:

1. **Scenario suite.** Seeds chosen to stress different worlds — ice-rich, metal-poor,
   short-countdown. Seeded worlds are free (§ 3, AI_OPPONENT.md); the suite is a curated seed
   list.
2. **Play.** Cloud models play scenario × deck through the same MCP interface, cards pinned —
   plus deck-less exploration games, which is where genuinely new lines come from. Traces are
   labelled by construction (consumer 2).
3. **Post-mortem mints.** After each game, a post-mortem pass writes or revises cards *in the
   schema* — the § 10c.4 cross-game reflection, now producing a lint-checkable artifact instead
   of free prose. The post-mortem prompt is content-neutral by rule: it asks what worked and
   what the tells were; it never hints at traps (no telepathy).
4. **The reward gate.** A card earns or keeps its slot only by **same-seed superiority on the
   game's own scoreboard** — net-worth trajectory, era entry, rupture survival — against two
   baselines: the deterministic corp AI (BL-202/203) and the same model playing deck-less. No
   bespoke success metric: if meta works, the existing reward schema shows it (Ben, 2026-08-06).
5. **Freeze per release.** The surviving deck is the SFT conditioning corpus and the local
   model's runtime deck. Post-release, the same loop is the patch cycle.

**Compliance labels, not success metrics.** Each card still names trace-checkable signatures —
they answer *which card was being followed* (corpus labelling, the § 10c.6 rule-based filter),
never *whether it worked*:

| card | compliance signature (trace-checkable) |
|---|---|
| deep_seam | raw-sale share of income ≥ X; survey spend front-loaded |
| mill_gate | realised margin per tick; processor share of assets |
| long_haul | active route count; arbitrage share of income |
| propellant_first | off-world propellant produced before tick T |
| cadence | launches per 100 ticks rising; lift cost falling |
| bedrock | single-site capex share; completion date vs seeded date |
| quiet_foothold | alarm contribution ≤ Y through the countdown AND orbit ≤ T+Δ |
| yard_master | port throughput-fee share |
| iron_belt | yard-input share; **zero** Earth-side PGM sales |
| first_footing | own keystone deeds fired ahead of visible rival programme maturity, per probe spend |

A card that never wins its own favourable scenario is underpowered or fiction; a card that wins
everywhere flattens the meta — both are reward-gate readings, visible in the existing numbers.

> ⟳ 2026-08-19 (BL-411, emergent-strategy readout — landed): the loop's **measuring instrument
> now exists in-engine**. The Strategy readout ledger (`src/ui/strategy_readout.{hpp,cpp}`, nav
> rail slot 12) aggregates the corp_decision stream per corp over a rolling 64-quarter window —
> verb mix, spend split across the must/should/nice priority buckets, reason-code tally, and the
> bucket split quarter-by-quarter as a stacked band. Same-seed comparison (step 4's reward gate)
> can now point at a *distribution shift* rather than asserting "the card worked". Two fences it
> keeps deliberately: **no score/margin aggregates** (candidates sort by priority bucket before
> score — NR-226 — so raw margins do not aggregate honestly), and **no strategy labels** — the
> readout shows the mix and lets the shape speak, preserving this doc's discovered-not-authored
> position.

**Personas (BL-207)** are weightings over this roster — a persona is a prior over cards plus a
caution dial, not new strategy content. Distinguishable styles then come from selection bias,
which the Vox result says is enough (§ 10c.3).

## The design-gap yield

Writing strategy against the real vocabulary is cheap playtesting — every ⚠ above is a place
the game cannot yet *observe* what play needs. The blackboard speaks 24 predicates today; the
roster needs, and lacks:

| missing signal | needed by | arrives with |
|---|---|---|
| `deed` facts (fired firsts + tick) | ST-04/05/08/10 | the deed primitive (tree draft) |
| launch cadence / count | ST-05, rival table | launch dispatch system |
| `alarm_own` / `alarm_total` / `ceiling` | ST-05/06/07 | the danger model (BL-223 extension) |
| rupture countdown | ST-05/06/07 | Era-event mechanics (v0.2.0 design) |
| `tech_unlocked` / `doctrine_taken` | all [draft-tree] cards | BL-156 unlocked set |
| space-market `supply`/`demand` | ST-09 (unwritable without it) | propellant `resource_type` — the tree draft's own flagged blocker |
| import-dependence / interdependence | ST-07 | trade-route substrate reads (BL-088) |
| unrest | danger model's destabiliser row | POPULATION.md arc |

Two findings worth stating plainly. **ST-09 (iron_belt) cannot currently be expressed at all** —
the oversupply-paradox abandon line, the single most documented lesson in the research corpus,
has no observable to fire on. And **nothing in the danger model is observable yet** — a player
(or model) skilled at avoiding danger needs the alarm surfaces before that skill can exist.

## Open questions

*(The 2026-08-06 session answered the draft's original five — trap knowledge, runtime shape,
roster status, first_footing, deterministic binding; see § Resolutions. These are the successors.)*

1. **Deck cap.** How many live cards may the loop keep — a hard ~10, or elastic per era? The
   context budget argues hard; a rich meta argues elastic.
2. **Seed legitimacy.** Do authored seeds count as "told"? Taken strictly here: seed *verdict*
   lines are barred (the ⊘ marks), seed *structure* is allowed. Ben can tighten to zero seeds —
   the loop still runs, just slower to first useful deck.
3. **The post-mortem prompt is now the load-bearing authored artifact.** It must teach
   meta-*learning* without teaching meta. Who reviews it for neutrality, and does it live under
   BL-279 (trace corpus) as a versioned artifact?
4. **Does the invented deck ever surface to the player?** Ben's own conditional (2026-08-06):
   *if we use cards, they have to be in the tutorial.* A private invented deck keeps meta out
   of tutorial scope — but any post-release "strategy guide" surface would re-trigger the
   conditional. Flag before ever surfacing it.

## Landing shape (when this earns a data store)

Mirrors the action dictionary end to end, with one inversion: **`strategies.json`** is written
**by the loop, not by hand** (seeds excepted — hand-editing a live card is the exception that
needs a reason). **`STRATEGY_INDEX.json`** (`[id, one-line]`) generated for standing context,
**`tools/session/strategy_query.js`** for full cards, and a **lint** — every predicate in
`when`/`watch`/`abandon` ∈ blackboard predicates ∪ the declared ⚠ list; every opening id ∈
`ACTIONS_INDEX.json`; every tech node ∈ the tree store once it lands; no ⊘ verdict lines;
≤ 400 tokens per card. MCP grows `get_strategy` / `list_strategies` plus a `strategy_state`
resource; the harness owns the persistence protocol (§ The card contract); and BL-279's
pipeline gains the post-mortem protocol as a versioned, neutrality-reviewed artifact. The lint
is authored as a tool and pushed to a skill, per the standing rule.
