# Project Io — AI Opponent

Authority doc for the AI-opponent thread (BL-199). Ben's 2026-07-23 call: AI-opponent
development may now proceed **alongside** the basic mechanics rather than waiting. This
document opens with the **state-of-the-art research** BL-199 mandates as its first
activity; the architecture proposal, data strategy, and follow-on backlog decomposition
build on it.

**Status.** SOTA map + shortlist + staged path + data strategy: drafted 2026-07-23 (deep-research
harness). **Architecture accepted by Ben 2026-07-26** (§ 5) — the A → B utility core is the
target. The decision decomposition (§ 5), the corp-command seam + state export (§ 6), the
communication/diplomacy principle (§ 7), and the follow-on decomposition (§ 8) landed the same
session. The standing-rule relaxation is now earned **for the scoped path only**: AI-corp
strategic actions via the corp-command seam (BL-202 onward); the player's corp stays untouched
beyond the BL-181 workforce dial.

**Direction set 2026-08-03 (§ 10).** A second research sweep — the public LLM-grand-strategy
field — settled the C-route's transport and target. The interface is **MCP** (an out-of-process
protocol wrapper over the three legs Io already has: blackboard export, action dictionary,
corp-command seam); the runtime model is **small and local**; **cloud inference is a corpus
generator, not a runtime**, producing the supervised traces a small model is fine-tuned on.
Read § 10 before any work on the LLM planner — it supersedes § 7's 2026-08-02 note and answers
NR-040.

**The goal (unchanged from BL-199).** A computer opponent of **roughly human skill** — a
genuine rival across Io's loop (extraction, trade, later conflict), **beatable** by a decent
human, and **legible** (moves read as sensible, not alien or scripted). Explicitly *not* a
superhuman optimiser and *not* a handicap-only fake.

---

## 1. SOTA map

### Area 1 — the classical game-AI toolkit

The shipped-game consensus is blunt: **strategy games rarely use search-based planners for the
strategic layer.** The combinatorial explosion across economy, logistics, and combat makes
forward planning (GOAP, HTN) impractical at the top level; shipped titles use **prioritised
reactive scoring** — evaluate current needs, pick the highest-scoring action, repeat next tick.

| Technique | Good at | Bad at (for an economic 4X) |
|---|---|---|
| **Utility / scoring AI** | Cheap per-tick; degrades gracefully; tunable weights; legible ("built a refinery because refined-goods scored highest"); handles continuous trade-offs (workforce dial, recipe choice) | Weight-tuning is manual craft; can oscillate/thrash without hysteresis; no lookahead |
| **Behaviour trees** | Authoring, debugging, reactivity; good for *modes* and sequencing | Tree size explodes with combinatorial economic options; poor at "which of 50 build sites is best" (a scoring problem, not control-flow) |
| **GOAP** | Emergent action chains toward a goal | Combinatorial blow-up across domains; strategy games "rarely use GOAP" |
| **HTN** | Faster than GOAP in *well-structured* domains (decomposition prunes search) | "Can only find plans the author anticipated" — brittle to novel economic states |
| **Hierarchical strategic/operational/tactical decomposition** | The load-bearing idea; each layer uses the right tool | Needs clean layer interfaces; risk of layers fighting each other |
| **Scripted opening / build-order books** | Cheap, legible, human-like early game; sidesteps cold-start weakness | Exploitable once learned; doesn't generalise across seeds/bodies |

The **dominant shipped pattern is a hybrid**: a small state machine / BT for major modes, with
**utility-scored selectors inside**, and hierarchical decomposition (strategic desires →
operational goals → per-asset assignments). The canonical worked example for a *space 4X* is a
four-phase "generate tasks → enumerate viable assignments → score & sort → assign greedily"
loop, with a scoring formula like `score = (priority + context modifier) / distance`, and the
**same scoring machinery reused for the production/build queue**.

### Area 2 — what Paradox / Civ / Stellaris ship, and their documented failure modes

**What they ship:** weighted utility scoring, universally. Paradox's Clausewitz engine exposes
four primitives — *events, triggers, effects, data weights* — and the AI is "set the weights,
the rest handles itself." **Victoria 3** is the most transparent published economic-AI design:
three parallel **strategies** (Administrative / Political / Diplomatic) chosen by weighted
preference, plus a **Priority-and-Score** spending system — priority buckets ("Must-Have" …
"Should-not-Have") gate spending, scores tie-break within a bucket, and **Spending Variables**
predict economic trajectory so the AI knows a queued workshop will resolve a shortage rather
than over-building. Performance is managed via **tick tasks** — operations tagged with frequency
and dependencies so expensive updates don't run every tick.

**Documented failure modes:**
- **Can't run its own economy → gets handed resources.** Stellaris's AI is widely described as
  unable to build a functioning economy; the shipped remedy was flat resource bonuses and
  effectively spawning credits to launder via the market — "given unlimited alloys so players
  have something to shoot at."
- **Visible cheating breaks the illusion.** Civ's historical sins — free units under fog, instant
  Wonders — "broke the game's established mechanics."
- **Emergent-but-wrong behaviour.** Civ AIs formed apparent "tech-whoring cartels" via inter-AI
  trade; devs had to **intentionally cripple AI-to-AI trading** to stop it.
- **Stalling / difficulty-via-bonus.** Genre-wide, higher difficulty = bigger economic handicaps,
  not smarter play, because authoring genuinely competent economic play is hard.

**Lesson for Io:** the hard part is not the decision framework — it is making the AI actually
**solvent** in the same economy the human plays. Victoria 3's predictive-spending + priority-bucket
model is the directly transferable idea.

### Area 3 — modern learning approaches

- **Self-play deep RL (AlphaStar, OpenAI Five).** Reaches super-human play but is **prohibitively
  heavy for a solo dev** (AlphaStar: ~3,072 TPU + ~50k CPU cores, 44 days, ~$3M to replicate;
  OpenAI Five: 256 GPU + 128k CPU cores, 770+ PFlops/s-days). Both also aim at the *wrong target*
  for Io — alien, super-human, non-legible play. **Ruled out.**
- **Imitation / behavioural cloning.** Cheap and stable, but needs **expert demonstrations** (Io
  has none) and suffers **covariate shift → compounding errors** off-distribution; cannot exceed
  its demonstrator. Poor fit *now*; plausible *later* if a good heuristic AI generates trajectories.
- **Offline RL.** Extracts good policies even from **suboptimal** logged data and (e.g. CQL) beats
  BC. The learning family that fits Io's assets — a deterministic sim can generate huge logged
  datasets cheaply — but a significant engineering lift and research risk for a solo dev.
- **LLM-planner-over-scored-primitives / hybrid.** The most exciting recent direction and
  **directly aligned with Io's proposed out-of-process-policy-over-compact-state architecture.**
  *Vox Deorum* (Civ V + Vox Populi) puts an LLM on *macro-strategic reasoning* and delegates
  tactics to algorithmic subsystems; across 2,327 games open-source LLMs reached "competitive
  end-game play" with **human-varied play styles**. Broader hybrid work (LLM planner over
  scored/skill primitives, with an **action knowledge base masking illegal transitions to prevent
  hallucination**) reports parity with hand-crafted behaviour trees while beating flat RL. Key
  constraint: **ground the LLM against an explicit set of legal primitives.**

### Area 4 — human-skill calibration & exploiting deterministic seeds

**Calibration (Soren Johnson's "playing to lose").** There is **no truly "fair" difficulty** where
the AI plays the identical game, so **perception is what matters.** Findings that transfer to Io:
- Players tolerate **small, gradual, invisible bonuses** but revolt at **rule-breaking cheats**
  (acting under fog, free stuff, instant results).
- **Transparency is the antidote** (Mario Kart's rubber-banding is accepted because explicit).
- Players **misattribute** ordinary luck to cheating; a statistically fair system can feel unfair.
- Fairness ordering for Io: **genuine skill > mild transparent economic bonus > information
  asymmetry > rule-breaking cheat.** Io's goal means leaning on genuine skill first, reserving
  small *transparent* handicaps as difficulty knobs — never fog cheats.

**Exploiting infinite deterministic seeds — Io's structural advantage.** `make_hard_coded_world(seed)`
+ fixed ticks + the headless verify harness enable: **scenario generation** (an inexhaustible,
reproducible corpus of economic situations); **bot-vs-bot rollouts** (AI-vs-AI across thousands of
seeds to measure solvency, survival, net-worth curves); **deterministic offline policy evaluation**
(same seed, two policies, exact A/B); and **skill-regression testing** — freeze a seed-set
benchmark and assert the AI still hits its solvency/net-worth/win-rate bands after each change, the
AI analogue of Io's `verifier-*` culture (with stochastic Pass/Fail/Inconclusive verdicts for the
non-deterministic parts).

---

## 2. Shortlist for THIS codebase

### A. Scored utility AI over the existing local-agency seam — **recommended first build**
Extend the background-corp per-building triggers (BL-079) into a **strategic scoring layer**:
enumerate candidate actions (build X on tile T, switch recipe, set workforce dial, open/close a
trade route), score each with tunable weighted heuristics, apply hysteresis, execute top-N within
a per-tick action budget.
- **Dev cost:** low–medium (natural extension of what exists). **Determinism:** perfect (pure
  function of world state; seed the tie-break RNG). **Legibility:** excellent. **Skill ceiling:**
  medium — enough for a genuine, beatable rival with good weights + predictive spending.
  **Data needs:** none.

### B. Layered utility with a predictive economic model (Victoria-3-style) — **recommended second**
Add a **strategy layer** (extraction specialist vs trade arbitrageur), **priority buckets**
(Must-Have solvency vs Nice-to-Have expansion), and **spending variables** that forecast the
market impact of queued buildings before committing. This is what makes the AI *solvent* rather
than needing a cheat.
- **Dev cost:** medium. **Determinism:** perfect. **Legibility:** high. **Skill ceiling:** high
  for economic play. **Data:** none.

### C. Out-of-process LLM planner over scored primitives (Vox-Deorum-style) — **later, optional differentiator**
The external policy reads Io's compact JSON state export; the LLM picks **strategy / goals** among
an explicit legal-primitive set (masked to prevent hallucination); the utility layer (A/B) executes
tactically. Gives varied, legible, human-like *personalities* per corp.
- **Dev cost:** medium glue, but adds latency, non-determinism, API cost/dependency.
  **Determinism:** poor unless the LLM call is cached/replay-logged per decision — mitigate by
  making the LLM choose only coarse strategy occasionally, not per-tick. **Legibility:** high (can
  self-explain). **Skill ceiling:** competitive end-game. **Data:** none (zero/few-shot).
- **Updated 2026-08-03 (§ 10):** transport is **MCP**, and "API cost/dependency" no longer applies
  to the shipped runtime — the target model is **local**. Cloud play is a one-off corpus-generation
  activity (BL-279), not a per-decision dependency. The "Data: none" line still holds for the
  zero-shot path, but the direction now *chooses* to collect data, to shrink the model.

### D. Offline RL over logged self-play — **furthest out; only if A–C plateau**
Deterministic seeds generate massive logged datasets; train an offline-RL policy (CQL-class) as an
out-of-process policy over the JSON state.
- **Dev cost:** high + research risk. **Determinism:** good (greedy inference). **Legibility:** low
  (the weakness — risks alien play). **Skill ceiling:** potentially super-human (must be *capped*).
  **Data:** large, but self-generable.

### Recommended staged path
1. **Now:** build **A** — scored utility over the local-agency seam. The load-bearing 80%.
2. **Next:** layer **B** — strategy selection + priority buckets + predictive spending, so the AI
   is genuinely solvent and needs no cheat. Add a **seed-set skill-regression harness** (bot-vs-bot
   solvency/net-worth goldens) alongside.
3. **Difficulty knobs:** transparent, small, gradual economic multipliers only — never info/rule
   cheats.
4. **Later / optional:** **C** for personality and variety once the utility core is solid; keep it
   out-of-process, coarse-grained, replay-logged to protect determinism.
5. **Only if needed:** **D**, capped, as a top-difficulty tier.

---

## 3. Data strategy — exploiting infinite deterministic seeds

- **Benchmark seed-set (golden):** freeze N seeds spanning body/terrain/market diversity; the AI's
  economic outcomes become regression goldens (net-worth curve, survival rate, win-rate vs a
  reference AI). Re-run headlessly on every AI change — the direct analogue of `verifier-headless`.
- **Bot-vs-bot rollouts** as the primary skill signal.
- **Deterministic offline policy evaluation:** same seed, two policies, exact A/B.
- **Scenario generation for weakness-hunting:** search seeds for insolvency/thrash; those become
  new regression fixtures.
- **Trajectory logging** turns every rollout into future BC/offline-RL training data at near-zero
  cost — banking data now keeps option **D** open later.

---

## 4. For Project Io specifically — the verdict

Io is **unusually well-suited to a strong *classical* AI.** Its decision surface (where to build,
what recipe, workforce dial, supply routing, order-book trading) is exactly the discrete/continuous
mix that **weighted utility scoring handles best**, and it already has the substrate — background
corps taking narrow deterministic per-building actions (BL-079). The correct move is to **grow that
seam into a scored strategic layer, not to bolt on a planner or a network.**

The single most important design import is **Victoria 3's predictive-spending + priority-bucket
model** — the documented answer to the genre's central failure (an AI that can't stay solvent and
therefore gets handed resources). It lets Io's AI hit its "beatable human rival, no fake handicap"
goal *honestly*.

Io's deterministic seeds + headless harness + golden-and-verify culture let it **regression-test AI
skill** better than any shipped 4X — a genuine advantage a solo dev should exploit from day one.
Legibility (an Io requirement) also favours utility scoring: every move is a top-scoring choice with
a readable reason, the opposite of AlphaStar's alien play.

The **LLM-planner-over-scored-primitives** route (Vox Deorum) is the one modern approach worth
keeping on the roadmap — it maps cleanly onto Io's out-of-process-policy-over-JSON architecture and
adds legible *personality* — but it sits **on top of** a solid utility core, kept coarse-grained and
replay-logged, not as the first build.

**Bottom line:** build **A → B** with a seed-based skill-golden harness; reserve **C** for variety
and **D** (capped) for a top tier only if the heuristic core plateaus. Do **not** start with
learning; do **not** ship resource cheats as a substitute for competence.

---

## 5. Accepted architecture — the decision decomposition (Ben, 2026-07-26)

Ben accepted the **A → B staged path** (scored utility over the BL-079 seam, then Victoria-3-style
predictive spending). This section is the concrete decomposition the acceptance unlocks.

### Cadence — where the AI thinks

The AI evaluates at the **econ-tick boundary inside `run_economy_step`**, in the slot the BL-079
agency block occupies today; that block folds in as the scorer's **reflex tier** (tier 0: recipe
rescue, idle-a-loser — unchanged behaviour, now emitting commands). Strategic evaluation is
**staggered**: corp `c` evaluates every `K` ticks at offset `id(c) % K` (Victoria-3's tick-task
idea — bounded per-tick cost, fully deterministic).

### The candidate-action set (legal primitives)

The AI's verbs are exactly the player's, through the same validation — **no bypass, no cheats by
construction**:

| Verb | Engine seam | Notes |
|---|---|---|
| `build(type, tile, target, recipe)` | `construct_building` (placement_rules, build cost, `stack_capacity`) | The big decision; candidate sites bounded (below) |
| `demolish(building)` | `demolish_building` | Rare; scored against salvage vs sustained loss |
| `set_recipe(building, recipe)` | direct component write (BL-079 idiom) | Tier-0 rescue generalises to margin-chasing |
| `set_workforce(building, target)` | `solve_workforce_target` (BL-181) | AI corps simply run `workforce_auto` on |
| `idle(building)` / `resume` | `decommissioned` flag | Tier-0 loss-streak rule, now reversible |
| `dispatch_convoy(src, dst, res, qty, mode)` | `dispatch_convoys` seam | Strategic hauls beyond the shortfall auto-dispatch |
| `place_road(a, b, tier)` | `place_road` | Infrastructure investment; scored on route throughput |
| `survey(body, region)` | survey_system | The AI pays for discovery like the player |
| `set_exchange_policy` / `set_counterparty` | BL-160 / BL-161 once landed | The AI authors the same policy tables |

**Candidate enumeration is bounded**: build sites come from **surveyed tiles the corp can see**
(its own fog state), pre-filtered to the top-M by static suitability (terrain affinity × deposit
richness), M small; recipes and dials enumerate per owned building; routes enumerate over
known-body pairs. Bounded enumeration is what keeps the per-tick cost flat.

### Scoring

`score(action) = expected_net_per_tick / payback_ticks × strategy_weight`, terms computed from
**existing functions only** (`estimate_building_profit`, placement affinities, live market prices,
wage rate, logistics cost to nearest market, build-cost amortisation) — no new oracles. The
`strategy_weight` biases toward the corp's generated **industrial focus** (specialist premise,
CORPORATION_GENERATION.md), giving distinct-but-legible personalities for free. A **solvency
gate** (cash − committed spend > reserve floor) vetoes any spend that breaks the floor; stage B
replaces this crude floor with priority buckets + predictive spending (landed 2026-07-31, see
§ 2B below).

### 2B. Stage B — strategy, priority buckets, predictive spending (BL-203, landed 2026-07-31)

Implemented in `src/world/corp_ai.{hpp,cpp}`, extending BL-202's scorer rather than replacing it:

- **Strategy layer.** `corp_strategy` is a named alias of `industrial_focus` — the corp's
  generated specialist premise (extraction / processing / trade) IS its strategy, kept as its
  own concept so the bias is legible and can diverge from the generation-time focus later
  without a signature break. `focus_weight` (BL-202) already biases build/survey scores by this
  strategy; stage B does not change that mechanism, only names it.
- **Priority buckets** (`corp_priority_bucket`: `must_have` / `should_have` / `nice_to_have`),
  derived deterministically from each candidate's existing `corp_decision_reason` via
  `bucket_for_reason` — `dial_idle` is Must-Have (stops a sustained loss's wage/maintenance
  bleed); `dial_recipe` / `dial_workforce` / `dial_resume` are Should-Have (tune or restore a
  running asset); `best_build` / `survey_expand` are Nice-to-Have (expansion). Candidates sort
  bucket-ascending before score-descending, so a Must/Should-Have action is never starved by a
  higher-scoring Nice-to-Have one. Concretely, only Nice-to-Have candidates carry capex in this
  codebase (dials are free), so the "never starve a higher bucket" rule is enforced by gating
  build/survey spend against a **stricter** floor: `corp_should_have_buffer` sums
  `estimate_building_profit(...).input_cost` over the corp's own running processing facilities
  — the cash needed to keep feeding them this tick — and `nice_to_have_floor = reserve_floor +
  should_have_buffer` is the gate a build/survey must clear, on top of (not instead of) BL-202's
  existing reserve floor.
- **Predictive spending.** `forecast_glut_multiplier` forecasts a candidate build's added supply
  (`base_rate × richness × workforce × (1 − hazard)`) over a horizon of
  `build_duration_ticks + forecast_clearing_ticks` (1 by default — "one clearing pass") against
  the **local market's PUBLIC `supply`/`demand` aggregates only** — the same facts
  `export_corp_blackboard` would show a rival (BL-068/DISCOVERY.md), never a private read. No
  public demand signal (`demand <= 0`) yields no penalty (the AI cannot forecast against a fact
  it cannot see); the projected supply/demand ratio is unpenalised at or below
  `glut_taper_ratio` (1.0), tapers the build's score linearly to zero at `glut_veto_ratio` (2.0),
  and vetoes (removes the candidate entirely) at or above it. Applied only to build candidates;
  dials and survey are unaffected (a body's total surveyed area doesn't glut a market).

Verified by `tools/verify/corp_ai_predictive_harness.cpp` (R1: the reason→bucket mapping; R2: the
Should-Have buffer is well-defined and never loosens the floor; R3: the forecast is
visibility-honest, monotone, tapers, and vetoes at the documented ratios; R4: an end-to-end
saturated-market scene actually vetoes a build the plain BL-202 scorer would have taken) and
regression-checked against `corp_ai_harness.cpp` (BL-202), all green.

### 2C. Trading (BL-293, landed 2026-08-08)

Ben, 2026-08-07, resolving NR-083: *"Order book needs to be a background process, the AI must be
able to trade as a player does."* A player-only fence over the trade verbs was proposed and
explicitly rejected, so the scorer reaches the order book exactly as it reaches build and survey
— `place_sell_order` is a `corp_verb`, and rival corps are the corps that drive the seam.

**This is a grant of reach, not of skill, and the distinction is the design.** "Can trade" is not
"trades well": a scorer that dumps stock at the floor price is genuinely *worse* than one that
does not trade, because it drags the resolved price down for everyone including itself — and the
auto-surplus path was already clearing that stock at the reference price anyway. So the first cut
is the narrowest thing that is still trading:

- **Candidate**: for each `(corp, body)` pool, each resource the local market prices, stock above
  `trade_hold_threshold` (50 units) — well clear of any processor's per-tick draw, so listing can
  never compete with feeding the corp's own chain.
- **Quantity**: `trade_release_fraction` (0.5) of the excess. It meters its release rather than
  emptying the pool into one quarter's clearing.
- **Floor**: `trade_floor_multiple` × the market's `base_price` — the rarity-derived value floor,
  the closest per-resource cost reference the world exposes. At 1.0 the corp simply refuses to
  sell below it.
- **Score**: expected cash valued *at the floor*, not at the current price. The conservative
  estimate, so a listing on a crashed market cannot outscore a genuinely profitable dial.
- **Bucket**: Should-Have. Listing accumulated stock carries no capex — it brings cash *in* — so
  it can never starve a higher bucket, which is the only test the buckets apply.
- **Anti-thrash**: never a second order on a `(corp, body, resource)` that already has one, and
  at most `max_trades` (1) order-book command per evaluation. A trade command's subject is a body,
  not a building, so it takes no dial slot and records no building cooldown.

All three numbers are `corp_ai_params` fields, so tuning is a data change. **Two honest gaps**,
both follow-on work rather than defects here: `base_price` is a rarity floor and not a production
cost, so on a resource whose real cost sits above its rarity floor the AI will sell at a loss;
and the book is one-sided — `buy_order` has world state and a save format but no verb, so the AI
can release stock and cannot bid for it. A real strategy (price trend, timed release, targeting a
rival's shortage) is later work.

Verified by `tools/verify/order_book_harness.cpp` § R5, which asserts the conservatism as
behaviour rather than as intent: never below the rarity floor, never on a pool under the
threshold, never a duplicate, and never on the player's own corp.

### Hysteresis & action budget

- **Do-nothing bias**: a candidate must beat the incumbent (or doing nothing) by a relative
  margin θ (~15%) — the anti-thrash rule.
- **Cooldowns**: a building that changed recipe/workforce/state holds for C ticks; reversals ride
  loss/gain streaks (the BL-079 `loss_streak` idiom, generalised).
- **Budget**: per evaluation, at most **1 construction + a small number of dial changes + 1
  order-book command** per corp; total committed spend capped by the solvency gate.
- **Determinism**: stable iteration (sorted `corp_ids`, stored asset order, tile-index order);
  ties break on lowest entity id; the only randomness is a per-corp hash of the world seed used
  as a fixed personality jitter on weights — constant per campaign, deterministic by construction.

---

## 6. The corp-command seam & state export (shared with multiplayer)

The scorer does not mutate the world directly. It emits **`corp_command`** records —
`{tick, corp, verb, args}` — applied at the tick boundary through the player-grade validation
above. This is deliberate triple-duty:

- **AI**: the command stream *is* the decision log. A ring buffer of commands + score rationale
  (winning score vs runner-up) is the AI's legibility surface, its replay artifact, and the
  skill-harness's input.
- **Multiplayer**: `MULTIPLAYER_PRINCIPLES.md` § Preserve now #2 — lockstep exchanges exactly
  this: small serialisable intents tagged with their tick. An AI corp is a local command source;
  a network player is a remote one; the seam is identical. (`canvas_command` stays
  navigation-only; `corp_command` is its sim-mutating sibling.)
- **Out-of-process policies (C/D)**: an external policy consumes the state export and returns
  commands — the same contract, across a process boundary.

**State export (schema designed now, implemented with BL-202).** A compact, tick-tagged,
per-corp JSON view that is **visibility-honest**: it contains only what that corp could see under
the BL-068 rules and its own fog state — own buildings/pools/cash in full; public market
prices/aggregates; rival *buildings* but not their internals; its own routes and survey state.
The AI reads through the same information asymmetry the player does; anything else is the fog
cheat the calibration research warns against. Version the schema from day one.

---

## 6a. The action dictionary (BL-270, 2026-08-02) — the word interface's third leg

The seam above gives an out-of-process policy its **write** channel and the state export
its **read** channel. What neither states is *meaning*: which press to choose, and why.
**`docs/ai/ACTIONS.json`** (readable mirror `ACTIONS.md`, generated by
`tools/session/render_actions.js`) closes that gap — every control in the game, catalogued
as `{press, typed args, preconditions, expected_output, reason_to_select}` across five
families (gameplay / canvas / lens / ledger / chrome).

Two properties matter for the AI use:

- **The gameplay entries are transcribed, not authored.** Their `args` and `preconditions`
  mirror `corp_command.hpp`'s verbs and `corp_command_result` rejections exactly; where the
  dictionary and the seam disagree, the dictionary is wrong and the fix is mechanical.
- **`reason_to_select` is the policy prior.** It states, in words, why a press exists —
  the design intent an LLM policy conditions on before world state ever enters the picture.
  This is what makes word-based play (and later word-driven generation, and the
  difficulty-level work — Ben, 2026-08-02) a prompt-assembly problem rather than a
  fine-tuning one: blackboard (read) + dictionary (meaning) + command seam (write).

The catalogue axes, for orientation: `user_stories.json` = player **intent**;
`UX_QUESTIONS` (BL-260) = what a readout **answers**; `ACTIONS` = what a press **does**.

**Consumption model (Ben, 2026-08-02).** A language agent never absorbs the store
whole: it holds the generated **`ACTIONS_INDEX.json`** (`[id, surface]` per action)
in context and fetches full entries on demand via
`tools/session/actions_query.js` (by id, family, or keyword) — the lookup the
word-play harness wraps as the LLM's dictionary tool.

**Priority is NOT dictionary material (Ben, 2026-08-02).** Entries deliberately
carry no urgency or importance score. The live AI scores each *candidate* action
on a **2D urgency × importance map** at decision time, against blackboard state —
urgency (how much each tick of delay costs *now*) and importance (how much the
outcome matters to the strategy) are properties of the situation, not of the
verb. A static per-entry score was considered and rejected the same day: it
would claim `idle` is always urgent, when it is urgent only for a building
hemorrhaging cash. The dictionary supplies meaning; the policy supplies priority.

---

## 7. Communication & diplomacy — the chat principle (Ben, 2026-07-26)

Ben's steer: **since every rival is AI, inter-corp coordination should happen in a communication
medium, not hidden state.** Corps message **publicly or in private groups** to form plans;
diplomacy is *legible messaging*, not behind-the-scenes flags. This is a better diplomacy
principle than invisible opinion modifiers — and in multiplayer the same channels carry human
players, so the medium is actor-agnostic.

- **Surface**: a **chat log window replaces the Explorer placeholder** (right shell band, middle
  slot — the Explorer's pinning concept was never wired and is superseded; LAYOUT.md updated).
  Channels: **Public** (every corp), plus **arbitrary groups** (any subset of corps, created from
  the panel). The player reads Public and any group they belong to.
- **Stage A (now)**: messages are **templated, deterministic renderings of the decision log** —
  the BL-079 reflex events today ("Meridian idled Ironworks — sustained losses"), the BL-202
  command stream when it lands. The chat is the AI-observability surface first; personality
  prose comes later.
- **Stage C (later)**: the LLM planner speaks in-character in channels; AI↔AI private groups
  form plans the player cannot read — which is principled, because the *medium* is uniform and a
  future intelligence mechanic (Discovery-model extension) can expose intercepts as content.
- **Determinism**: stage-A message text derives purely from deterministic events. Free-text LLM
  chat arrives only under the C-route rules (out-of-process, coarse-grained, replay-logged).
- **Player input**: a message box posts to the selected channel. It has **no mechanical effect
  yet** — it is the hook the C-route consumes (the player negotiating with AI corps in language).

> **2026-08-02 note, superseded 2026-08-03 — see § 10.** A design pass explored C-route as a
> shipped in-process feature (a live Anthropic API call per corp per econ tick) and got as far as
> a full decomposition before Ben clarified that isn't the intent — see NR-039/NR-040 in
> `NEEDS_REVIEW.json` for the walk-back. The interim reading was that the near-term plan meant
> computer-use play (Claude driving the game visually via mouse/keyboard); the "what plumbing is
> missing" question NR-040 left open is **answered in § 10**: an MCP server over the three legs
> that already exist. Stage C's *shape* (out-of-process, coarse-grained, over legal primitives)
> stands unchanged; its *transport* is now MCP and its *target model* is small and local.

---

## 8. Follow-on decomposition (filed 2026-07-26)

BL-199 closes with this decomposition; the build work is carried by:

| Item | Carries | Requires |
|---|---|---|
| **BL-202** `CORP_AI_SCORED_UTILITY` | Stage A: the scorer, the corp-command seam + decision log, state-export implementation | — |
| **BL-203** `CORP_AI_PREDICTIVE_SPENDING` | Stage B: strategy layer, priority buckets, spending forecast — the solvency answer (**landed 2026-07-31**, § 2B) | BL-202 |
| **BL-204** `AI_SKILL_HARNESS` | Seed-set skill-regression harness (bot-vs-bot goldens: solvency, net-worth curves) + the tick-boundary **state hash** (doubles as the multiplayer desync primitive) | BL-202 |
| **BL-205** `CORP_CHAT_LOG` | The § 7 surface: chat window replacing the Explorer, channels + groups, agency-event feed (first slice lands with the item's filing) | — |

Economics the scorer prices, settled alongside (same session): **BL-153** (convoy freight
premium) and **BL-193** (stack diminishing returns); **BL-160/161** confirmed as the AI's trade
primitives (the AI authors the same policy objects via commands).

---

## 8a. The world history log (BL-208, landed 2026-08-02)

The project's first flat-binary serialisation seam (`src/world/history_log.{hpp,cpp}`),
answering backlog.json § BL-208's settled design: a **single interleaved, append-only, tagged,
serialised** world log — not per-body/per-corp logs, which fail the moment a corporation acts on
a body (every interesting event) and would need a join with no shared ordering.

**The shape**, in `src/world/world.hpp`:

```cpp
enum class history_topic : uint8_t { genesis, checkpoint, decision, agency, trade_route };

struct world_history_entry {
    int64_t        timestamp;   // unit depends on topic — see below
    history_topic  topic;
    entity_id      body = null_entity;  // tag, null_entity if not applicable
    entity_id      corp = null_entity;  // tag, null_entity if not applicable
    std::string    event;
    std::string    consequence;         // may be empty
};

std::vector<world_history_entry> history_log;  // on `world`
```

**Timestamp is topic-scoped, deliberately.** `genesis`/`checkpoint` entries reuse
`history_event::years_before_epoch` exactly (positive = the deep/historical past, 0 = the 1960
epoch) — no conversion, per the item's own settled design. `decision`/`agency`/`trade_route`
entries — which only ever occur during the live simulation, strictly after the genesis chapter's
one-time bulk-insert at world setup — instead carry the sim day tick. A reader branches on
`topic` to know which clock it is reading; nothing needs the two to compare numerically, because
the vector's append order is already the true chronological order end-to-end.

**The four sources, all additive to their existing consumers** (no behaviour change to anything
that already read `ai_decisions`, `agency_events`, or `trade_routes`):

- **genesis** — `seed_genesis_history(world&, const generation_report&)` copies each body's dated
  `history_event` lines into the log at world setup (`app::setup_world`, right after
  `make_hard_coded_world`), tagged by that body's entity id. This is the first bridge from
  `generation_report` (presentation-only; per `hard_coded_world.hpp` it never otherwise reaches
  `world`) into world state.
- **checkpoint** — the same call migrates `planetology_state::checkpoints` alongside genesis, per
  body, merged and stable-sorted into one chronological run. `checkpoint_record` carries no
  timestamp of its own; the resolution rule (a documented simplification, not an exact per-line
  pairing — see `history_log.cpp`'s `resolve_checkpoint_timestamp` and the NEEDS_REVIEW entry it
  cites) takes the LAST dated history line at or before the checkpoint's own chain stage.
- **decision** — `corp_ai.cpp`'s strategic-tier push site additionally logs a one-line narration of
  each applied `corp_decision` (verb + reason + score). `corp_decision_ring` (the existing 256-cap
  ring) is untouched.
- **agency** — both the BL-079 reflex tier (`economy_system.cpp`'s recipe-rescue and idle-a-loser
  sites) and the BL-202 strategic tier (`corp_ai.cpp`) additionally log a narrated `agency` entry.
  `economy_report::agency_events` (the chat feed's existing source) is untouched.
- **trade_route** — `supply_system.cpp`'s `credit_arrived_convoys` logs only when a body-pair lane
  is **first established** (the `rit == w.trade_routes.end()` branch), never on a repeat completion
  (which only bumps the existing `trade_route`). `world::trade_routes` and
  `body_activity_visibility` are untouched, byte-for-byte.

  **Two entries per establishment (BL-282, landed 2026-08-09).** A new route is a *two-body* event
  but `world_history_entry` carries one body tag, so tagging only the destination made a
  body-scoped "what happened at X" filter miss the route from its **source** side. The branch now
  pushes **two** entries — one tagged `src_body`, one tagged `dest_body` — carrying the *same*
  narration naming both endpoints. Order within the tick is fixed **source then destination**, not
  iteration-dependent, so replays stay byte-identical.

  *Why not widen the struct.* A `body_b` field would change a struct four other call sites depend
  on, for one topic's need, and would leave every other topic carrying a field it never sets. Two
  tagged entries keep the one-body invariant and keep every existing reader correct unmodified.

  *The cost.* Route establishment doubles in log volume — bounded by **body-pair count**, not
  convoy traffic (a route is logged once, not per delivery), so the absolute growth is small: with
  *n* bodies the ceiling is `2 × n(n−1)/2` entries per corp instead of `n(n−1)/2`. The real price
  is duplicated narration: a reader rendering chronologically sees the same sentence twice with
  different body tags. Mitigation if that reads badly — keep the narration identical (it is) and
  let the renderer de-duplicate on `(tick, event)` when it is *not* filtering by body.

**Serialisation** (`write_history_log`/`read_history_log`): a leading 4-byte magic (`"IOHL"`) plus
a `uint32_t` version (BL-107's "first thing to add when the serialiser lands" rule), then an entry
count, then each entry length-prefixed. `read_history_log` rejects — rather than misreads — a
wrong magic, an unsupported version, an unknown topic byte, or a truncated stream, leaving the
destination world's `history_log` untouched on any rejection. Round-trip is field-identical.
Verified by `tools/verify/history_log_harness.cpp` (mirrors `history_ladder_harness.cpp`'s style:
built over the real generated world, not hand-fabricated entries) and an added determinism check
in `tools/verify/determinism_harness.cpp` (two identical-seed generations produce an identical
genesis+checkpoint chapter).

Not built (deliberately out of scope, per the item's brief): a save-game menu/UI. The serialiser
is a library function a harness exercises; the one live hook is the genesis-chapter bridge at
world setup.

BL-218 (nations rewrite) and BL-219 (corporations rewrite) are expected to write into this same
log — the substrate this item exists to hand them, per the item's own resequencing rationale.

---

## 9. Citations

Sources gathered by the deep-research harness (2026-07-23). Each supports the claim noted.

- DeepMind — AlphaStar (Grandmaster StarCraft II): self-play RL ceiling + alien-play concern. https://deepmind.google/blog/alphastar-grandmaster-level-in-starcraft-ii-using-multi-agent-reinforcement-learning/
- OpenAI Five — Dota 2 with Large-Scale Deep RL: scale of self-play RL. https://arxiv.org/pdf/1912.06680
- Soren Johnson — "Game AI & Our Cheatin' Hearts": fair vs unfair cheats, transparency, playing-to-lose. https://www.gamedeveloper.com/game-platforms/analysis-game-ai-our-cheatin-hearts
- Victoria 3 Dev Diary #59 — AI: strategies + Priority-and-Score spending + predictive Spending Variables. https://www.paradoxinteractive.com/games/victoria-3/news/victoria-3-dev-diary-59-ai
- Victoria 3 Dev Diary #76 — Performance: tick-task model for budgeting economic updates. https://www.paradoxinteractive.com/games/victoria-3/news/dev-diary-76-performance
- Stellaris — "Is it true the AI cheats?" (Paradox forums): economy-failure-via-resource-handout. https://forum.paradoxplaza.com/forum/threads/is-it-true-that-the-ai-cheats.1139965/
- Game Developer — Designing AI Algorithms for Turn-Based Strategy Games: the 4X-space scoring loop. https://www.gamedeveloper.com/design/designing-ai-algorithms-for-turn-based-strategy-games
- Socratopia — Beyond State Machines (Utility AI / BT / GOAP): why strategy games avoid GOAP. https://www.socratopia.app/library/game-code-anatomy-en/chapter-12
- Tono — Game AI Planning (GOAP / Utility / Behavior Trees): comparative strengths/weaknesses. https://tonogameconsultants.com/game-ai-planning/
- Anbeeld — Designing AI for Strategy Games Through Modding: Clausewitz weighted-utility practice. https://anbeeld.com/articles/designing-ai-for-strategy-games-through-modding
- Vox Deorum — Hybrid LLM Architecture for 4X/Grand Strategy: LLM macro-strategy + subsystem tactics; 2,327 games. https://arxiv.org/abs/2512.18564
- Hierarchical Control in Multi-Agent Games (LLM planning + RL execution): action-KB masking to prevent illegal actions. https://arxiv.org/abs/2606.20014v1
- When Should We Prefer Offline RL Over Behavioral Cloning?: offline RL beats BC, tolerates suboptimal data. https://arxiv.org/abs/2204.05618
- Is Behavior Cloning All You Need? (horizon in imitation learning): covariate shift / compounding error. https://arxiv.org/pdf/2407.15007
- Metagame Autobalancing for Competitive Multiplayer Games: bot-vs-bot self-play to measure balance. https://arxiv.org/pdf/2006.04419
- Toward Automated Game Balance: self-play bots reduce manual testing / assess difficulty. https://www.researchgate.net/publication/355109124
- AgentAssay — Regression Testing for Non-Deterministic Agents: Pass/Fail/Inconclusive statistical verdicts. https://arxiv.org/pdf/2603.02601

**Two specifics the research could not pin down:** Vox Deorum's exact state-export schema, per-decision
latency, and token cost (the architecture and results are confirmed; the low-level interface numbers
are not); and a numeric per-tick AI CPU budget from Paradox (the tick-task *mechanism* is documented,
concrete budget values are not published). Both matter for designing Io's option C and should be
resolved from full-text sources before committing to an LLM-planner build.

> **Resolved 2026-08-03 (§ 10).** The Vox Deorum numbers are now published: **~1 minute per
> decision** (model-dependent) and **20.35M input / 555k output tokens per complete game** for
> `gpt-oss-120b`. The Paradox per-tick CPU budget remains unpublished and is now moot for the
> C-route — the planner runs out-of-process and off the sim's tick budget entirely.

---

## 10. The 2026-08-03 refresh — MCP, the public field, and the small-local-model direction

A wide survey of publicly available LLM-grand-strategy work, run 2026-08-03. It supersedes
§ 7's 2026-08-02 note, answers NR-040's open "what plumbing?" question, and states the
direction Ben set on reading it.

### 10a. What MCP is, and why it is the answer here

**MCP (Model Context Protocol)** is an open protocol for connecting a language model to context
and actions. It is a *cable standard*, not a capability — it does not make a model smarter, it
standardises the socket a model plugs into.

The shape: a **server** exposes a resource (here, the game); a **client** is whatever agent
runtime is driving (Claude Code, Claude Desktop, a Gemini/Codex CLI, an Ollama-backed local
loop, or a bespoke harness). They speak **JSON-RPC 2.0 over stdio** (the server is a
subprocess) or over HTTP. The server offers three primitives:

- **tools** — functions the model may call, with typed arguments and enumerated failures.
- **resources** — read-only data the model may fetch by URI.
- **prompts** — reusable templates that tell the model how to use the above.

Four properties are what make it the right seam for Io:

1. **The model side stays entirely out of process.** `ProjectIo.exe` ships **no HTTP client, no
   API key, no cloud dependency**. The MCP server is a local process; with no client attached
   the game is byte-identical to today. This is the exact thing NR-039's walk-back rejected —
   a live API call inside the econ tick — avoided by construction rather than by discipline.
2. **Model-agnostic by design.** The same server serves a cloud frontier model today and a
   locally-hosted small model tomorrow, with no change on the Io side. That swap *is* the
   direction in § 10d, so the interface must not care which model is attached.
3. **Fair by construction.** Tools are the only write channel, so the model plays through the
   same validation and the same visibility rules as the player. The field converged on this
   independently — `civ6-mcp` routes every agent action through Civilization VI's own
   rule-enforcing Lua APIs rather than mutating state.
4. **It is now the field standard.** Vox Deorum, `civ6-mcp`, `civStation` and CivBench all
   arrived at an MCP wrapper over a fixed verb list, separately. Adopting it costs nothing in
   originality and buys every existing client for free.

**Io is unusually close to MCP-ready**, because BL-270 (action dictionary) and BL-206
(blackboard export) already built the hard parts. The mapping is near-mechanical:

| MCP primitive | The Io asset that already fills it |
|---|---|
| **tools** | `corp_command.hpp`'s verbs — already typed, already validated, already rejection-enumerated (`corp_command_result`) |
| **resources** | `--export-blackboard` JSONL (BL-206) — visibility-honest, deterministic ordering, schema-versioned |
| **prompts** | `reason_to_select` in `ACTIONS.json` (BL-270) — the design-intent prior, in words |
| the lookup | `ACTIONS_INDEX.json` + `actions_query.js` — already the hold-the-index, fetch-on-demand pattern an MCP tool wraps |

The honest read of NR-040: the plumbing that was missing is **one wrapper**, not a subsystem.

### 10b. The public field

| Project | What it is | The transferable lesson |
|---|---|---|
| **Cicero** (Meta, 2022) | LLM dialogue + strategic planner for full-press *Diplomacy*; top-10% on webDiplomacy | Language and planning as **separate modules** with the planner controlling the language model — still the reference architecture for negotiation |
| **Vox Deorum** | LLM macro-strategy over Civ V + Vox Populi; **2,327 full games** | The load-bearing result — see § 10c.1 |
| **civ6-mcp / CivBench** | MCP server (76 tools) over Civ VI's FireTuner debug protocol; open benchmark over frontier models | Names the two dominant failure modes (§ 10c.3) |
| **civStation** | Voice-commanded Civ VI agent over a layered MCP | Human-sets-strategy / agent-executes — the counsel model, not the autopilot model |
| **CivAgent** (fuxiAIlab) | LLM digital player inside *Unciv*; explicitly built as a **data flywheel** | The play-to-collect-traces pattern Io's § 10d adopts |
| **CivRealm** (BIGAI) | Freeciv-web env with a server-proxy-client harness, RL *and* LLM agent interfaces | Turn-based pacing suits LLM latency; a proxy is the clean isolation seam |
| **SAGA** | Scene-graph + tool-augmented planner + dual-horizon feedback, over CivRealm | The best-documented recipe for *fixing* long-horizon play (§ 10c.4) |
| **Richelieu** | Self-evolving Diplomacy agent: memory, reflection, self-play, **no human data** | Self-play as the corpus source when no expert demonstrations exist — Io's exact position |
| **Agents of Change / HexMachina** | Catan agents that rewrite their own prompts and player code | Separating environment *discovery* from strategy *refinement* |
| **DSGBench / WarAgent** | Six-game strategic benchmark; multi-agent WWI/WWII/Warring-States sim | Fine-grained per-dimension scoring beats a single win-rate number |

### 10c. What the research actually says about *strategy*

The question is **not** "can an AI beat a human" — that was settled by AlphaStar and is not Io's
goal (§ "The goal": roughly human skill, beatable, legible). The useful findings are about *how*
these agents play, what breaks, and what a **small, local, text-driven** model can be expected
to do.

**1. Open-weight models already reached parity with a tuned algorithmic 4X AI — with a simple
prompt and no fine-tuning.** Vox Deorum's 2,327 games ran **`gpt-oss-120b` and `GLM-4.6`**, not
frontier cloud models: **97.5% survival vs the algorithmic baseline's 97.3%**, with all observed
differences statistically non-significant. This is the single most encouraging result for Ben's
plan — the bar Io wants (a genuine, beatable rival) was cleared by open weights, out of the box.
**The caveat is size, not openness**: 120B-class weights are open but not *small*. The gap to
close is 120B → a model that runs on Ben's machine, and that is a distillation problem (§ 10c.6),
not a capability problem.

**2. The LLM should hold only the macro layer.** Every project that worked delegated tactics to
algorithmic subsystems. Vox Deorum "decapitates" the game's algorithmic AI — replaces its
top-level strategic module and leaves every micro-tactical system in place. This is exactly the
A → B utility core with a coarse planner above it that § 2 already recommends; it now has
2,327 games of evidence behind it rather than one paper.

**3. Personality is emergent and free.** Vox Deorum's two models developed distinguishable
styles unprompted — `gpt-oss-120b` leaned domination (**+31.5% domination victories** vs
baseline), `GLM-4.6` weighted conquest and culture evenly. Io does not need to author
personalities; it needs to *not flatten* the ones that appear. That is a strong argument for
keeping the strategy prompt thin and letting `focus_weight` (§ 5) supply the bias.

**4. The failure modes are consistent everywhere, and none of them is about raw intelligence.**

- **Myopia / step-wise greed.** Step-by-step reasoning is structurally a *greedy* policy: fine
  over short horizons, and it makes early commitments that amplify over time and cannot be
  recovered from. The stated minimum fixes are **explicit future evaluation, backward value
  propagation, and limited commitment** — and even *one-step lookahead* escapes traps where all
  step-wise strategies provably fail.
- **The sensorium effect** (CivBench). Agents miss information they never think to query. A
  purely *pull*-based tool interface silently punishes an agent for not knowing what to ask.
- **The knowing-doing gap** (CivBench). Models articulate the right strategy and then fail to
  execute it. The worked example: Opus-controlled Portugal spent 50 turns planning and executing
  a nuclear campaign to stop a French cultural victory, struck twice — and lost anyway, to a
  *diplomatic* victory it had stopped tracking.
- **The observation-belief and belief-action gaps** (studied on Llama-3.1, Qwen3 and `gpt-oss` —
  precisely the open-weight class Io is targeting). Models' *internal* beliefs about hidden state
  are measurably **more accurate than their own verbal reports**; those beliefs degrade with
  multi-hop reasoning, show primacy and recency bias, and drift away from Bayesian coherence over
  long interactions. Practical reading: **never ask a small model to carry world state in its
  head across a campaign — hand it the state, every time.**
- **Spatial blindness.** Tile maps defeat flat text context. SAGA's fix is a **map-semantic scene
  graph** that renders typed spatial relations as short per-unit natural-language context,
  instead of inflating the global token budget with the whole map.

**5. What measurably improves play, cheapest first.** This is the actionable ranking:

1. **Push state to the model rather than making it pull** — kills the sensorium effect outright.
   Io's blackboard export is already a push artifact; the MCP server should lead with it rather
   than making the model discover it.
2. **Pre-render context into words**, including spatial relations (SAGA's scene graph). Io's
   `expected_output` / `reason_to_select` fields are this, already written.
3. **Mask to legal primitives.** Universal across every project; Io gets it free from
   `corp_command`'s validation.
4. **Add goal persistence with periodic re-evaluation, plus cross-game post-mortem** — SAGA's
   dual-horizon loop, Richelieu's memory-and-reflection, Agents-of-Change's self-rewriting
   prompts. This is the documented answer to both myopia and the knowing-doing gap.
5. **Only then, a bigger model.** Every project reports the scaffolding mattering more than the
   model tier.

**6. Distilling to a small local model is now a routine recipe, and on-task specialists beat
generalists.** Reported results in this class: a **1.7B specialist trained from traces beating a
744B frontier model on its target task**; Llama-3.2-3B going from 0.76% to ~55% on GSM8K purely
from frontier-model traces; 7B/8B supervised fine-tunes reaching 60% accuracy within ~1,000 steps
on a card-selection task. The recipe is consistent — **collect teacher traces → have the teacher
expand them into ~10k in-domain synthetic examples → filter with rule-based validators for
format, schema and novelty → SFT**. Io's advantage is that the validator is already built and
free: `corp_command_result`'s typed rejections mechanically identify any trace containing an
illegal or invalid command.

**7. The cost number that sizes the data plan.** `gpt-oss-120b` averaged **20.35M input tokens
and 555k output tokens per complete Civ V game**. Read it twice: it is why per-tick cloud
inference is not a shipping architecture, *and* it is the yield — one game produces ~555k output
tokens of decision trace. A few hundred logged games is a serious supervised corpus.

### 10d. The accepted direction (Ben, 2026-08-03)

**MCP is sanctioned.** Io gets an MCP server over the three legs that already exist — blackboard
export (read), action dictionary (meaning), corp-command seam (write). This replaces the
computer-use reading NR-040 recorded as the interim plan.

**The runtime target is a small, local model.** Not a cloud dependency, not a frontier model —
a model that runs on Ben's own machine, playing through text. Cloud inference is **not** the
shipped runtime and never becomes one.

**Cloud's only role is corpus generation.** A frontier model plays Io through the *same* MCP
interface; every decision is logged as an input/output pair — the blackboard state and dictionary
slice that went in, the `corp_command` and its rationale that came out. Those pairs are the
supervised fine-tuning corpus for the local model. This is CivAgent's data-flywheel pattern and
Richelieu's no-human-data self-play, applied to a game whose deterministic seeds can generate the
scenarios for free (§ 3).

**The goal remains "fair, beatable, legible", not "strong".** § 10c.1 is what makes this
credible: parity with a tuned algorithmic 4X AI was reached *without any fine-tuning at all*, so
the fine-tune's job is to get a **small** model to that bar — not to exceed it. Superhuman play
is explicitly not the target and would be a regression against the § "The goal" statement.

**What this does not change.** The simulation stays deterministic: the model is out-of-process,
commands still apply at the tick boundary through player-grade validation, and NR-039's proposed
determinism carve-out stays reverted. Trace logging gives replay for free — the corpus and the
replay log are the same artifact.

### 10e. Follow-on items

| Item | Carries |
|---|---|
| **BL-278** `IO_MCP_SERVER` — **landed 2026-08-03** | The MCP server over blackboard export + action dictionary + corp-command seam — the § 10a wrapper |
| **BL-279** `AI_TRACE_CORPUS` | Trace logging, corpus format, and the cloud-play → SFT-dataset pipeline for the small local model (§ 10d) |

**BL-278, as built.** `ProjectIo --serve [--ticks N]` (`src/main.cpp`) is the new persistent
headless mode: it builds the canonical world once, then reads one request per line from stdin
(`TICK`, `BLACKBOARD corp=<id> ticks=<n>`, `COMMAND corp=<id> verb=<0-7> ...`, `SHUTDOWN`) and
writes one response per line, reusing the existing `export_corp_blackboard`/`to_jsonl` (BL-206)
and `apply_corp_command` (no bypass) underneath. `tools/mcp/server.js` spawns that process and
speaks MCP-over-stdio to it — hand-rolled JSON-RPC 2.0 (no SDK dependency; none was installed in
this repo) covering `initialize`, `tools/list`, `tools/call` (`get_blackboard`, `issue_command`,
`advance_tick`, `lookup_action`, `list_actions`, `list_corps`), `resources/templates/list` and
`resources/read` (`blackboard://<corp>`). `get_blackboard` always returns the full current-tick
blackboard — the push-not-pull call from § 10c.5. Smoke-tested end-to-end (tool list, blackboard
read, a `set_workforce` command applying, a resource read) 2026-08-03; no golden/visual requirement
applies (doc-only surface, no rendering). Prompts (the `reason_to_select` leg) are not yet
exposed as MCP `prompts/*` — `lookup_action`/`list_actions` cover the same data via tools for now;
left as a follow-on rather than blocking the first attach.

**`list_corps` (added 2026-08-04, BL-306).** The seam had no way to enumerate corps or identify
the player — `get_blackboard`/`issue_command` both require a corp id, and corp ids in a generated
world are non-obvious. Read-only export from a new `CORPS` opcode in `run_serve`: one JSON line
per corp (`id`, `name`, `is_player`, `home_nation`), then `END`. Six tools, not five (NR-061).

### 10f. Sources added 2026-08-03

- Vox Deorum — hybrid LLM architecture for 4X, 2,327 games, open-weight parity, per-game token cost. https://arxiv.org/abs/2512.18564 · https://github.com/CIVITAS-John/vox-deorum
- CivBench — MCP-driven Civ VI benchmark; the sensorium effect and the knowing-doing gap. https://arxiv.org/html/2604.07733v1 · https://tasolabs.com/blog/ai/introducing-civbench-season-001
- civ6-mcp — MCP server over Civ VI's FireTuner protocol; rule-enforcing API as the write channel. https://github.com/lmwilki/civ6-mcp
- civStation — layered MCP, human-sets-strategy/agent-executes. https://github.com/NomaDamas/civStation
- CivAgent — LLM digital player in Unciv; the data-flywheel framing. https://github.com/fuxiAIlab/CivAgent · https://arxiv.org/html/2502.20807v1
- CivRealm — Freeciv env with server-proxy-client harness for RL and LLM agents. https://github.com/bigai-ai/civrealm · https://arxiv.org/abs/2401.10568
- SAGA — scene graph, tool-augmented planner, dual-horizon feedback on CivRealm. https://arxiv.org/abs/2606.29932
- Richelieu — self-evolving Diplomacy agents; memory, reflection, self-play without human data. https://arxiv.org/abs/2407.06813
- Agents of Change / HexMachina — self-evolving agents rewriting their own prompts and code. https://arxiv.org/abs/2506.04651
- Why Reasoning Fails to Plan — step-wise greed, myopic deviation, and the three requirements for long-horizon coherence. https://arxiv.org/abs/2601.22311
- Why Do LLMs Struggle in Strategic Play? — observation-belief and belief-action gaps in open-weight models. https://arxiv.org/abs/2605.00226
- DSGBench — six-game strategic benchmark with per-dimension scoring. https://arxiv.org/abs/2503.06047
- WarAgent — LLM multi-agent simulation of historical conflicts. https://arxiv.org/abs/2311.17227
- Cicero — human-level full-press Diplomacy; controllable dialogue model + planning engine. https://ai.meta.com/research/cicero/ · https://github.com/facebookresearch/diplomacy_cicero
- Small Language Models for Efficient Agentic Tool Calling — targeted fine-tunes beating much larger models. https://arxiv.org/abs/2512.15943
- Model Context Protocol — the specification. https://modelcontextprotocol.io

### 10g. Ruling on the C-route feasibility note (Ben, 2026-08-08 — NR-094)

A cloud research session (`docs/ai/LANGUAGE_POLICY_FEASIBILITY.md`) ran the two feasibility gates
§ 10d implicitly set — can the runtime model be compressed, and does it run in budget — and both
pass (3–8B compresses to Vox Deorum's already-open-weight-parity bar; the per-decision budget is
~90s at 1×, ~5.6s at 16× for 8 rivals, computed from `sim_loop`'s own constants, against ~3–7s of
measured 8B-Q4 decode). Neither gate disqualifies the local-model runtime target.

**But the note is right that § 10d, as written, drifted from this doc's own Stage A/B/C
decomposition (§ 7).** § 10d's MCP-attached "small local model plays through text" framing reads
as the model calling `issue_command` directly — i.e. the model AS Stage A/B, replacing or
augmenting the scorer's action-emission. That was never the plan `list_corps`/`issue_command`'s
sibling framing in § 7 set: **Stage A/B (the deterministic scorer, BL-202/203) is the action
generator; Stage C is the LLM speaking in-character in channels** — a dialogue layer over the
decision stream, not a replacement for it. § 10d's drift is corrected here, not by walking back
MCP or the local-model target (both stand), but by being explicit about which Stage the model
occupies.

**Ruling: adopt the Cicero configuration the note recommends, as Stage C's concrete shape.**

- **Stage A/B stays the action generator, indefinitely.** `corp_ai.cpp`'s scored-utility core
  (or a future RL policy, § 8 of the note — not disqualified, just not chosen yet) keeps emitting
  `corp_command`. This was never seriously in question once the note's § 9.1 point lands:
  distilling `corp_ai.cpp` yields, at best, `corp_ai.cpp` — there is no skill upside to buy, only
  legibility and determinism to lose, and both are named requirements (§ "The goal";
  `docs/development/DEVLOG.md` inherits nothing from a policy with no rationale to show).
- **Stage C is a conditioned dialogue layer over the `corp_decision` ring**, not an independent
  planner. Cicero's shape exactly: the scorer's winning command + reason code IS the "intent" a
  small (Cicero's own reference point: 2.7B) model is conditioned on to speak in Public/private
  channels (§ 7). This is where the diplomacy capability § 10d was actually reaching for lives —
  separable from action generation, per the note's § 7.
- **A goal layer above the scorer stays a live, separate option** — not decided here, filed as
  its own open question in BL-334 below — for when/if step-wise myopia (§ 10c.4's documented
  failure mode) is actually observed rather than assumed.
- **The constraint tax (the note's § 6) is the reason this isn't merely a style preference.**
  Small models measured dropping from 91.5% to 48.0% executable accuracy under a hard schema,
  with the damage entering where instructions suppress deliberation — exactly the failure mode a
  model-emits-`corp_command`-directly design sits on. Keeping the scorer as the action generator
  avoids the risk instead of mitigating it.
- **BL-279 is rescoped, not cancelled.** Its corpus still gets bootstrapped from `corp_ai.cpp`'s
  own decision ring first (free, no cloud spend, exactly the note's § 9 "bootstrap from the
  scorer" instruction) — but the corpus now trains the **Stage C dialogue layer** (BL-334), not
  an action-emitting model. Cloud play through the MCP server remains valuable for a DIFFERENT
  reason: an external agent (frontier or otherwise) playing a corp interactively is a research/
  spectacle use of BL-278, not the shipped rival AI's architecture — that capability is
  unaffected by this ruling and needs no rescoping.
- **MCP, BL-278, and the local-model-as-runtime-target all stand exactly as § 10d states.** This
  ruling changes which Stage the model occupies, not the transport or the no-cloud-dependency
  invariant.

**What this does not settle.** Whether Stage C ships before or after the v0.2.0 corp-AI arc
closes, and the model size/quantisation for Stage C specifically (Cicero's 2.7B is a reference
point, not a spec) — both left inside BL-334's design-owed remainder. The goal-layer question and
the unmeasured token-cost assumption are filed as their own items (BL-336, parked pending
observed evidence; BL-335, a cheap independent measurement) rather than carried as footnotes.
